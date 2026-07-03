// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"                 // TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "Components/WTBRInteractionComponent.h"
#include "Components/WTBRHealthComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRArcvenTrigger.h"          // concrete UWTBRTriggerBase for TriggerClass (code-only)
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Data/WTBRMatchModeRulesDataAsset.h"   // FWTBRMatchModeRules

// -----------------------------------------------------------------------------
// S7A Dropped Trigger Context Interact Automation Tests
// (WTBR.DroppedTrigger.DeathDrop / .Interact)
//
// Code-only. Actor-level tests exercise the S7A context-interact routing on an
// authority character in a transient (EWorldType::Game) world, with an in-match
// AWTBRGameState registered as the world GameState. Detection/focus is driven off
// AWTBRCharacter::GetActorEyesViewPoint() (controllerless pawn), so no PlayerController
// or PIE camera is required. No Blueprint, WBP, UMG, .uasset, .umap, or production
// gameplay changes.
//
// Server-authoritative contract preserved: S7B tests split coverage into two
// deterministic phases. Phase A drives RequestContextInteract with a dev-only
// capture seam at the production routing point, proving the resolved dropped trigger
// and active target slot without executing the RPC wrapper or mutating gameplay state.
// Phase B independently drives the server RPC body
// Server_RequestPickupDroppedTrigger_Implementation to prove authoritative mutation
// semantics. Transient headless worlds do not provide reliable network RPC transport,
// so these tests do not claim network transport coverage. They use a code-only
// transient UWTBRTriggerDataAsset with a concrete TriggerClass (UWTBRArcvenTrigger)
// — still no assets, still server-authoritative.
//
// Deferred (reported near the end): the ground-item branch pickup through
// GetFocusedGroundItem's ECC_Visibility line trace (no initialized physics scene in a
// transient headless world — see the NoDroppedTriggerFocus test comment).
// -----------------------------------------------------------------------------

namespace
{
    // Authority character + world fixture. Mirrors the S6 pickup fixture pattern:
    // spawn-only, no BeginPlay/GameMode; the transient Game world is authority by
    // default (no net driver), which is all the server-side paths under test need.
    class FWTBRDroppedTriggerWorldFixture
    {
    public:
        explicit FWTBRDroppedTriggerWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRDroppedTriggerWorldFixture()
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

    // RAII int32 console-variable override, restored on scope exit. Mirrors the
    // float variant in WTBRCorpseLootAutomationTests.cpp.
    class FWTBRScopedIntCVarOverride
    {
    public:
        FWTBRScopedIntCVarOverride(const TCHAR* InCVarName, int32 NewValue)
        {
            CVar = IConsoleManager::Get().FindConsoleVariable(InCVarName);
            if (CVar)
            {
                OriginalValue = CVar->GetInt();
                CVar->Set(NewValue, ECVF_SetByCode);
            }
        }

        ~FWTBRScopedIntCVarOverride()
        {
            if (CVar)
            {
                CVar->Set(OriginalValue, ECVF_SetByCode);
            }
        }

    private:
        IConsoleVariable* CVar = nullptr;
        int32 OriginalValue = 0;
    };

    // Spawns an in-match GameState that permits corpse loot + trigger pickup and
    // uses the legacy (non-container) death-drop path, then registers it as the
    // world GameState (transient worlds have no GameMode to do this automatically).
    AWTBRGameState* SpawnLegacyLootInMatchGameState(UWorld* World)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (!GameState) return nullptr;

        World->SetGameState(GameState);

        FWTBRMatchModeRules Rules;
        Rules.bAllowCorpseLoot = true;
        Rules.bAllowTriggerPickup = true;
        Rules.bUseCorpseLootContainerOnDeath = false; // legacy dropped-trigger path
        GameState->SetCurrentMatchRules(Rules);
        GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
        return GameState;
    }

