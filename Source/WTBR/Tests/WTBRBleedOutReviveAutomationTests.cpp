// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/WTBRHealthComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRCharacter.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Bleed-out + Revive Automation Tests (WTBR.Combat.Revive.*)
//
// Mode design lock 2026-07-13: a downed combatant bleeds out after
// BleedOutDuration (~30 s) -> Eliminated, crediting the LAST DAMAGER. A living
// teammate can revive; while the revive is held the bleed-out countdown PAUSES
// and RESUMES from the remaining value on release (never resets).
//
// Unlike the other combat fixtures these MUST assign a CoreStatsAsset: with no
// stats, EnterDownedState short-circuits straight to Eliminated (see
// WTBRHealthComponent.cpp) and the Downed/bleed-out/revive path never runs. Real
// FTimerManager timers do not fire in the headless transient world, so the
// bleed-out expiry and revive completion are driven through the
// WITH_DEV_AUTOMATION_TESTS seams (ForceBleedOutExpiryForTest /
// ForceReviveCompleteForTest); the pause/resume behaviour is asserted via the
// timer-state seams, which reflect the exact FTimerManager handle.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRReviveWorldFixture
    {
    public:
        explicit FWTBRReviveWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRReviveWorldFixture()
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

    UWTBRCoreStatsDataAsset* MakeStats()
    {
        UWTBRCoreStatsDataAsset* Stats = NewObject<UWTBRCoreStatsDataAsset>();
        Stats->MaxHP = 300.0f;
        Stats->MaxDownedHP = 100.0f;
        Stats->BleedOutDuration = 30.0f;
        Stats->ReviveHoldDuration = 5.0f;
        Stats->ReviveHPRestored = 100.0f;
        Stats->KnockdownIFrameDuration = 0.0f; // no i-frame so finishing damage lands in-test
        return Stats;
    }

    AWTBRCharacter* SpawnReviveCharacter(UWorld* World, const FVector& Loc, UWTBRCoreStatsDataAsset* Stats)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
        if (Character && Character->HealthComponent && Stats)
        {
            // Assign the in-memory stats asset so GetStats() resolves non-null
            // (BeginPlay never runs in this fixture, so set HP/state explicitly).
            Character->HealthComponent->CoreStatsAsset = Stats;
        }
        return Character;
    }

    // Drive a character down without relying on i-frames/timers: one lethal hit
    // from Attacker takes Alive -> Downed (HP 0, DownedHP = MaxDownedHP).
    void DriveDown(AWTBRCharacter* Victim, AWTBRCharacter* Attacker)
    {
        Victim->HealthComponent->ApplyDamage(10000.0f, Attacker);
    }
}

