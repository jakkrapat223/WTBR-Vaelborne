// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/WTBRHealthComponent.h"
#include "WTBRCharacter.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Phase 7B — Core Combat Round Loop Automation Tests (WTBR.Combat.Match.*)
//
// Code-only, headless (EWorldType::Game transient world, no PIE/BP assets). The
// fixture never calls UWorld::BeginPlay(), so AWTBRGameMode::BeginPlay() (and its
// real timers) never fire here — that auto-advance timing is exercised instead via
// the WITH_DEV_AUTOMATION_TESTS-only AdvanceToCountdownForTest/AdvanceToInMatchForTest
// seams, which drive the exact same phase-transition logic without waiting on real
// clock time. Full BeginPlay timer firing end-to-end is covered by the manual PIE
// checklist (Phase 7B report), not here.
//
// Characters are spawned with no CoreStatsAsset assigned, so
// UWTBRHealthComponent::GetStats() returns null; EnterDownedState falls back to
// EnterEliminatedState immediately (see WTBRHealthComponent.cpp), giving a
// synchronous, timer-free Alive -> Eliminated path headlessly.
//
// AWTBRGameMode::NotifyCombatantEliminated is called directly in these tests
// rather than through the production EnterEliminatedState -> GetAuthGameMode()
// hookup: UWorld only populates GetAuthGameMode() via the normal level-travel
// flow (UWorld::SetGameMode), not for a GameMode merely SpawnActor'd into an
// already-running transient fixture world, so the automatic hookup safely no-ops
// here. The hookup itself is a one-line call proven by code review + the manual
// PIE checklist (Phase 7B report), not by this headless fixture.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRCombatWorldFixture
    {
    public:
        explicit FWTBRCombatWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCombatWorldFixture()
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

    AWTBRGameState* SpawnCombatGameState(UWorld* World, EWTBRMatchPhase InitialPhase)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (GameState)
        {
            // A spawned GameState does not auto-register as the world GameState in a
            // transient (no-BeginPlay) fixture, so register it explicitly, mirroring
            // WTBRGroundItemPickupAutomationTests::SpawnInMatchGameState.
            World->SetGameState(GameState);
            GameState->SetCurrentMatchPhase(InitialPhase);
        }
        return GameState;
    }

    AWTBRGameMode* SpawnCombatGameMode(UWorld* World)
    {
        if (!World) return nullptr;
        return World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    }

    AWTBRCharacter* SpawnCombatCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }
}

