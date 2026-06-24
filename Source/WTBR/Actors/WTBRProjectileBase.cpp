// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "WTBRCharacter.h"
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
    BoxCollision->SetGenerateOverlapEvents(true);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
        TEXT("ProjectileMovement"));
    ProjectileMovement->bAutoActivate           = false;
    ProjectileMovement->InitialSpeed            = 0.0f;
    ProjectileMovement->MaxSpeed                = 0.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale   = 0.0f;
}

void AWTBRProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        BoxCollision->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRProjectileBase::OnOverlapBegin);
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

    OwnerInstigator = InInstigator;
    const FVector SafeDir = Direction.GetSafeNormal();

    ProjectileMovement->Velocity = SafeDir * CachedSpeed;
    ProjectileMovement->Activate();

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
    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;
    if (OtherActor == OwnerInstigator) return;

    // Bullet vs Bullet
    if (AWTBRProjectileBase* OtherBullet = Cast<AWTBRProjectileBase>(OtherActor))
    {
        OnBulletClash(OtherBullet);
        return;
    }

    // Explosive: detonate on any hit (character or geometry)
    if (bExplodeOnImpact)
    {
        TriggerExplosion();
        return;
    }

    // Direct hit on character
    AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor);
    if (IsValid(HitChar))
    {
        const FVector ImpactPoint = SweepResult.bBlockingHit
            ? SweepResult.ImpactPoint : GetActorLocation();

        OnProjectileHitVFX(ImpactPoint);

        if (IsValid(HitChar->HealthComponent))
            HitChar->HealthComponent->ApplyDamage(BaseDamage, OwnerInstigator.Get());

        ProjectileState = EProjectileState::Destroyed;
        Destroy();
    }
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

    // Case 2: Sniper pierces — destroy non-sniper, sniper continues
    const bool bThisSniper  = bIsSniper;
    const bool bOtherSniper = OtherBullet->bIsSniper;
    if (bThisSniper || bOtherSniper)
    {
        if (bThisSniper && !bOtherSniper)
        {
            OtherBullet->ProjectileState = EProjectileState::Destroyed;
            OtherBullet->Destroy();
            // This sniper continues flying — do not Destroy()
        }
        else if (!bThisSniper && bOtherSniper)
        {
            ProjectileState = EProjectileState::Destroyed;
            Destroy();
            // OtherBullet (sniper) continues flying
        }
        else
        {
            // Sniper vs Sniper: symmetric destruction
            OtherBullet->ProjectileState = EProjectileState::Destroyed;
            OtherBullet->Destroy();
            ProjectileState = EProjectileState::Destroyed;
            Destroy();
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

// ─── Replication ─────────────────────────────────────────────────────────────

void AWTBRProjectileBase::OnRep_ProjectileState()
{
    // Blueprint VFX hook: switch on ProjectileState to drive client-side effects
}
