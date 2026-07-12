// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRValidationLog.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "Trigger/WTBRSerpveilTrigger.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
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

    // Eagerly instantiate every slot whose DataAsset is already in memory.
    // EditDefaultsOnly assets are loaded before BeginPlay, so this is the
    // common case for all pre-configured loadouts.
    // RuntimeTriggers is Transient — both server and client run this loop.
    for (int32 i = 0; i < TotalSlotCount; ++i)
    {
        if (!TriggerSlots[i].IsEmpty() && TriggerSlots[i].DataAsset.IsValid())
        {
            InitializeLoadedSlot(i);
        }
    }

    if (HasServerAuthority())
    {
        if (TriggerSlots.IsValidIndex(ActiveMainIndex) && !TriggerSlots[ActiveMainIndex].IsEmpty())
        {
            SwitchMainSlot(ActiveMainIndex);
        }

        if (TriggerSlots.IsValidIndex(ActiveSubIndex) && !TriggerSlots[ActiveSubIndex].IsEmpty())
        {
            SwitchSubSlot(ActiveSubIndex - MainSlotCount);
        }
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

    if (!CanMutateTriggerLoadout())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger install rejected by match phase/rules for owner %s"),
            *GetNameSafe(GetOwner()));
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

    if (SlotIndex >= 0 && SlotIndex < MainSlotCount
        && SlotIndex != ActiveMainIndex
        && RuntimeTriggers.IsValidIndex(ActiveMainIndex))
    {
        if (UWTBRSerpveilTrigger* SerpveilTrigger =
            Cast<UWTBRSerpveilTrigger>(RuntimeTriggers[ActiveMainIndex]))
        {
            SerpveilTrigger->CancelCharge();
        }
    }

    // The outgoing active trigger is never Deactivate()'d on a slot switch —
    // only a full loadout replacement goes through that path. Clear any
    // cosmetic telegraph it left active so VFX doesn't stay stuck on switch.
    if (AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner()))
    {
        OwnerChar->ClearTriggerCosmeticVFXState();
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
    if (!HasServerAuthority()) return;

    // OnUnequipped() below is a no-op base virtual for Serpveil/Lacern — it does
    // not clear cosmetic telegraph state. Clear it centrally here instead.
    if (AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner()))
    {
        OwnerChar->ClearTriggerCosmeticVFXState();
    }

    const int32 AbsIdx = SlotIndex + MainSlotCount;
    if (AbsIdx < MainSlotCount || AbsIdx >= TotalSlotCount) return;

    const int32 OldAbsIdx = ActiveSubIndex;
    if (OldAbsIdx != AbsIdx && RuntimeTriggers.IsValidIndex(OldAbsIdx) && RuntimeTriggers[OldAbsIdx])
    {
        RuntimeTriggers[OldAbsIdx]->OnUnequipped();
        OnSubTriggerUnequipped.Broadcast(RuntimeTriggers[OldAbsIdx]);
    }

    ActiveSubIndex = AbsIdx;
    AsyncLoadSlot(AbsIdx, [this, AbsIdx]()
    {
        if (ActiveSubIndex == AbsIdx)
        {
            UpdateDualWieldState();
            if (RuntimeTriggers.IsValidIndex(AbsIdx) && RuntimeTriggers[AbsIdx])
            {
                RuntimeTriggers[AbsIdx]->OnEquipped();
                OnSubTriggerEquipped.Broadcast(RuntimeTriggers[AbsIdx]);
            }
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

void UWTBRTriggerSetComponent::CycleMainSlot()
{
    if (!HasServerAuthority()) return;

    if (RuntimeTriggers.IsValidIndex(ActiveMainIndex))
    {
        if (UWTBRSerpveilTrigger* SerpveilTrigger =
            Cast<UWTBRSerpveilTrigger>(RuntimeTriggers[ActiveMainIndex]))
        {
            SerpveilTrigger->CancelCharge();
        }
    }

    // See SwitchMainSlot — the outgoing trigger is never Deactivate()'d here.
    if (AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner()))
    {
        OwnerChar->ClearTriggerCosmeticVFXState();
    }

    const int32 OldIndex = ActiveMainIndex;
    const int32 NewIndex = (ActiveMainIndex + 1) % MainSlotCount;
    ActiveMainIndex = NewIndex;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test34 CycleMainSlot] Owner=%s | Old=%d | New=%d"),
        *GetNameSafe(GetOwner()),
        OldIndex,
        NewIndex);

    AsyncLoadSlot(NewIndex, [this, NewIndex]()
    {
        if (HasServerAuthority())
        {
            InstantiateRuntimeTrigger(NewIndex);
            UpdateDualWieldState();
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)NewIndex);
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Test34 CycleMainSlot Complete] Owner=%s | ActiveMainIndex=%d | Trigger=%s"),
                *GetNameSafe(GetOwner()),
                ActiveMainIndex,
                *GetNameSafe(GetActiveMainTrigger()));
        }
    });
}

