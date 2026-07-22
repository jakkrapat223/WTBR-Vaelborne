// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverSingleLaneTest,
    "WTBR.Composite.Path.SingleLaneSingleCubeIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverSingleLaneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);

    TestEqual(TEXT("Single valid lane creates one path"), Paths.Num(), 1);
    if (Paths.Num() != 1) return false;
    TestEqual(TEXT("Single path has both waypoints"), Paths[0].Num(), 2);
    if (Paths[0].Num() != 2) return false;
    TestTrue(TEXT("First waypoint remains at the origin"),
        Paths[0][0].Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Second waypoint resolves forward by range"),
        Paths[0][1].Equals(FVector(1000.0f, 0.0f, 0.0f), KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverMultiLaneTest,
    "WTBR.Composite.Path.MultiLaneCreatesOnePathPerLane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverMultiLaneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    }

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);
    TestEqual(TEXT("Two valid lanes create two paths"), Paths.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverFormationSpacingTest,
    "WTBR.Composite.Path.MultiCubeFormationSpacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverFormationSpacingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Lane.CubeCount = 3;
    Lane.FormationOffset = FVector(0.0f, 100.0f, 0.0f);

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);

    TestEqual(TEXT("Three cubes create three paths"), Paths.Num(), 3);
    if (Paths.Num() != 3) return false;
    TestTrue(TEXT("First cube uses negative lateral offset"),
        Paths[0][0].Equals(FVector(0.0f, -100.0f, 0.0f), KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Second cube remains centered"),
        Paths[1][0].Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Third cube uses positive lateral offset"),
        Paths[2][0].Equals(FVector(0.0f, 100.0f, 0.0f), KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverDegenerateLaneTest,
    "WTBR.Composite.Path.DegenerateLaneIsSkipped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverDegenerateLaneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {FVector::ZeroVector};

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);
    TestEqual(TEXT("Degenerate lane produces no path"), Paths.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverEmptyPresetTest,
    "WTBR.Composite.Path.EmptyPresetProducesNoPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverEmptyPresetTest::RunTest(const FString& /*Parameters*/)
{
    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        FWTBRPathPreset(), FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);
    TestEqual(TEXT("Empty preset produces no paths"), Paths.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetResolverRotatedAimTest,
    "WTBR.Composite.Path.NonIdentityRotationTransformsWaypoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetResolverRotatedAimTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f), 1000.0f, Paths);

    TestEqual(TEXT("Rotated lane creates one path"), Paths.Num(), 1);
    if (Paths.Num() != 1 || Paths[0].Num() != 2) return false;
    TestTrue(TEXT("Yaw rotation maps forward to world positive Y"),
        Paths[0][1].Equals(FVector(0.0f, 1000.0f, 0.0f), KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetTotalCubeSpreadTest,
    "WTBR.Composite.Path.TotalCubeOverrideSpreadsEvenlyAcrossEqualLanes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetTotalCubeSpreadTest::RunTest(const FString& /*Parameters*/)
{
    // Five lanes of equal weight sharing eight cubes must come out 2/2/2/1/1. The
    // whole point of the override is that "set 8, get 8" holds while the authored
    // ratio survives — piling the surplus onto one lane keeps the count right and
    // throws the SHAPE away, which is the part nobody would notice from a total.
    FWTBRPathPreset Preset;
    for (int32 Lane = 0; Lane < 5; ++Lane)
    {
        FWTBRPathLane& NewLane = Preset.Lanes.AddDefaulted_GetRef();
        NewLane.NormalizedWaypoints = {
            FVector::ZeroVector, FVector(1.0f, 0.1f * Lane, 0.0f)};
        NewLane.CubeCount = 1;
    }

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths,
        /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true, /*TotalCubeOverride=*/8);

    TestEqual(TEXT("Asking for eight cubes yields eight"), Paths.Num(), 8);

    // Paths are flat, so lanes are recovered from each cube's own endpoint — every
    // lane above was given a distinct lateral offset for exactly this.
    TMap<int32, int32> CubesPerLane;
    for (const TArray<FVector>& Path : Paths)
    {
        if (Path.Num() < 2) continue;
        const int32 LaneKey = FMath::RoundToInt(Path.Last().Y);
        CubesPerLane.FindOrAdd(LaneKey)++;
    }

    TestEqual(TEXT("Every lane still flies"), CubesPerLane.Num(), 5);

    int32 Fewest = TNumericLimits<int32>::Max();
    int32 Most = 0;
    for (const TPair<int32, int32>& Pair : CubesPerLane)
    {
        Fewest = FMath::Min(Fewest, Pair.Value);
        Most = FMath::Max(Most, Pair.Value);
    }
    TestTrue(
        FString::Printf(
            TEXT("Equal-weight lanes stay within one cube of each other (got %d..%d)"),
            Fewest, Most),
        Most - Fewest <= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPathPresetConvergeShapeTest,
    "WTBR.Composite.Path.ConvergePresetKeepsItsShapeInvariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPathPresetConvergeShapeTest::RunTest(const FString& /*Parameters*/)
{
    // Guards the three authored properties the Converge shape depends on. Each one
    // is a number a future balance pass could plausibly nudge, and each breaks the
    // shape rather than merely retuning it. Asserted against the authored preset in
    // normalized space, because that is where the invariants live — the resolver's
    // own scaling and rotation are covered by the tests above.
    const FWTBRFulgrixParams Params;

    const FWTBRPathPreset* Converge = Params.FulgrixPresets.FindByPredicate(
        [](const FWTBRPathPreset& Preset)
        {
            return Preset.PresetId == FName(TEXT("Converge"));
        });

    TestNotNull(TEXT("Fulgrix presets contain the Converge shape"), Converge);
    if (Converge == nullptr) return false;
    TestEqual(TEXT("Converge fans into four lanes"), Converge->Lanes.Num(), 4);
    if (Converge->Lanes.Num() != 4) return false;

    const int32 CrossingIndex = 2;
    const FVector Crossing = Converge->Lanes[0].NormalizedWaypoints.IsValidIndex(CrossingIndex)
        ? Converge->Lanes[0].NormalizedWaypoints[CrossingIndex]
        : FVector::ZeroVector;

    auto LegLength = [](const TArray<FVector>& Waypoints, int32 First, int32 Last)
    {
        float Total = 0.0f;
        for (int32 i = First + 1; i <= Last; ++i)
        {
            Total += FVector::Dist(Waypoints[i - 1], Waypoints[i]);
        }
        return Total;
    };

    float ApproachLength = -1.0f;
    float TotalLength = -1.0f;

    for (int32 LaneIndex = 0; LaneIndex < Converge->Lanes.Num(); ++LaneIndex)
    {
        const TArray<FVector>& Waypoints = Converge->Lanes[LaneIndex].NormalizedWaypoints;
        const FString Lane = FString::Printf(TEXT("Lane %d"), LaneIndex);

        TestEqual(*(Lane + TEXT(" has out/cross/in waypoints")), Waypoints.Num(), 4);
        if (Waypoints.Num() != 4) return false;

        // 1. Every lane passes through the same crossing point.
        TestTrue(*(Lane + TEXT(" shares the crossing waypoint")),
            Waypoints[CrossingIndex].Equals(Crossing, KINDA_SMALL_NUMBER));

        // 2. Equal length is what makes the cubes arrive together — duration is
        //    TotalDist/Speed per cube, so unequal lanes trickle through the crossing.
        const float Approach = LegLength(Waypoints, 0, CrossingIndex);
        const float Total = LegLength(Waypoints, 0, Waypoints.Num() - 1);
        if (ApproachLength < 0.0f)
        {
            ApproachLength = Approach;
            TotalLength = Total;
        }
        TestTrue(*(Lane + TEXT(" reaches the crossing after the same distance")),
            FMath::IsNearlyEqual(Approach, ApproachLength, 0.01f));
        TestTrue(*(Lane + TEXT(" travels the same total distance")),
            FMath::IsNearlyEqual(Total, TotalLength, 0.01f));

        // 3. Nothing dips below the aim line, or the cube meets terrain before it
        //    ever reaches the crossing.
        for (const FVector& Waypoint : Waypoints)
        {
            TestTrue(*(Lane + TEXT(" stays on or above the aim line")),
                Waypoint.Z >= -KINDA_SMALL_NUMBER);
        }

        // 4. Exits are mirrored across the fan, so the paths genuinely cross rather
        //    than pinching and continuing on their own side.
        TestTrue(*(Lane + TEXT(" exits on the opposite side it entered")),
            Waypoints[1].Y * Waypoints[3].Y < 0.0f);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