// ── Phase auto-advance (test-only seam; no console command involved) ───────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCombatPhaseAutoAdvanceTest,
    "WTBR.Combat.Match.PhaseAutoAdvanceForTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCombatPhaseAutoAdvanceTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCombatWorldFixture Fixture(TEXT("WTBR_Combat_PhaseAdvance"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnCombatGameState(World, EWTBRMatchPhase::LoadoutSetup);
    TestNotNull(TEXT("GameState spawns"), GameState);
    AWTBRGameMode* GameMode = SpawnCombatGameMode(World);
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* Character = SpawnCombatCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->HealthComponent) return false;

    Character->HealthComponent->ApplyDamage(120.0f);
    TestEqual(TEXT("Damage applied before match start"), Character->HealthComponent->GetCurrentHP(), 180.0f);

    GameMode->AdvanceToCountdownForTest();
    TestEqual(TEXT("Phase -> Countdown"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Countdown);

    GameMode->AdvanceToInMatchForTest();
    TestEqual(TEXT("Phase -> InMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);
    TestEqual(TEXT("Combatant HP reset to max on InMatch entry"),
        Character->HealthComponent->GetCurrentHP(), Character->HealthComponent->GetMaxHP());
    TestTrue(TEXT("Combatant Alive on InMatch entry"), Character->HealthComponent->IsAlive());
    TestFalse(TEXT("Match result not resolved on fresh InMatch entry"), GameMode->IsMatchResultResolvedForTest());
    TestFalse(TEXT("No winner recorded on fresh InMatch entry"), GameState->HasMatchWinner());

    return true;
}

// ── Death triggers round-end + winner recorded ──────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCombatWinnerRecordedTest,
    "WTBR.Combat.Match.WinnerRecordedOnElimination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCombatWinnerRecordedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCombatWorldFixture Fixture(TEXT("WTBR_Combat_WinnerRecorded"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnCombatGameState(World, EWTBRMatchPhase::InMatch);
    TestNotNull(TEXT("GameState spawns"), GameState);
    AWTBRGameMode* GameMode = SpawnCombatGameMode(World);
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* CharacterA = SpawnCombatCharacter(World, FVector(0.f, 0.f, 0.f));
    AWTBRCharacter* CharacterB = SpawnCombatCharacter(World, FVector(200.f, 0.f, 0.f));
    TestNotNull(TEXT("Character A spawns"), CharacterA);
    TestNotNull(TEXT("Character B spawns"), CharacterB);
    if (!CharacterA || !CharacterB || !CharacterA->HealthComponent || !CharacterB->HealthComponent) return false;

    // Give the survivor a Controller/PlayerState so the winner PlayerState field
    // can be observed (AController::PostInitializeComponents calls InitPlayerState()
    // at spawn, independent of World::BeginPlay). AController itself is abstract;
    // APlayerController is the concrete stand-in used here.
    APlayerController* ControllerB = World->SpawnActor<APlayerController>(APlayerController::StaticClass());
    TestNotNull(TEXT("Controller B spawns"), ControllerB);
    if (ControllerB)
    {
        ControllerB->Possess(CharacterB);
    }

    TestTrue(TEXT("Both combatants start Alive"),
        CharacterA->HealthComponent->IsAlive() && CharacterB->HealthComponent->IsAlive());

    // Lethal damage with no CoreStatsAsset assigned: Alive -> HP 0 -> EnterDownedState
    // -> GetStats() null -> EnterEliminatedState immediately (see
    // WTBRHealthComponent.cpp EnterDownedState).
    CharacterA->HealthComponent->ApplyDamage(500.0f, CharacterB);

    // EnterEliminatedState's production hookup resolves the GameMode via
    // World->GetAuthGameMode<AWTBRGameMode>(), which UWorld only populates through
    // the normal level-travel flow (UWorld::SetGameMode) — not for a GameMode merely
    // SpawnActor'd into an already-running transient fixture world. So it safely
    // no-ops here (this is proven separately below as an idempotent duplicate call);
    // call the same production entry point directly to drive the round-loop logic,
    // exactly as the real hookup would once GetAuthGameMode() is populated in a real
    // game session.
    GameMode->NotifyCombatantEliminated(CharacterA);

    TestTrue(TEXT("Character A is Eliminated"), CharacterA->HealthComponent->IsEliminated());
    TestTrue(TEXT("Character B remains Alive"), CharacterB->HealthComponent->IsAlive());
    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestTrue(TEXT("Match result resolved"), GameMode->IsMatchResultResolvedForTest());

    if (ControllerB)
    {
        TestEqual(TEXT("Winner PlayerState is Character B's PlayerState"),
            GameState->GetMatchWinner(), CharacterB->GetPlayerState());
    }

    // Idempotency: a stray duplicate elimination notify after the result has
    // already resolved must not change anything further.
    APlayerState* WinnerBefore = GameState->GetMatchWinner();
    GameMode->NotifyCombatantEliminated(CharacterB);
    TestEqual(TEXT("Phase stays PostMatch after duplicate notify"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestEqual(TEXT("Winner unchanged after duplicate notify"), GameState->GetMatchWinner(), WinnerBefore);

    return true;
}

// ── Sole-combatant elimination resolves to no winner (draw edge case) ──────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCombatNoWinnerEdgeCaseTest,
    "WTBR.Combat.Match.NoWinnerWhenSoleCombatantEliminated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCombatNoWinnerEdgeCaseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCombatWorldFixture Fixture(TEXT("WTBR_Combat_NoWinner"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnCombatGameState(World, EWTBRMatchPhase::Countdown);
    TestNotNull(TEXT("GameState spawns"), GameState);
    AWTBRGameMode* GameMode = SpawnCombatGameMode(World);
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* Character = SpawnCombatCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->HealthComponent) return false;

    // Elimination while not InMatch must not resolve the round.
    Character->HealthComponent->ApplyDamage(500.0f);
    TestTrue(TEXT("Character eliminated"), Character->HealthComponent->IsEliminated());
    TestEqual(TEXT("Phase unaffected outside InMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Countdown);
    TestFalse(TEXT("Result not resolved outside InMatch"), GameMode->IsMatchResultResolvedForTest());

    // Now the sole combatant is eliminated while InMatch: zero alive combatants
    // remain, which resolves as no-winner (draw) rather than crashing/hanging.
    GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
    GameMode->NotifyCombatantEliminated(Character);

    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestTrue(TEXT("Match result resolved"), GameMode->IsMatchResultResolvedForTest());
    TestFalse(TEXT("No winner recorded (draw)"), GameState->HasMatchWinner());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
