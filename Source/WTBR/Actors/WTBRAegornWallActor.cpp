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

FVector AWTBRAegornWallActor::ComputeIntendedCollisionExtent() const
{
    if (IsValid(WallMesh) && IsValid(WallMesh->GetStaticMesh()))
    {
        const FVector MeshExtent = WallMesh->GetStaticMesh()->GetBoundingBox().GetExtent();
        // GetRelativeScale3D, not GetComponentScale() — GetComponentScale reads
        // ComponentToWorld, which is only ever computed when a component
        // registers. A Class Default Object never registers its components, so
        // that call silently returned identity (1,1,1) and threw away whatever
        // scale the Blueprint authored on WallMesh. The bug it caused: a wall
        // whose BP scale is (0.2, 3.0, 2.0) computed as a 128-cube instead of a
        // thin 25.6-deep panel, so the anti-trap box reached back far enough to
        // always swallow the placing character — every placement, tap or preset,
        // read as invalid and the ghost preview was permanently red.
        // RelativeScale3D is plain serialized property data and is correct on a CDO.
        const FVector MeshScale = WallMesh->GetRelativeScale3D();
        return MeshExtent * MeshScale;
    }
    // No mesh assigned (shouldn't happen on a real BP) — fall back to
    // whatever WallCollision already has rather than guessing.
    return IsValid(WallCollision) ? WallCollision->GetUnscaledBoxExtent() : FVector(10.0f, 150.0f, 150.0f);
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

        // Auto-fit the collision box to whatever static mesh + scale
        // BP_WTBRAegornWallActor actually uses for WallMesh, instead of a
        // hardcoded C++ size that can silently drift out of sync with what
        // the player visually sees. Every gameplay use of this box (Escudo's
        // ValidatePanelPlacement anti-trap overlap check, SpawnOnePanel's
        // erupted spawn height, the ghost-preview footprint) now automatically
        // tracks a resize made purely by adjusting WallMesh's scale in the
        // Blueprint — that single change is enough, nothing else needs
        // touching to keep the wall's look and its hitbox matching.
        WallCollision->SetBoxExtent(ComputeIntendedCollisionExtent());
        WTBR_VALIDATION_LOG(Log, TEXT("[Escudo Test] CollisionAutoFit | Wall=%s | Mesh=%s | MeshScale=%s | ComputedExtent=%s"),
            *GetNameSafe(this),
            *GetNameSafe(IsValid(WallMesh) ? WallMesh->GetStaticMesh() : nullptr),
            IsValid(WallMesh) ? *WallMesh->GetComponentScale().ToString() : TEXT("n/a"),
            *WallCollision->GetUnscaledBoxExtent().ToString());

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
    float InEruptionImpulse, float InClearanceRatio)
{
    ensure(HasAuthority());
    if (!HasAuthority()) return;

    EruptStartLocation   = GetActorLocation();
    EruptFinalLocation   = InFinalLocation;
    EruptDuration        = FMath::Max(InBuildTime, 0.01f);
    EruptElapsed         = 0.0f;
    EruptImpulse         = InEruptionImpulse;
    EruptClearanceRatio  = InClearanceRatio;
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

    // Grown along the box's LOCAL Z (its grow axis) by however far the wall
    // actually travelled this frame, and centred on the midpoint of that
    // travel. Written against world Z originally, which only held while every
    // Escudo erupted straight up; a wall- or ceiling-anchored panel now rises
    // along its surface normal instead, and the old math would have swept a
    // box that missed the volume the wall really passed through.
    const FVector BaseExtent = WallCollision->GetScaledBoxExtent();
    const FVector Travel = SweepTo - SweepFrom;
    FVector SweepExtent = BaseExtent;
    SweepExtent.Z += Travel.Size() * 0.5f;
    const FVector SweepCenter = (SweepFrom + SweepTo) * 0.5f;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    // The caster is deliberately NOT ignored (owner's call, 2026-07-20).
    // Everyone the rising wall passes through gets hit — caster, ally, enemy.
    // Standing on your own panel to ride it up is a movement tech, and it is
    // also what makes it safe for UWTBREscudoTrigger::ValidatePanelPlacement to
    // have dropped its trap check: nothing can be trapped by a wall that
    // displaces every character it touches.
    AWTBRCharacter* CasterCharacter = Cast<AWTBRCharacter>(GetOwner());

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

    // Everything is thrown along the direction the wall is GROWING — its local
    // +Z, which UWTBREscudoTrigger::ComputeSurfaceAlignedRotation aligned to
    // the surface normal. Out of a floor that is straight up, exactly as
    // before; out of a wall it is horizontal, which reads as the car-hit shove
    // the owner asked for; out of a ceiling it drives the target into the
    // ground.
    const FVector GrowAxis = GetActorQuat().GetAxisZ();

    // A near-vertical eruption throws the target straight up and gravity drops
    // them back onto the footprint the wall has meanwhile finished filling —
    // they land embedded in it. An outward nudge clears them off it first. A
    // horizontal eruption already carries them clear and gravity does not undo
    // it, so this fades out as the grow axis tips over.
    //
    // The ratio is DataAsset-driven (EscudoEruptionClearanceRatio) rather than
    // a constant here: it is a pure feel value still pending playtest, and the
    // project's rule is that those stay tunable without a recompile.
    const float Verticality = FMath::Abs(GrowAxis.Z);
    auto BuildImpulse = [&](float Magnitude)
    {
        return GrowAxis * Magnitude
            + (-TowardOwner) * (Magnitude * EruptClearanceRatio * Verticality);
    };

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* OtherActor = Overlap.GetActor();
        if (!IsValid(OtherActor) || OtherActor == this) continue;

        if (AWTBRCharacter* Char = Cast<AWTBRCharacter>(OtherActor))
        {
            if (EruptDisplacedCharacters.Contains(Char)) continue;
            EruptDisplacedCharacters.Add(Char);

            // No team branch at all: caster, ally and enemy take the identical
            // hit (owner's call, 2026-07-20). Allies used to be shunted
            // sideways behind the caster at a weaker impulse and the caster was
            // skipped outright; both special cases are gone. Damage stays 0 for
            // everyone regardless — Escudo Slam is displacement, never damage.
            Char->LaunchCharacter(BuildImpulse(EruptImpulse), true, true);

            WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] EruptDisplacement | Wall=%s | Target=%s | Relation=%s | Impulse=%.1f | GrowAxis=%s"),
                *GetNameSafe(this),
                *GetNameSafe(Char),
                (Char == CasterCharacter) ? TEXT("Caster") : TEXT("Other"),
                EruptImpulse,
                *GrowAxis.ToString());
            continue;
        }

        UPrimitiveComponent* HitComp = Overlap.GetComponent();
        if (IsValid(HitComp) && HitComp->IsSimulatingPhysics())
        {
            if (EruptDisplacedProps.Contains(HitComp)) continue;
            EruptDisplacedProps.Add(HitComp);
            // Same grow-axis impulse the characters get. The outward component
            // matters just as much here: WallCollision is QueryOnly, so once a
            // prop falls back into the finished footprint nothing in the
            // physics engine would ever push it out again.
            HitComp->AddImpulse(BuildImpulse(EruptImpulse), NAME_None, /*bVelChange=*/true);

            WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] EruptDisplacement | Wall=%s | Prop=%s | Impulse=%.1f"),
                *GetNameSafe(this),
                *GetNameSafe(HitComp),
                EruptImpulse);
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
