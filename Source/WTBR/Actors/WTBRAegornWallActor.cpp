// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AWTBRAegornWallActor::AWTBRAegornWallActor()
{
    // Tick only drives the Escudo ground-eruption animation (BeginEscudoEruption
    // enables it explicitly, server-only); off by default so a normal
    // Aegorn Shield or a settled Escudo wall costs nothing per-frame.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    // A held Aegorn shield is repositioned ~20x/sec on the server
    // (UWTBRAegornTrigger::TickHeldShield); replicate that movement and update
    // often so clients see it track the caster's aim instead of lagging behind
    // at a stale/crooked transform.
    SetReplicateMovement(true);
    NetUpdateFrequency = 60.0f;
    MinNetUpdateFrequency = 30.0f;

    WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WallMesh"));
    RootComponent = WallMesh;

    WallCollision = CreateDefaultSubobject<UBoxComponent>(
        TEXT("WallCollision"));
    WallCollision->SetupAttachment(RootComponent);
    WallCollision->SetBoxExtent(FVector(10.0f, 150.0f, 150.0f));
    WallCollision->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);
    WallCollision->SetCollisionObjectType(ECC_WorldStatic);
    WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
    WallCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    WallCollision->SetGenerateOverlapEvents(true);

    WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WallMesh->SetGenerateOverlapEvents(false);
}

void AWTBRAegornWallActor::BeginPlay()
{
    Super::BeginPlay();
    Tags.Add(TEXT("AegornWall"));
    if (IsValid(WallMesh))
    {
        WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WallMesh->SetGenerateOverlapEvents(false);
    }
    if (IsValid(WallCollision))
    {
        WallCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WallCollision->SetCollisionObjectType(ECC_WorldStatic);
        WallCollision->SetCollisionResponseToAllChannels(ECR_Block);
        WallCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        WallCollision->SetGenerateOverlapEvents(true);
        FVector WallExtent = WallCollision->GetUnscaledBoxExtent();
        if (WallExtent.X < 20.0f)
        {
            WallExtent.X = 20.0f;
            WallCollision->SetBoxExtent(WallExtent);
        }

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] CollisionConfig | Wall=%s | HasAuthority=%s | CollisionEnabled=%d | ObjectType=%d | GenerateOverlap=%s | RespWorldDynamic=%d | RespWorldStatic=%d | RespPawn=%d | Extent=%s | Location=%s"),
            *GetNameSafe(this),
            HasAuthority() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(WallCollision->GetCollisionEnabled()),
            static_cast<int32>(WallCollision->GetCollisionObjectType()),
            WallCollision->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_WorldDynamic)),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_WorldStatic)),
            static_cast<int32>(WallCollision->GetCollisionResponseToChannel(ECC_Pawn)),
            *WallCollision->GetScaledBoxExtent().ToString(),
            *WallCollision->GetComponentLocation().ToString());
    }
    if (HasAuthority())
    {
        WallCollision->OnComponentHit.AddDynamic(
            this, &AWTBRAegornWallActor::OnWallHit);
        WallCollision->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRAegornWallActor::OnWallOverlap);
    }
}

void AWTBRAegornWallActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bIsErupting)
    {
        TickEscudoEruption(DeltaTime);
    }
}

void AWTBRAegornWallActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRAegornWallActor, WallHP);
    DOREPLIFETIME(AWTBRAegornWallActor, MaxWallHP);
}

void AWTBRAegornWallActor::InitializeWall(float InMaxHP, float InDuration)
{
    ensure(HasAuthority());
    MaxWallHP = InMaxHP;
    WallHP    = InMaxHP;
    if (InDuration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRAegornWallActor::OnLifetimeExpired,
            InDuration,
            false);
    }
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] InitializeWall | Wall=%s | HasAuthority=%s | WallHP=%.1f | MaxWallHP=%.1f | Duration=%.1f | Replicates=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        WallHP,
        MaxWallHP,
        InDuration,
        GetIsReplicated() ? TEXT("true") : TEXT("false"));
}

void AWTBRAegornWallActor::BeginEscudoEruption(const FVector& InFinalLocation, float InBuildTime,
    float InAllyPushImpulse, float InEnemyLaunchImpulse)
{
    ensure(HasAuthority());
    if (!HasAuthority()) return;

    EruptStartLocation   = GetActorLocation();
    EruptFinalLocation   = InFinalLocation;
    EruptDuration        = FMath::Max(InBuildTime, 0.01f);
    EruptElapsed         = 0.0f;
    EruptAllyPushImpulse = InAllyPushImpulse;
    EruptEnemyLaunchImpulse = InEnemyLaunchImpulse;
    EruptDisplacedCharacters.Empty();
    EruptDisplacedProps.Empty();
    bIsErupting = true;
    SetActorTickEnabled(true);
}

