// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Trigger/WTBRTriggerSetComponent.h"

// -----------------------------------------------------------------------------
// Corpse Loot Automation Test Foundation (Phase 1)
//
// This file is the first automation-test seam for BR corpse loot logic.
// It intentionally covers only pure, world-independent logic that can run
// without spawning actors or standing up a UWorld.
//
// Anything that requires authority / world / actor setup is documented below
// as TODO and deferred to Phase 2 (see comment block near the end), so that we
// do NOT add world/functional-test scaffolding or gameplay seams in this pass.
//
// Production behavior is unchanged by this file.
// -----------------------------------------------------------------------------

namespace
{
    // Builds a soft pointer that reports IsNull()==false WITHOUT loading anything.
    // Uses a deliberately non-existent path so no asset dependency is introduced.
    TSoftObjectPtr<UWTBRTriggerDataAsset> MakeNonNullSoftTriggerRef()
    {
        return TSoftObjectPtr<UWTBRTriggerDataAsset>(
            FSoftObjectPath(TEXT("/Game/WTBR/Automation/NonExistentTrigger.NonExistentTrigger")));
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

// -----------------------------------------------------------------------------
// Phase 2 TODO coverage (requires UWorld / authority / actor setup):
//
//  [ ] stable corpse loot entry index
//        - InitializeCorpseLootContainer preserves order/index of installed
//          trigger snapshots; GetEntryTriggerDataAsset(i) maps 1:1.
//  [ ] consumed entry reject
//        - TryMarkEntryConsumed on an already-consumed index returns false.
//  [ ] invalid entry reject
//        - IsEntryValidForPickup / TryMarkEntryConsumed reject out-of-range and
//          null-asset indices.
//  [ ] invalid slot reject
//        - Server_RequestPickupCorpseLootEntry rejects invalid TargetSlotIndex
//          (server-authoritative validation stays the real gate).
//  [ ] rollback consumed flag on replacement failure
//        - ClearEntryConsumedForFailedPickup restores bConsumed=false after a
//          failed ReplaceTriggerSlotFromDataAsset.
//  [ ] empty container destroy behavior
//        - AreAllEntriesConsumed() -> container Destroy() path.
//  [ ] CVar corpse container vs legacy path semantics
//        - WTBR.UseCorpseLootContainerOnDeath (-1/0/1) selects the correct
//          death-loot branch; MakeDefaultRulesForMode defaults unchanged.
//
// These need an automation world (FAutomationEditorCommonUtils / test world) and
// possibly a small test-only spawn helper. Introducing that scaffolding — and any
// production test seam it may require — is deferred to keep this pass code-only,
// world-free, and non-invasive to gameplay classes.
// -----------------------------------------------------------------------------

#endif // WITH_DEV_AUTOMATION_TESTS