    // Spawns an in-match GameState that permits trigger pickup AND trigger swap during
    // match, registered as the world GameState. Server_RequestPickupDroppedTrigger and
    // CanMutateTriggerLoadout both require IsInMatch + AllowsTriggerPickup +
    // AllowsTriggerSwapDuringMatch for a pickup to actually mutate a slot.
    AWTBRGameState* SpawnTriggerSwapInMatchGameState(UWorld* World)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (!GameState) return nullptr;

        World->SetGameState(GameState);

        FWTBRMatchModeRules Rules;
        Rules.bAllowTriggerPickup = true;
        Rules.bAllowTriggerSwapDuringMatch = true;
        GameState->SetCurrentMatchRules(Rules);
        GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
        return GameState;
    }

    AWTBRCharacter* SpawnCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    // Transient trigger data asset. Non-null soft ptr (points at the transient
    // object), so it satisfies the "DataAsset not null" checks in the focus finders,
    // the installed-snapshot validity test, and the constraint resolver's load.
    UWTBRTriggerDataAsset* MakeTransientTriggerDataAsset(ETriggerSlotConstraint Constraint)
    {
        UWTBRTriggerDataAsset* Asset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        Asset->SlotConstraint = Constraint;
        return Asset;
    }

    // Transient trigger data asset with a valid (concrete, code-only) TriggerClass.
    // The server-side slot replacement (ReplaceTriggerSlotFromDataAsset) rejects a
    // DataAsset whose TriggerClass is null, and InstantiateRuntimeTrigger NewObjects
    // that class — so it must be a concrete UWTBRTriggerBase subclass. UWTBRArcvenTrigger
    // is a concrete C++ class (no override of InitializeTrigger), so no .uasset is needed.
    UWTBRTriggerDataAsset* MakeTransientPickableTriggerDataAsset(ETriggerSlotConstraint Constraint)
    {
        UWTBRTriggerDataAsset* Asset = MakeTransientTriggerDataAsset(Constraint);
        Asset->TriggerClass = UWTBRArcvenTrigger::StaticClass();
        return Asset;
    }

    // Spawns a dropped trigger initialized with the given (non-null) data asset.
    AWTBRDroppedTriggerActor* SpawnDroppedTrigger(
        UWorld* World, const TSoftObjectPtr<UWTBRTriggerDataAsset>& DataAsset, const FVector& Loc)
    {
        if (!World) return nullptr;
        AWTBRDroppedTriggerActor* Dropped = World->SpawnActor<AWTBRDroppedTriggerActor>(
            AWTBRDroppedTriggerActor::StaticClass(), Loc, FRotator::ZeroRotator);
        if (Dropped)
        {
            Dropped->InitializeDroppedTrigger(DataAsset, /*SourceSlotIndex*/ 0, ETriggerCategory::Melee);
        }
        return Dropped;
    }

    int32 CountDroppedTriggers(UWorld* World)
    {
        int32 Count = 0;
        if (!World) return 0;
        for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                ++Count;
            }
        }
        return Count;
    }

    // Places a location directly in front of the character's eyes (dot ~= 1.0, so it
    // clears the aim-dot threshold in both the component focus finder and the character
    // pickup finder). Keep Distance well under the character-side pickup range
    // (WTBRDroppedTriggerPickupRange, 300 units, measured from the actor location) —
    // that finder, not the wider 800-unit focus trace, is the binding constraint.
    FVector InFrontOfEyes(const AWTBRCharacter* Character, float Distance)
    {
        FVector EyeLoc = FVector::ZeroVector;
        FRotator EyeRot = FRotator::ZeroRotator;
        Character->GetActorEyesViewPoint(EyeLoc, EyeRot);
        return EyeLoc + EyeRot.Vector() * Distance;
    }
}

