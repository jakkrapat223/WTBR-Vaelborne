// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRCompositeReadyWorldFixture
    {
    public:
        explicit FWTBRCompositeReadyWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCompositeReadyWorldFixture()
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

    UWTBRTriggerDataAsset* MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype Archetype)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->BulletArchetype = Archetype;
        return DataAsset;
    }

    UWTBRCompositeRegistryDataAsset* MakeReadyTestRegistry(
        float CompositeCooldown = 0.0f, float MergeTime = 0.5f)
    {
        UWTBRCompositeRegistryDataAsset* Registry =
            NewObject<UWTBRCompositeRegistryDataAsset>(GetTransientPackage());
        FWTBRCompositeDefinition& Definition = Registry->Definitions.AddDefaulted_GetRef();
        Definition.RequiredArchetypeA = EWTBRBulletArchetype::Solux;
        Definition.RequiredArchetypeB = EWTBRBulletArchetype::Fulgrix;
        Definition.CompositeType = EWTBRCompositeBulletType::Solgrix;
        Definition.ProjectileClass = AWTBRProjectileBase::StaticClass();
        Definition.VaelCost = 20.0f;
        Definition.MergeTime = MergeTime;
        Definition.CompositeCooldown = CompositeCooldown;
        return Registry;
    }

    UWTBRTriggerSetComponent* MakeReadyTestTriggerSet(
        FWTBRCompositeReadyWorldFixture& Fixture, FAutomationTestBase& Test)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AWTBRCharacter* Character = Fixture.GetWorld()
            ? Fixture.GetWorld()->SpawnActor<AWTBRCharacter>(
                AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params)
            : nullptr;
        Test.TestNotNull(TEXT("Character spawns"), Character);
        if (!Character || !Character->VaelComponent || !Character->TriggerSetComponent) return nullptr;

        Character->DispatchBeginPlay();
        Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        return Character->TriggerSetComponent;
    }

    void StartReadyTestMerge(
        UWTBRTriggerSetComponent* TriggerSet, float CompositeCooldown = 0.0f, float MergeTime = 0.5f)
    {
        TriggerSet->SetSlotDataAssetForTest(0,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
        TriggerSet->SetSlotDataAssetForTest(4,
            TSoftObjectPtr<UWTBRTriggerDataAsset>(
                MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeReadyTestRegistry(CompositeCooldown, MergeTime));
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    void CompleteReadyTestMerge(
        UWTBRTriggerSetComponent* TriggerSet, float CompositeCooldown = 0.0f, float MergeTime = 0.5f)
    {
        StartReadyTestMerge(TriggerSet, CompositeCooldown, MergeTime);
        TriggerSet->TriggerMergeCompleteForTest();
    }

    void TickReadyTestWorldForCooldown(UWorld* World, float TotalSeconds, float StepSeconds = 0.05f)
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

    AWTBRCharacter* GetReadyTestCharacter(UWTBRTriggerSetComponent* TriggerSet)
    {
        return TriggerSet ? Cast<AWTBRCharacter>(TriggerSet->GetOwner()) : nullptr;
    }

    int32 CountReadyTestProjectiles(UWorld* World)
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

    void StartReadyTestMergeWithNormalFulgrixRuntimeTrigger(
        UWTBRTriggerSetComponent* TriggerSet, bool bInstallOnMain)
    {
        AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
        if (!TriggerSet || !Character) return;

        UWTBRTriggerDataAsset* MainDataAsset =
            MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Solux);
        UWTBRTriggerDataAsset* SubDataAsset =
            MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix);
        UWTBRTriggerDataAsset* RuntimeDataAsset = bInstallOnMain ? MainDataAsset : SubDataAsset;
        RuntimeDataAsset->VaelCostPerUse = 10.0f;
        RuntimeDataAsset->FulgrixParams.FulgrixProjectileClass = AWTBRProjectileBase::StaticClass();

        TriggerSet->SetSlotDataAssetForTest(0, TSoftObjectPtr<UWTBRTriggerDataAsset>(MainDataAsset));
        TriggerSet->SetSlotDataAssetForTest(4, TSoftObjectPtr<UWTBRTriggerDataAsset>(SubDataAsset));
        TriggerSet->CompositeRegistryAsset =
            TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeReadyTestRegistry());

        UWTBRFulgrixTrigger* RuntimeTrigger = NewObject<UWTBRFulgrixTrigger>(Character);
        RuntimeTrigger->InitializeTrigger(Character, RuntimeDataAsset);
        TriggerSet->InstallTriggerForTest(bInstallOnMain ? ETriggerSlot::Main1 : ETriggerSlot::Sub1, RuntimeTrigger);
        TriggerSet->Server_StartCompositeMerge_Implementation();
    }

    struct FWTBRCompositeReadyCooldownTimerTestState
    {
        FWTBRCompositeReadyWorldFixture Fixture{TEXT("WTBR_Ready_CooldownTimer")};
        TWeakObjectPtr<UWTBRTriggerSetComponent> TriggerSet;
        float ElapsedSeconds = 0.0f;
    };

    class FWTBRCompositeReadyCooldownExpiryLatentCommand : public IAutomationLatentCommand
    {
    public:
        FWTBRCompositeReadyCooldownExpiryLatentCommand(
            FAutomationTestBase* InTest,
            const TSharedRef<FWTBRCompositeReadyCooldownTimerTestState>& InState)
            : Test(InTest)
            , State(InState)
        {
        }

        virtual bool Update() override
        {
            UWTBRTriggerSetComponent* TriggerSet = State->TriggerSet.Get();
            UWorld* World = State->Fixture.GetWorld();
            if (!Test || !TriggerSet || !World)
            {
                if (Test) Test->AddError(TEXT("Cooldown timer test fixture became invalid before expiry."));
                return true;
            }

            constexpr float TickStepSeconds = 0.05f;
            TickReadyTestWorldForCooldown(World, TickStepSeconds, TickStepSeconds);
            State->ElapsedSeconds += TickStepSeconds;
            if (State->ElapsedSeconds < 0.2f)
            {
                return false;
            }

            Test->TestFalse(TEXT("Cooldown expiry clears replicated cooldown active"),
                TriggerSet->bCompositeCooldownActive);
            Test->TestFalse(TEXT("Cooldown expiry clears cooldown timer"),
                TriggerSet->IsCompositeCooldownActiveForTest());
            return true;
        }

    private:
        FAutomationTestBase* Test = nullptr;
        TSharedRef<FWTBRCompositeReadyCooldownTimerTestState> State;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMergeCompletionCreatesReadyCompositeTest,
    "WTBR.Composite.Ready.MergeCompletionCreatesReadyComposite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMergeCompletionCreatesReadyCompositeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_Completion"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    CompleteReadyTestMerge(TriggerSet, 2.0f);

    TestTrue(TEXT("Merge completion creates a ready composite"), TriggerSet->HasReadyComposite());
    TestEqual(TEXT("Ready type matches the completed recipe"),
        TriggerSet->GetReadyCompositeType(), EWTBRCompositeBulletType::Solgrix);
    TestEqual(TEXT("Merge completion commits Vael"), Character->VaelComponent->GetCurrentVael(), 80.0f);
    TestFalse(TEXT("Merge completion does not start cooldown"), TriggerSet->IsCompositeCooldownActiveForTest());
    TestFalse(TEXT("Merge completion leaves replicated cooldown inactive"), TriggerSet->bCompositeCooldownActive);
    TestEqual(TEXT("Merge state is clear after completion"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireReadyCompositeStartsCooldownTest,
    "WTBR.Composite.Ready.FireClearsReadyAndStartsCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireReadyCompositeStartsCooldownTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_Fire"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    CompleteReadyTestMerge(TriggerSet, 2.0f);
    TestTrue(TEXT("Ready composite fires"), TriggerSet->FireReadyComposite());
    TestFalse(TEXT("Fire clears ready state"), TriggerSet->HasReadyComposite());
    TestTrue(TEXT("Fire starts the configured cooldown"), TriggerSet->IsCompositeCooldownActiveForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRFireWithoutReadyCompositeIsSafeTest,
    "WTBR.Composite.Ready.FireWithoutReadyIsSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRFireWithoutReadyCompositeIsSafeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_NoFire"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    TestFalse(TEXT("Firing without a ready composite returns false"), TriggerSet->FireReadyComposite());
    TestFalse(TEXT("Firing without a ready composite starts no cooldown"),
        TriggerSet->IsCompositeCooldownActiveForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReadyCompositeBlocksNewMergeTest,
    "WTBR.Composite.Ready.ReadyBlocksNewMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReadyCompositeBlocksNewMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_Gate"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    CompleteReadyTestMerge(TriggerSet);
    TriggerSet->Server_StartCompositeMerge_Implementation();

    TestTrue(TEXT("Ready composite remains after a new merge request"), TriggerSet->HasReadyComposite());
    TestEqual(TEXT("Ready composite gate creates no new reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCycleMainDiscardsReadyCompositeWithoutRefundTest,
    "WTBR.Composite.Ready.CycleMainDiscardsWithoutRefund",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCycleMainDiscardsReadyCompositeWithoutRefundTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_CycleMain"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    CompleteReadyTestMerge(TriggerSet);
    TriggerSet->CycleMainSlot();

    TestFalse(TEXT("Cycling Main discards the ready composite"), TriggerSet->HasReadyComposite());
    TestEqual(TEXT("Cycling Main does not refund committed Vael"), Character->VaelComponent->GetCurrentVael(), 80.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBREndPlayDiscardsReadyCompositeTest,
    "WTBR.Composite.Ready.EndPlayDiscardsReadyComposite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBREndPlayDiscardsReadyCompositeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_EndPlay"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    CompleteReadyTestMerge(TriggerSet);
    TriggerSet->EndPlayForTest();

    TestFalse(TEXT("EndPlay discards the ready composite"), TriggerSet->HasReadyComposite());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRReadyCompositeFireInputInterceptsBothButtonsTest,
    "WTBR.Composite.Ready.FireInputInterceptsBothButtons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRReadyCompositeFireInputInterceptsBothButtonsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_Input"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character) return false;

    CompleteReadyTestMerge(TriggerSet);
    Character->ExecuteBotTriggerInput(true, true);
    TestFalse(TEXT("Main fire press consumes the ready composite"), TriggerSet->HasReadyComposite());
    TestEqual(TEXT("Ready fire does not begin a new merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);

    CompleteReadyTestMerge(TriggerSet);
    Character->ExecuteBotTriggerInput(false, true);
    TestFalse(TEXT("Sub fire press consumes the ready composite"), TriggerSet->HasReadyComposite());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRMainFireCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Ready.MainFireCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRMainFireCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_MainFireCancelsMerge"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartReadyTestMergeWithNormalFulgrixRuntimeTrigger(TriggerSet, true);
    TestEqual(TEXT("Main-fire fixture begins an active merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    Character->ExecuteBotTriggerInput(true, true);

    TestEqual(TEXT("Main fire cancels the active merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Main fire cancellation releases the reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    TestEqual(TEXT("Main fire cancellation does not spend normal-trigger Vael"),
        Character->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Main fire cancellation produces no normal projectile"),
        CountReadyTestProjectiles(Fixture.GetWorld()), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSubFireCancelsActiveCompositeMergeTest,
    "WTBR.Composite.Ready.SubFireCancelsActiveMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSubFireCancelsActiveCompositeMergeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_SubFireCancelsMerge"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->VaelComponent) return false;

    StartReadyTestMergeWithNormalFulgrixRuntimeTrigger(TriggerSet, false);
    TestEqual(TEXT("Sub-fire fixture begins an active merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::Solgrix);
    Character->ExecuteBotTriggerInput(false, true);

    TestEqual(TEXT("Sub fire cancels the active merge"),
        TriggerSet->GetCurrentMergeState(), EWTBRCompositeBulletType::None);
    TestEqual(TEXT("Sub fire cancellation releases the reservation"),
        Character->VaelComponent->GetAvailableVael(), Character->VaelComponent->GetCurrentVael());
    TestEqual(TEXT("Sub fire cancellation does not spend normal-trigger Vael"),
        Character->VaelComponent->GetCurrentVael(), 100.0f);
    TestEqual(TEXT("Sub fire cancellation produces no normal projectile"),
        CountReadyTestProjectiles(Fixture.GetWorld()), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRDamageDoesNotDiscardReadyCompositeTest,
    "WTBR.Composite.Ready.DamageDoesNotDiscardReadyComposite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRDamageDoesNotDiscardReadyCompositeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_Damage"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;
    AWTBRCharacter* Character = GetReadyTestCharacter(TriggerSet);
    if (!Character || !Character->HealthComponent) return false;

    CompleteReadyTestMerge(TriggerSet);
    Character->HealthComponent->ApplyDamage(1.0f);

    TestTrue(TEXT("Damage does not discard the committed ready composite"), TriggerSet->HasReadyComposite());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeMergeDurationReplicatesAtStartTest,
    "WTBR.Composite.Ready.MergeDurationReplicatesAtStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeMergeDurationReplicatesAtStartTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_MergeDuration"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    StartReadyTestMerge(TriggerSet, 0.0f, 0.75f);

    TestEqual(TEXT("Replicated merge duration matches the recipe MergeTime"),
        TriggerSet->CompositeMergeDuration, 0.75f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCompositeCooldownReplicatedStateTracksTimerTest,
    "WTBR.Composite.Ready.CooldownReplicatedStateTracksTimer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCompositeCooldownReplicatedStateTracksTimerTest::RunTest(const FString& /*Parameters*/)
{
    const TSharedRef<FWTBRCompositeReadyCooldownTimerTestState> State =
        MakeShared<FWTBRCompositeReadyCooldownTimerTestState>();
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(State->Fixture, *this);
    if (!TriggerSet) return false;

    CompleteReadyTestMerge(TriggerSet, 0.1f, 2.0f);
    TestFalse(TEXT("Ready but unfired composite has no replicated cooldown"), TriggerSet->bCompositeCooldownActive);
    TestFalse(TEXT("Ready but unfired composite has no active cooldown timer"),
        TriggerSet->IsCompositeCooldownActiveForTest());

    TestTrue(TEXT("Ready composite fires"), TriggerSet->FireReadyComposite());
    TestTrue(TEXT("Firing sets replicated cooldown active"), TriggerSet->bCompositeCooldownActive);
    TestTrue(TEXT("Firing starts cooldown timer"), TriggerSet->IsCompositeCooldownActiveForTest());

    State->TriggerSet = TriggerSet;
    ADD_LATENT_AUTOMATION_COMMAND(FWTBRCompositeReadyCooldownExpiryLatentCommand(this, State));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRClearCompositeCooldownClearsReplicatedStateTest,
    "WTBR.Composite.Ready.ClearCooldownClearsReplicatedState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRClearCompositeCooldownClearsReplicatedStateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_ClearCooldown"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    CompleteReadyTestMerge(TriggerSet, 2.0f);
    TestTrue(TEXT("Ready composite fires"), TriggerSet->FireReadyComposite());
    TestTrue(TEXT("Precondition: replicated cooldown is active"), TriggerSet->bCompositeCooldownActive);
    TestTrue(TEXT("Precondition: cooldown timer is active"), TriggerSet->IsCompositeCooldownActiveForTest());

    TriggerSet->ClearCompositeCooldownForMatchRestart();

    TestFalse(TEXT("Match restart clear resets replicated cooldown state"), TriggerSet->bCompositeCooldownActive);
    TestFalse(TEXT("Match restart clear resets cooldown timer"), TriggerSet->IsCompositeCooldownActiveForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCanStartMergeMirrorsClientVisibleGatesTest,
    "WTBR.Composite.Ready.CanStartMergeMirrorsClientVisibleGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCanStartMergeMirrorsClientVisibleGatesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCompositeReadyWorldFixture Fixture(TEXT("WTBR_Ready_CanStart"));
    UWTBRTriggerSetComponent* TriggerSet = MakeReadyTestTriggerSet(Fixture, *this);
    if (!TriggerSet) return false;

    TriggerSet->SetSlotDataAssetForTest(0,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Solux)));
    TriggerSet->SetSlotDataAssetForTest(4,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Fulgrix)));
    TriggerSet->CompositeRegistryAsset =
        TSoftObjectPtr<UWTBRCompositeRegistryDataAsset>(MakeReadyTestRegistry(2.0f));

    TestTrue(TEXT("Combinable loaded slots can start a merge"), TriggerSet->CanStartMerge());

    TriggerSet->Server_StartCompositeMerge_Implementation();
    TestFalse(TEXT("An active merge blocks CanStartMerge"), TriggerSet->CanStartMerge());

    TriggerSet->TriggerMergeCompleteForTest();
    TestFalse(TEXT("A ready composite blocks CanStartMerge"), TriggerSet->CanStartMerge());

    TestTrue(TEXT("Ready composite fires"), TriggerSet->FireReadyComposite());
    TestFalse(TEXT("An active cooldown blocks CanStartMerge"), TriggerSet->CanStartMerge());

    TriggerSet->ClearCompositeCooldownForMatchRestart();
    TriggerSet->SetSlotDataAssetForTest(4,
        TSoftObjectPtr<UWTBRTriggerDataAsset>(
            MakeReadyTestArchetypeDataAsset(EWTBRBulletArchetype::Venyx)));
    TestFalse(TEXT("A non-resolving archetype pair blocks CanStartMerge"), TriggerSet->CanStartMerge());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
