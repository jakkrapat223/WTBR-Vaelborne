// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"

namespace WTBRSerpveilScatterTestUtils
{
    /** Closest distance between any two points in the set. */
    static float MinimumPairDistance(const TArray<FVector>& Points)
    {
        float Closest = TNumericLimits<float>::Max();
        for (int32 First = 0; First < Points.Num(); ++First)
        {
            for (int32 Second = First + 1; Second < Points.Num(); ++Second)
            {
                Closest = FMath::Min(Closest, FVector::Dist(Points[First], Points[Second]));
            }
        }
        return Closest;
    }

    /** Every offset of a sphere, matching how the trigger indexes one slot's volley. */
    static TArray<FVector> BuildSlotOffsets(int32 CubeCount, float Radius, bool bIsMain)
    {
        TArray<FVector> Offsets;
        Offsets.Reserve(CubeCount);
        for (int32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
        {
            const int32 FibIndex = CubeIndex * 2 + (bIsMain ? 0 : 1);
            Offsets.Add(UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(
                FibIndex, CubeCount * 2, Radius));
        }
        return Offsets;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterRadiusTest,
    "WTBR.Serpveil.Scatter.PointsLieOnSphereRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterRadiusTest::RunTest(const FString& /*Parameters*/)
{
    const float Radius = 135.0f;
    for (int32 Index = 0; Index < 40; ++Index)
    {
        const FVector Offset =
            UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(Index, 40, Radius);
        TestTrue(
            FString::Printf(TEXT("Offset %d lies on the sphere surface"), Index),
            FMath::IsNearlyEqual(Offset.Size(), Radius, 0.01f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterSpacingTest,
    "WTBR.Serpveil.Scatter.MinimumPairSpacingHolds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterSpacingTest::RunTest(const FString& /*Parameters*/)
{
    TArray<FVector> Offsets;
    for (int32 Index = 0; Index < 20; ++Index)
    {
        Offsets.Add(
            UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(Index, 20, 135.0f));
    }

    const float Closest = WTBRSerpveilScatterTestUtils::MinimumPairDistance(Offsets);
    TestTrue(
        FString::Printf(TEXT("20 cubes stay clear of each other (closest %.1f)"), Closest),
        Closest > 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterParityTest,
    "WTBR.Serpveil.Scatter.MainSubParityNeverCollides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterParityTest::RunTest(const FString& /*Parameters*/)
{
    // The whole point of the even/odd split: Main and Sub share one sphere
    // centred on the body, so only interleaving keeps their cubes apart.
    TArray<FVector> Combined = WTBRSerpveilScatterTestUtils::BuildSlotOffsets(20, 135.0f, true);
    Combined.Append(WTBRSerpveilScatterTestUtils::BuildSlotOffsets(20, 135.0f, false));

    TestEqual(TEXT("Main and Sub together produce 40 cubes"), Combined.Num(), 40);

    const float Closest = WTBRSerpveilScatterTestUtils::MinimumPairDistance(Combined);
    TestTrue(
        FString::Printf(TEXT("Simultaneous Main+Sub volleys never overlap (closest %.1f)"), Closest),
        Closest > 40.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterDeterminismTest,
    "WTBR.Serpveil.Scatter.IsDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterDeterminismTest::RunTest(const FString& /*Parameters*/)
{
    // Guards against anyone swapping in randomness later — server and client
    // must derive identical positions without replicating them.
    for (int32 Index = 0; Index < 20; ++Index)
    {
        const FVector First =
            UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(Index, 20, 135.0f);
        const FVector Second =
            UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(Index, 20, 135.0f);
        TestTrue(
            FString::Printf(TEXT("Offset %d is stable across calls"), Index),
            First.Equals(Second, 0.0f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterSingleCubeTest,
    "WTBR.Serpveil.Scatter.SingleCubeReturnsZeroOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterSingleCubeTest::RunTest(const FString& /*Parameters*/)
{
    // Total < 2 would divide by zero in the spiral formula.
    TestTrue(TEXT("Single-point sphere returns a zero offset"),
        UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(0, 1, 135.0f)
            .Equals(FVector::ZeroVector, 0.0f));
    TestTrue(TEXT("Zero-point sphere returns a zero offset"),
        UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(0, 0, 135.0f)
            .Equals(FVector::ZeroVector, 0.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterGroundClearanceTest,
    "WTBR.Serpveil.Scatter.LowestCubeClearsTheFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterGroundClearanceTest::RunTest(const FString& /*Parameters*/)
{
    // Owner-found in PIE: with the sphere centred on the capsule centre, its
    // underside sat below the feet and cubes spawned inside the floor. Guards
    // the defaults against anyone reintroducing that.
    const FWTBRSerpveilParams Defaults;
    const float CapsuleHalfHeight = 96.0f;   // AWTBRCharacter::InitCapsuleSize(42, 96)

    const float LowestCubeZ =
        Defaults.SerpveilScatterHeightOffset - Defaults.SerpveilScatterRadius;

    TestTrue(
        FString::Printf(
            TEXT("Lowest cube (%.1f from capsule centre) stays above the feet (-%.1f)"),
            LowestCubeZ, CapsuleHalfHeight),
        LowestCubeZ > -CapsuleHalfHeight);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilScatterOptInTest,
    "WTBR.Composite.Path.ScatterRadiusZeroKeepsLegacyLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilScatterOptInTest::RunTest(const FString& /*Parameters*/)
{
    // Regression guard for the three composites that share this resolver and are
    // gated until Composite Bullet ships: with scatter off, layout must be the
    // pre-change FormationOffset line-up, exactly.
    FWTBRPathPreset Preset;
    FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
    Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
    Lane.CubeCount = 3;
    Lane.FormationOffset = FVector(0.0f, 50.0f, 0.0f);

    TArray<TArray<FVector>> Paths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, Paths);

    TestEqual(TEXT("Three cubes produce three paths"), Paths.Num(), 3);
    if (Paths.Num() != 3) return false;

    // Centred spread: −50, 0, +50 along Y.
    TestTrue(TEXT("First cube keeps its negative formation offset"),
        Paths[0][0].Equals(FVector(0.0f, -50.0f, 0.0f), KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Middle cube sits on the aim line"),
        Paths[1][0].Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Last cube keeps its positive formation offset"),
        Paths[2][0].Equals(FVector(0.0f, 50.0f, 0.0f), KINDA_SMALL_NUMBER));

    // And the opt-in branch must actually move the cubes off that line.
    TArray<TArray<FVector>> ScatteredPaths;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Preset, FVector::ZeroVector, FRotator::ZeroRotator, 1000.0f, ScatteredPaths,
        135.0f, true);
    TestEqual(TEXT("Scatter produces the same path count"), ScatteredPaths.Num(), 3);
    if (ScatteredPaths.Num() != 3) return false;
    TestFalse(TEXT("Scatter moves the first cube off the legacy position"),
        ScatteredPaths[0][0].Equals(Paths[0][0], KINDA_SMALL_NUMBER));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
