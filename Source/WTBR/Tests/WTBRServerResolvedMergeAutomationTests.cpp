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
    class FWTBRServerResolvedMergeWorldFixture
    {
    public:
        explicit FWTBRServerResolvedMergeWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRServerResolvedMergeWorldFixture()
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

    AWTBRCharacter* SpawnMergeTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeMergeTestArchetypeDataAsset(
        EWTBRBulletArchetype Archetype,
        bool bIncludeSolgrixMergeValues = false)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        if (bIncludeSolgrixMergeValues)
        {
            DataAsset->CompositeVaelCosts.Add(EWTBRCompositeBulletType::Solgrix, 20.0f);
            DataAsset->CompositeMergeTimes.Add(EWTBRCompositeBulletType::Solgrix, 0.5f);
        }
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeMergeTestRegistry()
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = 0.5f;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeMergeTestTriggerSet(
        FWTBRServerResolvedMergeWorldFixture& Fixture,
        FAutomationTestBase& Test,
        float InitialVael = 100.0f)
    {
        AWTBRCharacter* Character = SpawnMergeTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // BeginPlay does not run in this fixture, so seed Vael directly.
        Character->VaelComponent->DebugSetCurrentVaelDirect(InitialVael);
        return Character->TriggerSetComponent;
    }

    void ConfigureMergeTestSlots(
        UWTBRTriggerSetComponent* TriggerSet,
        UWTBRTriggerDataAsset* MainDataAsset,
        UWTBRTriggerDataAsset* SubDataAsset)
    {
        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        if (SubDataAsset)
        {
            TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        }
    }

    UWTBRVaelComponent* GetMergeTestVaelComponent(UWTBRTriggerSetComponent* TriggerSet)
    {
        AWTBRCharacter* Character = TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
        return Character ? Character->VaelComponent : nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeStartsWhenRegistryHasMatchingDefinitionTest,
    "WTBR.Composite.Merge.MergeStartsWhenRegistryHasMatchingDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeStartsWhenRegistryHasMatchingDefinitionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_Match"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix));
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Matching registry definition starts Solgrix merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    TestEqual(TEXT("Merge start does not deduct Current Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Merge start reserves the registry Vael cost"),
        GetMergeTestVaelComponent(TriggerSet)->GetAvailableVael(), 80.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeResolvesOrderIndependentlyTest,
    "WTBR.Composite.Merge.MergeResolvesOrderIndependently",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeResolvesOrderIndependentlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_Reversed"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux));
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Reversed archetype order resolves to Solgrix"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    TestEqual(TEXT("Reversed merge does not deduct Current Vael at start"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Reversed merge reserves the configured cost"),
        GetMergeTestVaelComponent(TriggerSet)->GetAvailableVael(), 80.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectedWhenNoMatchingDefinitionTest,
    "WTBR.Composite.Merge.MergeRejectedWhenNoMatchingDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectedWhenNoMatchingDefinitionTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_NoRecipe"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx));
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("No registry recipe leaves merge state empty"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("No registry recipe spends no Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectedWhenSubSlotEmptyTest,
    "WTBR.Composite.Merge.MergeRejectedWhenSubSlotEmpty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectedWhenSubSlotEmptyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_EmptySub"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux, true),
        nullptr);
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Empty sub slot rejects the merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Empty sub slot spends no Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectedWhenArchetypeIsNonCombinableTest,
    "WTBR.Composite.Merge.MergeRejectedWhenArchetypeIsNonCombinable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectedWhenArchetypeIsNonCombinableTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_NonCombinable"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::NonCombinable, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix));
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Non-combinable archetype rejects the merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Non-combinable archetype spends no Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeRejectedWhenRegistryAssetUnsetTest,
    "WTBR.Composite.Merge.MergeRejectedWhenRegistryAssetUnset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeRejectedWhenRegistryAssetUnsetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_NoRegistry"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix));

    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Unset registry leaves merge state empty"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Unset registry spends no Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeDoesNotStartWhenAlreadyMergingTest,
    "WTBR.Composite.Merge.MergeDoesNotStartWhenAlreadyMerging",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeDoesNotStartWhenAlreadyMergingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRServerResolvedMergeWorldFixture Fixture(TEXT("WTBR_ServerMerge_AlreadyMerging"));
    UWTBRTriggerSetComponent* TriggerSet = MakeMergeTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    ConfigureMergeTestSlots(TriggerSet,
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Solux, true),
        MakeMergeTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix));
    TriggerSet->CompositeRegistryAsset = TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeMergeTestRegistry());

    TriggerSet->Server_StartCompositeMerge_Implementation();
    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestEqual(TEXT("Second request preserves the active merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    TestEqual(TEXT("Second request does not deduct Current Vael"),
        GetMergeTestVaelComponent(TriggerSet)->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Second request does not reserve Vael twice"),
        GetMergeTestVaelComponent(TriggerSet)->GetAvailableVael(), 80.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
