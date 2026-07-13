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
// Team Match Automation Tests (WTBR.Combat.Team.*)
//
// Covers the TeamThree15P foundation (mode design lock 2026-07-13): block-fill
// team assignment at InMatch entry, friendly-fire blocking in
// UWTBRHealthComponent::ApplyDamage, kill-point scoring at Eliminated, the
// survivor bonus, and winner-by-total-score (including the signature case where
// an already-wiped team beats the survivors on kill points).
//
// Same headless conventions as WTBRCombatRoundAutomationTests.cpp: transient
// EWorldType::Game world, no BeginPlay, GameState registered via
// World->SetGameState, characters spawned with no CoreStatsAsset so lethal
// damage lands on Eliminated synchronously, and
// AWTBRGameMode::NotifyCombatantEliminated invoked directly because
// GetAuthGameMode() is only populated by real level travel.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRTeamWorldFixture
    {
    public:
        explicit FWTBRTeamWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRTeamWorldFixture()
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

    AWTBRGameState* SpawnTeamGameState(UWorld* World, const FWTBRMatchModeRules& Rules, EWTBRMatchPhase InitialPhase)
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

    AWTBRGameMode* SpawnTeamGameMode(UWorld* World)
    {
        if (!World) return nullptr;
        return World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    }

    AWTBRCharacter* SpawnTeamCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    FWTBRMatchModeRules MakeTeamRules(int32 TeamSize, bool bScoreBasedTeamWinner, bool bAllowFriendlyFire = false)
    {
        FWTBRMatchModeRules Rules;
        Rules.MatchMode = EWTBRMatchMode::TeamThree15P;
        Rules.TeamSize = TeamSize;
        Rules.MaxPlayers = 15;
        Rules.bUseTeams = true;
        Rules.bAssignTeamsAtMatchStart = true;
        Rules.bAllowFriendlyFire = bAllowFriendlyFire;
        Rules.bScoreBasedTeamWinner = bScoreBasedTeamWinner;
        return Rules;
    }

    // Lethal enemy kill: no CoreStatsAsset -> synchronous Eliminated, then the
    // production round-loop entry point is driven directly (fixture worlds never
    // populate GetAuthGameMode, see file header).
    void EliminateBy(AWTBRGameMode* GameMode, AWTBRCharacter* Victim, AWTBRCharacter* Killer)
    {
        Victim->HealthComponent->ApplyDamage(10000.0f, Killer);
        GameMode->NotifyCombatantEliminated(Victim, Killer);
    }

    void EliminateByEnvironment(AWTBRGameMode* GameMode, AWTBRCharacter* Victim)
    {
        Victim->HealthComponent->ApplyDamage(10000.0f, nullptr);
        GameMode->NotifyCombatantEliminated(Victim, nullptr);
    }
}

