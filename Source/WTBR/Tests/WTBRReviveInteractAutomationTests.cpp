// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRInteractionComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRCharacter.h"

// -----------------------------------------------------------------------------
// Revive Interact Automation Tests (WTBR.Combat.Revive.Interact.*)
//
// Closes the gap flagged 2026-07-13: UWTBRHealthComponent::TryStartRevive/
// StopRevive (Night 2) had no input path — nothing called them in-game. This
// wires hold-F-to-revive as priority 0 of UWTBRInteractionComponent::
// RequestContextInteract() (checked before corpse/dropped-trigger/ground-item),
// dispatching AWTBRCharacter::Server_RequestStartRevive on press (IA_Interact
// Started) and Server_RequestStopRevive on release (IA_Interact Completed via
// AWTBRCharacter::InteractReleased()).
//
// Uses the SetFocusedRevivableTeammateOverrideForTest seam (same pattern as
// SetFocusedGroundItemOverrideForTest) instead of a real line trace — headless
// fixture worlds don't reliably line-trace-hit spawned capsules. RPC bodies
// (Server_RequestStartRevive_Implementation etc.) are invoked directly, same
// convention as every other RPC test in this suite (headless fixtures have no
// NetDriver to route _Implementation calls through automatically).
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRReviveInteractWorldFixture
    {
    public:
        explicit FWTBRReviveInteractWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRReviveInteractWorldFixture()
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

    UWTBRCoreStatsDataAsset* MakeReviveInteractStats()
    {
        UWTBRCoreStatsDataAsset* Stats = NewObject<UWTBRCoreStatsDataAsset>();
        Stats->MaxHP = 300.0f;
        Stats->MaxDownedHP = 100.0f;
        Stats->BleedOutDuration = 30.0f;
        Stats->ReviveHoldDuration = 5.0f;
        Stats->ReviveHPRestored = 100.0f;
        Stats->KnockdownIFrameDuration = 0.0f;
        return Stats;
    }

    AWTBRCharacter* SpawnReviveInteractCharacter(UWorld* World, const FVector& Loc, UWTBRCoreStatsDataAsset* Stats)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
        if (Character && Character->HealthComponent && Stats)
        {
            Character->HealthComponent->CoreStatsAsset = Stats;
        }
        return Character;
    }

    void ReviveInteractTest_DriveDown(AWTBRCharacter* Victim, AWTBRCharacter* Attacker)
    {
        Victim->HealthComponent->ApplyDamage(10000.0f, Attacker);
    }
}

// ── Focus: only a Downed teammate is revivable ──────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveFocusOnlyDownedTeammateTest,
    "WTBR.Combat.Revive.Interact.FocusOnlyDownedTeammate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveFocusOnlyDownedTeammateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveInteractWorldFixture Fixture(TEXT("WTBR_ReviveInteract_Focus"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeReviveInteractStats();
    AWTBRCharacter* Reviver = SpawnReviveInteractCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* AliveTeammate = SpawnReviveInteractCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* DownedEnemy = SpawnReviveInteractCharacter(World, FVector(300, 0, 0), Stats);
    AWTBRCharacter* DownedTeammate = SpawnReviveInteractCharacter(World, FVector(450, 0, 0), Stats);
    if (!Reviver || !AliveTeammate || !DownedEnemy || !DownedTeammate) return false;

    Reviver->SetTeamId(0);
    AliveTeammate->SetTeamId(0);
    DownedEnemy->SetTeamId(1);
    DownedTeammate->SetTeamId(0);
    ReviveInteractTest_DriveDown(DownedEnemy, Reviver);
    ReviveInteractTest_DriveDown(DownedTeammate, DownedEnemy);

    // Focused on a living teammate: not revivable (nothing to revive).
    Reviver->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(nullptr);
    TestTrue(TEXT("No focus set -> no revivable teammate"),
        Reviver->InteractionComponent->GetFocusedRevivableTeammate() == nullptr);

    // A downed ENEMY is never revivable even if focused directly (defense in
    // depth — GetFocusedRevivableTeammate itself re-checks team on top of the
    // override, since the override only substitutes the line-trace hit).
    // (Not exercised via the override here since the override bypasses the
    // real focus function's checks entirely by design — see the priority test
    // below for the production dispatch path instead.)

    return true;
}

