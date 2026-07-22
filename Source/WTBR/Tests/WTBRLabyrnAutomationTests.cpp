// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/InterpToMovementComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRLabyrnCompositeBehavior.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRCharacter.h"

// -----------------------------------------------------------------------------
// Labyrn tests (WTBR.Composite.Labyrn.*)
//
// Viper + Viper. Range and turn budget come from data and from the shared resolver;
// what this behaviour itself owns is the speed profile — fast for the first four
// fifths of each lane, easing off over the last one.
//
// The turn budget is covered here too rather than with the resolver tests, because
// the RULE (four per Viper, so Labyrn gets eight) is a Labyrn design decision even
// though the code enforcing it is shared.
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLabyrnTurnBudgetTest,
    "WTBR.Composite.Labyrn.TurnBudgetIsFourPerViperInTheComposite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLabyrnTurnBudgetTest::RunTest(const FString& /*Parameters*/)
{
    // Labyrn = Viper + Viper. Eight is the entire mechanical payoff of the merge,
    // so it is worth asserting rather than trusting the arithmetic to stay put.
    FWTBRCompositeDefinition Labyrn;
    Labyrn.RequiredArchetypeA = EWTBRBulletArchetype::Serpveil;
    Labyrn.RequiredArchetypeB = EWTBRBulletArchetype::Serpveil;
    TestEqual(TEXT("Labyrn gets eight turns"),
        UWTBRCompositeRegistryDataAsset::ComputeTurnBudget(Labyrn), 8);

    // One Viper in the recipe means the player keeps exactly the budget they had
    // before merging — merging into a non-Labyrn composite must not cost them turns.
    FWTBRCompositeDefinition Coilvyn;
    Coilvyn.RequiredArchetypeA = EWTBRBulletArchetype::Serpveil;
    Coilvyn.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
    TestEqual(TEXT("A one-Viper composite gets four turns"),
        UWTBRCompositeRegistryDataAsset::ComputeTurnBudget(Coilvyn), WTBR_TURNS_PER_VIPER);

    // Zero means uncapped. A Hound/Meteo composite's preset is not a Viper preset
    // and the cap has nothing to say about it — capping it at 0 turns would flatten
    // every authored sweep into a straight line.
    FWTBRCompositeDefinition Fulgvyn;
    Fulgvyn.RequiredArchetypeA = EWTBRBulletArchetype::Fulgrix;
    Fulgvyn.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
    TestEqual(TEXT("A composite with no Viper is left uncapped"),
        UWTBRCompositeRegistryDataAsset::ComputeTurnBudget(Fulgvyn), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeSplitRuleTest,
    "WTBR.Composite.SplitRule.AsteroidAndMeteorMergesStayOneMassUnlessAViperWentIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeSplitRuleTest::RunTest(const FString& /*Parameters*/)
{
    auto Pair = [](EWTBRBulletArchetype A, EWTBRBulletArchetype B)
    {
        FWTBRCompositeDefinition Definition;
        Definition.RequiredArchetypeA = A;
        Definition.RequiredArchetypeB = B;
        return Definition;
    };

    using EA = EWTBRBulletArchetype;

    // Mass merges: one projectile. Asteroid and Meteor merge into a single body.
    TestTrue(TEXT("Solgrix (Solux + Fulgrix) fires one"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Solux, EA::Fulgrix)));
    TestTrue(TEXT("Dualux (Solux + Solux) fires one"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Solux, EA::Solux)));
    TestTrue(TEXT("Solhunt (Solux + Venyx) fires one"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Solux, EA::Venyx)));
    TestTrue(TEXT("Fulgvyn (Fulgrix + Venyx) fires one"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Fulgrix, EA::Venyx)));
    TestTrue(TEXT("Catarix (Fulgrix + Fulgrix) fires one"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Fulgrix, EA::Fulgrix)));

    // The canon exception: Viper is the archetype that splits, and the anime shows a
    // Meteor+Viper merge dividing into a grid before it flies. A Viper in the recipe
    // therefore overrides the mass rule.
    TestFalse(TEXT("Solveil (Solux + Viper) still splits"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Solux, EA::Serpveil)));
    TestFalse(TEXT("Ignivex (Fulgrix + Viper) still splits"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Fulgrix, EA::Serpveil)));

    // Order must not matter: the archetypes are authored per definition and nothing
    // guarantees which slot a given one lands in.
    TestFalse(TEXT("Viper first reads the same as Viper second"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Serpveil, EA::Fulgrix)));

    // No mass component at all: unaffected by this rule.
    TestFalse(TEXT("Venspire (Venyx + Venyx) splits"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Venyx, EA::Venyx)));
    TestFalse(TEXT("Coilvyn (Viper + Venyx) splits"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Serpveil, EA::Venyx)));
    TestFalse(TEXT("Labyrn (Viper + Viper) splits"),
        UWTBRCompositeRegistryDataAsset::FiresSingleProjectile(Pair(EA::Serpveil, EA::Serpveil)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLabyrnTurnClampTest,
    "WTBR.Composite.Labyrn.ClampingTurnsKeepsTheLaneEndpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLabyrnTurnClampTest::RunTest(const FString& /*Parameters*/)
{
    // Start + six turns + end.
    TArray<FVector> Authored;
    for (int32 i = 0; i <= 7; ++i) Authored.Add(FVector(i * 100.0f, i * 10.0f, 0.0f));

    TArray<FVector> Clamped;
    UWTBRCompositeRegistryDataAsset::ClampLaneTurns(Authored, WTBR_TURNS_PER_VIPER, Clamped);

    TestEqual(TEXT("Four turns leaves six waypoints"), Clamped.Num(), WTBR_TURNS_PER_VIPER + 2);

    // The endpoint is load-bearing: it is where the lane LANDS, so dropping it would
    // silently shorten the shot's reach. The player loses corners, never range.
    if (Clamped.Num() > 0)
    {
        TestEqual(TEXT("The lane still lands where it was authored to"),
            Clamped.Last(), Authored.Last());
        TestEqual(TEXT("The lane still starts where it was authored to"),
            Clamped[0], Authored[0]);
    }

    // Labyrn's eight is enough for this lane, so it must come through untouched —
    // this is the difference the merge buys, and it has to be visible.
    TArray<FVector> AsLabyrn;
    UWTBRCompositeRegistryDataAsset::ClampLaneTurns(Authored, 8, AsLabyrn);
    TestEqual(TEXT("Labyrn's budget flies the whole authored lane"),
        AsLabyrn.Num(), Authored.Num());

    // Zero is the uncapped path every non-Viper family takes.
    TArray<FVector> Uncapped;
    UWTBRCompositeRegistryDataAsset::ClampLaneTurns(Authored, 0, Uncapped);
    TestEqual(TEXT("Zero leaves the lane alone"), Uncapped.Num(), Authored.Num());

    // A lane already inside budget must be bit-identical, or every preset authored
    // before the cap existed would quietly change shape.
    TArray<FVector> Short = { FVector::ZeroVector, FVector(500.0f, 0.0f, 0.0f) };
    TArray<FVector> ShortClamped;
    UWTBRCompositeRegistryDataAsset::ClampLaneTurns(Short, WTBR_TURNS_PER_VIPER, ShortClamped);
    TestEqual(TEXT("A straight lane is untouched"), ShortClamped.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLabyrnEasesOffAtTheEndTest,
    "WTBR.Composite.Labyrn.CubeEasesOffOverItsClosingStretch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLabyrnEasesOffAtTheEndTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WTBRLabyrnEase"));
    TestNotNull(TEXT("Fixture world exists"), World);
    if (!World) return false;
    if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Cube = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

    bool bResult = false;
    if (Cube)
    {
        Cube->InitializeProjectile(10.0f, 2000.0f, ETriggerCategory::Gunner, false, false, 0.0f);
        Cube->SetPathSpeedProfile(0.80f, 0.35f);

        // Evenly spaced points, so any difference in per-segment TIME is the profile
        // rather than the geometry.
        TArray<FVector> Path;
        for (int32 i = 0; i <= 20; ++i) Path.Add(FVector(i * 100.0f, 0.0f, 0.0f));
        Cube->InitializePathMovement(Path, 2000.0f, nullptr);

        const TArray<FInterpControlPoint>& Points = Cube->InterpMovement->ControlPoints;
        bResult = Points.Num() == Path.Num();
        TestEqual(TEXT("Every waypoint became a control point"), Points.Num(), Path.Num());

        if (bResult)
        {
            // Equal distances, so a flat speed would give every segment an identical
            // time slice. The closing segment must take visibly longer than an early
            // one, or the cube is still arriving too fast to answer.
            const float EarlyShare = Points[1].Percentage;
            const float LateShare = Points[Points.Num() - 2].Percentage;
            TestTrue(
                FString::Printf(TEXT("The closing segment takes longer than an early one (%.4f vs %.4f)"),
                    LateShare, EarlyShare),
                LateShare > EarlyShare * 1.5f);

            // Time shares must still sum to one. The profile reshapes HOW the flight
            // time is spent and must never change how much of it there is — that is
            // what makes Definition.ProjectileSpeed readable as an average.
            float Sum = 0.0f;
            for (int32 i = 0; i < Points.Num() - 1; ++i) Sum += Points[i].Percentage;
            TestTrue(FString::Printf(TEXT("Time shares still total one (%.4f)"), Sum),
                FMath::IsNearlyEqual(Sum, 1.0f, 0.01f));
        }
    }

    if (GEngine) GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRLabyrnProfileOptOutTest,
    "WTBR.Composite.Labyrn.NoProfileLeavesEveryOtherProjectileFlat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRLabyrnProfileOptOutTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WTBRLabyrnFlat"));
    if (!World) return false;
    if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Cube = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

    if (Cube)
    {
        // The profile is opt-in. Every Viper, Venyx and Meteo path in the game runs
        // through this same function, and none of them asked to decelerate.
        Cube->InitializeProjectile(10.0f, 2000.0f, ETriggerCategory::Gunner, false, false, 0.0f);
        TArray<FVector> Path;
        for (int32 i = 0; i <= 10; ++i) Path.Add(FVector(i * 100.0f, 0.0f, 0.0f));
        Cube->InitializePathMovement(Path, 2000.0f, nullptr);

        const TArray<FInterpControlPoint>& Points = Cube->InterpMovement->ControlPoints;
        if (Points.Num() >= 3)
        {
            TestTrue(TEXT("Equal segments keep equal time when no profile is set"),
                FMath::IsNearlyEqual(Points[1].Percentage, Points[Points.Num()-2].Percentage, 0.001f));
        }
    }

    if (GEngine) GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
