// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Interaction/WTBRCorpseLootContainerActor.h"

#include "Components/SceneComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

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

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    SetRootComponent(RootSceneComponent);
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
