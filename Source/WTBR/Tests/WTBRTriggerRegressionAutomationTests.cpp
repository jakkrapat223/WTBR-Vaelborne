// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRArcvenTrigger.h"
#include "Trigger/WTBRAcervynTrigger.h"
#include "Trigger/WTBRFeryxTrigger.h"
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Trigger/WTBRMantornTrigger.h"
#include "Trigger/WTBRPiercexTrigger.h"
#include "Trigger/WTBRSoluxTrigger.h"
#include "Trigger/WTBRTelornTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Trigger/WTBRVexornTrigger.h"
#include "Trigger/WTBRNexilTrigger.h"
#include "Actors/WTBRNexilWireActor.h"
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"
#include "AI/WTBRBasicBotCharacter.h"
#include "AI/WTBRBasicBotController.h"

// -----------------------------------------------------------------------------
// Trigger Regression Automation Tests (WTBR.Trigger.*)
//
// First-ever automation coverage for Mantorn, Arcven, Telorn, and Piercex —
// flagged 2026-07-13 as having zero test coverage in the 15P final checklist.
// Owner scoped this as a pure gameplay-logic regression pass (VFX/map/some
// other Triggers still incomplete, so UI work is paused but this backend work
// isn't blocked by any of that).
//
// Each trigger's UWTBRTriggerBase-derived object is a plain UObject (no
// Actor/Component lifecycle) — NewObject + InitializeTrigger + Activate()
// called directly, exactly mirroring how AWTBRCharacter::ExecuteServerTriggerInput
// invokes it in production, with no input/animation layer in between. Same
// headless world-fixture conventions as every other file in this suite;
// AWTBRProjectileBase spawns fine headlessly (SpawnActorDeferred/FinishSpawning/
// Launch don't require an initialized physics scene) since only its config
// fields, not its actual flight, are under test here.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRTriggerRegressionWorldFixture
    {
    public:
        explicit FWTBRTriggerRegressionWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRTriggerRegressionWorldFixture()
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

    AWTBRCharacter* TriggerRegressionTest_SpawnCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* TriggerRegressionTest_MakeDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->VaelCostPerUse = 10.0f;
        return DataAsset;
    }

    int32 TriggerRegressionTest_CountProjectiles(UWorld* World)
    {
        int32 Count = 0;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                ++Count;
            }
        }
        return Count;
    }

    // Returns the single spawned projectile, or nullptr if not exactly one.
    AWTBRProjectileBase* TriggerRegressionTest_FindSoleProjectile(UWorld* World)
    {
        AWTBRProjectileBase* Found = nullptr;
        int32 Count = 0;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                Found = *It;
                ++Count;
            }
        }
        return (Count == 1) ? Found : nullptr;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Mantorn — Feryx + Feryx composite FORM behavior executor (WhipSlash / SpinSlash)
