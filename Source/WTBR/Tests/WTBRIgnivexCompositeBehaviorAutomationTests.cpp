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
#include "Trigger/WTBRIgnivexCompositeBehavior.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRIgnivexCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRIgnivexCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRIgnivexCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnIgnivexCompositeBehaviorCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindIgnivexCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) return *It;
        }
        return nullptr;
    }

    int32 CountIgnivexCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeIgnivexCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Ignivex;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 80.0f;
        Definition.ProjectileSpeed = 2800.0f;
        Definition.PathRange = 1500.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 250.0f;

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

    UWTBRTriggerDataAsset* MakeIgnivexCompositeBehaviorDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeIgnivexCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition = MakeIgnivexCompositeBehaviorDefinition();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Fulgrix;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Serpveil;
        Definition.BehaviorClass = UWTBRIgnivexCompositeBehavior::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeIgnivexCompositeBehaviorTriggerSet(
        FWTBRIgnivexCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnIgnivexCompositeBehaviorCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartIgnivexCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset =
            MakeIgnivexCompositeBehaviorDataAsset(EWTBRBulletArchetype::Fulgrix);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeIgnivexCompositeBehaviorDataAsset(EWTBRBulletArchetype::Serpveil);
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRIgnivexCompositeBehaviorPathMovementTest,
    "WTBR.Composite.Behavior.Ignivex.PathMovementEngagesWithoutStraightLaunch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRIgnivexCompositeBehaviorPathMovementTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRIgnivexCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Ignivex_PathMovement"));
    AWTBRCharacter* Owner = SpawnIgnivexCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeIgnivexCompositeBehaviorDefinition();
    UWTBRIgnivexCompositeBehavior* Behavior = NewObject<UWTBRIgnivexCompositeBehavior>(Owner);
    TestTrue(TEXT("Ignivex behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindIgnivexCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Ignivex projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("Path movement is active"), Projectile->InterpMovement->IsActive());
    TestFalse(TEXT("Straight projectile movement stays inactive"), Projectile->ProjectileMovement->IsActive());
    TestTrue(TEXT("Ignivex projectile is armed to explode"), Projectile->bExplodeOnImpact);
    TestEqual(TEXT("Ignivex uses the definition explosion radius"),
        Projectile->ExplosionRadius, Definition.ExplosionParams.ExplosionRadius);
    TestFalse(TEXT("Ignivex projectile cannot penetrate"), Projectile->bCanPenetrate);
    TestEqual(TEXT("Ignivex retains the full damage budget"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRIgnivexCompositeBehaviorNoPathPresetTest,
    "WTBR.Composite.Behavior.Ignivex.NoPathPresetFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRIgnivexCompositeBehaviorNoPathPresetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRIgnivexCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Ignivex_NoPathPreset"));
    AWTBRCharacter* Owner = SpawnIgnivexCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeIgnivexCompositeBehaviorDefinition();
    Definition.PathPreset.Lanes.Reset();
    UWTBRIgnivexCompositeBehavior* Behavior = NewObject<UWTBRIgnivexCompositeBehavior>(Owner);
    TestFalse(TEXT("Missing path preset returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Missing path preset spawns nothing"),
        CountIgnivexCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRIgnivexCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Ignivex.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRIgnivexCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRIgnivexCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Ignivex_NullClass"));
    AWTBRCharacter* Owner = SpawnIgnivexCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeIgnivexCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRIgnivexCompositeBehavior* Behavior = NewObject<UWTBRIgnivexCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountIgnivexCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRIgnivexCompositeBehaviorMultiWaypointPathTest,
    "WTBR.Composite.Behavior.Ignivex.MultiWaypointPathIsHonored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRIgnivexCompositeBehaviorMultiWaypointPathTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRIgnivexCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Ignivex_MultiWaypoint"));
    AWTBRCharacter* Owner = SpawnIgnivexCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeIgnivexCompositeBehaviorDefinition();
    UWTBRIgnivexCompositeBehavior* Behavior = NewObject<UWTBRIgnivexCompositeBehavior>(Owner);
    TestTrue(TEXT("Ignivex behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindIgnivexCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Ignivex projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("Multi-waypoint path computes a positive duration"),
        Projectile->InterpMovement->Duration > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRIgnivexCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Ignivex.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRIgnivexCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRIgnivexCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Ignivex_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeIgnivexCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeIgnivexCompositeBehaviorRegistry();
    StartIgnivexCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Ignivex becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Ignivex fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindIgnivexCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("BehaviorClass path engages path movement"), Projectile->InterpMovement->IsActive());
    TestTrue(TEXT("BehaviorClass path arms the explosion"), Projectile->bExplodeOnImpact);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
