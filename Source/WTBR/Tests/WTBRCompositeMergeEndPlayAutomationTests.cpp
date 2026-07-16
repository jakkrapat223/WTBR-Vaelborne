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
    class FWTBRCompositeMergeEndPlayWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeEndPlayWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeEndPlayWorldFixture()
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

    AWTBRCharacter* SpawnEndPlayTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeEndPlayTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeEndPlayTestRegistry()
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

    UWTBRTriggerSetComponent* MakeEndPlayTestTriggerSet(
        FWTBRCompositeMergeEndPlayWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnEndPlayTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // EndPlay calls Super::EndPlay, which requires the component lifecycle to have begun.
        Character->DispatchBeginPlay();

        // Seed Vael directly for deterministic merge accounting.
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureEndPlayTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeEndPlayTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeEndPlayTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    void StartEndPlayTestMerge(UWTBRTriggerSetComponent* TriggerSet)
    {
        ConfigureEndPlayTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeEndPlayTestRegistry());
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    AWTBRCharacter* GetEndPlayTestCharacter(UWTBRTriggerSetComponent* TriggerSet)
    {
        return TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBREndPlayReleasesCompositeMergeReservationTest,
    "WTBR.Composite.Merge.EndPlay.EndPlayReleasesActiveMergeReservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBREndPlayReleasesCompositeMergeReservationTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeEndPlayWorldFixture Fixture(TEXT("WTBR_MergeEndPlay_Release"));
    UWTBRTriggerSetComponent* TriggerSet = MakeEndPlayTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetEndPlayTestCharacter(TriggerSet);
    TestNotNull(TEXT("Vael component exists"), Character ? Character->VaelComponent.Get() : nullptr);
    if (!Character || !Character->VaelComponent) return false;

    StartEndPlayTestMerge(TriggerSet);
    TriggerSet->EndPlayForTest();

    TestEqual(TEXT("EndPlay releases the active merge reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    TestFalse(TEXT("EndPlay clears the pending reservation handle"),
        TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBREndPlayClearsCompositeMergeRootTest,
    "WTBR.Composite.Merge.EndPlay.EndPlayClearsActiveMergeRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBREndPlayClearsCompositeMergeRootTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeEndPlayWorldFixture Fixture(TEXT("WTBR_MergeEndPlay_Root"));
    UWTBRTriggerSetComponent* TriggerSet = MakeEndPlayTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetEndPlayTestCharacter(TriggerSet);
    TestNotNull(TEXT("Movement extension component exists"), Character ? Character->MovementExtComponent.Get() : nullptr);
    if (!Character || !Character->MovementExtComponent) return false;

    StartEndPlayTestMerge(TriggerSet);
    TriggerSet->EndPlayForTest();

    TestEqual(TEXT("EndPlay clears Root"),
        Character->MovementExtComponent->GetSpeedModifiers().DebuffPenalty, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBREndPlayResetsCompositeMergeStateTest,
    "WTBR.Composite.Merge.EndPlay.EndPlayResetsActiveMergeState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBREndPlayResetsCompositeMergeStateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeEndPlayWorldFixture Fixture(TEXT("WTBR_MergeEndPlay_State"));
    UWTBRTriggerSetComponent* TriggerSet = MakeEndPlayTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartEndPlayTestMerge(TriggerSet);
    TriggerSet->EndPlayForTest();

    TestEqual(TEXT("EndPlay clears the active merge state"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBREndPlayWithoutActiveMergeIsSafeTest,
    "WTBR.Composite.Merge.EndPlay.EndPlayWithoutActiveMergeIsSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBREndPlayWithoutActiveMergeIsSafeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeEndPlayWorldFixture Fixture(TEXT("WTBR_MergeEndPlay_NoMerge"));
    UWTBRTriggerSetComponent* TriggerSet = MakeEndPlayTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    TriggerSet->EndPlayForTest();

    TestFalse(TEXT("EndPlay without an active merge leaves no reservation handle"),
        TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
