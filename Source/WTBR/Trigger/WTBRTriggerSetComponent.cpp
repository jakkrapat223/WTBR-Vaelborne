// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRValidationLog.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "Trigger/WTBREscudoTrigger.h"
#include "Trigger/WTBRSerpveilTrigger.h"
#include "Trigger/WTBRFeryxTrigger.h"
#include "Trigger/WTBRMantornTrigger.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"
#include "Trigger/WTBRLabyrnCompositeBehavior.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Engine/Engine.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Subsystem/WTBRCustomPresetSubsystem.h"

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
    CancelMerge();
    DiscardReadyComposite();

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

    if (CurrentMergeState != EWTBRCompositeBulletType::None)
    {
        CancelMerge();
    }
    DiscardReadyComposite();

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

    if (CurrentMergeState != EWTBRCompositeBulletType::None)
    {
        CancelMerge();
    }
    DiscardReadyComposite();

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

FText UWTBRTriggerSetComponent::GetSlotDisplayName(int32 SlotIndex) const
{
    if (!TriggerSlots.IsValidIndex(SlotIndex))
    {
        return FText::GetEmpty();
    }

    const TSoftObjectPtr<UWTBRTriggerDataAsset>& SlotAsset = TriggerSlots[SlotIndex].DataAsset;
    if (SlotAsset.IsNull())
    {
        return FText::GetEmpty();
    }

    if (const UWTBRTriggerDataAsset* Loaded = SlotAsset.Get())
    {
        return Loaded->FunctionalName;
    }

    // Deliberately not forcing a synchronous load here — this is called from UI
    // paint, and the slot's real name arrives once the existing async load lands.
    return FText::FromString(SlotAsset.ToSoftObjectPath().GetAssetName());
}

