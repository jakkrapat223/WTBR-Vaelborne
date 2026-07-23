// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InterpToMovementComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "Trigger/WTBRTriggerDataAsset.h"

// -----------------------------------------------------------------------------
// Lane events (WTBR.Composite.LaneEvent.*)
//
// The player clicks a spot on the lane they drew and says what happens there. Two
// things belong to the START of the shot and are NOT events — the launch delay and
// how many cubes the block splits into — so everything here is a change to a cube
// already in the air: hover, homing on/off, speed.
//
// The firing rule is measured in DISTANCE travelled, never elapsed time, because a
// hover stops the cube without stopping the clock and a speed change breaks the
// time-to-distance mapping outright. Distance is also literally what the player
// clicked on.
// -----------------------------------------------------------------------------

namespace
{
    FWTBRLaneEvent WTBRMakeEvent(EWTBRLaneEventType Type, float AtFraction)
    {
        FWTBRLaneEvent Event;
        Event.Type = Type;
        Event.AtPathFraction = AtFraction;
        return Event;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLaneEventsRideThroughResolutionTest,
    "WTBR.Composite.LaneEvent.MarkersSurviveTheFlattenIntoPerCubePaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLaneEventsRideThroughResolutionTest::RunTest(const FString& /*Parameters*/)
{
    // ResolvePathPreset flattens lanes into one flat list of cube paths, which loses
    // which lane a cube came from. Events are authored PER LANE, so they have to be
    // carried across with the cube or they silently never fire — the same trap the
    // launch delay and homing radius already needed solving.
    FWTBRPathPreset Preset;

    FWTBRPathLane Marked;
    Marked.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Marked.CubeCount = 2;
    Marked.Events.Add(WTBRMakeEvent(EWTBRLaneEventType::Hover, 0.4f));
    Marked.Events.Add(WTBRMakeEvent(EWTBRLaneEventType::SetHoming, 0.8f));
    Preset.Lanes.Add(Marked);

    FWTBRPathLane Plain;
    Plain.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.3f, 0.0f)};
    Plain.CubeCount = 2;
    Preset.Lanes.Add(Plain);

    TArray<TArray<FVector>> Paths;
    TArray<FWTBRResolvedCubeLaunch> Launches;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths,
        /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true, /*TotalCubeOverride=*/0, &Launches);

    TestEqual(TEXT("One launch entry per cube"), Launches.Num(), Paths.Num());

    int32 WithEvents = 0;
    int32 WithoutEvents = 0;
    for (const FWTBRResolvedCubeLaunch& Launch : Launches)
    {
        if (Launch.Events.Num() == 2) ++WithEvents;
        if (Launch.Events.Num() == 0) ++WithoutEvents;
    }

    // Every cube of the marked lane carries the markers; the plain lane's carry none.
    TestEqual(TEXT("The marked lane's cubes all carry its markers"), WithEvents, 2);
    TestEqual(TEXT("The unmarked lane's cubes carry none"), WithoutEvents, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLaneEventsFireByDistanceTest,
    "WTBR.Composite.LaneEvent.MarkersFireInOrderOfDistanceNotAuthoringOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLaneEventsFireByDistanceTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WTBRLaneEventOrder"));
    if (!World) return false;
    if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Cube = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

    if (Cube)
    {
        Cube->InitializeProjectile(10.0f, 2000.0f, ETriggerCategory::Gunner, false, false, 0.0f);

        // Deliberately authored out of order — the player clicks wherever they like,
        // and a later click can land earlier along the line.
        TArray<FWTBRLaneEvent> Events;
        Events.Add(WTBRMakeEvent(EWTBRLaneEventType::Hover, 0.9f));
        Events.Add(WTBRMakeEvent(EWTBRLaneEventType::SetHoming, 0.2f));
        Cube->SetAuthoredLaneEventsForTest(Events);

        Cube->InitializePathMovement(
            {FVector::ZeroVector, FVector(1000.0f, 0.0f, 0.0f)}, 2000.0f, nullptr);

        // Armed, sorted, and none consumed yet: the cube has not moved.
        TestEqual(TEXT("Both markers armed"), Cube->GetPendingLaneEventCountForTest(), 2);
        TestTrue(TEXT("The nearer marker is queued first regardless of authoring order"),
            FMath::IsNearlyEqual(Cube->GetNextLaneEventDistanceForTest(), 200.0f, 1.0f));
    }

    if (GEngine) GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPresetCubeCountIsClampedTest,
    "WTBR.Composite.LaneEvent.PresetCubeCountNeverDropsBelowTheTapCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPresetCubeCountIsClampedTest::RunTest(const FString& /*Parameters*/)
{
    // A preset may divide the SAME damage budget further — more cubes, each weaker —
    // but never into fewer, heavier ones. Fewer-and-heavier would be a power increase
    // dressed as a pattern, which the tap/hold lock forbids.
    //
    // Checked on the resolver directly, since that is where a count becomes cubes.
    FWTBRPathPreset Preset;
    FWTBRPathLane Lane;
    Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Lane.CubeCount = 1;
    Preset.Lanes.Add(Lane);
    Preset.Lanes.Add(Lane);

    auto CountFor = [&Preset](int32 Override)
    {
        TArray<TArray<FVector>> Paths;
        UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
            Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths,
            0.0f, true, Override, nullptr);
        return Paths.Num();
    };

    TestEqual(TEXT("A clamped-up request spends the floor"), CountFor(8), 8);
    TestEqual(TEXT("A clamped-down request spends the ceiling"), CountFor(15), 15);

    // Zero is the untouched case: authored per-lane counts are used literally, which
    // is what every preset written before this field existed relies on.
    TestEqual(TEXT("Zero leaves the authored per-lane counts alone"), CountFor(0), 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
