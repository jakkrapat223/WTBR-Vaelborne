// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Components/WTBRInteractionComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"

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

    struct FWTBRPositiveLifetimeLatentTestState
    {
        FWTBRPositiveLifetimeLatentTestState()
            : LifetimeOverride(TEXT("WTBR.CorpseLootContainerLifetimeSeconds"), 0.1f)
            , WorldFixture(TEXT("WTBR_CorpseLoot_LifetimePositive"))
        {
            UWorld* World = WorldFixture.GetWorld();
            if (!LifetimeOverride.IsValid() || !World)
            {
                return;
            }

            AWTBRCorpseLootContainerActor* SpawnedContainer = SpawnCorpseLootContainer(World);
            if (!SpawnedContainer)
            {
                return;
            }

            TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
            Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("LifetimePositiveTrigger")));
            SpawnedContainer->InitializeCorpseLootContainer(Snapshots);

            Container = SpawnedContainer;
            bSetupSucceeded = true;
        }

        FWTBRScopedFloatCVarOverride LifetimeOverride;
        FWTBRTransientWorldFixture WorldFixture;
        TWeakObjectPtr<AWTBRCorpseLootContainerActor> Container;
        float ElapsedSeconds = 0.0f;
        bool bSetupSucceeded = false;
    };

    class FWTBRPositiveLifetimeLatentCommand : public IAutomationLatentCommand
    {
    public:
        FWTBRPositiveLifetimeLatentCommand(
            FAutomationTestBase* InTest,
            const TSharedRef<FWTBRPositiveLifetimeLatentTestState>& InState)
            : Test(InTest)
            , State(InState)
        {
        }

        virtual bool Update() override
        {
            if (!State->bSetupSucceeded)
            {
                return true;
            }

            constexpr float TickStepSeconds = 0.05f;
            constexpr float TimeoutSeconds = 1.0f;

            UWorld* World = State->WorldFixture.GetWorld();
            if (!World)
            {
                Test->AddError(TEXT("Transient test world disappeared before lifetime expiry could be verified."));
                return true;
            }

            TickTransientWorld(World, TickStepSeconds, TickStepSeconds);
            State->ElapsedSeconds += TickStepSeconds;

            if (!State->Container.IsValid())
            {
                Test->TestTrue(TEXT("Container is destroyed after positive lifetime expires"), true);
                return true;
            }

            if (State->ElapsedSeconds >= TimeoutSeconds)
            {
                Test->TestTrue(TEXT("Container is destroyed after positive lifetime expires"), false);
                return true;
            }

            return false;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        TSharedRef<FWTBRPositiveLifetimeLatentTestState> State;
    };
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
 * Verifies the same-container swap seam: a consumed entry can be replaced with
 * the target slot's previous trigger while keeping its stable entry index.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerReplaceEntryWithSnapshotPreservesIndexTest,
    "WTBR.CorpseLoot.ContainerReplaceEntryWithSnapshotPreservesIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerReplaceEntryWithSnapshotPreservesIndexTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_CorpseLoot_ReplaceEntry"));
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
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("CorpseEntryOriginal")));
    Container->InitializeCorpseLootContainer(Snapshots);

    const FString ReturnedTriggerPath = MakeSoftTriggerRef(TEXT("ReturnedTargetSlotTrigger")).ToSoftObjectPath().ToString();
    const FWTBRInstalledTriggerSlotSnapshot ReturnedSnapshot = MakeInstalledTriggerSnapshot(
        3,
        TEXT("ReturnedTargetSlotTrigger"),
        ETriggerCategory::Gunner);

    TestTrue(TEXT("Entry can be reserved/consumed before same-container swap"), Container->TryMarkEntryConsumed(0));
    TestTrue(TEXT("Consumed entry is replaced by returned target-slot snapshot"), Container->ReplaceEntryWithSnapshot(0, ReturnedSnapshot));
    TestFalse(TEXT("Entry is available after same-container swap"), Container->IsEntryConsumed(0));
    TestTrue(TEXT("Entry remains valid for pickup after same-container swap"), Container->IsEntryValidForPickup(0));
    TestEqual(TEXT("Stable entry index now exposes returned trigger"), Container->GetEntryTriggerDataAsset(0).ToSoftObjectPath().ToString(), ReturnedTriggerPath);
    TestFalse(TEXT("Single returned entry is not considered fully consumed"), Container->AreAllEntriesConsumed());

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

