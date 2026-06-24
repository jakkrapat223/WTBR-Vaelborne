// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRCharacter.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Net/UnrealNetwork.h"

UWTBRTriggerSetComponent::UWTBRTriggerSetComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    TriggerSlots.Init(FWTBRTriggerSlot(), TotalSlotCount);
    RuntimeTriggers.Init(nullptr, TotalSlotCount);
}

void UWTBRTriggerSetComponent::BeginPlay()
{
    Super::BeginPlay();
    if (TriggerSlots.Num() != TotalSlotCount)
    {
        TriggerSlots.Init(FWTBRTriggerSlot(), TotalSlotCount);
    }
    if (RuntimeTriggers.Num() != TotalSlotCount)
    {
        RuntimeTriggers.Init(nullptr, TotalSlotCount);
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
    if (!HasServerAuthority())
    {
        return;
    }

    const int32 Idx = (int32)Slot;
    if (TriggerSlots.IsValidIndex(Idx) && RuntimeTriggers.IsValidIndex(Idx))
    {
        RuntimeTriggers[Idx] = Trigger;
        if (Trigger)
        {
            TriggerSlots[Idx].CachedCategory = Trigger->Category;
        }
    }
}

void UWTBRTriggerSetComponent::SwitchMainSlot(int32 SlotIndex)
{
    if (!HasServerAuthority())
    {
        return;
    }

    if (SlotIndex >= 0 && SlotIndex < MainSlotCount)
    {
        ActiveMainIndex = SlotIndex;
        AsyncLoadSlot(SlotIndex, [this, SlotIndex]()
        {
            if (ActiveMainIndex == SlotIndex)
            {
                UpdateDualWieldState();
                OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
                UE_LOG(LogTemp, Log, TEXT("WTBR Main trigger switched to slot %d"), SlotIndex);
            }
        });

        if (!TriggerSlots.IsValidIndex(SlotIndex) || TriggerSlots[SlotIndex].IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("WTBR Main trigger slot %d is empty"), SlotIndex);
            UpdateDualWieldState();
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
        }
    }
}

void UWTBRTriggerSetComponent::SwitchSubSlot(int32 SlotIndex)
{
    if (!HasServerAuthority())
    {
        return;
    }

    const int32 AbsIdx = SlotIndex + MainSlotCount;
    if (AbsIdx >= MainSlotCount && AbsIdx < TotalSlotCount)
    {
        ActiveSubIndex = AbsIdx;
        AsyncLoadSlot(AbsIdx, [this, AbsIdx]()
        {
            if (ActiveSubIndex == AbsIdx)
            {
                UpdateDualWieldState();
                OnActiveTriggerChanged.Broadcast((ETriggerSlot)AbsIdx);
                UE_LOG(LogTemp, Log, TEXT("WTBR Sub trigger switched to slot %d"), AbsIdx);
            }
        });

        if (!TriggerSlots.IsValidIndex(AbsIdx) || TriggerSlots[AbsIdx].IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("WTBR Sub trigger slot %d is empty"), AbsIdx);
            UpdateDualWieldState();
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)AbsIdx);
        }
    }
}

void UWTBRTriggerSetComponent::CycleMainSlot()
{
    if (!HasServerAuthority()) return;

    const int32 NewIndex = (ActiveMainIndex + 1) % MainSlotCount;
    ActiveMainIndex = NewIndex;

    AsyncLoadSlot(NewIndex, [this, NewIndex]()
    {
        if (HasServerAuthority())
        {
            InstantiateRuntimeTrigger(NewIndex);
            UpdateDualWieldState();
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)NewIndex);
        }
    });
}

