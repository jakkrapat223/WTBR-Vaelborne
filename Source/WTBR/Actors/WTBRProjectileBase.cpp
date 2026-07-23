// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRProjectileBase.h"
#include "WTBRValidationLog.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InterpToMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "WTBRCharacter.h"
#include "Actors/WTBRAegornWallActor.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "VFX/WTBRVFXManagerSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

namespace
{
    FVector WTBRResolveProjectileImpactNormal(const FHitResult& Hit,
        const FVector& ImpactPoint, const AActor* HitActor, const FVector& FallbackNormal)
    {
        if (!Hit.ImpactNormal.IsNearlyZero())
        {
            return Hit.ImpactNormal.GetSafeNormal();
        }
        if (!Hit.Normal.IsNearlyZero())
        {
            return Hit.Normal.GetSafeNormal();
        }
        if (IsValid(HitActor))
        {
            const FVector FromActor = (ImpactPoint - HitActor->GetActorLocation()).GetSafeNormal();
            if (!FromActor.IsNearlyZero())
            {
                return FromActor;
            }
        }
        if (!FallbackNormal.IsNearlyZero())
        {
            return FallbackNormal.GetSafeNormal();
        }
        return FVector::UpVector;
    }

    uint8 WTBRResolveProjectileImpactSurface(const FHitResult& Hit)
    {
        const UPhysicalMaterial* PhysicalMaterial = Hit.PhysMaterial.Get();
        return static_cast<uint8>(UPhysicalMaterial::DetermineSurfaceType(PhysicalMaterial));
    }
}

AWTBRProjectileBase::AWTBRProjectileBase()
{
    // bCanEverTick must be true HERE or the tick function is never registered, and
    // SetActorTickEnabled() later is then a silent no-op — which is exactly how
    // Venyx's proximity sweep shipped dead: the code was correct and simply never
    // ran. bStartWithTickEnabled keeps the cost off for every projectile that does
    // not sweep, which is all of them except a Hound volley.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;

    BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
    RootComponent = BoxCollision;
    BoxCollision->SetBoxExtent(FVector(15.0f, 15.0f, 15.0f));
    BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxCollision->SetCollisionObjectType(ECC_WorldDynamic);
    BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoxCollision->SetCollisionResponseToChannel(ECC_Pawn,         ECR_Overlap);
    BoxCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    BoxCollision->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Overlap);
    BoxCollision->SetGenerateOverlapEvents(true);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
        TEXT("ProjectileMovement"));
    ProjectileMovement->bAutoActivate           = false;
    ProjectileMovement->InitialSpeed            = 0.0f;
    ProjectileMovement->MaxSpeed                = 0.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale   = 0.0f;

    InterpMovement = CreateDefaultSubobject<UInterpToMovementComponent>(
        TEXT("InterpMovement"));
    InterpMovement->bAutoActivate  = false;
    InterpMovement->BehaviourType  = EInterpToBehaviourType::OneShot;

    SetReplicatingMovement(true);
}

void AWTBRProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    if (IsValid(BoxCollision))
    {
        BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        BoxCollision->SetCollisionObjectType(ECC_WorldDynamic);
        BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
        BoxCollision->SetCollisionResponseToChannel(ECC_Pawn,         ECR_Overlap);
        BoxCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
        BoxCollision->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Block);
        BoxCollision->SetGenerateOverlapEvents(true);
        BoxCollision->SetNotifyRigidBodyCollision(true);

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 ProjectileContact] CollisionConfig | Projectile=%s | HasAuthority=%s | CollisionEnabled=%d | ObjectType=%d | GenerateOverlap=%s | RespPawn=%d | RespWorldDynamic=%d | RespWorldStatic=%d | Extent=%s | Location=%s"),
            *GetNameSafe(this),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(BoxCollision->GetCollisionEnabled()),
            static_cast<int32>(BoxCollision->GetCollisionObjectType()),
            BoxCollision->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(BoxCollision->GetCollisionResponseToChannel(ECC_Pawn)),
            static_cast<int32>(BoxCollision->GetCollisionResponseToChannel(ECC_WorldDynamic)),
            static_cast<int32>(BoxCollision->GetCollisionResponseToChannel(ECC_WorldStatic)),
            *BoxCollision->GetScaledBoxExtent().ToString(),
            *BoxCollision->GetComponentLocation().ToString());
    }

    if (HasAuthority() && IsValid(BoxCollision))
    {
        BoxCollision->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRProjectileBase::OnOverlapBegin);
        BoxCollision->OnComponentHit.AddDynamic(
            this, &AWTBRProjectileBase::OnProjectileHit);
    }

    // Purely cosmetic — runs on every machine the actor exists on (server and
    // every client it replicates to), not just the authority.
    if (IsValid(TrailEffect) && IsValid(RootComponent))
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            TrailEffect, RootComponent, NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            /*bAutoDestroy=*/true);
    }
}

void AWTBRProjectileBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRProjectileBase, ProjectileState);
}

// ─── Core Interface ───────────────────────────────────────────────────────────

void AWTBRProjectileBase::ApplyVFXConfig(const FWTBRProjectileVFXConfig& Config)
{
    TrailEffect = Config.TrailEffect;
    DefaultImpactEffect = Config.DefaultImpactEffect;
    SurfaceImpactOverrides = Config.SurfaceImpactOverrides;
    ImpactAssetParameters = Config.ImpactAssetParameters;
    bUseBuiltInImpactVFX = Config.bUseBuiltInImpactVFX;
    MaxImpactVFXDistance = Config.MaxImpactVFXDistance;
    ImpactSound = Config.ImpactSound;
    ImpactDecalMaterial = Config.ImpactDecalMaterial;
    ImpactDecalSize = Config.ImpactDecalSize;
    ImpactDecalLifeSpan = Config.ImpactDecalLifeSpan;
    ImpactCameraShake = Config.ImpactCameraShake;
    bDrawImpactDebug = Config.bDrawImpactDebug;
}

UNiagaraSystem* AWTBRProjectileBase::ResolveImpactEffect(uint8 SurfaceType) const
{
    return WTBRResolveSurfaceImpactEffect(
        DefaultImpactEffect, SurfaceImpactOverrides, SurfaceType);
}

void AWTBRProjectileBase::SpawnBuiltInImpactVFX(
    FVector ImpactPoint, FVector ImpactNormal, uint8 SurfaceType) const
{
    UNiagaraSystem* Effect = ResolveImpactEffect(SurfaceType);
    UWorld* World = GetWorld();
    UWTBRVFXManagerSubsystem* VFXManager = World
        ? World->GetSubsystem<UWTBRVFXManagerSubsystem>() : nullptr;
    if (!IsValid(Effect) || !VFXManager)
    {
        return;
    }

    FWTBRImpactVFXRequest Request;
    Request.Effect = Effect;
    Request.Sound = ImpactSound;
    Request.DecalMaterial = ImpactDecalMaterial;
    Request.AssetParameters = ImpactAssetParameters;
    Request.CameraShake = ImpactCameraShake;
    Request.Location = ImpactPoint;
    Request.Normal = ImpactNormal;
    Request.DecalSize = ImpactDecalSize;
    Request.DecalLifeSpan = ImpactDecalLifeSpan;
    Request.MaxDistance = MaxImpactVFXDistance;
    Request.SurfaceType = SurfaceType;
    Request.bDrawDebug = bDrawImpactDebug;
    VFXManager->SpawnImpact(Request);
}

void AWTBRProjectileBase::Multicast_ProjectileHitVFX_Implementation(
    FVector ImpactPoint, FVector ImpactNormal, uint8 SurfaceType)
{
    const FVector SafeNormal = ImpactNormal.IsNearlyZero()
        ? FVector::UpVector : ImpactNormal.GetSafeNormal();

    if (bUseBuiltInImpactVFX)
    {
        SpawnBuiltInImpactVFX(ImpactPoint, SafeNormal, SurfaceType);
    }
    else
    {
        OnProjectileHitVFX(ImpactPoint, SafeNormal);
    }
    OnProjectileSurfaceHitVFX(ImpactPoint, SafeNormal, SurfaceType);
}

void AWTBRProjectileBase::InitializeProjectile(
    float InDamage, float InSpeed,
    ETriggerCategory InCategory, bool bSniper,
    bool bExplode, float ExplodeRadius)
{
    BaseDamage       = InDamage;
    CachedSpeed      = FMath::Max(1.0f, InSpeed);
    OwnerCategory    = InCategory;
    bIsSniper        = bSniper;
    bExplodeOnImpact = bExplode;
    ExplosionRadius  = ExplodeRadius;

    ProjectileMovement->InitialSpeed = CachedSpeed;
    ProjectileMovement->MaxSpeed     = CachedSpeed;
}

void AWTBRProjectileBase::ConfigureOnHitEffects(float InBleedDamagePerTick, float InBleedDuration,
    float InShieldBrittleDamageMultiplier, float InShieldBrittleDuration)
{
    BleedDamagePerTick = FMath::Max(0.0f, InBleedDamagePerTick);
    BleedDuration = FMath::Max(0.0f, InBleedDuration);
    ShieldBrittleDamageMultiplier = FMath::Max(1.0f, InShieldBrittleDamageMultiplier);
    ShieldBrittleDuration = FMath::Max(0.0f, InShieldBrittleDuration);
}