/**
 * Verifies positive corpse loot container lifetime expiry using a latent command
 * so the world timer manager advances across automation framework frames.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCorpseLootContainerLifetimeCVarPositiveDestroysActorTest,
    "WTBR.CorpseLoot.ContainerLifetimeCVarPositiveDestroysActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCorpseLootContainerLifetimeCVarPositiveDestroysActorTest::RunTest(const FString& /*Parameters*/)
{
    TSharedRef<FWTBRPositiveLifetimeLatentTestState> State = MakeShared<FWTBRPositiveLifetimeLatentTestState>();

    TestTrue(TEXT("Corpse loot lifetime CVar is available"), State->LifetimeOverride.IsValid());
    TestNotNull(TEXT("Transient test world is available"), State->WorldFixture.GetWorld());
    TestTrue(TEXT("Corpse loot container was spawned and initialized"), State->bSetupSucceeded);
    TestTrue(TEXT("Container starts valid before lifetime expiry"), State->Container.IsValid());

    if (!State->LifetimeOverride.IsValid() || !State->WorldFixture.GetWorld() || !State->bSetupSucceeded || !State->Container.IsValid())
    {
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWTBRPositiveLifetimeLatentCommand(this, State));

    return true;
}

// =============================================================================
// UI Model / Query Layer Tests
// =============================================================================

