// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRInteractionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Components/WTBRHealthComponent.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "Actors/WTBRNexilWireActor.h"
#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"

namespace
{
    // Minimum dot(view-forward, dir-to-candidate) for a dropped trigger to be treated
    // as focused. AWTBRDroppedTriggerActor has no collision primitive, so focus uses
    // actor iteration + this view-cone gate instead of a line trace. ~0.5 ≈ 60-degree
    // half-cone, forgiving enough for low ground loot.
    constexpr float WTBRInteractionDroppedTriggerFocusAimDotThreshold = 0.5f;
}

UWTBRInteractionComponent::UWTBRInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

AWTBRCorpseLootContainerActor* UWTBRInteractionComponent::GetFocusedCorpseLootContainer() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (FocusedCorpseLootContainerOverrideForTest.IsValid())
    {
        return FocusedCorpseLootContainerOverrideForTest.Get();
    }
#endif

    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + (ViewRotation.Vector() * InteractionTraceDistance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WTBRInteractionFocusedCorpseLootContainer), false, OwnerCharacter);
    QueryParams.AddIgnoredActor(OwnerCharacter);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    // Verbose + CVar-gated: this trace runs on every HUD snapshot refresh, which can
    // fire per damage tick in combat — must never log at default settings.
    if (!bHit)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[WTBR Interact] Corpse loot focus trace hit nothing (start=%s end=%s range=%.0f)."),
            *TraceStart.ToString(),
            *TraceEnd.ToString(),
            InteractionTraceDistance);
        return nullptr;
    }

    AWTBRCorpseLootContainerActor* Container = Cast<AWTBRCorpseLootContainerActor>(Hit.GetActor());
    if (!Container)
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[WTBR Interact] Corpse loot focus trace hit %s (component=%s profile=%s) — not a corpse loot container, rejected. Distance=%.0f/%.0f."),
            *GetNameSafe(Hit.GetActor()),
            *GetNameSafe(Hit.GetComponent()),
            Hit.GetComponent() ? *Hit.GetComponent()->GetCollisionProfileName().ToString() : TEXT("None"),
            (Hit.Location - TraceStart).Size(),
            InteractionTraceDistance);
        return nullptr;
    }

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[WTBR Interact] Corpse loot focus trace hit container %s (component=%s) at %.0f/%.0f units."),
        *GetNameSafe(Container),
        *GetNameSafe(Hit.GetComponent()),
        (Hit.Location - TraceStart).Size(),
        InteractionTraceDistance);
    return Container;
}

FText UWTBRInteractionComponent::GetFocusedInteractionPromptText() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return FText::GetEmpty();
    }

    const AWTBRCorpseLootContainerActor* FocusedContainer = GetFocusedCorpseLootContainer();
    if (!IsValid(FocusedContainer) || !FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        return FText::GetEmpty();
    }

    return FocusedContainer->GetInteractionPromptText();
}

bool UWTBRInteractionComponent::HasValidFocusedInteractable() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    const AWTBRCorpseLootContainerActor* FocusedContainer = GetFocusedCorpseLootContainer();
    if (IsValid(FocusedContainer) && FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        return true;
    }

    if (IsValid(GetFocusedDroppedTrigger()))
    {
        return true;
    }

    if (IsValid(GetFocusedGroundItem()))
    {
        return true;
    }

    if (IsValid(GetFocusedNexilWire()))
    {
        return true;
    }

    return false;
}

bool UWTBRInteractionComponent::RequestCorpseLootInteract()
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* FocusedContainer = GetFocusedCorpseLootContainer();
    if (!IsValid(FocusedContainer))
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestCorpseLootInteract: no focused container (see focus trace log above)."));
        return false;
    }

    if (!FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestCorpseLootInteract: container %s focused but CanBeInteractedWithBy rejected it (likely no available loot entries left)."),
            *GetNameSafe(FocusedContainer));
        return false;
    }

    WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestCorpseLootInteract: broadcasting OnCorpseLootInteractRequested for container %s."),
        *GetNameSafe(FocusedContainer));
    OnCorpseLootInteractRequested.Broadcast(FocusedContainer);
    return true;
}

