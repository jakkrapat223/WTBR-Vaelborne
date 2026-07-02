// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"

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
