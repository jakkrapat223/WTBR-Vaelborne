// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Actors/WTBRVoltisTrapActor.h"
#include "WTBRValidationLog.h"
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

namespace
{
    // BP_WTBRVoltisTrigger was created before the prototype effects existed and
    // serializes its own inherited values.  Resolve the production plates here
    // so an old Blueprint override cannot silently restore the placeholder VFX.
    UNiagaraSystem* GetVoltisPlacedPlateEffect()
    {
        return LoadObject<UNiagaraSystem>(nullptr,
            TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_TrapPrism_P02.NS_Voltis_TrapPrism_P02"));
    }

    UNiagaraSystem* GetVoltisTriggeredPlateEffect()
    {
        return LoadObject<UNiagaraSystem>(nullptr,
            TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_TrapTriggerPrism_P02.NS_Voltis_TrapTriggerPrism_P02"));
    }
}

AWTBRVoltisTrapActor::AWTBRVoltisTrapActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    TrapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrapMesh"));
    RootComponent = TrapMesh;
    TrapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Placeholder art (⚠ not final) so the trap is visible on the ground the
    // moment it's placed, without needing any Editor/Blueprint mesh assignment —
    // same ConstructorHelpers idiom already used by WTBRCombatBlockoutGenerator.
    // A small rectangular footprint is used only as a fallback silhouette; the
    // Niagara plate below supplies the actual Voltis visual.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (PlaceholderMesh.Succeeded())
    {
        TrapMesh->SetStaticMesh(PlaceholderMesh.Object);
        TrapMesh->SetRelativeScale3D(FVector(0.45f, 0.30f, 0.03f));
        TrapMesh->SetVisibility(false);
    }

    // Real production VFX (⚠ PLAYTEST PENDING tuning), generated via the
    // NiagaraJsonGenerator pipeline from /Game/VFX/Templates/NS_Template_Burst —
    // set as native defaults so no Editor/Blueprint step is required for the
    // trap to be visible; a BP subclass can still override either field.
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultPlacedFX(
        TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_TrapPrism_P02.NS_Voltis_TrapPrism_P02"));
    if (DefaultPlacedFX.Succeeded())
    {
        PlacedEffect = DefaultPlacedFX.Object;
    }
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultTriggeredFX(
        TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_TrapTriggerPrism_P02.NS_Voltis_TrapTriggerPrism_P02"));
    if (DefaultTriggeredFX.Succeeded())
    {
        TriggeredEffect = DefaultTriggeredFX.Object;
    }

    TrapOverlap = CreateDefaultSubobject<USphereComponent>(TEXT("TrapOverlap"));
    TrapOverlap->SetupAttachment(RootComponent);
    TrapOverlap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TrapOverlap->SetCollisionResponseToAllChannels(ECR_Ignore);
    TrapOverlap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TrapOverlap->SetGenerateOverlapEvents(true);
}

void AWTBRVoltisTrapActor::BeginPlay()
{
    Super::BeginPlay();

    // Apply at runtime too: a BP subclass can retain component edits made before
    // the rectangular plate replaced the old disc placeholder.
    if (IsValid(TrapMesh))
    {
        TrapMesh->SetRelativeScale3D(FVector(0.45f, 0.30f, 0.03f));
        TrapMesh->SetVisibility(false);
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] Trap BeginPlay | Trap=%s | HasAuthority=%s | Replicates=%s | Location=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        GetIsReplicated() ? TEXT("true") : TEXT("false"),
        *GetActorLocation().ToString());
    if (HasAuthority())
    {
        TrapOverlap->OnComponentBeginOverlap.AddDynamic(
            this, &AWTBRVoltisTrapActor::OnTrapOverlapBegin);
    }

    // Purely cosmetic — runs on every machine the actor exists on (server and
    // every client it replicates to), not just the authority. BeginPlay (not
    // InitializeTrap, which is server-only) is the correct place for this so
    // clients actually see the trap being placed, matching the same reasoning
    // as AWTBRProjectileBase::TrailEffect's spawn-in-BeginPlay comment.
    // Keep the marker alive for the full armed lifetime. A trap needs to be
    // readable on the floor until it triggers, rather than flashing once.
    StartPlacedPlate();
}

void AWTBRVoltisTrapActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRVoltisTrapActor, bIsTriggered);
}