/**
 * Verifies BuildLootEntryViewModel assigns the stable entry index equal to the
 * array position at which the entry lives inside the container.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelEntryViewModelPreservesStableEntryIndexTest,
    "WTBR.CorpseLoot.UIModel.EntryViewModelPreservesStableEntryIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelEntryViewModelPreservesStableEntryIndexTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_StableIndex"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("VMIndexA")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(1, TEXT("VMIndexB")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(2, TEXT("VMIndexC")));
    Container->InitializeCorpseLootContainer(Snapshots);

    for (int32 i = 0; i < 3; ++i)
    {
        const FWTBRCorpseLootEntryViewModel VM = Container->BuildLootEntryViewModel(i);
        TestEqual(FString::Printf(TEXT("VM[%d] StableEntryIndex == %d"), i, i), VM.StableEntryIndex, i);
        TestTrue(FString::Printf(TEXT("VM[%d] bIsAvailable true"), i), VM.bIsAvailable);
        TestFalse(FString::Printf(TEXT("VM[%d] TriggerDataAsset not null"), i), VM.TriggerDataAsset.IsNull());
    }

    // Out-of-range index returns a default-constructed ViewModel with INDEX_NONE.
    const FWTBRCorpseLootEntryViewModel VMInvalid = Container->BuildLootEntryViewModel(99);
    TestEqual(TEXT("Invalid index ViewModel has INDEX_NONE stable index"), VMInvalid.StableEntryIndex, (int32)INDEX_NONE);
    TestFalse(TEXT("Invalid index ViewModel is not available"), VMInvalid.bIsAvailable);

    return true;
}

/**
 * Verifies BuildLootEntryViewModel reflects consumed state correctly via bIsAvailable.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelEntryViewModelConsumedStateTest,
    "WTBR.CorpseLoot.UIModel.EntryViewModelConsumedStateReflectsAvailability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelEntryViewModelConsumedStateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_Consumed"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("VMConsumedTrigger")));
    Container->InitializeCorpseLootContainer(Snapshots);

    TestTrue(TEXT("ViewModel shows available before consume"),
        Container->BuildLootEntryViewModel(0).bIsAvailable);

    Container->TryMarkEntryConsumed(0);
    TestFalse(TEXT("ViewModel shows unavailable after consume"),
        Container->BuildLootEntryViewModel(0).bIsAvailable);

    Container->ClearEntryConsumedForFailedPickup(0);
    TestTrue(TEXT("ViewModel shows available again after rollback"),
        Container->BuildLootEntryViewModel(0).bIsAvailable);

    return true;
}

/**
 * Verifies BuildTargetSlotOptions produces 8 options and respects MainOnly constraint:
 * Main slots [0,3] compatible, Sub slots [4,7] incompatible.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelTargetSlotOptionsMainOnlyCompatibilityTest,
    "WTBR.CorpseLoot.UIModel.TargetSlotOptionsMainOnlyCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelTargetSlotOptionsMainOnlyCompatibilityTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_MainOnly"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    Container->InitializeCorpseLootContainer({});

    UWTBRTriggerSetComponent* TriggerSet = NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
    TestNotNull(TEXT("DataAsset created"), DA);
    if (!DA) return false;
    DA->SlotConstraint = ETriggerSlotConstraint::MainOnly;

    TArray<FWTBRTargetSlotOptionViewModel> Options;
    Container->BuildTargetSlotOptions(TriggerSet, DA, Options);

    TestEqual(TEXT("Exactly 8 slot options built"), Options.Num(), 8);

    for (int32 i = 0; i < 4; ++i)
    {
        TestEqual(FString::Printf(TEXT("Option[%d] SlotIndex correct"), i), Options[i].SlotIndex, i);
        TestTrue(FString::Printf(TEXT("Main slot %d compatible with MainOnly"), i), Options[i].bIsCompatible);
        TestTrue(FString::Printf(TEXT("Main slot %d IncompatibleReason empty"), i), Options[i].IncompatibleReason.IsEmpty());
    }

    for (int32 i = 4; i < 8; ++i)
    {
        TestEqual(FString::Printf(TEXT("Option[%d] SlotIndex correct"), i), Options[i].SlotIndex, i);
        TestFalse(FString::Printf(TEXT("Sub slot %d incompatible with MainOnly"), i), Options[i].bIsCompatible);
        TestFalse(FString::Printf(TEXT("Sub slot %d has IncompatibleReason"), i), Options[i].IncompatibleReason.IsEmpty());
    }

    return true;
}

/**
 * Verifies BuildTargetSlotOptions respects SubOnly constraint:
 * Sub slots [4,7] compatible, Main slots [0,3] incompatible.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelTargetSlotOptionsSubOnlyCompatibilityTest,
    "WTBR.CorpseLoot.UIModel.TargetSlotOptionsSubOnlyCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelTargetSlotOptionsSubOnlyCompatibilityTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_SubOnly"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    Container->InitializeCorpseLootContainer({});

    UWTBRTriggerSetComponent* TriggerSet = NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
    TestNotNull(TEXT("DataAsset created"), DA);
    if (!DA) return false;
    DA->SlotConstraint = ETriggerSlotConstraint::SubOnly;

    TArray<FWTBRTargetSlotOptionViewModel> Options;
    Container->BuildTargetSlotOptions(TriggerSet, DA, Options);

    TestEqual(TEXT("Exactly 8 slot options built"), Options.Num(), 8);

    for (int32 i = 0; i < 4; ++i)
    {
        TestFalse(FString::Printf(TEXT("Main slot %d incompatible with SubOnly"), i), Options[i].bIsCompatible);
        TestFalse(FString::Printf(TEXT("Main slot %d has IncompatibleReason"), i), Options[i].IncompatibleReason.IsEmpty());
    }

    for (int32 i = 4; i < 8; ++i)
    {
        TestTrue(FString::Printf(TEXT("Sub slot %d compatible with SubOnly"), i), Options[i].bIsCompatible);
        TestTrue(FString::Printf(TEXT("Sub slot %d IncompatibleReason empty"), i), Options[i].IncompatibleReason.IsEmpty());
    }

    return true;
}

/**
 * Verifies that each option's SlotLabel is non-empty and distinguishes Main from Sub.
 * Also verifies null DataAsset produces optimistic-compatible options (no IncompatibleReason).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelTargetSlotOptionsLabelAndNullDataAssetTest,
    "WTBR.CorpseLoot.UIModel.TargetSlotOptionsLabelAndNullDataAssetIsOptimistic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelTargetSlotOptionsLabelAndNullDataAssetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_Label"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    Container->InitializeCorpseLootContainer({});

    UWTBRTriggerSetComponent* TriggerSet = NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    TArray<FWTBRTargetSlotOptionViewModel> Options;
    // Null DataAsset — all slots should be shown as compatible.
    Container->BuildTargetSlotOptions(TriggerSet, nullptr, Options);

    TestEqual(TEXT("8 options built with null DataAsset"), Options.Num(), 8);

    for (int32 i = 0; i < 8; ++i)
    {
        TestFalse(FString::Printf(TEXT("Option[%d] SlotLabel not empty"), i), Options[i].SlotLabel.IsEmpty());
        TestTrue(FString::Printf(TEXT("Option[%d] optimistically compatible with null DataAsset"), i), Options[i].bIsCompatible);
        TestTrue(FString::Printf(TEXT("Option[%d] IncompatibleReason empty when null DataAsset"), i), Options[i].IncompatibleReason.IsEmpty());
    }

    // Labels for Main slots should contain different text than Sub slots.
    TestNotEqual(TEXT("Main label != Sub label"), Options[0].SlotLabel.ToString(), Options[4].SlotLabel.ToString());

    return true;
}

/**
 * Verifies that an occupied slot is correctly identified in the option ViewModel:
 * bIsEmpty false and EquippedDataAsset matches the installed asset.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelOccupiedSlotReportsEquippedTriggerTest,
    "WTBR.CorpseLoot.UIModel.OccupiedSlotReportsEquippedTrigger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelOccupiedSlotReportsEquippedTriggerTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_Occupied"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    Container->InitializeCorpseLootContainer({});

    UWTBRTriggerSetComponent* TriggerSet = NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    // Occupy slot 2 via test-only setter.
    const TSoftObjectPtr<UWTBRTriggerDataAsset> OccupiedRef = MakeSoftTriggerRef(TEXT("OccupiedSlotDA"));
    TriggerSet->SetSlotDataAssetForTest(2, OccupiedRef);

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
    if (!DA) return false;
    DA->SlotConstraint = ETriggerSlotConstraint::Any;

    TArray<FWTBRTargetSlotOptionViewModel> Options;
    Container->BuildTargetSlotOptions(TriggerSet, DA, Options);

    TestEqual(TEXT("8 options built"), Options.Num(), 8);

    // Slot 2 must be occupied.
    TestFalse(TEXT("Slot 2 is not empty"), Options[2].bIsEmpty);
    TestFalse(TEXT("Slot 2 EquippedDataAsset is not null"), Options[2].EquippedDataAsset.IsNull());
    TestEqual(TEXT("Slot 2 EquippedDataAsset path matches installed asset"),
        Options[2].EquippedDataAsset.ToSoftObjectPath().ToString(),
        OccupiedRef.ToSoftObjectPath().ToString());

    // All other slots must be empty.
    for (int32 i = 0; i < 8; ++i)
    {
        if (i == 2) continue;
        TestTrue(FString::Printf(TEXT("Slot %d is empty"), i), Options[i].bIsEmpty);
        TestTrue(FString::Printf(TEXT("Slot %d EquippedDataAsset is null"), i), Options[i].EquippedDataAsset.IsNull());
    }

    return true;
}

/**
 * Verifies that calling BuildLootEntryViewModel and BuildTargetSlotOptions causes
 * no mutation to the container's loot entries or the trigger set component's slots.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUIModelBuildCausesNoMutationTest,
    "WTBR.CorpseLoot.UIModel.BuildCausesNoMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUIModelBuildCausesNoMutationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_UIModel_NoMutation"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("NoMutTriggerA")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(1, TEXT("NoMutTriggerB")));
    Container->InitializeCorpseLootContainer(Snapshots);

    UWTBRTriggerSetComponent* TriggerSet = NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    // Record pre-call state.
    const int32 EntryCountBefore          = Container->GetLootEntryCount();
    const bool bEntry0AvailableBefore     = Container->IsEntryValidForPickup(0);
    const bool bEntry1AvailableBefore     = Container->IsEntryValidForPickup(1);
    const bool bHasAvailableLootBefore    = Container->HasAvailableLootEntries();

    FWTBRInstalledTriggerSlotSnapshot SlotSnapshotBefore;
    const bool bSlot0Before = TriggerSet->GetTriggerSlotSnapshot(0, SlotSnapshotBefore);

    // Exercise both query paths.
    Container->BuildLootEntryViewModel(0);
    Container->BuildLootEntryViewModel(1);

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
    if (!DA) return false;
    DA->SlotConstraint = ETriggerSlotConstraint::Any;

    TArray<FWTBRTargetSlotOptionViewModel> Options;
    Container->BuildTargetSlotOptions(TriggerSet, DA, Options);
    Container->BuildTargetSlotOptionsForEntry(TriggerSet, 0, Options);

    // Verify no mutation.
    TestEqual(TEXT("Entry count unchanged after build"), Container->GetLootEntryCount(), EntryCountBefore);
    TestEqual(TEXT("Entry 0 availability unchanged"), Container->IsEntryValidForPickup(0), bEntry0AvailableBefore);
    TestEqual(TEXT("Entry 1 availability unchanged"), Container->IsEntryValidForPickup(1), bEntry1AvailableBefore);
    TestEqual(TEXT("Container HasAvailableLoot unchanged"), Container->HasAvailableLootEntries(), bHasAvailableLootBefore);

    FWTBRInstalledTriggerSlotSnapshot SlotSnapshotAfter;
    const bool bSlot0After = TriggerSet->GetTriggerSlotSnapshot(0, SlotSnapshotAfter);
    TestEqual(TEXT("TriggerSet slot 0 occupancy unchanged"), bSlot0After, bSlot0Before);

    return true;
}

// =============================================================================
// WTBR.CorpseLoot.Interact — Delegate Bridge Tests
//
// Covers the guard logic and no-mutation contract of
// UWTBRInteractionComponent::RequestCorpseLootInteract().
//
// The focused trace path (requires live world/camera/collision) is intentionally
// omitted — tests that depend on line trace results are brittle in headless
// automation. Trace integration is covered by the Human Test Gate.
// =============================================================================

/**
 * RequestCorpseLootInteract returns false immediately when the owning object
 * is not an AWTBRCharacter (null / non-character outer).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInteractReturnsFalseNoOwnerTest,
    "WTBR.CorpseLoot.Interact.ReturnsFalse_WhenOwnerIsNotCharacter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInteractReturnsFalseNoOwnerTest::RunTest(const FString& /*Parameters*/)
{
    // Component created with GetTransientPackage() as outer — GetOwner() returns
    // null, so Cast<AWTBRCharacter> fails and the function returns false before
    // the trace or delegate fire.
    UWTBRInteractionComponent* Comp =
        NewObject<UWTBRInteractionComponent>(GetTransientPackage());
    TestNotNull(TEXT("InteractionComponent created"), Comp);
    if (!Comp) return false;

    TestFalse(
        TEXT("RequestCorpseLootInteract returns false when owner is not a character"),
        Comp->RequestCorpseLootInteract());

    return true;
}

