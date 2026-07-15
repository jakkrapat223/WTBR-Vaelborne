// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WTBRCharacter.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Random Spawn Automation Tests (WTBR.Combat.Team.Spawn.*)
//
// Mode design lock 2026-07-13: 15P/BR players spawn at random points on the
// match map with "appropriate" spacing between them. AWTBRGameMode::
// GenerateRandomSpawnPoints is a pure, seedable algorithm (rejection sampling
// inside a disc) tested directly here without a world; ApplyRandomSpawnPositions
// (the GameMode-level wiring that teleports every AWTBRCharacter) is tested
// against a headless world via the ApplyRandomSpawnPositionsForTest seam.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRSpawnWorldFixture
    {
    public:
        explicit FWTBRSpawnWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRSpawnWorldFixture()
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

    AWTBRCharacter* SpawnPlainCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    float MinPairwiseDistance(const TArray<FVector>& Points)
    {
        float MinDist = TNumericLimits<float>::Max();
        for (int32 i = 0; i < Points.Num(); ++i)
        {
            for (int32 j = i + 1; j < Points.Num(); ++j)
            {
                MinDist = FMath::Min(MinDist, FVector::Dist(Points[i], Points[j]));
            }
        }
        return MinDist;
    }

    TArray<FVector> MakeSafeAnchorTestSet()
    {
        TArray<FVector> Anchors;
        for (int32 X = 0; X < 5; ++X)
        {
            for (int32 Y = 0; Y < 4; ++Y)
            {
                Anchors.Add(FVector(X * 12000.0f, Y * 12000.0f, 0.0f));
            }
        }
        return Anchors;
    }
}

// ── Algorithm: correct count, inside the area, minimum spacing honored ─────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSpawnPointsCountAndSpacingTest,
    "WTBR.Combat.Team.Spawn.PointsRespectCountAreaAndSpacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSpawnPointsCountAndSpacingTest::RunTest(const FString& /*Parameters*/)
{
    const FVector Center(1000.0f, -500.0f, 200.0f);
    const float AreaRadius = 50000.0f; // 500 m
    const float MinDistance = 3000.0f; // 30 m — comfortably satisfiable for 15 points in a 500 m disc.
    const int32 Count = 15;

    const TArray<FVector> Points = AWTBRGameMode::GenerateRandomSpawnPointsForTest(Center, AreaRadius, MinDistance, Count, /*Seed=*/12345);

    TestEqual(TEXT("Returns exactly Count points"), Points.Num(), Count);

    for (const FVector& Point : Points)
    {
        TestTrue(TEXT("Point stays within the spawn disc (+small float slack)"),
            FVector::Dist2D(Point, Center) <= AreaRadius + 1.0f);
        TestEqual(TEXT("Z is fixed at Center.Z"), Point.Z, Center.Z);
    }

    TestTrue(TEXT("Every pair is at least MinDistance apart when the area comfortably allows it"),
        MinPairwiseDistance(Points) >= MinDistance);

    return true;
}

// ── Determinism: same seed -> identical points (needed for repeatable tests) ───

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSpawnPointsDeterministicTest,
    "WTBR.Combat.Team.Spawn.SameSeedProducesIdenticalPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSpawnPointsDeterministicTest::RunTest(const FString& /*Parameters*/)
{
    const TArray<FVector> PointsA = AWTBRGameMode::GenerateRandomSpawnPointsForTest(FVector::ZeroVector, 50000.0f, 3000.0f, 10, /*Seed=*/777);
    const TArray<FVector> PointsB = AWTBRGameMode::GenerateRandomSpawnPointsForTest(FVector::ZeroVector, 50000.0f, 3000.0f, 10, /*Seed=*/777);
    const TArray<FVector> PointsDifferentSeed = AWTBRGameMode::GenerateRandomSpawnPointsForTest(FVector::ZeroVector, 50000.0f, 3000.0f, 10, /*Seed=*/778);

    TestEqual(TEXT("Same seed: identical point count"), PointsA.Num(), PointsB.Num());
    bool bAllMatch = PointsA.Num() == PointsB.Num();
    for (int32 i = 0; bAllMatch && i < PointsA.Num(); ++i)
    {
        bAllMatch = PointsA[i].Equals(PointsB[i], 0.01f);
    }
    TestTrue(TEXT("Same seed produces identical points"), bAllMatch);

    bool bAnyDiffer = PointsA.Num() != PointsDifferentSeed.Num();
    for (int32 i = 0; !bAnyDiffer && i < PointsA.Num(); ++i)
    {
        if (!PointsA[i].Equals(PointsDifferentSeed[i], 0.01f))
        {
            bAnyDiffer = true;
        }
    }
    TestTrue(TEXT("Different seed produces a different layout"), bAnyDiffer);

    return true;
}

