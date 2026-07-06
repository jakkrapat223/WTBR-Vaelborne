// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRBagLootViewModelComponent.h"

#include "WTBRCharacter.h"
#include "Components/WTBRInteractionComponent.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Inventory/WTBRInventorySlot.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRValidationLog.h"
#include "TimerManager.h"

namespace
{
    // 5 Hz is enough for a pickup-prompt/panel focus update and costs at most one
    // extra line trace per interval for the single locally controlled character.
    constexpr float WTBRBagLootFocusPollIntervalSeconds = 0.2f;
}

UWTBRBagLootViewModelComponent::UWTBRBagLootViewModelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWTBRBagLootViewModelComponent::BeginPlay()
{
    Super::BeginPlay();
    BindOwnerDelegates();
    RefreshBagLootSnapshot();

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() != NM_DedicatedServer)
        {
            World->GetTimerManager().SetTimer(
                FocusPollTimerHandle,
                this, &UWTBRBagLootViewModelComponent::PollFocusedContainerChange,
                WTBRBagLootFocusPollIntervalSeconds,
                /*bLoop=*/true);
        }
    }
}

void UWTBRBagLootViewModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FocusPollTimerHandle);
    }
    UnbindOwnerDelegates();
    Super::EndPlay(EndPlayReason);
}

void UWTBRBagLootViewModelComponent::PollFocusedContainerChange()
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    if (!IsValid(Character) || !Character->IsLocallyControlled())
    {
        return;
    }

    AWTBRCorpseLootContainerActor* FocusedContainer = ResolveFocusedCorpseLootContainer(Character);
    const bool bHasFocus = (FocusedContainer != nullptr);
    // The had-focus flag also catches "focused container was destroyed while aimed
    // elsewhere" (weak ptr and fresh resolve both null, but the panel is stale).
    if (FocusedContainer == LastPolledFocusedContainer.Get()
        && bHasFocus == bLastPolledHadFocusedContainer)
    {
        return;
    }

    LastPolledFocusedContainer = FocusedContainer;
    bLastPolledHadFocusedContainer = bHasFocus;
    RefreshBagLootSnapshot();
}

void UWTBRBagLootViewModelComponent::BindOwnerDelegates()
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    if (!IsValid(Character))
    {
        return;
    }

    if (IsValid(Character->InventoryComponent))
    {
        Character->InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &UWTBRBagLootViewModelComponent::OnInventoryChangedHandler);
    }
}

void UWTBRBagLootViewModelComponent::UnbindOwnerDelegates()
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    if (IsValid(Character) && IsValid(Character->InventoryComponent))
    {
        Character->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UWTBRBagLootViewModelComponent::OnInventoryChangedHandler);
    }

    if (AWTBRCorpseLootContainerActor* PreviouslyBound = BoundCorpseLootContainer.Get())
    {
        PreviouslyBound->OnCorpseLootEntriesChanged.RemoveDynamic(this, &UWTBRBagLootViewModelComponent::OnCorpseLootEntriesChangedHandler);
    }
    BoundCorpseLootContainer.Reset();
}

AWTBRCharacter* UWTBRBagLootViewModelComponent::ResolveOwnerCharacter() const
{
    return Cast<AWTBRCharacter>(GetOwner());
}

AWTBRCorpseLootContainerActor* UWTBRBagLootViewModelComponent::ResolveFocusedCorpseLootContainer(const AWTBRCharacter* Character) const
{
    if (!IsValid(Character) || !IsValid(Character->InteractionComponent))
    {
        return nullptr;
    }
    return Character->InteractionComponent->GetFocusedCorpseLootContainer();
}

void UWTBRBagLootViewModelComponent::RebindFocusedCorpseLootContainerDelegate(AWTBRCorpseLootContainerActor* FocusedContainer)
{
    AWTBRCorpseLootContainerActor* PreviouslyBound = BoundCorpseLootContainer.Get();
    if (PreviouslyBound == FocusedContainer)
    {
        return;
    }

    if (IsValid(PreviouslyBound))
    {
        PreviouslyBound->OnCorpseLootEntriesChanged.RemoveDynamic(this, &UWTBRBagLootViewModelComponent::OnCorpseLootEntriesChangedHandler);
    }

    if (IsValid(FocusedContainer))
    {
        FocusedContainer->OnCorpseLootEntriesChanged.AddUniqueDynamic(this, &UWTBRBagLootViewModelComponent::OnCorpseLootEntriesChangedHandler);
    }

    BoundCorpseLootContainer = FocusedContainer;
}

void UWTBRBagLootViewModelComponent::OnInventoryChangedHandler()
{
    RefreshBagLootSnapshot();
}

void UWTBRBagLootViewModelComponent::OnCorpseLootEntriesChangedHandler()
{
    RefreshBagLootSnapshot();
}

FWTBRBagLootSnapshot UWTBRBagLootViewModelComponent::GetBagLootSnapshot() const
{
    return CachedSnapshot;
}