/**
 * Calling RequestCorpseLootInteract with a non-character owner must not mutate
 * LootEntries on any nearby container.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInteractLootEntriesUnchangedTest,
    "WTBR.CorpseLoot.Interact.LootEntriesUnchanged_AfterRequestWithNoOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInteractLootEntriesUnchangedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTransientWorldFixture WorldFixture(TEXT("WTBR_Interact_NoMutLoot"));
    UWorld* World = WorldFixture.GetWorld();
    TestNotNull(TEXT("Transient world available"), World);
    if (!World) return false;

    AWTBRCorpseLootContainerActor* Container = SpawnCorpseLootContainer(World);
    TestNotNull(TEXT("Container spawns"), Container);
    if (!Container) return false;

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Snapshots.Add(MakeInstalledTriggerSnapshot(0, TEXT("InteractMutTestA")));
    Snapshots.Add(MakeInstalledTriggerSnapshot(1, TEXT("InteractMutTestB")));
    Container->InitializeCorpseLootContainer(Snapshots);

    const int32  EntryCountBefore       = Container->GetLootEntryCount();
    const bool   bEntry0ValidBefore     = Container->IsEntryValidForPickup(0);
    const bool   bEntry1ValidBefore     = Container->IsEntryValidForPickup(1);
    const bool   bHasAvailableBefore    = Container->HasAvailableLootEntries();

    UWTBRInteractionComponent* Comp =
        NewObject<UWTBRInteractionComponent>(GetTransientPackage());
    if (!Comp) return false;

    Comp->RequestCorpseLootInteract(); // must be a no-op (null owner path)

    TestEqual(TEXT("Entry count unchanged"), Container->GetLootEntryCount(),    EntryCountBefore);
    TestEqual(TEXT("Entry 0 validity unchanged"), Container->IsEntryValidForPickup(0), bEntry0ValidBefore);
    TestEqual(TEXT("Entry 1 validity unchanged"), Container->IsEntryValidForPickup(1), bEntry1ValidBefore);
    TestEqual(TEXT("HasAvailableLoot unchanged"), Container->HasAvailableLootEntries(), bHasAvailableBefore);

    return true;
}

/**
 * Calling RequestCorpseLootInteract with a non-character owner must not mutate
 * any trigger slot on a TriggerSetComponent.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInteractTriggerSlotsUnchangedTest,
    "WTBR.CorpseLoot.Interact.TriggerSlotsUnchanged_AfterRequestWithNoOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInteractTriggerSlotsUnchangedTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRTriggerSetComponent* TriggerSet =
        NewObject<UWTBRTriggerSetComponent>(GetTransientPackage());
    TestNotNull(TEXT("TriggerSetComponent created"), TriggerSet);
    if (!TriggerSet) return false;

    // Record pre-call snapshots for all 8 slots.
    TArray<FWTBRInstalledTriggerSlotSnapshot> SnapshotsBefore;
    for (int32 i = 0; i < UWTBRTriggerSetComponent::TotalSlotCount; ++i)
    {
        FWTBRInstalledTriggerSlotSnapshot S;
        TriggerSet->GetTriggerSlotSnapshot(i, S);
        SnapshotsBefore.Add(S);
    }

    UWTBRInteractionComponent* Comp =
        NewObject<UWTBRInteractionComponent>(GetTransientPackage());
    if (!Comp) return false;

    Comp->RequestCorpseLootInteract(); // must be a no-op (null owner path)

    for (int32 i = 0; i < UWTBRTriggerSetComponent::TotalSlotCount; ++i)
    {
        FWTBRInstalledTriggerSlotSnapshot SAfter;
        const bool bHasAfter = TriggerSet->GetTriggerSlotSnapshot(i, SAfter);
        const bool bHasBefore = SnapshotsBefore[i].IsValid();
        TestEqual(FString::Printf(TEXT("Slot %d occupancy unchanged"), i), bHasAfter, bHasBefore);
    }

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
//  [ ] focused trace / RequestCorpseLootInteract with valid owner+container
//        - Requires character/controller/viewpoint/collision setup; brittle in
//          headless automation. Covered by Human Test Gate (prompt trace feel).
//  [ ] dedicated server / late join
//        - Requires multiplayer automation coverage outside this code-only file.
// -----------------------------------------------------------------------------

#endif // WITH_DEV_AUTOMATION_TESTS
