// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCompositeMergeStateMachineWorldFixture
    {
    public:
        explicit FWTBRCompositeMergeStateMachineWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeMergeStateMachineWorldFixture()
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

    void TickTransientWorldForCompositeStateMachineTests(UWorld* World, float TotalSeconds, float StepSeconds = 0.05f)
    {
        if (!World || TotalSeconds <= 0.0f || StepSeconds <= 0.0f) return;

        float ElapsedSeconds = 0.0f;
        while (ElapsedSeconds < TotalSeconds)
        {
            const float DeltaSeconds = FMath::Min(StepSeconds, TotalSeconds - ElapsedSeconds);
            World->Tick(LEVELTICK_All, DeltaSeconds);
            ElapsedSeconds += DeltaSeconds;
        }
    }

    AWTBRCharacter* SpawnStateMachineTestCharacter(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    UWTBRTriggerDataAsset* MakeStateMachineTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeStateMachineTestRegistry(float MergeTime)
    {
        UWTBRCompositeRegistryDataAsset* Registry = NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = MergeTime;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeStateMachineTestTriggerSet(
        FWTBRCompositeMergeStateMachineWorldFixture& Fixture,
        FAutomationTestBase& Test)
    {
        AWTBRCharacter* Character = SpawnStateMachineTestCharacter(Fixture.GetWorld());
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        // The real timer callback is bound to this component, so begin its actor lifecycle.
        Character->DispatchBeginPlay();

        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void ConfigureStateMachineTestSlots(UWTBRTriggerSetComponent* TriggerSet)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeStateMachineTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeStateMachineTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    }

    void StartStateMachineTestMerge(UWTBRTriggerSetComponent* TriggerSet, float MergeTime = 0.5f)
    {
        ConfigureStateMachineTestSlots(TriggerSet);
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeStateMachineTestRegistry(MergeTime));
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    AWTBRCharacter* GetStateMachineTestCharacter(UWTBRTriggerSetComponent* TriggerSet)
    {
        return TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
    }

    struct FWTBRCompositeMergeTimerExpiryTestState
    {
        FWTBRCompositeMergeStateMachineWorldFixture Fixture{TEXT("WTBR_MergeStateMachine_Timer")};
        TWeakObjectPtr<UWTBRTriggerSetComponent> TriggerSet;
        TWeakObjectPtr<UWTBRVaelComponent> Vael;
        float ElapsedSeconds = 0.0f;
    };

    class FWTBRCompositeMergeTimerExpiryLatentCommand : public IAutomationLatentCommand
    {
    public:
        FWTBRCompositeMergeTimerExpiryLatentCommand(
            FAutomationTestBase* InTest,
            const TSharedRef<FWTBRCompositeMergeTimerExpiryTestState>& InState)
            : Test(InTest)
            , State(InState)
        {
        }

        virtual bool Update() override
        {
            UWTBRTriggerSetComponent* TriggerSet = State->TriggerSet.Get();
            UWTBRVaelComponent* Vael = State->Vael.Get();
            UWorld* World = State->Fixture.GetWorld();
            if (!Test || !TriggerSet || !Vael || !World)
            {
                if (Test) Test->AddError(TEXT("Timer-expiry test fixture became invalid before completion."));
                return true;
            }

            constexpr float TickStepSeconds = 0.05f;
            TickTransientWorldForCompositeStateMachineTests(World, TickStepSeconds, TickStepSeconds);
            State->ElapsedSeconds += TickStepSeconds;
            if (State->ElapsedSeconds < 0.2f)
            {
                return false;
            }

            Test->TestEqual(TEXT("Timer expiry clears the merge state"),
                TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
            Test->TestEqual(TEXT("Timer expiry commits the merge reservation"),
                Vael->GetCurrentVael(), 80.0f);
            Test->TestTrue(TEXT("Timer expiry leaves the committed composite ready"),
                TriggerSet->HasReadyComposite());
            Test->TestFalse(TEXT("Timer expiry clears the pending reservation"),
                TriggerSet->HasPendingMergeReservationForTest());
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        TSharedRef<FWTBRCompositeMergeTimerExpiryTestState> State;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRMergeTimerExpiryCompletesMergeTest,
    "WTBR.Composite.Merge.StateMachine.TimerExpiryCompletesMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeTimerExpiryCompletesMergeTest::RunTest(const FString& /*Parameters*/)
{
    const TSharedRef<FWTBRCompositeMergeTimerExpiryTestState> State =
        MakeShared<FWTBRCompositeMergeTimerExpiryTestState>();
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(State->Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet, 0.1f);
    State->TriggerSet = TriggerSet;
    State->Vael = Character->VaelComponent;
    ADD_LATENT_AUTOMATION_COMMAND(FWTBRCompositeMergeTimerExpiryLatentCommand(this, State));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRMergeTimerDoesNotCompleteEarlyTest,
    "WTBR.Composite.Merge.StateMachine.TimerDoesNotCompleteEarly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeTimerDoesNotCompleteEarlyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_TimerEarly"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartStateMachineTestMerge(TriggerSet, 0.1f);
    TickTransientWorldForCompositeStateMachineTests(Fixture.GetWorld(), 0.04f, 0.04f);

    TestEqual(TEXT("Merge remains active before MergeTime elapses"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    TestTrue(TEXT("Reservation remains pending before MergeTime elapses"),
        TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRDamageCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Merge.StateMachine.DamageCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDamageCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_Damage"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    TestNotNull(TEXT("Health component exists"), Character ? Character->HealthComponent.Get() : nullptr);
    if (!Character || !Character->HealthComponent || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet);
    Character->HealthComponent->ApplyDamage(1.0f);

    TestEqual(TEXT("Damage cancels the active merge"), TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Damage releases the reservation"), Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRStaggerCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Merge.StateMachine.StaggerCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRStaggerCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_Stagger"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet);
    Character->ApplyStagger(0.5f);

    TestEqual(TEXT("Stagger cancels the active merge"), TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Stagger releases the reservation"), Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRDoubleCancelCompositeMergeIsIdempotentTest,
    "WTBR.Composite.Merge.StateMachine.DoubleCancelIsIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDoubleCancelCompositeMergeIsIdempotentTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_DoubleCancel"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet);
    TriggerSet->CancelMerge();
    TriggerSet->CancelMerge();

    TestEqual(TEXT("Double cancel leaves Current Vael unchanged"), Character->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Double cancel releases the reservation exactly once"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    TestFalse(TEXT("Double cancel leaves no pending reservation"), TriggerSet->HasPendingMergeReservationForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRCycleMainSlotCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Merge.StateMachine.CycleMainSlotCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCycleMainSlotCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_CycleMain"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet);
    TriggerSet->CycleMainSlot();

    TestEqual(TEXT("Cycling the Main slot cancels the active merge"), TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Cycling the Main slot releases the reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWTBRCycleSubSlotCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Merge.StateMachine.CycleSubSlotCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCycleSubSlotCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeMergeStateMachineWorldFixture Fixture(TEXT("WTBR_MergeStateMachine_CycleSub"));
    UWTBRTriggerSetComponent* TriggerSet = MakeStateMachineTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    AWTBRCharacter* Character = GetStateMachineTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartStateMachineTestMerge(TriggerSet);
    TriggerSet->CycleSubSlot();

    TestEqual(TEXT("Cycling the Sub slot cancels the active merge"), TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Cycling the Sub slot releases the reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
