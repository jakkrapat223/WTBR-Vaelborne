// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"

// Owner rule 2026-07-23: every non-Viper bullet flies a smooth curve, and anything
// with a Viper in it (including composites) keeps Viper's sharp line. These lock the
// two halves of that: the sampling itself, and which weapons get it.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathKeepsEndpointsTest,
    "WTBR.Composite.SmoothPath.KeepsTheAuthoredEndpoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathKeepsEndpointsTest::RunTest(const FString& /*Parameters*/)
{
    // The muzzle and the landing point are not negotiable: a curve that misses
    // either would change where the shot starts or how far it reaches.
    const TArray<FVector> Authored = {
        FVector::ZeroVector,
        FVector(0.5f, 0.4f, 0.3f),
        FVector(1.0f, -0.2f, 0.0f)
    };

    TArray<FVector> Sampled;
    UWTBRCompositeRegistryDataAsset::SampleSmoothPath(Authored, Sampled);

    TestTrue(TEXT("Sampling produces more points than were authored"), Sampled.Num() > Authored.Num());
    TestTrue(TEXT("Curve starts exactly at the caster"),
        Sampled[0].Equals(Authored[0], KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Curve ends exactly at the authored landing point"),
        Sampled.Last().Equals(Authored.Last(), KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathPassesThroughMidWaypointTest,
    "WTBR.Composite.SmoothPath.PassesThroughEveryAuthoredWaypoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathPassesThroughMidWaypointTest::RunTest(const FString& /*Parameters*/)
{
    // This is the whole reason for Catmull-Rom over Bezier. A Bezier's control
    // points sit OFF the curve, so a dragged handle would not be where the line
    // goes; here every authored point must appear on the sampled path exactly.
    const FVector Mid(0.5f, 0.4f, 0.3f);
    const TArray<FVector> Authored = {FVector::ZeroVector, Mid, FVector(1.0f, -0.2f, 0.0f)};

    TArray<FVector> Sampled;
    UWTBRCompositeRegistryDataAsset::SampleSmoothPath(Authored, Sampled);

    bool bFoundMid = false;
    for (const FVector& Point : Sampled)
    {
        if (Point.Equals(Mid, KINDA_SMALL_NUMBER))
        {
            bFoundMid = true;
            break;
        }
    }
    TestTrue(TEXT("The authored mid waypoint lies on the sampled curve"), bFoundMid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathLeavesShortLanesAloneTest,
    "WTBR.Composite.SmoothPath.TwoPointLaneIsNotResampled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathLeavesShortLanesAloneTest::RunTest(const FString& /*Parameters*/)
{
    // Two points describe a straight line and nothing else. Sampling it would spend
    // real InterpToMovement control points — a per-tick cost on every cube — on
    // points that all sit on the same line.
    const TArray<FVector> Straight = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};

    TArray<FVector> Sampled;
    UWTBRCompositeRegistryDataAsset::SampleSmoothPath(Straight, Sampled);

    TestEqual(TEXT("A two-point lane is passed through untouched"), Sampled.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathActuallyBowsTest,
    "WTBR.Composite.SmoothPath.CurveLeavesTheStraightSegments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathActuallyBowsTest::RunTest(const FString& /*Parameters*/)
{
    // Endpoints and waypoints all landing correctly would still be satisfied by
    // simply subdividing the straight segments, which would look identical to the
    // old behaviour. Prove at least one sample leaves the polyline.
    const TArray<FVector> Authored = {
        FVector::ZeroVector,
        FVector(0.5f, 0.5f, 0.0f),
        FVector(1.0f, 0.0f, 0.0f)
    };

    TArray<FVector> Sampled;
    UWTBRCompositeRegistryDataAsset::SampleSmoothPath(Authored, Sampled);

    // Measured as distance from the authored POLYLINE rather than by looking for a
    // sample near some chosen X. An earlier version of this test did the latter and
    // failed against correct output purely because the samples straddled the window
    // it was watching — the deviation is the thing being asserted, so measure it.
    float MaxDeviation = 0.0f;
    for (const FVector& Point : Sampled)
    {
        float NearestOnPolyline = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index < Authored.Num() - 1; ++Index)
        {
            NearestOnPolyline = FMath::Min(NearestOnPolyline,
                FMath::PointDistToSegment(Point, Authored[Index], Authored[Index + 1]));
        }
        MaxDeviation = FMath::Max(MaxDeviation, NearestOnPolyline);
    }

    // In fractions of range: 0.01 is 40uu on a 4000uu shot, comfortably visible and
    // far above float noise.
    TestTrue(
        FString::Printf(TEXT("The sampled path bows away from the straight segments (max deviation %.4f)"),
            MaxDeviation),
        MaxDeviation > 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathViperStaysSharpTest,
    "WTBR.Composite.SmoothPath.OnlyViperWeaponsKeepTheSharpLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathViperStaysSharpTest::RunTest(const FString& /*Parameters*/)
{
    using EArch = EWTBRBulletArchetype;

    TestTrue(TEXT("Viper alone is sharp"),
        UWTBRCompositeRegistryDataAsset::UsesSharpPath(EArch::Serpveil, EArch::None));
    TestTrue(TEXT("A composite whose SECOND half is a Viper is still sharp"),
        UWTBRCompositeRegistryDataAsset::UsesSharpPath(EArch::Venyx, EArch::Serpveil));
    TestTrue(TEXT("Viper + Viper (Labyrn) is sharp"),
        UWTBRCompositeRegistryDataAsset::UsesSharpPath(EArch::Serpveil, EArch::Serpveil));

    TestFalse(TEXT("Hound alone curves"),
        UWTBRCompositeRegistryDataAsset::UsesSharpPath(EArch::Venyx, EArch::None));
    TestFalse(TEXT("A composite with no Viper in it curves"),
        UWTBRCompositeRegistryDataAsset::UsesSharpPath(EArch::Venyx, EArch::Fulgrix));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSmoothPathResolverOptInTest,
    "WTBR.Composite.SmoothPath.ResolverSmoothsOnlyWhenAsked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSmoothPathResolverOptInTest::RunTest(const FString& /*Parameters*/)
{
    // Defaulting to OFF is what keeps Viper, and every caller not yet audited,
    // flying the exact path it flew before this feature existed.
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {
        FVector::ZeroVector, FVector(0.5f, 0.5f, 0.0f), FVector(1.0f, 0.0f, 0.0f)};

    TArray<TArray<FVector>> SharpPaths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, SharpPaths);

    TArray<TArray<FVector>> SmoothPaths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, SmoothPaths,
        /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true, /*TotalCubeOverride=*/0,
        /*OutCubeLaunches=*/nullptr, /*MaxTurns=*/0, /*bSmoothCurve=*/true);

    if (!TestEqual(TEXT("Both resolve one path"), SharpPaths.Num(), 1)) return false;
    if (!TestEqual(TEXT("Smooth resolves one path too"), SmoothPaths.Num(), 1)) return false;

    TestEqual(TEXT("Default keeps exactly the authored waypoints"), SharpPaths[0].Num(), 3);
    TestTrue(TEXT("Opting in produces a denser path"), SmoothPaths[0].Num() > SharpPaths[0].Num());

    // Reach must not change just because the line curves.
    TestTrue(TEXT("Both land on the same final point"),
        SmoothPaths[0].Last().Equals(SharpPaths[0].Last(), 0.01f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