void AWTBRProjectileBase::Launch(FVector Direction, AActor* InInstigator)
{
    if (!HasAuthority()) return;
    if (!IsValid(ProjectileMovement)) return;

    OwnerInstigator = InInstigator;

    FVector SafeDir = Direction.GetSafeNormal();
    if (SafeDir.IsNearlyZero())
    {
        SafeDir = GetActorForwardVector().GetSafeNormal();
    }
    if (SafeDir.IsNearlyZero())
    {
        SafeDir = FVector::ForwardVector;
    }

    SetActorRotation(SafeDir.Rotation());

    USceneComponent* DesiredUpdatedComponent = nullptr;
    if (IsValid(BoxCollision))
    {
        DesiredUpdatedComponent = BoxCollision;
    }
    else if (IsValid(RootComponent))
    {
        DesiredUpdatedComponent = RootComponent;
    }
    if (IsValid(DesiredUpdatedComponent))
    {
        ProjectileMovement->SetUpdatedComponent(DesiredUpdatedComponent);
    }

    // Launch is the straight ProjectileMovement path; curve/path projectiles use InitializePathMovement().
    if (IsValid(InterpMovement) && InterpMovement->IsActive())
    {
        InterpMovement->Deactivate();
    }

    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    ProjectileMovement->ProjectileGravityScale = 0.0f;
    ProjectileMovement->Velocity = SafeDir * CachedSpeed;
    ProjectileMovement->Activate();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test36 Projectile Launch] Projectile=%s | Dir=%s | Speed=%.2f | Velocity=%s | GravityScale=%.2f | UpdatedComponent=%s | Active=%s"),
        *GetNameSafe(this),
        *SafeDir.ToString(),
        CachedSpeed,
        *ProjectileMovement->Velocity.ToString(),
        ProjectileMovement->ProjectileGravityScale,
        *GetNameSafe(ProjectileMovement->UpdatedComponent),
        ProjectileMovement->IsActive() ? TEXT("true") : TEXT("false"));

    if (CachedSpeed > 0.0f)
        SetLifeSpan(MaxRange / CachedSpeed);

    OnProjectileLaunched();
}

void AWTBRProjectileBase::EnableHoming(USceneComponent* Target, float Accel)
{
    if (!IsValid(Target)) return;
    ProjectileMovement->bIsHomingProjectile         = true;
    ProjectileMovement->HomingTargetComponent       = Target;
    ProjectileMovement->HomingAccelerationMagnitude = Accel;
}

// ─── Overlap ─────────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnOverlapBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    const bool bIsAegornWall =
        IsValid(OtherActor) && OtherActor->IsA(AWTBRAegornWallActor::StaticClass());
    const int32 ProjectileCollisionEnabled = IsValid(BoxCollision)
        ? static_cast<int32>(BoxCollision->GetCollisionEnabled())
        : -1;
    const int32 ProjectileObjectType = IsValid(BoxCollision)
        ? static_cast<int32>(BoxCollision->GetCollisionObjectType())
        : -1;
    const int32 OtherObjectType = IsValid(OtherComp)
        ? static_cast<int32>(OtherComp->GetCollisionObjectType())
        : -1;
    const int32 ProjectileResponseToOther =
        (IsValid(BoxCollision) && IsValid(OtherComp))
            ? static_cast<int32>(BoxCollision->GetCollisionResponseToChannel(
                OtherComp->GetCollisionObjectType()))
            : -1;
    const int32 OtherResponseToProjectile =
        (IsValid(BoxCollision) && IsValid(OtherComp))
            ? static_cast<int32>(OtherComp->GetCollisionResponseToChannel(
                BoxCollision->GetCollisionObjectType()))
            : -1;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 ProjectileContact] Projectile=%s | ProjectileClass=%s | Other=%s | OtherClass=%s | OtherComp=%s | HasAuthority=%s | CollisionEnabled=%d | ObjectType=%d | OtherObjectType=%d | ResponseToOther=%d | OtherResponseToProjectile=%d | IsAegornWall=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        *GetNameSafe(OtherComp),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        ProjectileCollisionEnabled,
        ProjectileObjectType,
        OtherObjectType,
        ProjectileResponseToOther,
        OtherResponseToProjectile,
        bIsAegornWall ? TEXT("true") : TEXT("false"));
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileOverlap | Projectile=%s | Attacker=%s | Target=%s | TargetClass=%s | HasAuthority=%s | Damage=%.1f | OwnerCategory=%d | CollisionEnabled=%d | ObjectType=%d | OtherObjectType=%d | ResponseToOther=%d | OtherResponseToProjectile=%d"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerInstigator.Get()),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        BaseDamage,
        static_cast<int32>(OwnerCategory),
        ProjectileCollisionEnabled,
        ProjectileObjectType,
        OtherObjectType,
        ProjectileResponseToOther,
        OtherResponseToProjectile);

    if (OwnerCategory == ETriggerCategory::Gunner)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitAttempt | Projectile=%s | Other=%s | HasAuthority=%s | DamagedActors=%d | Damage=%.1f"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            DamagedActors.Num(),
            BaseDamage);
    }

    if (!HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Reason=NoAuthority"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitRejected | Reason=NoAuthority | Projectile=%s | Other=%s"),
                *GetNameSafe(this),
                *GetNameSafe(OtherActor));
        }
        return;
    }
    if (!IsValid(OtherActor))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Reason=InvalidTarget"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitRejected | Reason=InvalidTarget | Projectile=%s"),
                *GetNameSafe(this));
        }
        return;
    }
    if (OtherActor == OwnerInstigator)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Attacker=%s | Reason=SelfOwnerInstigator"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor),
            *GetNameSafe(OwnerInstigator.Get()));
        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitRejected | Reason=Self | Projectile=%s | Other=%s"),
                *GetNameSafe(this),
                *GetNameSafe(OtherActor));
        }
        return;
    }

    if (AWTBRAegornWallActor* AegornWall = Cast<AWTBRAegornWallActor>(OtherActor))
    {
        AegornWall->ApplyBrittle(ShieldBrittleDamageMultiplier, ShieldBrittleDuration);
        AegornWall->HandleProjectileContact(this, TEXT("ProjectileOverlap"));
        return;
    }

    if (DamagedActors.Contains(OtherActor))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Reason=AlreadyHit | DamagedActors=%d"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor),
            DamagedActors.Num());
        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitRejected | Reason=AlreadyHit | Projectile=%s | Other=%s | DamagedActors=%d"),
                *GetNameSafe(this),
                *GetNameSafe(OtherActor),
                DamagedActors.Num());
        }
        return; // skip already-hit actors (penetration guard)
    }

    // Bullet vs Bullet
    if (AWTBRProjectileBase* OtherBullet = Cast<AWTBRProjectileBase>(OtherActor))
    {
        OnBulletClash(OtherBullet);
        return;
    }

    // Explosive: detonate on any hit (character or environment)
    if (bExplodeOnImpact)
    {
        TriggerExplosion();
        return;
    }

    // Direct hit on character
    if (AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor))
    {
        // Character contact is deliberately an overlap, not a blocking hit.
        // bBlockingHit is therefore false even when ProjectileMovement swept
        // into a target and supplied the true contact point. bFromSweep is
        // the valid signal that SweepResult carries overlap contact data.
        const FVector ImpactPoint = ResolveHitZoneImpactPoint(
            bFromSweep, SweepResult, GetActorLocation());
        FVector FallbackImpactNormal = FVector::ZeroVector;
        if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero())
        {
            FallbackImpactNormal = -ProjectileMovement->Velocity.GetSafeNormal();
        }
        if (FallbackImpactNormal.IsNearlyZero())
        {
            FallbackImpactNormal = -GetActorForwardVector().GetSafeNormal();
        }
        const FVector ImpactNormal = WTBRResolveProjectileImpactNormal(
            SweepResult, ImpactPoint, HitChar, FallbackImpactNormal);

        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitAccepted | Projectile=%s | Target=%s | Impact=%s | Damage=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(HitChar),
                *ImpactPoint.ToString(),
                BaseDamage);
        }

        Multicast_ProjectileHitVFX(
            ImpactPoint, ImpactNormal, WTBRResolveProjectileImpactSurface(SweepResult));
        DamagedActors.Add(OtherActor); // track before applying so re-entry is blocked

        if (IsValid(HitChar->HealthComponent))
        {
            // Headshot-style damage multiplier (owner-requested 2026-07-17,
            // applies to every projectile weapon — Gunner/Sniper/Composite —
            // via this shared hit path). See ClassifyHitZone's own comment for
            // why this is a capsule-geometry approximation, not a real
            // per-bone hitbox lookup.
            const UWTBRCoreStatsDataAsset* Stats = HitChar->HealthComponent->CoreStatsAsset.LoadSynchronous();
            const UCapsuleComponent* TargetCapsule = HitChar->GetCapsuleComponent();
            const EWTBRHitZone Zone = IsValid(TargetCapsule)
                ? ClassifyHitZone(TargetCapsule->GetComponentLocation(),
                    TargetCapsule->GetScaledCapsuleHalfHeight(), TargetCapsule->GetScaledCapsuleRadius(),
                    ImpactPoint,
                    Stats ? Stats->HeadZoneHeightThreshold : 0.85f,
                    Stats ? Stats->LegZoneHeightThreshold  : 0.35f,
                    Stats ? Stats->ArmLateralRadiusRatio   : 0.75f)
                : EWTBRHitZone::Torso;
            const float ZoneMultiplier = GetHitZoneDamageMultiplier(Zone, Stats);
            const float FinalDamage = BaseDamage * ZoneMultiplier;

            const float OldHP = HitChar->HealthComponent->GetCurrentHP();
            WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileDamageApply | Projectile=%s | Attacker=%s | Target=%s | OldHP=%.1f | BaseDamage=%.1f | HitZone=%d | ZoneMultiplier=%.2f | FinalDamage=%.1f | ImpactPoint=%s | FromSweep=%s | ApplyDamageCalled=true"),
                *GetNameSafe(this),
                *GetNameSafe(OwnerInstigator.Get()),
                *GetNameSafe(HitChar),
                OldHP,
                BaseDamage,
                static_cast<int32>(Zone),
                ZoneMultiplier,
                FinalDamage,
                *ImpactPoint.ToString(),
                bFromSweep ? TEXT("true") : TEXT("false"));
            HitChar->HealthComponent->ApplyDamage(FinalDamage, OwnerInstigator.Get());
            HitChar->HealthComponent->ApplyBleed(BleedDamagePerTick, BleedDuration, OwnerInstigator.Get());
            const float NewHP = HitChar->HealthComponent->GetCurrentHP();
            WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileDamageResult | Projectile=%s | Attacker=%s | Target=%s | OldHP=%.1f | Damage=%.1f | NewHP=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(OwnerInstigator.Get()),
                *GetNameSafe(HitChar),
                OldHP,
                FinalDamage,
                NewHP);
            if (OwnerCategory == ETriggerCategory::Gunner)
            {
                WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] DamageApplied | Projectile=%s | Target=%s | Damage=%.1f | OldHP=%.1f | NewHP=%.1f | DamagedActors=%d"),
                    *GetNameSafe(this),
                    *GetNameSafe(HitChar),
                    FinalDamage,
                    OldHP,
                    NewHP,
                    DamagedActors.Num());
            }
            if (OwnerCategory == ETriggerCategory::SniperBullet)
            {
                WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] DamageApplied | Target=%s | Damage=%.1f | OldHP=%.1f | NewHP=%.1f"),
                    *GetNameSafe(HitChar),
                    FinalDamage,
                    OldHP,
                    NewHP);
            }
        }
        else
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Reason=HealthComponentInvalid"),
                *GetNameSafe(this),
                *GetNameSafe(HitChar));
        }

        if (KnockbackForce > 0.0f)
        {
            const FVector KnockDir =
                (HitChar->GetActorLocation() - GetActorLocation()).GetSafeNormal();
            HitChar->LaunchCharacter(KnockDir * KnockbackForce, true, true);
        }

        if (bCanPenetrate)
            return; // penetrating bullet continues flying through character

        ProjectileState = EProjectileState::Destroyed;
        Destroy();
        return;
    }

    // Environment hit (not character, not projectile):
    // All bullets stop at geometry — even penetrating snipers cannot pass through walls
    FVector EnvironmentFallbackImpactNormal = FVector::ZeroVector;
    if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero())
    {
        EnvironmentFallbackImpactNormal = -ProjectileMovement->Velocity.GetSafeNormal();
    }
    if (EnvironmentFallbackImpactNormal.IsNearlyZero())
    {
        EnvironmentFallbackImpactNormal = -GetActorForwardVector().GetSafeNormal();
    }
    Multicast_ProjectileHitVFX(
        GetActorLocation(), EnvironmentFallbackImpactNormal,
        WTBRResolveProjectileImpactSurface(SweepResult));

    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