void UWTBRTriggerSetComponent::CycleSubSlot()
{
    if (!HasServerAuthority()) return;

    // See SwitchSubSlot — OnUnequipped() below does not clear cosmetic state.
    if (AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner()))
    {
        OwnerChar->ClearTriggerCosmeticVFXState();
    }

    const int32 OldAbsIndex      = ActiveSubIndex;
    const int32 NewRelativeIndex = (OldAbsIndex - MainSlotCount + 1) % SubSlotCount;
    const int32 NewAbsIndex      = NewRelativeIndex + MainSlotCount;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test34 CycleSubSlot] Owner=%s | Old=%d | New=%d"),
        *GetNameSafe(GetOwner()),
        OldAbsIndex,
        NewAbsIndex);

    if (RuntimeTriggers.IsValidIndex(OldAbsIndex) && RuntimeTriggers[OldAbsIndex])
    {
        RuntimeTriggers[OldAbsIndex]->OnUnequipped();
        OnSubTriggerUnequipped.Broadcast(RuntimeTriggers[OldAbsIndex]);
    }

    ActiveSubIndex = NewAbsIndex;

    AsyncLoadSlot(NewAbsIndex, [this, NewAbsIndex]()
    {
        if (HasServerAuthority())
        {
            InstantiateRuntimeTrigger(NewAbsIndex);
            UpdateDualWieldState();
            if (ActiveSubIndex == NewAbsIndex && RuntimeTriggers.IsValidIndex(NewAbsIndex) && RuntimeTriggers[NewAbsIndex])
            {
                RuntimeTriggers[NewAbsIndex]->OnEquipped();
                OnSubTriggerEquipped.Broadcast(RuntimeTriggers[NewAbsIndex]);
            }
            OnActiveTriggerChanged.Broadcast((ETriggerSlot)NewAbsIndex);
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Test34 CycleSubSlot Complete] Owner=%s | ActiveSubIndex=%d | Trigger=%s"),
                *GetNameSafe(GetOwner()),
                ActiveSubIndex,
                *GetNameSafe(GetActiveSubTrigger()));
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

UWTBRTriggerDataAsset* UWTBRTriggerSetComponent::GetActiveMainDataAsset() const
{
    return TriggerSlots.IsValidIndex(ActiveMainIndex)
        ? TriggerSlots[ActiveMainIndex].DataAsset.Get()
        : nullptr;
}

UWTBRTriggerDataAsset* UWTBRTriggerSetComponent::GetActiveSubDataAsset() const
{
    return TriggerSlots.IsValidIndex(ActiveSubIndex)
        ? TriggerSlots[ActiveSubIndex].DataAsset.Get()
        : nullptr;
}

void UWTBRTriggerSetComponent::GetInstalledTriggerSlotSnapshots(TArray<FWTBRInstalledTriggerSlotSnapshot>& OutSnapshots) const
{
    OutSnapshots.Reset();

    for (int32 SlotIndex = 0; SlotIndex < TriggerSlots.Num(); ++SlotIndex)
    {
        const FWTBRTriggerSlot& Slot = TriggerSlots[SlotIndex];
        if (Slot.IsEmpty())
        {
            continue;
        }

        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        Snapshot.SlotIndex = SlotIndex;
        Snapshot.DataAsset = Slot.DataAsset;
        Snapshot.CachedCategory = Slot.CachedCategory;
        OutSnapshots.Add(Snapshot);
    }
}

bool UWTBRTriggerSetComponent::GetTriggerSlotSnapshot(int32 SlotIndex, FWTBRInstalledTriggerSlotSnapshot& OutSnapshot) const
{
    OutSnapshot = FWTBRInstalledTriggerSlotSnapshot();

    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    const FWTBRTriggerSlot& Slot = TriggerSlots[SlotIndex];
    if (Slot.IsEmpty())
    {
        return false;
    }

    OutSnapshot.SlotIndex = SlotIndex;
    OutSnapshot.DataAsset = Slot.DataAsset;
    OutSnapshot.CachedCategory = Slot.CachedCategory;
    return OutSnapshot.IsValid();
}

bool UWTBRTriggerSetComponent::ReplaceTriggerSlotFromDataAsset(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> NewDataAsset)
{
    if (!HasServerAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: owner %s has no authority"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    if (!CanMutateTriggerLoadout())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected by match phase/rules for owner %s"),
            *GetNameSafe(GetOwner()));
        return false;
    }

    if (!IsValidSlotIndex(SlotIndex) || !RuntimeTriggers.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: invalid slot %d for owner %s"),
            SlotIndex,
            *GetNameSafe(GetOwner()));
        return false;
    }

    if (NewDataAsset.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: null DataAsset for owner %s slot %d"),
            *GetNameSafe(GetOwner()),
            SlotIndex);
        return false;
    }

    UWTBRTriggerDataAsset* LoadedDataAsset = NewDataAsset.LoadSynchronous();
    if (!IsValid(LoadedDataAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: failed to load DataAsset for owner %s slot %d"),
            *GetNameSafe(GetOwner()),
            SlotIndex);
        return false;
    }

    if (!LoadedDataAsset->TriggerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: DataAsset %s has no TriggerClass"),
            *GetNameSafe(LoadedDataAsset));
        return false;
    }

    if (!IsDataAssetCompatibleWithSlot(SlotIndex, LoadedDataAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger pickup replacement rejected: DataAsset %s is incompatible with slot %d"),
            *GetNameSafe(LoadedDataAsset),
            SlotIndex);
        return false;
    }

    CleanupRuntimeTriggerForReplacement(SlotIndex);

    TriggerSlots[SlotIndex].DataAsset = NewDataAsset;
    TriggerSlots[SlotIndex].CachedCategory = ETriggerCategory::None;
    RuntimeTriggers[SlotIndex] = nullptr;

    InitializeLoadedSlot(SlotIndex);
    UpdateDualWieldState();

    if (SlotIndex == ActiveSubIndex && RuntimeTriggers.IsValidIndex(SlotIndex) && RuntimeTriggers[SlotIndex])
    {
        RuntimeTriggers[SlotIndex]->OnEquipped();
        OnSubTriggerEquipped.Broadcast(RuntimeTriggers[SlotIndex]);
    }

    if (SlotIndex == ActiveMainIndex || SlotIndex == ActiveSubIndex)
    {
        OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR trigger pickup replacement succeeded: owner %s slot %d DataAsset %s"),
        *GetNameSafe(GetOwner()),
        SlotIndex,
        *GetNameSafe(LoadedDataAsset));
    return true;
}

