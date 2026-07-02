// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Interaction/WTBRCorpseLootContainerActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Internationalization/Text.h"

namespace
{
static TAutoConsoleVariable<float> CVarWTBRCorpseLootContainerLifetimeSeconds(
    TEXT("WTBR.CorpseLootContainerLifetimeSeconds"),
    0.0f,
    TEXT("<= 0.0 = disabled, > 0.0 = authority corpse loot containers auto-destroy after this many seconds."),
    ECVF_Default);
}

AWTBRCorpseLootContainerActor::AWTBRCorpseLootContainerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(SceneRoot);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(SceneRoot);
    InteractionCollision->InitSphereRadius(InteractionRadius);
    InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionCollision->SetGenerateOverlapEvents(false);
}

void AWTBRCorpseLootContainerActor::InitializeCorpseLootContainer(
    const TArray<FWTBRInstalledTriggerSlotSnapshot>& InstalledTriggerSnapshots)
{
    if (!HasAuthority())
    {
        return;
    }

    LootEntries.Reset();

    for (const FWTBRInstalledTriggerSlotSnapshot& Snapshot : InstalledTriggerSnapshots)
    {
        if (!Snapshot.IsValid())
        {
            continue;
        }

        FWTBRCorpseLootEntry Entry;
        Entry.TriggerDataAsset = Snapshot.DataAsset;
        Entry.SourceSlotIndex = Snapshot.SlotIndex;
        Entry.CachedCategory = Snapshot.CachedCategory;
        LootEntries.Add(Entry);
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot container initialized: Container=%s Entries=%d"),
        *GetNameSafe(this),
        LootEntries.Num());

    const float LifetimeSeconds = CVarWTBRCorpseLootContainerLifetimeSeconds.GetValueOnGameThread();
    if (LootEntries.Num() > 0 && LifetimeSeconds > 0.0f)
    {
        SetLifeSpan(LifetimeSeconds);
    }

    NotifyLootEntriesChanged();
}

bool AWTBRCorpseLootContainerActor::IsEntryValidForPickup(int32 LootEntryIndex) const
{
    return LootEntries.IsValidIndex(LootEntryIndex) && LootEntries[LootEntryIndex].IsValidForPickup();
}

bool AWTBRCorpseLootContainerActor::IsEntryConsumed(int32 LootEntryIndex) const
{
    return LootEntries.IsValidIndex(LootEntryIndex) && LootEntries[LootEntryIndex].bConsumed;
}

bool AWTBRCorpseLootContainerActor::TryMarkEntryConsumed(int32 LootEntryIndex)
{
    if (!HasAuthority() || !IsEntryValidForPickup(LootEntryIndex))
    {
        return false;
    }

    LootEntries[LootEntryIndex].bConsumed = true;
    NotifyLootEntriesChanged();
    return true;
}

void AWTBRCorpseLootContainerActor::ClearEntryConsumedForFailedPickup(int32 LootEntryIndex)
{
    if (!HasAuthority() || !LootEntries.IsValidIndex(LootEntryIndex))
    {
        return;
    }

    LootEntries[LootEntryIndex].bConsumed = false;
    NotifyLootEntriesChanged();
}

bool AWTBRCorpseLootContainerActor::ReplaceEntryWithSnapshot(
    int32 LootEntryIndex,
    const FWTBRInstalledTriggerSlotSnapshot& Snapshot)
{
    if (!HasAuthority() || !LootEntries.IsValidIndex(LootEntryIndex) || !Snapshot.IsValid())
    {
        return false;
    }

    FWTBRCorpseLootEntry& Entry = LootEntries[LootEntryIndex];
    Entry.TriggerDataAsset = Snapshot.DataAsset;
    Entry.SourceSlotIndex = Snapshot.SlotIndex;
    Entry.CachedCategory = Snapshot.CachedCategory;
    Entry.bConsumed = false;

    NotifyLootEntriesChanged();
    return true;
}

bool AWTBRCorpseLootContainerActor::AreAllEntriesConsumed() const
{
    if (LootEntries.Num() == 0)
    {
        return true;
    }

    for (const FWTBRCorpseLootEntry& Entry : LootEntries)
    {
        if (!Entry.bConsumed)
        {
            return false;
        }
    }

    return true;
}

TSoftObjectPtr<UWTBRTriggerDataAsset> AWTBRCorpseLootContainerActor::GetEntryTriggerDataAsset(int32 LootEntryIndex) const
{
    return LootEntries.IsValidIndex(LootEntryIndex)
        ? LootEntries[LootEntryIndex].TriggerDataAsset
        : TSoftObjectPtr<UWTBRTriggerDataAsset>();
}

bool AWTBRCorpseLootContainerActor::HasAvailableLootEntries() const
{
    for (const FWTBRCorpseLootEntry& Entry : LootEntries)
    {
        if (Entry.IsValidForPickup())
        {
            return true;
        }
    }

    return false;
}

bool AWTBRCorpseLootContainerActor::CanBeInteractedWithBy(const AActor* InteractingActor) const
{
    return IsValid(InteractingActor)
        && InteractingActor != this
        && HasAvailableLootEntries();
}

