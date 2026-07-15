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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxTeammateDoesNotEnableHomingTest,
    "WTBR.Trigger.Venyx.TeammateDoesNotEnableHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxTeammateDoesNotEnableHomingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxTargetingWorldFixture Fixture(TEXT("WTBR_VenyxTeammateTargeting"));
    AWTBRCharacter* Owner = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Teammate = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Owner spawns"), Owner);
    TestNotNull(TEXT("Teammate spawns"), Teammate);
    if (!Owner || !Teammate)
    {
        return false;
    }

    Owner->SetTeamId(0);
    Teammate->SetTeamId(0);
    AWTBRProjectileBase* Projectile = FireVenyx(Fixture.GetWorld(), Owner, MakeVenyxTargetingDataAsset());
    TestNotNull(TEXT("Venyx projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement)
    {
        return false;
    }

    TestFalse(TEXT("Teammate in range, cone, and LOS does not enable homing"),
        Projectile->ProjectileMovement->bIsHomingProjectile);
    TestNull(TEXT("No homing target is assigned for teammate"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenyxEnemyEnablesHomingTest,
    "WTBR.Trigger.Venyx.EnemyEnablesHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenyxEnemyEnablesHomingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenyxTargetingWorldFixture Fixture(TEXT("WTBR_VenyxEnemyTargeting"));
    AWTBRCharacter* Owner = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnVenyxTargetingCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Owner spawns"), Owner);
    TestNotNull(TEXT("Enemy spawns"), Enemy);
    if (!Owner || !Enemy)
    {
        return false;
    }

    Owner->SetTeamId(0);
    Enemy->SetTeamId(1);
    AWTBRProjectileBase* Projectile = FireVenyx(Fixture.GetWorld(), Owner, MakeVenyxTargetingDataAsset());
    TestNotNull(TEXT("Venyx projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement)
    {
        return false;
    }

    TestTrue(TEXT("Enemy in range, cone, and LOS enables homing"),
        Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Homing targets the enemy root component"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get(), Enemy->GetRootComponent());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
