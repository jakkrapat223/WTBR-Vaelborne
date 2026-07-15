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
    class FWTBRCompositeMergeSnapshotWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeSnapshotWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeSnapshotWorldFixture()
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

    AWTBRCharacter* SpawnSnapshotTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeSnapshotTestRegistry()
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

    UWTBRTriggerSetComponent* MakeSnapshotTestTriggerSet(
        FWTBRCompositeMergeSnapshotWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnSnapshotTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed Vael directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureSnapshotTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    UWTBRVaelComponent* GetSnapshotTestVaelComponent(UWTBRTriggerSetComponent* TriggerSet)
    {
        AWTBRCharacter* Character = TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
        return Character ? Character->VaelComponent : nullptr;
    }

    void StartSnapshotTestMerge(UWTBRTriggerSetComponent* TriggerSet)
    {
        ConfigureSnapshotTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeSnapshotTestRegistry());
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeStartSnapshotsMainAndSubSlotIndicesTest,
    "WTBR.Composite.Merge.Snapshot.MergeStartSnapshotsMainAndSubSlotIndices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeStartSnapshotsMainAndSubSlotIndicesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_Indices"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);

    TestEqual(TEXT("Merge snapshots Main slot index"), TriggerSet->GetActiveMergeMainSlotIndexForTest(), 0);
    TestEqual(TEXT("Merge snapshots Sub slot index"), TriggerSet->GetActiveMergeSubSlotIndexForTest(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireCommitsNormallyWhenSlotsUnchangedTest,
    "WTBR.Composite.Merge.Snapshot.FireCommitsNormallyWhenSlotsUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireCommitsNormallyWhenSlotsUnchangedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_HappyPath"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetSnapshotTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Unchanged slots commit Current Vael"), Vael->GetCurrentVael(), 80.0f);
    TestEqual(TEXT("Unchanged slots clear the availability reservation"), Vael->GetAvailableVael(), 80.0f);
    TestFalse(TEXT("Unchanged slots clear the pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireRefundsWhenMainArchetypeChangesDuringMergeTest,
    "WTBR.Composite.Merge.Snapshot.FireRefundsInsteadOfFiringWhenMainSlotArchetypeChangesDuringMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireRefundsWhenMainArchetypeChangesDuringMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_MainChanged"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);
    TriggerSet->SetSlotDataAssetForTest(0,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetSnapshotTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Changed Main slot refunds Current Vael"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Changed Main slot restores Available Vael"), Vael->GetAvailableVael(), 100.0f);
    TestFalse(TEXT("Changed Main slot clears the pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    TestEqual(TEXT("Changed Main slot clears merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireRefundsWhenSubArchetypeChangesDuringMergeTest,
    "WTBR.Composite.Merge.Snapshot.FireRefundsInsteadOfFiringWhenSubSlotArchetypeChangesDuringMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireRefundsWhenSubArchetypeChangesDuringMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_SubChanged"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);
    TriggerSet->SetSlotDataAssetForTest(4,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetSnapshotTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Changed Sub slot refunds Current Vael"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Changed Sub slot restores Available Vael"), Vael->GetAvailableVael(), 100.0f);
    TestFalse(TEXT("Changed Sub slot clears the pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    TestEqual(TEXT("Changed Sub slot clears merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireRefundsWhenMainSlotEmptiedDuringMergeTest,
    "WTBR.Composite.Merge.Snapshot.FireRefundsWhenMainSlotEmptiedDuringMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireRefundsWhenMainSlotEmptiedDuringMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_MainEmptied"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);
    TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>());
    TriggerSet->TriggerMergeCompleteForTest();

    UWTBRVaelComponent* Vael = GetSnapshotTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Empty Main slot refunds Current Vael"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Empty Main slot restores Available Vael"), Vael->GetAvailableVael(), 100.0f);
    TestFalse(TEXT("Empty Main slot clears the pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    TestEqual(TEXT("Empty Main slot clears merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCancelMergeAlwaysReleasesRegardlessOfSlotChangeTest,
    "WTBR.Composite.Merge.Snapshot.CancelMergeAlwaysReleasesRegardlessOfSlotChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCancelMergeAlwaysReleasesRegardlessOfSlotChangeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeSnapshotWorldFixture Fixture(TEXT("WTBR_MergeSnapshot_CancelChanged"));
    UWTBRTriggerSetComponent* TriggerSet = MakeSnapshotTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartSnapshotTestMerge(TriggerSet);
    TriggerSet->SetSlotDataAssetForTest(0,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeSnapshotTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TriggerSet->CancelMerge();

    UWTBRVaelComponent* Vael = GetSnapshotTestVaelComponent(TriggerSet);
    TestEqual(TEXT("Cancel after slot change leaves Current Vael intact"), Vael->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Cancel after slot change restores Available Vael"), Vael->GetAvailableVael(), 100.0f);
    TestFalse(TEXT("Cancel after slot change clears pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
