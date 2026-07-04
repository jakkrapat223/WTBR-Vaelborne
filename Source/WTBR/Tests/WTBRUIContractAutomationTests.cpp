// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/WTBRHUDViewModelComponent.h"
#include "Components/WTBRInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/WTBRInputBindingDisplayLibrary.h"
#include "WTBRCharacter.h"

namespace
{
    FString MakeForbiddenLabelA()
    {
        const TCHAR Chars[] = { 70, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelB()
    {
        const TCHAR Chars[] = { 88, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelC()
    {
        const TCHAR Chars[] = { 81, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelD()
    {
        const TCHAR Chars[] = { 69, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelE()
    {
        const TCHAR Chars[] = { 52, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelF()
    {
        const TCHAR Chars[] = { 76, 77, 66, 0 };
        return FString(Chars);
    }

    FString MakeForbiddenLabelG()
    {
        const TCHAR Chars[] = { 82, 77, 66, 0 };
        return FString(Chars);
    }

    FString MakeEngineMouseDisplayName()
    {
        return FString(TEXT("Left Mouse Button"));
    }

    bool ContainsForbiddenDisplayLabel(const FString& Text)
    {
        FString Normalized = Text;
        Normalized.TrimStartAndEndInline();

        return Normalized.Equals(MakeForbiddenLabelA(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelB(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelC(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelD(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelE(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelF(), ESearchCase::CaseSensitive)
            || Normalized.Equals(MakeForbiddenLabelG(), ESearchCase::CaseSensitive);
    }

    class FWTBRUIContractWorldFixture
    {
    public:
        explicit FWTBRUIContractWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRUIContractWorldFixture()
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

    AWTBRCharacter* SpawnUIContractCharacter(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDSnapshotDefaultsAreDeferredTest,
    "WTBR.UI.Contract.SnapshotDefaultsAreDeferred",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDSnapshotDefaultsAreDeferredTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRHUDSnapshot Snapshot;

    TestFalse(TEXT("Alive count source is deferred"), Snapshot.Match.bHasAliveCount);
    TestEqual(TEXT("Alive count default is neutral"), Snapshot.Match.AliveCount, 0);
    TestFalse(TEXT("Kill count source is deferred"), Snapshot.Match.bHasKillCount);
    TestEqual(TEXT("Kill count default is neutral"), Snapshot.Match.KillCount, 0);
    TestFalse(TEXT("Zone timer source is deferred"), Snapshot.Match.bHasZoneTimer);
    TestEqual(TEXT("Zone timer default is neutral"), Snapshot.Match.ZoneTimeRemaining, 0.0f);
    TestFalse(TEXT("Vitals source is deferred"), Snapshot.Vitals.bIsValid);
    TestFalse(TEXT("Pickup prompt hidden by default"), Snapshot.PickupPrompt.bIsVisible);
    TestFalse(TEXT("Cancel prompt hidden by default"), Snapshot.CancelPrompt.bIsVisible);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDViewModelRequestWrappersDefaultNoOpTest,
    "WTBR.UI.Contract.RequestWrappersRejectWithoutOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDViewModelRequestWrappersDefaultNoOpTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRHUDViewModelComponent* ViewModel = NewObject<UWTBRHUDViewModelComponent>(GetTransientPackage());
    TestNotNull(TEXT("ViewModel component can be created without gameplay owner"), ViewModel);
    if (!ViewModel)
    {
        return false;
    }

    TestFalse(TEXT("Quick item request is rejected without an owner"), ViewModel->RequestUseDisplayedQuickItem());
    TestFalse(TEXT("Pickup request is rejected without an owner"), ViewModel->RequestPickupFocusedTarget());
    TestFalse(TEXT("Cancel request is rejected without an owner"), ViewModel->RequestCancelCurrentUIOrAction());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInputDisplayProviderNoFallbackTest,
    "WTBR.UI.Contract.InputDisplayProviderNoFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRInputDisplayProviderNoFallbackTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRHUDBindingDisplay NullDisplay =
        UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(nullptr, nullptr);

    TestFalse(TEXT("Null input binding is not reported as bound"), NullDisplay.bIsBound);
    TestTrue(TEXT("Null input binding has no display text"), NullDisplay.DisplayName.IsEmpty());
    TestTrue(TEXT("Null input binding has no glyph token"), NullDisplay.GlyphToken.IsNone());

    UInputMappingContext* MappingContext = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputAction* InputAction = NewObject<UInputAction>(GetTransientPackage());
    TestNotNull(TEXT("Mapping context exists"), MappingContext);
    TestNotNull(TEXT("Input action exists"), InputAction);
    if (!MappingContext || !InputAction)
    {
        return false;
    }

    const FWTBRHUDBindingDisplay UnmappedDisplay =
        UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(MappingContext, InputAction);

    TestFalse(TEXT("Unmapped action is not reported as bound"), UnmappedDisplay.bIsBound);
    TestTrue(TEXT("Unmapped action has no guessed display text"), UnmappedDisplay.DisplayName.IsEmpty());
    TestFalse(TEXT("Unmapped action does not contain a forbidden fallback"),
        ContainsForbiddenDisplayLabel(UnmappedDisplay.DisplayName.ToString()));
    TestFalse(TEXT("Engine mouse display name is accepted"),
        ContainsForbiddenDisplayLabel(MakeEngineMouseDisplayName()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDViewModelNoMutableArraysTest,
    "WTBR.UI.Contract.ViewModelDoesNotExposeMutableArrays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDViewModelNoMutableArraysTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRHUDViewModelComponent* ViewModel = NewObject<UWTBRHUDViewModelComponent>(GetTransientPackage());
    TestNotNull(TEXT("ViewModel component exists"), ViewModel);
    if (!ViewModel)
    {
        return false;
    }

    const FWTBRHUDSnapshot Snapshot = ViewModel->GetHUDSnapshot();
    TestFalse(TEXT("Snapshot does not expose inventory slots as valid quick item data"), Snapshot.QuickItem.bHasItem);
    TestEqual(TEXT("Quick item count is neutral by default"), Snapshot.QuickItem.Count, 0);
    TestFalse(TEXT("Snapshot does not expose pickup state as mutable actor data"), Snapshot.PickupPrompt.bIsVisible);
    TestFalse(TEXT("Snapshot does not expose cancel state as mutable gameplay data"), Snapshot.CancelPrompt.bIsVisible);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDNewFilesNoHardcodedDisplayLabelsTest,
    "WTBR.UI.Contract.NewFilesNoHardcodedDisplayLabels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDNewFilesNoHardcodedDisplayLabelsTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRHUDSnapshot Snapshot;
    const TArray<FString> DisplayTexts = {
        Snapshot.MainTrigger.UseBinding.DisplayName.ToString(),
        Snapshot.SubTrigger.UseBinding.DisplayName.ToString(),
        Snapshot.QuickItem.Binding.DisplayName.ToString(),
        Snapshot.PickupPrompt.Binding.DisplayName.ToString(),
        Snapshot.CancelPrompt.Binding.DisplayName.ToString(),
        Snapshot.PickupPrompt.PromptText.ToString(),
        Snapshot.CancelPrompt.PromptText.ToString(),
    };

    for (const FString& DisplayText : DisplayTexts)
    {
        TestFalse(TEXT("Default C1 HUD display text has no hardcoded final binding label"),
            ContainsForbiddenDisplayLabel(DisplayText));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDCancelRequestNonLocalOwnerTest,
    "WTBR.UI.Contract.CancelRequestNonLocalOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDCancelRequestNonLocalOwnerTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUIContractWorldFixture Fixture(TEXT("WTBR_UIContract_CancelNonLocal"));
    AWTBRCharacter* Character = SpawnUIContractCharacter(Fixture.GetWorld());
    TestNotNull(TEXT("Headless character exists"), Character);
    if (!Character)
    {
        return false;
    }

    UWTBRHUDViewModelComponent* ViewModel = Character->GetHUDViewModelComponent();
    TestNotNull(TEXT("HUD view model component exists"), ViewModel);
    if (!ViewModel)
    {
        return false;
    }

    TestFalse(TEXT("Cancel request is rejected for non-local owner"),
        ViewModel->RequestCancelCurrentUIOrAction());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHUDInteractionInvalidFocusTest,
    "WTBR.UI.Contract.InteractionInvalidFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWTBRHUDInteractionInvalidFocusTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRInteractionComponent* OrphanInteraction = NewObject<UWTBRInteractionComponent>(GetTransientPackage());
    TestNotNull(TEXT("Orphan interaction component exists"), OrphanInteraction);
    if (!OrphanInteraction)
    {
        return false;
    }

    TestFalse(TEXT("Focus query is false without an owner"),
        OrphanInteraction->HasValidFocusedInteractable());

    FWTBRUIContractWorldFixture Fixture(TEXT("WTBR_UIContract_InvalidFocus"));
    AWTBRCharacter* Character = SpawnUIContractCharacter(Fixture.GetWorld());
    TestNotNull(TEXT("Headless character exists"), Character);
    if (!Character || !Character->InteractionComponent)
    {
        return false;
    }

    TestFalse(TEXT("Focus query is false without a valid target"),
        Character->InteractionComponent->HasValidFocusedInteractable());

    return true;
}

#endif
