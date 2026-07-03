// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRInteractionComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "WTBRCharacter.h"

UWTBRInteractionComponent::UWTBRInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

AWTBRCorpseLootContainerActor* UWTBRInteractionComponent::GetFocusedCorpseLootContainer() const
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

AWTBRGroundItemActor* UWTBRInteractionComponent::GetFocusedGroundItem() const
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

    // Priority 2 — dropped trigger.
    // TODO(S4-B): Dropped Trigger branch needs target slot policy before implementation.
    // A server-authoritative pickup path already exists on AWTBRCharacter
    // (Server_RequestPickupDroppedTrigger, resolved via FindAimedDroppedTriggerForPickup),
    // but it exposes two distinct targets — RequestPickupAimedDroppedTriggerIntoActiveMainSlot
    // and ...IntoActiveSubSlot. A single context-interact press has no explicit
    // active-main vs active-sub rule yet, so the slot must not be guessed here.

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