void AWTBRVoltisTrapActor::InitializeTrap(
    float InLifetime,
    float InExplosionRadius,
    float InDamage,
    float InVerticalLaunchForce,
    UWTBRVoltisLaunchTrigger* InOwnerTrigger)
{
    ensure(HasAuthority());
    ExplosionRadius    = InExplosionRadius;
    TrapDamage          = InDamage;
    VerticalLaunchForce = InVerticalLaunchForce;
    OwnerTrigger        = InOwnerTrigger;
    TrapOverlap->SetSphereRadius(FMath::Max(1.0f, ExplosionRadius));

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] Trap Initialized | Trap=%s | Lifetime=%.1f | Radius=%.1f | Damage=%.1f | LaunchForce=%.1f | OwnerTrigger=%s"),
        *GetNameSafe(this),
        InLifetime,
        ExplosionRadius,
        TrapDamage,
        VerticalLaunchForce,
        *GetNameSafe(InOwnerTrigger));

    if (InLifetime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            LifetimeTimer,
            this, &AWTBRVoltisTrapActor::OnLifetimeExpired,
            InLifetime,
            false);
    }
}

void AWTBRVoltisTrapActor::OnTrapOverlapBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] TrapOverlap | Trap=%s | Other=%s | HasAuthority=%s | Triggered=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OtherActor),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsTriggered ? TEXT("true") : TEXT("false"));

    if (!HasAuthority() || bIsTriggered || !IsValid(OtherActor))
    {
        return;
    }

    // Self-check: instigator (the caster who placed this trap) never triggers it.
    if (OtherActor == GetInstigator())
    {
        return;
    }

    AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(OtherActor);
    if (!IsValid(HitChar))
    {
        return;
    }

    // Team-Check (locked design): teammates walking over their own ally's trap
    // must not self-trigger it — matches the existing Nexil tripwire convention.
    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetInstigator());
    if (IsValid(OwnerChar) && OwnerChar->IsSameTeamAs(HitChar))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] TrapRejected | Trap=%s | Other=%s | Reason=SameTeam"),
            *GetNameSafe(this),
            *GetNameSafe(OtherActor));
        return;
    }

    TriggerAndDestroy(OtherActor);
}

void AWTBRVoltisTrapActor::LaunchCharacterUp(AActor* TargetActor)
{
    AWTBRCharacter* TargetChar = Cast<AWTBRCharacter>(TargetActor);
    if (!IsValid(TargetChar))
    {
        return;
    }

    // Vertical only — unlike the Hold-teammate direct-apply branch, an untriggered
    // enemy's movement input can't be read ahead of time (locked design).
    TargetChar->LaunchCharacter(FVector(0.0f, 0.0f, VerticalLaunchForce), false, true);

    if (TrapDamage > 0.0f && IsValid(TargetChar->HealthComponent))
    {
        TargetChar->HealthComponent->ApplyDamage(TrapDamage, GetInstigator());
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] TrapLaunchApplied | Target=%s | VerticalForce=%.1f | Damage=%.1f"),
        *GetNameSafe(TargetChar),
        VerticalLaunchForce,
        TrapDamage);
}

void AWTBRVoltisTrapActor::TriggerAndDestroy(AActor* TriggeredBy)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] Trap Triggered | Trap=%s | TriggeredBy=%s"),
        *GetNameSafe(this),
        *GetNameSafe(TriggeredBy));
    bIsTriggered = true;
    StopPlacedPlate();
    Multicast_PlayTriggeredVFX();
    LaunchCharacterUp(TriggeredBy);
    if (OwnerTrigger.IsValid())
    {
        OwnerTrigger->NotifyTrapConsumed(this);
    }
    GetWorld()->GetTimerManager().ClearTimer(LifetimeTimer);
    Destroy();
}