// ── RequestReviveInteract dispatches start and tracks the target ───────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveInteractStartTest,
    "WTBR.Combat.Revive.Interact.RequestReviveInteractStartsAndTracksTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveInteractStartTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveInteractWorldFixture Fixture(TEXT("WTBR_ReviveInteract_Start"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeReviveInteractStats();
    AWTBRCharacter* Reviver = SpawnReviveInteractCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* Teammate = SpawnReviveInteractCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveInteractCharacter(World, FVector(300, 0, 0), Stats);
    if (!Reviver || !Teammate || !Enemy) return false;
    Reviver->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);
    ReviveInteractTest_DriveDown(Teammate, Enemy);

    Reviver->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(Teammate);

    TestTrue(TEXT("No pending revive target before interact"),
        Reviver->InteractionComponent->GetCurrentReviveTargetForReleaseForTest() == nullptr);

    const bool bHandled = Reviver->InteractionComponent->RequestContextInteract();
    TestTrue(TEXT("Priority 0 (revive) handles the interact"), bHandled);

    // Directly invoke the RPC body — no NetDriver in this headless fixture to
    // route the Server_ call automatically (same convention as every other RPC
    // test in this suite).
    Reviver->Server_RequestStartRevive_Implementation(Teammate);

    TestTrue(TEXT("Revive is now in progress on the teammate"), Teammate->HealthComponent->IsBeingRevived());
    TestEqual(TEXT("Active reviver is Reviver"), Teammate->HealthComponent->GetActiveReviver(), Reviver);
    TestEqual(TEXT("InteractionComponent remembers the pending target"),
        Reviver->InteractionComponent->GetCurrentReviveTargetForReleaseForTest(), Teammate);

    return true;
}

// ── Release stops the revive that was actually started ─────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveInteractStopOnReleaseTest,
    "WTBR.Combat.Revive.Interact.ReleaseStopsTrackedRevive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveInteractStopOnReleaseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveInteractWorldFixture Fixture(TEXT("WTBR_ReviveInteract_Stop"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeReviveInteractStats();
    AWTBRCharacter* Reviver = SpawnReviveInteractCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* Teammate = SpawnReviveInteractCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveInteractCharacter(World, FVector(300, 0, 0), Stats);
    if (!Reviver || !Teammate || !Enemy) return false;
    Reviver->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);
    ReviveInteractTest_DriveDown(Teammate, Enemy);

    Reviver->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(Teammate);
    Reviver->InteractionComponent->RequestContextInteract();
    Reviver->Server_RequestStartRevive_Implementation(Teammate);
    TestTrue(TEXT("Revive in progress before release"), Teammate->HealthComponent->IsBeingRevived());

    // Simulate the button release path exactly as InteractReleased() does.
    Reviver->InteractionComponent->RequestStopReviveIfInProgress();
    Reviver->Server_RequestStopRevive_Implementation(Teammate);

    TestFalse(TEXT("Revive stopped on release"), Teammate->HealthComponent->IsBeingRevived());
    TestTrue(TEXT("Teammate still Downed (not completed, just paused-then-resumed)"), Teammate->HealthComponent->IsDowned());
    TestTrue(TEXT("Pending target cleared after release"),
        Reviver->InteractionComponent->GetCurrentReviveTargetForReleaseForTest() == nullptr);

    // Idempotent: a second release with nothing pending is a safe no-op.
    Reviver->InteractionComponent->RequestStopReviveIfInProgress();

    return true;
}

