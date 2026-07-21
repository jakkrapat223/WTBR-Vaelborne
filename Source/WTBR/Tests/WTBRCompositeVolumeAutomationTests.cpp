// Copyright Vaelborne: Dominion. All Rights Reserved.

// Covers the invariants introduced with the ready-composite Tap/Hold flow that no
// single-behavior test owns: that one CubeCount drives the volley size for both
// tap and hold, and that TotalDamageBudget is SPLIT across that volley rather than
// handed to every cube. The second one matters more than it looks — the composite
// design lock calls out "each core gets FULL damage" as a real bug class already
// hit once by Labyrn, and a per-cube budget is exactly how it reappears.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRSolveilCompositeBehavior.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCompositeVolumeWorldFixture
    {
    public:
        explicit FWTBRCompositeVolumeWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeVolumeWorldFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
            }
        }

        UWorld* GetWorld() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    AWTBRCharacter* SpawnCompositeVolumeCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    int32 CountCompositeVolumeProjectiles(UWorld* World, const AActor* Owner)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetOwner() == Owner) ++Count;
        }
        return Count;
    }

    float SumCompositeVolumeDamage(UWorld* World, const AActor* Owner)
    {
        float Total = 0.0f;
        if (!World) return Total;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetOwner() == Owner) Total += It->BaseDamage;
        }
        return Total;
    }

    // Two lanes weighted 3:1, so a redistributed total has a proportion to preserve
    // rather than just a count to hit.
    FWTBRCompositeDefinition MakeCompositeVolumeDefinition(int32 CubeCount)
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Solveil;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 120.0f;
        Definition.ProjectileSpeed = 2800.0f;
        Definition.PathRange = 1500.0f;
        Definition.CubeCount = CubeCount;

        FWTBRPathLane& Heavy = Definition.PathPreset.Lanes.AddDefaulted_GetRef();
        Heavy.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.2f, 0.0f)};
        Heavy.CubeCount = 3;
        Heavy.FormationOffset = FVector::ZeroVector;

        FWTBRPathLane& Light = Definition.PathPreset.Lanes.AddDefaulted_GetRef();
        Light.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, -0.2f, 0.0f)};
        Light.CubeCount = 1;
        Light.FormationOffset = FVector::ZeroVector;

        return Definition;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeVolumeDamageIsSplitTest,
    "WTBR.Composite.Volume.DamageBudgetIsSplitNotMultiplied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeVolumeDamageIsSplitTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeVolumeWorldFixture Fixture(TEXT("WTBR_CompositeVolume_Damage"));
    AWTBRCharacter* Owner = SpawnCompositeVolumeCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeCompositeVolumeDefinition(8);
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestTrue(TEXT("Behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    const int32 Spawned = CountCompositeVolumeProjectiles(Fixture.GetWorld(), Owner);
    TestTrue(TEXT("A volley spawned"), Spawned > 1);

    // The whole point: more cubes must never mean more total damage.
    TestEqual(TEXT("Summed damage equals the budget, not budget x cubes"),
        SumCompositeVolumeDamage(Fixture.GetWorld(), Owner), Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeVolumeCubeCountHonouredTest,
    "WTBR.Composite.Volume.CubeCountDrivesVolleySize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeVolumeCubeCountHonouredTest::RunTest(const FString& /*Parameters*/)
{
    // "Set 8, get 8" has to hold even though the preset's own lanes only add up to
    // four — lane counts are weights once a composite supplies a total.
    FWTBRCompositeVolumeWorldFixture Fixture(TEXT("WTBR_CompositeVolume_Count"));
    AWTBRCharacter* Owner = SpawnCompositeVolumeCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeCompositeVolumeDefinition(8);
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestTrue(TEXT("Behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    TestEqual(TEXT("Volley size matches Definition.CubeCount"),
        CountCompositeVolumeProjectiles(Fixture.GetWorld(), Owner), Definition.CubeCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeVolumeWeightsPreservedTest,
    "WTBR.Composite.Volume.LaneWeightsSurviveRedistribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeVolumeWeightsPreservedTest::RunTest(const FString& /*Parameters*/)
{
    // 3:1 lanes redistributed to 8 cubes should land 6:2, not 4:4. Checked through
    // ResolvePathPreset directly so the assertion is about the distribution maths
    // rather than about anything the spawning behavior does afterwards.
    TArray<TArray<FVector>> Paths;
    const FWTBRCompositeDefinition Definition = MakeCompositeVolumeDefinition(8);

    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Definition.PathPreset, FVector::ZeroVector, FRotator::ZeroRotator,
        Definition.PathRange, Paths, /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true,
        /*TotalCubeOverride=*/8);

    TestEqual(TEXT("Redistribution hits the requested total exactly"), Paths.Num(), 8);

    // Lanes bend to opposite sides, so the sign of the endpoint's Y separates them.
    int32 HeavyLane = 0;
    int32 LightLane = 0;
    for (const TArray<FVector>& Path : Paths)
    {
        if (Path.Num() < 2) continue;
        (Path.Last().Y > 0.0f ? HeavyLane : LightLane)++;
    }
    TestEqual(TEXT("Heavy lane keeps 3/4 of the volley"), HeavyLane, 6);
    TestEqual(TEXT("Light lane keeps 1/4 of the volley"), LightLane, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeVolumeDistinctSpawnPointsTest,
    "WTBR.Composite.Volume.CubesDoNotShareASpawnPoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeVolumeDistinctSpawnPointsTest::RunTest(const FString& /*Parameters*/)
{
    // Regression guard for a real PIE bug: every cube of a hold-fired volley was
    // conjured on ONE point, so they overlapped and destroyed each other before
    // moving. Nothing fired, and no test caught it — headless fixtures have no
    // physics scene, so the collision itself is invisible here.
    //
    // What IS checkable is the precondition: each cube must start somewhere
    // different. Assert that instead of the collision, so the bug cannot come back
    // silently if someone zeroes the scatter radius.
    TArray<TArray<FVector>> Paths;
    FWTBRCompositeDefinition Definition = MakeCompositeVolumeDefinition(8);

    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Definition.PathPreset, FVector::ZeroVector, FRotator::ZeroRotator,
        Definition.PathRange, Paths, /*ScatterRadius=*/Definition.TapScatterRadius,
        /*bIsMainSlot=*/true, /*TotalCubeOverride=*/8);

    TestEqual(TEXT("Volley resolved"), Paths.Num(), 8);

    // Smallest gap between any two starting points, compared against the
    // projectile's own footprint rather than an arbitrary epsilon.
    float ClosestPair = TNumericLimits<float>::Max();
    for (int32 i = 0; i < Paths.Num(); ++i)
    {
        if (Paths[i].Num() == 0) continue;
        for (int32 j = i + 1; j < Paths.Num(); ++j)
        {
            if (Paths[j].Num() == 0) continue;
            ClosestPair = FMath::Min(
                ClosestPair, FVector::Dist(Paths[i][0], Paths[j][0]));
        }
    }

    TestTrue(TEXT("No two cubes are conjured on the same point"), ClosestPair > 1.0f);
    TestTrue(TEXT("Spawn spacing clears a plausible projectile footprint"), ClosestPair > 30.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