bool UWTBRTriggerSetComponent::IsDataAssetCompatibleWithSlot(int32 SlotIndex, const UWTBRTriggerDataAsset* DataAsset) const
{
    if (!IsValidSlotIndex(SlotIndex) || !IsValid(DataAsset))
    {
        return false;
    }

    switch (DataAsset->SlotConstraint)
    {
        case ETriggerSlotConstraint::MainOnly:
            return SlotIndex >= 0 && SlotIndex < MainSlotCount;

        case ETriggerSlotConstraint::SubOnly:
            return SlotIndex >= MainSlotCount && SlotIndex < TotalSlotCount;

        case ETriggerSlotConstraint::Any:
            return true;

        default:
            return false;
    }
}

void UWTBRTriggerSetComponent::CleanupRuntimeTriggerForReplacement(int32 SlotIndex)
{
    if (!HasServerAuthority() || !IsValidSlotIndex(SlotIndex) || !RuntimeTriggers.IsValidIndex(SlotIndex))
    {
        return;
    }

    if (TSharedPtr<FStreamableHandle>* ExistingHandle = PendingSlotLoads.Find(SlotIndex))
    {
        if (ExistingHandle->IsValid())
        {
            (*ExistingHandle)->CancelHandle();
        }
        PendingSlotLoads.Remove(SlotIndex);
    }

    const bool bReplacingActiveMain = SlotIndex == ActiveMainIndex;
    const bool bReplacingActiveSub = SlotIndex == ActiveSubIndex;

    if (bReplacingActiveMain && CurrentMergeState != EWTBRCompositeBulletType::None)
    {
        CancelMerge();
    }

    UWTBRTriggerBase* OldTrigger = RuntimeTriggers[SlotIndex];
    if (!IsValid(OldTrigger))
    {
        return;
    }

    if (bReplacingActiveMain || bReplacingActiveSub)
    {
        OldTrigger->OnTriggerDeactivated(GetOwner(), bReplacingActiveMain);
    }

    if (UWTBRSerpveilTrigger* SerpveilTrigger = Cast<UWTBRSerpveilTrigger>(OldTrigger))
    {
        SerpveilTrigger->CancelCharge();
    }

    if (UWTBRAegornTrigger* AegornTrigger = Cast<UWTBRAegornTrigger>(OldTrigger))
    {
        AegornTrigger->CancelShield();
    }

    if (bReplacingActiveSub)
    {
        OldTrigger->OnUnequipped();
        OnSubTriggerUnequipped.Broadcast(OldTrigger);
    }

    OldTrigger->Deactivate();
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

void UWTBRTriggerSetComponent::RequestClientSlotDataAssetLoad(int32 SlotIndex, const TCHAR* Reason)
{
    if (HasServerAuthority())
    {
        return;
    }
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }
    if (!RuntimeTriggers.IsValidIndex(SlotIndex))
    {
        RuntimeTriggers.SetNum(TotalSlotCount);
    }

    FWTBRTriggerSlot& Slot = TriggerSlots[SlotIndex];
    const AWTBRCharacter* OwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
    const TCHAR* LocalText = OwnerCharacter && OwnerCharacter->IsLocallyControlled()
        ? TEXT("true")
        : TEXT("false");
    const bool bSoftPathValid = !Slot.DataAsset.IsNull();
    const bool bDataAssetLoaded = Slot.DataAsset.IsValid();
    const bool bRuntimeTriggerValid =
        RuntimeTriggers.IsValidIndex(SlotIndex) && IsValid(RuntimeTriggers[SlotIndex]);

    if (bDataAssetLoaded)
    {
        InitializeLoadedSlot(SlotIndex);
        OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);

        const UWTBRTriggerDataAsset* DataAsset = Slot.DataAsset.Get();
        WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] SlotLoad | Owner=%s | Auth=%s | Local=%s | Slot=%d | Reason=%s | SoftPathValid=%s | DataAssetLoaded=true | AsyncRequested=false | AsyncCompleted=true | RuntimeTriggerValid=%s | Name=%s"),
            *GetNameSafe(GetOwner()),
            HasServerAuthority() ? TEXT("true") : TEXT("false"),
            LocalText,
            SlotIndex,
            Reason ? Reason : TEXT("None"),
            bSoftPathValid ? TEXT("true") : TEXT("false"),
            RuntimeTriggers.IsValidIndex(SlotIndex) && IsValid(RuntimeTriggers[SlotIndex]) ? TEXT("true") : TEXT("false"),
            DataAsset ? *DataAsset->FunctionalName.ToString() : TEXT("None"));
        return;
    }

    if (!bSoftPathValid)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] SlotLoad | Owner=%s | Auth=%s | Local=%s | Slot=%d | Reason=%s | SoftPathValid=false | DataAssetLoaded=false | AsyncRequested=false | AsyncCompleted=false | RuntimeTriggerValid=%s | Name=None"),
            *GetNameSafe(GetOwner()),
            HasServerAuthority() ? TEXT("true") : TEXT("false"),
            LocalText,
            SlotIndex,
            Reason ? Reason : TEXT("None"),
            bRuntimeTriggerValid ? TEXT("true") : TEXT("false"));
        OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
        return;
    }

    if (PendingSlotLoads.Contains(SlotIndex))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] SlotLoad | Owner=%s | Auth=%s | Local=%s | Slot=%d | Reason=%s | SoftPathValid=true | DataAssetLoaded=false | AsyncRequested=false | AsyncCompleted=false | RuntimeTriggerValid=%s | Name=Pending"),
            *GetNameSafe(GetOwner()),
            HasServerAuthority() ? TEXT("true") : TEXT("false"),
            LocalText,
            SlotIndex,
            Reason ? Reason : TEXT("None"),
            bRuntimeTriggerValid ? TEXT("true") : TEXT("false"));
        return;
    }

    FStreamableManager& StreamableMgr = UAssetManager::GetStreamableManager();
    TSharedPtr<FStreamableHandle> Handle = StreamableMgr.RequestAsyncLoad(
        Slot.DataAsset.ToSoftObjectPath(),
        [this, SlotIndex]()
        {
            if (!IsValidSlotIndex(SlotIndex))
            {
                return;
            }

            FWTBRTriggerSlot& LoadedSlot = TriggerSlots[SlotIndex];
            const bool bLoaded = LoadedSlot.DataAsset.IsValid();
            if (bLoaded)
            {
                InitializeLoadedSlot(SlotIndex);
            }

            const UWTBRTriggerDataAsset* DataAsset = LoadedSlot.DataAsset.Get();
            const bool bRuntimeTriggerValid =
                RuntimeTriggers.IsValidIndex(SlotIndex) && IsValid(RuntimeTriggers[SlotIndex]);
            const AWTBRCharacter* LoadedOwnerCharacter = Cast<AWTBRCharacter>(GetOwner());
            const TCHAR* LoadedLocalText = LoadedOwnerCharacter && LoadedOwnerCharacter->IsLocallyControlled()
                ? TEXT("true")
                : TEXT("false");

            PendingSlotLoads.Remove(SlotIndex);

            WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] SlotLoad | Owner=%s | Auth=%s | Local=%s | Slot=%d | Reason=AsyncComplete | SoftPathValid=%s | DataAssetLoaded=%s | AsyncRequested=true | AsyncCompleted=true | RuntimeTriggerValid=%s | Name=%s"),
                *GetNameSafe(GetOwner()),
                HasServerAuthority() ? TEXT("true") : TEXT("false"),
                LoadedLocalText,
                SlotIndex,
                !LoadedSlot.DataAsset.IsNull() ? TEXT("true") : TEXT("false"),
                bLoaded ? TEXT("true") : TEXT("false"),
                bRuntimeTriggerValid ? TEXT("true") : TEXT("false"),
                DataAsset ? *DataAsset->FunctionalName.ToString() : TEXT("None"));

            OnActiveTriggerChanged.Broadcast((ETriggerSlot)SlotIndex);
        });

    if (Handle.IsValid())
    {
        PendingSlotLoads.Add(SlotIndex, Handle);
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] SlotLoad | Owner=%s | Auth=%s | Local=%s | Slot=%d | Reason=%s | SoftPathValid=true | DataAssetLoaded=false | AsyncRequested=%s | AsyncCompleted=false | RuntimeTriggerValid=%s | Name=Loading"),
        *GetNameSafe(GetOwner()),
        HasServerAuthority() ? TEXT("true") : TEXT("false"),
        LocalText,
        SlotIndex,
        Reason ? Reason : TEXT("None"),
        Handle.IsValid() ? TEXT("true") : TEXT("false"),
        bRuntimeTriggerValid ? TEXT("true") : TEXT("false"));
}