bool UWTBRTriggerSetComponent::IsSlotOccupied(int32 SlotIndex) const
{
    return TriggerSlots.IsValidIndex(SlotIndex) && !TriggerSlots[SlotIndex].DataAsset.IsNull();
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

// ── Trigger Option (canon: Senkū-style attachment) ────────────────────────────

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetOrCreateOptionRuntimeTrigger(int32 SlotIndex)
{
    if (!IsValidSlotIndex(SlotIndex)) return nullptr;

    if (TObjectPtr<UWTBRTriggerBase>* Cached = RuntimeOptionTriggers.Find(SlotIndex))
    {
        if (IsValid(*Cached)) return *Cached;
        RuntimeOptionTriggers.Remove(SlotIndex);
    }

    UWTBRTriggerDataAsset* OptionDA = TriggerSlots[SlotIndex].OptionDataAsset.Get();
    if (!OptionDA)
    {
        // Not yet loaded — try a synchronous load. Options are EditDefaultsOnly/
        // design-time authored, so this should already be resident in practice.
        OptionDA = TriggerSlots[SlotIndex].OptionDataAsset.LoadSynchronous();
    }
    if (!OptionDA || !OptionDA->TriggerClass) return nullptr;

    UWTBRTriggerBase* NewOptionTrigger = NewObject<UWTBRTriggerBase>(this, OptionDA->TriggerClass);
    if (!NewOptionTrigger) return nullptr;

    NewOptionTrigger->InitializeTrigger(Cast<AWTBRCharacter>(GetOwner()), OptionDA);
    RuntimeOptionTriggers.Add(SlotIndex, NewOptionTrigger);
    return NewOptionTrigger;
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveMainOptionTrigger()
{
    return GetOrCreateOptionRuntimeTrigger(ActiveMainIndex);
}

UWTBRTriggerBase* UWTBRTriggerSetComponent::GetActiveSubOptionTrigger()
{
    return GetOrCreateOptionRuntimeTrigger(ActiveSubIndex);
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
    DiscardReadyComposite();

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

    // Auto-exit Mantorn form the moment the active loadout is no longer a pair of
    // Mantorn-capable Feryx (e.g. player cycled a slot away from Feryx). Server only.
    if (bMantornFormActive && HasServerAuthority() && !AreBothActiveFeryxMantornCapable())
    {
        ExitMantornForm(TEXT("ActiveTriggerChanged"));
    }
}

// ── Mantorn (Feryx + Feryx composite form) ───────────────────────────────────

bool UWTBRTriggerSetComponent::AreBothActiveFeryxMantornCapable() const
{
    const UWTBRFeryxTrigger* Main = Cast<UWTBRFeryxTrigger>(GetActiveMainTrigger());
    const UWTBRFeryxTrigger* Sub  = Cast<UWTBRFeryxTrigger>(GetActiveSubTrigger());
    if (!Main || !Sub) return false;

    const UWTBRTriggerDataAsset* MainDA = Main->DataAsset;
    const UWTBRTriggerDataAsset* SubDA  = Sub->DataAsset;
    return IsValid(MainDA) && IsValid(SubDA)
        && MainDA->MantornParams.bCanFormMantorn
        && SubDA->MantornParams.bCanFormMantorn
        && !Main->IsSpent() && !Sub->IsSpent();
}

void UWTBRTriggerSetComponent::Server_RequestMantornToggle_Implementation()
{
    if (!HasServerAuthority()) return;

    if (bMantornFormActive)
    {
        ExitMantornForm(TEXT("PlayerToggle"));
    }
    else
    {
        EnterMantornForm();
    }
}

bool UWTBRTriggerSetComponent::EnterMantornForm()
{
    if (!HasServerAuthority()) return false;
    if (bMantornFormActive) return false;
    if (!AreBothActiveFeryxMantornCapable()) return false;

    UWTBRTriggerDataAsset* MainDA = GetActiveMainDataAsset();
    if (!IsValid(MainDA)) return false;

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->VaelComponent) return false;

    // Separate one-time transform cost, like Dual Senku. No refund on exit.
    if (!OwnerChar->VaelComponent->TryConsumeVael(MainDA->MantornParams.TransformVaelCost))
    {
        return false;
    }

    ActiveMantornForm = NewObject<UWTBRMantornTrigger>(this);
    ActiveMantornForm->InitializeTrigger(OwnerChar, MainDA);

    bMantornFormActive = true;
    // OnRep does not fire on the server (listen host authority) — call the VFX hook directly.
    OnMantornFormEntered();

    // Visible-now confirmation until real VFX/animation is bound to OnMantornFormEntered
    // in Blueprint. Same on-screen debug convention as UWTBRMeleeTrigger's Feryx logging.
    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Purple,
            FString::Printf(TEXT("[Mantorn] FORM ENTERED — %s"), *GetNameSafe(OwnerChar)));
    }
    UE_LOG(LogTemp, Log, TEXT("WTBR Mantorn form entered: %s"), *GetNameSafe(OwnerChar));
    return true;
}

void UWTBRTriggerSetComponent::ExitMantornForm(const TCHAR* Reason)
{
    if (!HasServerAuthority()) return;
    if (!bMantornFormActive) return;

    UE_LOG(LogTemp, Log, TEXT("WTBR Mantorn form exit: %s | Owner=%s"),
        Reason ? Reason : TEXT("None"), *GetNameSafe(GetOwner()));

    bMantornFormActive = false;
    ActiveMantornForm  = nullptr;
    OnMantornFormExited();

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Silver,
            FString::Printf(TEXT("[Mantorn] Form exited (%s)"), Reason ? Reason : TEXT("None")));
    }
}

void UWTBRTriggerSetComponent::OnRep_MantornForm()
{
    // Client cosmetic hook only. Gameplay executes on the server.
    if (bMantornFormActive)
    {
        OnMantornFormEntered();
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Purple, TEXT("[Mantorn] FORM ENTERED (client)"));
        }
    }
    else
    {
        OnMantornFormExited();
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Silver, TEXT("[Mantorn] Form exited (client)"));
        }
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

    // A freshly-instantiated Venyx Trigger starts with an empty combined-preset
    // cache; if this player's custom presets were already known (an earlier-equipped
    // slot already triggered RefreshCustomVenyxPresetsFromLocalSubsystem this
    // session), apply them now instead of waiting for the next upload.
    if (UWTBRVenyxTrigger* VenyxTrigger = Cast<UWTBRVenyxTrigger>(NewTrigger))
    {
        VenyxTrigger->RefreshCustomHoldPresets(CachedCustomVenyxPresets);
    }
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

    // Same reasoning as InitializeLoadedSlot above: a mid-session slot switch INTO
    // Venyx should not have to wait for a fresh upload to see presets this player
    // already authored earlier in the session.
    if (UWTBRVenyxTrigger* VenyxTrigger = Cast<UWTBRVenyxTrigger>(NewTrigger))
    {
        VenyxTrigger->RefreshCustomHoldPresets(CachedCustomVenyxPresets);
    }
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

