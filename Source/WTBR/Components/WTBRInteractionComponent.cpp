// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRInteractionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "WTBRCharacter.h"

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

    return bHit ? Cast<AWTBRCorpseLootContainerActor>(Hit.GetActor()) : nullptr;
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
        return false;
    }

    if (!FocusedContainer->CanBeInteractedWithBy(OwnerCharacter))
    {
        return false;
    }

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
        UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] Ground focus trace hit nothing (range=%.0f)."),
            InteractionTraceDistance);
        return nullptr;
    }

    AWTBRGroundItemActor* GroundItem = Cast<AWTBRGroundItemActor>(Hit.GetActor());
    UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] Ground focus trace hit %s (isGroundItem=%s) at %.0f/%.0f units."),
        *GetNameSafe(Hit.GetActor()),
        GroundItem ? TEXT("true") : TEXT("false"),
        (Hit.Location - TraceStart).Size(),
        InteractionTraceDistance);
    return GroundItem;
}

bool UWTBRInteractionComponent::RequestContextInteract()
{
    UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] RequestContextInteract called (owner=%s)."),
        *GetNameSafe(GetOwner()));

    // Priority 1 — corpse / container / chest.
    // Reuses the existing client-side loot request bridge (no gameplay mutation).
    if (RequestCorpseLootInteract())
    {
        UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] Handled by corpse/container/chest focus (priority 1)."));
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
            UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] Dropped trigger %s focused (priority 2); routing to RequestPickupAimedDroppedTriggerByConstraint."),
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
            UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] Ground item %s focused (priority 3); dispatching Server_RequestPickupGroundItem."),
                *GetNameSafe(FocusedGroundItem));
            OwnerCharacter->Server_RequestPickupGroundItem(FocusedGroundItem);
            return true;
        }

        UE_LOG(LogTemp, Warning, TEXT("[WTBR Interact] Ground item %s focused but owner is not an AWTBRCharacter; cannot dispatch."),
            *GetNameSafe(FocusedGroundItem));
        return false;
    }

    // Priority 4 — generic interactable.
    // Generic interactable branch waits for interface pass
    // (no interactable interface exists yet).

    // Priority 5 — no valid focus: no-op.
    UE_LOG(LogTemp, Log, TEXT("[WTBR Interact] No valid focus for context interact (no-op)."));
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
#endif