void UWTBRTriggerSetComponent::InitializeLoadedSlot(int32 SlotIndex)
{
    // RuntimeTriggers is Transient (not replicated) — both server and client
    // must instantiate their own local copy. Authority guard removed intentionally.

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

bool UWTBRTriggerSetComponent::CanMutateTriggerLoadout() const
{
    if (!HasServerAuthority())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!WTBRGameState)
    {
        return false;
    }

    if (WTBRGameState->IsLoadoutSetupAllowedPhase())
    {
        return true;
    }

    if (WTBRGameState->IsInMatch())
    {
        return WTBRGameState->AllowsTriggerSwapDuringMatch();
    }

    return false;
}

void UWTBRTriggerSetComponent::DebugPrintTriggerLoadoutMutationGate() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("DebugPrintTriggerLoadoutMutationGate is disabled in Shipping builds."));
#else
    const bool bFinalDecision = CanMutateTriggerLoadout();

    const TCHAR* Reason = TEXT("BlockedByPhase");
    EWTBRMatchMode MatchMode = EWTBRMatchMode::None;
    EWTBRMatchPhase MatchPhase = EWTBRMatchPhase::None;
    bool bLoadoutSetupAllowedPhase = false;
    bool bInMatch = false;
    bool bAllowsTriggerSwapDuringMatch = false;

    if (!HasServerAuthority())
    {
        Reason = TEXT("NoAuthority");
    }
    else if (const UWorld* World = GetWorld())
    {
        if (const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>())
        {
            MatchMode = WTBRGameState->GetCurrentMatchMode();
            MatchPhase = WTBRGameState->GetCurrentMatchPhase();
            bLoadoutSetupAllowedPhase = WTBRGameState->IsLoadoutSetupAllowedPhase();
            bInMatch = WTBRGameState->IsInMatch();
            bAllowsTriggerSwapDuringMatch = WTBRGameState->AllowsTriggerSwapDuringMatch();

            if (bLoadoutSetupAllowedPhase)
            {
                Reason = TEXT("LoadoutSetupAllowedPhase");
            }
            else if (bInMatch && bAllowsTriggerSwapDuringMatch)
            {
                Reason = TEXT("InMatchRuleAllowsTriggerSwap");
            }
            else if (bInMatch)
            {
                Reason = TEXT("InMatchRuleBlocksTriggerSwap");
            }
        }
        else
        {
            Reason = TEXT("MissingGameState");
        }
    }
    else
    {
        Reason = TEXT("MissingWorld");
    }

    UE_LOG(LogTemp, Log,
        TEXT("WTBRDebugPrintTriggerLoadoutGate: Owner=%s Mode=%s Phase=%s IsLoadoutSetupAllowedPhase=%s IsInMatch=%s AllowsTriggerSwapDuringMatch=%s Decision=%s Reason=%s"),
        *GetNameSafe(GetOwner()),
        *UEnum::GetValueAsString(MatchMode),
        *UEnum::GetValueAsString(MatchPhase),
        bLoadoutSetupAllowedPhase ? TEXT("true") : TEXT("false"),
        bInMatch ? TEXT("true") : TEXT("false"),
        bAllowsTriggerSwapDuringMatch ? TEXT("true") : TEXT("false"),
        bFinalDecision ? TEXT("ALLOW") : TEXT("BLOCK"),
        Reason);