void AWTBRProjectileBase::OnProjectileHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    const bool bIsAegornWall =
        IsValid(OtherActor) && OtherActor->IsA(AWTBRAegornWallActor::StaticClass());
    const int32 ProjectileCollisionEnabled = IsValid(BoxCollision)
        ? static_cast<int32>(BoxCollision->GetCollisionEnabled())
        : -1;
    const int32 ProjectileObjectType = IsValid(BoxCollision)
        ? static_cast<int32>(BoxCollision->GetCollisionObjectType())
        : -1;
    const int32 OtherObjectType = IsValid(OtherComp)
        ? static_cast<int32>(OtherComp->GetCollisionObjectType())
        : -1;
    const int32 ProjectileResponseToOther =
        (IsValid(BoxCollision) && IsValid(OtherComp))
            ? static_cast<int32>(BoxCollision->GetCollisionResponseToChannel(
                OtherComp->GetCollisionObjectType()))
            : -1;
    const int32 OtherResponseToProjectile =
        (IsValid(BoxCollision) && IsValid(OtherComp))
            ? static_cast<int32>(OtherComp->GetCollisionResponseToChannel(
                BoxCollision->GetCollisionObjectType()))
            : -1;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 ProjectileContact] Hit | Projectile=%s | ProjectileClass=%s | Other=%s | OtherClass=%s | OtherComp=%s | HasAuthority=%s | CollisionEnabled=%d | ObjectType=%d | OtherObjectType=%d | ResponseToOther=%d | OtherResponseToProjectile=%d | IsAegornWall=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        *GetNameSafe(OtherComp),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        ProjectileCollisionEnabled,
        ProjectileObjectType,
        OtherObjectType,
        ProjectileResponseToOther,
        OtherResponseToProjectile,
        bIsAegornWall ? TEXT("true") : TEXT("false"));

    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;
    if (OtherActor == OwnerInstigator) return;

    if (AWTBRAegornWallActor* AegornWall = Cast<AWTBRAegornWallActor>(OtherActor))
    {
        AegornWall->HandleProjectileContact(this, TEXT("ProjectileHit"));
        return;
    }

    if (AWTBRProjectileBase* OtherBullet = Cast<AWTBRProjectileBase>(OtherActor))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileRejected | Projectile=%s | Target=%s | Reason=BulletClash"),
            *GetNameSafe(this),
            *GetNameSafe(OtherBullet));
        OnBulletClash(OtherBullet);
        return;
    }

    if (bExplodeOnImpact)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileExplosion | Projectile=%s | Target=%s | Damage=%.1f | Radius=%.1f"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor),
            BaseDamage,
            ExplosionRadius);
        TriggerExplosion();
        return;
    }

    const FVector ImpactPoint = !Hit.ImpactPoint.IsNearlyZero()
        ? Hit.ImpactPoint
        : (!Hit.Location.IsNearlyZero() ? Hit.Location : GetActorLocation());
    FVector FallbackImpactNormal = FVector::ZeroVector;
    if (IsValid(ProjectileMovement) && !ProjectileMovement->Velocity.IsNearlyZero())
    {
        FallbackImpactNormal = -ProjectileMovement->Velocity.GetSafeNormal();
    }
    if (FallbackImpactNormal.IsNearlyZero())
    {
        FallbackImpactNormal = -GetActorForwardVector().GetSafeNormal();
    }
    const FVector ImpactNormal = WTBRResolveProjectileImpactNormal(
        Hit, ImpactPoint, OtherActor, FallbackImpactNormal);
    Multicast_ProjectileHitVFX(
        ImpactPoint, ImpactNormal, WTBRResolveProjectileImpactSurface(Hit));

    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

// ─── Explosion ───────────────────────────────────────────────────────────────

void AWTBRProjectileBase::TriggerExplosion()
{
    if (!HasAuthority()) return;

    const FVector Center = GetActorLocation();
    ApplyRadialDamage(ExplosionRadius, BaseDamage);

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Center, ExplosionRadius, 16,
        FColor::Orange, false, 0.5f);
#endif

    ProjectileState = EProjectileState::Exploded;
    OnExplosionVFX(Center, ExplosionRadius);

    if (bHasDelayedSecondExplosion)
    {
        SetLifeSpan(0.0f);
        GetWorldTimerManager().SetTimer(
            SecondExplosionTimer, this, &AWTBRProjectileBase::TriggerSecondExplosion,
            SecondExplosionDelay, false);
        return;
    }

    Destroy();
}

void AWTBRProjectileBase::TriggerSecondExplosion()
{
    if (!HasAuthority()) return;

    const FVector Center = GetActorLocation();
    ApplyRadialDamage(SecondExplosionRadius, SecondExplosionDamage);

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Center, SecondExplosionRadius, 16,
        FColor::Orange, false, 0.5f);
#endif

    OnExplosionVFX(Center, SecondExplosionRadius);
    Destroy();
}

void AWTBRProjectileBase::ApplyRadialDamage(float Radius, float Damage)
{
    const FVector Center = GetActorLocation();

    TArray<FHitResult> Hits;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (IsValid(OwnerInstigator))
        Params.AddIgnoredActor(OwnerInstigator.Get());

    GetWorld()->SweepMultiByChannel(
        Hits, Center, Center, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);

    TArray<AWTBRCharacter*> Targets;
    CollectUniqueRadialTargets(Hits, Targets);

    for (AWTBRCharacter* HitChar : Targets)
    {
        if (!IsLocationWithinShapedChargeCone(HitChar->GetActorLocation())) continue;
        HitChar->HealthComponent->ApplyDamage(Damage, OwnerInstigator.Get());
    }
}

void AWTBRProjectileBase::CollectUniqueRadialTargets(
    const TArray<FHitResult>& Hits, TArray<AWTBRCharacter*>& OutTargets)
{
    OutTargets.Reset();

    // A multi-sweep reports one hit per overlapping COMPONENT, and a character
    // answers ECC_Pawn on more than one of them — so damaging straight from the hit
    // list charged every explosion twice against the same body. Invisible while
    // explosives fired one at a time; an eight-cube Hound volley made it obvious by
    // logging sixteen damage events.
    TSet<AWTBRCharacter*> Seen;
    Seen.Reserve(Hits.Num());

    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar) || !IsValid(HitChar->HealthComponent)) continue;

        bool bAlreadySeen = false;
        Seen.Add(HitChar, &bAlreadySeen);
        if (bAlreadySeen) continue;

        OutTargets.Add(HitChar);
    }
}

