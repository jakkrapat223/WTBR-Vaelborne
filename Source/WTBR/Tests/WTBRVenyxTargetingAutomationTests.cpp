// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRVenyxTargetingWorldFixture
    {
    public:
        explicit FWTBRVenyxTargetingWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRVenyxTargetingWorldFixture()
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

    AWTBRCharacter* SpawnVenyxTargetingCharacter(UWorld* World, const FVector& Location)
    {
        if (!World)
        {
            return nullptr;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeVenyxTargetingDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->VaelCostPerUse = 10.0f;
        DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();
        DataAsset->VenyxParams.VenyxRange = 2000.0f;
        DataAsset->VenyxParams.VenyxAimConeHalfAngleDegrees = 35.0f;
        return DataAsset;
    }

    AWTBRProjectileBase* FindVenyxTargetingProjectile(UWorld* World)
    {
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                return *It;
            }
        }
        return nullptr;
    }

    AWTBRProjectileBase* FireVenyx(UWorld* World, AWTBRCharacter* Owner, UWTBRTriggerDataAsset* DataAsset)
    {
        if (!World || !Owner || !Owner->VaelComponent || !DataAsset)
        {
            return nullptr;
        }

        Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
        Venyx->InitializeTrigger(Owner, DataAsset);
        return Venyx->Activate(FInputActionValue(), false)
            ? FindVenyxTargetingProjectile(World)
            : nullptr;
    }
}

// -----------------------------------------------------------------------------
// Venyx targeting moved from FIRE TIME to IN FLIGHT (2026-07-22).
//
// Tap used to pick a target out of the caster's aim cone before the shot left, so
// every bolt bent away from the crosshair immediately and missing was not really
// possible. It now flies straight and acquires only what it passes near.
//
// These tests therefore exercise the rule where it actually lives now —
// FindProximityHomingTarget — rather than the projectile's state at spawn. The
// friendly-fire and liveness exclusions still matter just as much; they simply moved.
// Whether a tap ARMS the sweep at all is covered by
// WTBR.Gunner.CubeSplit.VenyxTapArmsAProximitySweepRatherThanLockingAtFireTime.
// -----------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepIgnoresTeammatesTest,
    "WTBR.Trigger.Venyx.SweepIgnoresTeammatesAndTheCasterItself",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepIgnoresTeammatesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxTargetingWorldFixture Fixture(TEXT("WTBR_VenyxSweepTeammate"));
    AWTBRCharacter* Owner = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Teammate = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(300.0f, 0.0f, 0.0f));
    if (!Owner || !Teammate) return false;

    Owner->SetTeamId(0);
    Teammate->SetTeamId(0);

    // A cube sitting right on top of both of them. Only the team rule can keep it
    // from acquiring, which is the point.
    const FVector CubeLocation(300.0f, 0.0f, 0.0f);
    const TArray<AWTBRCharacter*> Candidates = { Owner, Teammate };

    TestNull(TEXT("A teammate inside the radius is never acquired"),
        AWTBRProjectileBase::FindProximityHomingTarget(
            Owner, CubeLocation, /*RadiusUU=*/1000.0f, Candidates));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepAcquiresOnlyWithinItsRadiusTest,
    "WTBR.Trigger.Venyx.SweepAcquiresAnEnemyOnlyOnceItIsInsideTheRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepAcquiresOnlyWithinItsRadiusTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxTargetingWorldFixture Fixture(TEXT("WTBR_VenyxSweepRadius"));
    AWTBRCharacter* Owner = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(2000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;

    Owner->SetTeamId(0);
    Enemy->SetTeamId(1);

    const TArray<AWTBRCharacter*> Candidates = { Owner, Enemy };

    // Cube still well short of the enemy: this is the "flies past nobody, behaves
    // like an ordinary bullet" case the whole design rests on.
    TestNull(TEXT("An enemy outside the radius is not acquired"),
        AWTBRProjectileBase::FindProximityHomingTarget(
            Owner, FVector(500.0f, 0.0f, 0.0f), /*RadiusUU=*/500.0f, Candidates));

    // Same shot, further along its flight — now it hooks.
    TestEqual(TEXT("The same enemy is acquired once the cube is close enough"),
        AWTBRProjectileBase::FindProximityHomingTarget(
            Owner, FVector(1700.0f, 0.0f, 0.0f), /*RadiusUU=*/500.0f, Candidates),
        Enemy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxSweepPrefersNearestTest,
    "WTBR.Trigger.Venyx.SweepTakesTheNearestEnemyNotTheFirstFound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxSweepPrefersNearestTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxTargetingWorldFixture Fixture(TEXT("WTBR_VenyxSweepNearest"));
    AWTBRCharacter* Owner = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Near = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(1100.0f, 0.0f, 0.0f));
    AWTBRCharacter* Far = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(1400.0f, 0.0f, 0.0f));
    if (!Owner || !Near || !Far) return false;

    Owner->SetTeamId(0);
    Near->SetTeamId(1);
    Far->SetTeamId(1);

    // Deliberately listed far-first. Without a distance comparison the result would
    // depend on actor iteration order, and two cubes sweeping the same ground would
    // disagree about who they were chasing.
    const TArray<AWTBRCharacter*> Candidates = { Owner, Far, Near };

    TestEqual(TEXT("Nearest enemy wins regardless of iteration order"),
        AWTBRProjectileBase::FindProximityHomingTarget(
            Owner, FVector(1000.0f, 0.0f, 0.0f), /*RadiusUU=*/1000.0f, Candidates),
        Near);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