#endif
}

void UWTBRTriggerSetComponent::Server_SetTriggerLoadout_Implementation(
    const TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>>& InLoadout)
{
    if (!CanMutateTriggerLoadout())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR trigger loadout mutation rejected by match phase/rules for owner %s"),
            *GetNameSafe(GetOwner()));
        return;
    }

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

void UWTBRTriggerSetComponent::NotifySubSlotChanged(int32 OldAbsIdx, int32 NewAbsIdx)
{
    if (RuntimeTriggers.IsValidIndex(OldAbsIdx) && RuntimeTriggers[OldAbsIdx])
    {
        RuntimeTriggers[OldAbsIdx]->OnUnequipped();
        OnSubTriggerUnequipped.Broadcast(RuntimeTriggers[OldAbsIdx]);
    }
    if (RuntimeTriggers.IsValidIndex(NewAbsIdx) && RuntimeTriggers[NewAbsIdx])
    {
        RuntimeTriggers[NewAbsIdx]->OnEquipped();
        OnSubTriggerEquipped.Broadcast(RuntimeTriggers[NewAbsIdx]);
    }
}

void UWTBRTriggerSetComponent::Server_FireSerpveil_Implementation(
    EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange, bool bIsMain)
{
    if (!HasServerAuthority()) return;

    UWTBRTriggerBase* Trigger = bIsMain ? GetActiveMainTrigger() : GetActiveSubTrigger();
    if (!IsValid(Trigger)) return;

    UWTBRSerpveilTrigger* SerpveilTrigger = Cast<UWTBRSerpveilTrigger>(Trigger);
    if (!IsValid(SerpveilTrigger)) return;

    SerpveilTrigger->ExecuteServerFire(Shape, Direction, ChargedRange);
}

