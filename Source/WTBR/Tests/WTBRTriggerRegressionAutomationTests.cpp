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
#include "Trigger/WTBRMantornTrigger.h"
#include "Trigger/WTBRPiercexTrigger.h"
#include "Trigger/WTBRTelornTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRCharacter.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
