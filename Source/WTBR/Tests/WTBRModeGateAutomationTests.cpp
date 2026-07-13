// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/WTBRHealthComponent.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "WTBRCharacter.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Mode Gate Automation Tests (WTBR.Combat.ModeGate.*)
//
// Verifies the TeamThree15P design lock (2026-07-13: "no corpse loot, no bag")
// actually holds at the production spawn path, not just as an unread DA flag.
// UWTBRHealthComponent::SpawnDroppedTriggersForEliminatedCharacter() already
// gates both its legacy and container sub-paths on
// AWTBRGameState::AllowsCorpseLoot()/AllowsTriggerPickup() — this file locks
// that AWTBRGameMode::MakeDefaultRulesForMode(TeamThree15P) actually ships those
// flags off, and that the real Eliminated-time spawn call is a true no-op under
// those rules (regression guard: catches anyone flipping the mode default
// without also being aware corpse loot must stay off in 15P).
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRModeGateWorldFixture
    {
    public:
        explicit FWTBRModeGateWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRModeGateWorldFixture()
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

    template <typename TActor>
    int32 CountActorsOfClass(UWorld* World)
    {
        int32 Count = 0;
        for (TActorIterator<TActor> It(World); It; ++It)
        {
            ++Count;
        }
        return Count;
    }
}

// ── Production 15P defaults ship with corpse loot / trigger pickup OFF ─────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRModeGate15PDefaultsTest,
    "WTBR.Combat.ModeGate.TeamThree15PDefaultsDisableCorpseLoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRModeGate15PDefaultsTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRMatchModeRules Rules = AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::TeamThree15P);

    TestFalse(TEXT("15P default: corpse loot OFF"), Rules.bAllowCorpseLoot);
    TestFalse(TEXT("15P default: trigger pickup OFF"), Rules.bAllowTriggerPickup);
    TestFalse(TEXT("15P default: corpse loot container OFF (uses legacy path, which is itself gated off)"),
        Rules.bUseCorpseLootContainerOnDeath);
    TestTrue(TEXT("15P default: teams assigned at match start"), Rules.bAssignTeamsAtMatchStart);
    TestTrue(TEXT("15P default: score-based team winner"), Rules.bScoreBasedTeamWinner);
    TestEqual(TEXT("15P default: team size 3"), Rules.TeamSize, 3);
    TestEqual(TEXT("15P default: 15 max players"), Rules.MaxPlayers, 15);

    return true;
}

// ── BR defaults keep corpse loot / trigger pickup ON (contrast, catches an
//    accidental global flip of the "off" default) ─────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRModeGateBRDefaultsTest,
    "WTBR.Combat.ModeGate.BattleRoyaleDefaultsEnableCorpseLoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRModeGateBRDefaultsTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRMatchModeRules Rules = AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::BattleRoyale);

    TestTrue(TEXT("BR default: corpse loot ON"), Rules.bAllowCorpseLoot);
    TestTrue(TEXT("BR default: trigger pickup ON"), Rules.bAllowTriggerPickup);
    TestTrue(TEXT("BR default: teams assigned at match start"), Rules.bAssignTeamsAtMatchStart);
    TestFalse(TEXT("BR default: NOT score-based (surviving team just wins)"), Rules.bScoreBasedTeamWinner);
    TestEqual(TEXT("BR default: team size 3"), Rules.TeamSize, 3);
    TestEqual(TEXT("BR default: 60 max players"), Rules.MaxPlayers, 60);

    return true;
}

// ── Eliminated-time spawn is a true no-op under 15P rules ──────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRModeGate15PSpawnNoOpTest,
    "WTBR.Combat.ModeGate.TeamThree15PEliminationSpawnsNoLoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRModeGate15PSpawnNoOpTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRModeGateWorldFixture Fixture(TEXT("WTBR_ModeGate_15P_NoLoot"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;
    World->SetGameState(GameState);

    // The real production defaults, not a hand-rolled struct.
    GameState->SetCurrentMatchRules(AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::TeamThree15P));
    GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRCharacter* VictimCharacter = World->SpawnActor<AWTBRCharacter>(
        AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    TestNotNull(TEXT("Victim spawns"), VictimCharacter);
    if (!VictimCharacter || !VictimCharacter->HealthComponent) return false;

    TestEqual(TEXT("No dropped trigger actors before elimination"),
        CountActorsOfClass<AWTBRDroppedTriggerActor>(World), 0);
    TestEqual(TEXT("No corpse loot container actors before elimination"),
        CountActorsOfClass<AWTBRCorpseLootContainerActor>(World), 0);

    // Exercises the exact call EnterEliminatedState makes on death.
    VictimCharacter->HealthComponent->SpawnDroppedTriggersForEliminatedCharacterForTest();

    TestEqual(TEXT("Still no dropped trigger actors: 15P has corpse loot off"),
        CountActorsOfClass<AWTBRDroppedTriggerActor>(World), 0);
    TestEqual(TEXT("Still no corpse loot container actors: 15P has corpse loot off"),
        CountActorsOfClass<AWTBRCorpseLootContainerActor>(World), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
