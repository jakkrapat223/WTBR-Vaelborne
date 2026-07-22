// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRSoluxTrigger.h"
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRCharacter.h"

// -----------------------------------------------------------------------------
// Gunner cube split (WTBR.Gunner.CubeSplit.*)
//
// All three energy archetypes tap through one shared formation — conjure a block,
// split it into N cubes, fire them converging on the aim point. What must NOT be
// shared is the per-cube payload: Solux carries nothing, Fulgrix explodes, Venyx
// homes. These tests exist to keep the formation shared and the payload separate,
// because the tempting refactor is to flatten all three into one function.
// -----------------------------------------------------------------------------

namespace
{
    struct FWTBRCubeSplitFixture
    {
        explicit FWTBRCubeSplitFixture(const TCHAR* Name)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(Name));
            if (GEngine && World)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }
        ~FWTBRCubeSplitFixture()
        {
            if (GEngine && World) GEngine->DestroyWorldContext(World);
            if (World) World->DestroyWorld(false);
        }
        UWorld* World = nullptr;
    };

    AWTBRCharacter* WTBRCubeSplitSpawnOwner(UWorld* World)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Owner = World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        if (Owner && Owner->VaelComponent)
        {
            Owner->VaelComponent->DebugSetCurrentVaelDirect(500.0f);
        }
        return Owner;
    }

    int32 WTBRCubeSplitCount(UWorld* World)
    {
        int32 Count = 0;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It)) ++Count;
        }
        return Count;
    }

    // Distinct spawn positions are the whole reason the scatter sphere exists:
    // cubes conjured on one point overlap and destroy each other on contact before
    // InitializePathMovement ever runs. This bit us on composites already.
    int32 WTBRCubeSplitDistinctOrigins(UWorld* World)
    {
        TArray<FVector> Seen;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (!IsValid(*It)) continue;
            const FVector Loc = It->GetActorLocation();
            const bool bDuplicate = Seen.ContainsByPredicate(
                [&Loc](const FVector& V) { return V.Equals(Loc, 1.0f); });
            if (!bDuplicate) Seen.Add(Loc);
        }
        return Seen.Num();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGunnerSplitDamageIsDividedTest,
    "WTBR.Gunner.CubeSplit.TotalDamageIsSplitAcrossTheVolleyNotMultiplied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGunnerSplitDamageIsDividedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCubeSplitFixture Fixture(TEXT("WTBRCubeSplitDamage"));
    UWorld* World = Fixture.World;
    if (!World) return false;

    AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
    if (!Owner) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
    DA->VaelCostPerUse = 10.0f;
    DA->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();
    DA->SoluxParams.SoluxTotalDamage = 100.0f;
    DA->SoluxParams.SoluxTapCubeCount = 8;

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
    Solux->InitializeTrigger(Owner, DA);
    Solux->Activate(FInputActionValue(), false);

    const int32 Count = WTBRCubeSplitCount(World);
    TestEqual(TEXT("Eight cubes spawned"), Count, 8);

    float Total = 0.0f;
    for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
    {
        if (IsValid(*It)) Total += It->BaseDamage;
    }
    // The volley must be worth exactly one shot. Splitting is a formation change,
    // not a damage buff — getting this backwards turns every archetype into an 8x.
    TestTrue(FString::Printf(TEXT("Volley totals the authored damage (%.1f)"), Total),
        FMath::IsNearlyEqual(Total, 100.0f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGunnerSplitOriginsAreDistinctTest,
    "WTBR.Gunner.CubeSplit.CubesAreConjuredOnDistinctPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGunnerSplitOriginsAreDistinctTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCubeSplitFixture Fixture(TEXT("WTBRCubeSplitOrigins"));
    UWorld* World = Fixture.World;
    if (!World) return false;

    AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
    if (!Owner) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
    DA->VaelCostPerUse = 10.0f;
    DA->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();
    DA->SoluxParams.SoluxTapCubeCount = 8;
    DA->SoluxParams.SoluxTapScatterRadius = 135.0f;

    UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
    Solux->InitializeTrigger(Owner, DA);
    Solux->Activate(FInputActionValue(), false);

    TestEqual(TEXT("Every cube has its own spawn point"),
        WTBRCubeSplitDistinctOrigins(World), 8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGunnerSplitVolleyHoldsFormationTest,
    "WTBR.Gunner.CubeSplit.VolleyFliesInFormationInsteadOfCrossingThroughItself",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGunnerSplitVolleyHoldsFormationTest::RunTest(const FString& /*Parameters*/)
{
    // Owner-found in PIE: an earlier version sent each cube to the OPPOSITE side of
    // the pattern, so the volley pinched into a waist mid-flight and opened back
    // out. It read as a shot that had drifted rather than one that was aimed.
    //
    // A cube launched on the upper left must still be heading upper-left. Checked as
    // a sign agreement between where the cube sits relative to the muzzle and where
    // its velocity is taking it, which needs no physics scene.
    FWTBRCubeSplitFixture Fixture(TEXT("WTBRCubeSplitFormation"));
    UWorld* World = Fixture.World;
    if (!World) return false;

    AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
    if (!Owner) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
    DA->VaelCostPerUse = 10.0f;
    DA->FulgrixParams.FulgrixProjectileClass = AWTBRProjectileBase::StaticClass();
    DA->FulgrixParams.FulgrixTapCubeCount = 8;
    DA->FulgrixParams.FulgrixTapScatterRadius = 135.0f;
    // Wider at the far end than at the muzzle: the cluster must OPEN, never pinch.
    DA->FulgrixParams.FulgrixTapImpactSpread = 400.0f;

    UWTBRFulgrixTrigger* Fulgrix = NewObject<UWTBRFulgrixTrigger>(Owner);
    Fulgrix->InitializeTrigger(Owner, DA);
    Fulgrix->Activate(FInputActionValue(), false);

    const FVector Centre = Owner->GetActorLocation();
    int32 Checked = 0;
    int32 Crossing = 0;

    for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
    {
        AWTBRProjectileBase* Cube = *It;
        if (!IsValid(Cube)) continue;

        // Offset from the volley's axis, and the heading, both flattened to the
        // plane across the shot so the forward component does not drown them out.
        const FVector Offset = Cube->GetActorLocation() - Centre;
        const FVector Heading = Cube->GetActorForwardVector();

        const float OffsetSide = Offset.Y;
        const float HeadingSide = Heading.Y;
        if (FMath::Abs(OffsetSide) < 1.0f) continue;

        ++Checked;
        // Opposite signs mean the cube is travelling back across the volley's centre
        // line — the crossing this test exists to prevent.
        if (OffsetSide * HeadingSide < 0.0f) ++Crossing;
    }

    TestTrue(TEXT("Some cubes were off-axis enough to judge"), Checked > 0);
    TestEqual(TEXT("No cube crosses the volley's centre line"), Crossing, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRArcingPresetKeepsFlyingPastItsPathTest,
    "WTBR.Gunner.CubeSplit.AnArcingCubeKeepsFlyingPastTheEndOfItsAuthoredPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRArcingPresetKeepsFlyingPastItsPathTest::RunTest(const FString& /*Parameters*/)
{
    // Owner-found in PIE on Solux's Hammer: the cubes climbed, turned over, and
    // vanished mid-dive at about chest height having touched nothing. The authored
    // path had simply run out in open air and the cube was destroyed for it.
    //
    // Owner rule: a bullet stops for THREE reasons only — it hits somebody, it hits
    // the world, or it runs out of range. "The designer's curve ended" is not one.
    FWTBRCubeSplitFixture Fixture(TEXT("WTBRArcContinues"));
    UWorld* World = Fixture.World;
    if (!World) return false;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRProjectileBase* Cube = World->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Cube) return false;

    Cube->InitializeProjectile(10.0f, 2000.0f, ETriggerCategory::Gunner, false, false, 0.0f);
    Cube->MaxRange = 8000.0f;

    // An arc that climbs and comes back down, ending well inside the range budget.
    Cube->InitializePathMovement(
        {FVector::ZeroVector, FVector(600.0f, 0.0f, 1200.0f), FVector(1500.0f, 0.0f, 0.0f)},
        2000.0f, nullptr);

    Cube->OnInterpMovementEndForTest();
    TestTrue(TEXT("A cube with range left survives the end of its path"), IsValid(Cube));
    if (!IsValid(Cube)) return false;

    // And it must leave along the FINAL LEG's heading, which points down. Reading
    // the actor's own facing would give the spawn direction instead: a path-driven
    // projectile is translated, never rotated, so it still faces where it started.
    if (IsValid(Cube->ProjectileMovement))
    {
        TestTrue(TEXT("It continues along the arc's descending heading, not its spawn facing"),
            Cube->ProjectileMovement->Velocity.Z < 0.0f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxTapSweepsInsteadOfLockingTest,
    "WTBR.Gunner.CubeSplit.VenyxTapArmsAProximitySweepRatherThanLockingAtFireTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxTapSweepsInsteadOfLockingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCubeSplitFixture Fixture(TEXT("WTBRVenyxTapSweep"));
    UWorld* World = Fixture.World;
    if (!World) return false;

    AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
    if (!Owner) return false;

    UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
    DA->VaelCostPerUse = 10.0f;
    DA->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();
    DA->VenyxParams.VenyxTapCubeCount = 8;
    DA->VenyxParams.VenyxTapHomingRadius = 500.0f;

    UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
    Venyx->InitializeTrigger(Owner, DA);
    Venyx->Activate(FInputActionValue(), false);

    int32 Armed = 0;
    int32 Ticking = 0;
    int32 AlreadyChasing = 0;
    for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
    {
        AWTBRProjectileBase* Cube = *It;
        if (!IsValid(Cube)) continue;
        if (Cube->ProximityHomingRadius > 0.0f) ++Armed;
        // A green suite says nothing about code that only runs from a tick, so the
        // WIRING is asserted, not just the rule. Arming used to be a silent no-op
        // because bCanEverTick was false at registration time.
        if (Cube->IsActorTickEnabled()) ++Ticking;
        if (IsValid(Cube->ProjectileMovement)
            && Cube->ProjectileMovement->bIsHomingProjectile) ++AlreadyChasing;
    }

    TestEqual(TEXT("Every tap cube is armed to sweep"), Armed, 8);
    TestEqual(TEXT("Every armed cube is actually ticking"), Ticking, 8);
    // The whole point of the change: a tap leaves the muzzle committed to nothing.
    TestEqual(TEXT("No cube has a target at fire time"), AlreadyChasing, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGunnerSplitPayloadsStayArchetypeSpecificTest,
    "WTBR.Gunner.CubeSplit.SharedFormationDoesNotFlattenPerCubePayloads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGunnerSplitPayloadsStayArchetypeSpecificTest::RunTest(const FString& /*Parameters*/)
{
    // Fulgrix's cubes must each explode and Solux's must not. Only the formation is
    // shared — the moment that stops being true, Meteor and Asteroid are the same
    // weapon with different numbers.
    {
        FWTBRCubeSplitFixture Fixture(TEXT("WTBRCubeSplitFulgrix"));
        UWorld* World = Fixture.World;
        if (!World) return false;
        AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
        if (!Owner) return false;

        UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
        DA->VaelCostPerUse = 10.0f;
        DA->FulgrixParams.FulgrixProjectileClass = AWTBRProjectileBase::StaticClass();
        DA->FulgrixParams.FulgrixTapCubeCount = 8;
        DA->FulgrixParams.FulgrixTapExplosionRadius = 110.0f;

        UWTBRFulgrixTrigger* Fulgrix = NewObject<UWTBRFulgrixTrigger>(Owner);
        Fulgrix->InitializeTrigger(Owner, DA);
        Fulgrix->Activate(FInputActionValue(), false);

        int32 Exploding = 0;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && It->bExplodeOnImpact) ++Exploding;
        }
        TestEqual(TEXT("All eight Fulgrix cubes explode"), Exploding, 8);
    }

    {
        FWTBRCubeSplitFixture Fixture(TEXT("WTBRCubeSplitSolux"));
        UWorld* World = Fixture.World;
        if (!World) return false;
        AWTBRCharacter* Owner = WTBRCubeSplitSpawnOwner(World);
        if (!Owner) return false;

        UWTBRTriggerDataAsset* DA = NewObject<UWTBRTriggerDataAsset>(Owner);
        DA->VaelCostPerUse = 10.0f;
        DA->SoluxParams.SoluxProjectileClass = AWTBRProjectileBase::StaticClass();
        DA->SoluxParams.SoluxTapCubeCount = 8;

        UWTBRSoluxTrigger* Solux = NewObject<UWTBRSoluxTrigger>(Owner);
        Solux->InitializeTrigger(Owner, DA);
        Solux->Activate(FInputActionValue(), false);

        int32 Exploding = 0;
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && It->bExplodeOnImpact) ++Exploding;
        }
        TestEqual(TEXT("No Solux cube explodes"), Exploding, 0);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
