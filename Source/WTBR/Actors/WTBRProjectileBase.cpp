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

    // Duration must be set BEFORE FinaliseControlPoints() — it locks in
    // TimeMultiplier = 1/Duration internally and never recomputes it later.
    InterpMovement->Duration = (Speed > 0.0f) ? (TotalDist / Speed) : 1.0f;

    InterpMovement->FinaliseControlPoints();

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
    float HomingAcceleration,
    const FRotator& SpawnRotation,
    const TArray<TArray<FVector>>& CubeWorldPaths,
    const TArray<FWTBRResolvedCubeLaunch>& CubeLaunches,
    const FWTBRProjectileVFXConfig* VFXConfig)
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

        const FWTBRResolvedCubeLaunch Launch = CubeLaunches.IsValidIndex(CubeIndex)
            ? CubeLaunches[CubeIndex] : FWTBRResolvedCubeLaunch();

        // Armed BEFORE the path starts — starting the path is what begins ticking.
        if (Launch.HomingRadiusUU > 0.0f)
        {
            Projectile->EnableProximityHoming(Launch.HomingRadiusUU, HomingAcceleration);
        }

        // Per-composite look from the registry — keeps one shared projectile BP
        // viable. The standalone Trigger passes none and keeps its own BP's look.
        if (VFXConfig)
        {
            Projectile->ApplyVFXConfig(*VFXConfig);
        }

        Projectile->FinishSpawning(CubeSpawnTransform);
        Projectile->SchedulePathMovement(PathPoints, Speed, OwningCharacter, Launch.DelaySeconds);
        ++SpawnedCount;
    }

    return SpawnedCount;
}

// ─── Venyx proximity homing ──────────────────────────────────────────────────

void AWTBRProjectileBase::EnableProximityHoming(float RadiusUU, float Accel)
{
    if (!HasAuthority()) return;
    if (RadiusUU <= 0.0f) return;

    ProximityHomingRadius = RadiusUU;
    ProximityHomingAcceleration = Accel;

    // Ticking is off by default on every projectile and stays off for all of them
    // except these — the sweep is the only thing here that needs a per-frame query.
    SetActorTickEnabled(true);

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Venyx Sweep] Armed | Cube=%s | RadiusUU=%.0f | Accel=%.0f | Ticking=%s"),
        *GetNameSafe(this), RadiusUU, Accel,
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

void AWTBRProjectileBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority() || ProximityHomingRadius <= 0.0f) return;
    // Once the cube is chasing, the sweep is over — it does not re-acquire.
    if (!IsValid(InterpMovement) || !InterpMovement->IsActive()) return;

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
                TEXT("[Venyx Sweep] LOSBlocked | Cube=%s | Target=%s | BlockedBy=%s"),
                *GetNameSafe(this), *GetNameSafe(Target),
                *GetNameSafe(Blocking.GetActor()));
        }
        return;
    }

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Venyx Sweep] Acquired | Cube=%s | Target=%s | Dist=%.0fuu | RadiusUU=%.0f"),
        *GetNameSafe(this), *GetNameSafe(Target),
        FVector::Dist(CubeLocation, Target->GetActorLocation()), ProximityHomingRadius);

    BeginProximityChase(Target);
}

void AWTBRProjectileBase::BeginProximityChase(AWTBRCharacter* Target)
{
    if (!HasAuthority() || !IsValid(Target)) return;

    // Same handoff OnInterpMovementEnd performs, except it happens mid-path: the
    // cube keeps the heading it already had so the turn reads as a peel-off rather
    // than a snap toward the target.
    const FVector ContinueDirection = InterpMovement->Velocity.IsNearlyZero()
        ? GetActorForwardVector()
        : InterpMovement->Velocity.GetSafeNormal();

    // MUST be set before Deactivate(). Deactivating InterpToMovement broadcasts
    // OnInterpToStop, which lands in OnInterpMovementEnd, which destroys the
    // projectile — so without this guard a cube killed itself the instant it
    // acquired anything, mid-way through this very function. It looked exactly
    // like homing never working at all.
    bTransitioningToProximityChase = true;

    InterpMovement->Deactivate();

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

    EnableHoming(Target->GetRootComponent(), ProximityHomingAcceleration);

    // Stop sweeping: the query is the expensive part and the cube is committed now.
    ProximityHomingRadius = 0.0f;
    SetActorTickEnabled(false);
    bTransitioningToProximityChase = false;
}

void AWTBRProjectileBase::OnInterpMovementEnd(const FHitResult& /*ImpactResult*/, float /*Time*/)
{
    if (!HasAuthority()) return;

    // Re-entered from BeginProximityChase's own Deactivate() call. The path did not
    // end — the cube is leaving it early to chase something — so destroying here
    // would kill the cube at the exact moment it found a target.
    if (bTransitioningToProximityChase) return;

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
            TEXT("[Venyx Sweep] Expired | Cube=%s | ClosestApproach=%.0fuu | RadiusUU=%.0f | ShortBy=%.0fuu"),
            *GetNameSafe(this), Closest, ProximityHomingRadius,
            bSawAnyone ? FMath::Max(0.0f, Closest - ProximityHomingRadius) : -1.0f);
    }

    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

// ─── Replication ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnRep_ProjectileState()
{
    // Blueprint VFX hook: switch on ProjectileState to drive client-side effects
}