void UWTBRBagLootViewModelComponent::RefreshBagLootSnapshot()
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    AWTBRCorpseLootContainerActor* FocusedContainer = ResolveFocusedCorpseLootContainer(Character);

    RebindFocusedCorpseLootContainerDelegate(FocusedContainer);

    CachedSnapshot = BuildLiveSnapshot(Character, FocusedContainer);

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[WTBR BagLoot] RefreshBagLootSnapshot: Owner=%s FocusedContainer=%s CorpseLootVisible=%s EntryCount=%d"),
        *GetNameSafe(Character),
        *GetNameSafe(FocusedContainer),
        CachedSnapshot.bCorpseLootVisible ? TEXT("true") : TEXT("false"),
        CachedSnapshot.CorpseLoot.Entries.Num());

    OnBagLootSnapshotChanged.Broadcast();
}

FWTBRBagLootSnapshot UWTBRBagLootViewModelComponent::BuildLiveSnapshot(
    const AWTBRCharacter* Character,
    const AWTBRCorpseLootContainerActor* FocusedContainer) const
{
    FWTBRBagLootSnapshot Snapshot;
    Snapshot.Bag = BuildBagPanelSnapshot(Character);
    Snapshot.CorpseLoot = BuildCorpseLootPanelSnapshot(FocusedContainer);
    Snapshot.bCorpseLootVisible = Snapshot.CorpseLoot.bHasFocusedContainer;
    // Snapshot.bBagVisible intentionally left at its default (false) — see the
    // TODO on FWTBRBagLootSnapshot::bBagVisible; no open/close state source exists.
    return Snapshot;
}

FWTBRBagPanelSnapshot UWTBRBagLootViewModelComponent::BuildBagPanelSnapshot(const AWTBRCharacter* Character) const
{
    FWTBRBagPanelSnapshot Snapshot;
    if (!IsValid(Character) || !IsValid(Character->InventoryComponent))
    {
        return Snapshot;
    }

    const TArray<FWTBRInventorySlot>& Slots = Character->InventoryComponent->GetInventorySlots();
    Snapshot.MaxSlots = Character->InventoryComponent->InventorySlotCount;

    Snapshot.Slots.Reserve(Slots.Num());
    for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
    {
        const FWTBRBagSlotSnapshot SlotSnapshot = BuildBagSlotSnapshot(SlotIndex, Slots[SlotIndex]);
        if (!SlotSnapshot.bIsEmpty)
        {
            ++Snapshot.CurrentSlotsUsed;
        }
        Snapshot.Slots.Add(SlotSnapshot);
    }

    Snapshot.CapacityText = FormatCapacityText(Snapshot.CurrentSlotsUsed, Snapshot.MaxSlots);
    Snapshot.WeightText = FormatUnknownWeightText();
    Snapshot.WarningText = (Snapshot.MaxSlots > 0 && Snapshot.CurrentSlotsUsed >= Snapshot.MaxSlots)
        ? NSLOCTEXT("WTBRBagLoot", "BagFullWarning", "BAG FULL / CANNOT TRANSFER")
        : FText::GetEmpty();

    return Snapshot;
}

FWTBRBagSlotSnapshot UWTBRBagLootViewModelComponent::BuildBagSlotSnapshot(int32 SlotIndex, const FWTBRInventorySlot& Slot) const
{
    FWTBRBagSlotSnapshot Snapshot;
    Snapshot.SlotIndex = SlotIndex;
    Snapshot.bIsEmpty = Slot.IsEmpty();
    Snapshot.bHasItem = !Snapshot.bIsEmpty;

    if (!Snapshot.bHasItem)
    {
        return Snapshot;
    }

    Snapshot.Quantity = Slot.Quantity;
    Snapshot.CountText = FormatCountText(Slot.Quantity);

    // Display refresh is a hot path; never force a synchronous asset load here
    // (mirrors UWTBRHUDViewModelComponent::BuildQuickItemSnapshot()'s rule).
    const UWTBRItemDataAsset* ItemData = Slot.ItemData.Get();
    if (!IsValid(ItemData))
    {
        return Snapshot;
    }

    Snapshot.DisplayName = ItemData->DisplayName;
    Snapshot.Icon = ItemData->HUDIcon;
    if (const UEnum* ItemTypeEnum = StaticEnum<EWTBRItemType>())
    {
        Snapshot.ItemTypeText = ItemTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(ItemData->ItemType));
    }

    return Snapshot;
}