void UWTBRTriggerSetComponent::Server_TEMP_TEST46_PlaceAegornWall_Implementation(bool bIsMain)
{
#if UE_BUILD_SHIPPING
    return; // Debug-only RPC — no-op in Shipping builds.
#endif
    if (!WTBRShouldLogValidation()) return;
    if (!HasServerAuthority()) return;

    UWTBRTriggerBase* Trigger = bIsMain ? GetActiveMainTrigger() : GetActiveSubTrigger();
    UWTBRAegornTrigger* AegornTrigger = Cast<UWTBRAegornTrigger>(Trigger);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] TEMP_TEST46 Request | Owner=%s | Main=%s | Trigger=%s | IsAegorn=%s"),
        *GetNameSafe(GetOwner()),
        bIsMain ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Trigger),
        IsValid(AegornTrigger) ? TEXT("true") : TEXT("false"));

    if (!IsValid(AegornTrigger)) return;
    AegornTrigger->PlaceWall();
}

void UWTBRTriggerSetComponent::OnRep_TriggerSlots()
{
    // Guard: ensure RuntimeTriggers array is sized before use.
    // OnRep_ may fire before BeginPlay on client — without this,
    // IsValidIndex() returns false and every slot is skipped.
    if (RuntimeTriggers.Num() < TotalSlotCount)
    {
        RuntimeTriggers.SetNum(TotalSlotCount);
    }

    // Instantiate any slot whose DataAsset just arrived from server
    // and has not been instantiated yet (RuntimeTrigger still null).
    for (int32 i = 0; i < TotalSlotCount; ++i)
    {
        if (!TriggerSlots[i].IsEmpty() && RuntimeTriggers.IsValidIndex(i) && !RuntimeTriggers[i])
        {
            if (TriggerSlots[i].DataAsset.IsValid())
            {
                InitializeLoadedSlot(i);
            }
            else
            {
                RequestClientSlotDataAssetLoad(i, TEXT("OnRep_TriggerSlots"));
            }
        }
    }

    RequestClientSlotDataAssetLoad(ActiveMainIndex, TEXT("OnRep_TriggerSlotsActiveMain"));
    RequestClientSlotDataAssetLoad(ActiveSubIndex, TEXT("OnRep_TriggerSlotsActiveSub"));
    OnActiveTriggerChanged.Broadcast((ETriggerSlot)ActiveMainIndex);
    OnActiveTriggerChanged.Broadcast((ETriggerSlot)ActiveSubIndex);
}