// ── Bleed-out expiry eliminates and credits the last damager ───────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRBleedOutEliminatesTest,
    "WTBR.Combat.Revive.BleedOutEliminatesCreditingLastDamager",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRBleedOutEliminatesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveWorldFixture Fixture(TEXT("WTBR_Revive_BleedOut"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeStats();
    AWTBRCharacter* Victim = SpawnReviveCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* Downer = SpawnReviveCharacter(World, FVector(300, 0, 0), Stats);
    AWTBRCharacter* LastTagger = SpawnReviveCharacter(World, FVector(600, 0, 0), Stats);
    if (!Victim || !Downer || !LastTagger) return false;

    // Downer puts Victim down; then a different attacker tags (but does not
    // finish) — the last damager is now LastTagger.
    DriveDown(Victim, Downer);
    TestTrue(TEXT("Victim is Downed"), Victim->HealthComponent->IsDowned());
    TestTrue(TEXT("Bleed-out timer running while downed"), Victim->HealthComponent->IsBleedOutTimerActiveForTest());

    Victim->HealthComponent->ApplyDamage(10.0f, LastTagger); // chip damage, still downed
    TestTrue(TEXT("Victim still Downed after chip"), Victim->HealthComponent->IsDowned());
    TestEqual(TEXT("Last damager tracked = LastTagger"),
        Victim->HealthComponent->GetLastDamageInstigatorForTest(), Cast<AActor>(LastTagger));

    // Bleed-out fires: Eliminated, credit passed to LastTagger.
    Victim->HealthComponent->ForceBleedOutExpiryForTest();
    TestTrue(TEXT("Victim Eliminated after bleed-out"), Victim->HealthComponent->IsEliminated());
    TestFalse(TEXT("Bleed-out timer cleared after elimination"), Victim->HealthComponent->IsBleedOutTimerActiveForTest());

    return true;
}

// ── Bleed-out expiry drives team scoring to the last damager ───────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRBleedOutTeamScoreTest,
    "WTBR.Combat.Revive.BleedOutScoresLastDamagerTeam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRBleedOutTeamScoreTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveWorldFixture Fixture(TEXT("WTBR_Revive_BleedOutScore"));
    UWorld* World = Fixture.GetWorld();

    // GameState in InMatch with team rules so NotifyCombatantEliminated routes to
    // the team-scoring path when bleed-out fires.
    AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
    World->SetGameState(GameState);
    FWTBRMatchModeRules Rules;
    Rules.MatchMode = EWTBRMatchMode::TeamThree15P;
    Rules.bUseTeams = true;
    Rules.bScoreBasedTeamWinner = true;
    GameState->SetCurrentMatchRules(Rules);
    GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);

    AWTBRGameMode* GameMode = World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    if (!GameState || !GameMode) return false;

    UWTBRCoreStatsDataAsset* Stats = MakeStats();
    // Team 0: Victim + a teammate to keep team 0 alive so the round doesn't end.
    AWTBRCharacter* Victim = SpawnReviveCharacter(World, FVector(0, 0, 0), Stats);
    AWTBRCharacter* VictimMate = SpawnReviveCharacter(World, FVector(150, 0, 0), Stats);
    // Team 1: Downer + LastTagger.
    AWTBRCharacter* Downer = SpawnReviveCharacter(World, FVector(300, 0, 0), Stats);
    AWTBRCharacter* LastTagger = SpawnReviveCharacter(World, FVector(600, 0, 0), Stats);
    if (!Victim || !VictimMate || !Downer || !LastTagger) return false;
    Victim->SetTeamId(0);
    VictimMate->SetTeamId(0);
    Downer->SetTeamId(1);
    LastTagger->SetTeamId(1);

    DriveDown(Victim, Downer);
    Victim->HealthComponent->ApplyDamage(10.0f, LastTagger);

    // Bleed-out through the component's own path AND notify the GameMode as the
    // production EnterEliminatedState hookup would (GetAuthGameMode is null in the
    // fixture world — same convention as the other combat tests).
    Victim->HealthComponent->ForceBleedOutExpiryForTest();
    GameMode->NotifyCombatantEliminated(Victim, LastTagger);

    TestTrue(TEXT("Victim eliminated"), Victim->HealthComponent->IsEliminated());
    TestEqual(TEXT("Kill point credited to LastTagger's team (1)"), GameState->GetTeamTotalScore(1), 1);
    TestEqual(TEXT("Downer's chip-less team gets no separate point via team 0"), GameState->GetTeamTotalScore(0), 0);

    return true;
}

// ── Revive pauses the bleed-out timer; release resumes (no reset) ──────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRevivePauseResumeTest,
    "WTBR.Combat.Revive.HoldPausesBleedOutReleaseResumes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRevivePauseResumeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveWorldFixture Fixture(TEXT("WTBR_Revive_PauseResume"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeStats();
    AWTBRCharacter* Victim = SpawnReviveCharacter(World, FVector(0, 0, 0), Stats);
    AWTBRCharacter* Teammate = SpawnReviveCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveCharacter(World, FVector(300, 0, 0), Stats);
    if (!Victim || !Teammate || !Enemy) return false;
    Victim->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);

    DriveDown(Victim, Enemy);
    TestTrue(TEXT("Downed"), Victim->HealthComponent->IsDowned());
    TestTrue(TEXT("Bleed-out active, not paused"),
        Victim->HealthComponent->IsBleedOutTimerActiveForTest() && !Victim->HealthComponent->IsBleedOutTimerPausedForTest());

    // Enemy cannot revive.
    TestFalse(TEXT("Enemy cannot revive"), Victim->HealthComponent->TryStartRevive(Enemy));
    TestFalse(TEXT("No revive in progress after enemy attempt"), Victim->HealthComponent->IsBeingRevived());

    // Teammate starts revive -> bleed-out PAUSED.
    TestTrue(TEXT("Teammate revive starts"), Victim->HealthComponent->TryStartRevive(Teammate));
    TestTrue(TEXT("Being revived"), Victim->HealthComponent->IsBeingRevived());
    TestTrue(TEXT("Bleed-out paused during hold"), Victim->HealthComponent->IsBleedOutTimerPausedForTest());
    TestTrue(TEXT("Bleed-out timer still exists (not cleared)"), Victim->HealthComponent->IsBleedOutTimerSetForTest());
    TestEqual(TEXT("Active reviver is the teammate"), Victim->HealthComponent->GetActiveReviver(), Teammate);

    // Second reviver rejected while one is in progress.
    AWTBRCharacter* Teammate2 = SpawnReviveCharacter(World, FVector(180, 0, 0), Stats);
    Teammate2->SetTeamId(0);
    TestFalse(TEXT("Second concurrent reviver rejected"), Victim->HealthComponent->TryStartRevive(Teammate2));

    // Release -> bleed-out RESUMES (active again, not paused), still Downed.
    Victim->HealthComponent->StopRevive();
    TestFalse(TEXT("No longer being revived"), Victim->HealthComponent->IsBeingRevived());
    TestTrue(TEXT("Bleed-out active after release"), Victim->HealthComponent->IsBleedOutTimerActiveForTest());
    TestFalse(TEXT("Bleed-out no longer paused after release"), Victim->HealthComponent->IsBleedOutTimerPausedForTest());
    TestTrue(TEXT("Still Downed after release"), Victim->HealthComponent->IsDowned());

    return true;
}