// ── Test 1 — Death drop spawns dropped triggers while in match ───────────────
//
// WTBR.DroppedTrigger.DeathDrop.SpawnsInMatch
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerDeathDropSpawnsInMatchTest,
    "WTBR.DroppedTrigger.DeathDrop.SpawnsInMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerDeathDropSpawnsInMatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_DeathDrop"));
    UWorld* World = Fixture.GetWorld();

    // Force the legacy death-drop path explicitly (CVar == 0 → never container path),
    // independent of the GameState rule, per the S7A-Auto arrange contract.
    FWTBRScopedIntCVarOverride ForceLegacy(TEXT("WTBR.UseCorpseLootContainerOnDeath"), 0);

    AWTBRGameState* GameState = SpawnLegacyLootInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;
    TestTrue(TEXT("IsInMatch is true"), GameState->IsInMatch());
    TestTrue(TEXT("Match allows corpse loot"), GameState->AllowsCorpseLoot());
    TestTrue(TEXT("Match allows trigger pickup"), GameState->AllowsTriggerPickup());

    AWTBRCharacter* Victim = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Victim spawns"), Victim);
    if (!Victim || !Victim->TriggerSetComponent) return false;

    // Ensure installed trigger snapshots > 0 without runtime instantiation or assets:
    // seed a slot with a transient (non-null) data asset via the test-only seam.
    TStrongObjectPtr<UWTBRTriggerDataAsset> InstalledAsset(
        MakeTransientTriggerDataAsset(ETriggerSlotConstraint::MainOnly));
    Victim->TriggerSetComponent->SetSlotDataAssetForTest(
        0, TSoftObjectPtr<UWTBRTriggerDataAsset>(InstalledAsset.Get()));

    TArray<FWTBRInstalledTriggerSlotSnapshot> Snapshots;
    Victim->TriggerSetComponent->GetInstalledTriggerSlotSnapshots(Snapshots);
    TestTrue(TEXT("Victim has installed trigger snapshots > 0"), Snapshots.Num() > 0);

    UWTBRHealthComponent* Health = Victim->FindComponentByClass<UWTBRHealthComponent>();
    TestNotNull(TEXT("Victim has a health component"), Health);
    if (!Health) return false;

    const int32 DroppedBefore = CountDroppedTriggers(World);
    TestEqual(TEXT("No dropped triggers before death"), DroppedBefore, 0);

    // Act — run the legacy death-drop path (server-authoritative; victim holds
    // authority). Uses the test-only seam that invokes the eliminated-character drop
    // routine directly: the full damage→downed→eliminated pipeline is timer/CoreStats
    // dependent and not headless-safe, so it is not driven here.
    Health->SpawnDroppedTriggersForEliminatedCharacterForTest();

    const int32 DroppedAfter = CountDroppedTriggers(World);
    TestTrue(TEXT("Dropped trigger count after death is > 0"), DroppedAfter > 0);
    return true;
}

// ── Test 2 — Context-interact detection finds the dropped trigger ────────────
//
// WTBR.DroppedTrigger.Interact.FindsDroppedTrigger
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerInteractFindsTest,
    "WTBR.DroppedTrigger.Interact.FindsDroppedTrigger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerInteractFindsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_Finds"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLegacyLootInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InteractionComponent) return false;

    // The Any-constraint route logs an AmbiguousTargetSlot warning when it detects +
    // routes the trigger (target slot policy is Any); consume the one expected warning.
    AddExpectedError(TEXT("AmbiguousTargetSlot"), EAutomationExpectedErrorFlags::Contains, 1);

    // Dropped trigger placed directly in front of the character's eyes so the S7A
    // detection path (dot >= 0.5, within range) sees it.
    TStrongObjectPtr<UWTBRTriggerDataAsset> Asset(
        MakeTransientTriggerDataAsset(ETriggerSlotConstraint::Any));
    AWTBRDroppedTriggerActor* Dropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(Asset.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Dropped trigger spawns"), Dropped);
    if (!Dropped) return false;

    // Detection is exercised through the public context-interact path (the same one
    // IA_Interact drives) — not a legacy direct-pickup shortcut. With no corpse and no
    // ground item present, the only focusable actor is the dropped trigger, so a
    // handled==true result is attributable to the S7A dropped-trigger branch (priority
    // 2) having detected it. Slot policy is Any, so no pickup/mutation is required.
    const bool bFoundAndRouted = Character->InteractionComponent->RequestContextInteract();
    TestTrue(TEXT("Context interact detects and routes to the dropped trigger"), bFoundAndRouted);

    // Control: once the trigger is consumed it is no longer detectable, so the same
    // call is no longer handled — confirming detection (not another branch) drove the
    // result above.
    TestTrue(TEXT("Mark dropped trigger consumed"), Dropped->TryMarkConsumed());
    const bool bHandledAfterConsumed = Character->InteractionComponent->RequestContextInteract();
    TestFalse(TEXT("Consumed dropped trigger is no longer detected/routed"), bHandledAfterConsumed);
    return true;
}

