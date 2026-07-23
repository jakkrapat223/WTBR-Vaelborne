// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/WTBRPathGraphViewWidget.h"

// Painting itself needs a real viewport and is left to PIE, matching the convention
// stated in WTBRTriggerWheelAutomationTests. What IS testable headlessly is the
// projection, which is where a sign error would hide: an inverted vertical axis
// looks perfectly fine on any symmetric preset and only betrays itself on an
// asymmetric one, which is exactly what a player will author first.

namespace
{
    // 100x100 pane at the origin keeps every expected value trivially readable.
    const FSlateRect TestPane(0.0f, 0.0f, 100.0f, 100.0f);
    constexpr float TestForwardMin = 0.0f;
    constexpr float TestForwardMax = 1.0f;
    constexpr float TestLateralExtent = 0.5f;

    FVector2D Project(const FVector& Normalized, bool bElevation)
    {
        return UWTBRPathGraphViewWidget::ProjectNormalizedToPane(
            Normalized, TestPane, bElevation, TestForwardMin, TestForwardMax, TestLateralExtent);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphProjectionCasterTest,
    "WTBR.UI.PathGraph.CasterSitsAtTheLeftEdgeOnTheZeroAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphProjectionCasterTest::RunTest(const FString& /*Parameters*/)
{
    // Double literals, not float: FVector2D is double-precision in UE5, and a float
    // expected value makes TestEqual's overload set ambiguous.
    const FVector2D Plan = Project(FVector::ZeroVector, /*bElevation=*/false);
    TestEqual(TEXT("Caster is at the pane's left edge"), Plan.X, 0.0);
    TestEqual(TEXT("Caster is on the vertical centre line"), Plan.Y, 50.0);

    const FVector2D Elevation = Project(FVector::ZeroVector, /*bElevation=*/true);
    TestEqual(TEXT("Caster is at the left edge in elevation too"), Elevation.X, 0.0);
    TestEqual(TEXT("Caster is on the centre line in elevation too"), Elevation.Y, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphProjectionForwardTest,
    "WTBR.UI.PathGraph.ForwardMaxReachesTheRightEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphProjectionForwardTest::RunTest(const FString& /*Parameters*/)
{
    const FVector2D Full = Project(FVector(1.0f, 0.0f, 0.0f), /*bElevation=*/false);
    TestEqual(TEXT("Forward max maps to the right edge"), Full.X, 100.0);

    const FVector2D Half = Project(FVector(0.5f, 0.0f, 0.0f), /*bElevation=*/false);
    TestEqual(TEXT("Half forward maps to the pane's midpoint"), Half.X, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphProjectionVerticalSignTest,
    "WTBR.UI.PathGraph.PositiveValuesDrawAboveTheCentreLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphProjectionVerticalSignTest::RunTest(const FString& /*Parameters*/)
{
    // Slate's Y axis grows downward, so "above" means a SMALLER Y. Getting this
    // backwards mirrors every preset vertically.
    const FVector2D PlanPositive = Project(FVector(0.0f, 0.25f, 0.0f), /*bElevation=*/false);
    TestTrue(TEXT("Positive lateral draws above the centre line in plan"), PlanPositive.Y < 50.0f);

    const FVector2D PlanNegative = Project(FVector(0.0f, -0.25f, 0.0f), /*bElevation=*/false);
    TestTrue(TEXT("Negative lateral draws below the centre line in plan"), PlanNegative.Y > 50.0f);

    const FVector2D ElevationUp = Project(FVector(0.0f, 0.0f, 0.25f), /*bElevation=*/true);
    TestTrue(TEXT("Positive vertical draws above the centre line in elevation"), ElevationUp.Y < 50.0f);

    const FVector2D ElevationDown = Project(FVector(0.0f, 0.0f, -0.25f), /*bElevation=*/true);
    TestTrue(TEXT("Negative vertical draws below the centre line in elevation"), ElevationDown.Y > 50.0f);

    // At full extent the value must land exactly on the pane edge, not past it.
    const FVector2D AtExtent = Project(FVector(0.0f, TestLateralExtent, 0.0f), /*bElevation=*/false);
    TestEqual(TEXT("Lateral at full extent lands on the pane's top edge"), AtExtent.Y, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphProjectionPaneAxisTest,
    "WTBR.UI.PathGraph.EachPaneReadsOnlyItsOwnAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphProjectionPaneAxisTest::RunTest(const FString& /*Parameters*/)
{
    // A lane bowing sideways must not appear to climb in the elevation view, and
    // vice versa — that would make the two panes contradict each other and the
    // player would be dragging against a lie.
    const FVector LateralOnly(0.5f, 0.3f, 0.0f);
    TestEqual(TEXT("Elevation pane ignores a purely lateral waypoint"),
        Project(LateralOnly, /*bElevation=*/true).Y, 50.0);

    const FVector VerticalOnly(0.5f, 0.0f, 0.3f);
    TestEqual(TEXT("Plan pane ignores a purely vertical waypoint"),
        Project(VerticalOnly, /*bElevation=*/false).Y, 50.0);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Drag editing. The pixel side is only the unprojection; every RULE below is
// geometry-free by design, which is what makes it testable without a viewport.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    FVector Unproject(const FVector2D& LocalPoint, bool bElevation, const FVector& PreservedFrom)
    {
        return UWTBRPathGraphViewWidget::UnprojectPaneToNormalized(
            LocalPoint, TestPane, bElevation,
            TestForwardMin, TestForwardMax, TestLateralExtent, PreservedFrom);
    }

    // Matches the widget's own defaults so projection and unprojection agree.
    UWTBRPathGraphViewWidget* MakeGraphWithLane(const TArray<FVector>& Waypoints)
    {
        UWTBRPathGraphViewWidget* Widget = NewObject<UWTBRPathGraphViewWidget>();
        FWTBRPathPreset Preset;
        Preset.PresetId = FName(TEXT("Test"));
        FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = Waypoints;
        Widget->SetPreset(Preset);
        return Widget;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphUnprojectRoundTripTest,
    "WTBR.UI.PathGraph.UnprojectIsTheInverseOfProject",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphUnprojectRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    // If these two ever disagree, a dragged handle drifts away from the cursor.
    const FVector Original(0.4f, 0.25f, -0.1f);

    const FVector2D PlanScreen = Project(Original, /*bElevation=*/false);
    const FVector PlanBack = Unproject(PlanScreen, /*bElevation=*/false, Original);
    TestTrue(TEXT("Plan round-trip returns the original waypoint"),
        PlanBack.Equals(Original, 0.001f));

    const FVector2D ElevScreen = Project(Original, /*bElevation=*/true);
    const FVector ElevBack = Unproject(ElevScreen, /*bElevation=*/true, Original);
    TestTrue(TEXT("Elevation round-trip returns the original waypoint"),
        ElevBack.Equals(Original, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphUnprojectPreservesHiddenAxisTest,
    "WTBR.UI.PathGraph.DraggingInOnePaneLeavesTheOtherAxisAlone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphUnprojectPreservesHiddenAxisTest::RunTest(const FString& /*Parameters*/)
{
    // The failure this guards against is subtle and destructive: drag a lane sideways
    // in plan, and its whole authored height collapses to zero because the pane that
    // cannot see Z wrote one anyway.
    const FVector Existing(0.3f, 0.1f, 0.45f);

    const FVector AfterPlanDrag = Unproject(FVector2D(80.0f, 20.0f), /*bElevation=*/false, Existing);
    TestEqual(TEXT("A plan drag preserves Z exactly"), AfterPlanDrag.Z, Existing.Z);
    TestNotEqual(TEXT("A plan drag does change Y"), AfterPlanDrag.Y, Existing.Y);

    const FVector AfterElevDrag = Unproject(FVector2D(80.0f, 20.0f), /*bElevation=*/true, Existing);
    TestEqual(TEXT("An elevation drag preserves Y exactly"), AfterElevDrag.Y, Existing.Y);
    TestNotEqual(TEXT("An elevation drag does change Z"), AfterElevDrag.Z, Existing.Z);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphCasterIsImmovableTest,
    "WTBR.UI.PathGraph.CasterWaypointCanBeNeitherMovedNorDeleted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphCasterIsImmovableTest::RunTest(const FString& /*Parameters*/)
{
    // ResolvePathPreset assumes waypoint 0 is the caster, and the server's sanitizer
    // DROPS any lane that starts elsewhere — so a movable caster would let the editor
    // author presets that silently disappear on upload.
    UWTBRPathGraphViewWidget* Widget = MakeGraphWithLane(
        {FVector::ZeroVector, FVector(0.5f, 0.2f, 0.1f), FVector(1.0f, 0.0f, 0.0f)});

    TestFalse(TEXT("Moving the caster is refused"),
        Widget->TryMoveWaypoint(0, 0, FVector(0.2f, 0.2f, 0.0f)));
    TestFalse(TEXT("Deleting the caster is refused"), Widget->TryDeleteWaypoint(0, 0));

    TestTrue(TEXT("The caster is still at the origin"),
        Widget->GetPreset().Lanes[0].NormalizedWaypoints[0].IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphWaypointCapTest,
    "WTBR.UI.PathGraph.InsertStopsAtTheWaypointCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphWaypointCapTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRPathGraphViewWidget* Widget = MakeGraphWithLane(
        {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)});

    int32 Inserted = 0;
    while (Widget->CanAddWaypointToLane(0))
    {
        if (!Widget->TryInsertWaypoint(0, 1, FVector(0.5f, 0.1f, 0.0f))) break;
        ++Inserted;
        // Guards against the cap never engaging and spinning forever.
        if (Inserted > WTBR_MAX_CUSTOM_LANE_WAYPOINTS * 4) break;
    }

    TestEqual(TEXT("The lane fills exactly to the cap"),
        Widget->GetPreset().Lanes[0].NormalizedWaypoints.Num(), WTBR_MAX_CUSTOM_LANE_WAYPOINTS);
    TestFalse(TEXT("A full lane reports it cannot take more"), Widget->CanAddWaypointToLane(0));
    TestFalse(TEXT("And refuses one anyway"),
        Widget->TryInsertWaypoint(0, 1, FVector(0.5f, 0.1f, 0.0f)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphKeepsTwoWaypointsTest,
    "WTBR.UI.PathGraph.DeleteNeverLeavesFewerThanTwoWaypoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphKeepsTwoWaypointsTest::RunTest(const FString& /*Parameters*/)
{
    // ResolvePathPreset skips any lane with fewer than two waypoints, so deleting
    // down to one would produce a lane that silently fires nothing.
    UWTBRPathGraphViewWidget* Widget = MakeGraphWithLane(
        {FVector::ZeroVector, FVector(0.5f, 0.2f, 0.0f), FVector(1.0f, 0.0f, 0.0f)});

    TestTrue(TEXT("Deleting the middle bend is allowed"), Widget->TryDeleteWaypoint(0, 1));
    TestEqual(TEXT("Two waypoints remain"), Widget->GetPreset().Lanes[0].NormalizedWaypoints.Num(), 2);

    TestFalse(TEXT("Deleting the landing point is refused at two"), Widget->TryDeleteWaypoint(0, 1));
    TestEqual(TEXT("Still two waypoints"), Widget->GetPreset().Lanes[0].NormalizedWaypoints.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphMoveClampsTest,
    "WTBR.UI.PathGraph.MoveClampsToTheViewBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphMoveClampsTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRPathGraphViewWidget* Widget = MakeGraphWithLane(
        {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)});

    TestTrue(TEXT("Move succeeds"),
        Widget->TryMoveWaypoint(0, 1, FVector(999.0f, 999.0f, -999.0f)));

    // Cast to float explicitly: FVector components are double in UE5, and comparing
    // one against a float property leaves TestEqual's overload set ambiguous.
    const FVector Moved = Widget->GetPreset().Lanes[0].NormalizedWaypoints[1];
    TestEqual(TEXT("Forward is clamped to the view's max"),
        static_cast<float>(Moved.X), Widget->ForwardMax);
    TestEqual(TEXT("Lateral is clamped to the view's extent"),
        static_cast<float>(Moved.Y), Widget->LateralExtent);
    TestEqual(TEXT("Vertical is clamped to the negative extent"),
        static_cast<float>(Moved.Z), -Widget->LateralExtent);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathGraphEditNotifiesTest,
    "WTBR.UI.PathGraph.EveryAcceptedEditBroadcastsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathGraphEditNotifiesTest::RunTest(const FString& /*Parameters*/)
{
    // The editor root widget holds the draft this view mutates; a missed broadcast
    // means Save writes a preset the player did not see.
    UWTBRPathGraphViewWidget* Widget = MakeGraphWithLane(
        {FVector::ZeroVector, FVector(0.5f, 0.2f, 0.0f), FVector(1.0f, 0.0f, 0.0f)});

    int32 EditCount = 0;
    Widget->OnPresetEdited.AddLambda([&EditCount]() { ++EditCount; });

    Widget->TryMoveWaypoint(0, 1, FVector(0.4f, 0.1f, 0.0f));
    TestEqual(TEXT("A move notifies"), EditCount, 1);

    Widget->TryInsertWaypoint(0, 1, FVector(0.2f, 0.0f, 0.0f));
    TestEqual(TEXT("An insert notifies"), EditCount, 2);

    Widget->TryDeleteWaypoint(0, 1);
    TestEqual(TEXT("A delete notifies"), EditCount, 3);

    Widget->TryMoveWaypoint(0, 0, FVector(0.1f, 0.0f, 0.0f));
    TestEqual(TEXT("A REFUSED edit does not notify"), EditCount, 3);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
