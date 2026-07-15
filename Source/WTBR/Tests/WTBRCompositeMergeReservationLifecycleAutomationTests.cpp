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
    class FWTBRCompositeMergeReservationLifecycleWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeReservationLifecycleWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeReservationLifecycleWorldFixture()
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

    AWTBRCharacter* SpawnLifecycleTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeLifecycleTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeLifecycleTestRegistry()
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
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeLifecycleTestTriggerSet(
        FWTBRCompositeMergeReservationLifecycleWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnLifecycleTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed Vael directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureLifecycleTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeLifecycleTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeLifecycleTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    UWTBRVaelComponent* GetLifecycleTestVaelComponent(UWTBRTriggerSetComponent* TriggerSet)
    {
        AWTBRCharacter* Character = TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
        return Character ? Character->VaelComponent : nullptr;
    }

    void StartLifecycleTestMerge(UWTBRTriggerSetComponent* TriggerSet, UWTBRCompositeRegistryDataAsset* Registry)
    {
        ConfigureLifecycleTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(Registry);
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectsDefinitionWithNoProjectileClassTest,
    "WTBR.Composite.Merge.Reservation.MergeRejectsDefinitionWithNoProjectileClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectsDefinitionWithNoProjectileClassTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_NoProjectile"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeLifecycleTestRegistry();
    Registry->Definitions[0].ProjectileClass = nullptr;
    StartLifecycleTestMerge(TriggerSet, Registry);

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Missing projectile class rejects the merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Missing projectile class leaves Current Vael intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Missing projectile class leaves Available Vael intact"), Vael->GetAvailableVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectsDefinitionWithNonPositiveMergeTimeTest,
    "WTBR.Composite.Merge.Reservation.MergeRejectsDefinitionWithNonPositiveMergeTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectsDefinitionWithNonPositiveMergeTimeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_NoTime"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    UWTBRCompositeRegistryDataAsset* Registry = MakeLifecycleTestRegistry();
    Registry->Definitions[0].MergeTime = 0.0f;
    StartLifecycleTestMerge(TriggerSet, Registry);

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Non-positive merge time rejects the merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Non-positive merge time leaves Current Vael intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Non-positive merge time leaves Available Vael intact"), Vael->GetAvailableVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeReservesButDoesNotDeductVaelAtStartTest,
    "WTBR.Composite.Merge.Reservation.MergeReservesButDoesNotDeductVaelAtStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeReservesButDoesNotDeductVaelAtStartTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_Start"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartLifecycleTestMerge(TriggerSet, MakeLifecycleTestRegistry());

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Merge start leaves Current Vael intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Merge start reserves Vael from availability"), Vael->GetAvailableVael(), 80.0f);
    TestTrue(TEXT("Merge start stores a pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeCompleteCommitsReservationAndDeductsVaelTest,
    "WTBR.Composite.Merge.Reservation.MergeCompleteCommitsReservationAndDeductsVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeCompleteCommitsReservationAndDeductsVaelTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_Complete"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartLifecycleTestMerge(TriggerSet, MakeLifecycleTestRegistry());
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Merge completion commits the reservation"), Vael->GetCurrentVael(), 80.0f);
    TestEqual(TEXT("No reservation remains after completion"), Vael->GetAvailableVael(), 80.0f);
    TestFalse(TEXT("Merge completion clears the reservation handle"), TriggerSet->HasPendingMergeReservationForTest());
    TestEqual(TEXT("Merge completion clears the merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCancelMergeReleasesReservationWithoutDeductingVaelTest,
    "WTBR.Composite.Merge.Reservation.CancelMergeReleasesReservationWithoutDeductingVael",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCancelMergeReleasesReservationWithoutDeductingVaelTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_Cancel"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartLifecycleTestMerge(TriggerSet, MakeLifecycleTestRegistry());
    TriggerSet->CancelMerge();

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Cancel leaves Current Vael intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Cancel releases the unavailable Vael"), Vael->GetAvailableVael(), 100.0f);
    TestFalse(TEXT("Cancel clears the reservation handle"), TriggerSet->HasPendingMergeReservationForTest());
    TestEqual(TEXT("Cancel clears the merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeCompleteCalledTwiceDoesNotDoubleDeductTest,
    "WTBR.Composite.Merge.Reservation.MergeCompleteCalledTwiceDoesNotDoubleDeduct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeCompleteCalledTwiceDoesNotDoubleDeductTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeReservationLifecycleWorldFixture Fixture(TEXT("WTBR_MergeReservation_DoubleComplete"));
    UWTBRTriggerSetComponent* TriggerSet = MakeLifecycleTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartLifecycleTestMerge(TriggerSet, MakeLifecycleTestRegistry());
    TriggerSet->TriggerMergeCompleteForTest();
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetLifecycleTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Repeated completion does not deduct Vael twice"), Vael->GetCurrentVael(), 80.0f);
    TestEqual(TEXT("Repeated completion leaves availability aligned with Current Vael"),
        Vael->GetAvailableVael(), 80.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
