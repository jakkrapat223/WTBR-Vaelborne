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
    class FWTBRHomingExplosiveCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRHomingExplosiveCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRHomingExplosiveCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnHomingExplosiveCompositeBehaviorCharacter(UWorld* World, const FVector& Location)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindHomingExplosiveCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
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

    int32 CountHomingExplosiveCompositeBehaviorProjectiles(UWorld* World)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                ++Count;
            }
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeHomingExplosiveCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Fulgvyn;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 125.0f;
        Definition.ProjectileSpeed = 2345.0f;
        Definition.ExplosionParams.bExplodes = true;
        Definition.ExplosionParams.ExplosionRadius = 325.0f;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeHomingExplosiveCompositeBehaviorArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeHomingExplosiveCompositeBehaviorRegistry(bool bWithBehavior)
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Fulgrix;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
        Definition.CompositeType = EWTBRCompositeBulletType::Fulgvyn;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = bWithBehavior ? UWTBRHomingExplosiveCompositeBehavior::StaticClass() : nullptr;
        Definition.TotalDamageBudget = 77.0f;
        Definition.ProjectileSpeed = 1777.0f;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeHomingExplosiveCompositeBehaviorTriggerSet(
        FWTBRHomingExplosiveCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnHomingExplosiveCompositeBehaviorCharacter(
            Fixture.GetWorld(), FVector::ZeroVector);
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartHomingExplosiveCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry,
        UWTBRTriggerDataAsset* MainDataAsset, UWTBRTriggerDataAsset* SubDataAsset)
    {
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHomingExplosiveCompositeBehaviorNonHomingProjectileTest,
    "WTBR.Composite.Behavior.HomingExplosive.NonHomingProjectile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHomingExplosiveCompositeBehaviorNonHomingProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_NonHoming"));
    AWTBRCharacter* Owner = SpawnHomingExplosiveCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    TestNotNull(TEXT("Owner spawns"), Owner);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeHomingExplosiveCompositeBehaviorDefinition();
    UWTBRHomingExplosiveCompositeBehavior* Behavior = NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestTrue(TEXT("Non-homing behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindHomingExplosiveCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Non-homing projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("Behavior uses definition damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    TestEqual(TEXT("Behavior uses definition speed"), Projectile->ProjectileMovement->InitialSpeed, Definition.ProjectileSpeed);
    TestTrue(TEXT("Behavior enables configured explosion"), Projectile->bExplodeOnImpact);
    TestEqual(TEXT("Behavior uses configured explosion radius"), Projectile->ExplosionRadius, Definition.ExplosionParams.ExplosionRadius);
    TestFalse(TEXT("Non-homing definition fires straight"), Projectile->ProjectileMovement->bIsHomingProjectile);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHomingExplosiveCompositeBehaviorValidTargetHomingTest,
    "WTBR.Composite.Behavior.HomingExplosive.ValidTargetEnablesHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHomingExplosiveCompositeBehaviorValidTargetHomingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_Target"));
    AWTBRCharacter* Owner = SpawnHomingExplosiveCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnHomingExplosiveCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Owner spawns"), Owner);
    TestNotNull(TEXT("Enemy spawns"), Enemy);
    if (!Owner || !Enemy) return false;

    Owner->SetTeamId(0);
    Enemy->SetTeamId(1);
    FWTBRCompositeDefinition Definition = MakeHomingExplosiveCompositeBehaviorDefinition();
    Definition.HomingParams.bEnableHoming = true;
    Definition.HomingParams.TargetAcquisitionRange = 2000.0f;
    Definition.HomingParams.AimConeHalfAngleDegrees = 35.0f;
    Definition.HomingParams.HomingAcceleration = 5000.0f;

    UWTBRHomingExplosiveCompositeBehavior* Behavior = NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestTrue(TEXT("Homing behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindHomingExplosiveCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Homing projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestTrue(TEXT("Valid target enables homing"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Homing targets enemy root component"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get(), Enemy->GetRootComponent());
    TestEqual(TEXT("Homing uses definition acceleration"),
        Projectile->ProjectileMovement->HomingAccelerationMagnitude, Definition.HomingParams.HomingAcceleration);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHomingExplosiveCompositeBehaviorNoTargetFiresStraightTest,
    "WTBR.Composite.Behavior.HomingExplosive.NoTargetFiresStraight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHomingExplosiveCompositeBehaviorNoTargetFiresStraightTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_NoTarget"));
    AWTBRCharacter* Owner = SpawnHomingExplosiveCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeHomingExplosiveCompositeBehaviorDefinition();
    Definition.HomingParams.bEnableHoming = true;
    Definition.HomingParams.TargetAcquisitionRange = 2000.0f;
    Definition.HomingParams.AimConeHalfAngleDegrees = 35.0f;
    Definition.HomingParams.HomingAcceleration = 5000.0f;

    UWTBRHomingExplosiveCompositeBehavior* Behavior = NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestTrue(TEXT("Homing behavior without a target still fires"), Behavior->ExecuteComposite(Owner, Definition));

    AWTBRProjectileBase* Projectile = FindHomingExplosiveCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Straight projectile spawns without a target"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestFalse(TEXT("No valid target leaves homing disabled"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestNull(TEXT("No valid target assigns no homing component"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHomingExplosiveCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.HomingExplosive.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHomingExplosiveCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_NullClass"));
    AWTBRCharacter* Owner = SpawnHomingExplosiveCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeHomingExplosiveCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    const int32 ProjectileCountBefore = CountHomingExplosiveCompositeBehaviorProjectiles(Fixture.GetWorld());

    UWTBRHomingExplosiveCompositeBehavior* Behavior = NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountHomingExplosiveCompositeBehaviorProjectiles(Fixture.GetWorld()), ProjectileCountBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeBehaviorClassDispatchesBeforeLegacyPathTest,
    "WTBR.Composite.Behavior.HomingExplosive.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeBehaviorClassDispatchesBeforeLegacyPathTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeHomingExplosiveCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeHomingExplosiveCompositeBehaviorRegistry(true);
    StartHomingExplosiveCompositeBehaviorMerge(
        TriggerSet,
        Registry,
        MakeHomingExplosiveCompositeBehaviorArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix),
        MakeHomingExplosiveCompositeBehaviorArchetypeDataAsset(EWTBRBulletArchetype::Venyx));

    TestTrue(TEXT("Fulgvyn becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Fulgvyn fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindHomingExplosiveCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns a projectile without legacy maps"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("BehaviorClass path uses definition damage"), Projectile->BaseDamage, 77.0f);
    TestEqual(TEXT("BehaviorClass path uses definition speed"), Projectile->ProjectileMovement->InitialSpeed, 1777.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeNoBehaviorClassPreservesGenericLegacyPathTest,
    "WTBR.Composite.Behavior.HomingExplosive.NoBehaviorClassPreservesGenericLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeNoBehaviorClassPreservesGenericLegacyPathTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRHomingExplosiveCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_HomingExplosive_Legacy"));
    UWTBRTriggerSetComponent* TriggerSet = MakeHomingExplosiveCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRTriggerDataAsset* MainDataAsset =
        MakeHomingExplosiveCompositeBehaviorArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix);
    UWTBRTriggerDataAsset* SubDataAsset =
        MakeHomingExplosiveCompositeBehaviorArchetypeDataAsset(EWTBRBulletArchetype::Venyx);
    MainDataAsset->CompositeProjectileClasses.Add(EWTBRCompositeBulletType::Fulgvyn, AWTBRProjectileBase::StaticClass());
    MainDataAsset->CompositeDamages.Add(EWTBRCompositeBulletType::Fulgvyn, 33.0f);

    UWTBRCompositeRegistryDataAsset* Registry = MakeHomingExplosiveCompositeBehaviorRegistry(false);
    StartHomingExplosiveCompositeBehaviorMerge(TriggerSet, Registry, MainDataAsset, SubDataAsset);
    TestTrue(TEXT("Ready Fulgvyn fires through the legacy generic path"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindHomingExplosiveCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("Legacy generic path still spawns its projectile"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("Legacy generic path retains map damage"), Projectile->BaseDamage, 33.0f);
    TestEqual(TEXT("Legacy generic path retains hardcoded speed"), Projectile->ProjectileMovement->InitialSpeed, 3000.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