// ── Team assignment: block fill at InMatch entry ───────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamAssignmentTest,
    "WTBR.Combat.Team.AssignmentBlockFillAtInMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamAssignmentTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_Assignment"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(3, true), EWTBRMatchPhase::Countdown);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameState || !GameMode) return false;

    TArray<AWTBRCharacter*> Characters;
    for (int32 i = 0; i < 15; ++i)
    {
        AWTBRCharacter* Character = SpawnTeamCharacter(World, FVector(i * 300.0f, 0.0f, 0.0f));
        TestNotNull(TEXT("Character spawns"), Character);
        if (!Character) return false;
        Characters.Add(Character);
        TestEqual(TEXT("TeamId starts unassigned"), Character->GetTeamId(), (int32)INDEX_NONE);
    }

    GameMode->AdvanceToInMatchForTest();
    TestEqual(TEXT("Phase -> InMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);

    // 15 combatants in teams of 3 => exactly 5 teams, 3 members each, no one left out.
    TMap<int32, int32> MembersPerTeam;
    for (const AWTBRCharacter* Character : Characters)
    {
        TestTrue(TEXT("Every combatant has a team after InMatch"), Character->HasTeam());
        MembersPerTeam.FindOrAdd(Character->GetTeamId())++;
    }
    TestEqual(TEXT("Exactly 5 teams"), MembersPerTeam.Num(), 5);
    for (const TPair<int32, int32>& Pair : MembersPerTeam)
    {
        TestEqual(FString::Printf(TEXT("Team %d has exactly 3 members"), Pair.Key), Pair.Value, 3);
        TestTrue(TEXT("TeamId in expected range [0,4]"), Pair.Key >= 0 && Pair.Key <= 4);
    }

    return true;
}

// ── Assignment gate: legacy rules leave TeamId untouched ───────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamAssignmentGateTest,
    "WTBR.Combat.Team.NoAssignmentWithoutOptInFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamAssignmentGateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_AssignmentGate"));
    UWorld* World = Fixture.GetWorld();

    // Default-constructed rules: bUseTeams is true but bAssignTeamsAtMatchStart is
    // false — the exact shape every pre-team fixture and the 1v1 harness run with.
    FWTBRMatchModeRules LegacyRules;
    AWTBRGameState* GameState = SpawnTeamGameState(World, LegacyRules, EWTBRMatchPhase::Countdown);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* CharacterA = SpawnTeamCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* CharacterB = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    if (!CharacterA || !CharacterB) return false;

    GameMode->AdvanceToInMatchForTest();

    TestEqual(TEXT("A stays team-less without opt-in flag"), CharacterA->GetTeamId(), (int32)INDEX_NONE);
    TestEqual(TEXT("B stays team-less without opt-in flag"), CharacterB->GetTeamId(), (int32)INDEX_NONE);

    // And the legacy individual winner path still runs: eliminating A resolves
    // the round with B as the (individual) winner, no team result.
    EliminateBy(GameMode, CharacterA, CharacterB);
    TestEqual(TEXT("Phase -> PostMatch via individual path"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestFalse(TEXT("No winning team recorded in individual mode"), GameState->HasWinningTeam());

    return true;
}

// ── Friendly fire ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamFriendlyFireTest,
    "WTBR.Combat.Team.FriendlyFireBlockedByDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamFriendlyFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_FriendlyFire"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(2, true), EWTBRMatchPhase::InMatch);
    if (!GameState) return false;

    AWTBRCharacter* Teammate1 = SpawnTeamCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Teammate2 = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* Enemy = SpawnTeamCharacter(World, FVector(600.0f, 0.0f, 0.0f));
    if (!Teammate1 || !Teammate2 || !Enemy) return false;

    Teammate1->SetTeamId(0);
    Teammate2->SetTeamId(0);
    Enemy->SetTeamId(1);

    const float MaxHP = Teammate2->HealthComponent->GetMaxHP();

    // Same team: blocked.
    Teammate2->HealthComponent->ApplyDamage(50.0f, Teammate1);
    TestEqual(TEXT("Friendly fire blocked (no HP lost)"), Teammate2->HealthComponent->GetCurrentHP(), MaxHP);

    // Self damage is NOT friendly fire (limb drain / self-inflicted paths).
    Teammate2->HealthComponent->ApplyDamage(25.0f, Teammate2);
    TestEqual(TEXT("Self damage still applies"), Teammate2->HealthComponent->GetCurrentHP(), MaxHP - 25.0f);

    // Enemy team: applies.
    Teammate2->HealthComponent->ApplyDamage(50.0f, Enemy);
    TestEqual(TEXT("Enemy damage applies"), Teammate2->HealthComponent->GetCurrentHP(), MaxHP - 75.0f);

    // Team-less pair (e.g. 1v1 harness): never treated as teammates.
    AWTBRCharacter* Solo1 = SpawnTeamCharacter(World, FVector(900.0f, 0.0f, 0.0f));
    AWTBRCharacter* Solo2 = SpawnTeamCharacter(World, FVector(1200.0f, 0.0f, 0.0f));
    if (!Solo1 || !Solo2) return false;
    Solo2->HealthComponent->ApplyDamage(50.0f, Solo1);
    TestEqual(TEXT("Team-less combatants damage each other"),
        Solo2->HealthComponent->GetCurrentHP(), Solo2->HealthComponent->GetMaxHP() - 50.0f);

    // DA-driven override: bAllowFriendlyFire=true re-enables team damage.
    GameState->SetCurrentMatchRules(MakeTeamRules(2, true, /*bAllowFriendlyFire=*/true));
    Teammate2->HealthComponent->ApplyDamage(25.0f, Teammate1);
    TestEqual(TEXT("Friendly fire applies when rules allow it"),
        Teammate2->HealthComponent->GetCurrentHP(), MaxHP - 100.0f);

    return true;
}