// ── Graceful degradation: an impossible constraint still returns Count points
//    (no hang, no crash, no fewer/more points) ──────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSpawnPointsImpossibleConstraintTest,
    "WTBR.Combat.Team.Spawn.ImpossibleSpacingStillReturnsAllPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSpawnPointsImpossibleConstraintTest::RunTest(const FString& /*Parameters*/)
{
    // A tiny 10-unit-radius disc cannot possibly fit 15 points 3000 units apart —
    // the algorithm must still terminate and return exactly 15 points (best
    // effort), not hang retrying forever or return a short array.
    const TArray<FVector> Points = AWTBRGameMode::GenerateRandomSpawnPointsForTest(
        FVector::ZeroVector, /*AreaRadius=*/10.0f, /*MinDistance=*/3000.0f, /*Count=*/15, /*Seed=*/1);

    TestEqual(TEXT("Still returns exactly Count points under an impossible constraint"), Points.Num(), 15);

    return true;
}

// ── Edge cases: zero/one point ──────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSpawnPointsEdgeCountsTest,
    "WTBR.Combat.Team.Spawn.ZeroAndOnePointEdgeCases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSpawnPointsEdgeCountsTest::RunTest(const FString& /*Parameters*/)
{
    const TArray<FVector> ZeroPoints = AWTBRGameMode::GenerateRandomSpawnPointsForTest(FVector::ZeroVector, 50000.0f, 3000.0f, 0, 1);
    TestEqual(TEXT("Count=0 returns an empty array"), ZeroPoints.Num(), 0);

    const TArray<FVector> OnePoint = AWTBRGameMode::GenerateRandomSpawnPointsForTest(FVector::ZeroVector, 50000.0f, 3000.0f, 1, 1);
    TestEqual(TEXT("Count=1 returns exactly one point"), OnePoint.Num(), 1);

    return true;
}

// ── Safe anchors: a valid subset is selected with spacing preserved ──────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSafeAnchorSelectionTest,
    "WTBR.Combat.Team.Spawn.SafeAnchors.SelectsSpacedSubset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSafeAnchorSelectionTest::RunTest(const FString& /*Parameters*/)
{
    const TArray<FVector> Anchors = MakeSafeAnchorTestSet();
    TArray<FVector> PointsA;
    TArray<FVector> PointsB;

    TestTrue(TEXT("Finds a valid 15-point safe-anchor layout"),
        AWTBRGameMode::TryGenerateSafeAnchorLayoutForTest(Anchors, 6000.0f, 15, 12345, PointsA));
    TestEqual(TEXT("Safe-anchor layout has exactly 15 points"), PointsA.Num(), 15);
    TestTrue(TEXT("Safe-anchor layout preserves MinSpawnDistance"), MinPairwiseDistance(PointsA) >= 6000.0f);

    TestTrue(TEXT("Same seed finds the same safe-anchor layout"),
        AWTBRGameMode::TryGenerateSafeAnchorLayoutForTest(Anchors, 6000.0f, 15, 12345, PointsB));
    bool bAllMatch = PointsA.Num() == PointsB.Num();
    for (int32 Index = 0; bAllMatch && Index < PointsA.Num(); ++Index)
    {
        bAllMatch = PointsA[Index].Equals(PointsB[Index], 0.01f);
    }
    TestTrue(TEXT("Safe-anchor selection is deterministic for a fixed seed"), bAllMatch);

    return true;
}

// ── Safe anchors: no NavMesh means the all-or-nothing relocation aborts ──────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSafeAnchorNavFailureIsAtomicTest,
    "WTBR.Combat.Team.Spawn.SafeAnchors.NavFailureLeavesEveryoneInPlace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSafeAnchorNavFailureIsAtomicTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSpawnWorldFixture Fixture(TEXT("WTBR_SafeAnchor_NavFailure"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameMode* GameMode = World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    if (!GameMode) return false;

    GameMode->SetSafeSpawnAnchorsForTest(MakeSafeAnchorTestSet(), 6000.0f);
    // This headless fixture intentionally has no NavMesh.  The production
    // error below is the condition being tested, rather than a test failure.
    AddExpectedError(TEXT("random spawn aborted. Safe anchor"), EAutomationExpectedErrorFlags::Contains, 1);
    TArray<AWTBRCharacter*> Characters;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AWTBRCharacter* Character = SpawnPlainCharacter(World, FVector(Index * 300.0f, 0.0f, 100.0f));
        if (!Character) return false;
        Characters.Add(Character);
    }

    GameMode->ApplyRandomSpawnPositionsForTest();
    for (int32 Index = 0; Index < Characters.Num(); ++Index)
    {
        TestTrue(TEXT("No character moved when safe anchors lack NavMesh"),
            Characters[Index]->GetActorLocation().Equals(FVector(Index * 300.0f, 0.0f, 100.0f), 0.01f));
    }

    return true;
}