AWTBRDroppedTriggerActor* UWTBRInteractionComponent::GetFocusedDroppedTrigger() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ViewForward = ViewRotation.Vector();

    // AWTBRDroppedTriggerActor has no collision primitive (bare USceneComponent root),
    // so a line trace can never hit it and would instead stop on the floor. Iterate the
    // dropped-trigger actors and pick the nearest not-yet-consumed candidate within
    // InteractionTraceDistance that is in front of the view. Focus only — no mutation.
    AWTBRDroppedTriggerActor* BestDroppedTrigger = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    const float MaxRangeSq = FMath::Square(InteractionTraceDistance);

    for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
    {
        AWTBRDroppedTriggerActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsConsumed() || Candidate->GetDroppedTriggerDataAsset().IsNull())
        {
            continue;
        }

        const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
        const float CandidateDistanceSq = ToCandidate.SizeSquared();
        if (CandidateDistanceSq > MaxRangeSq)
        {
            continue;
        }

        if (FVector::DotProduct(ToCandidate.GetSafeNormal(), ViewForward) < WTBRInteractionDroppedTriggerFocusAimDotThreshold)
        {
            continue;
        }

        if (CandidateDistanceSq < BestDistanceSq)
        {
            BestDroppedTrigger = Candidate;
            BestDistanceSq = CandidateDistanceSq;
        }
    }

    return BestDroppedTrigger;
}

AWTBRGroundItemActor* UWTBRInteractionComponent::GetFocusedGroundItem() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (FocusedGroundItemOverrideForTest.IsValid())
    {
        return FocusedGroundItemOverrideForTest.Get();
    }
#endif

    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + (ViewRotation.Vector() * InteractionTraceDistance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WTBRInteractionFocusedGroundItem), false, OwnerCharacter);
    QueryParams.AddIgnoredActor(OwnerCharacter);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    if (!bHit)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[WTBR Interact] Ground focus trace hit nothing (range=%.0f)."),
            InteractionTraceDistance);
        return nullptr;
    }

    AWTBRGroundItemActor* GroundItem = Cast<AWTBRGroundItemActor>(Hit.GetActor());
    WTBR_VALIDATION_LOG(Verbose, TEXT("[WTBR Interact] Ground focus trace hit %s (isGroundItem=%s) at %.0f/%.0f units."),
        *GetNameSafe(Hit.GetActor()),
        GroundItem ? TEXT("true") : TEXT("false"),
        (Hit.Location - TraceStart).Size(),
        InteractionTraceDistance);
    return GroundItem;
}

AWTBRNexilWireActor* UWTBRInteractionComponent::GetFocusedNexilWire() const
{
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ViewForward = ViewRotation.Vector();

    // Same reasoning as GetFocusedDroppedTrigger: a thin rope-like wire is easy
    // to miss with a precise line trace, so use actor-iteration + view-cone
    // instead. Only wires this character can actually grab are candidates.
    AWTBRNexilWireActor* BestWire = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    const float MaxRangeSq = FMath::Square(InteractionTraceDistance);

    for (TActorIterator<AWTBRNexilWireActor> It(World); It; ++It)
    {
        AWTBRNexilWireActor* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->CanBeGrabbedBy(OwnerCharacter))
        {
            continue;
        }

        const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
        const float CandidateDistanceSq = ToCandidate.SizeSquared();
        if (CandidateDistanceSq > MaxRangeSq)
        {
            continue;
        }

        if (FVector::DotProduct(ToCandidate.GetSafeNormal(), ViewForward) < WTBRInteractionDroppedTriggerFocusAimDotThreshold)
        {
            continue;
        }

        if (CandidateDistanceSq < BestDistanceSq)
        {
            BestWire = Candidate;
            BestDistanceSq = CandidateDistanceSq;
        }
    }

    return BestWire;
}

