// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Net/UnrealNetwork.h"

UWTBRTriggerSetComponent::UWTBRTriggerSetComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    TriggerSlots.Init(FWTBRTriggerSlot(), TotalSlotCount);
}

void UWTBRTriggerSetComponent::BeginPlay()
{
    Super::BeginPlay();
    if (TriggerSlots.Num() != TotalSlotCount)
    {
        TriggerSlots.Init(FWTBRTriggerSlot(), TotalSlotCount);
    }
}

void UWTBRTriggerSetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Cancel all in-flight loads when component is destroyed
    // (match end, player disconnect, etc.)
    for (auto& Pair : PendingSlotLoads)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->CancelHandle();
        }
    }
    PendingSlotLoads.Empty();

    Super::EndPlay(EndPlayReason);
}

void UWTBRTriggerSetComponent::InstallTrigger(ETriggerSlot Slot, UWTBRTriggerBase* Trigger)
{
    const int32 Idx = (int32)Slot;
    if (TriggerSlots.IsValidIndex(Idx))
    {
        TriggerSlots[Idx].RuntimeTrigger = Trigger;
        if (Trigger)
        {
            TriggerSlots[Idx].CachedCategory = Trigger->Category;
        }
    }
}

void UWTBRTriggerSetComponent::SwitchMainSlot(int32 SlotIndex)
{
    if (SlotIndex >= 0 && SlotIndex < MainSlotCount)
    {
        ActiveMainIndex = SlotIndex;
        UpdateDualWieldState();
        OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
    }
}

void UWTBRTriggerSetComponent::SwitchSubSlot(int32 SlotIndex)
{
    const int32 AbsIdx = SlotIndex + MainSlotCount;
    if (AbsIdx >= MainSlotCount && AbsIdx < TotalSlotCount)
    {
        ActiveSubIndex = AbsIdx;
        UpdateDualWieldState();
        OnActiveTriggerChanged.Broadcast((ETriggerSlot)AbsIdx);
    }
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveMainTrigger() const
{
    return TriggerSlots.IsValidIndex(ActiveMainIndex) ? TriggerSlots[ActiveMainIndex].RuntimeTrigger : nullptr;
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveSubTrigger() const
{
    return TriggerSlots.IsValidIndex(ActiveSubIndex) ? TriggerSlots[ActiveSubIndex].RuntimeTrigger : nullptr;
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetTriggerInSlot(ETriggerSlot Slot) const
{
    const int32 Idx = (int32)Slot;
    return TriggerSlots.IsValidIndex(Idx) ? TriggerSlots[Idx].RuntimeTrigger : nullptr;
}

void UWTBRTriggerSetComponent::UpdateDualWieldState()
{
    const UWTBRTriggerBase* Main = GetActiveMainTrigger();
    const UWTBRTriggerBase* Sub  = GetActiveSubTrigger();

    EWTBRDualWieldState NewState  = EWTBRDualWieldState::None;
    ETriggerCategory    ActiveCat = ETriggerCategory::None;

    // Pure Type-Match — no priority, no timing (GDD §3.4 Lock)
    if (Main && Sub && Main->Category == Sub->Category)
    {
        ActiveCat = Main->Category;
        switch (ActiveCat)
        {
            case ETriggerCategory::Melee:        NewState = EWTBRDualWieldState::DualMelee;    break;
            case ETriggerCategory::Gunner:       NewState = EWTBRDualWieldState::DualGunner;   break;
            case ETriggerCategory::Movement:     NewState = EWTBRDualWieldState::DualMovement; break;
            case ETriggerCategory::Defense:      NewState = EWTBRDualWieldState::DualDefense;  break;
            case ETriggerCategory::SniperBullet: NewState = EWTBRDualWieldState::DualSniper;   break;
            default: break;
        }
    }

    if (NewState != CurrentDualWieldState)
    {
        CurrentDualWieldState = NewState;
        OnDualWieldStateChanged.Broadcast(CurrentDualWieldState, ActiveCat);
    }
}

void UWTBRTriggerSetComponent::AsyncLoadSlot(int32 SlotIndex, TFunction<void()> OnComplete)
{
    if (!IsValidSlotIndex(SlotIndex)) return;
    FWTBRTriggerSlot& Slot = TriggerSlots[SlotIndex];
    if (Slot.IsEmpty()) return;

    // Cancel any previous in-flight load for this slot index.
    // Critical for rapid slot switching (Q spam) — prevents stale callbacks.
    if (TSharedPtr<FStreamableHandle>* ExistingHandle = PendingSlotLoads.Find(SlotIndex))
    {
        if (ExistingHandle->IsValid())
        {
            (*ExistingHandle)->CancelHandle();
        }
        PendingSlotLoads.Remove(SlotIndex);
    }

    // If DataAsset is already loaded into memory, use it immediately.
    // This is the hot path after first activation — no async needed.
    if (Slot.DataAsset.IsValid())
    {
        Slot.CachedCategory = Slot.DataAsset->Category;
        if (OnComplete) OnComplete();
        return;
    }

    // Lazy load via Asset Manager — asset NOT in memory yet.
    // This is the FIRST activation of this slot since match start.
    FStreamableManager& StreamableMgr = UAssetManager::GetStreamableManager();

    TSharedPtr<FStreamableHandle> Handle = StreamableMgr.RequestAsyncLoad(
        Slot.DataAsset.ToSoftObjectPath(),
        [this, SlotIndex, OnComplete]()
        {
            // Verify slot is still valid (player may have unequipped during load)
            if (!IsValidSlotIndex(SlotIndex)) return;

            FWTBRTriggerSlot& LoadedSlot = TriggerSlots[SlotIndex];
            if (LoadedSlot.DataAsset.IsValid())
            {
                LoadedSlot.CachedCategory = LoadedSlot.DataAsset->Category;
            }

            // Remove handle from map — load is complete
            PendingSlotLoads.Remove(SlotIndex);

            if (OnComplete) OnComplete();
        }
    );

    // Store handle so we can cancel it if slot switches before load completes
    if (Handle.IsValid())
    {
        PendingSlotLoads.Add(SlotIndex, Handle);
    }
}

void UWTBRTriggerSetComponent::Server_SetTriggerLoadout_Implementation(
    const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout)
{
    for (int32 i = 0; i < TotalSlotCount && i < InLoadout.Num(); ++i)
    {
        TriggerSlots[i].DataAsset = InLoadout[i];
    }
}

void UWTBRTriggerSetComponent::OnRep_TriggerSlots()
{
    // Client-side: slot array received from server — no cosmetic action needed yet.
}

void UWTBRTriggerSetComponent::OnRep_DualWieldState()
{
    // Client-side: react for animation/UI
}

void UWTBRTriggerSetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRTriggerSetComponent, TriggerSlots);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveMainIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveSubIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CurrentDualWieldState);
}