bool AWTBRProjectileBase::IsLocationWithinShapedChargeCone(const FVector& TargetLocation) const
{
    if (!bIsShapedCharge) return true;

    const FVector ToTarget = TargetLocation - GetActorLocation();
    if (ToTarget.IsNearlyZero()) return true;

    // Fire-time direction when one was supplied, so the cone is identical on every
    // machine regardless of where exactly the projectile stopped.
    const FVector ConeAxis = ShapedChargeDirection.IsNearlyZero()
        ? GetActorForwardVector()
        : ShapedChargeDirection.GetSafeNormal();

    const float ConeDotThreshold = FMath::Cos(FMath::DegreesToRadians(
        FMath::Clamp(ShapedChargeConeHalfAngleDegrees, 0.0f, 180.0f)));
    return FVector::DotProduct(ConeAxis, ToTarget.GetSafeNormal()) >= ConeDotThreshold;
}

EWTBRHitZone AWTBRProjectileBase::ClassifyHitZone(const FVector& CapsuleCenter, float CapsuleHalfHeight,
    float CapsuleRadius, const FVector& ImpactPoint, float HeadHeightThreshold,
    float LegHeightThreshold, float ArmLateralRadiusRatio)
{
    if (CapsuleHalfHeight <= 0.0f) return EWTBRHitZone::Torso;

    const float BottomZ = CapsuleCenter.Z - CapsuleHalfHeight;
    const float RelativeHeight = FMath::Clamp(
        (ImpactPoint.Z - BottomZ) / (CapsuleHalfHeight * 2.0f), 0.0f, 1.0f);

    if (RelativeHeight >= HeadHeightThreshold) return EWTBRHitZone::Head;
    if (RelativeHeight < LegHeightThreshold)   return EWTBRHitZone::Leg;

    // Torso height band — lateral distance from the central vertical axis
    // approximates an outstretched-arm hit (no separate arm geometry exists
    // to test against directly).
    const float LateralDistSq = FVector::DistSquared2D(ImpactPoint, CapsuleCenter);
    // Capsule collisions never exceed the capsule radius. Convert legacy
    // values above 1.0 to the reachable default so arm hits do not silently
    // turn into torso hits.
    constexpr float DefaultArmRadiusRatio = 0.75f;
    const float ArmRadiusRatio = ArmLateralRadiusRatio > 1.0f
        ? DefaultArmRadiusRatio
        : FMath::Clamp(ArmLateralRadiusRatio, 0.0f, 1.0f);
    const float ArmThresholdDist = CapsuleRadius * ArmRadiusRatio;
    if (LateralDistSq >= FMath::Square(ArmThresholdDist)) return EWTBRHitZone::Arm;

    return EWTBRHitZone::Torso;
}

FVector AWTBRProjectileBase::ResolveHitZoneImpactPoint(bool bFromSweep,
    const FHitResult& SweepResult, const FVector& FallbackLocation)
{
    if (!bFromSweep)
    {
        return FallbackLocation;
    }

    // Overlap sweeps can report bFromSweep=true while leaving ImpactPoint at
    // its default zero vector. That point is not a real impact (a character
    // capsule centre is above ground) and would classify every hit as a leg.
    // Location is the next-best swept position; only fall back to the
    // projectile's current location when neither sweep field is populated.
    if (!SweepResult.ImpactPoint.IsNearlyZero())
    {
        return SweepResult.ImpactPoint;
    }
    if (!SweepResult.Location.IsNearlyZero())
    {
        return SweepResult.Location;
    }
    return FallbackLocation;
}

float AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone Zone, const UWTBRCoreStatsDataAsset* Stats)
{
    switch (Zone)
    {
    case EWTBRHitZone::Head: return Stats ? Stats->HeadDamageMultiplier : 2.0f;
    case EWTBRHitZone::Arm:  return Stats ? Stats->ArmDamageMultiplier  : 0.85f;
    case EWTBRHitZone::Leg:  return Stats ? Stats->LegDamageMultiplier  : 0.85f;
    default:                 return Stats ? Stats->TorsoDamageMultiplier : 1.0f;
    }
}

// ─── Bullet Clash ────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnBulletClash(AWTBRProjectileBase* OtherBullet)
{
    if (!HasAuthority()) return;
    if (!IsValid(OtherBullet)) return;
    // Melee-category projectiles (e.g. Arcven arc waves) are sweep-like energy,
    // not Gunner bullets — they never participate in the bullet-clash pipeline
    // (no cube split, no mutual destruction) and simply pass straight through
    // whatever they overlap. Without this, two arc waves meeting mid-air fell
    // through to the Gunner-vs-Gunner branch below and split into cube fragments
    // that fanned 90° and damaged both casters.
    if (OwnerCategory == ETriggerCategory::Melee || OtherBullet->OwnerCategory == ETriggerCategory::Melee) return;
    // Burst bullets from the same instigator never clash with each other
    if (IsValid(OwnerInstigator) && OtherBullet->OwnerInstigator == OwnerInstigator) return;
    // GDD Rule: Sniper vs Sniper — no clash, both continue unaffected
    if (bIsSniper && OtherBullet->bIsSniper) return;
    // First bullet to reach here wins; prevents double-processing when both overlaps fire
    if (bHandledClash || OtherBullet->bHandledClash) return;
    bHandledClash            = true;
    OtherBullet->bHandledClash = true;

    // Case 1: Fragment vs Fragment — apply contact damage, no further splits
    if (bIsCubeSplit && OtherBullet->bIsCubeSplit)
    {
        const FVector ClashPoint = GetActorLocation();
        TArray<FHitResult> Hits;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);
        Params.AddIgnoredActor(OtherBullet);
        GetWorld()->SweepMultiByChannel(
            Hits, ClashPoint, ClashPoint, FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeSphere(50.0f), Params);

        for (const FHitResult& Hit : Hits)
        {
            AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
            if (!IsValid(HitChar) || !IsValid(HitChar->HealthComponent)) continue;
            HitChar->HealthComponent->ApplyDamage(BaseDamage, OwnerInstigator.Get());
        }

        OtherBullet->ProjectileState = EProjectileState::Destroyed;
        OtherBullet->Destroy();
        ProjectileState = EProjectileState::Destroyed;
        Destroy();
        return;
    }

    // Case 2: Sniper vs non-Sniper — sniper pierces, non-sniper destroyed
    // Note: Sniper vs Sniper is caught above (no clash, both continue)
    if (bIsSniper || OtherBullet->bIsSniper)
    {
        if (bIsSniper)
        {
            OtherBullet->ProjectileState = EProjectileState::Destroyed;
            OtherBullet->Destroy();
            // This sniper continues flying
        }
        else
        {
            ProjectileState = EProjectileState::Destroyed;
            Destroy();
            // OtherBullet (sniper) continues flying
        }
        return;
    }

    // Case 3: Gunner vs Gunner — both spawn cube splits then destroy
    SpawnCubeSplits();
    OtherBullet->SpawnCubeSplits();

    OtherBullet->ProjectileState = EProjectileState::Destroyed;
    OtherBullet->Destroy();
    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

// ─── Cube Splits ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::SpawnCubeSplits()
{
    if (!HasAuthority()) return;
    if (CubeSplitCount <= 0) return;

    UWorld* World = GetWorld();
    if (!IsValid(World)) return;

    const float SplitDamage = BaseDamage / static_cast<float>(CubeSplitCount);
    const FVector Origin    = GetActorLocation();
    const FVector BaseDir   = ProjectileMovement->Velocity.GetSafeNormal();

    // Fan the splits evenly across 90° centred on the current velocity direction
    const float TotalSpreadDeg = 90.0f;
    const float StepDeg = (CubeSplitCount > 1)
        ? TotalSpreadDeg / static_cast<float>(CubeSplitCount - 1)
        : 0.0f;
    const float StartDeg = -TotalSpreadDeg * 0.5f;

    for (int32 i = 0; i < CubeSplitCount; ++i)
    {
        const float   Angle    = StartDeg + i * StepDeg;
        const FVector SplitDir = BaseDir.RotateAngleAxis(Angle, FVector::UpVector);
        const FTransform SpawnTF(
            FRotationMatrix::MakeFromX(SplitDir).Rotator(), Origin);

        AWTBRProjectileBase* Split = World->SpawnActorDeferred<AWTBRProjectileBase>(
            GetClass(), SpawnTF, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Split)) continue;

        Split->BaseDamage       = SplitDamage;
        Split->MaxRange         = MaxRange;
        Split->OwnerCategory    = OwnerCategory;
        Split->bIsCubeSplit     = true;
        Split->CubeSplitCount   = CubeSplitCount;
        Split->bIsSniper        = false;
        Split->bExplodeOnImpact = false;
        Split->CachedSpeed      = CachedSpeed;

        Split->FinishSpawning(SpawnTF);
        Split->Launch(SplitDir, OwnerInstigator.Get());
    }

    OnCubeSplitVFX();
}

// ─── Serpveil Path Movement ──────────────────────────────────────────────────