//
// Mantorn is no longer a standalone equippable spin Trigger. It is the in-form
// behavior object the TriggerSetComponent drives while the player is in Mantorn
// form (see WTBRMantornTrigger.h). These tests exercise the two moves directly,
// mirroring how AWTBRCharacter::HandleMantornFormInput calls them on the server.
// The transform enter/exit + dual-hold gesture + Server RPC path is inherently
// networked (client gesture → replicated state) and needs a real GameState join
// flow, so it is deferred to the PIE Human Test Gate rather than covered here.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornSpinDamagesTargetInRadiusTest,
    "WTBR.Trigger.Mantorn.SpinDamagesTargetInRadiusNotOutOfRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornSpinDamagesTargetInRadiusTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornSpinRadius"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MantornParams.SpinVaelCost   = 12.0f;
    DataAsset->MantornParams.ActionCooldown = 999.0f; // Keep cooldown from interfering here.

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* InRange = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    AWTBRCharacter* OutOfRange = TriggerRegressionTest_SpawnCharacter(World, FVector(1000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Owner spawns"), Owner);
    TestNotNull(TEXT("In-range target spawns"), InRange);
    TestNotNull(TEXT("Out-of-range target spawns"), OutOfRange);
    if (!Owner || !InRange || !OutOfRange || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float OwnerHPBefore = Owner->HealthComponent->GetCurrentHP();
    const float InRangeHPBefore = InRange->HealthComponent->GetCurrentHP();
    const float OutOfRangeHPBefore = OutOfRange->HealthComponent->GetCurrentHP();

    const bool bSpun = Mantorn->SpinSlash();

    TestTrue(TEXT("SpinSlash succeeds"), bSpun);
    TestEqual(TEXT("In-range target takes SpinDamage"),
        InRange->HealthComponent->GetCurrentHP(), InRangeHPBefore - 80.0f);
    TestEqual(TEXT("Out-of-range target untouched"),
        OutOfRange->HealthComponent->GetCurrentHP(), OutOfRangeHPBefore);
    TestEqual(TEXT("Owner does not damage self (FilterHits excludes instigator)"),
        Owner->HealthComponent->GetCurrentHP(), OwnerHPBefore);
    TestEqual(TEXT("Vael consumed by SpinVaelCost"),
        Owner->VaelComponent->GetCurrentVael(), 88.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornWhipDamagesForwardTargetTest,
    "WTBR.Trigger.Mantorn.WhipDamagesForwardTargetNotBehind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornWhipDamagesForwardTargetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornWhip"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.ForwardOffset    = 120.0f;
    DataAsset->MeleeHitbox.CapsuleHalfHeight = 50.0f;
    DataAsset->MantornParams.WhipReach      = 450.0f;
    DataAsset->MantornParams.WhipRadius     = 60.0f;
    DataAsset->MantornParams.WhipDamage     = 90.0f;
    DataAsset->MantornParams.WhipVaelCost   = 8.0f;
    DataAsset->MantornParams.ActionCooldown = 999.0f;

    // No controller possesses the test pawn → whip falls back to actor forward
    // (+X for a zero rotator), so a +X target is "in front".
    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Front = TriggerRegressionTest_SpawnCharacter(World, FVector(250.0f, 0.0f, 0.0f));
    AWTBRCharacter* Behind = TriggerRegressionTest_SpawnCharacter(World, FVector(-250.0f, 0.0f, 0.0f));
    if (!Owner || !Front || !Behind || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float FrontHPBefore = Front->HealthComponent->GetCurrentHP();
    const float BehindHPBefore = Behind->HealthComponent->GetCurrentHP();

    const bool bWhipped = Mantorn->WhipSlash();

    TestTrue(TEXT("WhipSlash succeeds"), bWhipped);
    TestEqual(TEXT("Forward target takes WhipDamage"),
        Front->HealthComponent->GetCurrentHP(), FrontHPBefore - 90.0f);
    TestEqual(TEXT("Target behind is untouched (forward-only sweep)"),
        Behind->HealthComponent->GetCurrentHP(), BehindHPBefore);
    TestEqual(TEXT("Vael consumed by WhipVaelCost"),
        Owner->VaelComponent->GetCurrentVael(), 92.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornCooldownBlocksImmediateReactionTest,
    "WTBR.Trigger.Mantorn.InFormCooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornCooldownBlocksImmediateReactionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MantornParams.SpinVaelCost   = 12.0f;
    DataAsset->MantornParams.ActionCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Target = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    if (!Owner || !Target || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First spin succeeds"), Mantorn->SpinSlash());
    TestTrue(TEXT("On cooldown immediately after"), Mantorn->IsOnCooldown());

    const float HPAfterFirstSpin = Target->HealthComponent->GetCurrentHP();
    const float VaelAfterFirstSpin = Owner->VaelComponent->GetCurrentVael();
    const bool bSecondSpin = Mantorn->SpinSlash();

    TestFalse(TEXT("Second immediate spin rejected by cooldown"), bSecondSpin);
    TestEqual(TEXT("No additional damage from the rejected second spin"),
        Target->HealthComponent->GetCurrentHP(), HPAfterFirstSpin);
    TestEqual(TEXT("No additional Vael consumed by the rejected second spin"),
        Owner->VaelComponent->GetCurrentVael(), VaelAfterFirstSpin);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornFriendlyFireBlockedTest,
    "WTBR.Trigger.Mantorn.FriendlyFireBlockedByHealthComponentGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornFriendlyFireBlockedTest::RunTest(const FString& /*Parameters*/)
{
    // Mantorn itself has no team check (FilterHits only excludes the instigator) —
    // this locks that the shared UWTBRHealthComponent::ApplyDamage friendly-fire
    // gate (added for TeamThree15P/BR, 2026-07-13) still protects teammates hit
    // by it, without Mantorn needing its own team-awareness.
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornFriendlyFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MantornParams.SpinVaelCost   = 12.0f;
    DataAsset->MantornParams.ActionCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Teammate = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    AWTBRCharacter* Enemy = TriggerRegressionTest_SpawnCharacter(World, FVector(-150.0f, 0.0f, 0.0f));
    if (!Owner || !Teammate || !Enemy || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
    Owner->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float TeammateHPBefore = Teammate->HealthComponent->GetCurrentHP();
    const float EnemyHPBefore = Enemy->HealthComponent->GetCurrentHP();

    Mantorn->SpinSlash();

    TestEqual(TEXT("Teammate untouched (friendly fire blocked)"),
        Teammate->HealthComponent->GetCurrentHP(), TeammateHPBefore);
    TestEqual(TEXT("Enemy takes full SpinDamage"),
        Enemy->HealthComponent->GetCurrentHP(), EnemyHPBefore - 80.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornInsufficientVaelBlocksActionTest,
    "WTBR.Trigger.Mantorn.InsufficientVaelBlocksInFormAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornInsufficientVaelBlocksActionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MantornParams.SpinVaelCost   = 12.0f;
    DataAsset->MantornParams.ActionCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Target = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    if (!Owner || !Target || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f); // Below SpinVaelCost=12.

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float TargetHPBefore = Target->HealthComponent->GetCurrentHP();
    const bool bSpun = Mantorn->SpinSlash();

    TestFalse(TEXT("Spin rejected: insufficient Vael"), bSpun);
    TestEqual(TEXT("Vael unchanged (TryConsumeVael fails atomically)"),
        Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("Target untouched by the rejected spin"),
        Target->HealthComponent->GetCurrentHP(), TargetHPBefore);
    TestFalse(TEXT("Not left on cooldown by a rejected action"), Mantorn->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornRejectsWithoutDataAssetTest,
    "WTBR.Trigger.Mantorn.RejectsActionWithoutDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornRejectsWithoutDataAssetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornNoData"));
    UWorld* World = Fixture.GetWorld();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, nullptr);

    TestFalse(TEXT("SpinSlash rejected with no DataAsset"), Mantorn->SpinSlash());
    TestFalse(TEXT("WhipSlash rejected with no DataAsset"), Mantorn->WhipSlash());

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Arcven — arc-wave projectile "melee" (single or dual-wield spread)
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenSingleFireTest,
    "WTBR.Trigger.Arcven.SingleFireSpawnsOneProjectileAndConsumesVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenSingleFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenSingle"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->ArcvenParams.ArcDamage = 120.0f;
    DataAsset->ArcvenParams.ArcRange = 800.0f;
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Arcven->Activate(FInputActionValue(), /*bIsDualWield=*/false);

    TestTrue(TEXT("Single-wield activation succeeds"), bActivated);
    TestEqual(TEXT("Vael consumed by VaelCostPerUse"), Owner->VaelComponent->GetCurrentVael(), 90.0f);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one arc wave spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Projectile damage = ArcDamage"), Proj->BaseDamage, 120.0f);
        TestEqual(TEXT("Projectile MaxRange = ArcRange"), Proj->MaxRange, 800.0f);
        TestEqual(TEXT("Projectile category = Melee (Arcven is DA-categorized Melee)"),
            (int32)Proj->OwnerCategory, (int32)ETriggerCategory::Melee);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenDualWieldFireTest,
    "WTBR.Trigger.Arcven.DualWieldSpawnsTwoHalfDamageWaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenDualWieldFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenDual"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->ArcvenParams.DualArcTotalDamage = 200.0f;
    DataAsset->ArcvenParams.DualWaveSpreadAngle = 15.0f;
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Arcven->Activate(FInputActionValue(), /*bIsDualWield=*/true);

    TestTrue(TEXT("Dual-wield activation succeeds"), bActivated);
    TestEqual(TEXT("Vael consumed once regardless of wave count (single VaelCostPerUse)"),
        Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Two arc waves spawned"), TriggerRegressionTest_CountProjectiles(World), 2);

    for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            TestEqual(TEXT("Each dual wave carries half of DualArcTotalDamage"), It->BaseDamage, 100.0f);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Arcven.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f); // Below VaelCostPerUse=10.

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Arcven->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Activation rejected: insufficient Vael"), bActivated);
    TestEqual(TEXT("Vael unchanged (TryConsumeVael fails atomically)"),
        Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestFalse(TEXT("Not left on cooldown by a rejected activation"), Arcven->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenMissingProjectileClassTest,
    "WTBR.Trigger.Arcven.MissingProjectileClassSilentlyFiresNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenMissingProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    // Locks existing behavior (see WTBRArcvenTrigger.cpp): with no
    // ArcProjectileClass set, FireArcWave logs a warning and silently returns —
    // Activate() still consumes Vael, starts cooldown, and returns true, even
    // though zero projectiles were spawned. Flagged as a real gap, not fixed
    // here (out of scope for a regression-coverage pass).
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenNoProjClass"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    // ArcProjectileClass deliberately left unset.
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    AddExpectedError(TEXT("ArcProjectileClass not set"), EAutomationExpectedErrorFlags::Contains, 1);
    const bool bActivated = Arcven->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Activate still reports success (existing quirk, not fixed here)"), bActivated);
    TestEqual(TEXT("Vael still consumed despite no projectile spawning"),
        Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("No projectile actually spawned"), TriggerRegressionTest_CountProjectiles(World), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenCooldownBlocksImmediateReactivationTest,
    "WTBR.Trigger.Arcven.CooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenCooldownBlocksImmediateReactivationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First activation succeeds"), Arcven->Activate(FInputActionValue(), false));
    const bool bSecondActivate = Arcven->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Second immediate activation rejected by cooldown"), bSecondActivate);
    TestEqual(TEXT("Vael not consumed twice"), Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Still only one projectile from the first shot"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Arcven.FireChargedWave — Lacern-hold charge tiers (Senkū Trigger Option).
// FireChargedWave bypasses Arcven's own cooldown/Vael/Activate flow entirely —
// the caller (AWTBRCharacter::HandleLacernHoldOptionInput) already resolves
// tap-vs-hold timing and charges Vael against the Option's own DataAsset.
// That input-routing/timing layer is networked (per-slot Option attachment,
// press/release timing) and is deferred to the PIE Human Test Gate, same as
// Mantorn's transform enter/exit — these tests cover the fire executor only.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenChargedWaveWeakTierTest,
    "WTBR.Trigger.Arcven.FireChargedWave.WeakTierUsesCallerDamageAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenChargedWaveWeakTierTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenChargedWeak"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    // Weak tier values per the caller's charge-time classification (0.2s-1.0s hold).
    Arcven->FireChargedWave(/*Damage=*/100.0f, /*Range=*/650.0f, /*bIsDualWield=*/false);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one wave spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = caller's weak-tier value, not ArcDamage"), Proj->BaseDamage, 100.0f);
        TestEqual(TEXT("MaxRange = caller's weak-tier value, not ArcRange"), Proj->MaxRange, 650.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenChargedWaveFullTierTest,
    "WTBR.Trigger.Arcven.FireChargedWave.FullTierUsesCallerDamageAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenChargedWaveFullTierTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenChargedFull"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    // Full-charge tier — matches the GDD-locked ArcDamage/ArcRange (120/800),
    // but FireChargedWave takes them as explicit params from the caller.
    Arcven->FireChargedWave(/*Damage=*/120.0f, /*Range=*/800.0f, /*bIsDualWield=*/false);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one wave spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = full-charge tier"), Proj->BaseDamage, 120.0f);
        TestEqual(TEXT("MaxRange = full-charge tier"), Proj->MaxRange, 800.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenChargedWaveDualSplitsHalfDamageTest,
    "WTBR.Trigger.Arcven.FireChargedWave.DualWieldSplitsIntoTwoHalfDamageWaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenChargedWaveDualSplitsHalfDamageTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenChargedDual"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;
    DataAsset->ArcvenParams.DualWaveSpreadAngle = 15.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    // Double Senkū (GDD §5.4): 200 total = 100/line at the full-charge tier.
    Arcven->FireChargedWave(/*Damage=*/200.0f, /*Range=*/800.0f, /*bIsDualWield=*/true);

    TestEqual(TEXT("Two waves spawned (one per hand)"), TriggerRegressionTest_CountProjectiles(World), 2);
    for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            TestEqual(TEXT("Each wave carries half of the caller's Damage"), It->BaseDamage, 100.0f);
            TestEqual(TEXT("Each wave keeps the caller's full Range (not halved)"), It->MaxRange, 800.0f);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenChargedWaveRejectsWithoutDataAssetTest,
    "WTBR.Trigger.Arcven.FireChargedWave.RejectsWithoutDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenChargedWaveRejectsWithoutDataAssetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenChargedNoData"));
    UWorld* World = Fixture.GetWorld();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, nullptr);

    Arcven->FireChargedWave(100.0f, 650.0f, false);

    TestEqual(TEXT("No projectile spawned with no DataAsset"), TriggerRegressionTest_CountProjectiles(World), 0);

    return true;
}

// Arc waves are Melee-category sweep energy and must NEVER fragment into cubes —
// FireArcWave clears the projectile base's default CubeSplitCount (4) to 0. This
// (plus the OnBulletClash Melee guard, whose physical overlap path is PIE-only)
// stops two arc waves that meet mid-air from cube-splitting and damaging both
// casters. Locks fix for the 2026-07-14 "arc wave hits caster" PIE bug.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcvenChargedWaveNeverCubeSplitsTest,
    "WTBR.Trigger.Arcven.FireChargedWave.ProjectileIsMeleeAndNeverCubeSplits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcvenChargedWaveNeverCubeSplitsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_ArcvenNoCubeSplit"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->ArcvenParams.ArcProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->ArcvenParams.ArcWaveSpeed = 1200.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRArcvenTrigger* Arcven = NewObject<UWTBRArcvenTrigger>(Owner);
    Arcven->InitializeTrigger(Owner, DataAsset);

    Arcven->FireChargedWave(/*Damage=*/120.0f, /*Range=*/800.0f, /*bIsDualWield=*/false);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one wave spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Arc wave carries Melee category (excluded from Gunner clash path)"),
            static_cast<int32>(Proj->OwnerCategory), static_cast<int32>(ETriggerCategory::Melee));
        TestEqual(TEXT("Arc wave CubeSplitCount cleared to 0 (never fragments)"),
            Proj->CubeSplitCount, 0);
    }

    return true;
}

// SpawnCubeSplits() on a projectile with CubeSplitCount == 0 must be a no-op —
// the defensive floor that makes the arc-wave CubeSplitCount=0 clear meaningful.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRProjectileSpawnCubeSplitsZeroCountIsNoOpTest,
    "WTBR.Actors.Projectile.SpawnCubeSplitsWithZeroCountSpawnsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRProjectileSpawnCubeSplitsZeroCountIsNoOpTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Projectile_CubeSplitZero"));
    UWorld* World = Fixture.GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    TestNotNull(TEXT("Projectile spawns"), Proj);
    if (!Proj) return false;

    Proj->CubeSplitCount = 0;
    Proj->SpawnCubeSplits();

    TestEqual(TEXT("Zero-count SpawnCubeSplits spawns no fragments (still just the one projectile)"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Base Gunners — Solux / Fulgrix / Venyx / Acervyn (fire/cooldown/Vael contract only; Serpveil is the 4th composite-source archetype and is tested separately; Acervyn is the standalone NonCombinable advanced Gunner excluded from the composite resolver, not a composite-source archetype).
// Tap = Activate-on-press (unlike the snipers' aim-then-fire): validate DataAsset
// + cooldown + Vael, then fire. These lock the shared fire/cooldown/Vael contract
// each gunner relies on. Homing target acquisition (Venyx/Acervyn sphere overlap)
// needs a real physics scene, which headless fixtures lack — those find zero
// overlaps, so "acquires target → homes" is deferred to the PIE gate; here we
// confirm the shot still spawns straight with no target, which is the fallback.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSoluxFireConfiguresProjectileTest,
    "WTBR.Trigger.Solux.FireConfiguresProjectileAndConsumesVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSoluxFireConfiguresProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_SoluxFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 8.0f;
    DataAsset->SoluxParams.SoluxDamage = 30.0f;
    DataAsset->SoluxParams.SoluxSpeed = 3000.0f;
    DataAsset->SoluxParams.SoluxFireCooldown = 0.5f;
    DataAsset->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
    Solux->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Solux->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Solux fires on press"), bActivated);
    TestEqual(TEXT("Vael consumed by VaelCostPerUse"), Owner->VaelComponent->GetCurrentVael(), 92.0f);
    TestTrue(TEXT("Solux is on cooldown after firing"), Solux->IsOnCooldown());

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one bullet spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = SoluxDamage"), Proj->BaseDamage, 30.0f);
        TestEqual(TEXT("Category = Gunner"), (int32)Proj->OwnerCategory, (int32)ETriggerCategory::Gunner);
        TestFalse(TEXT("Solux bullet does not explode"), Proj->bExplodeOnImpact);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSoluxInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Solux.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSoluxInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_SoluxNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 20.0f;
    DataAsset->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
    Solux->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Solux->Activate(FInputActionValue(), false);
    TestFalse(TEXT("Fire rejected when Vael insufficient (fail-closed on press)"), bActivated);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestFalse(TEXT("Not left on cooldown"), Solux->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSoluxCooldownBlocksReactivationTest,
    "WTBR.Trigger.Solux.CooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSoluxCooldownBlocksReactivationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_SoluxCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 5.0f;
    DataAsset->SoluxParams.SoluxFireCooldown = 5.0f;
    DataAsset->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
    Solux->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First fire succeeds"), Solux->Activate(FInputActionValue(), false));
    const bool bSecond = Solux->Activate(FInputActionValue(), false);
    TestFalse(TEXT("Second fire blocked by cooldown"), bSecond);
    TestEqual(TEXT("Still only one bullet spawned"), TriggerRegressionTest_CountProjectiles(World), 1);
    TestEqual(TEXT("Vael consumed exactly once"), Owner->VaelComponent->GetCurrentVael(), 95.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFulgrixFireConfiguresExplosiveTest,
    "WTBR.Trigger.Fulgrix.FireConfiguresExplosiveProjectile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFulgrixFireConfiguresExplosiveTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_FulgrixFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 12.0f;
    DataAsset->FulgrixParams.FulgrixDamage = 80.0f;
    DataAsset->FulgrixParams.FulgrixSpeed = 2500.0f;
    DataAsset->FulgrixParams.FulgrixExplosionRadius = 300.0f;
    DataAsset->FulgrixParams.FulgrixFireCooldown = 0.8f;
    DataAsset->FulgrixParams.FulgrixProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRFulgrixTrigger* Fulgrix = NewObject<UWTBRFulgrixTrigger>(Owner);
    Fulgrix->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Fulgrix->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Fulgrix fires on press"), bActivated);
    TestEqual(TEXT("Vael consumed"), Owner->VaelComponent->GetCurrentVael(), 88.0f);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one shell spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = FulgrixDamage"), Proj->BaseDamage, 80.0f);
        TestTrue(TEXT("Fulgrix shell explodes on impact (unlike Solux)"), Proj->bExplodeOnImpact);
        TestEqual(TEXT("ExplosionRadius = FulgrixExplosionRadius"), Proj->ExplosionRadius, 300.0f);
        TestEqual(TEXT("Category = Gunner"), (int32)Proj->OwnerCategory, (int32)ETriggerCategory::Gunner);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFulgrixInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Fulgrix.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFulgrixInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_FulgrixNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 20.0f;
    DataAsset->FulgrixParams.FulgrixProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRFulgrixTrigger* Fulgrix = NewObject<UWTBRFulgrixTrigger>(Owner);
    Fulgrix->InitializeTrigger(Owner, DataAsset);

    TestFalse(TEXT("Fire rejected when Vael insufficient"), Fulgrix->Activate(FInputActionValue(), false));
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxFireSpawnsProjectileTest,
    "WTBR.Trigger.Venyx.FireSpawnsProjectileConsumesVaelNoTargetStraight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxFireSpawnsProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VenyxFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 6.0f;
    DataAsset->VenyxParams.VenyxDamage = 25.0f;
    DataAsset->VenyxParams.VenyxSpeed = 3500.0f;
    DataAsset->VenyxParams.VenyxFireCooldown = 0.6f;
    DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
    Venyx->InitializeTrigger(Owner, DataAsset);

    // No physics scene → sphere overlap finds no target → the bolt still fires
    // straight (homing acquisition is PIE-only, see section header).
    const bool bActivated = Venyx->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Venyx fires on press"), bActivated);
    TestEqual(TEXT("Vael consumed"), Owner->VaelComponent->GetCurrentVael(), 94.0f);
    TestTrue(TEXT("On cooldown after firing"), Venyx->IsOnCooldown());

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one bolt spawned even with no homing target"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = VenyxDamage"), Proj->BaseDamage, 25.0f);
        TestEqual(TEXT("Category = Gunner"), (int32)Proj->OwnerCategory, (int32)ETriggerCategory::Gunner);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Venyx.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VenyxNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 20.0f;
    DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
    Venyx->InitializeTrigger(Owner, DataAsset);

    TestFalse(TEXT("Fire rejected when Vael insufficient"), Venyx->Activate(FInputActionValue(), false));
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRAcervynBurstFirstShotFiresTest,
    "WTBR.Trigger.Acervyn.BurstFirstShotFiresImmediatelyConsumesVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRAcervynBurstFirstShotFiresTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_AcervynBurst"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 14.0f;
    DataAsset->AcervynParams.AcervynDamage = 20.0f;
    DataAsset->AcervynParams.AcervynSpeed = 4000.0f;
    DataAsset->AcervynParams.AcervynBurstCount = 5;
    DataAsset->AcervynParams.AcervynBurstInterval = 0.1f;
    DataAsset->AcervynParams.AcervynFireCooldown = 1.5f;
    DataAsset->AcervynParams.AcervynProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRAcervynTrigger* Acervyn = NewObject<UWTBRAcervynTrigger>(Owner);
    Acervyn->InitializeTrigger(Owner, DataAsset);

    // Burst fires the first shot immediately; the rest are on a timer that a
    // headless fixture won't advance, so we assert exactly the immediate shot.
    const bool bActivated = Acervyn->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Acervyn burst activates"), bActivated);
    TestEqual(TEXT("Vael consumed once for the whole burst"), Owner->VaelComponent->GetCurrentVael(), 86.0f);
    TestTrue(TEXT("On cooldown after activating"), Acervyn->IsOnCooldown());

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("First burst shot spawns immediately (exactly one so far)"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = AcervynDamage"), Proj->BaseDamage, 20.0f);
        TestEqual(TEXT("Category = Gunner"), (int32)Proj->OwnerCategory, (int32)ETriggerCategory::Gunner);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRAcervynInsufficientVaelBlocksBurstTest,
    "WTBR.Trigger.Acervyn.InsufficientVaelBlocksBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRAcervynInsufficientVaelBlocksBurstTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_AcervynNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 20.0f;
    DataAsset->AcervynParams.AcervynProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRAcervynTrigger* Acervyn = NewObject<UWTBRAcervynTrigger>(Owner);
    Acervyn->InitializeTrigger(Owner, DataAsset);

    TestFalse(TEXT("Burst rejected when Vael insufficient"), Acervyn->Activate(FInputActionValue(), false));
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Telorn — Egret, straight-line non-penetrating sniper
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTelornFireConfiguresProjectileTest,
    "WTBR.Trigger.Telorn.FireConfiguresProjectileFromDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTelornFireConfiguresProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_TelornFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->TelornParams.TelornDamage = 60.0f;
    DataAsset->TelornParams.TelornSpeed = 6000.0f;
    DataAsset->TelornParams.TelornRange = 10000.0f;
    DataAsset->TelornParams.TelornProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->TelornParams.TelornFireCooldown = 1.5f;
    DataAsset->TelornParams.TelornCubeSplitCount = 1;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRTelornTrigger* Telorn = NewObject<UWTBRTelornTrigger>(Owner);
    Telorn->InitializeTrigger(Owner, DataAsset);

    TestEqual(TEXT("GetCooldownDuration reads TelornFireCooldown from DataAsset"),
        Telorn->GetCooldownDuration(), 1.5f);

    // Aim-then-fire: press only starts aiming, no Vael/projectile yet.
    const bool bActivated = Telorn->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Activation (aim start) succeeds"), bActivated);
    TestEqual(TEXT("Vael not yet consumed while aiming"), Owner->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("No projectile spawned while still aiming"), TriggerRegressionTest_CountProjectiles(World), 0);

    // Release actually fires.
    Telorn->OnReleased(FInputActionValue(), false);

    TestEqual(TEXT("Vael consumed on release"), Owner->VaelComponent->GetCurrentVael(), 90.0f);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one bolt spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = TelornDamage"), Proj->BaseDamage, 60.0f);
        TestEqual(TEXT("MaxRange = TelornRange"), Proj->MaxRange, 10000.0f);
        TestEqual(TEXT("CubeSplitCount = TelornCubeSplitCount"), Proj->CubeSplitCount, 1);
        TestFalse(TEXT("Telorn does NOT penetrate (unlike Piercex)"), Proj->bCanPenetrate);
        TestTrue(TEXT("Flagged as a sniper projectile"), Proj->bIsSniper);
        TestEqual(TEXT("Category = SniperBullet"), (int32)Proj->OwnerCategory, (int32)ETriggerCategory::SniperBullet);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTelornInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Telorn.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTelornInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_TelornNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->TelornParams.TelornProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRTelornTrigger* Telorn = NewObject<UWTBRTelornTrigger>(Owner);
    Telorn->InitializeTrigger(Owner, DataAsset);

    // Aim-then-fire: the Vael check now happens on release (ExecuteFire), not
    // on press — Activate only gates on cooldown, so it succeeds here.
    const bool bActivated = Telorn->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Activation (aim start) succeeds even with insufficient Vael"), bActivated);

    Telorn->OnReleased(FInputActionValue(), false);

    TestEqual(TEXT("Vael unchanged — release fire rejected (fail-closed)"), Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestFalse(TEXT("Not left on cooldown"), Telorn->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTelornMissingProjectileClassTest,
    "WTBR.Trigger.Telorn.MissingProjectileClassStillConsumesVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTelornMissingProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    // Locks existing behavior: Activate_Implementation consumes Vael and calls
    // StartCooldown() unconditionally, never checking FireSniper's return value
    // — so a misconfigured DataAsset (no TelornProjectileClass) still charges
    // the player and starts cooldown for a shot that never spawned. Flagged as
    // a real gap, not fixed here (out of scope for a regression-coverage pass).
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_TelornNoProjClass"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    // TelornProjectileClass deliberately left unset.

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRTelornTrigger* Telorn = NewObject<UWTBRTelornTrigger>(Owner);
    Telorn->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Telorn->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Activate (aim start) succeeds"), bActivated);

    Telorn->OnReleased(FInputActionValue(), false);

    TestEqual(TEXT("Vael still consumed despite no projectile spawning"),
        Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("No projectile actually spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestTrue(TEXT("Cooldown still started despite the failed spawn"), Telorn->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRTelornCooldownBlocksImmediateReactivationTest,
    "WTBR.Trigger.Telorn.CooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRTelornCooldownBlocksImmediateReactivationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_TelornCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->TelornParams.TelornProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->TelornParams.TelornFireCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRTelornTrigger* Telorn = NewObject<UWTBRTelornTrigger>(Owner);
    Telorn->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First activation (aim start) succeeds"), Telorn->Activate(FInputActionValue(), false));
    Telorn->OnReleased(FInputActionValue(), false); // fires the shot, starts the (999s) cooldown

    const bool bSecondActivate = Telorn->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Second immediate activation rejected by cooldown"), bSecondActivate);
    TestEqual(TEXT("Vael not consumed twice"), Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Still only one bolt from the first shot"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Piercex — Ibis, penetrating sniper (highest damage among snipers)
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPiercexFireConfiguresProjectileTest,
    "WTBR.Trigger.Piercex.FireConfiguresProjectileFromDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPiercexFireConfiguresProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_PiercexFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->PiercexParams.PiercexDamage = 100.0f;
    DataAsset->PiercexParams.PiercexSpeed = 5000.0f;
    DataAsset->PiercexParams.PiercexRange = 12000.0f;
    DataAsset->PiercexParams.PiercexProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->PiercexParams.PiercexFireCooldown = 2.0f;
    DataAsset->PiercexParams.PiercexCubeSplitCount = 1;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRPiercexTrigger* Piercex = NewObject<UWTBRPiercexTrigger>(Owner);
    Piercex->InitializeTrigger(Owner, DataAsset);

    TestEqual(TEXT("GetCooldownDuration reads PiercexFireCooldown from DataAsset"),
        Piercex->GetCooldownDuration(), 2.0f);

    // Aim-then-fire: press only starts aiming, no Vael/projectile yet.
    const bool bActivated = Piercex->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Activation (aim start) succeeds"), bActivated);
    TestEqual(TEXT("Vael not yet consumed while aiming"), Owner->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("No projectile spawned while still aiming"), TriggerRegressionTest_CountProjectiles(World), 0);

    // Release actually fires.
    Piercex->OnReleased(FInputActionValue(), false);

    TestEqual(TEXT("Vael consumed on release"), Owner->VaelComponent->GetCurrentVael(), 90.0f);

    AWTBRProjectileBase* Proj = TriggerRegressionTest_FindSoleProjectile(World);
    TestNotNull(TEXT("Exactly one bolt spawned"), Proj);
    if (Proj)
    {
        TestEqual(TEXT("Damage = PiercexDamage (highest among snipers)"), Proj->BaseDamage, 100.0f);
        TestEqual(TEXT("MaxRange = PiercexRange"), Proj->MaxRange, 12000.0f);
        TestEqual(TEXT("CubeSplitCount = PiercexCubeSplitCount"), Proj->CubeSplitCount, 1);
        TestTrue(TEXT("Piercex DOES penetrate — the headline differentiator from Telorn"), Proj->bCanPenetrate);
        TestTrue(TEXT("Flagged as a sniper projectile"), Proj->bIsSniper);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPiercexInsufficientVaelBlocksFireTest,
    "WTBR.Trigger.Piercex.InsufficientVaelBlocksFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPiercexInsufficientVaelBlocksFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_PiercexNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->PiercexParams.PiercexProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f);

    UWTBRPiercexTrigger* Piercex = NewObject<UWTBRPiercexTrigger>(Owner);
    Piercex->InitializeTrigger(Owner, DataAsset);

    // Aim-then-fire: the Vael check now happens on release (ExecuteFire), not
    // on press — Activate only gates on cooldown, so it succeeds here.
    const bool bActivated = Piercex->Activate(FInputActionValue(), false);
    TestTrue(TEXT("Activation (aim start) succeeds even with insufficient Vael"), bActivated);

    Piercex->OnReleased(FInputActionValue(), false);

    TestEqual(TEXT("Vael unchanged — release fire rejected (fail-closed)"), Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("No projectile spawned"), TriggerRegressionTest_CountProjectiles(World), 0);
    TestFalse(TEXT("Not left on cooldown"), Piercex->IsOnCooldown());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPiercexCooldownBlocksImmediateReactivationTest,
    "WTBR.Trigger.Piercex.CooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPiercexCooldownBlocksImmediateReactivationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_PiercexCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f;
    DataAsset->PiercexParams.PiercexProjectileClass = AWTBRProjectileBase::StaticClass();
    DataAsset->PiercexParams.PiercexFireCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRPiercexTrigger* Piercex = NewObject<UWTBRPiercexTrigger>(Owner);
    Piercex->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First activation (aim start) succeeds"), Piercex->Activate(FInputActionValue(), false));
    Piercex->OnReleased(FInputActionValue(), false); // fires the shot, starts the (999s) cooldown

    const bool bSecondActivate = Piercex->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Second immediate activation rejected by cooldown"), bSecondActivate);
    TestEqual(TEXT("Vael not consumed twice"), Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Still only one bolt from the first shot"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Vexorn — Signal Block (passive Sub-Trigger)
//
// Vexorn has NO button action: it pulses a sphere overlap every 0.5s while
// equipped and registers a radar signal-block (via UWTBRActionPingSubsystem)
// for every enemy inside VexornSuppressionRadius, unregistering those who leave.
// The overlap itself needs a physics scene headless fixtures lack (finds zero
// overlaps), so the pulse->suppress->unsuppress loop is a PIE-gate item. What IS
// headless-testable and locked here: (1) the ActionPingSubsystem signal-block
// registry contract Vexorn drives, and (2) that Vexorn's button press is a true
// no-op — a passive Sub-Trigger must never consume Vael or fire on LMB/RMB.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRActionPingSignalBlockRegistryTest,
    "WTBR.Subsystem.ActionPing.SignalBlockRegisterUnregisterQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRActionPingSignalBlockRegistryTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Subsystem_SignalBlock"));
    UWorld* World = Fixture.GetWorld();

    UWTBRActionPingSubsystem* PingSys = World ? World->GetSubsystem<UWTBRActionPingSubsystem>() : nullptr;
    TestNotNull(TEXT("ActionPing world subsystem exists in the fixture"), PingSys);
    if (!PingSys) return false;

    AWTBRCharacter* A = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* B = TriggerRegressionTest_SpawnCharacter(World, FVector(200.0f, 0.0f, 0.0f));
    if (!A || !B) return false;

    TestFalse(TEXT("Nobody blocked initially (A)"), PingSys->IsSignalBlocked(A));
    TestFalse(TEXT("Nobody blocked initially (B)"), PingSys->IsSignalBlocked(B));

    PingSys->RegisterSignalBlock(A);
    PingSys->RegisterSignalBlock(B);
    TestTrue(TEXT("A blocked after register"), PingSys->IsSignalBlocked(A));
    TestTrue(TEXT("B blocked after register"), PingSys->IsSignalBlocked(B));

    // Unregistering one must not affect the other (per-actor registry).
    PingSys->UnregisterSignalBlock(A);
    TestFalse(TEXT("A unblocked after unregister"), PingSys->IsSignalBlocked(A));
    TestTrue(TEXT("B still blocked (independent)"), PingSys->IsSignalBlocked(B));

    // Idempotent: unregistering A again is harmless.
    PingSys->UnregisterSignalBlock(A);
    TestFalse(TEXT("A still unblocked after redundant unregister"), PingSys->IsSignalBlocked(A));

    PingSys->UnregisterSignalBlock(B);
    TestFalse(TEXT("B unblocked after unregister"), PingSys->IsSignalBlocked(B));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVexornPassiveActivateIsNoOpTest,
    "WTBR.Trigger.Vexorn.PassiveActivateConsumesNoVaelAndFiresNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVexornPassiveActivateIsNoOpTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VexornPassive"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VexornParams.VexornVaelDrainPerSecond = 10.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRVexornTrigger* Vexorn = NewObject<UWTBRVexornTrigger>(Owner);
    Vexorn->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Vexorn->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Passive Activate returns false (no button action)"), bActivated);
    TestEqual(TEXT("Vael unchanged — passive press must never drain Vael"),
        Owner->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("No projectile spawned by a passive trigger"),
        TriggerRegressionTest_CountProjectiles(World), 0);

    // Release is likewise a no-op and must not crash or drain.
    Vexorn->OnReleased(FInputActionValue(), false);
    TestEqual(TEXT("Vael still unchanged after release"),
        Owner->VaelComponent->GetCurrentVael(), 100.0f);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Nexil — Wire tripwire (place-to-deploy)
//
// Tap places one wire actor at the player's aim yaw (360°). NexilVaelCost is
// charged per wire (added 2026-07-15 — base Activate consumes nothing; canon
// GDD §5.2 = 15/wire, DA-tunable). Max wires FIFO (9th removes 1st), auto-
// despawn after WireDuration, cleared on Deactivate. The wire's TRIP detection
// (enemy overlap → stagger + Action-Ping reveal) needs a physics scene → PIE-
// gated; here we lock placement, the Vael economy, the FIFO cap, and cleanup.
// The ghost-preview + hold-to-place confirm flow is a separate BP/UMG phase.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVexornBagwormCloakAndDrainTest,
    "WTBR.Trigger.Vexorn.BagwormCloaksOwnerAndDrainsVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVexornBagwormCloakAndDrainTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VexornBagworm"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VexornParams.VexornVaelDrainPerSecond = 10.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRVexornTrigger* Vexorn = NewObject<UWTBRVexornTrigger>(Owner);
    Vexorn->InitializeTrigger(Owner, DataAsset);
    Vexorn->OnEquipped();

    TestTrue(TEXT("Equipping Vexorn cloaks its owner from radar"), Owner->IsRadarCloaked());
    TestEqual(TEXT("First half-second upkeep tick consumes Vael"),
        Owner->VaelComponent->GetCurrentVael(), 95.0f);

    Vexorn->OnUnequipped();
    TestFalse(TEXT("Unequipping Vexorn reveals its owner to radar"), Owner->IsRadarCloaked());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilPlaceConsumesVaelAndSpawnsWireTest,
    "WTBR.Trigger.Nexil.PlaceConsumesVaelAndSpawnsOneWire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilPlaceConsumesVaelAndSpawnsWireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_NexilPlace"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->NexilParams.NexilVaelCost = 15.0f;
    DataAsset->NexilParams.MaxWires = 8;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRNexilTrigger* Nexil = NewObject<UWTBRNexilTrigger>(Owner);
    Nexil->InitializeTrigger(Owner, DataAsset);
    Nexil->SetWireActorClassForTest(AWTBRNexilWireActor::StaticClass());

    const bool bPlaced = Nexil->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Nexil places on press"), bPlaced);
    TestEqual(TEXT("Vael consumed by NexilVaelCost"), Owner->VaelComponent->GetCurrentVael(), 85.0f);
    TestEqual(TEXT("Exactly one wire active"), Nexil->GetActiveWireCount(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilInsufficientVaelBlocksPlaceTest,
    "WTBR.Trigger.Nexil.InsufficientVaelBlocksPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilInsufficientVaelBlocksPlaceTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_NexilNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->NexilParams.NexilVaelCost = 15.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(10.0f); // < 15

    UWTBRNexilTrigger* Nexil = NewObject<UWTBRNexilTrigger>(Owner);
    Nexil->InitializeTrigger(Owner, DataAsset);
    Nexil->SetWireActorClassForTest(AWTBRNexilWireActor::StaticClass());

    const bool bPlaced = Nexil->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Placement rejected when Vael insufficient"), bPlaced);
    TestEqual(TEXT("Vael unchanged (fail-closed)"), Owner->VaelComponent->GetCurrentVael(), 10.0f);
    TestEqual(TEXT("No wire spawned"), Nexil->GetActiveWireCount(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilMaxWiresFifoTest,
    "WTBR.Trigger.Nexil.MaxWiresFifoRemovesOldest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilMaxWiresFifoTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_NexilFifo"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->NexilParams.NexilVaelCost = 0.0f; // isolate FIFO from Vael economy
    DataAsset->NexilParams.MaxWires = 8;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRNexilTrigger* Nexil = NewObject<UWTBRNexilTrigger>(Owner);
    Nexil->InitializeTrigger(Owner, DataAsset);
    Nexil->SetWireActorClassForTest(AWTBRNexilWireActor::StaticClass());

    // Place the max (8).
    for (int32 i = 0; i < 8; ++i) Nexil->Activate(FInputActionValue(), false);
    TestEqual(TEXT("Eight wires active at cap"), Nexil->GetActiveWireCount(), 8);

    // The 9th must remove the oldest, keeping the count at the cap.
    Nexil->Activate(FInputActionValue(), false);
    TestEqual(TEXT("9th placement keeps count at MaxWires (oldest removed)"),
        Nexil->GetActiveWireCount(), 8);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRNexilDeactivateClearsAllWiresTest,
    "WTBR.Trigger.Nexil.DeactivateDestroysAllWires",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRNexilDeactivateClearsAllWiresTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_NexilDeactivate"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->NexilParams.NexilVaelCost = 0.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRNexilTrigger* Nexil = NewObject<UWTBRNexilTrigger>(Owner);
    Nexil->InitializeTrigger(Owner, DataAsset);
    Nexil->SetWireActorClassForTest(AWTBRNexilWireActor::StaticClass());

    for (int32 i = 0; i < 3; ++i) Nexil->Activate(FInputActionValue(), false);
    TestEqual(TEXT("Three wires placed"), Nexil->GetActiveWireCount(), 3);

    Nexil->Deactivate();
    TestEqual(TEXT("Deactivate destroys every wire"), Nexil->GetActiveWireCount(), 0);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Voltis — air-launch/dash movement Trigger
//
// Activate = LaunchCharacter (works headlessly, sets Velocity directly, no
// physics scene needed). Zero-Vael by design ("Voltis 0-cost temporary" — an
// existing, intentional Vael-cost-lock entry, not a gap) — the test below
// documents that current contract rather than assuming it's a bug. The 2026-07-
// 14 Codex fix removed the only call site of StartStagger()/OnStaggerExpired()
// (it lived inside the deleted ceiling-obstruction check) — bIsStaggered is now
// never set true through normal gameplay, so that path is effectively dead code
// today. It's still public + still gates Activate, so the guard test below locks
// the mechanism (useful if a future feature re-wires something into it) without
// claiming it's currently reachable.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVoltisInitializeSetsMaxLaunchesTest,
    "WTBR.Trigger.Voltis.InitializeSetsRemainingLaunchesToMax",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVoltisInitializeSetsMaxLaunchesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VoltisInit"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VoltisParams.MaxAirLaunches = 3;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRVoltisLaunchTrigger* Voltis = NewObject<UWTBRVoltisLaunchTrigger>(Owner);
    Voltis->InitializeTrigger(Owner, DataAsset);

    TestEqual(TEXT("RemainingLaunches seeded from MaxAirLaunches"), Voltis->GetRemainingLaunches(), 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVoltisActivateConsumesLaunchNoVaelTest,
    "WTBR.Trigger.Voltis.ActivateConsumesOneLaunchAndNoVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVoltisActivateConsumesLaunchNoVaelTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VoltisActivate"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VaelCostPerUse = 10.0f; // deliberately non-zero, to prove Voltis ignores it
    DataAsset->VoltisParams.MaxAirLaunches = 3;
    DataAsset->VoltisParams.VerticalLaunchForce = 1200.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(50.0f);

    UWTBRVoltisLaunchTrigger* Voltis = NewObject<UWTBRVoltisLaunchTrigger>(Owner);
    Voltis->InitializeTrigger(Owner, DataAsset);

    const bool bActivated = Voltis->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Voltis launches on press"), bActivated);
    TestEqual(TEXT("RemainingLaunches decremented by one"), Voltis->GetRemainingLaunches(), 2);
    TestEqual(TEXT("Vael untouched — Voltis is 0-cost by current design lock"),
        Owner->VaelComponent->GetCurrentVael(), 50.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVoltisBlocksWhenLaunchesExhaustedTest,
    "WTBR.Trigger.Voltis.BlocksActivateWhenNoRemainingLaunches",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVoltisBlocksWhenLaunchesExhaustedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VoltisExhausted"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VoltisParams.MaxAirLaunches = 2;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRVoltisLaunchTrigger* Voltis = NewObject<UWTBRVoltisLaunchTrigger>(Owner);
    Voltis->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("1st launch succeeds"), Voltis->Activate(FInputActionValue(), false));
    TestTrue(TEXT("2nd launch succeeds"), Voltis->Activate(FInputActionValue(), false));
    TestEqual(TEXT("Both launches consumed"), Voltis->GetRemainingLaunches(), 0);

    const bool bThirdActivate = Voltis->Activate(FInputActionValue(), false);
    TestFalse(TEXT("3rd launch rejected — no launches remaining"), bThirdActivate);
    TestEqual(TEXT("RemainingLaunches stays at 0, not negative"), Voltis->GetRemainingLaunches(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVoltisLandingResetsLaunchesTest,
    "WTBR.Trigger.Voltis.LandingResetsRemainingLaunchesToMax",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVoltisLandingResetsLaunchesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VoltisLanding"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VoltisParams.MaxAirLaunches = 3;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRVoltisLaunchTrigger* Voltis = NewObject<UWTBRVoltisLaunchTrigger>(Owner);
    Voltis->InitializeTrigger(Owner, DataAsset);

    Voltis->Activate(FInputActionValue(), false);
    Voltis->Activate(FInputActionValue(), false);
    TestEqual(TEXT("Two launches spent"), Voltis->GetRemainingLaunches(), 1);

    Voltis->OnCharacterLanded(FHitResult());
    TestEqual(TEXT("Landing refills RemainingLaunches to MaxAirLaunches"),
        Voltis->GetRemainingLaunches(), 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVoltisStaggeredGuardBlocksActivateTest,
    "WTBR.Trigger.Voltis.StaggeredGuardBlocksActivate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVoltisStaggeredGuardBlocksActivateTest::RunTest(const FString& /*Parameters*/)
{
    // Locks the bIsStaggered guard itself — nothing in current gameplay code
    // sets this true (StartStagger()'s only call site was removed with the
    // ceiling-obstruction check), but the flag is still public/replicated and
    // Activate still checks it, so this proves the guard still works correctly
    // if something sets it in the future.
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_VoltisStagger"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->VoltisParams.MaxAirLaunches = 3;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRVoltisLaunchTrigger* Voltis = NewObject<UWTBRVoltisLaunchTrigger>(Owner);
    Voltis->InitializeTrigger(Owner, DataAsset);
    Voltis->bIsStaggered = true;

    const bool bActivated = Voltis->Activate(FInputActionValue(), false);
    TestFalse(TEXT("Activate rejected while staggered"), bActivated);
    TestEqual(TEXT("No launch consumed while staggered"), Voltis->GetRemainingLaunches(), 3);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// AWTBRBasicBotController — server-only combat bot (WTBR.AI.BasicBot.*)
//
// First automation coverage for the basic combat bot (added 2026-07-15). Two
// things are locked headlessly:
// (1) AcquireClosestRadarContact — pure distance/team/cloak/alive filtering,
//     no physics involved, so it's fully testable here.
// (2) AWTBRCharacter::ExecuteBotTriggerInput — the server-only entry point
//     UpdateCombat calls to fire, proven to be the SAME dispatch path a
//     player's LMB tap uses (Vael cost, cooldown) by driving it directly and
//     asserting the same results as FWTBRSoluxFireConfiguresProjectileTest's
//     direct-tap expectations.
// UpdateCombat's own movement + LineOfSightTo gate needs a physics scene these
// headless fixtures don't run (same reason Nexil/Spider's overlap detection is
// PIE-gated elsewhere in this file) — that full move/aim/fire orchestration is
// a PIE Human Test Gate item, not covered here.
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRBasicBotAcquireClosestRadarContactTest,
    "WTBR.AI.BasicBot.AcquiresClosestAliveEnemyIgnoringTeamCloakAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRBasicBotAcquireClosestRadarContactTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_AI_BasicBotTargeting"));
    UWorld* World = Fixture.GetWorld();

    AWTBRCharacter* Bot = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Bot) return false;
    Bot->SetTeamId(0);

    // Nearer-than-NearEnemy candidates that must each be skipped for a different
    // reason, plus a valid-but-farther enemy and an out-of-radar-range enemy, so
    // one test locks every filter AND the "closest wins" tiebreak in one pass.
    AWTBRCharacter* Teammate       = TriggerRegressionTest_SpawnCharacter(World, FVector(100.0f, 0.0f, 0.0f));
    AWTBRCharacter* CloakedEnemy   = TriggerRegressionTest_SpawnCharacter(World, FVector(120.0f, 0.0f, 0.0f));
    AWTBRCharacter* DeadEnemy      = TriggerRegressionTest_SpawnCharacter(World, FVector(140.0f, 0.0f, 0.0f));
    AWTBRCharacter* NearEnemy      = TriggerRegressionTest_SpawnCharacter(World, FVector(300.0f, 0.0f, 0.0f));
    AWTBRCharacter* FartherEnemy   = TriggerRegressionTest_SpawnCharacter(World, FVector(500.0f, 0.0f, 0.0f));
    AWTBRCharacter* OutOfRangeEnemy = TriggerRegressionTest_SpawnCharacter(World, FVector(6500.0f, 0.0f, 0.0f));
    if (!Teammate || !CloakedEnemy || !DeadEnemy || !NearEnemy || !FartherEnemy || !OutOfRangeEnemy) return false;

    Teammate->SetTeamId(0);
    CloakedEnemy->SetTeamId(1);
    DeadEnemy->SetTeamId(1);
    NearEnemy->SetTeamId(1);
    FartherEnemy->SetTeamId(1);
    OutOfRangeEnemy->SetTeamId(1);

    CloakedEnemy->SetRadarCloaked(true);
    TestTrue(TEXT("Sanity: CloakedEnemy is actually cloaked"), CloakedEnemy->IsRadarCloaked());

    if (!DeadEnemy->HealthComponent) return false;
    DeadEnemy->HealthComponent->ApplyDamage(100000.0f);
    TestFalse(TEXT("Sanity: DeadEnemy is no longer Alive after lethal damage"), DeadEnemy->HealthComponent->IsAlive());

    AWTBRBasicBotController* Controller = World->SpawnActor<AWTBRBasicBotController>();
    if (!Controller) return false;

    Controller->AcquireClosestRadarContactForTest(Bot);

    TestTrue(TEXT("Picks the closest alive, non-cloaked, enemy-team, in-range character (skips teammate/cloaked/dead/farther/out-of-range)"),
        Controller->GetCombatTarget() == NearEnemy);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRBasicBotNoValidTargetClearsCombatTargetTest,
    "WTBR.AI.BasicBot.NoValidCandidateLeavesCombatTargetNull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRBasicBotNoValidTargetClearsCombatTargetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_AI_BasicBotNoTarget"));
    UWorld* World = Fixture.GetWorld();

    AWTBRCharacter* Bot = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Bot) return false;
    Bot->SetTeamId(0);

    // Only a teammate in range — nothing valid to acquire.
    AWTBRCharacter* Teammate = TriggerRegressionTest_SpawnCharacter(World, FVector(100.0f, 0.0f, 0.0f));
    if (!Teammate) return false;
    Teammate->SetTeamId(0);

    AWTBRBasicBotController* Controller = World->SpawnActor<AWTBRBasicBotController>();
    if (!Controller) return false;

    Controller->AcquireClosestRadarContactForTest(Bot);
    TestNull(TEXT("No enemy in range — CombatTarget stays null"), Controller->GetCombatTarget());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRBasicBotExecuteBotTriggerInputFiresLikePlayerTapTest,
    "WTBR.AI.BasicBot.ExecuteBotTriggerInputFiresLikePlayerTap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRBasicBotExecuteBotTriggerInputFiresLikePlayerTapTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_AI_BasicBotFire"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->Category = ETriggerCategory::Gunner;
    DataAsset->VaelCostPerUse = 8.0f;
    DataAsset->SoluxParams.SoluxFireCooldown = 5.0f;
    DataAsset->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();

    AWTBRCharacter* Bot = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Bot || !Bot->VaelComponent || !Bot->TriggerSetComponent) return false;
    Bot->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Bot);
    Solux->InitializeTrigger(Bot, DataAsset);
    Bot->TriggerSetComponent->InstallTriggerForTest(ETriggerSlot::Main1, Solux);

    // Mirrors AWTBRBasicBotController::UpdateCombat exactly: press then release
    // with no analog movement input — a tap through the bot's own entry point,
    // not a direct call into the Trigger class.
    Bot->ExecuteBotTriggerInput(true, true);
    Bot->ExecuteBotTriggerInput(true, false);

    TestEqual(TEXT("Vael consumed by the bot's tap, same as a player tap"),
        Bot->VaelComponent->GetCurrentVael(), 92.0f);
    TestEqual(TEXT("Exactly one bullet spawned"), TriggerRegressionTest_CountProjectiles(World), 1);
    TestTrue(TEXT("Solux is on cooldown after the bot's tap"), Solux->IsOnCooldown());

    // Second tap while on cooldown must be rejected, just like for a player.
    Bot->ExecuteBotTriggerInput(true, true);
    Bot->ExecuteBotTriggerInput(true, false);
    TestEqual(TEXT("Cooldown blocks the second tap — no extra Vael spent"),
        Bot->VaelComponent->GetCurrentVael(), 92.0f);
    TestEqual(TEXT("Cooldown blocks the second tap — no extra bullet"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Feryx.ThrowBladeStars — LMB/RMB hold: one large blade-star, not a fan spread
// (canon: Team Yuma vs Team Ninomiya). Regression lock added 2026-07-15 after
// review found the hold was spawning projectiles for free (no Vael consumed).
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFeryxThrowBladeStarsConsumesVaelAndSpawnsOneTest,
    "WTBR.Trigger.Feryx.ThrowBladeStars.ConsumesVaelAndSpawnsOneStar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFeryxThrowBladeStarsConsumesVaelAndSpawnsOneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_FeryxHold"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->FeryxParams.StarThrowVaelCost = 10.0f;
    DataAsset->FeryxParams.StarDamage = 30.0f;
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f; // Keep cooldown from interfering here.

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);

    UWTBRFeryxTrigger* Feryx = NewObject<UWTBRFeryxTrigger>(Owner);
    Feryx->InitializeTrigger(Owner, DataAsset);

    Feryx->ThrowBladeStars();

    TestEqual(TEXT("Vael consumed by StarThrowVaelCost"),
        Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Exactly one blade-star spawned — not a multi-projectile fan"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFeryxThrowBladeStarsRejectsWithoutVaelTest,
    "WTBR.Trigger.Feryx.ThrowBladeStars.InsufficientVaelBlocksThrow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFeryxThrowBladeStarsRejectsWithoutVaelTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_FeryxHoldNoVael"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->FeryxParams.StarThrowVaelCost = 10.0f;
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner || !Owner->VaelComponent) return false;
    Owner->VaelComponent->DebugSetCurrentVaelDirect(5.0f); // Below StarThrowVaelCost=10.

    UWTBRFeryxTrigger* Feryx = NewObject<UWTBRFeryxTrigger>(Owner);
    Feryx->InitializeTrigger(Owner, DataAsset);

    Feryx->ThrowBladeStars();

    TestEqual(TEXT("Vael unchanged — insufficient Vael blocks the throw"),
        Owner->VaelComponent->GetCurrentVael(), 5.0f);
    TestEqual(TEXT("No blade-star spawned"), TriggerRegressionTest_CountProjectiles(World), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
