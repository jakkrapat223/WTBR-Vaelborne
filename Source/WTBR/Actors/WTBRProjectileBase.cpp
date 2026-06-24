// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InterpToMovementComponent.h"
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
    if (DamagedActors.Contains(OtherActor)) return; // skip already-hit actors (penetration guard)

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

        OnProjectileHitVFX(ImpactPoint);
        DamagedActors.Add(OtherActor); // track before applying so re-entry is blocked

        if (IsValid(HitChar->HealthComponent))
            HitChar->HealthComponent->ApplyDamage(BaseDamage, OwnerInstigator.Get());

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

    OwnerInstigator = InInstigator;

    // Straight-shot component must not compete with InterpToMovement
    ProjectileMovement->Deactivate();

    InterpMovement->ResetControlPoints();
    for (const FVector& Pt : Points)
        InterpMovement->AddControlPointPosition(Pt, /*bRelative=*/false);
    InterpMovement->FinaliseControlPoints();

    // Compute total path length to derive travel duration from speed
    float TotalDist = 0.0f;
    for (int32 i = 1; i < Points.Num(); ++i)
        TotalDist += FVector::Dist(Points[i - 1], Points[i]);

    InterpMovement->Duration = (Speed > 0.0f) ? (TotalDist / Speed) : 1.0f;

    InterpMovement->OnInterpToStop.AddDynamic(
        this, &AWTBRProjectileBase::OnInterpMovementEnd);

    InterpMovement->Activate();

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