void AWTBRVoltisTrapActor::OnLifetimeExpired()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Voltis Test] Trap LifetimeExpired | Trap=%s"),
        *GetNameSafe(this));
    StopPlacedPlate();
    if (OwnerTrigger.IsValid())
    {
        OwnerTrigger->NotifyTrapConsumed(this);
    }
    Destroy();
}

void AWTBRVoltisTrapActor::OnRep_bIsTriggered()
{
    if (bIsTriggered && !bTriggeredVFXAlreadyPlayed)
    {
        bTriggeredVFXAlreadyPlayed = true;
        StopPlacedPlate();
        OnTrapTriggeredVFX();
        // Fires on simulated proxies (clients) only — same known limitation as
        // AWTBRNexilWireActor's identical OnRep-only VFX hookup (a listen host
        // playing as a local player won't see its own trigger burst, since OnRep
        // never fires for the authoritative instance). Not fixed here to stay
        // consistent with the existing pattern; flag if that's ever revisited.
        if (UNiagaraSystem* PlateEffect = GetVoltisTriggeredPlateEffect())
        {
            PlayOneShotBurst(PlateEffect, 0.6f, FVector(1.05f, 0.62f, 0.12f));
        }
    }
}

void AWTBRVoltisTrapActor::Multicast_PlayTriggeredVFX_Implementation()
{
    if (bTriggeredVFXAlreadyPlayed)
    {
        return;
    }

    bTriggeredVFXAlreadyPlayed = true;
    StopPlacedPlate();
    OnTrapTriggeredVFX();
    if (UNiagaraSystem* PlateEffect = GetVoltisTriggeredPlateEffect())
    {
        PlayOneShotBurst(PlateEffect, 0.6f, FVector(1.05f, 0.62f, 0.12f));
    }
}

void AWTBRVoltisTrapActor::PlayOneShotBurst(
    UNiagaraSystem* Effect,
    float Lifetime,
    const FVector& VisualScale)
{
    if (!IsValid(Effect) || !GetWorld())
    {
        return;
    }

    UWorld* World = GetWorld();
    UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World, Effect, GetActorLocation(), GetActorRotation());
    if (!IsValid(Comp) || Lifetime <= 0.0f)
    {
        return;
    }

    Comp->SetWorldScale3D(VisualScale);

    // Do not bind this timer to the trap Actor: TriggerAndDestroy removes that
    // actor immediately, which also removes actor-bound timers before they can
    // stop the looping prototype effect. A weak component lambda remains valid
    // long enough to clean up the burst after the trap is gone.
    Comp->SetAutoDestroy(true);
    const TWeakObjectPtr<UNiagaraComponent> WeakComponent = Comp;
    FTimerHandle DeactivateTimer;
    World->GetTimerManager().SetTimer(
        DeactivateTimer,
        FTimerDelegate::CreateLambda([WeakComponent]()
        {
            if (WeakComponent.IsValid())
            {
                WeakComponent->Deactivate();
            }
        }),
        Lifetime,
        false);
}

void AWTBRVoltisTrapActor::StartPlacedPlate()
{
    if (IsValid(PlacedPlateComponent) || !IsValid(RootComponent))
    {
        return;
    }

    UNiagaraSystem* PlateEffect = GetVoltisPlacedPlateEffect();
    if (!IsValid(PlateEffect))
    {
        return;
    }

    PlacedPlateComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        PlateEffect,
        RootComponent,
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        false,
        true);
    if (IsValid(PlacedPlateComponent))
    {
        // Roughly 38 x 22 cm and only a few centimetres thick: a single-foot
        // rectangular plate, attached at the exact floor impact point.
        PlacedPlateComponent->SetRelativeScale3D(FVector(0.90f, 0.52f, 0.08f));
    }
}

void AWTBRVoltisTrapActor::StopPlacedPlate()
{
    if (IsValid(PlacedPlateComponent))
    {
        PlacedPlateComponent->DeactivateImmediate();
        PlacedPlateComponent = nullptr;
    }
}