// ── Test 3 — Any constraint rejects as AmbiguousTargetSlot (no mutation) ─────
//
// WTBR.DroppedTrigger.Interact.AnyRejectsAmbiguousTargetSlot
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerInteractAnyRejectsTest,
    "WTBR.DroppedTrigger.Interact.AnyRejectsAmbiguousTargetSlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerInteractAnyRejectsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_AnyRejects"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLegacyLootInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InteractionComponent || !Character->TriggerSetComponent) return false;

    // No public result enum is exposed; the resolver rejects Any via a Warning log
    // + early return. Consume the expected warning and assert the externally visible
    // effects: actor remains unconsumed, active slots unchanged, no dispatch/mutation.
    AddExpectedError(TEXT("AmbiguousTargetSlot"), EAutomationExpectedErrorFlags::Contains, 1);

    TStrongObjectPtr<UWTBRTriggerDataAsset> Asset(
        MakeTransientTriggerDataAsset(ETriggerSlotConstraint::Any));
    AWTBRDroppedTriggerActor* Dropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(Asset.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Dropped trigger spawns"), Dropped);
    if (!Dropped) return false;
    TestTrue(TEXT("Dropped trigger data asset resolves to Any"),
        Asset->SlotConstraint == ETriggerSlotConstraint::Any);

    const int32 MainBefore = Character->TriggerSetComponent->GetActiveMainIndex();
    const int32 SubBefore = Character->TriggerSetComponent->GetActiveSubIndex();

    // Act — same context-interact path as IA_Interact.
    const bool bHandled = Character->InteractionComponent->RequestContextInteract();

    // Routed to the dropped-trigger branch (handled), but rejected inside the resolver.
    TestTrue(TEXT("Interact routed to dropped-trigger branch"), bHandled);
    TestFalse(TEXT("Dropped trigger remains unconsumed"), Dropped->IsConsumed());
    TestTrue(TEXT("Dropped trigger actor still valid (not picked up)"), IsValid(Dropped));
    TestEqual(TEXT("Active main slot unchanged"),
        Character->TriggerSetComponent->GetActiveMainIndex(), MainBefore);
    TestEqual(TEXT("Active sub slot unchanged"),
        Character->TriggerSetComponent->GetActiveSubIndex(), SubBefore);
    return true;
}