namespace
{
    // FMath::Clamp does NOT sanitize NaN: it is implemented as a pair of
    // comparisons, and every comparison against NaN is false, so a NaN passes
    // straight through every ClampMin/ClampMax in this file. A NaN reaching
    // ResolvePathPreset propagates into an InterpToMovement control point and from
    // there into an actor transform, which is undefined behaviour rather than a
    // merely wrong shot. So non-finite values are REPLACED with a safe default
    // before any clamping happens.
    float WTBRSanitizeFloat(float Value, float Fallback, float Min, float Max)
    {
        if (!FMath::IsFinite(Value))
        {
            return Fallback;
        }
        return FMath::Clamp(Value, Min, Max);
    }
}

bool UWTBRTriggerSetComponent::SanitizeCustomVenyxPreset(FWTBRPathPreset& Preset)
{
    // Anti-abuse ceilings, not balance numbers — a modified client could otherwise
    // upload an unbounded number of lanes/events per preset. The lane ceiling is
    // shared with the editor so its "add lane" button cannot build lanes that this
    // function would then silently truncate away.
    static constexpr int32 MaxLanesPerPreset = WTBR_MAX_CUSTOM_LANES;
    static constexpr int32 MaxEventsPerLane = 8;

    // Waypoints are fractions of the committed range, so anything authorable in the
    // editor sits comfortably inside +/-2. This is a sanity bound on a finite but
    // absurd value (1e30 is finite and would still resolve to a nonsense world
    // position), not a design constraint on how far a lane may reach.
    static constexpr float MaxWaypointComponent = 4.0f;

    if (Preset.PresetId.IsNone())
    {
        return false;
    }

    if (Preset.Lanes.Num() > MaxLanesPerPreset)
    {
        Preset.Lanes.SetNum(MaxLanesPerPreset);
    }
    Preset.CubeCount = FMath::Max(0, Preset.CubeCount);

    for (int32 LaneIndex = Preset.Lanes.Num() - 1; LaneIndex >= 0; --LaneIndex)
    {
        FWTBRPathLane& Lane = Preset.Lanes[LaneIndex];

        // A lane carrying a non-finite waypoint is DROPPED rather than repaired.
        // Unlike a scalar, there is no defensible "safe default" for a position the
        // player supposedly drew — silently substituting one would fire a shot that
        // does not match anything they authored.
        bool bLaneHasNonFiniteWaypoint = false;
        for (const FVector& Waypoint : Lane.NormalizedWaypoints)
        {
            if (Waypoint.ContainsNaN())
            {
                bLaneHasNonFiniteWaypoint = true;
                break;
            }
        }

        // ResolvePathPreset itself only requires >= 2 waypoints; that waypoint 0 is
        // the caster origin is a project-wide authoring convention every existing
        // (C++-authored, trusted) preset follows but nothing has ever validated.
        // This RPC is the first place an untrusted waypoint array can reach it, so
        // a lane that doesn't start at the caster is dropped rather than silently
        // re-based — there is no safe way to guess what the player meant.
        const bool bStartsAtCaster =
            Lane.NormalizedWaypoints.Num() >= 2 &&
            Lane.NormalizedWaypoints[0].Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER);

        if (bLaneHasNonFiniteWaypoint || !bStartsAtCaster)
        {
            Preset.Lanes.RemoveAt(LaneIndex);
            continue;
        }

        // Reuses the same truncation the composite turn-budget rule already relies
        // on (keeps the first N turns, always keeps the final waypoint) rather than
        // re-deriving it — see UWTBRCompositeRegistryDataAsset::ClampLaneTurns.
        TArray<FVector> Clamped;
        UWTBRCompositeRegistryDataAsset::ClampLaneTurns(
            Lane.NormalizedWaypoints, WTBR_MAX_CUSTOM_LANE_WAYPOINTS - 2, Clamped);
        Lane.NormalizedWaypoints = Clamped;

        for (FVector& Waypoint : Lane.NormalizedWaypoints)
        {
            Waypoint.X = FMath::Clamp(Waypoint.X, -MaxWaypointComponent, MaxWaypointComponent);
            Waypoint.Y = FMath::Clamp(Waypoint.Y, -MaxWaypointComponent, MaxWaypointComponent);
            Waypoint.Z = FMath::Clamp(Waypoint.Z, -MaxWaypointComponent, MaxWaypointComponent);
        }

        // Every remaining field's UPROPERTY meta=(ClampMin/ClampMax) is a
        // details-panel constraint only — it enforces nothing on data that arrives
        // over an RPC, so each range is re-applied here explicitly, through the
        // NaN-safe helper above.
        Lane.CubeCount = FMath::Max(1, Lane.CubeCount);
        Lane.LaunchDelay = WTBRSanitizeFloat(Lane.LaunchDelay, 0.0f, 0.0f, 5.0f);
        Lane.HomingRadius = WTBRSanitizeFloat(Lane.HomingRadius, 0.0f, 0.0f, 1.0f);
        Lane.HomingRadiusFloorUU = WTBRSanitizeFloat(Lane.HomingRadiusFloorUU, 400.0f, 0.0f, 100000.0f);

        if (Lane.Events.Num() > MaxEventsPerLane)
        {
            Lane.Events.SetNum(MaxEventsPerLane);
        }
        for (FWTBRLaneEvent& Event : Lane.Events)
        {
            Event.AtPathFraction = WTBRSanitizeFloat(Event.AtPathFraction, 0.5f, 0.0f, 1.0f);
            Event.DurationSeconds = WTBRSanitizeFloat(Event.DurationSeconds, 0.0f, 0.0f, 5.0f);
        }
    }

    return Preset.Lanes.Num() > 0;
}

void UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetList(
    const TArray<FWTBRPathPreset>& InPresets, TArray<FWTBRPathPreset>& OutPresets)
{
    // Anti-abuse ceiling on preset COUNT, alongside the per-preset ceilings in
    // SanitizeCustomVenyxPreset.
    static constexpr int32 MaxCustomPresets = 12;

    OutPresets.Reset();
    for (const FWTBRPathPreset& Incoming : InPresets)
    {
        if (OutPresets.Num() >= MaxCustomPresets) break;

        FWTBRPathPreset Sanitized = Incoming;
        if (SanitizeCustomVenyxPreset(Sanitized))
        {
            OutPresets.Add(MoveTemp(Sanitized));
        }
    }
}

void UWTBRTriggerSetComponent::RefreshAllVenyxHoldPresetCaches()
{
    for (UWTBRTriggerBase* Trigger : RuntimeTriggers)
    {
        if (UWTBRVenyxTrigger* VenyxTrigger = Cast<UWTBRVenyxTrigger>(Trigger))
        {
            VenyxTrigger->RefreshCustomHoldPresets(CachedCustomVenyxPresets);
        }
    }
}

void UWTBRTriggerSetComponent::Server_SetCustomVenyxPresets_Implementation(
    const TArray<FWTBRPathPreset>& InCustomPresets)
{
    if (!HasServerAuthority()) return;

    SanitizeCustomVenyxPresetList(InCustomPresets, CachedCustomVenyxPresets);
    RefreshAllVenyxHoldPresetCaches();
}

