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
#include "Trigger/WTBRCoilvynCompositeBehavior.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCoilvynCompositeBehaviorWorldFixture
    {
    public:
        explicit FWTBRCoilvynCompositeBehaviorWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCoilvynCompositeBehaviorWorldFixture()
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

    AWTBRCharacter* SpawnCoilvynCompositeBehaviorCharacter(UWorld* World, const FVector& Location)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    }

    AWTBRProjectileBase* FindCoilvynCompositeBehaviorProjectile(UWorld* World, const AActor* Owner)
    {
        if (!World) return nullptr;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) return *It;
        }
        return nullptr;
    }

    int32 CountCoilvynCompositeBehaviorProjectiles(UWorld* World, const AActor* Owner = nullptr)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It) && (!Owner || It->GetOwner() == Owner)) ++Count;
        }
        return Count;
    }

    FWTBRCompositeDefinition MakeCoilvynCompositeBehaviorDefinition()
    {
        FWTBRCompositeDefinition Definition;
        Definition.CompositeType = EWTBRCompositeBulletType::Coilvyn;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.TotalDamageBudget = 80.0f;
        Definition.ProjectileSpeed = 2800.0f;
        Definition.PathRange = 1500.0f;
        Definition.HomingParams.bEnableHoming = true;
        Definition.HomingParams.TargetAcquisitionRange = 3000.0f;
        Definition.HomingParams.AimConeHalfAngleDegrees = 35.0f;
        Definition.HomingParams.HomingAcceleration = 20000.0f;

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

    UWTBRTriggerDataAsset* MakeCoilvynCompositeBehaviorDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeCoilvynCompositeBehaviorRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition = MakeCoilvynCompositeBehaviorDefinition();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Serpveil;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Venyx;
        Definition.BehaviorClass = UWTBRCoilvynCompositeBehavior::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeCoilvynCompositeBehaviorTriggerSet(
        FWTBRCoilvynCompositeBehaviorWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnCoilvynCompositeBehaviorCharacter(
            Fixture.GetWorld(), FVector::ZeroVector);
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartCoilvynCompositeBehaviorMerge(
        UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        UWTBRTriggerDataAsset* MainDataAsset =
            MakeCoilvynCompositeBehaviorDataAsset(EWTBRBulletArchetype::Serpveil);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeCoilvynCompositeBehaviorDataAsset(EWTBRBulletArchetype::Venyx);
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
        TriggerSet->TriggerMergeCompleteForTest();
    }

    AWTBRCharacter* SpawnCoilvynTarget(UWorld* World, AWTBRCharacter* Owner)
    {
        AWTBRCharacter* Enemy = SpawnCoilvynCompositeBehaviorCharacter(
            World, FVector(1000.0f, 0.0f, 0.0f));
        if (Owner) Owner->SetTeamId(0);
        if (Enemy) Enemy->SetTeamId(1);
        return Enemy;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorPathPhaseTest,
    "WTBR.Composite.Behavior.Coilvyn.PathPhaseArmsCastTimeHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorPathPhaseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_PathPhase"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnCoilvynTarget(Fixture.GetWorld(), Owner);
    if (!Owner || !Enemy) return false;

    const FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestTrue(TEXT("Coilvyn behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindCoilvynCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Coilvyn projectile spawns"), Projectile);
    if (!Projectile || !Projectile->InterpMovement || !Projectile->ProjectileMovement) return false;

    TestTrue(TEXT("Path movement is active"), Projectile->InterpMovement->IsActive());
    TestFalse(TEXT("Straight projectile movement stays inactive"), Projectile->ProjectileMovement->IsActive());
    TestFalse(TEXT("Coilvyn does not explode"), Projectile->bExplodeOnImpact);
    TestTrue(TEXT("Cast-time target arms homing after the path"), Projectile->bHomeAfterPathEnd);
    TestEqual(TEXT("Cast-time target is retained"),
        Projectile->PathEndHomingTarget.Get(), Enemy->GetRootComponent());
    TestEqual(TEXT("Definition homing acceleration is retained"),
        Projectile->PathEndHomingAcceleration, Definition.HomingParams.HomingAcceleration);
    TestEqual(TEXT("Coilvyn retains the full damage budget"), Projectile->BaseDamage, Definition.TotalDamageBudget);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorPathEndTransitionTest,
    "WTBR.Composite.Behavior.Coilvyn.PathEndTransitionsToHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorPathEndTransitionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_PathEnd"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    AWTBRCharacter* Enemy = SpawnCoilvynTarget(Fixture.GetWorld(), Owner);
    if (!Owner || !Enemy) return false;

    const FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestTrue(TEXT("Coilvyn behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindCoilvynCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Coilvyn projectile spawns"), Projectile);
    if (!Projectile || !Projectile->InterpMovement || !Projectile->ProjectileMovement) return false;

    Projectile->OnInterpMovementEndForTest();
    TestTrue(TEXT("Projectile remains valid after path end"), IsValid(Projectile));
    TestFalse(TEXT("Path movement deactivates at path end"), Projectile->InterpMovement->IsActive());
    TestTrue(TEXT("Projectile movement activates for homing"), Projectile->ProjectileMovement->IsActive());
    TestTrue(TEXT("Projectile movement enables homing"), Projectile->ProjectileMovement->bIsHomingProjectile);
    TestEqual(TEXT("Homing keeps the cast-time target"),
        Projectile->ProjectileMovement->HomingTargetComponent.Get(), Enemy->GetRootComponent());
    TestEqual(TEXT("Homing uses definition acceleration"),
        Projectile->ProjectileMovement->HomingAccelerationMagnitude,
        Definition.HomingParams.HomingAcceleration);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorNoTargetPathEndTest,
    "WTBR.Composite.Behavior.Coilvyn.NoTargetPathEndDestroysNormally",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorNoTargetPathEndTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_NoTarget"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    const FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestTrue(TEXT("Coilvyn behavior executes without a target"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindCoilvynCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("No-target Coilvyn projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestFalse(TEXT("No target leaves path-end homing disabled"), Projectile->bHomeAfterPathEnd);
    Projectile->OnInterpMovementEndForTest();
    TestFalse(TEXT("No-target projectile destroys at path end"), IsValid(Projectile));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorNeverExplodesTest,
    "WTBR.Composite.Behavior.Coilvyn.NeverExplodesDespiteDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorNeverExplodesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_NeverExplodes"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    Definition.ExplosionParams.bExplodes = true;
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestTrue(TEXT("Coilvyn behavior executes"), Behavior->ExecuteComposite(Owner, Definition));
    AWTBRProjectileBase* Projectile = FindCoilvynCompositeBehaviorProjectile(Fixture.GetWorld(), Owner);
    TestNotNull(TEXT("Coilvyn projectile spawns"), Projectile);
    if (!Projectile) return false;

    TestFalse(TEXT("Coilvyn remains non-explosive despite the definition"), Projectile->bExplodeOnImpact);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorNoPathPresetTest,
    "WTBR.Composite.Behavior.Coilvyn.NoPathPresetFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorNoPathPresetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_NoPathPreset"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    Definition.PathPreset.Lanes.Reset();
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestFalse(TEXT("Missing path preset returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Missing path preset spawns nothing"),
        CountCoilvynCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorNullProjectileClassTest,
    "WTBR.Composite.Behavior.Coilvyn.NullProjectileClassFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorNullProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_NullClass"));
    AWTBRCharacter* Owner = SpawnCoilvynCompositeBehaviorCharacter(Fixture.GetWorld(), FVector::ZeroVector);
    if (!Owner) return false;

    FWTBRCompositeDefinition Definition = MakeCoilvynCompositeBehaviorDefinition();
    Definition.ProjectileClass = nullptr;
    UWTBRCoilvynCompositeBehavior* Behavior = NewObject<UWTBRCoilvynCompositeBehavior>(Owner);
    TestFalse(TEXT("Null projectile class returns false"), Behavior->ExecuteComposite(Owner, Definition));
    TestEqual(TEXT("Null projectile class spawns nothing"),
        CountCoilvynCompositeBehaviorProjectiles(Fixture.GetWorld(), Owner), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCoilvynCompositeBehaviorClassDispatchTest,
    "WTBR.Composite.Behavior.Coilvyn.BehaviorClassDispatchesCastTimeHoming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCoilvynCompositeBehaviorClassDispatchTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCoilvynCompositeBehaviorWorldFixture Fixture(TEXT("WTBR_Coilvyn_Dispatch"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCoilvynCompositeBehaviorTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Owner = Cast<AWTBRCharacter>(TriggerSet->GetOwner());
    AWTBRCharacter* Enemy = SpawnCoilvynTarget(Fixture.GetWorld(), Owner);
    if (!Owner || !Enemy) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeCoilvynCompositeBehaviorRegistry();
    StartCoilvynCompositeBehaviorMerge(TriggerSet, Registry);
    TestTrue(TEXT("Coilvyn becomes ready"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Ready Coilvyn fires through BehaviorClass"), TriggerSet->FireReadyComposite());

    AWTBRProjectileBase* Projectile = FindCoilvynCompositeBehaviorProjectile(
        Fixture.GetWorld(), TriggerSet->GetOwner());
    TestNotNull(TEXT("BehaviorClass path spawns without legacy maps"), Projectile);
    if (!Projectile) return false;

    TestTrue(TEXT("BehaviorClass path arms cast-time homing"), Projectile->bHomeAfterPathEnd);
    TestEqual(TEXT("BehaviorClass path retains the cast-time target"),
        Projectile->PathEndHomingTarget.Get(), Enemy->GetRootComponent());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
