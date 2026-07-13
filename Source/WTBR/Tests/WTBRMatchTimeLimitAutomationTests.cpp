// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/WTBRHealthComponent.h"
#include "WTBRCharacter.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Match Time-Limit Automation Tests (WTBR.Combat.Team.TimeLimit.*)
//
// Mode design lock 2026-07-13: TeamThree15P force-ends the round at a time
// limit even if MORE than one team still has a living member (e.g. 3 teams
// left) — it does not wait for last-team-standing. At that point every team
// still holding a living member banks its survivor bonus (generalizing the
// old single-surviving-team case), and the winner is still the highest total
// score. Same headless conventions as WTBRTeamMatchAutomationTests.cpp; real
// FTimerManager timers don't fire here, so the expiry is driven via the
// ForceMatchTimeLimitExpiredForTest seam, which calls the exact production
// handler.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRTimeLimitWorldFixture
    {
    public:
        explicit FWTBRTimeLimitWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRTimeLimitWorldFixture()
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

    AWTBRGameState* SpawnTimeLimitGameState(UWorld* World, const FWTBRMatchModeRules& Rules, EWTBRMatchPhase InitialPhase)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (GameState)
        {
            World->SetGameState(GameState);
            GameState->SetCurrentMatchRules(Rules);
            GameState->SetCurrentMatchPhase(InitialPhase);
        }
        return GameState;
    }

    AWTBRGameMode* SpawnTimeLimitGameMode(UWorld* World)
    {
        if (!World) return nullptr;
        return World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    }

    AWTBRCharacter* SpawnTimeLimitCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    FWTBRMatchModeRules Make15PRules(float TimeLimitSeconds)
    {
        FWTBRMatchModeRules Rules;
        Rules.MatchMode = EWTBRMatchMode::TeamThree15P;
        Rules.TeamSize = 3;
        Rules.MaxPlayers = 15;
        Rules.bUseTeams = true;
        Rules.bAssignTeamsAtMatchStart = true;
        Rules.bScoreBasedTeamWinner = true;
        Rules.MatchTimeLimitSeconds = TimeLimitSeconds;
        return Rules;
    }

    void TimeLimitTest_EliminateBy(AWTBRGameMode* GameMode, AWTBRCharacter* Victim, AWTBRCharacter* Killer)
    {
        Victim->HealthComponent->ApplyDamage(10000.0f, Killer);
        GameMode->NotifyCombatantEliminated(Victim, Killer);
    }
}

// ── Production default: 15P ships with a positive time limit ───────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimit15PDefaultTest,
    "WTBR.Combat.Team.TimeLimit.FifteenPDefaultHasPositiveLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimit15PDefaultTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRMatchModeRules Rules = AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::TeamThree15P);
    TestTrue(TEXT("15P default ships a positive match time limit"), Rules.MatchTimeLimitSeconds > 0.0f);

    const FWTBRMatchModeRules BRRules = AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::BattleRoyale);
    TestEqual(TEXT("BR default has no time limit (relies on zone)"), BRRules.MatchTimeLimitSeconds, 0.0f);

    return true;
}

// ── Timer arms at InMatch entry, clears on a natural (last-team-standing) end ──

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimitArmAndClearOnNaturalEndTest,
    "WTBR.Combat.Team.TimeLimit.TimerArmsAndClearsOnNaturalEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimitArmAndClearOnNaturalEndTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTimeLimitWorldFixture Fixture(TEXT("WTBR_TimeLimit_ArmClear"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTimeLimitGameState(World, Make15PRules(600.0f), EWTBRMatchPhase::Countdown);
    AWTBRGameMode* GameMode = SpawnTimeLimitGameMode(World);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* A = SpawnTimeLimitCharacter(World, FVector(0, 0, 0));
    AWTBRCharacter* B = SpawnTimeLimitCharacter(World, FVector(300, 0, 0));
    if (!A || !B) return false;

    TestFalse(TEXT("Timer not armed before InMatch"), GameMode->IsMatchTimeLimitTimerActiveForTest());

    GameMode->AdvanceToInMatchForTest();
    TestTrue(TEXT("Timer armed at InMatch entry"), GameMode->IsMatchTimeLimitTimerActiveForTest());

    // Teams get block-filled 1-per-team here (TeamSize=3, only 2 combatants) —
    // reassign explicitly so the win condition is unambiguous for this test.
    A->SetTeamId(0);
    B->SetTeamId(1);

    TimeLimitTest_EliminateBy(GameMode, A, B);
    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestFalse(TEXT("Time-limit timer cleared once the round ends naturally"),
        GameMode->IsMatchTimeLimitTimerActiveForTest());

    return true;
}

