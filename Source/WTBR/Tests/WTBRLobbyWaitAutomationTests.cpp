// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Lobby Wait-For-Players Automation Tests (WTBR.Combat.Match.Lobby.*)
//
// Closes the gap flagged 2026-07-13: BeginPlay used to jump straight into
// LoadoutSetup regardless of how many players (if any) happened to be
// connected at that exact instant. Now BeginPlay enters a real Lobby phase
// and holds there (polling GetNumPlayers()) until MinPlayersToStart is met,
// LobbyMaxWaitSeconds elapses (safety valve, if >0), or a host/dev calls
// WTBRForceStartMatch. Default MinPlayersToStart=1 preserves the existing
// solo/PIE/1v1-harness behavior.
//
// Same headless conventions as the other GameMode fixtures: transient
// EWorldType::Game world, no real UWorld::BeginPlay() (so this exercises the
// Lobby/poll logic directly via test seams, not the real BeginPlay hookup —
// that hookup is a one-line call, proven by code review same as the other
// BeginPlay-adjacent logic in this codebase), GameState registered via
// World->SetGameState. World->GetTimeSeconds() does not advance without
// ticking, so the timeout path is driven via SetLobbyEnteredWorldTimeForTest.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRLobbyWorldFixture
    {
    public:
        explicit FWTBRLobbyWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRLobbyWorldFixture()
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

    AWTBRGameState* SpawnLobbyGameState(UWorld* World)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (GameState)
        {
            World->SetGameState(GameState);
        }
        return GameState;
    }

    AWTBRGameMode* SpawnLobbyGameMode(UWorld* World)
    {
        if (!World) return nullptr;
        return World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    }

    // AController::PostInitializeComponents calls InitPlayerState() at spawn
    // (independent of World::BeginPlay), so a bare spawned APlayerController
    // counts toward AGameModeBase::GetNumPlayers() — this is the standard way
    // these fixtures simulate "a player is connected" (see
    // WTBRCombatRoundAutomationTests.cpp's WinnerRecordedOnElimination test).
    APlayerController* SpawnConnectedPlayer(UWorld* World)
    {
        if (!World) return nullptr;
        return World->SpawnActor<APlayerController>(APlayerController::StaticClass());
    }
}