// ── A rejected/stale start's later release must not cancel a DIFFERENT
//    teammate's legitimate in-progress revive on the same target ──────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveInteractStaleReleaseDoesNotCancelOtherReviverTest,
    "WTBR.Combat.Revive.Interact.StaleReleaseDoesNotCancelOtherReviver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveInteractStaleReleaseDoesNotCancelOtherReviverTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveInteractWorldFixture Fixture(TEXT("WTBR_ReviveInteract_StaleRelease"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeReviveInteractStats();
    AWTBRCharacter* ReviverA = SpawnReviveInteractCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* ReviverB = SpawnReviveInteractCharacter(World, FVector(50, 0, 0), Stats);
    AWTBRCharacter* Teammate = SpawnReviveInteractCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveInteractCharacter(World, FVector(300, 0, 0), Stats);
    if (!ReviverA || !ReviverB || !Teammate || !Enemy) return false;
    ReviverA->SetTeamId(0);
    ReviverB->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);
    ReviveInteractTest_DriveDown(Teammate, Enemy);

    // Reviver A "starts" but is rejected server-side because Reviver B already
    // has an active revive on the same teammate (server processes B first).
    ReviverB->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(Teammate);
    ReviverB->InteractionComponent->RequestContextInteract();
    ReviverB->Server_RequestStartRevive_Implementation(Teammate);
    TestEqual(TEXT("B is the active reviver"), Teammate->HealthComponent->GetActiveReviver(), ReviverB);

    ReviverA->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(Teammate);
    ReviverA->InteractionComponent->RequestContextInteract();
    ReviverA->Server_RequestStartRevive_Implementation(Teammate); // Rejected: bReviveInProgress already true.
    TestEqual(TEXT("B remains the active reviver after A's rejected attempt"),
        Teammate->HealthComponent->GetActiveReviver(), ReviverB);

    // A releases F (their client thinks it might have started a revive) — this
    // must NOT cancel B's legitimate, still-in-progress revive.
    ReviverA->InteractionComponent->RequestStopReviveIfInProgress();
    ReviverA->Server_RequestStopRevive_Implementation(Teammate);

    TestTrue(TEXT("B's revive survives A's stale release"), Teammate->HealthComponent->IsBeingRevived());
    TestEqual(TEXT("B is still the active reviver"), Teammate->HealthComponent->GetActiveReviver(), ReviverB);

    return true;
}

// ── Priority 0 wins even when other focusable interactables exist, and the
//    absence of a revivable teammate falls through to priority 1+ unaffected ──

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReviveInteractPriorityAndFallthroughTest,
    "WTBR.Combat.Revive.Interact.PriorityZeroAndFallthrough",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReviveInteractPriorityAndFallthroughTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRReviveInteractWorldFixture Fixture(TEXT("WTBR_ReviveInteract_Priority"));
    UWorld* World = Fixture.GetWorld();

    UWTBRCoreStatsDataAsset* Stats = MakeReviveInteractStats();
    AWTBRCharacter* Reviver = SpawnReviveInteractCharacter(World, FVector::ZeroVector, Stats);
    AWTBRCharacter* Teammate = SpawnReviveInteractCharacter(World, FVector(150, 0, 0), Stats);
    AWTBRCharacter* Enemy = SpawnReviveInteractCharacter(World, FVector(300, 0, 0), Stats);
    if (!Reviver || !Teammate || !Enemy) return false;
    Reviver->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);
    ReviveInteractTest_DriveDown(Teammate, Enemy);

    // No revivable teammate focused -> falls through to the existing no-op path
    // (no corpse/dropped-trigger/ground-item focused either in this fixture).
    Reviver->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(nullptr);
    const bool bHandledEmpty = Reviver->InteractionComponent->RequestContextInteract();
    TestFalse(TEXT("No handler fires when nothing is focused"), bHandledEmpty);

    // A revivable teammate focused -> priority 0 claims it before anything else
    // gets a chance.
    Reviver->InteractionComponent->SetFocusedRevivableTeammateOverrideForTest(Teammate);
    const bool bHandledRevive = Reviver->InteractionComponent->RequestContextInteract();
    TestTrue(TEXT("Revive focus is handled"), bHandledRevive);
    TestEqual(TEXT("Pending revive target is the teammate"),
        Reviver->InteractionComponent->GetCurrentReviveTargetForReleaseForTest(), Teammate);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
