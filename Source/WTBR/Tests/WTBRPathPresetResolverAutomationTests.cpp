// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