// ── Scoring: kill points, survivor bonus, winner by total ──────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamScoringSurvivorWinsTest,
    "WTBR.Combat.Team.KillPointsAndSurvivorBonus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamScoringSurvivorWinsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_ScoringSurvivor"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(2, true), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    // Team 0: A, B — Team 1: C, D. Manual assignment (phase already InMatch).
    AWTBRCharacter* A = SpawnTeamCharacter(World, FVector(0.0f, 0.0f, 0.0f));
    AWTBRCharacter* B = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* C = SpawnTeamCharacter(World, FVector(600.0f, 0.0f, 0.0f));
    AWTBRCharacter* D = SpawnTeamCharacter(World, FVector(900.0f, 0.0f, 0.0f));
    if (!A || !B || !C || !D) return false;
    A->SetTeamId(0);
    B->SetTeamId(0);
    C->SetTeamId(1);
    D->SetTeamId(1);

    // C eliminates A: team 1 scores 1, round continues (B still up for team 0).
    EliminateBy(GameMode, A, C);
    TestEqual(TEXT("Team 1 kill point recorded"), GameState->GetTeamTotalScore(1), 1);
    TestEqual(TEXT("Round continues while team 0 lives"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);
    TestFalse(TEXT("No result resolved yet"), GameMode->IsMatchResultResolvedForTest());

    // C eliminates B: team 0 wiped -> round over. Team 1: 2 kills + 2 alive = 4.
    EliminateBy(GameMode, B, C);
    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestTrue(TEXT("Result resolved"), GameMode->IsMatchResultResolvedForTest());
    TestEqual(TEXT("Team 1 total = 2 kills + 2 survivor bonus"), GameState->GetTeamTotalScore(1), 4);
    TestEqual(TEXT("Team 0 total stays 0"), GameState->GetTeamTotalScore(0), 0);
    TestEqual(TEXT("Winning team is team 1"), GameState->GetWinningTeamId(), 1);
    TestFalse(TEXT("Individual winner field stays null in team mode"), GameState->HasMatchWinner());

    // Idempotency: duplicate notify after resolve changes nothing.
    GameMode->NotifyCombatantEliminated(B, C);
    TestEqual(TEXT("Score unchanged after duplicate notify"), GameState->GetTeamTotalScore(1), 4);
    TestEqual(TEXT("Winner unchanged after duplicate notify"), GameState->GetWinningTeamId(), 1);

    return true;
}

// ── Scoring: a wiped-out team can still win on kill points (15P signature rule) ─

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamWipedTeamWinsOnPointsTest,
    "WTBR.Combat.Team.WipedTeamWinsOnKillPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamWipedTeamWinsOnPointsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_WipedTeamWins"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(3, true), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    // Team 0: lone A — Team 1: B, C, D.
    AWTBRCharacter* A = SpawnTeamCharacter(World, FVector(0.0f, 0.0f, 0.0f));
    AWTBRCharacter* B = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* C = SpawnTeamCharacter(World, FVector(600.0f, 0.0f, 0.0f));
    AWTBRCharacter* D = SpawnTeamCharacter(World, FVector(900.0f, 0.0f, 0.0f));
    if (!A || !B || !C || !D) return false;
    A->SetTeamId(0);
    B->SetTeamId(1);
    C->SetTeamId(1);
    D->SetTeamId(1);

    // A takes down two of team 1 (team 0: 2 points), then dies to the
    // environment — no kill point for anyone, team 1 survives with 1 member
    // (+1 survivor bonus = 1 total). Highest total wins: the wiped team 0.
    EliminateBy(GameMode, B, A);
    EliminateBy(GameMode, C, A);
    TestEqual(TEXT("Team 0 has 2 kill points"), GameState->GetTeamTotalScore(0), 2);
    TestEqual(TEXT("Round continues"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);

    EliminateByEnvironment(GameMode, A);
    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestEqual(TEXT("Environment death scores nothing"), GameState->GetTeamTotalScore(0), 2);
    TestEqual(TEXT("Surviving team 1 total = 0 kills + 1 survivor"), GameState->GetTeamTotalScore(1), 1);
    TestEqual(TEXT("WIPED team 0 wins on points"), GameState->GetWinningTeamId(), 0);

    return true;
}

// ── Scoring: tie resolves to the surviving team ────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamScoreTieSurvivorTiebreakTest,
    "WTBR.Combat.Team.ScoreTieResolvesToSurvivors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamScoreTieSurvivorTiebreakTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_ScoreTie"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(2, true), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    // Team 0: lone A — Team 1: B, C.
    AWTBRCharacter* A = SpawnTeamCharacter(World, FVector(0.0f, 0.0f, 0.0f));
    AWTBRCharacter* B = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* C = SpawnTeamCharacter(World, FVector(600.0f, 0.0f, 0.0f));
    if (!A || !B || !C) return false;
    A->SetTeamId(0);
    B->SetTeamId(1);
    C->SetTeamId(1);

    // A kills B (team 0: 1), then dies to the environment. Team 1 survives with
    // C alive (+1 = 1). 1-1 tie -> surviving team 1 wins the tiebreak.
    EliminateBy(GameMode, B, A);
    EliminateByEnvironment(GameMode, A);

    TestEqual(TEXT("Team 0 total"), GameState->GetTeamTotalScore(0), 1);
    TestEqual(TEXT("Team 1 total"), GameState->GetTeamTotalScore(1), 1);
    TestEqual(TEXT("Tie resolves to surviving team 1"), GameState->GetWinningTeamId(), 1);

    return true;
}

