// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRHUDViewModelComponent.h"

#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRInteractionComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Inventory/WTBRInventorySlot.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "UI/WTBRInputBindingDisplayLibrary.h"

UWTBRHUDViewModelComponent::UWTBRHUDViewModelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWTBRHUDViewModelComponent::BeginPlay()
{
    Super::BeginPlay();
    BindOwnerDelegates();
    RefreshHUDSnapshot();
}

void UWTBRHUDViewModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindOwnerDelegates();
    Super::EndPlay(EndPlayReason);
}

void UWTBRHUDViewModelComponent::BindOwnerDelegates()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Character))
    {
        return;
    }

    if (IsValid(Character->HealthComponent))
    {
        Character->HealthComponent->OnHPChanged.AddUniqueDynamic(this, &UWTBRHUDViewModelComponent::OnHPChangedHandler);
    }

    if (IsValid(Character->VaelComponent))
    {
        Character->VaelComponent->OnVaelChanged.AddUniqueDynamic(this, &UWTBRHUDViewModelComponent::OnVaelChangedHandler);
    }

    if (IsValid(Character->InventoryComponent))
    {
        Character->InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &UWTBRHUDViewModelComponent::OnInventoryChangedHandler);
    }

    if (IsValid(Character->TriggerSetComponent))
    {
        Character->TriggerSetComponent->OnActiveTriggerChanged.AddUniqueDynamic(this, &UWTBRHUDViewModelComponent::OnActiveTriggerChangedHandler);
    }

    if (const UWorld* World = GetWorld())
    {
        if (AWTBRGameState* GameState = World->GetGameState<AWTBRGameState>())
        {
            GameState->OnMatchPhaseChanged.AddUniqueDynamic(this, &UWTBRHUDViewModelComponent::OnMatchPhaseChangedHandler);
        }
    }
}

void UWTBRHUDViewModelComponent::UnbindOwnerDelegates()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (IsValid(Character))
    {
        if (IsValid(Character->HealthComponent))
        {
            Character->HealthComponent->OnHPChanged.RemoveDynamic(this, &UWTBRHUDViewModelComponent::OnHPChangedHandler);
        }

        if (IsValid(Character->VaelComponent))
        {
            Character->VaelComponent->OnVaelChanged.RemoveDynamic(this, &UWTBRHUDViewModelComponent::OnVaelChangedHandler);
        }

        if (IsValid(Character->InventoryComponent))
        {
            Character->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UWTBRHUDViewModelComponent::OnInventoryChangedHandler);
        }

        if (IsValid(Character->TriggerSetComponent))
        {
            Character->TriggerSetComponent->OnActiveTriggerChanged.RemoveDynamic(this, &UWTBRHUDViewModelComponent::OnActiveTriggerChangedHandler);
        }
    }

    if (const UWorld* World = GetWorld())
    {
        if (AWTBRGameState* GameState = World->GetGameState<AWTBRGameState>())
        {
            GameState->OnMatchPhaseChanged.RemoveDynamic(this, &UWTBRHUDViewModelComponent::OnMatchPhaseChangedHandler);
        }
    }
}

void UWTBRHUDViewModelComponent::OnHPChangedHandler(float /*NewHP*/, float /*MaxHP*/)
{
    RefreshHUDSnapshot();
}

void UWTBRHUDViewModelComponent::OnVaelChangedHandler(float /*NewVael*/, float /*MaxVael*/)
{
    RefreshHUDSnapshot();
}

void UWTBRHUDViewModelComponent::OnInventoryChangedHandler()
{
    RefreshHUDSnapshot();
}

void UWTBRHUDViewModelComponent::OnActiveTriggerChangedHandler(ETriggerSlot /*NewSlot*/)
{
    RefreshHUDSnapshot();
}

void UWTBRHUDViewModelComponent::OnMatchPhaseChangedHandler(EWTBRMatchPhase /*MatchPhase*/)
{
    RefreshHUDSnapshot();
}

FWTBRHUDSnapshot UWTBRHUDViewModelComponent::GetHUDSnapshot() const
{
    return CachedSnapshot;
}

void UWTBRHUDViewModelComponent::RefreshHUDSnapshot()
{
    CachedSnapshot = BuildLiveSnapshot();
    OnHUDSnapshotChanged.Broadcast();
}

