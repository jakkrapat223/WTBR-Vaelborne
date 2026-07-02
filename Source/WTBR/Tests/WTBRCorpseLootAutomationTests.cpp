// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Trigger/WTBRTriggerSetComponent.h"

// -----------------------------------------------------------------------------
// Corpse Loot Automation Tests
//
// This file covers both pure corpse loot struct/predicate contracts and limited
// actor-level Corpse Loot Container behavior. Actor-level tests use a transient
// UWorld fixture and remain code-only: no map, Blueprint, WBP, UMG, .uasset, or
// production gameplay changes.
//
// Full character RPC pickup, trigger replacement, focused trace, dedicated
// server, and late-join behavior are intentionally outside this file's current
// coverage and are tracked in the gap list near the end.
//
// Production behavior is unchanged by this file.
// -----------------------------------------------------------------------------

namespace
{
    // Minimal spawn-only world fixture. It does not initialize full gameplay or
    // BeginPlay flow; do not reuse it for controller, focused trace, network,
    // match rules, or begun-play gameplay tests without extending it deliberately.
    class FWTBRTransientWorldFixture
    {
    public:
        explicit FWTBRTransientWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRTransientWorldFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World = nullptr;
            }
        }

        UWorld* GetWorld() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    class FWTBRScopedFloatCVarOverride
    {
    public:
        FWTBRScopedFloatCVarOverride(const TCHAR* InCVarName, float NewValue)
            : CVarName(InCVarName)
        {
            CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
            if (CVar)
            {
                OriginalValue = CVar->GetFloat();
                CVar->Set(NewValue, ECVF_SetByCode);
            }
        }

        ~FWTBRScopedFloatCVarOverride()
        {
            if (CVar)
            {
                CVar->Set(OriginalValue, ECVF_SetByCode);
            }
        }

        bool IsValid() const { return CVar != nullptr; }

    private:
        const TCHAR* CVarName = nullptr;
        IConsoleVariable* CVar = nullptr;
        float OriginalValue = 0.0f;
    };

    // Builds a soft pointer that reports IsNull()==false WITHOUT loading anything.
    // Uses a deliberately non-existent path so no asset dependency is introduced.
    TSoftObjectPtr<UWTBRTriggerDataAsset> MakeSoftTriggerRef(const TCHAR* AssetName)
    {
        const FString AssetPath = FString::Printf(
            TEXT("/Game/WTBR/Automation/%s.%s"),
            AssetName,
            AssetName);

        return TSoftObjectPtr<UWTBRTriggerDataAsset>(
            FSoftObjectPath(AssetPath));
    }

    TSoftObjectPtr<UWTBRTriggerDataAsset> MakeNonNullSoftTriggerRef()
    {
        return MakeSoftTriggerRef(TEXT("NonExistentTrigger"));
    }

    FWTBRInstalledTriggerSlotSnapshot MakeInstalledTriggerSnapshot(
        int32 SlotIndex,
        const TCHAR* AssetName,
        ETriggerCategory CachedCategory = ETriggerCategory::None)
    {
        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        Snapshot.SlotIndex = SlotIndex;
        Snapshot.DataAsset = MakeSoftTriggerRef(AssetName);
        Snapshot.CachedCategory = CachedCategory;
        return Snapshot;
    }

    AWTBRCorpseLootContainerActor* SpawnCorpseLootContainer(UWorld* World)
    {
        return World
            ? World->SpawnActor<AWTBRCorpseLootContainerActor>(
                AWTBRCorpseLootContainerActor::StaticClass(),
                FVector::ZeroVector,
                FRotator::ZeroRotator)
            : nullptr;
    }

    void TickTransientWorld(UWorld* World, float TotalSeconds, float StepSeconds = 0.05f)
    {
        if (!World || TotalSeconds <= 0.0f || StepSeconds <= 0.0f)
        {
            return;
        }

        float ElapsedSeconds = 0.0f;
        while (ElapsedSeconds < TotalSeconds)
        {
            const float DeltaSeconds = FMath::Min(StepSeconds, TotalSeconds - ElapsedSeconds);
            World->Tick(LEVELTICK_All, DeltaSeconds);
            ElapsedSeconds += DeltaSeconds;
        }
    }
}

