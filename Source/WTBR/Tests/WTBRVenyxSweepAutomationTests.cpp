// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Components/WTBRVaelComponent.h"
#include "WTBRCharacter.h"

// -----------------------------------------------------------------------------
// Venyx swept-volley tests (WTBR.Venyx.Sweep.*)
//
// The Hound model, revised with the owner 2026-07-21: a lane is a SEARCH SWEEP,
// not an approach to a target the shot already knows about. Nothing is locked at
// fire time — each cube carries a homing radius along its arc and takes the first
// enemy it passes near. Lanes also carry a launch delay so one shot can arrive in
// waves.
//
// What is asserted here is the acquisition rule and the authored data, both of
// which are pure. The flight handoff itself (InterpToMovement -> homing) needs a
// real physics scene and belongs to PIE.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRVenyxSweepWorldFixture
    {
    public:
        explicit FWTBRVenyxSweepWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRVenyxSweepWorldFixture()
        {
            if (World)
            {
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World->DestroyWorld(false);
                World = nullptr;
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    AWTBRCharacter* SpawnVenyxSweepCharacter(
        UWorld* World, const FVector& Location, int32 TeamId)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
        if (!Character) return nullptr;

        Character->SetTeamId(TeamId);
        return Character;
    }

    // No CoreStatsAsset is assigned in this fixture, so a lethal hit goes straight
    // to Eliminated rather than into the Downed/bleed-out path — which is exactly
    // the "not a valid target" state the acquisition rule must skip.
    void KillVenyxSweepCharacter(AWTBRCharacter* Victim, AWTBRCharacter* Attacker)
    {
        if (Victim && Victim->HealthComponent)
        {
            Victim->HealthComponent->ApplyDamage(10000.0f, Attacker);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepTakesNearestHostileTest,
    "WTBR.Venyx.Sweep.AcquiresNearestHostileInsideRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepTakesNearestHostileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxSweepNearest"));
    UWorld* World = Fixture.Get();
    TestNotNull(TEXT("Fixture world exists"), World);
    if (!World) return false;

    AWTBRCharacter* Shooter = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 0);
    AWTBRCharacter* Far = SpawnVenyxSweepCharacter(World, FVector(400.0f, 0.0f, 0.0f), 1);
    AWTBRCharacter* Near = SpawnVenyxSweepCharacter(World, FVector(200.0f, 0.0f, 0.0f), 1);
    if (!Shooter || !Far || !Near) return false;

    // Deliberately spawned far-then-near: nearest must win on distance, not on the
    // order the world happens to hand actors back, or two cubes sweeping the same
    // ground could pick different people.
    const TArray<AWTBRCharacter*> Candidates = {Shooter, Far, Near};
    AWTBRCharacter* Acquired = AWTBRProjectileBase::FindProximityHomingTarget(
        Shooter, FVector::ZeroVector, 1000.0f, Candidates);

    TestEqual(TEXT("Nearest hostile is acquired"), Acquired, Near);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepSkipsInvalidCandidatesTest,
    "WTBR.Venyx.Sweep.SkipsSelfTeammatesDeadAndOutOfRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepSkipsInvalidCandidatesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxSweepSkips"));
    UWorld* World = Fixture.Get();
    TestNotNull(TEXT("Fixture world exists"), World);
    if (!World) return false;

    AWTBRCharacter* Shooter = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 0);
    AWTBRCharacter* Teammate = SpawnVenyxSweepCharacter(World, FVector(100.0f, 0.0f, 0.0f), 0);
    AWTBRCharacter* Corpse = SpawnVenyxSweepCharacter(World, FVector(150.0f, 0.0f, 0.0f), 1);
    AWTBRCharacter* Distant = SpawnVenyxSweepCharacter(World, FVector(5000.0f, 0.0f, 0.0f), 1);
    if (!Shooter || !Teammate || !Corpse || !Distant) return false;
    KillVenyxSweepCharacter(Corpse, Shooter);
    TestFalse(TEXT("The corpse really is dead"), Corpse->HealthComponent->IsAlive());

    const TArray<AWTBRCharacter*> Candidates = {Shooter, Teammate, Corpse, Distant};

    // The shooter itself sits at the query point and every other candidate is
    // disqualified for a different reason, so a rule that dropped any one of the
    // four filters would return non-null here.
    AWTBRCharacter* Acquired = AWTBRProjectileBase::FindProximityHomingTarget(
        Shooter, FVector::ZeroVector, 1000.0f, Candidates);

    TestNull(TEXT("No valid target when every candidate is disqualified"), Acquired);

    // Same set, but now the distant hostile is inside a radius wide enough to reach
    // it — proves the null above came from the filters, not from an inert rule.
    AWTBRCharacter* Reached = AWTBRProjectileBase::FindProximityHomingTarget(
        Shooter, FVector::ZeroVector, 6000.0f, Candidates);
    TestEqual(TEXT("Widening the radius reaches the distant hostile"), Reached, Distant);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepZeroRadiusIsInertTest,
    "WTBR.Venyx.Sweep.ZeroRadiusNeverAcquires",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepZeroRadiusIsInertTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxSweepInert"));
    UWorld* World = Fixture.Get();
    if (!World) return false;

    AWTBRCharacter* Shooter = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 0);
    AWTBRCharacter* Enemy = SpawnVenyxSweepCharacter(World, FVector(10.0f, 0.0f, 0.0f), 1);
    if (!Shooter || !Enemy) return false;

    // Every non-Hound preset authors radius 0, and those lanes must stay plain
    // paths — an enemy sitting right on top of the cube still must not divert it.
    AWTBRCharacter* Acquired = AWTBRProjectileBase::FindProximityHomingTarget(
        Shooter, FVector::ZeroVector, 0.0f, {Shooter, Enemy});

    TestNull(TEXT("A zero-radius lane never acquires"), Acquired);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepResolverCarriesLaunchDataTest,
    "WTBR.Venyx.Sweep.ResolverCarriesDelayAndScaledRadiusPerCube",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepResolverCarriesLaunchDataTest::RunTest(const FString& /*Parameters*/)
{
    // Resolution flattens lanes into a single list of cube paths, which loses the
    // lane a cube came from — but delay and radius are authored PER LANE. This is
    // the seam where that could silently go missing.
    FWTBRPathPreset Preset;

    FWTBRPathLane& Early = Preset.Lanes.AddDefaulted_GetRef();
    Early.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Early.CubeCount = 2;
    Early.LaunchDelay = 0.0f;
    Early.HomingRadius = 0.10f;
    // Floor disabled so this test measures the SCALING alone; the floor has its own
    // test, and leaving it on here would mask a scaling regression behind it.
    Early.HomingRadiusFloorUU = 0.0f;

    FWTBRPathLane& Late = Preset.Lanes.AddDefaulted_GetRef();
    Late.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.3f, 0.0f)};
    Late.CubeCount = 1;
    Late.LaunchDelay = 1.60f;
    Late.HomingRadius = 0.25f;
    Late.HomingRadiusFloorUU = 0.0f;

    TArray<TArray<FVector>> Paths;
    TArray<FWTBRResolvedCubeLaunch> Launches;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths,
        /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true, /*TotalCubeOverride=*/0, &Launches);

    TestEqual(TEXT("Three cubes are emitted"), Paths.Num(), 3);
    TestEqual(TEXT("One launch entry per emitted cube"), Launches.Num(), Paths.Num());
    if (Launches.Num() != 3) return false;

    TestEqual(TEXT("First early cube inherits its lane's delay"), Launches[0].DelaySeconds, 0.0f);
    TestEqual(TEXT("Second early cube inherits its lane's delay"), Launches[1].DelaySeconds, 0.0f);
    TestEqual(TEXT("Late cube inherits its own lane's delay"), Launches[2].DelaySeconds, 1.60f);

    // Radius is authored as a fraction of range for the same reason waypoints are:
    // one preset then reads the same at every charge level.
    TestEqual(TEXT("Early radius is scaled into world units"), Launches[0].HomingRadiusUU, 100.0f);
    TestEqual(TEXT("Late radius is scaled into world units"), Launches[2].HomingRadiusUU, 250.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRExplosionDamagesEachTargetOnceTest,
    "WTBR.Combat.Explosion.RadialDamageHitsEachCharacterOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRExplosionDamagesEachTargetOnceTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRExplosionDedup"));
    UWorld* World = Fixture.Get();
    if (!World) return false;

    AWTBRCharacter* A = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 1);
    AWTBRCharacter* B = SpawnVenyxSweepCharacter(World, FVector(200.0f, 0.0f, 0.0f), 1);
    if (!A || !B) return false;

    // A multi-sweep returns one hit per overlapping COMPONENT, and a character
    // answers ECC_Pawn on both its capsule and its mesh. Every explosion in the game
    // therefore charged double against the same body — found when an eight-cube
    // Hound volley logged sixteen damage events and killed a 300 HP target outright.
    TArray<FHitResult> Hits;
    Hits.AddDefaulted_GetRef().HitObjectHandle = FActorInstanceHandle(A);   // capsule
    Hits.AddDefaulted_GetRef().HitObjectHandle = FActorInstanceHandle(A);   // mesh
    Hits.AddDefaulted_GetRef().HitObjectHandle = FActorInstanceHandle(B);
    Hits.AddDefaulted_GetRef().HitObjectHandle = FActorInstanceHandle(A);   // another A component
    Hits.AddDefaulted();                                                   // nothing hit

    TArray<AWTBRCharacter*> Targets;
    AWTBRProjectileBase::CollectUniqueRadialTargets(Hits, Targets);

    TestEqual(TEXT("Each character is damaged exactly once"), Targets.Num(), 2);
    TestTrue(TEXT("The multi-component character is included"), Targets.Contains(A));
    TestTrue(TEXT("The single-component character is included"), Targets.Contains(B));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepRadiusFloorTest,
    "WTBR.Venyx.Sweep.RadiusFloorAppliesOnlyToLanesThatHunt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepRadiusFloorTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;

    FWTBRPathLane& Hunter = Preset.Lanes.AddDefaulted_GetRef();
    Hunter.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Hunter.HomingRadius = 0.16f;
    Hunter.HomingRadiusFloorUU = 400.0f;

    FWTBRPathLane& Plain = Preset.Lanes.AddDefaulted_GetRef();
    Plain.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.5f, 0.0f)};
    Plain.HomingRadius = 0.0f;
    Plain.HomingRadiusFloorUU = 400.0f;

    // Short shot: the fraction alone would give 160uu, which PIE showed missing by
    // as little as 6uu against arcs that were flying true.
    TArray<TArray<FVector>> Paths;
    TArray<FWTBRResolvedCubeLaunch> Launches;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths,
        0.0f, true, 0, &Launches);

    if (Launches.Num() != 2) return false;
    TestEqual(TEXT("A short shot is lifted to the floor"), Launches[0].HomingRadiusUU, 400.0f);

    // The zero check matters more than the floor does: without it every ordinary
    // Viper and Meteo lane in the game would quietly become a homing sweep.
    TestEqual(TEXT("A lane that does not hunt stays inert"), Launches[1].HomingRadiusUU, 0.0f);

    // Long shot: the fraction is already above the floor, so the floor must not
    // clamp it back down or charging would stop widening the sweep.
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 6000.0f, Paths,
        0.0f, true, 0, &Launches);

    if (Launches.Num() != 2) return false;
    TestEqual(TEXT("A long shot keeps its scaled radius"), Launches[0].HomingRadiusUU, 960.0f);
    TestEqual(TEXT("A non-hunting lane is still inert at range"),
        Launches[1].HomingRadiusUU, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxPresetsKeepSweepInvariantsTest,
    "WTBR.Venyx.Sweep.AuthoredPresetsKeepTheirInvariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxPresetsKeepSweepInvariantsTest::RunTest(const FString& /*Parameters*/)
{
    // Both invariants below were found by simulating these shapes rather than by
    // reasoning about them, and both are the kind of number a balance pass would
    // plausibly nudge without realising what it breaks.
    const FWTBRVenyxParams Params;

    TestEqual(TEXT("Three Hound presets are authored"), Params.VenyxPresets.Num(), 3);
    if (Params.VenyxPresets.Num() != 3) return false;

    for (const FWTBRPathPreset& Preset : Params.VenyxPresets)
    {
        const FString Name = Preset.PresetId.ToString();
        TestTrue(*(Name + TEXT(" has lanes")), Preset.Lanes.Num() > 0);

        float EarliestDelay = TNumericLimits<float>::Max();
        float LatestDelay = -1.0f;
        float EarliestRadius = 0.0f;
        float LatestRadius = 0.0f;

        for (const FWTBRPathLane& Lane : Preset.Lanes)
        {
            const FString LaneName = Name + TEXT(" lane");

            // 1. Every Hound lane must actually hunt, or the wheel offers a choice
            //    that changes nothing — the exact failure this whole pass removed.
            TestTrue(*(LaneName + TEXT(" sweeps")), Lane.HomingRadius > 0.0f);

            // 2. A sweep must come back DOWN. An arc that ends airborne passes over
            //    everyone and hits nothing; under the old locked-target model the
            //    cube always dove at the end, so this could not happen before.
            TestTrue(*(LaneName + TEXT(" has an out and back shape")),
                Lane.NormalizedWaypoints.Num() >= 3);
            if (Lane.NormalizedWaypoints.Num() >= 3)
            {
                const float ApexZ = Lane.NormalizedWaypoints[1].Z;
                const float EndZ = Lane.NormalizedWaypoints.Last().Z;
                TestTrue(*(LaneName + TEXT(" returns below its apex")), EndZ < ApexZ);
                TestTrue(*(LaneName + TEXT(" ends near the ground")), EndZ <= 0.10f);
            }

            if (Lane.LaunchDelay < EarliestDelay)
            {
                EarliestDelay = Lane.LaunchDelay;
                EarliestRadius = Lane.HomingRadius;
            }
            if (Lane.LaunchDelay > LatestDelay)
            {
                LatestDelay = Lane.LaunchDelay;
                LatestRadius = Lane.HomingRadius;
            }
        }

        // 3. Every preset fires in at least two waves, and the late wave sweeps
        //    WIDER — it arrives after the targets have scattered, so it is covering
        //    ground rather than a body.
        TestTrue(*(Name + TEXT(" fires in waves")), LatestDelay > EarliestDelay);
        TestTrue(*(Name + TEXT(" widens its late wave")), LatestRadius >= EarliestRadius);
        TestTrue(*(Name + TEXT(" stays inside the authored delay ceiling")), LatestDelay <= 5.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepActuallyTicksTest,
    "WTBR.Venyx.Sweep.ArmingASweepActuallyEnablesTicking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepActuallyTicksTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxSweepTicks"));
    UWorld* World = Fixture.Get();
    if (!World) return false;

    // Regression: the sweep shipped dead because the projectile's constructor set
    // bCanEverTick = false. Unreal only REGISTERS a tick function when that flag is
    // true at registration, so SetActorTickEnabled() afterwards was a silent no-op
    // and Tick() never ran once. Every other test still passed, because they all
    // exercise the acquisition rule directly rather than through a frame.
    const AWTBRProjectileBase* CDO = GetDefault<AWTBRProjectileBase>();
    TestNotNull(TEXT("Projectile CDO exists"), CDO);
    if (!CDO) return false;
    TestTrue(TEXT("Projectiles can tick at all, or arming one can never work"),
        CDO->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("But ticking stays off until something arms it"),
        CDO->PrimaryActorTick.bStartWithTickEnabled);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Projectile = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Projectile) return false;

    TestFalse(TEXT("A fresh projectile is not ticking"), Projectile->IsActorTickEnabled());
    Projectile->EnableProximityHoming(500.0f, 20000.0f);
    TestTrue(TEXT("Arming a sweep turns ticking on"), Projectile->IsActorTickEnabled());

    // A zero radius is how every non-Hound lane is authored, and it must not put a
    // per-frame world query on projectiles that will never use it.
    AWTBRProjectileBase* Plain = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Plain) return false;
    Plain->EnableProximityHoming(0.0f, 20000.0f);
    TestFalse(TEXT("A zero-radius arm leaves ticking off"), Plain->IsActorTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepSurvivesAcquisitionTest,
    "WTBR.Venyx.Sweep.AcquiringATargetDoesNotDestroyTheCube",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepSurvivesAcquisitionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxSweepSurvives"));
    UWorld* World = Fixture.Get();
    if (!World) return false;

    AWTBRCharacter* Shooter = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 0);
    AWTBRCharacter* Enemy = SpawnVenyxSweepCharacter(World, FVector(300.0f, 0.0f, 0.0f), 1);
    if (!Shooter || !Enemy) return false;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Cube = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Cube) return false;

    Cube->InitializeProjectile(10.0f, 2000.0f, ETriggerCategory::Gunner, false, false, 0.0f);
    Cube->EnableProximityHoming(500.0f, 20000.0f);
    Cube->InitializePathMovement(
        {FVector::ZeroVector, FVector(1000.0f, 0.0f, 0.0f)}, 2000.0f, Shooter);

    // Regression, found in PIE: peeling off the path calls InterpToMovement's
    // Deactivate(), which broadcasts OnInterpToStop -> OnInterpMovementEnd ->
    // Destroy(). The cube therefore killed itself at the exact instant it acquired
    // a target, which from outside is indistinguishable from homing never running.
    Cube->BeginProximityChaseForTest(Enemy);

    TestTrue(TEXT("The cube survives acquiring a target"), IsValid(Cube));
    if (!IsValid(Cube)) return false;
    TestFalse(TEXT("A committed cube stops sweeping"), Cube->IsActorTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxHoldRejectsBadPresetIndexTest,
    "WTBR.Venyx.Sweep.HoldRejectsOutOfRangePresetIndexWithoutSpendingVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxHoldRejectsBadPresetIndexTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxSweepWorldFixture Fixture(TEXT("WTBRVenyxHoldIndex"));
    UWorld* World = Fixture.Get();
    TestNotNull(TEXT("Fixture world exists"), World);
    if (!World) return false;

    AWTBRCharacter* Shooter = SpawnVenyxSweepCharacter(World, FVector::ZeroVector, 0);
    if (!Shooter || !Shooter->VaelComponent) return false;

    UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>();
    DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();

    UWTBRVenyxTrigger* Trigger = NewObject<UWTBRVenyxTrigger>(Shooter);
    Trigger->InitializeTrigger(Shooter, DataAsset);

    Shooter->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
    const float VaelBefore = Shooter->VaelComponent->GetCurrentVael();

    // The index arrives from a client, so the server must bounds-check it. Spending
    // Vael before rejecting would let a malformed request drain a player's meter.
    const int32 OutOfRange = DataAsset->VenyxParams.VenyxPresets.Num() + 5;
    TestFalse(TEXT("An out-of-range preset index is rejected"),
        Trigger->FireSelectedPreset(OutOfRange, 1.0f, true));
    TestFalse(TEXT("A negative preset index is rejected"),
        Trigger->FireSelectedPreset(-1, 1.0f, true));
    TestEqual(TEXT("A rejected request spends no Vael"),
        Shooter->VaelComponent->GetCurrentVael(), VaelBefore);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxHoldChargeBuysReachTest,
    "WTBR.Venyx.Sweep.HoldChargeScalesTheWholeShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxHoldChargeBuysReachTest::RunTest(const FString& /*Parameters*/)
{
    // Charge lerps MinRange -> VenyxRange, and BOTH the waypoints and the homing
    // radius are authored as fractions of that range. So a low-charge shot must come
    // out as the same shape, only smaller — never as a differently-shaped one.
    const FWTBRVenyxParams Params;
    TestTrue(TEXT("Uncharged reach is shorter than full reach"),
        Params.VenyxPresetMinRange < Params.VenyxRange);

    if (Params.VenyxPresets.Num() == 0) return false;
    const FWTBRPathPreset& Preset = Params.VenyxPresets[0];

    TArray<TArray<FVector>> NearPaths, FarPaths;
    TArray<FWTBRResolvedCubeLaunch> NearLaunches, FarLaunches;

    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, Params.VenyxPresetMinRange,
        NearPaths, /*ScatterRadius=*/0.0f, true, 0, &NearLaunches);
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, Params.VenyxRange,
        FarPaths, /*ScatterRadius=*/0.0f, true, 0, &FarLaunches);

    TestEqual(TEXT("Charge does not change how many cubes fly"),
        NearPaths.Num(), FarPaths.Num());
    if (NearLaunches.Num() == 0 || NearLaunches.Num() != FarLaunches.Num()) return false;

    // At full reach the fraction is well clear of the floor, so it must come through
    // untouched — a floor that clamped here would stop charging widening the sweep.
    const float AuthoredFraction = Preset.Lanes[0].HomingRadius;
    TestTrue(TEXT("A full-charge radius is the pure scaled value"),
        FMath::IsNearlyEqual(
            FarLaunches[0].HomingRadiusUU, AuthoredFraction * Params.VenyxRange, 0.01f));

    // Near the floor the two no longer scale proportionally, and that is the point
    // of the floor. What must still hold is that charging never SHRINKS the sweep.
    TestTrue(TEXT("Charging never narrows the sweep"),
        FarLaunches[0].HomingRadiusUU >= NearLaunches[0].HomingRadiusUU);
    TestTrue(TEXT("An uncharged radius is at least its authored floor"),
        NearLaunches[0].HomingRadiusUU >= Preset.Lanes[0].HomingRadiusFloorUU - 0.01f);

    // Delay is a duration, not a distance — charging must not slow the volley down.
    TestEqual(TEXT("Charge never changes launch timing"),
        NearLaunches.Last().DelaySeconds, FarLaunches.Last().DelaySeconds);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
