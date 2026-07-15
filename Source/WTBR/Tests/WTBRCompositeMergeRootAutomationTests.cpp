// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCompositeMergeRootWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeRootWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeRootWorldFixture()
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

    AWTBRCharacter* SpawnRootTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeRootTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeRootTestRegistry()
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

    UWTBRTriggerSetComponent* MakeRootTestTriggerSet(
        FWTBRCompositeMergeRootWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnRootTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed Vael directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureRootTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeRootTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeRootTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    void StartRootTestMerge(UWTBRTriggerSetComponent* TriggerSet)
    {
        ConfigureRootTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeRootTestRegistry());
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    AWTBRCharacter* GetRootTestCharacter(UWTBRTriggerSetComponent* TriggerSet)
    {
        return TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeStartRootsCharacterTest,
    "WTBR.Composite.Merge.Root.MergeStartRootsCharacter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeStartRootsCharacterTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeRootWorldFixture Fixture(TEXT("WTBR_MergeRoot_Start"));
    UWTBRTriggerSetComponent* TriggerSet = MakeRootTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetRootTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartRootTestMerge(TriggerSet);
    TestEqual(TEXT("Merge start applies Root"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCancelMergeUnrootsCharacterTest,
    "WTBR.Composite.Merge.Root.CancelMergeUnrootsCharacter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCancelMergeUnrootsCharacterTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeRootWorldFixture Fixture(TEXT("WTBR_MergeRoot_Cancel"));
    UWTBRTriggerSetComponent* TriggerSet = MakeRootTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetRootTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartRootTestMerge(TriggerSet);
    TriggerSet->CancelMerge();
    TestEqual(TEXT("Cancel clears Root"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeCompleteUnrootsOnSuccessfulFireTest,
    "WTBR.Composite.Merge.Root.MergeCompleteUnrootsCharacterOnSuccessfulFire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeCompleteUnrootsOnSuccessfulFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeRootWorldFixture Fixture(TEXT("WTBR_MergeRoot_Complete"));
    UWTBRTriggerSetComponent* TriggerSet = MakeRootTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetRootTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartRootTestMerge(TriggerSet);
    TriggerSet->TriggerMergeCompleteForTest();
    TestEqual(TEXT("Successful merge completion clears Root"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeCompleteUnrootsWhenSlotChangesTest,
    "WTBR.Composite.Merge.Root.MergeCompleteUnrootsCharacterEvenWhenSlotChangedMidMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeCompleteUnrootsWhenSlotChangesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeRootWorldFixture Fixture(TEXT("WTBR_MergeRoot_ChangedSlot"));
    UWTBRTriggerSetComponent* TriggerSet = MakeRootTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetRootTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartRootTestMerge(TriggerSet);
    TriggerSet->SetSlotDataAssetForTest(0,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeRootTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TriggerSet->TriggerMergeCompleteForTest();
    TestEqual(TEXT("Refund-path completion clears Root"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCharacterStaysRootedWhileMergeInProgressTest,
    "WTBR.Composite.Merge.Root.CharacterStaysRootedWhileMergeIsInProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCharacterStaysRootedWhileMergeInProgressTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeRootWorldFixture Fixture(TEXT("WTBR_MergeRoot_InProgress"));
    UWTBRTriggerSetComponent* TriggerSet = MakeRootTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetRootTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartRootTestMerge(TriggerSet);
    TestEqual(TEXT("Active merge keeps Root applied"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 1.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