// ── No time limit configured (0) never arms a timer ─────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimitZeroNeverArmsTest,
    "WTBR.Combat.Team.TimeLimit.ZeroTimeLimitNeverArms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimitZeroNeverArmsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTimeLimitWorldFixture Fixture(TEXT("WTBR_TimeLimit_ZeroNeverArms"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTimeLimitGameState(World, Make15PRules(0.0f), EWTBRMatchPhase::Countdown);
    AWTBRGameMode* GameMode = SpawnTimeLimitGameMode(World);
    if (!GameState || !GameMode) return false;

    GameMode->AdvanceToInMatchForTest();
    TestFalse(TEXT("No timer armed when MatchTimeLimitSeconds is 0"), GameMode->IsMatchTimeLimitTimerActiveForTest());

    return true;
}

// ── Timeout with 3 teams alive: every alive team scores survivor bonus,
//    highest total wins even though nobody was eliminated to a single team ─────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimitMultiTeamSurvivorScoringTest,
    "WTBR.Combat.Team.TimeLimit.ExpiryWithThreeTeamsAliveScoresAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimitMultiTeamSurvivorScoringTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTimeLimitWorldFixture Fixture(TEXT("WTBR_TimeLimit_MultiTeamAlive"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTimeLimitGameState(World, Make15PRules(600.0f), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTimeLimitGameMode(World);
    if (!GameState || !GameMode) return false;

    // Team 0: A, B (2 alive) — Team 1: C (1 alive) — Team 2: D, E (2 alive).
    // Team 3: F, G both eliminated before the timer fires (0 alive, but already
    // has 1 kill point banked from killing F... no — give team 3 a kill point by
    // having it down a team-2 member before being wiped itself, to prove a wiped
    // team's earlier kill points still count at a timeout too).
    AWTBRCharacter* A = SpawnTimeLimitCharacter(World, FVector(0, 0, 0));
    AWTBRCharacter* B = SpawnTimeLimitCharacter(World, FVector(100, 0, 0));
    AWTBRCharacter* C = SpawnTimeLimitCharacter(World, FVector(200, 0, 0));
    AWTBRCharacter* D = SpawnTimeLimitCharacter(World, FVector(300, 0, 0));
    AWTBRCharacter* E = SpawnTimeLimitCharacter(World, FVector(400, 0, 0));
    AWTBRCharacter* F = SpawnTimeLimitCharacter(World, FVector(500, 0, 0));
    AWTBRCharacter* G = SpawnTimeLimitCharacter(World, FVector(600, 0, 0));
    if (!A || !B || !C || !D || !E || !F || !G) return false;
    A->SetTeamId(0); B->SetTeamId(0);
    C->SetTeamId(1);
    D->SetTeamId(2); E->SetTeamId(2);
    F->SetTeamId(3); G->SetTeamId(3);

    // Team 3 gets a kill point (on a throwaway extra) before being wiped, so its
    // score survives the wipe and can still factor into the timeout winner.
    AWTBRCharacter* Fodder = SpawnTimeLimitCharacter(World, FVector(700, 0, 0));
    Fodder->SetTeamId(4);
    TimeLimitTest_EliminateBy(GameMode, Fodder, F);
    TestEqual(TEXT("Team 3 has 1 kill point pre-timeout"), GameState->GetTeamTotalScore(3), 1);
    TestEqual(TEXT("Match still InMatch (team 4 was solo-eliminated, others remain)"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);

    // Wipe team 3 entirely (no kill credit needed for this step).
    F->HealthComponent->ApplyDamage(10000.0f, D);
    GameMode->NotifyCombatantEliminated(F, D);
    G->HealthComponent->ApplyDamage(10000.0f, D);
    GameMode->NotifyCombatantEliminated(G, D);
    TestEqual(TEXT("Team 2 has 2 kill points from wiping team 3"), GameState->GetTeamTotalScore(2), 2);
    TestEqual(TEXT("Still InMatch — teams 0/1/2 all still alive"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);

    // Time runs out with teams 0 (2 alive), 1 (1 alive), 2 (2 alive) all still
    // standing — the design lock: end the match anyway, don't wait for one team.
    GameMode->ForceMatchTimeLimitExpiredForTest();

    TestEqual(TEXT("Phase -> PostMatch on timeout"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestTrue(TEXT("Result resolved"), GameMode->IsMatchResultResolvedForTest());

    TestEqual(TEXT("Team 0: 0 kills + 2 survivor bonus"), GameState->GetTeamTotalScore(0), 2);
    TestEqual(TEXT("Team 1: 0 kills + 1 survivor bonus"), GameState->GetTeamTotalScore(1), 1);
    TestEqual(TEXT("Team 2: 2 kills + 2 survivor bonus = 4"), GameState->GetTeamTotalScore(2), 4);
    TestEqual(TEXT("Team 3: 1 kill point survives its own wipe, no survivor bonus (0 alive)"),
        GameState->GetTeamTotalScore(3), 1);
    TestEqual(TEXT("Team 4 (fodder, wiped, no kills): 0"), GameState->GetTeamTotalScore(4), 0);

    TestEqual(TEXT("Highest total (team 2, 4 points) wins the timeout"), GameState->GetWinningTeamId(), 2);
    TestFalse(TEXT("Individual winner stays null in team mode"), GameState->HasMatchWinner());

    // Idempotency: a stray second expiry after resolution changes nothing.
    GameMode->ForceMatchTimeLimitExpiredForTest();
    TestEqual(TEXT("Score unchanged after duplicate expiry"), GameState->GetTeamTotalScore(2), 4);
    TestEqual(TEXT("Winner unchanged after duplicate expiry"), GameState->GetWinningTeamId(), 2);

    return true;
}

// ── Timeout tie among alive teams with equal alive counts is a draw ────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimitTieIsDrawTest,
    "WTBR.Combat.Team.TimeLimit.TieAmongEquallyAliveTeamsIsDraw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimitTieIsDrawTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTimeLimitWorldFixture Fixture(TEXT("WTBR_TimeLimit_TieDraw"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTimeLimitGameState(World, Make15PRules(600.0f), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTimeLimitGameMode(World);
    if (!GameState || !GameMode) return false;

    // Two teams, one member each, neither has scored a kill — both end up tied
    // at 1 total (0 kills + 1 survivor) with equal alive counts (1 each).
    AWTBRCharacter* A = SpawnTimeLimitCharacter(World, FVector(0, 0, 0));
    AWTBRCharacter* B = SpawnTimeLimitCharacter(World, FVector(300, 0, 0));
    if (!A || !B) return false;
    A->SetTeamId(0);
    B->SetTeamId(1);

    GameMode->ForceMatchTimeLimitExpiredForTest();

    TestEqual(TEXT("Team 0 total"), GameState->GetTeamTotalScore(0), 1);
    TestEqual(TEXT("Team 1 total"), GameState->GetTeamTotalScore(1), 1);
    TestFalse(TEXT("Tied on score AND alive count -> draw (no winning team)"), GameState->HasWinningTeam());

    return true;
}

// ── Timeout is ignored for non-team modes (defensive; time limit is a
//    team-mode-only feature per the design lock) ───────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTimeLimitIgnoredWithoutTeamsTest,
    "WTBR.Combat.Team.TimeLimit.IgnoredWhenTeamsNotUsed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTimeLimitIgnoredWithoutTeamsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTimeLimitWorldFixture Fixture(TEXT("WTBR_TimeLimit_NoTeams"));
    UWorld* World = Fixture.GetWorld();

    FWTBRMatchModeRules Rules;
    Rules.bUseTeams = false;
    Rules.MatchTimeLimitSeconds = 600.0f; // Hypothetical — no mode sets this today.
    AWTBRGameState* GameState = SpawnTimeLimitGameState(World, Rules, EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTimeLimitGameMode(World);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* A = SpawnTimeLimitCharacter(World, FVector(0, 0, 0));
    if (!A) return false;

    GameMode->ForceMatchTimeLimitExpiredForTest();

    TestEqual(TEXT("Phase unaffected — not a team round"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);
    TestFalse(TEXT("Result not resolved"), GameMode->IsMatchResultResolvedForTest());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