void UWTBRTriggerSetComponent::RefreshCustomVenyxPresetsFromLocalSubsystem()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;
    UGameInstance* GameInstance = Owner->GetGameInstance();
    if (!GameInstance) return;
    UWTBRCustomPresetSubsystem* Subsystem = GameInstance->GetSubsystem<UWTBRCustomPresetSubsystem>();
    if (!Subsystem) return;

    // ⚠ The local copy is sanitized with the SAME function the server applies on
    // receipt, and this is load-bearing rather than defensive tidiness.
    //
    // The fire path sends only an int32 index; the server resolves it against its
    // own array. Sanitizing DROPS presets and lanes that fail validation. If the
    // client displayed its raw list while the server held a filtered one, the two
    // arrays would differ in LENGTH and therefore in ORDER — the wheel would offer
    // an entry at index N that resolves to a different preset server-side, or to
    // none. That is a wrong-shot-fired bug, not a cosmetic one.
    //
    // Running the identical sanitizer on both sides makes the two arrays equal by
    // construction, so no confirm-back round trip is needed to keep indices aligned.
    TArray<FWTBRPathPreset> Sanitized;
    SanitizeCustomVenyxPresetList(Subsystem->GetCustomVenyxPresets(), Sanitized);

    // Local copy first — this is the same process, so there's no reason to wait on
    // a round trip before this player's own Venyx wheel can see their presets.
    CachedCustomVenyxPresets = Sanitized;
    RefreshAllVenyxHoldPresetCaches();

    // Then upload so a remote listen-server/dedicated-server also has a validated
    // copy before it can be asked to fire an index into it. Harmless when this side
    // already IS the server (a listen-server's own local player) — the RPC just
    // re-validates (idempotent: sanitizing an already-sanitized list is a no-op)
    // and re-applies the same data.
    Server_SetCustomVenyxPresets(Sanitized);
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

    // Escudo (wall placement) is now a real, independently-equippable Trigger
    // reachable via the normal Activate() input path — this RPC is legacy
    // PIE/BP-debug scaffolding kept only in case an existing debug widget
    // still calls it.
    UWTBRTriggerBase* Trigger = bIsMain ? GetActiveMainTrigger() : GetActiveSubTrigger();
    UWTBREscudoTrigger* EscudoTrigger = Cast<UWTBREscudoTrigger>(Trigger);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test46 AegornWall] TEMP_TEST46 Request | Owner=%s | Main=%s | Trigger=%s | IsEscudo=%s"),
        *GetNameSafe(GetOwner()),
        bIsMain ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Trigger),
        IsValid(EscudoTrigger) ? TEXT("true") : TEXT("false"));

    if (!IsValid(EscudoTrigger)) return;
    EscudoTrigger->Activate(FInputActionValue(), false);
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