FWTBRHUDSnapshot UWTBRHUDViewModelComponent::BuildLiveSnapshot()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Character))
    {
        CachedQuickItemSlotIndex = INDEX_NONE;
        return BuildDefaultSnapshot();
    }

    FWTBRHUDSnapshot Snapshot;
    Snapshot.Match = BuildMatchSnapshot();
    Snapshot.Vitals = BuildVitalsSnapshot(Character);
    Snapshot.MainTrigger = BuildTriggerCardSnapshot(Character, true);
    Snapshot.SubTrigger = BuildTriggerCardSnapshot(Character, false);
    Snapshot.QuickItem = BuildQuickItemSnapshot(Character, CachedQuickItemSlotIndex);
    Snapshot.PickupPrompt = BuildPickupPromptSnapshot(Character);

    return Snapshot;
}

FWTBRHUDVitalsSnapshot UWTBRHUDViewModelComponent::BuildVitalsSnapshot(const AWTBRCharacter* Character) const
{
    FWTBRHUDVitalsSnapshot Snapshot;
    if (!IsValid(Character))
    {
        return Snapshot;
    }

    const UWTBRHealthComponent* Health = Character->HealthComponent;
    const UWTBRVaelComponent* Vael = Character->VaelComponent;
    Snapshot.bIsValid = IsValid(Health) || IsValid(Vael);

    if (IsValid(Health))
    {
        Snapshot.HP = Health->GetCurrentHP();
        Snapshot.MaxHP = Health->GetMaxHP();
    }

    if (IsValid(Vael))
    {
        Snapshot.Vael = Vael->GetCurrentVael();
        Snapshot.MaxVael = Vael->GetMaxVael();
    }

    return Snapshot;
}

FWTBRHUDTriggerCardSnapshot UWTBRHUDViewModelComponent::BuildTriggerCardSnapshot(
    const AWTBRCharacter* Character,
    bool bIsMain) const
{
    FWTBRHUDTriggerCardSnapshot Snapshot;
    if (!IsValid(Character))
    {
        return Snapshot;
    }

    Snapshot.bIsMain = bIsMain;
    Snapshot.TriggerName = bIsMain
        ? Character->GetMainTriggerNameText()
        : Character->GetSubTriggerNameText();
    Snapshot.Icon = bIsMain
        ? Character->GetMainTriggerHUDIcon()
        : Character->GetSubTriggerHUDIcon();
    Snapshot.ActiveSlotIndex = bIsMain
        ? Character->GetActiveMainTriggerSlotIndex()
        : Character->GetActiveSubTriggerSlotIndex();
    Snapshot.bCanAffordVaelCost = bIsMain
        ? Character->CanAffordActiveMainTriggerForHUD()
        : Character->CanAffordActiveSubTriggerForHUD();

    UInputAction* InputAction = bIsMain
        ? Character->GetFireMainInputAction()
        : Character->GetFireSubInputAction();
    Snapshot.UseBinding = UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(
        Character->GetDefaultMappingContext(), InputAction);
    Snapshot.bIsValid = Snapshot.ActiveSlotIndex != INDEX_NONE
        || !Snapshot.TriggerName.IsEmpty()
        || !Snapshot.Icon.IsNull();

    return Snapshot;
}

FWTBRHUDQuickItemSnapshot UWTBRHUDViewModelComponent::BuildQuickItemSnapshot(
    const AWTBRCharacter* Character,
    int32& OutSlotIndex) const
{
    FWTBRHUDQuickItemSnapshot Snapshot;
    // Display refresh is a hot path; never force a synchronous asset load here.
    OutSlotIndex = ResolveQuickItemSlotIndex(Character, /*bAllowSyncLoad=*/false);
    if (!IsValid(Character) || OutSlotIndex == INDEX_NONE || !IsValid(Character->InventoryComponent))
    {
        return Snapshot;
    }

    const TArray<FWTBRInventorySlot>& Slots = Character->InventoryComponent->GetInventorySlots();
    if (!Slots.IsValidIndex(OutSlotIndex))
    {
        OutSlotIndex = INDEX_NONE;
        return Snapshot;
    }

    const FWTBRInventorySlot& Slot = Slots[OutSlotIndex];
    // Already-resident item data only; a not-yet-loaded slot shows no quick item this frame.
    const UWTBRItemDataAsset* ItemData = Slot.ItemData.Get();
    if (!IsValid(ItemData))
    {
        OutSlotIndex = INDEX_NONE;
        return Snapshot;
    }

    Snapshot.bHasItem = true;
    Snapshot.Icon = ItemData->HUDIcon;
    Snapshot.Count = Slot.Quantity;
    Snapshot.State = Slot.Quantity <= 1
        ? EWTBRHUDQuickItemState::Warning
        : EWTBRHUDQuickItemState::Normal;

    return Snapshot;
}