void AWTBRProjectileBase::InitializePathMovement(
    const TArray<FVector>& Points, float Speed, AActor* InInstigator)
{
    if (!HasAuthority()) return;
    if (Points.Num() < 2) return;
    if (IsActorBeingDestroyed() || !IsValid(InterpMovement))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR InitializePathMovement skipped: projectile %s is being destroyed or InterpMovement is invalid"),
            *GetNameSafe(this));
        return;
    }

    OwnerInstigator = InInstigator;

    // Straight-shot component must not compete with InterpToMovement
    ProjectileMovement->Deactivate();

    if (IsValid(BoxCollision))
    {
        InterpMovement->SetUpdatedComponent(BoxCollision);
    }

    // Convert incoming world-space points into actor-local control points.
    // UInterpToMovementComponent::ComputeMoveDelta() re-rotates every control
    // point by UpdatedComponent's rotation regardless of bPositionIsRelative
    // (that flag only skips adding StartLocation, not the rotation). Points
    // is already fully world-space — pre-rotated by fire Direction inside
    // WTBRSerpveilTrigger::BuildPathPoints() — and this actor is spawned with
    // rotation = Direction, so passing world points with bRelative=false
    // caused every point to be rotated a second time. Un-rotating into
    // actor-local space here cancels that out.
    const FVector StartLoc  = GetActorLocation();
    const FQuat   ActorQuat = GetActorQuat();

    InterpMovement->ResetControlPoints();
    for (const FVector& Pt : Points)
    {
        const FVector LocalPoint = ActorQuat.UnrotateVector(Pt - StartLoc);
        InterpMovement->AddControlPointPosition(LocalPoint, /*bRelative=*/true);
    }

    // Compute total path length to derive travel duration from speed
    float TotalDist = 0.0f;
    for (int32 i = 1; i < Points.Num(); ++i)
        TotalDist += FVector::Dist(Points[i - 1], Points[i]);

    PathTotalLength = TotalDist;
    PathStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    PathLaunchOrigin = Points[0];

    // The heading the cube will keep once the authored path runs out. Read from the
    // last leg rather than from the actor: a path-driven projectile never rotates,
    // so GetActorForwardVector() still points wherever it was spawned facing — which
    // for an arcing preset is the opposite of where it is actually going by the end.
    {
        const FVector FinalLeg = Points.Last() - Points[Points.Num() - 2];
        PathFinalDirection = FinalLeg.IsNearlyZero()
            ? GetActorForwardVector() : FinalLeg.GetSafeNormal();
    }

    // Duration must be set BEFORE FinaliseControlPoints() — it locks in
    // TimeMultiplier = 1/Duration internally and never recomputes it later.
    InterpMovement->Duration = (Speed > 0.0f) ? (TotalDist / Speed) : 1.0f;

    InterpMovement->FinaliseControlPoints();

    // Must run AFTER finalising: it rewrites the per-segment timing that call fills in.
    ApplyPathSpeedProfile();

    // ResetControlPoints() (called above) sets bStopped=true. TickComponent()
    // early-returns every frame while bStopped is true, and only
    // RestartMovement() clears it back to false — Activate() alone does NOT.
    // Without this call the component reports IsActive()=true (Activate()
    // still flips bIsActive/tick-enabled) but never actually advances
    // CurrentTime/moves, matching observed evidence: ActorLoc/BoxCollisionLoc
    // unchanged, Velocity=0, only overlap seen is the spawn-snap self-overlap.
    InterpMovement->RestartMovement();

    InterpMovement->OnInterpToStop.AddDynamic(
        this, &AWTBRProjectileBase::OnInterpMovementEnd);

    InterpMovement->Activate(true);

    ArmLaneEvents(AuthoredLaneEvents, TotalDist);

    // Clear any lifespan the projectile Blueprint set on itself.
    //
    // A bullet stops for three reasons: it hits somebody, it hits the world, or it
    // runs out of RANGE. A wall-clock timer is not one of them, and it silently
    // becomes one the moment a flight lasts longer than whatever number sits in the
    // Blueprint's Initial Life Span. Owner hit exactly that: Solux flies at 1000uu/s,
    // so an arcing preset takes five to eight seconds and the cubes were being culled
    // part-way down, at a different point on every lane because the lanes are
    // different lengths. Range takes over at the end of the path instead — see
    // OnInterpMovementEnd.
    SetLifeSpan(0.0f);

    // Start-of-path, so a cube that never reports [Path] Landed can be told apart
    // from one that was never given a path at all.
    WTBR_VALIDATION_LOG(Log,
        TEXT("[Path] Start | Cube=%s | Points=%d | Length=%.0fuu | Duration=%.2fs | MaxRange=%.0fuu | StartZ=%.0f | EndZ=%.0f | FinalDir=%s"),
        *GetNameSafe(this), Points.Num(), TotalDist, InterpMovement->Duration, MaxRange,
        Points[0].Z, Points.Last().Z, *PathFinalDirection.ToCompactString());

    OnProjectileLaunched();
}

// ─── Staggered launch ────────────────────────────────────────────────────────

void AWTBRProjectileBase::SchedulePathMovement(
    const TArray<FVector>& Points, float Speed, AActor* InInstigator, float DelaySeconds)
{
    if (!HasAuthority()) return;
    if (DelaySeconds <= 0.0f)
    {
        InitializePathMovement(Points, Speed, InInstigator);
        return;
    }

    OwnerInstigator = InInstigator;
    PendingPathPoints = Points;
    PendingPathSpeed = Speed;

    // The cube is conjured and visibly hovers while it waits, which is the canon
    // image — but it must not damage anything it happens to be spawned touching,
    // and it must not be shot out of the air before it has done anything.
    if (IsValid(BoxCollision))
    {
        BoxCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    GetWorldTimerManager().SetTimer(DelayedLaunchTimer, this,
        &AWTBRProjectileBase::OnDelayedLaunchElapsed, DelaySeconds, false);
}

void AWTBRProjectileBase::OnDelayedLaunchElapsed()
{
    if (!HasAuthority()) return;
    if (IsActorBeingDestroyed()) return;

    if (IsValid(BoxCollision))
    {
        BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    InitializePathMovement(PendingPathPoints, PendingPathSpeed, OwnerInstigator.Get());
    PendingPathPoints.Reset();
}

int32 AWTBRProjectileBase::SpawnSweptVolley(
    AWTBRCharacter* OwningCharacter,
    TSubclassOf<AWTBRProjectileBase> ProjectileClass,
    float TotalDamageBudget,
    float Speed,
    bool bExplodes,
    float ExplosionRadius,
    const FRotator& SpawnRotation,
    const TArray<TArray<FVector>>& CubeWorldPaths,
    const TArray<FWTBRResolvedCubeLaunch>& CubeLaunches,
    const FWTBRProjectileVFXConfig* VFXConfig,
    float HomingTurnRateDegPerSec,
    float MaxRangeUU,
    bool bReacquireAfterOvershoot,
    float DetonationRadiusUU,
    float OverrideHomingRadiusUU)
{
    if (!IsValid(OwningCharacter) || !ProjectileClass) return 0;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World || CubeWorldPaths.Num() == 0) return 0;

    // Budget is SPLIT across the volley, never multiplied — a preset that asks for
    // more cubes must not also hand out more damage.
    const float PerCubeDamage = TotalDamageBudget / static_cast<float>(CubeWorldPaths.Num());

    int32 SpawnedCount = 0;
    for (int32 CubeIndex = 0; CubeIndex < CubeWorldPaths.Num(); ++CubeIndex)
    {
        const TArray<FVector>& PathPoints = CubeWorldPaths[CubeIndex];
        if (PathPoints.Num() < 2) continue;

        // Spawn on this cube's own path start, not a shared muzzle point: cubes
        // conjured on one spot overlap and destroy each other before they move.
        const FTransform CubeSpawnTransform(SpawnRotation, PathPoints[0]);

        AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
            ProjectileClass,
            CubeSpawnTransform,
            OwningCharacter,
            OwningCharacter,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Projectile)) continue;

        Projectile->InitializeProjectile(
            PerCubeDamage, Speed, ETriggerCategory::Gunner,
            false, bExplodes, ExplosionRadius);

        // Reason three. Without this every preset and composite cube inherited the
        // 3000uu class default, so a shot authored to reach 7000 quietly stopped at
        // 3000 the moment its path ended.
        if (MaxRangeUU > 0.0f) Projectile->MaxRange = MaxRangeUU;

        const FWTBRResolvedCubeLaunch Launch = CubeLaunches.IsValidIndex(CubeIndex)
            ? CubeLaunches[CubeIndex] : FWTBRResolvedCubeLaunch();

        // Armed BEFORE the path starts — starting the path is what begins ticking.
        //
        // The weapon's own radius wins when it supplies one. That is what lets a
        // player-authored preset hunt at all: the editor no longer writes a per-lane
        // radius, and a SetHoming marker can only ever RESTORE the radius a cube was
        // armed with, so a cube armed at zero would ignore every marker on its path.
        const float EffectiveHomingRadius = OverrideHomingRadiusUU > 0.0f
            ? OverrideHomingRadiusUU : Launch.HomingRadiusUU;

        if (EffectiveHomingRadius > 0.0f)
        {
            Projectile->EnableProximityHoming(
                EffectiveHomingRadius, HomingTurnRateDegPerSec, bReacquireAfterOvershoot,
                DetonationRadiusUU);
        }

        // Per-composite look from the registry — keeps one shared projectile BP
        // viable. The standalone Trigger passes none and keeps its own BP's look.
        if (VFXConfig)
        {
            Projectile->ApplyVFXConfig(*VFXConfig);
        }

        Projectile->FinishSpawning(CubeSpawnTransform);
        // Stored, not armed: a lane with a LaunchDelay has no path length yet, and
        // the markers are placed as fractions OF that length. InitializePathMovement
        // arms them once it knows how long the path actually came out.
        Projectile->AuthoredLaneEvents = Launch.Events;
        Projectile->SchedulePathMovement(PathPoints, Speed, OwningCharacter, Launch.DelaySeconds);
        ++SpawnedCount;
    }

    return SpawnedCount;
}