/**
 * Verifies FWTBRCorpseLootEntry::IsValidForPickup() semantics.
 * This is the smallest stable contract the pickup path relies on:
 *   valid  == (!bConsumed && !TriggerDataAsset.IsNull())
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootEntryValidityTest,
    "WTBR.CorpseLoot.LootEntryValidity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootEntryValidityTest::RunTest(const FString& /*Parameters*/)
{
    // Case 1: default-constructed entry has a null asset -> not pickable.
    {
        FWTBRCorpseLootEntry Entry;
        TestFalse(TEXT("Default entry (null asset) is not valid for pickup"), Entry.IsValidForPickup());
    }

    // Case 2: valid asset, not consumed -> pickable.
    {
        FWTBRCorpseLootEntry Entry;
        Entry.TriggerDataAsset = MakeNonNullSoftTriggerRef();
        Entry.bConsumed = false;
        TestTrue(TEXT("Non-consumed entry with a set asset is valid for pickup"), Entry.IsValidForPickup());
    }

    // Case 3: valid asset, consumed -> rejected (consumed-entry reject at struct level).
    {
        FWTBRCorpseLootEntry Entry;
        Entry.TriggerDataAsset = MakeNonNullSoftTriggerRef();
        Entry.bConsumed = true;
        TestFalse(TEXT("Consumed entry is rejected even with a set asset"), Entry.IsValidForPickup());
    }

    // Case 4: null asset but not consumed -> still rejected (invalid-entry reject).
    {
        FWTBRCorpseLootEntry Entry;
        Entry.bConsumed = false; // asset stays null
        TestFalse(TEXT("Entry with null asset is rejected regardless of consumed flag"), Entry.IsValidForPickup());
    }

    return true;
}

/**
 * Verifies the consumed-flag round-trip contract on FWTBRCorpseLootEntry.
 * This mirrors the server rollback semantics (ClearEntryConsumedForFailedPickup)
 * at struct level: consuming an entry invalidates it, restoring the flag makes
 * the same entry pickable again with no other state involved.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootEntryConsumedRoundTripTest,
    "WTBR.CorpseLoot.LootEntryConsumedRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootEntryConsumedRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCorpseLootEntry Entry;
    Entry.TriggerDataAsset = MakeNonNullSoftTriggerRef();

    TestTrue(TEXT("Entry starts valid for pickup"), Entry.IsValidForPickup());

    // Consume: entry must become rejected.
    Entry.bConsumed = true;
    TestFalse(TEXT("Consumed entry is not valid for pickup"), Entry.IsValidForPickup());

    // Rollback (failed replacement): restoring the flag must restore pickability.
    Entry.bConsumed = false;
    TestTrue(TEXT("Entry restored by rollback is valid for pickup again"), Entry.IsValidForPickup());

    // The asset reference must be untouched by the flag round-trip.
    TestFalse(TEXT("Asset reference survives the consume/rollback round-trip"), Entry.TriggerDataAsset.IsNull());

    return true;
}

/**
 * Verifies FWTBRInstalledTriggerSlotSnapshot::IsValid() semantics.
 * Snapshots are the input contract for BOTH death-loot paths: invalid snapshots
 * are skipped by SpawnLegacyDroppedTriggers_Internal and by
 * InitializeCorpseLootContainer, so this gate decides what can become loot.
 *   valid == (SlotIndex != INDEX_NONE && !DataAsset.IsNull())
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInstalledTriggerSlotSnapshotValidityTest,
    "WTBR.CorpseLoot.InstalledTriggerSlotSnapshotValidity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInstalledTriggerSlotSnapshotValidityTest::RunTest(const FString& /*Parameters*/)
{
    // Case 1: default-constructed snapshot (INDEX_NONE slot, null asset) -> invalid.
    {
        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        TestFalse(TEXT("Default snapshot is invalid"), Snapshot.IsValid());
    }

    // Case 2: slot set but asset null -> invalid.
    {
        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        Snapshot.SlotIndex = 0;
        TestFalse(TEXT("Snapshot with null asset is invalid even with a set slot"), Snapshot.IsValid());
    }

    // Case 3: asset set but slot INDEX_NONE -> invalid.
    {
        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        Snapshot.DataAsset = MakeNonNullSoftTriggerRef();
        TestFalse(TEXT("Snapshot with INDEX_NONE slot is invalid even with a set asset"), Snapshot.IsValid());
    }

    // Case 4: slot set and asset set -> valid (becomes loot on death).
    {
        FWTBRInstalledTriggerSlotSnapshot Snapshot;
        Snapshot.SlotIndex = 2;
        Snapshot.DataAsset = MakeNonNullSoftTriggerRef();
        TestTrue(TEXT("Snapshot with valid slot and asset is valid"), Snapshot.IsValid());
    }

    return true;
}

