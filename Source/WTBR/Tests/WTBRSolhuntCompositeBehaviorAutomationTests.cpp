// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRHomingExplosiveCompositeBehavior.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRSolhuntCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRSolhuntCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRSolhuntCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnSolhuntCompositeBehaviorCharacter(UWorld* World, const FVector& Location)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindSolhuntCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) return *It;
        }
        return nullptr;
    }

    int32 CountSolhuntCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeSolhuntCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Solhunt;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 100.0f;
        Definition.ProjectileSpeed = 3800.0f; // ⚠ PLAYTEST PENDING: faster than base Solux (3000)
        Definition.ExplosionParams.bExplodes = false;
        Definition.HomingParams.bEnableHoming = true;
        Definition.HomingParams.TargetAcquisitionRange = 3000.0f;
        Definition.HomingParams.AimConeHalfAngleDegrees = 35.0f;
        Definition.HomingParams.HomingAcceleration = 24000.0f; // ⚠ PLAYTEST PENDING: slightly better than Venyx (22000)
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeSolhuntCompositeBehaviorDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeSolhuntCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
        Definition.CompositeType = EWTBRCompositeBulletType::Solhunt;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = UWTBRHomingExplosiveCompositeBehavior::StaticClass();
        Definition.TotalDamageBudget = 100.0f;
        Definition.ProjectileSpeed = 3800.0f;
        Definition.ExplosionParams.bExplodes = false;
        Definition.HomingParams.bEnableHoming = true;
        Definition.HomingParams.TargetAcquisitionRange = 3000.0f;
        Definition.HomingParams.AimConeHalfAngleDegrees = 35.0f;
        Definition.HomingParams.HomingAcceleration = 24000.0f;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeSolhuntCompositeBehaviorTriggerSet(
        FWTBRSolhuntCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnSolhuntCompositeBehaviorCharacter(
            Fixture.GetWorld(), FVector::ZeroVector);
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartSolhuntCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset =
            MakeSolhuntCompositeBehaviorDataAsset(EWTBRBulletArchetype::Solux);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeSolhuntCompositeBehaviorDataAsset(EWTBRBulletArchetype::Venyx);
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolhuntCompositeBehaviorNonExplodingHomingDispatchTest,
    "WTBR.Composite.Behavior.Solhunt.NonExplodingHomingDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolhuntCompositeBehaviorNonExplodingHomingDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolhuntCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solhunt_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSolhuntCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Owner = Cast<AWTBRCharacter>(TriggerSet->GetOwner());
    AWTBRCharacter* Enemy = SpawnSolhuntCompositeBehaviorCharacter(
        Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;
    Owner->SetTeamId(0);
    Enemy->SetTeamId(1);

    UWTBRCompositeRegistryDataAsset* Registry = MakeSolhuntCompositeBehaviorRegistry();
    StartSolhuntCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Solhunt becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Solhunt dispatches through the shared behavior"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindSolhuntCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Solhunt projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    const FWTBRCompositeDefinition& Definition = Registry->Definitions[0];
    TestFalse(TEXT("Solhunt does not explode on impact"), Projectile->bExplodeOnImpact);
    TestTrue(TEXT("Solhunt enables homing"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Solhunt targets the valid enemy root"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get(), Enemy->GetRootComponent());
    TestEqual(TEXT("Solhunt uses definition homing acceleration"),
        Projectile->ProjectileMovement->HomingAccelerationMagnitude, Definition.HomingParams.HomingAcceleration);
    TestEqual(TEXT("Solhunt retains full damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    TestEqual(TEXT("Solhunt uses definition projectile speed"),
        Projectile->ProjectileMovement->InitialSpeed, Definition.ProjectileSpeed);
    TestFalse(TEXT("Solhunt remains single-target and non-penetrating"), Projectile->bCanPenetrate);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolhuntCompositeBehaviorNoTargetFallbackTest,
    "WTBR.Composite.Behavior.Solhunt.NoTargetFiresStraight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolhuntCompositeBehaviorNoTargetFallbackTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolhuntCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solhunt_NoTarget"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSolhuntCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeSolhuntCompositeBehaviorRegistry();
    StartSolhuntCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Ready Solhunt fires without a target"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindSolhuntCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("No-target Solhunt projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestFalse(TEXT("No target leaves homing disabled"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestFalse(TEXT("No target remains non-exploding"), Projectile->bExplodeOnImpact);
    TestEqual(TEXT("No target retains full damage"),
        Projectile->BaseDamage, Registry->Definitions[0].TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolhuntCompositeBehaviorFulgvynReuseRegressionTest,
    "WTBR.Composite.Behavior.Solhunt.FulgvynExplosionReuseRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolhuntCompositeBehaviorFulgvynReuseRegressionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolhuntCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solhunt_FulgvynReuse"));
    AWTBRCharacter* Owner = SpawnSolhuntCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition FulgvynDefinition = MakeSolhuntCompositeBehaviorDefinition();
    FulgvynDefinition.CompositeType = EWTBRCompositeBulletType::Fulgvyn;
    FulgvynDefinition.ExplosionParams.bExplodes = true;

    UWTBRHomingExplosiveCompositeBehavior* Behavior =
        NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestTrue(TEXT("Shared behavior executes for a Fulgvyn-shaped definition"),
        Behavior->ExecuteComposite(Owner, FulgvynDefinition));

    AWTBRProjectileBase* Projectile = FindSolhuntCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Fulgvyn-shaped projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("Fulgvyn-shaped definition remains explosive"), Projectile->bExplodeOnImpact);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