FText AWTBRCorpseLootContainerActor::GetInteractionPromptText() const
{
    return HasAvailableLootEntries()
        ? NSLOCTEXT("WTBR", "CorpseLootContainerPrompt_OpenTriggerCache", "Open Trigger Cache")
        : FText::GetEmpty();
}

void AWTBRCorpseLootContainerActor::GetLootEntriesForUIReadOnly(TArray<FWTBRCorpseLootEntry>& OutLootEntries) const
{
    OutLootEntries = LootEntries;
}

FWTBRCorpseLootEntryViewModel AWTBRCorpseLootContainerActor::BuildLootEntryViewModel(int32 LootEntryIndex) const
{
    FWTBRCorpseLootEntryViewModel VM; // StableEntryIndex defaults to INDEX_NONE

    if (!LootEntries.IsValidIndex(LootEntryIndex))
    {
        return VM;
    }

    const FWTBRCorpseLootEntry& Entry = LootEntries[LootEntryIndex];
    VM.StableEntryIndex  = LootEntryIndex;
    VM.TriggerDataAsset  = Entry.TriggerDataAsset;
    VM.CachedCategory    = Entry.CachedCategory;
    VM.SourceSlotIndex   = Entry.SourceSlotIndex;
    VM.bIsAvailable      = Entry.IsValidForPickup();
    return VM;
}

void AWTBRCorpseLootContainerActor::BuildTargetSlotOptions(
    const UWTBRTriggerSetComponent* TriggerSet,
    const UWTBRTriggerDataAsset* LootDataAsset,
    TArray<FWTBRTargetSlotOptionViewModel>& OutOptions) const
{
    OutOptions.Reset();

    if (!IsValid(TriggerSet))
    {
        return;
    }

    const int32 TotalSlots = UWTBRTriggerSetComponent::TotalSlotCount;
    const int32 MainSlots  = UWTBRTriggerSetComponent::MainSlotCount;

    for (int32 SlotIndex = 0; SlotIndex < TotalSlots; ++SlotIndex)
    {
        FWTBRTargetSlotOptionViewModel Option;
        Option.SlotIndex = SlotIndex;

        const bool bIsMain   = SlotIndex < MainSlots;
        const int32 LabelNum = bIsMain ? SlotIndex + 1 : (SlotIndex - MainSlots) + 1;
        Option.SlotLabel = bIsMain
            ? FText::Format(NSLOCTEXT("WTBR", "CorpseLootUI_SlotMain", "Main {0}"), FText::AsNumber(LabelNum))
            : FText::Format(NSLOCTEXT("WTBR", "CorpseLootUI_SlotSub",  "Sub {0}"),  FText::AsNumber(LabelNum));

        FWTBRInstalledTriggerSlotSnapshot SlotSnapshot;
        const bool bHasSnapshot = TriggerSet->GetTriggerSlotSnapshot(SlotIndex, SlotSnapshot);
        Option.bIsEmpty        = !bHasSnapshot;
        Option.EquippedDataAsset = bHasSnapshot
            ? SlotSnapshot.DataAsset
            : TSoftObjectPtr<UWTBRTriggerDataAsset>();

        if (IsValid(LootDataAsset))
        {
            Option.bIsCompatible = TriggerSet->IsDataAssetCompatibleWithTargetSlot(SlotIndex, LootDataAsset);
            if (!Option.bIsCompatible)
            {
                switch (LootDataAsset->SlotConstraint)
                {
                    case ETriggerSlotConstraint::MainOnly:
                        Option.IncompatibleReason = NSLOCTEXT("WTBR", "CorpseLootUI_IncompatMainOnly", "Main slot only");
                        break;
                    case ETriggerSlotConstraint::SubOnly:
                        Option.IncompatibleReason = NSLOCTEXT("WTBR", "CorpseLootUI_IncompatSubOnly", "Sub slot only");
                        break;
                    default:
                        Option.IncompatibleReason = NSLOCTEXT("WTBR", "CorpseLootUI_IncompatUnknown", "Incompatible");
                        break;
                }
            }
        }
        else
        {
            // DataAsset not in memory — show optimistically compatible; server validates pickup.
            Option.bIsCompatible = true;
        }

        OutOptions.Add(MoveTemp(Option));
    }
}

void AWTBRCorpseLootContainerActor::BuildTargetSlotOptionsForEntry(
    const UWTBRTriggerSetComponent* TriggerSet,
    int32 LootEntryIndex,
    TArray<FWTBRTargetSlotOptionViewModel>& OutOptions) const
{
    OutOptions.Reset();

    if (!LootEntries.IsValidIndex(LootEntryIndex))
    {
        return;
    }

    const UWTBRTriggerDataAsset* LoadedDA = LootEntries[LootEntryIndex].TriggerDataAsset.Get();
    BuildTargetSlotOptions(TriggerSet, LoadedDA, OutOptions);
}

void AWTBRCorpseLootContainerActor::OnRep_LootEntries()
{
    NotifyLootEntriesChanged();
}

void AWTBRCorpseLootContainerActor::NotifyLootEntriesChanged()
{
    OnCorpseLootEntriesChanged.Broadcast();
}

void AWTBRCorpseLootContainerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWTBRCorpseLootContainerActor, LootEntries);
}