AWTBRCharacter* UWTBRInteractionComponent::GetFocusedRevivableTeammate() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (FocusedRevivableTeammateOverrideForTest.IsValid())
    {
        return FocusedRevivableTeammateOverrideForTest.Get();
    }
#endif

    AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter) || !IsValid(OwnerCharacter->HealthComponent) || !OwnerCharacter->HealthComponent->IsAlive())
    {
        // Only a living character can revive — no point tracing further.
        return nullptr;
    }

    UWorld* World = OwnerCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (AController* Controller = OwnerCharacter->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        OwnerCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + (ViewRotation.Vector() * InteractionTraceDistance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WTBRInteractionFocusedRevivableTeammate), false, OwnerCharacter);
    QueryParams.AddIgnoredActor(OwnerCharacter);

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    if (!bHit)
    {
        return nullptr;
    }

    AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Hit.GetActor());
    if (!IsValid(Candidate) || !IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsDowned())
    {
        return nullptr;
    }

    if (!OwnerCharacter->IsSameTeamAs(Candidate))
    {
        return nullptr;
    }

    return Candidate;
}

bool UWTBRInteractionComponent::RequestReviveInteract()
{
    AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    AWTBRCharacter* FocusedTeammate = GetFocusedRevivableTeammate();
    if (!IsValid(FocusedTeammate))
    {
        return false;
    }

    WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestReviveInteract: dispatching Server_RequestStartRevive for %s."),
        *GetNameSafe(FocusedTeammate));
    OwnerCharacter->Server_RequestStartRevive(FocusedTeammate);
    // Remember the target even though the server may still reject it (e.g.
    // someone else is already reviving) — RequestStopReviveIfInProgress only
    // stops it if THIS actor turns out to be the active reviver server-side, so
    // a rejected start can never cancel a different teammate's legitimate revive.
    CurrentReviveTargetForRelease = FocusedTeammate;
    return true;
}

void UWTBRInteractionComponent::RequestStopReviveIfInProgress()
{
    AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    AWTBRCharacter* Target = CurrentReviveTargetForRelease.Get();
    CurrentReviveTargetForRelease = nullptr;

    if (!IsValid(OwnerCharacter) || !IsValid(Target))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestStopReviveIfInProgress: dispatching Server_RequestStopRevive for %s."),
        *GetNameSafe(Target));
    OwnerCharacter->Server_RequestStopRevive(Target);
}