void UWTBRTriggerSetComponent::Server_StartCompositeMerge_Implementation()
{
    if (!HasServerAuthority()) return;
    if (CurrentMergeState != EWTBRCompositeBulletType::None)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT already-merging Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }
    if (GetWorld()->GetTimerManager().IsTimerActive(CompositeCooldownTimer))
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT on-cooldown Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }
    if (ReadyCompositeType != EWTBRCompositeBulletType::None)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT already-ready Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    if (!IsValidSlotIndex(ActiveMainIndex) || !IsValidSlotIndex(ActiveSubIndex))
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT invalid-slots Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }
    UWTBRTriggerDataAsset* MainDA = TriggerSlots[ActiveMainIndex].DataAsset.Get();
    UWTBRTriggerDataAsset* SubDA  = TriggerSlots[ActiveSubIndex].DataAsset.Get();
    if (!MainDA || !SubDA)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT invalid-slots Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous();
    if (!Registry)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT no-registry Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    const EWTBRCompositeBulletType Type =
        Registry->ResolveCompositeType(MainDA->BulletArchetype, SubDA->BulletArchetype);
    if (Type == EWTBRCompositeBulletType::None)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT no-resolve Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    // Validate the WHOLE definition BEFORE touching Vael — an unauthored/misconfigured
    // definition must never cost the player anything.
    FWTBRCompositeDefinition Definition;
    if (!Registry->FindDefinition(Type, Definition))
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT invalid-definition Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }
    if (!Definition.ProjectileClass)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT invalid-definition Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }
    if (Definition.MergeTime <= 0.0f)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT invalid-definition Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (!OwnerChar || !OwnerChar->VaelComponent)
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT insufficient-Vael Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    FGuid ReservationHandle;
    if (!OwnerChar->VaelComponent->TryReserveVael(Definition.VaelCost, ReservationHandle))
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
                FString::Printf(TEXT("[CompositeMerge] REJECT insufficient-Vael Owner=%s"), *GetNameSafe(GetOwner())));
        }
        return;
    }

    ActiveMergeSnapshot.CompositeType        = Type;
    ActiveMergeSnapshot.MainSlotIndex        = ActiveMainIndex;
    ActiveMergeSnapshot.SubSlotIndex         = ActiveSubIndex;
    ActiveMergeSnapshot.MainArchetype        = MainDA->BulletArchetype;
    ActiveMergeSnapshot.SubArchetype         = SubDA->BulletArchetype;
    ActiveMergeSnapshot.ReservationHandle    = ReservationHandle;
    ActiveMergeSnapshot.MergeStartServerTime = GetWorld()->GetTimeSeconds();

    CurrentMergeState = Type;
    CompositeMergeDuration = Definition.MergeTime;
    if (OwnerChar->MovementExtComponent)
    {
        OwnerChar->MovementExtComponent->SetDebuffPenalty(1.0f); // Root: translation only, camera/aim free
    }
    GetWorld()->GetTimerManager().SetTimer(
        MergeTimer,
        this, &UWTBRTriggerSetComponent::OnMergeCompleteCallback,
        Definition.MergeTime,
        false);

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
            FString::Printf(TEXT("[CompositeMerge] START Type=%s Duration=%.2f Owner=%s"),
                *UEnum::GetValueAsString(Type), Definition.MergeTime, *GetNameSafe(GetOwner())));
    }
}

void UWTBRTriggerSetComponent::CancelMerge()
{
    if (!HasServerAuthority()) return;
    if (CurrentMergeState == EWTBRCompositeBulletType::None) return;

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("[CompositeMerge] CANCEL Owner=%s"), *GetNameSafe(GetOwner())));
    }

    GetWorld()->GetTimerManager().ClearTimer(MergeTimer);

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (OwnerChar && OwnerChar->MovementExtComponent)
    {
        OwnerChar->MovementExtComponent->SetDebuffPenalty(0.0f); // Unroot on every exit path
    }
    if (OwnerChar && OwnerChar->VaelComponent)
    {
        OwnerChar->VaelComponent->ReleaseReservation(ActiveMergeSnapshot.ReservationHandle);
    }
    ActiveMergeSnapshot = FWTBRCompositeMergeSnapshot();

    bMergeWasFired    = false;
    CurrentMergeState = EWTBRCompositeBulletType::None;
}