void UWTBRTriggerSetComponent::OnRep_ActiveMainIndex()
{
    RequestClientSlotDataAssetLoad(ActiveMainIndex, TEXT("OnRep_ActiveMainIndex"));
    OnActiveTriggerChanged.Broadcast((ETriggerSlot)ActiveMainIndex);
}

void UWTBRTriggerSetComponent::OnRep_ActiveSubIndex()
{
    RequestClientSlotDataAssetLoad(ActiveSubIndex, TEXT("OnRep_ActiveSubIndex"));
    OnActiveTriggerChanged.Broadcast((ETriggerSlot)ActiveSubIndex);
}

void UWTBRTriggerSetComponent::OnRep_DualWieldState()
{
    // Client-side: react for animation/UI
}

// ── Composite Bullet Merge ────────────────────────────────────────────────────

void UWTBRTriggerSetComponent::Server_StartCompositeMerge_Implementation(
    EWTBRCompositeBulletType Type)
{
    if (!HasServerAuthority()) return;
    if (Type == EWTBRCompositeBulletType::None) return;
    if (CurrentMergeState != EWTBRCompositeBulletType::None) return; // already merging

    if (!IsValidSlotIndex(ActiveMainIndex)) return;
    UWTBRTriggerDataAsset* DA = TriggerSlots[ActiveMainIndex].DataAsset.Get();
    if (!DA) return;

    const float* VaelCostPtr  = DA->CompositeVaelCosts.Find(Type);
    const float* MergeTimePtr = DA->CompositeMergeTimes.Find(Type);
    if (!VaelCostPtr || !MergeTimePtr) return;

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->VaelComponent) return;
    if (!OwnerChar->VaelComponent->TryConsumeVael(*VaelCostPtr)) return;

    CurrentMergeState = Type;
    GetWorld()->GetTimerManager().SetTimer(
        MergeTimer,
        this, &UWTBRTriggerSetComponent::OnMergeCompleteCallback,
        *MergeTimePtr,
        false);
}

void UWTBRTriggerSetComponent::CancelMerge()
{
    if (!HasServerAuthority()) return;
    if (CurrentMergeState == EWTBRCompositeBulletType::None) return;

    GetWorld()->GetTimerManager().ClearTimer(MergeTimer);
    bMergeWasFired    = false;
    CurrentMergeState = EWTBRCompositeBulletType::None;
}

void UWTBRTriggerSetComponent::OnMergeCompleteCallback()
{
    if (!HasServerAuthority()) return;

    const EWTBRCompositeBulletType TypeToFire = CurrentMergeState;
    bMergeWasFired    = true;
    CurrentMergeState = EWTBRCompositeBulletType::None;
    FireComposite(TypeToFire);
}