// ── Test 4 — No dropped-trigger focus does not consume the interact ──────────
//
// WTBR.DroppedTrigger.Interact.NoDroppedTriggerFocusDoesNotConsumeInteract
//
// Deterministic regression guard, scoped exactly to what it can prove headlessly:
// with no corpse focus and no dropped-trigger focus, the S7A priority-2 branch must
// NOT consume the interact — RequestContextInteract falls through past it and returns
// false (no-op) rather than being wrongly handled at priority 2. This proves the S7A
// insertion did not regress branch precedence when there is nothing for priority 2 to
// pick up. It intentionally does NOT assert the ground-item pickup itself (see TODO).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerInteractNoFocusDoesNotConsumeTest,
    "WTBR.DroppedTrigger.Interact.NoDroppedTriggerFocusDoesNotConsumeInteract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerInteractNoFocusDoesNotConsumeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_NoFocusNoConsume"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLegacyLootInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InteractionComponent) return false;

    // No corpse/container/chest focus and no dropped-trigger focus present. With
    // nothing for priority 2 to pick up, the S7A dropped-trigger branch must not
    // consume the interact; RequestContextInteract falls through (returns false = no-op).
    const bool bHandled = Character->InteractionComponent->RequestContextInteract();
    TestFalse(TEXT("Interact not consumed by dropped-trigger branch when no dropped trigger is focused"), bHandled);

    // TODO — DEFERRED: full branch-3 ground-item pickup through RequestContextInteract.
    //  * Not asserted here: that RequestContextInteract dispatches Server_RequestPickupGroundItem
    //    and completes a pickup. That requires a reliable ECC_Visibility trace, i.e. a
    //    physics-backed PIE / fixture world; the transient headless UWorld used here has no
    //    initialized physics scene, so GetFocusedGroundItem()'s line trace cannot be relied
    //    upon to hit. Deferred to a PIE / Human Test Gate pass (no risky map/asset fixture
    //    is introduced in this pass by design).
    //  * Server_RequestPickupGroundItem pickup semantics remain covered headlessly by the
    //    existing S6 WTBR.Inventory.Pickup.* tests, which drive the RPC implementation directly.
    //  * This test only proves that dropped-trigger priority-2 does NOT consume the interact
    //    when no dropped trigger is focused (branch precedence is not regressed).
    return true;
}