bool UWTBRInteractionComponent::RequestContextInteract()
{
    WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] RequestContextInteract called (owner=%s)."),
        *GetNameSafe(GetOwner()));

    // Priority 0 — downed teammate (hold F to revive). Checked first: a downed-
    // teammate focus can never legitimately coincide with corpse/dropped-trigger/
    // ground-item focus, so this never steals priority from another valid action.
    if (RequestReviveInteract())
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] Handled by revive-teammate focus (priority 0)."));
        return true;
    }

    // Priority 1 — corpse / container / chest.
    // Reuses the existing client-side loot request bridge (no gameplay mutation).
    if (RequestCorpseLootInteract())
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] Handled by corpse/container/chest focus (priority 1)."));
        return true;
    }

    // Priority 2 — dropped trigger (S7A).
    // Server-authoritative: the client only decides priority via focus, then routes
    // to AWTBRCharacter::RequestPickupAimedDroppedTriggerByConstraint(), which reads
    // the trigger's ETriggerSlotConstraint and dispatches Server_RequestPickupDroppedTrigger
    // to the single valid ACTIVE target slot (MainOnly->active main, SubOnly->active sub).
    // Constraint Any is rejected as AmbiguousTargetSlot inside the character resolver
    // (no slot is guessed). No inventory/slot/actor mutation happens here.
    if (AWTBRDroppedTriggerActor* FocusedDroppedTrigger = GetFocusedDroppedTrigger())
    {
        if (AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner()))
        {
            WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] Dropped trigger %s focused (priority 2); routing to RequestPickupAimedDroppedTriggerByConstraint."),
                *GetNameSafe(FocusedDroppedTrigger));
            OwnerCharacter->RequestPickupAimedDroppedTriggerByConstraint();
            return true;
        }

        UE_LOG(LogTemp, Warning, TEXT("[WTBR Interact] Dropped trigger %s focused but owner is not an AWTBRCharacter; cannot route."),
            *GetNameSafe(FocusedDroppedTrigger));
        return false;
    }

    // Priority 3 — BR ground item (S6).
    // Server-authoritative: the client only requests focus + dispatch; AWTBRCharacter's
    // server RPC validates authority/alive/match/distance and performs the
    // all-or-nothing inventory add. No inventory/ground-item mutation happens here.
    if (AWTBRGroundItemActor* FocusedGroundItem = GetFocusedGroundItem())
    {
        if (AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner()))
        {
            WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] Ground item %s focused (priority 3); dispatching Server_RequestPickupGroundItem."),
                *GetNameSafe(FocusedGroundItem));
            OwnerCharacter->Server_RequestPickupGroundItem(FocusedGroundItem);
            return true;
        }

        UE_LOG(LogTemp, Warning, TEXT("[WTBR Interact] Ground item %s focused but owner is not an AWTBRCharacter; cannot dispatch."),
            *GetNameSafe(FocusedGroundItem));
        return false;
    }

    // Priority 4 — generic interactable (Nexil zipline wire grab).
    // Server-authoritative: client only decides focus, then dispatches
    // Server_GrabNexilWire, which re-validates CanBeGrabbedBy + range itself.
    if (AWTBRNexilWireActor* FocusedWire = GetFocusedNexilWire())
    {
        if (AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner()))
        {
            WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] Nexil wire %s focused (priority 4); dispatching Server_GrabNexilWire."),
                *GetNameSafe(FocusedWire));
            OwnerCharacter->Server_GrabNexilWire(FocusedWire);
            return true;
        }

        UE_LOG(LogTemp, Warning, TEXT("[WTBR Interact] Nexil wire %s focused but owner is not an AWTBRCharacter; cannot dispatch."),
            *GetNameSafe(FocusedWire));
        return false;
    }

    // Priority 5 — no valid focus: no-op.
    WTBR_VALIDATION_LOG(Log, TEXT("[WTBR Interact] No valid focus for context interact (no-op)."));
    return false;
}

#if WITH_DEV_AUTOMATION_TESTS
void UWTBRInteractionComponent::SetFocusedCorpseLootContainerOverrideForTest(AWTBRCorpseLootContainerActor* Container)
{
    FocusedCorpseLootContainerOverrideForTest = Container;
}

void UWTBRInteractionComponent::ClearFocusedCorpseLootContainerOverrideForTest()
{
    FocusedCorpseLootContainerOverrideForTest.Reset();
}

void UWTBRInteractionComponent::SetFocusedGroundItemOverrideForTest(AWTBRGroundItemActor* GroundItem)
{
    FocusedGroundItemOverrideForTest = GroundItem;
}

void UWTBRInteractionComponent::ClearFocusedGroundItemOverrideForTest()
{
    FocusedGroundItemOverrideForTest.Reset();
}

void UWTBRInteractionComponent::SetFocusedRevivableTeammateOverrideForTest(AWTBRCharacter* Teammate)
{
    FocusedRevivableTeammateOverrideForTest = Teammate;
}

void UWTBRInteractionComponent::ClearFocusedRevivableTeammateOverrideForTest()
{
    FocusedRevivableTeammateOverrideForTest.Reset();
}
#endif
