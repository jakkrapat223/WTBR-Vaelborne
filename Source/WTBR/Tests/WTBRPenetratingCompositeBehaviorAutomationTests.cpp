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
#include "Trigger/WTBRPenetratingCompositeBehavior.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRPenetratingCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRPenetratingCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRPenetratingCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnPenetratingCompositeBehaviorCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindPenetratingCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
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

    int32 CountPenetratingCompositeBehaviorProjectiles(UWorld* World)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakePenetratingCompositeBehaviorDefinition(bool bCanPenetrate)
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Dualux;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 160.0f;
        Definition.ProjectileSpeed = 2650.0f;
        Definition.bCanPenetrate = bCanPenetrate;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakePenetratingCompositeBehaviorDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = EWTBRBulletArchetype::Solux;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakePenetratingCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Solux;
        Definition.CompositeType = EWTBRCompositeBulletType::Dualux;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = UWTBRPenetratingCompositeBehavior::StaticClass();
        Definition.TotalDamageBudget = 160.0f;
        Definition.ProjectileSpeed = 2650.0f;
        Definition.bCanPenetrate = true;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakePenetratingCompositeBehaviorTriggerSet(
        FWTBRPenetratingCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnPenetratingCompositeBehaviorCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartPenetratingCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset = MakePenetratingCompositeBehaviorDataAsset();
        UWTBRTriggerDataAsset* SubDataAsset = MakePenetratingCompositeBehaviorDataAsset();
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPenetratingCompositeBehaviorEnabledTest,
    "WTBR.Composite.Behavior.Penetrating.EnabledUsesDefinitionValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPenetratingCompositeBehaviorEnabledTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPenetratingCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Penetrating_Enabled"));
    AWTBRCharacter* Owner = SpawnPenetratingCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakePenetratingCompositeBehaviorDefinition(true);
    UWTBRPenetratingCompositeBehavior* Behavior = NewObject<UWTBRPenetratingCompositeBehavior>(Owner);
    TestTrue(TEXT("Penetrating behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindPenetratingCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Penetrating projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestTrue(TEXT("Definition enables penetration"), Projectile->bCanPenetrate);
    TestEqual(TEXT("Behavior uses definition damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    TestEqual(TEXT("Behavior uses definition speed"), Projectile->ProjectileMovement->InitialSpeed, Definition.ProjectileSpeed);
    TestFalse(TEXT("Penetrating behavior does not explode"), Projectile->bExplodeOnImpact);
    TestEqual(TEXT("Penetrating behavior uses zero explosion radius"), Projectile->ExplosionRadius, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPenetratingCompositeBehaviorDisabledTest,
    "WTBR.Composite.Behavior.Penetrating.DisabledUsesDefinitionValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPenetratingCompositeBehaviorDisabledTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPenetratingCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Penetrating_Disabled"));
    AWTBRCharacter* Owner = SpawnPenetratingCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakePenetratingCompositeBehaviorDefinition(false);
    UWTBRPenetratingCompositeBehavior* Behavior = NewObject<UWTBRPenetratingCompositeBehavior>(Owner);
    TestTrue(TEXT("Non-penetrating definition still fires"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindPenetratingCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Non-penetrating projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestFalse(TEXT("Definition disables penetration"), Projectile->bCanPenetrate);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPenetratingCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Penetrating.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPenetratingCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPenetratingCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Penetrating_NullClass"));
    AWTBRCharacter* Owner = SpawnPenetratingCompositeBehaviorCharacter(Fixture.GetWorld());
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakePenetratingCompositeBehaviorDefinition(true);
    Definition.ProjectileClass = nullptr;
    const int32 ProjectileCountBefore = CountPenetratingCompositeBehaviorProjectiles(Fixture.GetWorld());

    UWTBRPenetratingCompositeBehavior* Behavior = NewObject<UWTBRPenetratingCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountPenetratingCompositeBehaviorProjectiles(Fixture.GetWorld()), ProjectileCountBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPenetratingCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Penetrating.BehaviorClassDispatchesDualux",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPenetratingCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPenetratingCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Penetrating_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakePenetratingCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakePenetratingCompositeBehaviorRegistry();
    StartPenetratingCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Dualux becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Dualux fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindPenetratingCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns a projectile without legacy maps"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestTrue(TEXT("BehaviorClass path uses definition penetration"), Projectile->bCanPenetrate);
    TestEqual(TEXT("BehaviorClass path uses definition damage"), Projectile->BaseDamage, Registry->Definitions[0].TotalDamageBudget);
    TestEqual(TEXT("BehaviorClass path uses definition speed"),
        Projectile->ProjectileMovement->InitialSpeed, Registry->Definitions[0].ProjectileSpeed);
    TestFalse(TEXT("BehaviorClass path bypasses explosion"), Projectile->bExplodeOnImpact);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