FWTBRCorpseLootPanelSnapshot UWTBRBagLootViewModelComponent::BuildCorpseLootPanelSnapshot(const AWTBRCorpseLootContainerActor* FocusedContainer) const
{
    FWTBRCorpseLootPanelSnapshot Snapshot;
    if (!IsValid(FocusedContainer))
    {
        return Snapshot;
    }

    Snapshot.bHasFocusedContainer = true;
    // "TRIGGER CACHE" reuses the same terminology as the container's own
    // GetInteractionPromptText() ("Open Trigger Cache") — not invented, just kept
    // consistent. No dynamic per-container title field exists on the actor.
    Snapshot.TitleText = NSLOCTEXT("WTBRBagLoot", "CorpseLootTitle", "TRIGGER CACHE");
    Snapshot.SubtitleText = FText::Format(
        NSLOCTEXT("WTBRBagLoot", "CorpseLootSubtitleFmt", "{0} ITEMS"),
        FText::AsNumber(FocusedContainer->GetLootEntryCount()));
    Snapshot.PromptText = FocusedContainer->GetInteractionPromptText();

    const int32 EntryCount = FocusedContainer->GetLootEntryCount();
    Snapshot.Entries.Reserve(EntryCount);
    for (int32 LootIndex = 0; LootIndex < EntryCount; ++LootIndex)
    {
        Snapshot.Entries.Add(BuildLootEntrySnapshot(FocusedContainer, LootIndex));
    }

    return Snapshot;
}

FWTBRLootEntrySnapshot UWTBRBagLootViewModelComponent::BuildLootEntrySnapshot(
    const AWTBRCorpseLootContainerActor* Container,
    int32 LootIndex) const
{
    FWTBRLootEntrySnapshot Snapshot;
    Snapshot.LootIndex = LootIndex;

    if (!IsValid(Container))
    {
        return Snapshot;
    }

    const FWTBRCorpseLootEntryViewModel EntryView = Container->BuildLootEntryViewModel(LootIndex);
    Snapshot.bHasEntry = EntryView.StableEntryIndex != INDEX_NONE;
    if (!Snapshot.bHasEntry)
    {
        return Snapshot;
    }

    // Corpse loot in this container is Trigger-only — no stacking, always singular.
    Snapshot.Quantity = 1;
    Snapshot.bCanRequestTake = EntryView.bIsAvailable;
    Snapshot.ActionText = EntryView.bIsAvailable
        ? NSLOCTEXT("WTBRBagLoot", "LootEntryActionRequest", "Request")
        : FText::GetEmpty();

    // Display refresh must never force a synchronous load — same rule as bag slots.
    const UWTBRTriggerDataAsset* DataAsset = EntryView.TriggerDataAsset.Get();
    if (IsValid(DataAsset))
    {
        Snapshot.DisplayName = DataAsset->DisplayName;
        Snapshot.Icon = DataAsset->HUDIcon;
        if (const UEnum* ConstraintEnum = StaticEnum<ETriggerSlotConstraint>())
        {
            Snapshot.TypeText = ConstraintEnum->GetDisplayNameTextByValue(static_cast<int64>(DataAsset->SlotConstraint));
        }
    }
    else if (const UEnum* CategoryEnum = StaticEnum<ETriggerCategory>())
    {
        // DataAsset not yet resident — fall back to the category already available
        // on the replicated entry itself (no load required). See
        // WBP_BagLoot_BindingPlan.md §1 "Type" column tradeoff note.
        Snapshot.TypeText = CategoryEnum->GetDisplayNameTextByValue(static_cast<int64>(EntryView.CachedCategory));
    }

    return Snapshot;
}

FText UWTBRBagLootViewModelComponent::FormatCapacityText(int32 CurrentUsed, int32 MaxSlots)
{
    return FText::Format(NSLOCTEXT("WTBRBagLoot", "CapacityFmt", "{0} / {1}"),
        FText::AsNumber(CurrentUsed), FText::AsNumber(MaxSlots));
}

FText UWTBRBagLootViewModelComponent::FormatCountText(int32 Quantity)
{
    return FText::Format(NSLOCTEXT("WTBRBagLoot", "CountFmt", "x{0}"), FText::AsNumber(Quantity));
}

FText UWTBRBagLootViewModelComponent::FormatUnknownWeightText()
{
    // No weight/volume system exists anywhere in UWTBRInventoryComponent (confirmed
    // by a full-module source grep — see WBP_BagLoot_BindingPlan.md §1/§5). Never
    // fabricate a number here — always render a neutral placeholder.
    return FText::FromString(TEXT("-"));
}

bool UWTBRBagLootViewModelComponent::RequestUseBagItem(int32 SlotIndex)
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    if (!IsValid(Character) || !Character->IsLocallyControlled())
    {
        return false;
    }

    Character->Server_RequestUseInventoryItem(SlotIndex);
    return true;
}

bool UWTBRBagLootViewModelComponent::RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex)
{
    AWTBRCharacter* Character = ResolveOwnerCharacter();
    if (!IsValid(Character) || !Character->IsLocallyControlled())
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* FocusedContainer = ResolveFocusedCorpseLootContainer(Character);
    if (!IsValid(FocusedContainer))
    {
        return false;
    }

    Character->Server_RequestPickupCorpseLootEntry(FocusedContainer, LootIndex, TargetSlotIndex);
    return true;
}