void UWTBRTriggerSetComponent::FireComposite(EWTBRCompositeBulletType Type)
{
    if (!HasServerAuthority()) return;

    if (!IsValidSlotIndex(ActiveMainIndex)) return;
    UWTBRTriggerDataAsset* DA = TriggerSlots[ActiveMainIndex].DataAsset.Get();
    if (!DA) return;

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (!OwnerChar || !GetWorld()) return;

    // ── Labyrn: Serpveil + Serpveil — dual mirrored path ─────────────────────
    if (Type == EWTBRCompositeBulletType::Labyrn)
    {
        const TSubclassOf<AWTBRProjectileBase>* ClassPtr  = DA->CompositeProjectileClasses.Find(Type);
        const float*                            DamagePtr = DA->CompositeDamages.Find(Type);
        if (!ClassPtr || !(*ClassPtr) || !DamagePtr) return;

        FVector  EyeLoc;
        FRotator EyeRot;
        OwnerChar->GetActorEyesViewPoint(EyeLoc, EyeRot);
        const FVector    SpawnLoc = EyeLoc + EyeRot.Vector() * 100.f;
        const FTransform SpawnTF(EyeRot, SpawnLoc);

        const FWTBRSerpveilParams& SParams = DA->SerpveilParams;
        const float Speed = SParams.SerpveilSpeed;

        TArray<FVector> PathA = UWTBRSerpveilTrigger::BuildPathPoints(
            SParams.PresetShape, SpawnLoc, EyeRot, SParams.SerpveilMaxRange, false);
        TArray<FVector> PathB = UWTBRSerpveilTrigger::BuildPathPoints(
            SParams.PresetShape, SpawnLoc, EyeRot, SParams.SerpveilMaxRange, true);

        if (PathA.Num() < 2 || PathB.Num() < 2) return;

        // Core A — normal path
        AWTBRProjectileBase* ProjA = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
            *ClassPtr, SpawnTF, OwnerChar, OwnerChar,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (ProjA)
        {
            ProjA->InitializeProjectile(*DamagePtr, Speed, ETriggerCategory::Gunner,
                /*bSniper=*/false, /*bExplode=*/false, /*ExplodeRadius=*/0.f);
            ProjA->FinishSpawning(SpawnTF);
            ProjA->InitializePathMovement(PathA, Speed, OwnerChar);
            ProjA->SpawnCubeSplits();
        }

        // Core B — mirrored path; same OwnerInstigator prevents self-clash
        AWTBRProjectileBase* ProjB = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
            *ClassPtr, SpawnTF, OwnerChar, OwnerChar,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (ProjB)
        {
            ProjB->InitializeProjectile(*DamagePtr, Speed, ETriggerCategory::Gunner,
                /*bSniper=*/false, /*bExplode=*/false, /*ExplodeRadius=*/0.f);
            ProjB->FinishSpawning(SpawnTF);
            ProjB->InitializePathMovement(PathB, Speed, OwnerChar);
            ProjB->SpawnCubeSplits();
        }

        return;
    }

    // ── Generic composite fire ────────────────────────────────────────────────
    const TSubclassOf<AWTBRProjectileBase>* ClassPtr  = DA->CompositeProjectileClasses.Find(Type);
    const float*                            DamagePtr = DA->CompositeDamages.Find(Type);
    if (!ClassPtr || !(*ClassPtr) || !DamagePtr) return;

    const FVector  SpawnLocation = OwnerChar->GetActorLocation()
                                    + OwnerChar->GetActorForwardVector() * 100.f;
    const FRotator SpawnRotation = OwnerChar->GetActorRotation();

    AWTBRProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
        *ClassPtr,
        FTransform(SpawnRotation, SpawnLocation),
        OwnerChar,
        OwnerChar,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Projectile) return;

    Projectile->InitializeProjectile(
        *DamagePtr,
        /*InSpeed=*/3000.f,
        ETriggerCategory::Gunner,
        /*bSniper=*/false,
        /*bExplode=*/false,
        /*ExplodeRadius=*/0.f);

    Projectile->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));
    Projectile->Launch(OwnerChar->GetActorForwardVector(), OwnerChar);
}

void UWTBRTriggerSetComponent::OnRep_MergeState(EWTBRCompositeBulletType OldState)
{
    if (CurrentMergeState != EWTBRCompositeBulletType::None)
    {
        OnMergeStart(CurrentMergeState);
    }
    else if (OldState != EWTBRCompositeBulletType::None && bMergeWasFired)
    {
        OnMergeComplete(OldState);
    }
    else
    {
        OnMergeCancelled();
    }
}

bool UWTBRTriggerSetComponent::IsDataAssetCompatibleWithTargetSlot(int32 SlotIndex, const UWTBRTriggerDataAsset* DataAsset) const
{
    return IsDataAssetCompatibleWithSlot(SlotIndex, DataAsset);
}

#if WITH_DEV_AUTOMATION_TESTS
void UWTBRTriggerSetComponent::SetSlotDataAssetForTest(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> InDataAsset)
{
    if (TriggerSlots.IsValidIndex(SlotIndex))
    {
        TriggerSlots[SlotIndex].DataAsset = InDataAsset;
        TriggerSlots[SlotIndex].CachedCategory = ETriggerCategory::None;
    }
}
#endif

void UWTBRTriggerSetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRTriggerSetComponent, TriggerSlots);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveMainIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveSubIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CurrentDualWieldState);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CurrentMergeState);
    DOREPLIFETIME(UWTBRTriggerSetComponent, bMergeWasFired);
}