// ─── Labyrn speed profile ────────────────────────────────────────────────────

void AWTBRProjectileBase::SetPathSpeedProfile(float SlowFromFraction, float EndSpeedFactor)
{
    PathSlowFromFraction = FMath::Clamp(SlowFromFraction, 0.0f, 0.99f);
    PathEndSpeedFactor = FMath::Clamp(EndSpeedFactor, 0.05f, 1.0f);
}

void AWTBRProjectileBase::ApplyPathSpeedProfile()
{
    if (PathSlowFromFraction <= 0.0f || PathEndSpeedFactor >= 1.0f) return;
    if (!IsValid(InterpMovement)) return;

    TArray<FInterpControlPoint>& Points = InterpMovement->ControlPoints;
    if (Points.Num() < 2) return;

    // FinaliseControlPoints has already filled DistanceToNext for every segment and
    // handed each one a time share equal to its share of the DISTANCE, which is
    // precisely why the flight is currently a flat speed.
    float TotalDistance = 0.0f;
    for (int32 i = 0; i < Points.Num() - 1; ++i)
    {
        TotalDistance += Points[i].DistanceToNext;
    }
    if (TotalDistance <= KINDA_SMALL_NUMBER) return;

    // Cost each segment as distance / speed, where speed eases from cruise down to
    // EndSpeedFactor across the closing stretch. Costs are relative — they are
    // normalised below — so this shapes the profile without changing arrival time.
    TArray<float> Costs;
    Costs.SetNumZeroed(Points.Num());
    float Travelled = 0.0f;
    float TotalCost = 0.0f;

    for (int32 i = 0; i < Points.Num() - 1; ++i)
    {
        const float SegmentDistance = Points[i].DistanceToNext;
        const float MidFraction = (Travelled + SegmentDistance * 0.5f) / TotalDistance;
        Travelled += SegmentDistance;

        float SpeedScale = 1.0f;
        if (MidFraction > PathSlowFromFraction)
        {
            const float IntoSlow =
                (MidFraction - PathSlowFromFraction) / (1.0f - PathSlowFromFraction);
            SpeedScale = FMath::Lerp(1.0f, PathEndSpeedFactor, FMath::Clamp(IntoSlow, 0.0f, 1.0f));
        }

        Costs[i] = SegmentDistance / FMath::Max(SpeedScale, KINDA_SMALL_NUMBER);
        TotalCost += Costs[i];
    }
    if (TotalCost <= KINDA_SMALL_NUMBER) return;

    // Duration is untouched, so the cube still lands exactly when it always would.
    // Only HOW it spends that time changes — which is what keeps a whole volley
    // arriving together while each cube still eases off at the end.
    float Elapsed = 0.0f;
    for (int32 i = 0; i < Points.Num() - 1; ++i)
    {
        Points[i].StartTime = Elapsed;
        Points[i].Percentage = Costs[i] / TotalCost;
        Elapsed += Points[i].Percentage;
    }
    Points.Last().StartTime = 1.0f;
    Points.Last().Percentage = 1.0f;
}


// ─── Lane events ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::ArmLaneEvents(const TArray<FWTBRLaneEvent>& Events, float PathLength)
{
    PendingLaneEvents.Reset();
    PendingLaneEventDistances.Reset();
    PathDistanceTravelled = 0.0f;
    LastEventSampleLocation = GetActorLocation();
    if (Events.Num() == 0 || PathLength <= 0.0f) return;

    TArray<FWTBRLaneEvent> Sorted = Events;
    Sorted.Sort([](const FWTBRLaneEvent& A, const FWTBRLaneEvent& B)
        { return A.AtPathFraction < B.AtPathFraction; });

    for (const FWTBRLaneEvent& Event : Sorted)
    {
        PendingLaneEvents.Add(Event);
        PendingLaneEventDistances.Add(
            FMath::Clamp(Event.AtPathFraction, 0.0f, 1.0f) * PathLength);
    }

    // Events need a per-frame position sample, so this is one of the few things that
    // turns ticking on. Same registration caveat as the sweep: bCanEverTick is true
    // in the constructor precisely so this is not a silent no-op.
    SetActorTickEnabled(true);
}

void AWTBRProjectileBase::TickLaneEvents()
{
    if (PendingLaneEvents.Num() == 0) return;
    if (bHoveringMidPath) return;

    const FVector Now = GetActorLocation();
    PathDistanceTravelled += FVector::Dist(LastEventSampleLocation, Now);
    LastEventSampleLocation = Now;

    // Measured by distance travelled, not by InterpToMovement's clock: a hover stops
    // the cube without stopping time, and a speed change breaks the time-to-distance
    // mapping outright. Distance is the thing the player actually clicked on.
    while (PendingLaneEvents.Num() > 0
        && PathDistanceTravelled >= PendingLaneEventDistances[0])
    {
        const FWTBRLaneEvent Event = PendingLaneEvents[0];
        PendingLaneEvents.RemoveAt(0);
        PendingLaneEventDistances.RemoveAt(0);
        ApplyLaneEvent(Event);

        // A hover suspends the path, so anything further along must wait for it.
        if (bHoveringMidPath) break;
    }
}

void AWTBRProjectileBase::ApplyLaneEvent(const FWTBRLaneEvent& Event)
{
    switch (Event.Type)
    {
    case EWTBRLaneEventType::Hover:
    {
        if (Event.DurationSeconds <= 0.0f || !IsValid(InterpMovement)) break;

        // Paused by switching the component's tick off, NOT by Deactivate().
        // Deactivate broadcasts OnInterpToStop, which lands in OnInterpMovementEnd —
        // the cube would treat a pause as the end of its path and fly off or die.
        InterpMovement->SetComponentTickEnabled(false);
        bHoveringMidPath = true;

        // Collision stays ON, unlike the pre-launch wait. A cube hanging in the open
        // for several seconds with nothing able to touch it would be free zoning;
        // being shootable is what pays for the timing trick.
        GetWorldTimerManager().SetTimer(HoverTimer, this,
            &AWTBRProjectileBase::EndHover, Event.DurationSeconds, false);

        WTBR_VALIDATION_LOG(Log, TEXT("[Lane Event] Hover | Cube=%s | Seconds=%.2f"),
            *GetNameSafe(this), Event.DurationSeconds);
        break;
    }

    case EWTBRLaneEventType::SetHoming:
    {
        // Only ever RESTORES a radius the weapon already had. A marker can never
        // grant hunting to something that does not hunt — Solux's cubes carrying no
        // special property at all is its identity, and Meteo and Viper have their
        // own. So this event belongs to the Hound line and nothing else.
        //
        // NOT behind the validation CVar: placed on any other archetype it is a
        // silent no-op, and a knob that does nothing without saying so is exactly the
        // kind of thing that gets tuned for an hour before anyone checks.
        if (Event.bEnable && ArmedHomingRadius <= 0.0f)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Lane Event] %s has a SetHoming marker but this projectile never ")
                TEXT("had homing to switch on. Only Hound-line weapons can use it — the ")
                TEXT("marker does nothing here."),
                *GetNameSafe(this));
            break;
        }

        if (Event.bEnable)
        {
            // Restoring the radius is what makes "fly straight, hunt only near the
            // end" authorable — the canon trick of setting tracking by time.
            ProximityHomingRadius = ArmedHomingRadius;
            if (ProximityHomingRadius > 0.0f) SetActorTickEnabled(true);
        }
        else
        {
            ProximityHomingRadius = 0.0f;
        }
        WTBR_VALIDATION_LOG(Log, TEXT("[Lane Event] SetHoming | Cube=%s | Enabled=%s | RadiusUU=%.0f"),
            *GetNameSafe(this), Event.bEnable ? TEXT("true") : TEXT("false"),
            ProximityHomingRadius);
        break;
    }

    // SetSpeed used to live here and was removed 2026-07-23 — see the note on
    // EWTBRLaneEventType. It scaled CustomTimeDilation, which compounded rather than
    // set, and does not replicate.
    }
}

void AWTBRProjectileBase::EndHover()
{
    bHoveringMidPath = false;
    if (IsValid(InterpMovement))
    {
        InterpMovement->SetComponentTickEnabled(true);
    }
    // Resampled so the pause itself does not count as distance travelled.
    LastEventSampleLocation = GetActorLocation();
}

// ─── Venyx proximity homing ──────────────────────────────────────────────────