FWTBRHUDPickupPromptSnapshot UWTBRHUDViewModelComponent::BuildPickupPromptSnapshot(const AWTBRCharacter* Character) const
{
    FWTBRHUDPickupPromptSnapshot Snapshot;
    if (!IsValid(Character) || !IsValid(Character->InteractionComponent))
    {
        return Snapshot;
    }

    Snapshot.bIsVisible = Character->InteractionComponent->HasValidFocusedInteractable();
    Snapshot.PromptText = Snapshot.bIsVisible
        ? Character->InteractionComponent->GetFocusedInteractionPromptText()
        : FText::GetEmpty();
    Snapshot.Binding = UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(
        Character->GetDefaultMappingContext(), Character->GetInteractInputAction());

    return Snapshot;
}

FWTBRHUDMatchSnapshot UWTBRHUDViewModelComponent::BuildMatchSnapshot() const
{
    FWTBRHUDMatchSnapshot Snapshot;
    if (const UWorld* World = GetWorld())
    {
        if (const AWTBRGameState* GameState = World->GetGameState<AWTBRGameState>())
        {
            const EWTBRMatchPhase MatchPhase = GameState->GetCurrentMatchPhase();
            Snapshot.bHasMatchPhase = true;
            if (const UEnum* MatchPhaseEnum = StaticEnum<EWTBRMatchPhase>())
            {
                Snapshot.MatchPhaseText = MatchPhaseEnum->GetDisplayNameTextByValue(static_cast<int64>(MatchPhase));
            }
        }
    }

    return Snapshot;
}

int32 UWTBRHUDViewModelComponent::ResolveQuickItemSlotIndex(const AWTBRCharacter* Character, bool bAllowSyncLoad)
{
    if (!IsValid(Character) || !IsValid(Character->InventoryComponent))
    {
        return INDEX_NONE;
    }

    const TArray<FWTBRInventorySlot>& Slots = Character->InventoryComponent->GetInventorySlots();
    for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
    {
        const FWTBRInventorySlot& Slot = Slots[SlotIndex];
        if (Slot.IsEmpty())
        {
            continue;
        }

        // Display refresh passes bAllowSyncLoad=false to stay non-blocking; the use-item
        // request passes true so a player action never fails to find a valid quick item.
        const UWTBRItemDataAsset* ItemData = bAllowSyncLoad
            ? Slot.ItemData.LoadSynchronous()
            : Slot.ItemData.Get();
        if (!IsValid(ItemData))
        {
            continue;
        }

        if (ItemData->ItemType == EWTBRItemType::Consumable
            || ItemData->ItemType == EWTBRItemType::VaelItem)
        {
            return SlotIndex;
        }
    }

    return INDEX_NONE;
}

bool UWTBRHUDViewModelComponent::RequestUseDisplayedQuickItem()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Character) || !Character->IsLocallyControlled())
    {
        return false;
    }

    // Real player action (not a display refresh): allow a sync load so a valid quick
    // item is never missed just because its data asset was not yet resident.
    const int32 ResolvedSlotIndex = ResolveQuickItemSlotIndex(Character, /*bAllowSyncLoad=*/true);
    CachedQuickItemSlotIndex = ResolvedSlotIndex;
    if (ResolvedSlotIndex == INDEX_NONE)
    {
        return false;
    }

    Character->Server_RequestUseInventoryItem(ResolvedSlotIndex);
    return true;
}

bool UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Character) || !Character->IsLocallyControlled() || !IsValid(Character->InteractionComponent))
    {
        return false;
    }

    return Character->InteractionComponent->RequestContextInteract();
}

bool UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()
{
    AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Character) || !Character->IsLocallyControlled())
    {
        return false;
    }

    Character->CancelCurrentAction();
    return true;
}

FWTBRHUDSnapshot UWTBRHUDViewModelComponent::BuildDefaultSnapshot() const
{
    return FWTBRHUDSnapshot();
}