// ── BR-style team winner: surviving team wins regardless of points ─────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamSurvivorWinnerModeTest,
    "WTBR.Combat.Team.SurvivorWinnerWhenNotScoreBased",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamSurvivorWinnerModeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_SurvivorWinner"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(1, /*bScoreBasedTeamWinner=*/false), EWTBRMatchPhase::InMatch);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    // Three solo teams: A(0), B(1), C(2). A scores a kill first, then C
    // eliminates A — BR rules: surviving team 2 wins even though team 0 leads
    // on kill points.
    AWTBRCharacter* A = SpawnTeamCharacter(World, FVector(0.0f, 0.0f, 0.0f));
    AWTBRCharacter* B = SpawnTeamCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* C = SpawnTeamCharacter(World, FVector(600.0f, 0.0f, 0.0f));
    if (!A || !B || !C) return false;
    A->SetTeamId(0);
    B->SetTeamId(1);
    C->SetTeamId(2);

    EliminateBy(GameMode, B, A);
    TestEqual(TEXT("Team 0 kill point recorded"), GameState->GetTeamTotalScore(0), 1);

    EliminateBy(GameMode, A, C);
    TestEqual(TEXT("Phase -> PostMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestEqual(TEXT("Surviving team 2 wins despite fewer points"), GameState->GetWinningTeamId(), 2);

    return true;
}

// ── 15-puppet full match smoke ─────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTeamFifteenPuppetMatchSmokeTest,
    "WTBR.Combat.Team.FifteenPuppetMatchSmoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTeamFifteenPuppetMatchSmokeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTeamWorldFixture Fixture(TEXT("WTBR_Team_FifteenPuppets"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnTeamGameState(World, MakeTeamRules(3, true), EWTBRMatchPhase::Countdown);
    AWTBRGameMode* GameMode = SpawnTeamGameMode(World);
    if (!GameState || !GameMode) return false;

    TArray<AWTBRCharacter*> Characters;
    for (int32 i = 0; i < 15; ++i)
    {
        AWTBRCharacter* Character = SpawnTeamCharacter(World, FVector(i * 300.0f, 0.0f, 0.0f));
        if (!Character) return false;
        Characters.Add(Character);
    }

    // Full production round entry: teams assigned, scores reset, phase InMatch.
    GameMode->AdvanceToInMatchForTest();
    TestEqual(TEXT("Phase -> InMatch"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);

    // Pick one killer and their teammates by assigned TeamId (assignment order is
    // an implementation detail — group by actual TeamId instead of assuming it).
    AWTBRCharacter* Killer = Characters[0];
    const int32 KillerTeam = Killer->GetTeamId();
    TestTrue(TEXT("Killer has a team"), Killer->HasTeam());

    // Friendly fire sanity inside the full match: a teammate takes no damage.
    AWTBRCharacter* KillerTeammate = nullptr;
    TArray<AWTBRCharacter*> Enemies;
    for (AWTBRCharacter* Character : Characters)
    {
        if (Character == Killer) continue;
        if (Character->GetTeamId() == KillerTeam)
        {
            KillerTeammate = Character;
        }
        else
        {
            Enemies.Add(Character);
        }
    }
    TestNotNull(TEXT("Killer has a teammate"), KillerTeammate);
    TestEqual(TEXT("12 enemies across 4 other teams"), Enemies.Num(), 12);
    if (!KillerTeammate || Enemies.Num() != 12) return false;

    KillerTeammate->HealthComponent->ApplyDamage(100.0f, Killer);
    TestEqual(TEXT("Teammate untouched by friendly fire"),
        KillerTeammate->HealthComponent->GetCurrentHP(), KillerTeammate->HealthComponent->GetMaxHP());

    // Killer wipes all 12 enemies. Round must stay open until the last one drops.
    for (int32 i = 0; i < Enemies.Num(); ++i)
    {
        const bool bLastEnemy = (i == Enemies.Num() - 1);
        EliminateBy(GameMode, Enemies[i], Killer);
        if (!bLastEnemy)
        {
            TestEqual(FString::Printf(TEXT("Round still open after enemy %d down"), i + 1),
                GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::InMatch);
        }
    }

    // 12 kills + 3 survivors = 15 total for the killer's team.
    TestEqual(TEXT("Phase -> PostMatch after last enemy"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::PostMatch);
    TestEqual(TEXT("Killer team total = 12 kills + 3 survivor bonus"),
        GameState->GetTeamTotalScore(KillerTeam), 15);
    TestEqual(TEXT("Killer team wins"), GameState->GetWinningTeamId(), KillerTeam);
    TestFalse(TEXT("Individual winner stays null"), GameState->HasMatchWinner());

    // Restart resets the team result for the next round.
    GameMode->WTBRRestartRound();
    TestFalse(TEXT("Winning team cleared on restart"), GameState->HasWinningTeam());
    TestEqual(TEXT("Team scores cleared on restart"), GameState->GetTeamScores().Num(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