/**
 * Verifies that InitializeCorpseLootContainer only exposes valid snapshots as
 * pickable entries while skipping null assets and INDEX_NONE slots.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerInitializationFiltersInvalidSnapshotsTest,
    "WTBR.CorpseLoot.ContainerInitializationFiltersInvalidSnapshots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerInitializationFiltersInvalidSnapshotsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_FilterInvalidSnapshots"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    FWTBRInstalledTriggerSlotSnapshot InvalidNullAsset;
    InvalidNullAsset.SlotIndex = 1;

    FWTBRInstalledTriggerSlotSnapshot InvalidSlot = MakeInstalledTriggerSnapshot(
        INDEX_NONE,
        TEXT("InvalidSlotTrigger"));

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("ValidTriggerA")));
    Snapshots.Add(InvalidNullAsset);
    Snapshots.Add(MakeInstalledTriggerSnapshot(3, TEXT("ValidTriggerB")));
    Snapshots.Add(InvalidSlot);

    Container->InitializeCorpseLootContainer(Snapshots);

    TestEqual(TEXT("Only valid snapshots become loot entries"), Container->GetLootEntryCount(), 2);
    TestTrue(TEXT("First valid entry is available for pickup"), Container->IsEntryValidForPickup(0));
    TestTrue(TEXT("Second valid entry is available for pickup"), Container->IsEntryValidForPickup(1));
    TestFalse(TEXT("Skipped invalid snapshot does not create an extra entry"), Container->IsEntryValidForPickup(2));
    TestTrue(TEXT("Container reports available loot after valid initialization"), Container->HasAvailableLootEntries());

    return true;
}

/**
 * Verifies stable entry index/order behavior for initialized valid snapshots.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerEntryIndexPreservesSnapshotOrderTest,
    "WTBR.CorpseLoot.ContainerEntryIndexPreservesSnapshotOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerEntryIndexPreservesSnapshotOrderTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_EntryOrder"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    const FString ExpectedFirstPath = MakeSoftTriggerRef(TEXT("OrderTriggerA")).ToSoftObjectPath().ToString();
    const FString ExpectedSecondPath = MakeSoftTriggerRef(TEXT("OrderTriggerB")).ToSoftObjectPath().ToString();
    const FString ExpectedThirdPath = MakeSoftTriggerRef(TEXT("OrderTriggerC")).ToSoftObjectPath().ToString();

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(4, TEXT("OrderTriggerA")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(2, TEXT("OrderTriggerB")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(7, TEXT("OrderTriggerC")));

    Container->InitializeCorpseLootContainer(Snapshots);

    TestEqual(TEXT("All valid snapshots become entries"), Container->GetLootEntryCount(), 3);
    TestEqual(TEXT("Entry 0 keeps snapshot 0 asset"), Container->GetEntryTriggerDataAsset(0).ToSoftObjectPath().ToString(), ExpectedFirstPath);
    TestEqual(TEXT("Entry 1 keeps snapshot 1 asset"), Container->GetEntryTriggerDataAsset(1).ToSoftObjectPath().ToString(), ExpectedSecondPath);
    TestEqual(TEXT("Entry 2 keeps snapshot 2 asset"), Container->GetEntryTriggerDataAsset(2).ToSoftObjectPath().ToString(), ExpectedThirdPath);
    TestTrue(TEXT("Entry 0 is valid for pickup"), Container->IsEntryValidForPickup(0));
    TestTrue(TEXT("Entry 1 is valid for pickup"), Container->IsEntryValidForPickup(1));
    TestTrue(TEXT("Entry 2 is valid for pickup"), Container->IsEntryValidForPickup(2));

    return true;
}

/**
 * Verifies direct actor consume semantics without exercising character pickup,
 * trigger replacement, or destroy-on-last-pickup behavior.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerConsumeRejectsInvalidAndConsumedEntriesTest,
    "WTBR.CorpseLoot.ContainerConsumeRejectsInvalidAndConsumedEntries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerConsumeRejectsInvalidAndConsumedEntriesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_ConsumeRejects"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("ConsumeTrigger")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TestFalse(TEXT("Negative entry index cannot be consumed"), Container->TryMarkEntryConsumed(-1));
    TestFalse(TEXT("Out-of-range entry index cannot be consumed"), Container->TryMarkEntryConsumed(1));
    TestFalse(TEXT("Entry starts unconsumed"), Container->IsEntryConsumed(0));
    TestTrue(TEXT("Valid entry can be consumed"), Container->TryMarkEntryConsumed(0));
    TestTrue(TEXT("Consumed state is visible"), Container->IsEntryConsumed(0));
    TestFalse(TEXT("Consumed entry is no longer valid for pickup"), Container->IsEntryValidForPickup(0));
    TestFalse(TEXT("Already-consumed entry cannot be consumed again"), Container->TryMarkEntryConsumed(0));

    return true;
}

/**
 * Verifies failed-replacement rollback behavior at the container actor boundary.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerConsumeRollbackRestoresAvailabilityTest,
    "WTBR.CorpseLoot.ContainerConsumeRollbackRestoresAvailability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerConsumeRollbackRestoresAvailabilityTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_Rollback"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("RollbackTrigger")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TestTrue(TEXT("Entry can be consumed before rollback"), Container->TryMarkEntryConsumed(0));
    TestFalse(TEXT("Consumed entry is unavailable before rollback"), Container->IsEntryValidForPickup(0));

    Container->ClearEntryConsumedForFailedPickup(0);

    TestFalse(TEXT("Entry is no longer consumed after rollback"), Container->IsEntryConsumed(0));
    TestTrue(TEXT("Rolled-back entry is available again"), Container->IsEntryValidForPickup(0));
    TestTrue(TEXT("Rolled-back entry can be consumed again"), Container->TryMarkEntryConsumed(0));

    return true;
}

/**
 * Verifies availability, prompt text, and the safe container-local interaction
 * predicate without depending on a character controller or trace setup.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerAvailabilityAndPromptTest,
    "WTBR.CorpseLoot.ContainerAvailabilityAndPrompt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerAvailabilityAndPromptTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_AvailabilityPrompt"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    AActor* InteractingActor = World->SpawnActor<AActor>(
        AActor::StaticClass(),
        FVector(100.0f, 0.0f, 0.0f),
        FRotator::ZeroRotator);
    TestNotNull(TEXT("Plain actor can stand in for safe container-local interaction predicate"), InteractingActor);
    if (!InteractingActor)
    {
        return false;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("PromptTrigger")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TestTrue(TEXT("Container has available loot before consumption"), Container->HasAvailableLootEntries());
    TestEqual(
        TEXT("Available container prompt is the expected prompt"),
        Container->GetInteractionPromptText().ToString(),
        FString(TEXT("Open Trigger Cache")));
    TestTrue(TEXT("Different valid actor can interact while loot is available"), Container->CanBeInteractedWithBy(InteractingActor));
    TestFalse(TEXT("Container cannot interact with itself"), Container->CanBeInteractedWithBy(Container));
    TestFalse(TEXT("Null actor cannot interact"), Container->CanBeInteractedWithBy(nullptr));

    TestTrue(TEXT("Entry can be consumed"), Container->TryMarkEntryConsumed(0));
    TestFalse(TEXT("Container has no available loot after consuming only entry"), Container->HasAvailableLootEntries());
    TestTrue(TEXT("Prompt is empty when no loot remains"), Container->GetInteractionPromptText().IsEmpty());
    TestFalse(TEXT("Interactor is rejected when no loot remains"), Container->CanBeInteractedWithBy(InteractingActor));

    Container->ClearEntryConsumedForFailedPickup(0);

    TestTrue(TEXT("Container availability returns after rollback"), Container->HasAvailableLootEntries());
    TestEqual(
        TEXT("Prompt returns after rollback"),
        Container->GetInteractionPromptText().ToString(),
        FString(TEXT("Open Trigger Cache")));
    TestTrue(TEXT("Interactor is accepted again after rollback"), Container->CanBeInteractedWithBy(InteractingActor));

    return true;
}

/**
 * Verifies aggregate consumed/availability state at the container actor seam.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerAllEntriesConsumedStateTest,
    "WTBR.CorpseLoot.ContainerAllEntriesConsumedState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerAllEntriesConsumedStateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_AllConsumed"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("AllConsumedTriggerA")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(1, TEXT("AllConsumedTriggerB")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TestFalse(TEXT("Container with available entries is not fully consumed"), Container->AreAllEntriesConsumed());
    TestTrue(TEXT("Container starts with available loot"), Container->HasAvailableLootEntries());

    TestTrue(TEXT("First entry can be consumed"), Container->TryMarkEntryConsumed(0));
    TestFalse(TEXT("Container is not fully consumed while one entry remains"), Container->AreAllEntriesConsumed());
    TestTrue(TEXT("Container still has available loot after one entry is consumed"), Container->HasAvailableLootEntries());

    TestTrue(TEXT("Second entry can be consumed"), Container->TryMarkEntryConsumed(1));
    TestTrue(TEXT("Container is fully consumed after both entries are consumed"), Container->AreAllEntriesConsumed());
    TestFalse(TEXT("Container has no available loot after all entries are consumed"), Container->HasAvailableLootEntries());

    Container->ClearEntryConsumedForFailedPickup(0);

    TestFalse(TEXT("Rolling back one entry makes container not fully consumed"), Container->AreAllEntriesConsumed());
    TestTrue(TEXT("Rolling back one entry restores available loot"), Container->HasAvailableLootEntries());

    return true;
}

/**
 * Verifies that the disabled lifetime CVar does not auto-destroy initialized
 * containers during deterministic transient-world ticks.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerLifetimeCVarZeroDoesNotAutoDestroyTest,
    "WTBR.CorpseLoot.ContainerLifetimeCVarZeroDoesNotAutoDestroy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerLifetimeCVarZeroDoesNotAutoDestroyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRScopedFloatCVarOverride LifetimeOverride(TEXT("WTBR.CorpseLootContainerLifetimeSeconds"), 0.0f);
    TestTrue(TEXT("Corpse loot lifetime CVar is available"), LifetimeOverride.IsValid());
    if (!LifetimeOverride.IsValid())
    {
        return false;
    }

    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_LifetimeZero"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient test world is available"), World);
    if (!World)
    {
        return false;
    }

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Corpse loot container spawns in transient world"), Container);
    if (!Container)
    {
        return false;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("LifetimeZeroTrigger")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TickTransientWorld(World, 0.5f);

    TestTrue(TEXT("Container remains valid when lifetime CVar is disabled"), IsValid(Container));
    TestFalse(TEXT("Container is not being destroyed when lifetime CVar is disabled"), Container->IsActorBeingDestroyed());
    TestTrue(TEXT("Container still has available loot after disabled-lifetime tick"), Container->HasAvailableLootEntries());

    return true;
}

// -----------------------------------------------------------------------------
// Remaining coverage gaps:
//
//  [ ] full AWTBRCharacter::Server_RequestPickupCorpseLootEntry path
//        - Includes match-rule gates, alive/authority checks, distance checks,
//          replacement, rollback, and final container cleanup in the real caller.
//  [ ] invalid target slot through RPC
//        - Server-authoritative TargetSlotIndex validation is exercised through
//          ReplaceTriggerSlotFromDataAsset in the character pickup flow.
//  [ ] ReplaceTriggerSlotFromDataAsset end-to-end
//        - Requires trigger set/loadout state beyond the container actor seam.
//  [ ] destroy-after-last-pickup through character flow
//        - Container-level availability is covered here; actual Destroy() lives
//          after successful character RPC pickup/replacement.
//  [ ] focused trace end-to-end
//        - Requires character/controller/viewpoint/collision setup.
//  [ ] positive WTBR.CorpseLootContainerLifetimeSeconds auto-destroy
//        - Skipped here because expiry did not fire deterministically in the
//          minimal spawn-only transient world fixture.
//  [ ] dedicated server / late join
//        - Requires multiplayer automation coverage outside this code-only file.
// -----------------------------------------------------------------------------

#endif // WITH_DEV_AUTOMATION_TESTS
