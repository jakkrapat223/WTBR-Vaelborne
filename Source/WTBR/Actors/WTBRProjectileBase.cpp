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

AWTBRProjectileBase::AWTBRProjectileBase()
{
    PrimaryActorTick.bCanEverTick = false;
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
}

void AWTBRProjectileBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRProjectileBase, ProjectileState);
}

// ─── Core Interface ───────────────────────────────────────────────────────────

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
        const FVector ImpactPoint = SweepResult.bBlockingHit
            ? SweepResult.ImpactPoint : GetActorLocation();

        if (OwnerCategory == ETriggerCategory::Gunner)
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] HitAccepted | Projectile=%s | Target=%s | Impact=%s | Damage=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(HitChar),
                *ImpactPoint.ToString(),
                BaseDamage);
        }

        OnProjectileHitVFX(ImpactPoint);
        DamagedActors.Add(OtherActor); // track before applying so re-entry is blocked

        if (IsValid(HitChar->HealthComponent))
        {
            const float OldHP = HitChar->HealthComponent->GetCurrentHP();
            WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileDamageApply | Projectile=%s | Attacker=%s | Target=%s | OldHP=%.1f | Damage=%.1f | ApplyDamageCalled=true"),
                *GetNameSafe(this),
                *GetNameSafe(OwnerInstigator.Get()),
                *GetNameSafe(HitChar),
                OldHP,
                BaseDamage);
            HitChar->HealthComponent->ApplyDamage(BaseDamage, OwnerInstigator.Get());
            const float NewHP = HitChar->HealthComponent->GetCurrentHP();
            WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ProjectileDamageResult | Projectile=%s | Attacker=%s | Target=%s | OldHP=%.1f | Damage=%.1f | NewHP=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(OwnerInstigator.Get()),
                *GetNameSafe(HitChar),
                OldHP,
                BaseDamage,
                NewHP);
            if (OwnerCategory == ETriggerCategory::Gunner)
            {
                WTBR_VALIDATION_LOG(Verbose, TEXT("[Acervyn Retest] DamageApplied | Projectile=%s | Target=%s | Damage=%.1f | OldHP=%.1f | NewHP=%.1f | DamagedActors=%d"),
                    *GetNameSafe(this),
                    *GetNameSafe(HitChar),
                    BaseDamage,
                    OldHP,
                    NewHP,
                    DamagedActors.Num());
            }
            if (OwnerCategory == ETriggerCategory::SniperBullet)
            {
                WTBR_VALIDATION_LOG(Verbose, TEXT("[Fulgris Test] DamageApplied | Target=%s | Damage=%.1f | OldHP=%.1f | NewHP=%.1f"),
                    *GetNameSafe(HitChar),
                    BaseDamage,
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

    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

// ─── Explosion ───────────────────────────────────────────────────────────────

void AWTBRProjectileBase::TriggerExplosion()
{
    if (!HasAuthority()) return;

    const FVector Center = GetActorLocation();

    TArray<FHitResult> Hits;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (IsValid(OwnerInstigator))
        Params.AddIgnoredActor(OwnerInstigator.Get());

    GetWorld()->SweepMultiByChannel(
        Hits, Center, Center, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeSphere(ExplosionRadius), Params);

    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar) || !IsValid(HitChar->HealthComponent)) continue;
        HitChar->HealthComponent->ApplyDamage(BaseDamage, OwnerInstigator.Get());
    }

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Center, ExplosionRadius, 16,
        FColor::Orange, false, 0.5f);
#endif

    ProjectileState = EProjectileState::Exploded;
    OnExplosionVFX(Center, ExplosionRadius);
    Destroy();
}

// ─── Bullet Clash ────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnBulletClash(AWTBRProjectileBase* OtherBullet)
{
    if (!HasAuthority()) return;
    if (!IsValid(OtherBullet)) return;
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

void AWTBRProjectileBase::OnInterpMovementEnd(const FHitResult& /*ImpactResult*/, float /*Time*/)
{
    if (!HasAuthority()) return;
    ProjectileState = EProjectileState::Destroyed;
    Destroy();
}

// ─── Replication ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnRep_ProjectileState()
{
    // Blueprint VFX hook: switch on ProjectileState to drive client-side effects
}