// ── Wiring: ApplyRandomSpawnPositions teleports every character, all inside
//    the configured area ───────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRApplyRandomSpawnPositionsTest,
    "WTBR.Combat.Team.Spawn.ApplyTeleportsEveryCharacterInArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRApplyRandomSpawnPositionsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSpawnWorldFixture Fixture(TEXT("WTBR_Spawn_Apply"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameMode* GameMode = World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameMode) return false;

    const FVector Center(5000.0f, 5000.0f, 100.0f);
    GameMode->SetRandomSpawnParamsForTest(Center, /*AreaRadius=*/20000.0f, /*MinDistance=*/2000.0f);

    TArray<AWTBRCharacter*> Characters;
	for (int32 i = 0; i < 15; ++i)
    {
        // All spawned at the exact same starting location — a real regression
        // could silently no-op and leave everyone stacked here.
        AWTBRCharacter* Character = SpawnPlainCharacter(World, FVector::ZeroVector);
        TestNotNull(TEXT("Character spawns"), Character);
        if (!Character) return false;
        Characters.Add(Character);
    }

    GameMode->ApplyRandomSpawnPositionsForTest();

    TArray<FVector> FinalLocations;
    for (const AWTBRCharacter* Character : Characters)
    {
        FinalLocations.Add(Character->GetActorLocation());
        TestTrue(TEXT("Character relocated within the configured spawn area"),
            FVector::Dist2D(Character->GetActorLocation(), Center) <= 20000.0f + 1.0f);
    }

    // Not everyone can still be stacked at the origin after a real
    // 6-point spawn into a large area (would only coincide by near-zero chance).
    bool bAnyMoved = false;
    for (const FVector& Loc : FinalLocations)
    {
        if (!Loc.Equals(FVector::ZeroVector, 1.0f))
        {
            bAnyMoved = true;
            break;
        }
    }
	TestTrue(TEXT("Characters were actually relocated, not left at spawn"), bAnyMoved);
	TestTrue(TEXT("Final teleport locations preserve the required pairwise minimum distance"),
		MinPairwiseDistance(FinalLocations) >= 2000.0f);

	return true;
}

// ── No RandomSpawnConfig assigned: skip cleanly, don't guess defaults ──────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRandomSpawnNoConfigTest,
    "WTBR.Combat.Team.Spawn.SkippedWithoutConfigDataAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRandomSpawnNoConfigTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSpawnWorldFixture Fixture(TEXT("WTBR_Spawn_NoConfig"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameMode* GameMode = World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    TestNotNull(TEXT("GameMode spawns"), GameMode);
    if (!GameMode) return false;

    // A fresh GameMode has no RandomSpawnConfig assigned by default —
    // explicit for clarity/documentation of the seam's intent.
    GameMode->ClearRandomSpawnConfigForTest();

    AWTBRCharacter* Character = SpawnPlainCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character) return false;

    GameMode->ApplyRandomSpawnPositionsForTest();

    TestTrue(TEXT("Character left untouched at its original location when no spawn config is assigned"),
        Character->GetActorLocation().Equals(FVector::ZeroVector, 1.0f));

    return true;
}

// ── Gate: modes without bUseRandomSpawnPositions never call the relocation ─────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRandomSpawnGateTest,
    "WTBR.Combat.Team.Spawn.NotAppliedWithoutOptInFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRandomSpawnGateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSpawnWorldFixture Fixture(TEXT("WTBR_Spawn_Gate"));
    UWorld* World = Fixture.GetWorld();

    AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
    World->SetGameState(GameState);
    // Default-constructed rules: bUseRandomSpawnPositions is false, matching
    // every pre-existing fixture and the 1v1 harness.
    GameState->SetCurrentMatchRules(FWTBRMatchModeRules());
    GameState->SetCurrentMatchPhase(EWTBRMatchPhase::Countdown);

    AWTBRGameMode* GameMode = World->SpawnActor<AWTBRGameMode>(AWTBRGameMode::StaticClass());
    if (!GameState || !GameMode) return false;

    AWTBRCharacter* Character = SpawnPlainCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character) return false;

    GameMode->AdvanceToInMatchForTest();

    TestTrue(TEXT("Character stays at spawn — random spawn never applied without opt-in"),
        Character->GetActorLocation().Equals(FVector::ZeroVector, 1.0f));

    return true;
}

// ── Production defaults: 15P/BR opt in, legacy modes don't ─────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRandomSpawnDefaultsTest,
    "WTBR.Combat.Team.Spawn.FifteenPAndBRDefaultsOptIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRandomSpawnDefaultsTest::RunTest(const FString& /*Parameters*/)
{
    TestTrue(TEXT("15P default opts into random spawn"),
        AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::TeamThree15P).bUseRandomSpawnPositions);
    TestTrue(TEXT("BR default opts into random spawn"),
        AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::BattleRoyale).bUseRandomSpawnPositions);
    TestFalse(TEXT("ThreeVThree default does not (legacy PlayerStart behavior)"),
        AWTBRGameMode::MakeDefaultRulesForModeForTest(EWTBRMatchMode::ThreeVThree).bUseRandomSpawnPositions);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
