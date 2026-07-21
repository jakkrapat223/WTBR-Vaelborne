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
#include "Trigger/WTBRCatarixCompositeBehavior.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCatarixCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRCatarixCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCatarixCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnCatarixCompositeBehaviorCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindCatarixCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner))
            {
                return *It;
            }
        }
        return nullptr;
    }

    int32 CountCatarixCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeCatarixCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Catarix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 160.0f;

        // Solgrix and Catarix became path-driven when Meteo presets landed, so their
        // fixtures need a lane the way the Viper composites' always did. Pinned to a
        // single straight cube so these tests keep measuring explosion lifecycle and
        // damage delivery rather than volley size.
        FWTBRPathLane& Lane = Definition.PathPreset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
        Lane.CubeCount = 1;
        Lane.FormationOffset = FVector::ZeroVector;
        Definition.CubeCount = 1;
        Definition.ProjectileSpeed = 2200.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 150.0f;
        Definition.ExplosionParams.bHasSecondBlast = true;
        Definition.ExplosionParams.SecondBlastDelay = 1.0f;
        Definition.ExplosionParams.SecondBlastRadius = 325.0f;
        Definition.ExplosionParams.SecondBlastDamageRatio = 0.25f;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeCatarixCompositeBehaviorDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = EWTBRBulletArchetype::Fulgrix;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeCatarixCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Fulgrix;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Catarix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = UWTBRCatarixCompositeBehavior::StaticClass();
        Definition.TotalDamageBudget = 160.0f;

        // Solgrix and Catarix became path-driven when Meteo presets landed, so their
        // fixtures need a lane the way the Viper composites' always did. Pinned to a
        // single straight cube so these tests keep measuring explosion lifecycle and
        // damage delivery rather than volley size.
        FWTBRPathLane& Lane = Definition.PathPreset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
        Lane.CubeCount = 1;
        Lane.FormationOffset = FVector::ZeroVector;
        Definition.CubeCount = 1;
        Definition.ProjectileSpeed = 2200.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 150.0f;
        Definition.ExplosionParams.bHasSecondBlast = true;
        Definition.ExplosionParams.SecondBlastDelay = 1.0f;
        Definition.ExplosionParams.SecondBlastRadius = 325.0f;
        Definition.ExplosionParams.SecondBlastDamageRatio = 0.25f;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeCatarixCompositeBehaviorTriggerSet(
        FWTBRCatarixCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnCatarixCompositeBehaviorCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartCatarixCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset = MakeCatarixCompositeBehaviorDataAsset();
        UWTBRTriggerDataAsset* SubDataAsset = MakeCatarixCompositeBehaviorDataAsset();
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCatarixCompositeBehaviorDamageSplitAndDelayedExplosionTest,
    "WTBR.Composite.Behavior.Catarix.DamageSplitAndDelayedExplosion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCatarixCompositeBehaviorDamageSplitAndDelayedExplosionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCatarixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Catarix_DelayedExplosion"));
    AWTBRCharacter* Owner = SpawnCatarixCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeCatarixCompositeBehaviorDefinition();
    UWTBRCatarixCompositeBehavior* Behavior = NewObject<UWTBRCatarixCompositeBehavior>(Owner);
    TestTrue(TEXT("Catarix behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindCatarixCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Catarix projectile spawns"), Projectile);
    if (!Projectile) return false;

    const float ExpectedFirstDamage = Definition.TotalDamageBudget *
        (1.0f - Definition.ExplosionParams.SecondBlastDamageRatio);
    const float ExpectedSecondDamage = Definition.TotalDamageBudget *
        Definition.ExplosionParams.SecondBlastDamageRatio;
    TestEqual(TEXT("First blast receives its definition split"), Projectile->BaseDamage, ExpectedFirstDamage);
    TestEqual(TEXT("Second blast receives its definition split"), Projectile->SecondExplosionDamage, ExpectedSecondDamage);
    TestEqual(TEXT("Both blasts remain capped at the total damage budget"),
        Projectile->BaseDamage + Projectile->SecondExplosionDamage, Definition.TotalDamageBudget);

    Projectile->TriggerExplosionForTest();
    TestTrue(TEXT("Projectile survives while the second blast is pending"), IsValid(Projectile));
    TestTrue(TEXT("Second blast timer is active"), Projectile->IsSecondExplosionTimerActiveForTest());

    Projectile->TriggerSecondExplosionForTest();
    TestFalse(TEXT("Projectile is destroyed after the second blast"), IsValid(Projectile));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCatarixCompositeBehaviorSingleBlastRegressionTest,
    "WTBR.Composite.Behavior.Catarix.SingleBlastPreservesImmediateDestroy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCatarixCompositeBehaviorSingleBlastRegressionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCatarixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Catarix_SingleBlast"));
    AWTBRCharacter* Owner = SpawnCatarixCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeCatarixCompositeBehaviorDefinition();
    Definition.ExplosionParams.bHasSecondBlast = false;
    UWTBRCatarixCompositeBehavior* Behavior = NewObject<UWTBRCatarixCompositeBehavior>(Owner);
    TestTrue(TEXT("Single-blast behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindCatarixCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Single-blast projectile spawns"), Projectile);
    if (!Projectile) return false;

    Projectile->TriggerExplosionForTest();
    TestFalse(TEXT("Default single blast destroys immediately"), IsValid(Projectile));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCatarixCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Catarix.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCatarixCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCatarixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Catarix_NullClass"));
    AWTBRCharacter* Owner = SpawnCatarixCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeCatarixCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRCatarixCompositeBehavior* Behavior = NewObject<UWTBRCatarixCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountCatarixCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCatarixCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Catarix.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCatarixCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCatarixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Catarix_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCatarixCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeCatarixCompositeBehaviorRegistry();
    StartCatarixCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Catarix becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Catarix fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindCatarixCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile) return false;

    const float ExpectedFirstDamage = Registry->Definitions[0].TotalDamageBudget *
        (1.0f - Registry->Definitions[0].ExplosionParams.SecondBlastDamageRatio);
    TestEqual(TEXT("BehaviorClass path uses the definition first-blast split"),
        Projectile->BaseDamage, ExpectedFirstDamage);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