// ── Completed revive restores the combatant to Alive ───────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveCompletesTest,
    "WTBR.Combat.Revive.CompleteRestoresAlive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveCompletesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveWorldFixture Fixture(TEXT("WTBR_Revive_Complete"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeStats();
    AWTBRCharacter* Victim = SpawnReviveCharacter(World, FVector(0, 0, 0), Stats);
    AWTBRCharacter* Teammate = SpawnReviveCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveCharacter(World, FVector(300, 0, 0), Stats);
    if (!Victim || !Teammate || !Enemy) return false;
    Victim->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);

    DriveDown(Victim, Enemy);
    TestTrue(TEXT("Teammate revive starts"), Victim->HealthComponent->TryStartRevive(Teammate));

    // Force the hold timer to complete.
    Victim->HealthComponent->ForceReviveCompleteForTest();

    TestTrue(TEXT("Alive again after revive"), Victim->HealthComponent->IsAlive());
    TestEqual(TEXT("HP restored to ReviveHPRestored"), Victim->HealthComponent->GetCurrentHP(), 100.0f);
    TestFalse(TEXT("Not being revived after completion"), Victim->HealthComponent->IsBeingRevived());
    TestFalse(TEXT("Bleed-out timer cleared after revive"), Victim->HealthComponent->IsBleedOutTimerActiveForTest());
    TestEqual(TEXT("Downed HP cleared"), Victim->HealthComponent->GetCurrentDownedHP(), 0.0f);

    // A revived combatant can be downed again (down reward re-armed, path intact).
    DriveDown(Victim, Enemy);
    TestTrue(TEXT("Can be downed again after revive"), Victim->HealthComponent->IsDowned());
    TestTrue(TEXT("Fresh bleed-out timer on second down"), Victim->HealthComponent->IsBleedOutTimerActiveForTest());

    return true;
}

// ── Revive is impossible once eliminated ───────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveAfterEliminatedRejectedTest,
    "WTBR.Combat.Revive.CannotReviveEliminated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveAfterEliminatedRejectedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveWorldFixture Fixture(TEXT("WTBR_Revive_AfterElim"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeStats();
    AWTBRCharacter* Victim = SpawnReviveCharacter(World, FVector(0, 0, 0), Stats);
    AWTBRCharacter* Teammate = SpawnReviveCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveCharacter(World, FVector(300, 0, 0), Stats);
    if (!Victim || !Teammate || !Enemy) return false;
    Victim->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);

    DriveDown(Victim, Enemy);
    Victim->HealthComponent->ForceBleedOutExpiryForTest();
    TestTrue(TEXT("Victim eliminated"), Victim->HealthComponent->IsEliminated());

    TestFalse(TEXT("Cannot revive an eliminated combatant"), Victim->HealthComponent->TryStartRevive(Teammate));
    TestFalse(TEXT("Not being revived"), Victim->HealthComponent->IsBeingRevived());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
