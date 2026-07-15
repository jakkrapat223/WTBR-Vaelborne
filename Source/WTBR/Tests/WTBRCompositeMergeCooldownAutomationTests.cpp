// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCompositeMergeCooldownWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeCooldownWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeCooldownWorldFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
                World = nullptr;
            }
        }

        UWorld* GetWorld() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    AWTBRCharacter* SpawnCooldownTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeCooldownTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeCooldownTestRegistry(float CompositeCooldown)
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        Definition.CompositeCooldown = CompositeCooldown;
        return Registry;
    }

    UWTBRCompositeRegistryDataAsset* MakeCooldownTestRegistryWithNoCooldown()
    {
        return MakeCooldownTestRegistry(0.0f);
    }

    UWTBRTriggerSetComponent* MakeCooldownTestTriggerSet(
        FWTBRCompositeMergeCooldownWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnCooldownTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed Vael directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureCooldownTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeCooldownTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeCooldownTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    void StartCooldownTestMerge(
        UWTBRTriggerSetComponent* TriggerSet,
        UWTBRCompositeRegistryDataAsset* Registry)
    {
        ConfigureCooldownTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    UWTBRVaelComponent* GetCooldownTestVaelComponent(UWTBRTriggerSetComponent* TriggerSet)
    {
        const AWTBRCharacter* Character = TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
        return Character ? Character->VaelComponent.Get() : nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSuccessfulFireStartsCompositeCooldownTest,
    "WTBR.Composite.Merge.Cooldown.SuccessfulFireStartsCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSuccessfulFireStartsCompositeCooldownTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeCooldownWorldFixture Fixture(TEXT("WTBR_MergeCooldown_Start"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCooldownTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartCooldownTestMerge(TriggerSet, MakeCooldownTestRegistry(2.0f));
    TriggerSet->TriggerMergeCompleteForTest();

    TestTrue(TEXT("Successful fire starts the composite cooldown"),
        TriggerSet->IsCompositeCooldownActiveForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCooldownBlocksNewMergeAttemptWhileActiveTest,
    "WTBR.Composite.Merge.Cooldown.CooldownBlocksNewMergeAttemptWhileActive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCooldownBlocksNewMergeAttemptWhileActiveTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeCooldownWorldFixture Fixture(TEXT("WTBR_MergeCooldown_Blocks"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCooldownTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRVaelComponent* Vael = GetCooldownTestVaelComponent(TriggerSet);
    TestNotNull(TEXT("Vael component exists"), Vael);
    if (!Vael) return false;

    StartCooldownTestMerge(TriggerSet, MakeCooldownTestRegistry(2.0f));
    TriggerSet->TriggerMergeCompleteForTest();
    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Cooldown blocks a new merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Cooldown blocks a new Vael reservation"),
        Vael->GetAvailableVael(), Vael->GetCurrentVael());
    TestEqual(TEXT("Only the successful fire's 20 Vael cost was committed"), Vael->GetCurrentVael(), 80.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRefundPathDoesNotStartCompositeCooldownTest,
    "WTBR.Composite.Merge.Cooldown.RefundPathDoesNotStartCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRefundPathDoesNotStartCompositeCooldownTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeCooldownWorldFixture Fixture(TEXT("WTBR_MergeCooldown_Refund"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCooldownTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeCooldownTestRegistry(2.0f);
    StartCooldownTestMerge(TriggerSet, Registry);
    TriggerSet->SetSlotDataAssetForTest(0,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeCooldownTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TriggerSet->TriggerMergeCompleteForTest();

    TestFalse(TEXT("Refund path does not start the composite cooldown"),
        TriggerSet->IsCompositeCooldownActiveForTest());

    ConfigureCooldownTestSlots(TriggerSet);
    TriggerSet->Server_StartCompositeMerge_Implementation();
    TestEqual(TEXT("A merge can start immediately after a refund"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCancelDoesNotStartCompositeCooldownTest,
    "WTBR.Composite.Merge.Cooldown.CancelDoesNotStartCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCancelDoesNotStartCompositeCooldownTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeCooldownWorldFixture Fixture(TEXT("WTBR_MergeCooldown_Cancel"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCooldownTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartCooldownTestMerge(TriggerSet, MakeCooldownTestRegistry(2.0f));
    TriggerSet->CancelMerge();

    TestFalse(TEXT("Cancel does not start the composite cooldown"),
        TriggerSet->IsCompositeCooldownActiveForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRZeroCompositeCooldownNeverBlocksTest,
    "WTBR.Composite.Merge.Cooldown.ZeroCompositeCooldownNeverBlocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRZeroCompositeCooldownNeverBlocksTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeCooldownWorldFixture Fixture(TEXT("WTBR_MergeCooldown_Zero"));
    UWTBRTriggerSetComponent* TriggerSet = MakeCooldownTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeCooldownTestRegistryWithNoCooldown();
    StartCooldownTestMerge(TriggerSet, Registry);
    TriggerSet->TriggerMergeCompleteForTest();

    TestFalse(TEXT("A zero CompositeCooldown leaves no active timer"),
        TriggerSet->IsCompositeCooldownActiveForTest());
    TriggerSet->Server_StartCompositeMerge_Implementation();
    TestEqual(TEXT("A zero CompositeCooldown permits an immediate second merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