void UWTBRTriggerSetComponent::OnMergeCompleteCallback()
{
    if (!HasServerAuthority()) return;

    const FWTBRCompositeMergeSnapshot Snapshot = ActiveMergeSnapshot;
    // Existing VFX signal now means the merge finished and the bullet is ready.
    bMergeWasFired    = true;
    CurrentMergeState = EWTBRCompositeBulletType::None;
    ActiveMergeSnapshot = FWTBRCompositeMergeSnapshot();

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (OwnerChar && OwnerChar->MovementExtComponent)
    {
        OwnerChar->MovementExtComponent->SetDebuffPenalty(0.0f); // Unroot unconditionally — even if the fire below refunds or fails
    }

    if (!OwnerChar || !OwnerChar->VaelComponent) return;

    const auto SlotStillMatches = [this](int32 SlotIndex, EWTBRBulletArchetype ExpectedArchetype)
    {
        if (!IsValidSlotIndex(SlotIndex)) return false;
        const UWTBRTriggerDataAsset* DataAsset = TriggerSlots[SlotIndex].DataAsset.Get();
        return DataAsset && DataAsset->BulletArchetype == ExpectedArchetype;
    };

    if (!SlotStillMatches(Snapshot.MainSlotIndex, Snapshot.MainArchetype) ||
        !SlotStillMatches(Snapshot.SubSlotIndex, Snapshot.SubArchetype))
    {
        if (GEngine && WTBRShouldLogValidation())
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
                FString::Printf(TEXT("[CompositeMerge] REFUND (slot changed) Owner=%s"), *GetNameSafe(GetOwner())));
        }
        OwnerChar->VaelComponent->ReleaseReservation(Snapshot.ReservationHandle);
        return;
    }

    OwnerChar->VaelComponent->CommitReservation(Snapshot.ReservationHandle);
    ReadyCompositeType = Snapshot.CompositeType;
    ReadyCompositeMainSlotIndex = Snapshot.MainSlotIndex;

    // A ready composite is already paid for, so it must not be free to carry
    // around indefinitely as a standing threat. Expiry discards it WITHOUT a
    // refund — same no-refund rule every other discard path already follows.
    if (UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous())
    {
        FWTBRCompositeDefinition Definition;
        if (Registry->FindDefinition(Snapshot.CompositeType, Definition)
            && Definition.ReadyCompositeHoldSeconds > 0.0f
            && GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                ReadyCompositeExpiryTimer,
                this,
                &UWTBRTriggerSetComponent::OnReadyCompositeExpired,
                Definition.ReadyCompositeHoldSeconds,
                false);
        }
    }

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green,
            FString::Printf(TEXT("[CompositeMerge] READY Type=%s Owner=%s"),
                *UEnum::GetValueAsString(Snapshot.CompositeType), *GetNameSafe(GetOwner())));
    }
}

bool UWTBRTriggerSetComponent::FireReadyComposite()
{
    if (!HasServerAuthority() || ReadyCompositeType == EWTBRCompositeBulletType::None) return false;

    const EWTBRCompositeBulletType Type = ReadyCompositeType;
    const int32 MainSlotIndex = ReadyCompositeMainSlotIndex;

    if (UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous())
    {
        FWTBRCompositeDefinition Definition;
        if (Registry->FindDefinition(Type, Definition) && Definition.CompositeCooldown > 0.0f)
        {
            bCompositeCooldownActive = true;
            GetWorld()->GetTimerManager().SetTimer(
                CompositeCooldownTimer,
                this,
                &UWTBRTriggerSetComponent::OnCompositeCooldownExpired,
                Definition.CompositeCooldown,
                false);
        }
    }

    FireComposite(Type, MainSlotIndex);
    DiscardReadyComposite();
    return true;
}

void UWTBRTriggerSetComponent::OnCompositeCooldownExpired()
{
    bCompositeCooldownActive = false;
}

bool UWTBRTriggerSetComponent::CanStartMerge() const
{
    if (CurrentMergeState != EWTBRCompositeBulletType::None) return false;
    if (ReadyCompositeType != EWTBRCompositeBulletType::None) return false;
    if (bCompositeCooldownActive) return false;
    if (!IsValidSlotIndex(ActiveMainIndex) || !IsValidSlotIndex(ActiveSubIndex)) return false;

    const UWTBRTriggerDataAsset* MainDA = TriggerSlots[ActiveMainIndex].DataAsset.Get();
    const UWTBRTriggerDataAsset* SubDA = TriggerSlots[ActiveSubIndex].DataAsset.Get();
    if (!MainDA || !SubDA) return false;

    const UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous();
    return Registry && Registry->ResolveCompositeType(
        MainDA->BulletArchetype, SubDA->BulletArchetype) != EWTBRCompositeBulletType::None;
}

void UWTBRTriggerSetComponent::DiscardReadyComposite()
{
    if (!HasServerAuthority()) return;

    if (ReadyCompositeType != EWTBRCompositeBulletType::None && GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("[CompositeMerge] DISCARD Owner=%s"), *GetNameSafe(GetOwner())));
    }

    // Cleared here rather than at each call site, so every existing discard path
    // (slot switch, match restart, EndPlay, fire) drops the timer for free.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReadyCompositeExpiryTimer);
    }

    ReadyCompositeType = EWTBRCompositeBulletType::None;
    ReadyCompositeMainSlotIndex = INDEX_NONE;
}