// ── Test 5 — MainOnly dropped trigger dispatches into the active main slot ────
//
// WTBR.DroppedTrigger.Interact.MainOnlyDispatchesToActiveMain
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerInteractMainOnlyTest,
    "WTBR.DroppedTrigger.Interact.MainOnlyDispatchesToActiveMain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerInteractMainOnlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_MainOnly"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTriggerSwapInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InteractionComponent || !Character->TriggerSetComponent) return false;

    UWTBRTriggerSetComponent* Triggers = Character->TriggerSetComponent;
    const int32 MainIndex = Triggers->GetActiveMainIndex();   // default 0 (a main slot)
    const int32 SubIndex  = Triggers->GetActiveSubIndex();     // default 4 (a sub slot)
    TestTrue(TEXT("Active main index is a main slot (0..3)"), MainIndex >= 0 && MainIndex < 4);
    TestTrue(TEXT("Active sub index is a sub slot (4..7)"), SubIndex >= 4 && SubIndex < 8);

    // Pre-seed both active slots with distinct markers so we can prove exactly one changed.
    TStrongObjectPtr<UWTBRTriggerDataAsset> MainMarker(MakeTransientTriggerDataAsset(ETriggerSlotConstraint::MainOnly));
    TStrongObjectPtr<UWTBRTriggerDataAsset> SubMarker(MakeTransientTriggerDataAsset(ETriggerSlotConstraint::SubOnly));
    Triggers->SetSlotDataAssetForTest(MainIndex, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainMarker.Get()));
    Triggers->SetSlotDataAssetForTest(SubIndex,  TSoftObjectPtr<UWTBRTriggerDataAsset>(SubMarker.Get()));

    // MainOnly dropped trigger (concrete TriggerClass so the server-side replace succeeds).
    TStrongObjectPtr<UWTBRTriggerDataAsset> Picked(
        MakeTransientPickableTriggerDataAsset(ETriggerSlotConstraint::MainOnly));
    AWTBRDroppedTriggerActor* Dropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(Picked.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Dropped trigger spawns"), Dropped);
    if (!Dropped) return false;

    // Phase A — context routing proof. Capture-only mode records the production
    // resolver's pending RPC target and prevents the RPC wrapper from executing in
    // this transient headless world, so RequestContextInteract remains testable
    // without mutation.
    Character->SetDroppedTriggerRouteCaptureForTest(true);
    const bool bHandled = Character->InteractionComponent->RequestContextInteract();
    TestTrue(TEXT("Context interact handled by dropped-trigger branch (MainOnly)"), bHandled);
    TestTrue(TEXT("MainOnly route was captured"), Character->WasDroppedTriggerRouteCapturedForTest());
    TestTrue(TEXT("Captured trigger is the focused dropped trigger"),
        Character->GetCapturedDroppedTriggerForTest() == Dropped);
    TestEqual(TEXT("MainOnly resolves to active main slot"),
        Character->GetCapturedDroppedTriggerSlotForTest(), MainIndex);
    TestTrue(TEXT("Captured policy is MainOnly"),
        Character->GetCapturedDroppedTriggerConstraintForTest() == ETriggerSlotConstraint::MainOnly);
    TestTrue(TEXT("Dropped trigger remains valid during capture-only routing"), IsValid(Dropped));
    TestFalse(TEXT("Dropped trigger remains unconsumed during capture-only routing"), Dropped->IsConsumed());

    FWTBRInstalledTriggerSlotSnapshot MainAfterCapture;
    FWTBRInstalledTriggerSlotSnapshot SubAfterCapture;
    TestTrue(TEXT("Active main snapshot readable after capture"), Triggers->GetTriggerSlotSnapshot(MainIndex, MainAfterCapture));
    TestTrue(TEXT("Active sub snapshot readable after capture"), Triggers->GetTriggerSlotSnapshot(SubIndex, SubAfterCapture));
    TestTrue(TEXT("Active main unchanged during capture"), MainAfterCapture.DataAsset.Get() == MainMarker.Get());
    TestTrue(TEXT("Active sub unchanged during capture"), SubAfterCapture.DataAsset.Get() == SubMarker.Get());
    Character->SetDroppedTriggerRouteCaptureForTest(false);

    // Phase B — server-authoritative mutation proof. Use a fresh dropped trigger
    // and call the server RPC body directly because transient headless automation
    // has no reliable network RPC transport. This proves server-side slot
    // compatibility, rollback, replacement, and destruction semantics only.
    TStrongObjectPtr<UWTBRTriggerDataAsset> PickedForServer(
        MakeTransientPickableTriggerDataAsset(ETriggerSlotConstraint::MainOnly));
    AWTBRDroppedTriggerActor* ServerDropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(PickedForServer.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Fresh MainOnly dropped trigger spawns for server body phase"), ServerDropped);
    if (!ServerDropped) return false;

    AddExpectedError(TEXT("WTBR trigger pickup replacement rejected"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("WTBR dropped trigger pickup rejected: replacement failed"), EAutomationExpectedErrorFlags::Contains, 1);
    Character->Server_RequestPickupDroppedTrigger_Implementation(ServerDropped, SubIndex);
    TestTrue(TEXT("MainOnly rejected at a sub slot: dropped trigger still present"), IsValid(ServerDropped));
    if (IsValid(ServerDropped))
    {
        TestFalse(TEXT("MainOnly rejected at a sub slot: not consumed (rolled back)"), ServerDropped->IsConsumed());
    }

    Character->Server_RequestPickupDroppedTrigger_Implementation(ServerDropped, MainIndex);
    TestFalse(TEXT("Dropped trigger destroyed after successful pickup into active main"), IsValid(ServerDropped));

    // Correct slot changed: active main now holds the picked asset; active sub is unchanged.
    FWTBRInstalledTriggerSlotSnapshot MainSnap;
    FWTBRInstalledTriggerSlotSnapshot SubSnap;
    TestTrue(TEXT("Active main slot snapshot readable"), Triggers->GetTriggerSlotSnapshot(MainIndex, MainSnap));
    TestTrue(TEXT("Active sub slot snapshot readable"), Triggers->GetTriggerSlotSnapshot(SubIndex, SubSnap));
    TestTrue(TEXT("Active main slot now holds the picked-up trigger"), MainSnap.DataAsset.Get() == PickedForServer.Get());
    TestTrue(TEXT("Active sub slot unchanged (still its marker)"), SubSnap.DataAsset.Get() == SubMarker.Get());
    return true;
}

// ── Test 6 — SubOnly dropped trigger dispatches into the active sub slot ──────
//
// WTBR.DroppedTrigger.Interact.SubOnlyDispatchesToActiveSub
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDroppedTriggerInteractSubOnlyTest,
    "WTBR.DroppedTrigger.Interact.SubOnlyDispatchesToActiveSub",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDroppedTriggerInteractSubOnlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRDroppedTriggerWorldFixture Fixture(TEXT("WTBR_DT_SubOnly"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTriggerSwapInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InteractionComponent || !Character->TriggerSetComponent) return false;

    UWTBRTriggerSetComponent* Triggers = Character->TriggerSetComponent;
    const int32 MainIndex = Triggers->GetActiveMainIndex();   // default 0 (a main slot)
    const int32 SubIndex  = Triggers->GetActiveSubIndex();     // default 4 (a sub slot)
    TestTrue(TEXT("Active main index is a main slot (0..3)"), MainIndex >= 0 && MainIndex < 4);
    TestTrue(TEXT("Active sub index is a sub slot (4..7)"), SubIndex >= 4 && SubIndex < 8);

    // Pre-seed both active slots with distinct markers so we can prove exactly one changed.
    TStrongObjectPtr<UWTBRTriggerDataAsset> MainMarker(MakeTransientTriggerDataAsset(ETriggerSlotConstraint::MainOnly));
    TStrongObjectPtr<UWTBRTriggerDataAsset> SubMarker(MakeTransientTriggerDataAsset(ETriggerSlotConstraint::SubOnly));
    Triggers->SetSlotDataAssetForTest(MainIndex, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainMarker.Get()));
    Triggers->SetSlotDataAssetForTest(SubIndex,  TSoftObjectPtr<UWTBRTriggerDataAsset>(SubMarker.Get()));

    // SubOnly dropped trigger (concrete TriggerClass so the server-side replace succeeds).
    TStrongObjectPtr<UWTBRTriggerDataAsset> Picked(
        MakeTransientPickableTriggerDataAsset(ETriggerSlotConstraint::SubOnly));
    AWTBRDroppedTriggerActor* Dropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(Picked.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Dropped trigger spawns"), Dropped);
    if (!Dropped) return false;

    // Phase A — context routing proof. Capture-only mode records the production
    // resolver's pending RPC target and prevents the RPC wrapper from executing in
    // this transient headless world, so RequestContextInteract remains testable
    // without mutation.
    Character->SetDroppedTriggerRouteCaptureForTest(true);
    const bool bHandled = Character->InteractionComponent->RequestContextInteract();
    TestTrue(TEXT("Context interact handled by dropped-trigger branch (SubOnly)"), bHandled);
    TestTrue(TEXT("SubOnly route was captured"), Character->WasDroppedTriggerRouteCapturedForTest());
    TestTrue(TEXT("Captured trigger is the focused dropped trigger"),
        Character->GetCapturedDroppedTriggerForTest() == Dropped);
    TestEqual(TEXT("SubOnly resolves to active sub slot"),
        Character->GetCapturedDroppedTriggerSlotForTest(), SubIndex);
    TestTrue(TEXT("Captured policy is SubOnly"),
        Character->GetCapturedDroppedTriggerConstraintForTest() == ETriggerSlotConstraint::SubOnly);
    TestTrue(TEXT("Dropped trigger remains valid during capture-only routing"), IsValid(Dropped));
    TestFalse(TEXT("Dropped trigger remains unconsumed during capture-only routing"), Dropped->IsConsumed());

    FWTBRInstalledTriggerSlotSnapshot MainAfterCapture;
    FWTBRInstalledTriggerSlotSnapshot SubAfterCapture;
    TestTrue(TEXT("Active main snapshot readable after capture"), Triggers->GetTriggerSlotSnapshot(MainIndex, MainAfterCapture));
    TestTrue(TEXT("Active sub snapshot readable after capture"), Triggers->GetTriggerSlotSnapshot(SubIndex, SubAfterCapture));
    TestTrue(TEXT("Active main unchanged during capture"), MainAfterCapture.DataAsset.Get() == MainMarker.Get());
    TestTrue(TEXT("Active sub unchanged during capture"), SubAfterCapture.DataAsset.Get() == SubMarker.Get());
    Character->SetDroppedTriggerRouteCaptureForTest(false);

    // Phase B — server-authoritative mutation proof. Use a fresh dropped trigger
    // and call the server RPC body directly because transient headless automation
    // has no reliable network RPC transport. This proves server-side slot
    // compatibility, rollback, replacement, and destruction semantics only.
    TStrongObjectPtr<UWTBRTriggerDataAsset> PickedForServer(
        MakeTransientPickableTriggerDataAsset(ETriggerSlotConstraint::SubOnly));
    AWTBRDroppedTriggerActor* ServerDropped = SpawnDroppedTrigger(
        World, TSoftObjectPtr<UWTBRTriggerDataAsset>(PickedForServer.Get()), InFrontOfEyes(Character, 150.0f));
    TestNotNull(TEXT("Fresh SubOnly dropped trigger spawns for server body phase"), ServerDropped);
    if (!ServerDropped) return false;

    AddExpectedError(TEXT("WTBR trigger pickup replacement rejected"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("WTBR dropped trigger pickup rejected: replacement failed"), EAutomationExpectedErrorFlags::Contains, 1);
    Character->Server_RequestPickupDroppedTrigger_Implementation(ServerDropped, MainIndex);
    TestTrue(TEXT("SubOnly rejected at a main slot: dropped trigger still present"), IsValid(ServerDropped));
    if (IsValid(ServerDropped))
    {
        TestFalse(TEXT("SubOnly rejected at a main slot: not consumed (rolled back)"), ServerDropped->IsConsumed());
    }

    AddExpectedError(TEXT("WTBR GetActiveMainTrigger: slot 0 RuntimeTrigger is nullptr"), EAutomationExpectedErrorFlags::Contains, 1);
    Character->Server_RequestPickupDroppedTrigger_Implementation(ServerDropped, SubIndex);
    TestFalse(TEXT("Dropped trigger destroyed after successful pickup into active sub"), IsValid(ServerDropped));

    // Correct slot changed: active sub now holds the picked asset; active main is unchanged.
    FWTBRInstalledTriggerSlotSnapshot MainSnap;
    FWTBRInstalledTriggerSlotSnapshot SubSnap;
    TestTrue(TEXT("Active main slot snapshot readable"), Triggers->GetTriggerSlotSnapshot(MainIndex, MainSnap));
    TestTrue(TEXT("Active sub slot snapshot readable"), Triggers->GetTriggerSlotSnapshot(SubIndex, SubSnap));
    TestTrue(TEXT("Active sub slot now holds the picked-up trigger"), SubSnap.DataAsset.Get() == PickedForServer.Get());
    TestTrue(TEXT("Active main slot unchanged (still its marker)"), MainSnap.DataAsset.Get() == MainMarker.Get());
    return true;
}

// -----------------------------------------------------------------------------
// Deferred / not covered here (reported, no assets created or modified):
//
// * Ground-item pickup through RequestContextInteract's focus trace — see the TODO in
//   WTBR.DroppedTrigger.Interact.NoDroppedTriggerFocusDoesNotConsumeInteract.
//
// MainOnly/SubOnly constraint dispatch is now covered (S7B):
//   WTBR.DroppedTrigger.Interact.MainOnlyDispatchesToActiveMain and .SubOnlyDispatchesToActiveSub
//   prove RequestContextInteract slot routing with a capture-only dev automation seam,
//   then separately prove server implementation semantics by directly calling
//   Server_RequestPickupDroppedTrigger_Implementation. They do not claim network RPC
//   transport coverage. Both phases use code-only transient UWTBRTriggerDataAssets with
//   a concrete TriggerClass (UWTBRArcvenTrigger) — no assets.
// -----------------------------------------------------------------------------

#endif // WITH_DEV_AUTOMATION_TESTS
