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
// Mantorn — 360° spin melee (ignores dual-wield, hits everyone in radius)
// ═════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornDamagesTargetInRadiusTest,
    "WTBR.Trigger.Mantorn.DamagesTargetInRadiusNotOutOfRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornDamagesTargetInRadiusTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornRadius"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f; // Keep cooldown from interfering here.

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* InRange = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    AWTBRCharacter* OutOfRange = TriggerRegressionTest_SpawnCharacter(World, FVector(1000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Owner spawns"), Owner);
    TestNotNull(TEXT("In-range target spawns"), InRange);
    TestNotNull(TEXT("Out-of-range target spawns"), OutOfRange);
    if (!Owner || !InRange || !OutOfRange) return false;

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float OwnerHPBefore = Owner->HealthComponent->GetCurrentHP();
    const float InRangeHPBefore = InRange->HealthComponent->GetCurrentHP();
    const float OutOfRangeHPBefore = OutOfRange->HealthComponent->GetCurrentHP();

    const bool bActivated = Mantorn->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Activate succeeds"), bActivated);
    TestEqual(TEXT("In-range target takes SpinDamage"),
        InRange->HealthComponent->GetCurrentHP(), InRangeHPBefore - 80.0f);
    TestEqual(TEXT("Out-of-range target untouched"),
        OutOfRange->HealthComponent->GetCurrentHP(), OutOfRangeHPBefore);
    TestEqual(TEXT("Owner does not damage self (FilterHits excludes instigator)"),
        Owner->HealthComponent->GetCurrentHP(), OwnerHPBefore);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornIgnoresDualWieldTest,
    "WTBR.Trigger.Mantorn.IgnoresDualWieldFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornIgnoresDualWieldTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornDual"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MeleeHitbox.SwingCooldown = 0.01f; // Fast enough to fire twice in-test.

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Target = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    if (!Owner || !Target) return false;

    // GDD §3.4 Lock (see WTBRMantornTrigger.cpp comment): "Mantorn ignores
    // bIsDualWield — always spins." Verify dual-wield deals the SAME single-spin
    // damage, not double.
    UWTBRMantornTrigger* MantornDual = NewObject<UWTBRMantornTrigger>(Owner);
    MantornDual->InitializeTrigger(Owner, DataAsset);
    const float HPBefore = Target->HealthComponent->GetCurrentHP();
    MantornDual->Activate(FInputActionValue(), /*bIsDualWield=*/true);

    TestEqual(TEXT("Dual-wield deals exactly one spin's damage, not double"),
        Target->HealthComponent->GetCurrentHP(), HPBefore - 80.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornCooldownBlocksImmediateReactivationTest,
    "WTBR.Trigger.Mantorn.CooldownBlocksImmediateReactivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornCooldownBlocksImmediateReactivationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornCooldown"));
    UWorld* World = Fixture.GetWorld();

    UWTBRTriggerDataAsset* DataAsset = TriggerRegressionTest_MakeDataAsset();
    DataAsset->MeleeHitbox.MantornSpinRadius = 200.0f;
    DataAsset->MeleeHitbox.MantornSpinDamage = 80.0f;
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Target = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    if (!Owner || !Target) return false;

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    TestTrue(TEXT("First activation succeeds"), Mantorn->Activate(FInputActionValue(), false));
    TestTrue(TEXT("On cooldown immediately after"), Mantorn->IsOnCooldown());

    const float HPAfterFirstSpin = Target->HealthComponent->GetCurrentHP();
    const bool bSecondActivate = Mantorn->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Second immediate activation rejected by cooldown"), bSecondActivate);
    TestEqual(TEXT("No additional damage from the rejected second spin"),
        Target->HealthComponent->GetCurrentHP(), HPAfterFirstSpin);

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
    DataAsset->MeleeHitbox.SwingCooldown = 999.0f;

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    AWTBRCharacter* Teammate = TriggerRegressionTest_SpawnCharacter(World, FVector(150.0f, 0.0f, 0.0f));
    AWTBRCharacter* Enemy = TriggerRegressionTest_SpawnCharacter(World, FVector(-150.0f, 0.0f, 0.0f));
    if (!Owner || !Teammate || !Enemy) return false;
    Owner->SetTeamId(0);
    Teammate->SetTeamId(0);
    Enemy->SetTeamId(1);

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, DataAsset);

    const float TeammateHPBefore = Teammate->HealthComponent->GetCurrentHP();
    const float EnemyHPBefore = Enemy->HealthComponent->GetCurrentHP();

    Mantorn->Activate(FInputActionValue(), false);

    TestEqual(TEXT("Teammate untouched (friendly fire blocked)"),
        Teammate->HealthComponent->GetCurrentHP(), TeammateHPBefore);
    TestEqual(TEXT("Enemy takes full SpinDamage"),
        Enemy->HealthComponent->GetCurrentHP(), EnemyHPBefore - 80.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMantornRejectsWithoutDataAssetTest,
    "WTBR.Trigger.Mantorn.RejectsActivationWithoutDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMantornRejectsWithoutDataAssetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRTriggerRegressionWorldFixture Fixture(TEXT("WTBR_Trigger_MantornNoData"));
    UWorld* World = Fixture.GetWorld();

    AWTBRCharacter* Owner = TriggerRegressionTest_SpawnCharacter(World, FVector::ZeroVector);
    if (!Owner) return false;

    UWTBRMantornTrigger* Mantorn = NewObject<UWTBRMantornTrigger>(Owner);
    Mantorn->InitializeTrigger(Owner, nullptr);

    TestFalse(TEXT("Activation rejected with no DataAsset"), Mantorn->Activate(FInputActionValue(), false));

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

    const bool bActivated = Telorn->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Activation succeeds"), bActivated);
    TestEqual(TEXT("Vael consumed"), Owner->VaelComponent->GetCurrentVael(), 90.0f);

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

    const bool bActivated = Telorn->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Activation rejected: insufficient Vael (fail-closed)"), bActivated);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);
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

    TestTrue(TEXT("Activate still reports success (existing quirk, not fixed here)"), bActivated);
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

    TestTrue(TEXT("First activation succeeds"), Telorn->Activate(FInputActionValue(), false));
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

    const bool bActivated = Piercex->Activate(FInputActionValue(), false);

    TestTrue(TEXT("Activation succeeds"), bActivated);
    TestEqual(TEXT("Vael consumed"), Owner->VaelComponent->GetCurrentVael(), 90.0f);

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

    const bool bActivated = Piercex->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Activation rejected: insufficient Vael (fail-closed)"), bActivated);
    TestEqual(TEXT("Vael unchanged"), Owner->VaelComponent->GetCurrentVael(), 5.0f);
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

    TestTrue(TEXT("First activation succeeds"), Piercex->Activate(FInputActionValue(), false));
    const bool bSecondActivate = Piercex->Activate(FInputActionValue(), false);

    TestFalse(TEXT("Second immediate activation rejected by cooldown"), bSecondActivate);
    TestEqual(TEXT("Vael not consumed twice"), Owner->VaelComponent->GetCurrentVael(), 90.0f);
    TestEqual(TEXT("Still only one bolt from the first shot"),
        TriggerRegressionTest_CountProjectiles(World), 1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