const FWTBRCompositeDefinition* UWTBRTriggerSetComponent::FindReadyCompositeDefinition() const
{
    if (ReadyCompositeType == EWTBRCompositeBulletType::None) return nullptr;

    UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous();
    if (!Registry) return nullptr;

    // Returns a pointer INTO the registry's array rather than a copy, so callers can
    // hand out references to the presets it names without another lookup.
    for (const FWTBRCompositeDefinition& Definition : Registry->Definitions)
    {
        if (Definition.CompositeType == ReadyCompositeType) return &Definition;
    }
    return nullptr;
}

void UWTBRTriggerSetComponent::OnReadyCompositeExpired()
{
    if (!HasServerAuthority() || ReadyCompositeType == EWTBRCompositeBulletType::None) return;

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("[CompositeMerge] EXPIRED (no refund) Owner=%s"),
                *GetNameSafe(GetOwner())));
    }

    DiscardReadyComposite();
}

void UWTBRTriggerSetComponent::FireComposite(EWTBRCompositeBulletType Type, int32 MainSlotIndex)
{
    if (!HasServerAuthority()) return;

    if (!IsValidSlotIndex(MainSlotIndex)) return;
    UWTBRTriggerDataAsset* DA = TriggerSlots[MainSlotIndex].DataAsset.Get();
    if (!DA) return;

    AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
    if (!OwnerChar || !GetWorld()) return;

    if (UWTBRCompositeRegistryDataAsset* Registry = CompositeRegistryAsset.LoadSynchronous())
    {
        FWTBRCompositeDefinition Definition;
        if (Registry->FindDefinition(Type, Definition))
        {
            // Labyrn falls back to its own behaviour when the registry names none,
            // so it works without a .uasset edit. An authored BehaviorClass still
            // wins, exactly as it does for every other composite.
            TSubclassOf<UWTBRCompositeBehaviorBase> BehaviorClass = Definition.BehaviorClass;
            if (!BehaviorClass && Type == EWTBRCompositeBulletType::Labyrn)
            {
                BehaviorClass = UWTBRLabyrnCompositeBehavior::StaticClass();
            }

            if (BehaviorClass)
            {
                if (UWTBRCompositeBehaviorBase* Behavior =
                    NewObject<UWTBRCompositeBehaviorBase>(this, BehaviorClass))
                {
                    Behavior->ExecuteComposite(OwnerChar, Definition);
                }
                return;
            }
        }
    }

    // ── Labyrn legacy path — SUPERSEDED by UWTBRLabyrnCompositeBehavior ──────
    // Only reachable now if the registry has no Labyrn definition at all. Kept
    // rather than deleted so a missing registry entry still fires something, but
    // it ignores the player's chosen preset entirely (it reads the deprecated
    // SerpveilParams.PresetShape) and knows nothing about synchronised arrival.
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

void UWTBRTriggerSetComponent::SetSlotOptionDataAssetForTest(int32 SlotIndex, TSoftObjectPtr<UWTBRTriggerDataAsset> InOptionDataAsset)
{
    if (TriggerSlots.IsValidIndex(SlotIndex))
    {
        TriggerSlots[SlotIndex].OptionDataAsset = InOptionDataAsset;
        RuntimeOptionTriggers.Remove(SlotIndex);
    }
}

void UWTBRTriggerSetComponent::InstallTriggerForTest(ETriggerSlot Slot, UWTBRTriggerBase* Trigger)
{
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
#endif

void UWTBRTriggerSetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRTriggerSetComponent, TriggerSlots);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveMainIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ActiveSubIndex);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CurrentDualWieldState);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CurrentMergeState);
    DOREPLIFETIME(UWTBRTriggerSetComponent, ReadyCompositeType);
    DOREPLIFETIME(UWTBRTriggerSetComponent, CompositeMergeDuration);
    DOREPLIFETIME(UWTBRTriggerSetComponent, bCompositeCooldownActive);
    DOREPLIFETIME(UWTBRTriggerSetComponent, bMergeWasFired);
    DOREPLIFETIME(UWTBRTriggerSetComponent, bMantornFormActive);
}
