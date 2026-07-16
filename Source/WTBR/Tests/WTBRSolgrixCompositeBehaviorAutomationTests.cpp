// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRSolgrixCompositeBehavior.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRSolgrixCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRSolgrixCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRSolgrixCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnSolgrixCompositeBehaviorCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindSolgrixCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) return *It;
        }
        return nullptr;
    }

    int32 CountSolgrixCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeSolgrixCompositeBehaviorDefinition(bool bIsShapedCharge = true)
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 140.0f;
        Definition.ProjectileSpeed = 2600.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 200.0f;
        Definition.ExplosionParams.bIsShapedCharge = bIsShapedCharge;
        Definition.ExplosionParams.ShapedChargeConeHalfAngleDegrees = 35.0f;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeSolgrixCompositeBehaviorDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeSolgrixCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = UWTBRSolgrixCompositeBehavior::StaticClass();
        Definition.TotalDamageBudget = 140.0f;
        Definition.ProjectileSpeed = 2600.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 200.0f;
        Definition.ExplosionParams.bIsShapedCharge = true;
        Definition.ExplosionParams.ShapedChargeConeHalfAngleDegrees = 30.0f;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeSolgrixCompositeBehaviorTriggerSet(
        FWTBRSolgrixCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnSolgrixCompositeBehaviorCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartSolgrixCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset =
            MakeSolgrixCompositeBehaviorDataAsset(EWTBRBulletArchetype::Solux);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeSolgrixCompositeBehaviorDataAsset(EWTBRBulletArchetype::Fulgrix);
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolgrixCompositeBehaviorConeMathTest,
    "WTBR.Composite.Behavior.Solgrix.ConeMathAndSphereFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolgrixCompositeBehaviorConeMathTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolgrixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solgrix_ConeMath"));
    AWTBRProjectileBase* Projectile = Fixture.GetWorld()->SpawnActor<AWTBRProjectileBase>(
        AWTBRProjectileBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    TestNotNull(TEXT("Projectile spawns"), Projectile);
    if (!Projectile) return false;

    Projectile->SetActorRotation(FRotator::ZeroRotator);
    Projectile->bIsShapedCharge = true;
    Projectile->ShapedChargeConeHalfAngleDegrees = 45.0f;
    const FVector Ahead(1000.0f, 0.0f, 0.0f);
    const FVector Behind(-1000.0f, 0.0f, 0.0f);
    const FVector Side(0.0f, 1000.0f, 0.0f);
    const FVector Epicenter = Projectile->GetActorLocation();

    TestTrue(TEXT("Forward target is in the shaped-charge cone"), Projectile->IsLocationWithinShapedChargeCone(Ahead));
    TestFalse(TEXT("Rear target is outside the shaped-charge cone"), Projectile->IsLocationWithinShapedChargeCone(Behind));
    TestFalse(TEXT("Side target is outside the shaped-charge cone"), Projectile->IsLocationWithinShapedChargeCone(Side));
    TestTrue(TEXT("Epicenter remains a valid direct hit"), Projectile->IsLocationWithinShapedChargeCone(Epicenter));

    Projectile->bIsShapedCharge = false;
    TestTrue(TEXT("Sphere fallback includes forward target"), Projectile->IsLocationWithinShapedChargeCone(Ahead));
    TestTrue(TEXT("Sphere fallback includes rear target"), Projectile->IsLocationWithinShapedChargeCone(Behind));
    TestTrue(TEXT("Sphere fallback includes side target"), Projectile->IsLocationWithinShapedChargeCone(Side));
    TestTrue(TEXT("Sphere fallback includes epicenter"), Projectile->IsLocationWithinShapedChargeCone(Epicenter));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolgrixCompositeBehaviorFullDamageAndLifecycleTest,
    "WTBR.Composite.Behavior.Solgrix.FullDamageAndDefaultExplosionLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolgrixCompositeBehaviorFullDamageAndLifecycleTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolgrixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solgrix_FullDamage"));
    AWTBRCharacter* Owner = SpawnSolgrixCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeSolgrixCompositeBehaviorDefinition(false);
    UWTBRSolgrixCompositeBehavior* Behavior = NewObject<UWTBRSolgrixCompositeBehavior>(Owner);
    TestTrue(TEXT("Solgrix behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindSolgrixCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Solgrix projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestEqual(TEXT("Solgrix retains the full damage budget"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    TestFalse(TEXT("Definition keeps shaped charge disabled"), Projectile->bIsShapedCharge);
    Projectile->TriggerExplosionForTest();
    TestFalse(TEXT("Default explosion lifecycle still destroys immediately"), IsValid(Projectile));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolgrixCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Solgrix.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolgrixCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolgrixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solgrix_NullClass"));
    AWTBRCharacter* Owner = SpawnSolgrixCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeSolgrixCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRSolgrixCompositeBehavior* Behavior = NewObject<UWTBRSolgrixCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountSolgrixCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSolgrixCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Solgrix.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSolgrixCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSolgrixCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Solgrix_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSolgrixCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeSolgrixCompositeBehaviorRegistry();
    StartSolgrixCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Solgrix becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Solgrix fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindSolgrixCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("BehaviorClass path enables shaped charge"), Projectile->bIsShapedCharge);
    TestEqual(TEXT("BehaviorClass path uses the definition cone angle"),
        Projectile->ShapedChargeConeHalfAngleDegrees,
        Registry->Definitions[0].ExplosionParams.ShapedChargeConeHalfAngleDegrees);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