void AWTBRProjectileBase::EnableProximityHoming(
    float RadiusUU, float TurnRateDegPerSec, bool bReacquireAfterOvershoot,
    float DetonationRadiusUU)
{
    if (!HasAuthority()) return;
    if (RadiusUU <= 0.0f) return;

    ProximityHomingRadius = RadiusUU;
    ArmedHomingRadius = RadiusUU;
    HomingTurnRateDegreesPerSecond = TurnRateDegPerSec;
    ReacquireRadius = bReacquireAfterOvershoot ? RadiusUU : 0.0f;
    ProximityDetonationRadius = FMath::Max(0.0f, DetonationRadiusUU);

    // Ticking is off by default on every projectile and stays off for all of them
    // except these — the sweep is the only thing here that needs a per-frame query.
    SetActorTickEnabled(true);

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Proximity Homing] Armed | Cube=%s | RadiusUU=%.0f | TurnRate=%.0fdeg/s | Reacquire=%s | DetonateUU=%.0f | Ticking=%s"),
        *GetNameSafe(this), RadiusUU, TurnRateDegPerSec,
        bReacquireAfterOvershoot ? TEXT("true") : TEXT("false"),
        ProximityDetonationRadius,
        IsActorTickEnabled() ? TEXT("true") : TEXT("false"));
}

AWTBRCharacter* AWTBRProjectileBase::FindProximityHomingTarget(
    const AWTBRCharacter* Shooter,
    const FVector& CubeLocation,
    float RadiusUU,
    const TArray<AWTBRCharacter*>& Candidates)
{
    if (RadiusUU <= 0.0f) return nullptr;

    const float RadiusSq = FMath::Square(RadiusUU);
    AWTBRCharacter* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();

    for (AWTBRCharacter* Candidate : Candidates)
    {
        if (!IsValid(Candidate) || Candidate == Shooter) continue;
        if (!IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsAlive()) continue;
        if (IsValid(Shooter) && Shooter->IsSameTeamAs(Candidate)) continue;

        const float DistSq = FVector::DistSquared(CubeLocation, Candidate->GetActorLocation());
        if (DistSq > RadiusSq || DistSq >= BestDistSq) continue;

        // Nearest wins, so two cubes sweeping the same ground behave the same way
        // regardless of actor iteration order.
        Best = Candidate;
        BestDistSq = DistSq;
    }
    return Best;
}

FVector AWTBRProjectileBase::SteerTowards(
    const FVector& Current, const FVector& Desired, float MaxRadians)
{
    if (Current.IsNearlyZero()) return Desired;
    if (Desired.IsNearlyZero() || MaxRadians <= 0.0f) return Current;

    const float Dot = FMath::Clamp(FVector::DotProduct(Current, Desired), -1.0f, 1.0f);
    const float Angle = FMath::Acos(Dot);
    if (Angle <= MaxRadians) return Desired;

    FVector Axis = FVector::CrossProduct(Current, Desired);
    if (Axis.IsNearlyZero())
    {
        // Current and Desired are collinear. Same direction was already handled by
        // the angle check, so this is the exactly-opposite case: a cube that flew
        // straight through its target and now needs a full reversal. There is no
        // natural turn plane, and returning Current would leave it flying away
        // forever — so pick any perpendicular and start the turn.
        Axis = FVector::CrossProduct(Current, FVector::UpVector);
        if (Axis.IsNearlyZero())
        {
            Axis = FVector::CrossProduct(Current, FVector::RightVector);
        }
    }

    return Current.RotateAngleAxis(
        FMath::RadiansToDegrees(MaxRadians), Axis.GetSafeNormal()).GetSafeNormal();
}

void AWTBRProjectileBase::TickProximityChase(float DeltaSeconds)
{
    AWTBRCharacter* Target = ProximityChaseTarget.Get();
    if (!IsValid(Target) || !IsValid(ProjectileMovement))
    {
        ProximityChaseTarget = nullptr;
        return;
    }

    const FVector Current = ProjectileMovement->Velocity.IsNearlyZero()
        ? GetActorForwardVector()
        : ProjectileMovement->Velocity.GetSafeNormal();
    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    const FVector Desired = ToTarget.GetSafeNormal();

    // Fuse, early check: a head-on cube that closes inside the (small) fuse radius
    // detonates immediately, giving the tightest airburst. Checked BEFORE the
    // overshoot test on purpose — it should resolve the shot, not drop the target
    // because it is about to fly past.
    if (ProximityDetonationRadius > 0.0f
        && ToTarget.SizeSquared() <= FMath::Square(ProximityDetonationRadius))
    {
        WTBR_VALIDATION_LOG(Log,
            TEXT("[Proximity Homing] Detonate | Cube=%s | Target=%s | Dist=%.0fuu | Reason=Fuse | FuseUU=%.0f"),
            *GetNameSafe(this), *GetNameSafe(Target),
            ToTarget.Size(), ProximityDetonationRadius);
        DetonateOnProximityTarget(Target);
        return;
    }

    // Overshoot = the target just crossed behind the cube, which means THIS tick is
    // the closest the cube will ever get on this pass. A turn-capped cube physically
    // cannot pinpoint-hit a target inside its turn radius, so closest approach is the
    // honest moment to resolve the shot.
    if (FVector::DotProduct(Current, Desired) < 0.0f)
    {
        // Fuse weapons connect at closest approach. The ceiling is the radius the cube
        // ACQUIRED within, so "you already earned this target, now cash it in" — a
        // committed chase always resolves into exactly one hit at its best point
        // rather than flying past. This supersedes the old drop-and-reacquire, which
        // could not reconverge on a target tighter than the turn radius and instead
        // spiralled OUTWARD re-acquiring at ever greater distance (owner PIE log:
        // 344 -> 567uu, never connecting). Gated on a fuse being authored, so a preset
        // that wants the old relentless wide-return sweep opts out by leaving it zero.
        if (ProximityDetonationRadius > 0.0f)
        {
            const float DetonateCeiling = FMath::Max(ArmedHomingRadius, ProximityDetonationRadius);
            if (ToTarget.SizeSquared() <= FMath::Square(DetonateCeiling))
            {
                WTBR_VALIDATION_LOG(Log,
                    TEXT("[Proximity Homing] Detonate | Cube=%s | Target=%s | Dist=%.0fuu | Reason=ClosestApproach | CeilingUU=%.0f"),
                    *GetNameSafe(this), *GetNameSafe(Target),
                    ToTarget.Size(), DetonateCeiling);
                DetonateOnProximityTarget(Target);
                return;
            }
        }

        // No fuse (or the target slipped beyond even the acquisition radius — a
        // teleport or an edge-of-radius clip): fall back to the relentless behaviour
        // if this weapon re-hunts. Venyx commits once and a miss is final, so it keeps
        // hauling itself round; a composite that may re-hunt drops the target and
        // resumes sweeping, still paying the turn-cap price to come back round.
        if (ReacquireRadius > 0.0f)
        {
            ProximityChaseTarget = nullptr;
            ProximityHomingRadius = ReacquireRadius;
            WTBR_VALIDATION_LOG(Log,
                TEXT("[Proximity Homing] Reacquiring | Cube=%s | LostTarget=%s"),
                *GetNameSafe(this), *GetNameSafe(Target));
            return;
        }
    }

    // Zero turn rate means uncapped, which is the old behaviour: snap onto the
    // target every frame.
    const float MaxRadians = (HomingTurnRateDegreesPerSecond > 0.0f)
        ? FMath::DegreesToRadians(HomingTurnRateDegreesPerSecond) * DeltaSeconds
        : PI;

    // Speed is held constant. Steering changes where the cube is going, never how
    // fast — otherwise the turn cap would double as a brake and the damage numbers
    // would quietly depend on how sharply someone dodged.
    ProjectileMovement->Velocity = SteerTowards(Current, Desired, MaxRadians) * CachedSpeed;
}

void AWTBRProjectileBase::DetonateOnProximityTarget(AWTBRCharacter* Target)
{
    if (!HasAuthority() || !IsValid(Target)) return;

    // Route through the real overlap handler rather than duplicating the damage,
    // hit-zone, VFX and destruction logic — a proximity detonation should be
    // indistinguishable from a contact hit on the same target. The synthetic
    // SweepResult pins the impact point to the target centre so the fuse always reads
    // as a torso hit (1.0x); a fuse is an area effect, not a pinpoint shot, so it must
    // not randomly catch a head or limb multiplier from wherever the cube happened to
    // be. bExplodeOnImpact is honoured inside OnOverlapBegin, so an explosive cube
    // still explodes here exactly as it would on contact.
    FHitResult SyntheticHit;
    SyntheticHit.ImpactPoint = Target->GetActorLocation();
    SyntheticHit.Location = Target->GetActorLocation();

    UPrimitiveComponent* TargetComponent =
        Cast<UPrimitiveComponent>(Target->GetRootComponent());
    OnOverlapBegin(BoxCollision, Target, TargetComponent, 0, /*bFromSweep=*/true, SyntheticHit);
}

void AWTBRProjectileBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority()) return;

    TickLaneEvents();

    // Already committed: steer, and never look for anyone else.
    if (ProximityChaseTarget.IsValid())
    {
        TickProximityChase(DeltaSeconds);
        return;
    }

    if (ProximityHomingRadius <= 0.0f) return;

    // Both movers are allowed to sweep. A path-driven cube (Venyx presets and the
    // composites) rides InterpToMovement; a tap-fired cube flies straight on
    // ProjectileMovement. This used to require an active InterpToMovement, which
    // silently excluded every tap — the cube ticked and returned on the first line
    // every frame, so nothing looked broken from the outside.
    const bool bOnPath = IsValid(InterpMovement) && InterpMovement->IsActive();
    const bool bFlyingStraight = IsValid(ProjectileMovement) && ProjectileMovement->IsActive();
    if (!bOnPath && !bFlyingStraight) return;

    UWorld* World = GetWorld();
    if (!World) return;

    AWTBRCharacter* Shooter = Cast<AWTBRCharacter>(OwnerInstigator.Get());
    const FVector CubeLocation = GetActorLocation();

    TArray<AWTBRCharacter*> Candidates;
    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        Candidates.Add(*It);
    }

    AWTBRCharacter* Target =
        FindProximityHomingTarget(Shooter, CubeLocation, ProximityHomingRadius, Candidates);
    if (!IsValid(Target))
    {
        // Tracked every frame but never logged here. "How close did this sweep
        // actually get" is the number that says whether the radius is slightly too
        // small or the arc simply goes nowhere near anyone — and it is only known
        // once the whole sweep is over, so it is reported at expiry instead.
        for (AWTBRCharacter* Candidate : Candidates)
        {
            if (!IsValid(Candidate) || Candidate == Shooter) continue;
            if (IsValid(Shooter) && Shooter->IsSameTeamAs(Candidate)) continue;
            ClosestSweepApproachSq = FMath::Min(ClosestSweepApproachSq,
                FVector::DistSquared(CubeLocation, Candidate->GetActorLocation()));
        }
        return;
    }

    // A sweep that locks through a wall would let a cube hunt someone the shooter
    // never saw, which is the same see-through-cover problem the project avoids
    // elsewhere. Cover therefore beats a sweep, and that is the counterplay.
    FHitResult Blocking;
    FCollisionQueryParams LineParams(SCENE_QUERY_STAT(WTBRProximityHomingLOS), false, this);
    LineParams.AddIgnoredActor(Target);
    if (IsValid(Shooter)) LineParams.AddIgnoredActor(Shooter);
    if (World->LineTraceSingleByChannel(
            Blocking, CubeLocation, Target->GetActorLocation(), ECC_Visibility, LineParams))
    {
        // Also once only. This is the single most likely reason a sweep that LOOKS
        // like it should have connected did not, so it names what blocked it.
        if (!bLoggedSweepLOSBlock)
        {
            bLoggedSweepLOSBlock = true;
            WTBR_VALIDATION_LOG(Log,
                TEXT("[Proximity Homing] LOSBlocked | Cube=%s | Target=%s | BlockedBy=%s"),
                *GetNameSafe(this), *GetNameSafe(Target),
                *GetNameSafe(Blocking.GetActor()));
        }
        return;
    }

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Proximity Homing] Acquired | Cube=%s | Target=%s | Dist=%.0fuu | RadiusUU=%.0f"),
        *GetNameSafe(this), *GetNameSafe(Target),
        FVector::Dist(CubeLocation, Target->GetActorLocation()), ProximityHomingRadius);

    BeginProximityChase(Target);
}

void AWTBRProjectileBase::BeginProximityChase(AWTBRCharacter* Target)
{
    if (!HasAuthority() || !IsValid(Target)) return;

    // The cube keeps the heading it already had, so the turn reads as a peel-off
    // rather than a snap toward the target. Which mover holds that heading depends
    // on how the cube was fired.
    const bool bOnPath = IsValid(InterpMovement) && InterpMovement->IsActive();

    FVector ContinueDirection = FVector::ZeroVector;
    if (bOnPath)
    {
        ContinueDirection = InterpMovement->Velocity;
    }
    else if (IsValid(ProjectileMovement))
    {
        ContinueDirection = ProjectileMovement->Velocity;
    }
    ContinueDirection = ContinueDirection.IsNearlyZero()
        ? GetActorForwardVector()
        : ContinueDirection.GetSafeNormal();

    if (bOnPath)
    {
        // MUST be set before Deactivate(). Deactivating InterpToMovement broadcasts
        // OnInterpToStop, which lands in OnInterpMovementEnd, which destroys the
        // projectile — so without this guard a cube killed itself the instant it
        // acquired anything, mid-way through this very function. It looked exactly
        // like homing never working at all.
        bTransitioningToProximityChase = true;
        InterpMovement->Deactivate();
    }

    USceneComponent* DesiredUpdatedComponent = IsValid(BoxCollision)
        ? static_cast<USceneComponent*>(BoxCollision) : RootComponent;
    if (IsValid(DesiredUpdatedComponent))
    {
        ProjectileMovement->SetUpdatedComponent(DesiredUpdatedComponent);
    }

    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    ProjectileMovement->Velocity = ContinueDirection * CachedSpeed;
    ProjectileMovement->Activate();

    // Deliberately NOT EnableHoming(). UProjectileMovementComponent's homing has no
    // turn limit and no way to add one, so a cube that overshot pivoted on the spot
    // and came straight back, repeatedly, until it expired. Steering is driven from
    // Tick instead — see TickProximityChase.
    ProximityChaseTarget = Target;

    // Stop sweeping: the query is the expensive part and the cube is committed now.
    // Ticking STAYS on, because the chase itself is what runs there.
    ProximityHomingRadius = 0.0f;
    bTransitioningToProximityChase = false;
}

void AWTBRProjectileBase::OnInterpMovementEnd(const FHitResult& /*ImpactResult*/, float /*Time*/)
{
    if (!HasAuthority()) return;

    // Re-entered from BeginProximityChase's own Deactivate() call. The path did not
    // end — the cube is leaving it early to chase something — so destroying here
    // would kill the cube at the exact moment it found a target.
    if (bTransitioningToProximityChase) return;

    // Flight time per cube. Lanes of DIFFERENT lengths reporting the SAME flight is
    // the only direct proof that a synchronised volley actually synchronised —
    // equal-length lanes agreeing proves nothing, since they would anyway.
    if (PathTotalLength > 0.0f && GetWorld())
    {
        const float Flight = GetWorld()->GetTimeSeconds() - PathStartWorldTime;
        WTBR_VALIDATION_LOG(Log,
            TEXT("[Path] Landed | Cube=%s | Flight=%.3fs | Length=%.0fuu | AvgSpeed=%.0f"),
            *GetNameSafe(this), Flight, PathTotalLength,
            Flight > KINDA_SMALL_NUMBER ? PathTotalLength / Flight : 0.0f);
    }

    if (bHomeAfterPathEnd && PathEndHomingTarget.IsValid())
    {
        InterpMovement->Deactivate();

        USceneComponent* DesiredUpdatedComponent = IsValid(BoxCollision)
            ? static_cast<USceneComponent*>(BoxCollision) : RootComponent;
        if (IsValid(DesiredUpdatedComponent))
        {
            ProjectileMovement->SetUpdatedComponent(DesiredUpdatedComponent);
        }

        ProjectileMovement->StopMovementImmediately();
        ProjectileMovement->Deactivate();
        ProjectileMovement->Velocity = PathEndContinueDirection.GetSafeNormal() * CachedSpeed;
        ProjectileMovement->Activate();

        EnableHoming(PathEndHomingTarget.Get(), PathEndHomingAcceleration);
        return;
    }

    // Closes the loop on a sweep: without this a cube that hunted nobody just
    // vanishes, which looks exactly like a cube whose homing never ran at all.
    if (ProximityHomingRadius > 0.0f)
    {
        const bool bSawAnyone = ClosestSweepApproachSq < TNumericLimits<float>::Max();
        const float Closest = bSawAnyone ? FMath::Sqrt(ClosestSweepApproachSq) : -1.0f;
        WTBR_VALIDATION_LOG(Log,
            TEXT("[Proximity Homing] Expired | Cube=%s | ClosestApproach=%.0fuu | RadiusUU=%.0f | ShortBy=%.0fuu"),
            *GetNameSafe(this), Closest, ProximityHomingRadius,
            bSawAnyone ? FMath::Max(0.0f, Closest - ProximityHomingRadius) : -1.0f);
    }

    // The authored path is finished, NOT the shot. A bullet stops for three reasons
    // and "the designer's curve ran out" is not one of them — so the cube keeps
    // flying along its last heading until it hits somebody, hits the world, or runs
    // out of range.
    //
    // Owner found this in PIE on an arcing Solux preset: the cubes climbed, turned
    // over, and vanished mid-dive at about chest height, having touched nothing. The
    // path had simply ended in open air.
    const float Travelled = FVector::Dist(PathLaunchOrigin, GetActorLocation());
    const float Remaining = MaxRange - Travelled;

    if (Remaining <= 0.0f || CachedSpeed <= 0.0f)
    {
        // Reason three, legitimately reached.
        ProjectileState = EProjectileState::Destroyed;
        Destroy();
        return;
    }

    USceneComponent* ContinueComponent = IsValid(BoxCollision)
        ? static_cast<USceneComponent*>(BoxCollision) : RootComponent;
    if (IsValid(ContinueComponent))
    {
        ProjectileMovement->SetUpdatedComponent(ContinueComponent);
    }

    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    ProjectileMovement->Velocity = PathFinalDirection * CachedSpeed;
    ProjectileMovement->Activate();

    // Range is what ends it now. Constant speed, so a time budget IS a distance
    // budget — the same way Launch() bounds an ordinary straight shot.
    SetLifeSpan(Remaining / CachedSpeed);

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Path] Continue | Cube=%s | Travelled=%.0fuu | MaxRange=%.0fuu | Remaining=%.0fuu"),
        *GetNameSafe(this), Travelled, MaxRange, Remaining);
}

// ─── Replication ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnRep_ProjectileState()
{
    // Blueprint VFX hook: switch on ProjectileState to drive client-side effects
}