void UWTBRTriggerSetComponent::CycleSubSlot()
{
    if (!HasServerAuthority()) return;

    const int32 CurrentRelativeIndex = ActiveSubIndex - MainSlotCount;
    const int32 NewRelativeIndex     = (CurrentRelativeIndex + 1) % SubSlotCount;
    const int32 NewAbsIndex          = NewRelativeIndex + MainSlotCount;
    ActiveSubIndex = NewAbsIndex;

    AsyncLoadSlot(NewAbsIndex, [this, NewAbsIndex]()
    {
        if (HasServerAuthority())
        {
            InstantiateRuntimeTrigger(NewAbsIndex);
            UpdateDualWieldState();
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)NewAbsIndex);
        }
    });
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveMainTrigger() const
{
    if (!RuntimeTriggers.IsValidIndex(ActiveMainIndex) || !RuntimeTriggers[ActiveMainIndex])
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR GetActiveMainTrigger: slot %d RuntimeTrigger is nullptr"),
            ActiveMainIndex);
        return nullptr;
    }
    return RuntimeTriggers[ActiveMainIndex];
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveSubTrigger() const
{
    return RuntimeTriggers.IsValidIndex(ActiveSubIndex) ? RuntimeTriggers[ActiveSubIndex] : nullptr;
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetTriggerInSlot(ETriggerSlot Slot) const
{
    const int32 Idx = (int32)Slot;
    return RuntimeTriggers.IsValidIndex(Idx) ? RuntimeTriggers[Idx] : nullptr;
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
    if (!HasServerAuthority()) return;
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
        InitializeLoadedSlot(SlotIndex);
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
                InitializeLoadedSlot(SlotIndex);
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

void UWTBRTriggerSetComponent::InitializeLoadedSlot(int32 SlotIndex)
{
    if (!HasServerAuthority())
    {
        return;
    }

    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }
    if (!RuntimeTriggers.IsValidIndex(SlotIndex))
    {
        return;
    }

    FWTBRTriggerSlot& Slot = TriggerSlots[SlotIndex];
    UWTBRTriggerDataAsset* LoadedDataAsset = Slot.DataAsset.Get();
    if (!LoadedDataAsset)
    {
        return;
    }

    Slot.CachedCategory = LoadedDataAsset->Category;
    if (RuntimeTriggers[SlotIndex])
    {
        return;
    }

    if (!LoadedDataAsset->TriggerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger slot %d has DataAsset %s but no TriggerClass"),
            SlotIndex,
            *LoadedDataAsset->GetName());
        return;
    }

    UWTBRTriggerBase* NewTrigger = NewObject<UWTBRTriggerBase>(this, LoadedDataAsset->TriggerClass);
    if (!NewTrigger)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR failed to instantiate trigger for slot %d"), SlotIndex);
        return;
    }

    NewTrigger->InitializeTrigger(Cast<AWTBRCharacter>(GetOwner()), LoadedDataAsset);
    RuntimeTriggers[SlotIndex] = NewTrigger;
}

void UWTBRTriggerSetComponent::InstantiateRuntimeTrigger(int32 SlotIndex)
{
    if (!HasServerAuthority()) return;
    if (!IsValidSlotIndex(SlotIndex)) return;
    if (!RuntimeTriggers.IsValidIndex(SlotIndex)) return;
    if (RuntimeTriggers[SlotIndex]) return; // Already instantiated

    UWTBRTriggerDataAsset* DA = TriggerSlots[SlotIndex].DataAsset.Get();
    if (!DA)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR InstantiateRuntimeTrigger: slot %d DataAsset not in memory — skipping"),
            SlotIndex);
        return;
    }
    if (!DA->TriggerClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR InstantiateRuntimeTrigger: slot %d DataAsset [%s] has no TriggerClass"),
            SlotIndex, *DA->GetName());
        return;
    }

    UWTBRTriggerBase* NewTrigger = NewObject<UWTBRTriggerBase>(this, DA->TriggerClass);
    if (!NewTrigger)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR InstantiateRuntimeTrigger: NewObject failed for slot %d"),
            SlotIndex);
        return;
    }

    NewTrigger->InitializeTrigger(Cast<AWTBRCharacter>(GetOwner()), DA);
    RuntimeTriggers[SlotIndex] = NewTrigger;
    UE_LOG(LogTemp, Log,
        TEXT("WTBR Instantiated [%s] for slot %d"),
        *DA->TriggerClass->GetName(), SlotIndex);
}

bool UWTBRTriggerSetComponent::HasServerAuthority() const
{
    const AActor* Owner = GetOwner();
    return Owner && Owner->HasAuthority();
}

void UWTBRTriggerSetComponent::Server_SetTriggerLoadout_Implementation(
    const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout)
{
    for (int32 i = 0; i < TotalSlotCount && i < InLoadout.Num(); ++i)
    {
        TriggerSlots[i].DataAsset = InLoadout[i];
        TriggerSlots[i].CachedCategory = ETriggerCategory::None;
        if (RuntimeTriggers.IsValidIndex(i))
        {
            RuntimeTriggers[i] = nullptr;
        }
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
