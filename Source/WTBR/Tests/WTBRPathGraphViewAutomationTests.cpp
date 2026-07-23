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

#endif // WITH_DEV_AUTOMATION_TESTS