void AWTBRAegornWallActor::TickEscudoEruption(float DeltaTime)
{
    if (!HasAuthority())
    {
        bIsErupting = false;
        SetActorTickEnabled(false);
        return;
    }

    EruptElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(EruptElapsed / EruptDuration, 0.0f, 1.0f);

    const FVector OldLocation = GetActorLocation();
    const FVector NewLocation = FMath::Lerp(EruptStartLocation, EruptFinalLocation, Alpha);

    ApplyEruptionDisplacement(OldLocation, NewLocation);
    SetActorLocation(NewLocation);

    if (Alpha >= 1.0f)
    {
        bIsErupting = false;
        SetActorTickEnabled(false);
        EruptDisplacedCharacters.Empty();
        EruptDisplacedProps.Empty();
    }
}

// Canon (Hyuse): the erupting wall shoves whoever/whatever stands in its
// footprint as it rises — teammates get pushed to safety behind the wall
// (caster side), enemies and physics props get launched skyward. Damage
// always 0 for characters (Escudo Slam lock). Runs every eruption tick with
// an expanded/swept overlap box (covers the Z travelled this frame) so a
// server hitch can't let something slip through un-displaced.
void AWTBRAegornWallActor::ApplyEruptionDisplacement(const FVector& SweepFrom, const FVector& SweepTo)
{
    if (!IsValid(WallCollision) || !GetWorld()) return;

    const FVector BaseExtent = WallCollision->GetScaledBoxExtent();
    const float DeltaZ = FMath::Abs(SweepTo.Z - SweepFrom.Z);
    FVector SweepExtent = BaseExtent;
    SweepExtent.Z += DeltaZ * 0.5f;
    const FVector SweepCenter(SweepTo.X, SweepTo.Y, (SweepFrom.Z + SweepTo.Z) * 0.5f);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    AWTBRCharacter* CasterCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (IsValid(CasterCharacter))
    {
        QueryParams.AddIgnoredActor(CasterCharacter);
    }

    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(
        Overlaps, SweepCenter, GetActorQuat(), ObjectParams,
        FCollisionShape::MakeBox(SweepExtent), QueryParams);

    if (Overlaps.Num() == 0) return;

    FVector TowardOwner = -GetActorForwardVector();
    if (IsValid(CasterCharacter))
    {
        TowardOwner = CasterCharacter->GetActorLocation() - GetActorLocation();
        TowardOwner.Z = 0.0f;
        TowardOwner = TowardOwner.IsNearlyZero()
            ? -CasterCharacter->GetActorForwardVector()
            : TowardOwner.GetSafeNormal();
    }

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* OtherActor = Overlap.GetActor();
        if (!IsValid(OtherActor) || OtherActor == this) continue;

        if (AWTBRCharacter* Char = Cast<AWTBRCharacter>(OtherActor))
        {
            if (Char == CasterCharacter) continue;
            if (EruptDisplacedCharacters.Contains(Char)) continue;
            EruptDisplacedCharacters.Add(Char);

            const bool bAlly = IsValid(CasterCharacter) && Char->IsSameTeamAs(CasterCharacter);
            if (bAlly)
            {
                Char->LaunchCharacter(
                    TowardOwner * EruptAllyPushImpulse + FVector(0.0f, 0.0f, EruptAllyPushImpulse * 0.25f),
                    true, true);
            }
            else
            {
                // Pure-vertical launch used to land the enemy right back on the
                // same X/Y the instant gravity pulled them down — by then the
                // wall had finished erupting there, so they landed embedded in
                // it. Add an outward (away-from-caster) horizontal component so
                // they clear the footprint before coming back down.
                Char->LaunchCharacter(
                    (-TowardOwner) * (EruptEnemyLaunchImpulse * 0.3f) + FVector(0.0f, 0.0f, EruptEnemyLaunchImpulse),
                    true, true);
            }

            WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] EruptDisplacement | Wall=%s | Target=%s | Relation=%s | Impulse=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(Char),
                bAlly ? TEXT("Ally->PushBehind") : TEXT("Enemy->LaunchUp"),
                bAlly ? EruptAllyPushImpulse : EruptEnemyLaunchImpulse);
            continue;
        }

        UPrimitiveComponent* HitComp = Overlap.GetComponent();
        if (IsValid(HitComp) && HitComp->IsSimulatingPhysics())
        {
            if (EruptDisplacedProps.Contains(HitComp)) continue;
            EruptDisplacedProps.Add(HitComp);
            // Same outward-plus-vertical fix as the enemy-character branch
            // above: a pure vertical impulse lets the prop fall straight back
            // down into the now-solid wall footprint (WallCollision is
            // QueryOnly, so nothing in the physics engine would ever push it
            // back out once embedded).
            HitComp->AddImpulse(
                (-TowardOwner) * (EruptEnemyLaunchImpulse * 0.3f) + FVector(0.0f, 0.0f, EruptEnemyLaunchImpulse),
                NAME_None, /*bVelChange=*/true);

            WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] EruptDisplacement | Wall=%s | Prop=%s | Impulse=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(HitComp),
                EruptEnemyLaunchImpulse);
        }
    }
}

void AWTBRAegornWallActor::OnLifetimeExpired()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] LifetimeExpired | Wall=%s"),
        *GetNameSafe(this));
    DestroyWall(/*bExpiredNaturally=*/true);
}

