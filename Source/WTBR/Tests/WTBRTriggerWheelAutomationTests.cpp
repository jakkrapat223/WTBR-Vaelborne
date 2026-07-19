// Copyright Vaelborne: Dominion Project. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/WTBRTriggerWheelWidget.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

// ═════════════════════════════════════════════════════════════════════════════
// Trigger selection wheel (Q/E hold)
//
// The wheel is native (UWTBRTriggerWheelWidget) because it is drawn geometry, not
// layout. What is worth pinning down in automation is the angle→slot mapping and the
// dead zone: those are pure maths, they decide which Trigger the player ends up
// holding, and getting them subtly wrong is invisible until someone switches to the
// wrong Trigger mid-fight.
//
// Painting and mouse accumulation need a real viewport and are left to PIE.
// ═════════════════════════════════════════════════════════════════════════════

namespace
{
    class FWTBRWheelWorldFixture
    {
    public:
        explicit FWTBRWheelWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRWheelWorldFixture()
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

    UWTBRTriggerWheelWidget* WheelTest_MakeWheel(UWorld* World)
    {
        if (!World) return nullptr;
        return NewObject<UWTBRTriggerWheelWidget>(World);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRWheelOpenCloseTest,
    "WTBR.UI.TriggerWheel.OpenAndCloseTracksState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRWheelOpenCloseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRWheelWorldFixture Fixture(TEXT("WTBR_Wheel_OpenClose"));
    UWTBRTriggerWheelWidget* Wheel = WheelTest_MakeWheel(Fixture.GetWorld());
    if (!Wheel) return false;

    TestFalse(TEXT("Wheel starts closed"), Wheel->IsWheelOpen());

    Wheel->OpenWheel(true);
    TestTrue(TEXT("Wheel is open after OpenWheel"), Wheel->IsWheelOpen());

    // Opening must not pre-select anything — a hold with no flick has to be a no-op,
    // otherwise merely tapping too slowly would switch Trigger.
    TestEqual(TEXT("Nothing is highlighted immediately after opening"),
        Wheel->GetHighlightedSlotIndex(), static_cast<int32>(INDEX_NONE));

    Wheel->CloseWheel();
    TestFalse(TEXT("Wheel is closed after CloseWheel"), Wheel->IsWheelOpen());
    TestEqual(TEXT("Nothing is highlighted while closed"),
        Wheel->GetHighlightedSlotIndex(), static_cast<int32>(INDEX_NONE));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRWheelServerRejectsCrossSetSlotTest,
    "WTBR.UI.TriggerWheel.ServerRejectsWrongSetSlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRWheelServerRejectsCrossSetSlotTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRWheelWorldFixture Fixture(TEXT("WTBR_Wheel_ServerValidate"));
    UWorld* World = Fixture.GetWorld();
    if (!World) return false;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRCharacter* Char = World->SpawnActor<AWTBRCharacter>(
        AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Char || !Char->TriggerSetComponent) return false;

    const int32 StartingMainIndex = Char->TriggerSetComponent->GetActiveMainIndex();

    // A Sub-set slot index sent as a Main-set selection must be refused: the client
    // picks the index, so the server cannot assume it belongs to the set that was
    // actually held.
    Char->Server_SelectTriggerSlot_Implementation(
        /*bIsMain=*/true, UWTBRTriggerSetComponent::MainSlotCount);
    TestEqual(TEXT("Main index unchanged after a cross-set request"),
        Char->TriggerSetComponent->GetActiveMainIndex(), StartingMainIndex);

    // Out-of-range indices must be refused too.
    Char->Server_SelectTriggerSlot_Implementation(/*bIsMain=*/true, 999);
    Char->Server_SelectTriggerSlot_Implementation(/*bIsMain=*/true, -1);
    TestEqual(TEXT("Main index unchanged after out-of-range requests"),
        Char->TriggerSetComponent->GetActiveMainIndex(), StartingMainIndex);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRWheelEmptySlotNotSelectableTest,
    "WTBR.UI.TriggerWheel.EmptySlotIsNotSelectable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRWheelEmptySlotNotSelectableTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRWheelWorldFixture Fixture(TEXT("WTBR_Wheel_EmptySlot"));
    UWorld* World = Fixture.GetWorld();
    if (!World) return false;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRCharacter* Char = World->SpawnActor<AWTBRCharacter>(
        AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Char || !Char->TriggerSetComponent) return false;

    UWTBRTriggerSetComponent* TSC = Char->TriggerSetComponent;
    const int32 StartingMainIndex = TSC->GetActiveMainIndex();

    // A default-constructed loadout has no DataAssets assigned, so every slot reports
    // unoccupied — switching to one would leave the player holding nothing.
    for (int32 SlotIndex = 0; SlotIndex < UWTBRTriggerSetComponent::MainSlotCount; ++SlotIndex)
    {
        if (!TSC->IsSlotOccupied(SlotIndex))
        {
            Char->Server_SelectTriggerSlot_Implementation(/*bIsMain=*/true, SlotIndex);
        }
    }

    TestEqual(TEXT("Empty slots never become the active Main slot"),
        TSC->GetActiveMainIndex(), StartingMainIndex);

    // And an unoccupied slot has no name to draw, so the wheel leaves that segment blank.
    if (!TSC->IsSlotOccupied(0))
    {
        TestTrue(TEXT("Unoccupied slot has no display name"),
            TSC->GetSlotDisplayName(0).IsEmpty());
    }
    TestTrue(TEXT("Out-of-range slot has no display name"),
        TSC->GetSlotDisplayName(999).IsEmpty());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
