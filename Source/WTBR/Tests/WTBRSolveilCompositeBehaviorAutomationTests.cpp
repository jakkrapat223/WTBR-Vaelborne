// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InterpToMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRSolveilCompositeBehavior.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRSolveilCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRSolveilCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRSolveilCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnSolveilCompositeBehaviorCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindSolveilCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) return *It;
        }
        return nullptr;
    }

    int32 CountSolveilCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeSolveilCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Solveil;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 80.0f;
        Definition.ProjectileSpeed = 2800.0f;
        Definition.bCanPenetrate = true;
        Definition.PathRange = 1500.0f;

        FWTBRPathLane& Lane = Definition.PathPreset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = {
            FVector(0.0f, 0.0f, 0.0f),
            FVector(0.5f, 0.3f, 0.0f),
            FVector(1.0f, 0.0f, 0.0f),
        };
        Lane.CubeCount = 1;
        // Pinned so these tests stay about path movement and damage, not volley size.
        // Definition.CubeCount now drives BOTH tap and hold and defaults to 5, which
        // would otherwise split the budget across 5 cubes and change what "full
        // damage budget" means here.
        Definition.CubeCount = 1;
        Lane.FormationOffset = FVector::ZeroVector;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeSolveilCompositeBehaviorDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeSolveilCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition = MakeSolveilCompositeBehaviorDefinition();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Serpveil;
        Definition.BehaviorClass = UWTBRSolveilCompositeBehavior::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeSolveilCompositeBehaviorTriggerSet(
        FWTBRSolveilCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnSolveilCompositeBehaviorCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartSolveilCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset =
            MakeSolveilCompositeBehaviorDataAsset(EWTBRBulletArchetype::Solux);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeSolveilCompositeBehaviorDataAsset(EWTBRBulletArchetype::Serpveil);
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolveilCompositeBehaviorPathMovementTest,
    "WTBR.Composite.Behavior.Solveil.PathMovementEngagesWithoutStraightLaunch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolveilCompositeBehaviorPathMovementTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolveilCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solveil_PathMovement"));
    AWTBRCharacter* Owner = SpawnSolveilCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeSolveilCompositeBehaviorDefinition();
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestTrue(TEXT("Solveil behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindSolveilCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Solveil projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("Path movement is active"), Projectile->InterpMovement->IsActive());
    TestFalse(TEXT("Straight projectile movement stays inactive"), Projectile->ProjectileMovement->IsActive());
    TestTrue(TEXT("Solveil projectile can penetrate"), Projectile->bCanPenetrate);
    TestEqual(TEXT("Solveil retains the full damage budget"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolveilCompositeBehaviorNoPathPresetTest,
    "WTBR.Composite.Behavior.Solveil.NoPathPresetFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolveilCompositeBehaviorNoPathPresetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolveilCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solveil_NoPathPreset"));
    AWTBRCharacter* Owner = SpawnSolveilCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeSolveilCompositeBehaviorDefinition();
    Definition.PathPreset.Lanes.Reset();
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestFalse(TEXT("Missing path preset returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Missing path preset spawns nothing"),
        CountSolveilCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolveilCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Solveil.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolveilCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolveilCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solveil_NullClass"));
    AWTBRCharacter* Owner = SpawnSolveilCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeSolveilCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountSolveilCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolveilCompositeBehaviorMultiWaypointPathTest,
    "WTBR.Composite.Behavior.Solveil.MultiWaypointPathIsHonored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolveilCompositeBehaviorMultiWaypointPathTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolveilCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solveil_MultiWaypoint"));
    AWTBRCharacter* Owner = SpawnSolveilCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeSolveilCompositeBehaviorDefinition();
    UWTBRSolveilCompositeBehavior* Behavior = NewObject<UWTBRSolveilCompositeBehavior>(Owner);
    TestTrue(TEXT("Solveil behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindSolveilCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Solveil projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("Multi-waypoint path computes a positive duration"),
        Projectile->InterpMovement->Duration > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolveilCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Solveil.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolveilCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolveilCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solveil_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSolveilCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeSolveilCompositeBehaviorRegistry();
    StartSolveilCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Solveil becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Solveil fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindSolveilCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("BehaviorClass path engages path movement"), Projectile->InterpMovement->IsActive());
    TestTrue(TEXT("BehaviorClass path preserves penetration"), Projectile->bCanPenetrate);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
