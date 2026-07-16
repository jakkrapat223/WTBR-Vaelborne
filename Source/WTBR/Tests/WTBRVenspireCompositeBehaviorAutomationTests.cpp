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
#include "Trigger/WTBRVenspireCompositeBehavior.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRVenspireCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRVenspireCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRVenspireCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnVenspireCompositeBehaviorCharacter(UWorld* World, const FVector& Location)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    void FindVenspireCompositeBehaviorProjectiles(
        UWorld* World, const AActor* Owner, TArray<AWTBRProjectileBase*>& OutProjectiles)
    {
        OutProjectiles.Reset();
        if (!World) return;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner))
            {
                OutProjectiles.Add(*It);
            }
        }
    }

    AWTBRProjectileBase* FindVenspireCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        TArray<AWTBRProjectileBase*> Projectiles;
        FindVenspireCompositeBehaviorProjectiles(World, Owner, Projectiles);
        return Projectiles.Num() > 0 ? Projectiles[0] : nullptr;
    }

    int32 CountVenspireCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        TArray<AWTBRProjectileBase*> Projectiles;
        FindVenspireCompositeBehaviorProjectiles(World, Owner, Projectiles);
        return Projectiles.Num();
    }

    FWTBRCompositeDefinition MakeVenspireCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Venspire;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 120.0f;
        Definition.ProjectileSpeed = 2400.0f;
        Definition.HomingParams.bEnableHoming = true;
        Definition.HomingParams.TargetAcquisitionRange = 2000.0f;
        Definition.HomingParams.AimConeHalfAngleDegrees = 45.0f;
        Definition.HomingParams.HomingAcceleration = 4500.0f;
        return Definition;
    }

    UWTBRTriggerDataAsset* MakeVenspireCompositeBehaviorDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = EWTBRBulletArchetype::Venyx;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeVenspireCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Venyx;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
        Definition.CompositeType = EWTBRCompositeBulletType::Venspire;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.BehaviorClass = UWTBRVenspireCompositeBehavior::StaticClass();
        Definition.TotalDamageBudget = 120.0f;
        Definition.ProjectileSpeed = 2400.0f;
        Definition.HomingParams.bEnableHoming = true;
        Definition.HomingParams.TargetAcquisitionRange = 2000.0f;
        Definition.HomingParams.AimConeHalfAngleDegrees = 45.0f;
        Definition.HomingParams.HomingAcceleration = 4500.0f;
        Definition.HomingParams.SimultaneousTargetCount = 2;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeVenspireCompositeBehaviorTriggerSet(
        FWTBRVenspireCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnVenspireCompositeBehaviorCharacter(
            Fixture.GetWorld(), FVector::ZeroVector);
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartVenspireCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset = MakeVenspireCompositeBehaviorDataAsset();
        UWTBRTriggerDataAsset* SubDataAsset = MakeVenspireCompositeBehaviorDataAsset();
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }

    void SetEnemyTeam(AWTBRCharacter* Enemy)
    {
        if (Enemy) Enemy->SetTeamId(1);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorNonHomingProjectileTest,
    "WTBR.Composite.Behavior.Venspire.NonHomingProjectile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorNonHomingProjectileTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_NonHoming"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;

    Owner->SetTeamId(0);
    SetEnemyTeam(Enemy);
    FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    Definition.HomingParams.bEnableHoming = false;

    UWTBRVenspireCompositeBehavior* Behavior = NewObject<UWTBRVenspireCompositeBehavior>(Owner);
    TestTrue(TEXT("Non-homing behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindVenspireCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Non-homing projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("Non-homing behavior fires one projectile"),
        CountVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 1);
    TestFalse(TEXT("Disabled homing fires straight"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Non-homing behavior preserves total damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorMultiTargetTest,
    "WTBR.Composite.Behavior.Venspire.MultiTargetSplitsDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorMultiTargetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_MultiTarget"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* BestAim = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    AWTBRCharacter* SecondBestAim = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 100.0f, 0.0f));
    AWTBRCharacter* ThirdBestAim = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 300.0f, 0.0f));
    if (!Owner || !BestAim || !SecondBestAim || !ThirdBestAim) return false;

    Owner->SetTeamId(0);
    SetEnemyTeam(BestAim);
    SetEnemyTeam(SecondBestAim);
    SetEnemyTeam(ThirdBestAim);
    FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    Definition.HomingParams.SimultaneousTargetCount = 2;

    UWTBRVenspireCompositeBehavior* Behavior = NewObject<UWTBRVenspireCompositeBehavior>(Owner);
    TestTrue(TEXT("Multi-target behavior executes"), Behavior->ExecuteComposite(Owner, Definition));

    TArray<AWTBRProjectileBase*> Projectiles;
    FindVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner, Projectiles);
    TestEqual(TEXT("Two target limit spawns two projectiles"), Projectiles.Num(), 2);
    if (Projectiles.Num() != 2) return false;

    TSet<USceneComponent*> HomingTargets;
    for (AWTBRProjectileBase* Projectile : Projectiles)
    {
        if (!Projectile || !Projectile->ProjectileMovement) return false;
        TestTrue(TEXT("Each multi-target projectile homes"), Projectile->ProjectileMovement->bIsHomingProjectile);
        TestEqual(TEXT("Damage splits across actual projectile count"),
            Projectile->BaseDamage, Definition.TotalDamageBudget / 2.0f);
        HomingTargets.Add(Projectile->ProjectileMovement->HomingTargetComponent.Get());
    }
    TestEqual(TEXT("Each projectile receives a distinct target"), HomingTargets.Num(), 2);
    TestTrue(TEXT("Best aim target is selected"), HomingTargets.Contains(BestAim->GetRootComponent()));
    TestTrue(TEXT("Second best aim target is selected"), HomingTargets.Contains(SecondBestAim->GetRootComponent()));
    TestFalse(TEXT("Third target is excluded by the limit"), HomingTargets.Contains(ThirdBestAim->GetRootComponent()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorNoTargetFallbackTest,
    "WTBR.Composite.Behavior.Venspire.NoTargetFiresStraight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorNoTargetFallbackTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_NoTarget"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    UWTBRVenspireCompositeBehavior* Behavior = NewObject<UWTBRVenspireCompositeBehavior>(Owner);
    TestTrue(TEXT("No-target behavior still fires"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindVenspireCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("No-target fallback projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("No target spawns exactly one projectile"),
        CountVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 1);
    TestFalse(TEXT("No target leaves homing disabled"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("No target preserves full damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorFewerTargetsTest,
    "WTBR.Composite.Behavior.Venspire.FewerTargetsDoesNotPadOrOverSplitDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorFewerTargetsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_FewerTargets"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;

    Owner->SetTeamId(0);
    SetEnemyTeam(Enemy);
    FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    Definition.HomingParams.SimultaneousTargetCount = 3;

    UWTBRVenspireCompositeBehavior* Behavior = NewObject<UWTBRVenspireCompositeBehavior>(Owner);
    TestTrue(TEXT("Fewer-target behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindVenspireCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Single available target spawns one projectile"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("Single available target is not padded"),
        CountVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 1);
    TestTrue(TEXT("Single available target enables homing"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Single available target receives full damage"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Venspire.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_NullClass"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRVenspireCompositeBehavior* Behavior = NewObject<UWTBRVenspireCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Venspire.BehaviorClassDispatchesBeforeLegacyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeVenspireCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Owner = Cast<AWTBRCharacter>(TriggerSet->GetOwner());
    AWTBRCharacter* Enemy = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;
    Owner->SetTeamId(0);
    SetEnemyTeam(Enemy);

    UWTBRCompositeRegistryDataAsset* Registry = MakeVenspireCompositeBehaviorRegistry();
    StartVenspireCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Venspire becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Venspire fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindVenspireCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("BehaviorClass path uses definition damage"), Projectile->BaseDamage, Registry->Definitions[0].TotalDamageBudget);
    TestEqual(TEXT("BehaviorClass path uses definition speed"),
        Projectile->ProjectileMovement->InitialSpeed, Registry->Definitions[0].ProjectileSpeed);
    TestTrue(TEXT("BehaviorClass path homes to the available target"), Projectile->ProjectileMovement->bIsHomingProjectile);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVenspireCompositeBehaviorFulgvynDefaultRegressionTest,
    "WTBR.Composite.Behavior.Venspire.FulgvynDefaultTargetCountRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVenspireCompositeBehaviorFulgvynDefaultRegressionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRVenspireCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Venspire_FulgvynRegression"));
    AWTBRCharacter* Owner = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnVenspireCompositeBehaviorCharacter(Fixture.GetWorld(), FVector(1000.0f, 0.0f, 0.0f));
    if (!Owner || !Enemy) return false;

    Owner->SetTeamId(0);
    SetEnemyTeam(Enemy);
    FWTBRCompositeDefinition Definition = MakeVenspireCompositeBehaviorDefinition();
    Definition.CompositeType = EWTBRCompositeBulletType::Fulgvyn;
    TestEqual(TEXT("Fulgvyn-style definition retains target-count default"),
        Definition.HomingParams.SimultaneousTargetCount, 1);

    UWTBRHomingExplosiveCompositeBehavior* Behavior = NewObject<UWTBRHomingExplosiveCompositeBehavior>(Owner);
    TestTrue(TEXT("Fulgvyn behavior executes with the new default field"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindVenspireCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Fulgvyn regression projectile spawns"), Projectile);
    if (!Projectile || !Projectile->ProjectileMovement) return false;

    TestEqual(TEXT("Fulgvyn remains one projectile"), CountVenspireCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 1);
    TestTrue(TEXT("Fulgvyn remains a homing projectile"), Projectile->ProjectileMovement->bIsHomingProjectile);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