void AWTBRAegornWallActor::TakeDamageFromProjectile(float DamageAmount)
{
    if (!HasAuthority()) return;
    if (WallHP <= 0.0f) return;

    const float OldHP = WallHP;
    const float EffectiveDamage = DamageAmount * BrittleDamageMultiplier;
    WallHP = FMath::Max(0.0f, WallHP - EffectiveDamage);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] WallDamage | OldHP=%.1f | Damage=%.1f | NewHP=%.1f"),
        OldHP,
        EffectiveDamage,
        WallHP);

    OnWallDamaged(WallHP, EffectiveDamage);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnWallDamaged Fired | Wall=%s"),
        *GetNameSafe(this));

    if (WallHP <= 0.0f)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] WallDestroyed | Wall=%s"),
            *GetNameSafe(this));
        DestroyWall(/*bExpiredNaturally=*/false);
    }
}

void AWTBRAegornWallActor::ApplyBrittle(float DamageMultiplier, float Duration)
{
    if (!HasAuthority() || DamageMultiplier <= 1.0f || Duration <= 0.0f) return;

    BrittleDamageMultiplier = FMath::Max(BrittleDamageMultiplier, DamageMultiplier);
    GetWorldTimerManager().SetTimer(BrittleExpiryTimer, this,
        &AWTBRAegornWallActor::ClearBrittle, Duration, false);
}

void AWTBRAegornWallActor::ClearBrittle()
{
    BrittleDamageMultiplier = 1.0f;
}

float AWTBRAegornWallActor::GetWallHPPercent() const
{
    if (MaxWallHP <= 0.0f) return 0.0f;
    return WallHP / MaxWallHP;
}

void AWTBRAegornWallActor::OnWallHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (OtherActor && OtherActor->IsA(AWTBRCharacter::StaticClass()))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] Hit | Wall=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Source=Hit"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    if (HandleProjectileContact(OtherActor, TEXT("Hit")))
    {
        return;
    }
    return;

    if (!HasAuthority()) return;
    if (!IsValid(OtherActor)) return;
    if (!OtherActor->ActorHasTag(TEXT("WTBRProjectile"))) return;

    // TODO Phase 4: ดึง Damage จาก Projectile DataAsset
    const float ProjectileDamage = 50.0f;
    TakeDamageFromProjectile(ProjectileDamage);
    OtherActor->Destroy();
}

void AWTBRAegornWallActor::OnWallOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA(AWTBRCharacter::StaticClass()))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] Hit | Wall=%s | Other=%s | OtherClass=%s | HasAuthority=%s | Source=Overlap"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        IsValid(OtherActor) ? *GetNameSafe(OtherActor->GetClass()) : TEXT("None"),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    HandleProjectileContact(OtherActor, TEXT("Overlap"));
}

bool AWTBRAegornWallActor::HandleProjectileContact(AActor* OtherActor, const TCHAR* Source)
{
    if (!HasAuthority()) return false;
    if (!IsValid(OtherActor)) return false;
    if (DamagedProjectiles.Contains(OtherActor)) return false;

    if (AWTBRProjectileBase* Projectile = Cast<AWTBRProjectileBase>(OtherActor))
    {
        const float ProjectileDamage = Projectile->BaseDamage;
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileDetected | Method=Cast | Projectile=%s | Damage=%.1f | Source=%s"),
            *GetNameSafe(Projectile),
            ProjectileDamage,
            Source);

        DamagedProjectiles.Add(OtherActor);
        TakeDamageFromProjectile(ProjectileDamage);
        OtherActor->Destroy();
        return true;
    }

    if (OtherActor->ActorHasTag(TEXT("WTBRProjectile")))
    {
        const float ProjectileDamage = 50.0f;
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileDetected | Method=Tag | Projectile=%s | Damage=%.1f | Source=%s"),
            *GetNameSafe(OtherActor),
            ProjectileDamage,
            Source);

        DamagedProjectiles.Add(OtherActor);
        TakeDamageFromProjectile(ProjectileDamage);
        OtherActor->Destroy();
        return true;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] ProjectileRejected | Other=%s | Source=%s | HasWTBRProjectileTag=%s"),
        *GetNameSafe(OtherActor),
        Source,
        OtherActor->ActorHasTag(TEXT("WTBRProjectile")) ? TEXT("true") : TEXT("false"));
    return false;
}

void AWTBRAegornWallActor::DestroyWall(bool bExpiredNaturally)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] DestroyWall | Wall=%s | HasAuthority=%s | ExpiredNaturally=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bExpiredNaturally ? TEXT("true") : TEXT("false"));
    OnWallDestroyed.Broadcast(bExpiredNaturally);
    OnWallCollapsed();
    Destroy();
}

void AWTBRAegornWallActor::OnRep_WallHP()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnRep_WallHP | Wall=%s | WallHP=%.1f | MaxWallHP=%.1f"),
        *GetNameSafe(this),
        WallHP,
        MaxWallHP);
    OnWallDamaged(WallHP, 0.0f);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] OnWallDamaged Fired | Wall=%s | Source=OnRep_WallHP"),
        *GetNameSafe(this));
}