// ── Default MinPlayersToStart=1: starts immediately once one player is connected ─

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLobbyDefaultStartsImmediatelyTest,
    "WTBR.Combat.Match.Lobby.DefaultMinPlayersStartsImmediately",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLobbyDefaultStartsImmediatelyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRLobbyWorldFixture Fixture(TEXT("WTBR_Lobby_DefaultImmediate"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLobbyGameState(World);
    AWTBRGameMode* GameMode = SpawnLobbyGameMode(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameState || !GameMode) return false;

    APlayerController* Player = SpawnConnectedPlayer(World);
    TestNotNull(TEXT("Player controller spawns"), Player);
    if (!Player) return false;

    // Default MinPlayersToStart=1, LobbyMaxWaitSeconds=0 — one connected player
    // is already enough, so entering Lobby should advance straight through to
    // LoadoutSetup without needing a poll tick.
    GameMode->EnterLobbyAndWaitForPlayersForTest();

    TestEqual(TEXT("Phase -> LoadoutSetup immediately"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::LoadoutSetup);
    TestFalse(TEXT("No poll timer left running once ready"), GameMode->IsLobbyPollTimerActiveForTest());

    return true;
}

// ── Under MinPlayersToStart: stays in Lobby and keeps polling ───────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLobbyWaitsUnderMinPlayersTest,
    "WTBR.Combat.Match.Lobby.WaitsInLobbyUnderMinPlayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLobbyWaitsUnderMinPlayersTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRLobbyWorldFixture Fixture(TEXT("WTBR_Lobby_WaitsUnderMin"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLobbyGameState(World);
    AWTBRGameMode* GameMode = SpawnLobbyGameMode(World);
    if (!GameState || !GameMode) return false;

    GameMode->SetMinPlayersToStartForTest(3);
    APlayerController* Player = SpawnConnectedPlayer(World);
    TestNotNull(TEXT("One player connects"), Player);
    if (!Player) return false;

    GameMode->EnterLobbyAndWaitForPlayersForTest();

    TestEqual(TEXT("Phase stays Lobby with 1/3 players"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Lobby);
    TestTrue(TEXT("Poll timer is running while under-strength"), GameMode->IsLobbyPollTimerActiveForTest());

    // A poll tick with still only 1/3 players changes nothing.
    GameMode->PollLobbyReadyToStartForTest();
    TestEqual(TEXT("Still Lobby after a poll with insufficient players"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Lobby);

    // Two more players connect, reaching 3/3 — the next poll advances.
    SpawnConnectedPlayer(World);
    SpawnConnectedPlayer(World);
    GameMode->PollLobbyReadyToStartForTest();

    TestEqual(TEXT("Phase -> LoadoutSetup once MinPlayersToStart is met"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::LoadoutSetup);
    TestFalse(TEXT("Poll timer cleared once ready"), GameMode->IsLobbyPollTimerActiveForTest());

    return true;
}

// ── LobbyMaxWaitSeconds safety valve forces a start even under MinPlayersToStart ─

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLobbyMaxWaitForcesStartTest,
    "WTBR.Combat.Match.Lobby.MaxWaitSecondsForcesStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLobbyMaxWaitForcesStartTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRLobbyWorldFixture Fixture(TEXT("WTBR_Lobby_MaxWait"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLobbyGameState(World);
    AWTBRGameMode* GameMode = SpawnLobbyGameMode(World);
    if (!GameState || !GameMode) return false;

    GameMode->SetMinPlayersToStartForTest(15);
    GameMode->SetLobbyMaxWaitSecondsForTest(60.0f);
    // No players connect at all.

    GameMode->EnterLobbyAndWaitForPlayersForTest();
    TestEqual(TEXT("Phase stays Lobby immediately (0/15, wait not yet elapsed)"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Lobby);

    // Simulate 61s having elapsed since Lobby entry (World->GetTimeSeconds()
    // doesn't advance without ticking, so the entry time is set directly).
    GameMode->SetLobbyEnteredWorldTimeForTest(World->GetTimeSeconds() - 61.0f);
    GameMode->PollLobbyReadyToStartForTest();

    TestEqual(TEXT("Phase -> LoadoutSetup once the safety-valve timeout elapses"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::LoadoutSetup);

    return true;
}

// ── LobbyMaxWaitSeconds=0 (default) never force-starts — waits indefinitely ────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLobbyZeroMaxWaitNeverForcesTest,
    "WTBR.Combat.Match.Lobby.ZeroMaxWaitNeverForcesStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLobbyZeroMaxWaitNeverForcesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRLobbyWorldFixture Fixture(TEXT("WTBR_Lobby_ZeroMaxWait"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLobbyGameState(World);
    AWTBRGameMode* GameMode = SpawnLobbyGameMode(World);
    if (!GameState || !GameMode) return false;

    GameMode->SetMinPlayersToStartForTest(15);
    GameMode->SetLobbyMaxWaitSecondsForTest(0.0f); // No safety valve.

    GameMode->EnterLobbyAndWaitForPlayersForTest();
    // Simulate an enormous elapsed time — still must not force-start.
    GameMode->SetLobbyEnteredWorldTimeForTest(World->GetTimeSeconds() - 100000.0f);
    GameMode->PollLobbyReadyToStartForTest();

    TestEqual(TEXT("Still Lobby no matter how much time passes when MaxWait=0"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Lobby);
    TestTrue(TEXT("Poll timer still running"), GameMode->IsLobbyPollTimerActiveForTest());

    return true;
}

// ── WTBRForceStartMatch: works from Lobby, rejected elsewhere ──────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLobbyForceStartMatchTest,
    "WTBR.Combat.Match.Lobby.ForceStartMatchFromLobbyOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLobbyForceStartMatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRLobbyWorldFixture Fixture(TEXT("WTBR_Lobby_ForceStart"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = SpawnLobbyGameState(World);
    AWTBRGameMode* GameMode = SpawnLobbyGameMode(World);
    if (!GameState || !GameMode) return false;

    GameMode->SetMinPlayersToStartForTest(15);
    GameMode->EnterLobbyAndWaitForPlayersForTest();
    TestEqual(TEXT("Waiting in Lobby with 0/15 players"), GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::Lobby);
    TestTrue(TEXT("Poll timer running before force-start"), GameMode->IsLobbyPollTimerActiveForTest());

    GameMode->WTBRForceStartMatch();

    TestEqual(TEXT("Force-start advances past the wait despite 0/15 players"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::LoadoutSetup);
    TestFalse(TEXT("Poll timer cleared by force-start"), GameMode->IsLobbyPollTimerActiveForTest());

    // Rejected once already out of Lobby: phase must not regress or reset timers.
    GameMode->WTBRForceStartMatch();
    TestEqual(TEXT("Second force-start call is rejected (not in Lobby) — phase unchanged"),
        GameState->GetCurrentMatchPhase(), EWTBRMatchPhase::LoadoutSetup);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
