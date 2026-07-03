// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Components/WTBRHealthComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// BR Ground Item + Pickup Automation Tests (WTBR.Inventory.GroundItem / .Pickup)
//
// Code-only. Actor-level tests exercise the AWTBRGroundItemActor consumed-claim
// model. Pickup tests drive AWTBRCharacter::Server_RequestPickupGroundItem_Implementation
// directly on an authority character in a transient (EWorldType::Game) world, with
// an in-match AWTBRGameState spawned into the world (it self-registers as the world
// GameState via PostInitializeComponents). No PIE camera/collision, no Blueprint
// assets, no .uasset dependency.
//
// Not covered here (reported): non-authority pickup (character always holds
// authority in the fixture — the component-level non-authority contract is proven
// in WTBRInventoryAutomationTests) and the alive/dead gate (no safe headless
// dead-state seam without asset-backed health). Both are deferred to the Human
// Test Gate / later multiplayer coverage.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRPickupWorldFixture
    {
    public:
        explicit FWTBRPickupWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRPickupWorldFixture()
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

    UWTBRItemDataAsset* MakeItem(int32 MaxStack)
    {
        UWTBRItemDataAsset* Item = NewObject<UWTBRItemDataAsset>(GetTransientPackage());
        Item->MaxStackSize = MaxStack;
        Item->ItemType = EWTBRItemType::Consumable;
        return Item;
    }

    AWTBRGroundItemActor* SpawnGroundItem(
        UWorld* World, const TSoftObjectPtr<UWTBRItemDataAsset>& Item, int32 Quantity, const FVector& Loc)
    {
        if (!World) return nullptr;
        AWTBRGroundItemActor* GroundItem = World->SpawnActor<AWTBRGroundItemActor>(
            AWTBRGroundItemActor::StaticClass(), Loc, FRotator::ZeroRotator);
        if (GroundItem)
        {
            GroundItem->InitializeGroundItem(Item, Quantity);
        }
        return GroundItem;
    }

    AWTBRGameState* SpawnInMatchGameState(UWorld* World)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (GameState)
        {
            // A spawned GameState does not auto-register as the world GameState in a
            // transient (GameMode-less) fixture, so register it explicitly. This is
            // what World->GetGameState<AWTBRGameState>() reads inside the pickup RPC.
            World->SetGameState(GameState);
            GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
        }
        return GameState;
    }

    AWTBRCharacter* SpawnCharacter(UWorld* World, const FVector& Loc)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), Loc, FRotator::ZeroRotator, Params);
    }

    int32 CountFilledSlots(const UWTBRInventoryComponent* Comp)
    {
        int32 Filled = 0;
        for (const FWTBRInventorySlot& Slot : Comp->GetInventorySlots())
        {
            if (!Slot.IsEmpty())
            {
                ++Filled;
            }
        }
        return Filled;
    }
}

// ── Actor-level: consumed-claim model ───────────────────────────────────────

// A1 — Initialize sets item/quantity and starts un-consumed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGroundItemInitializeTest,
    "WTBR.Inventory.GroundItem.InitializeAndGetters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGroundItemInitializeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_GI_Init"));
    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        Fixture.GetWorld(), TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 3, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    TestEqual(TEXT("Quantity is 3"), GroundItem->GetQuantity(), 3);
    TestFalse(TEXT("ItemData is set"), GroundItem->GetItemData().IsNull());
    TestFalse(TEXT("Starts un-consumed"), GroundItem->IsConsumed());
    return true;
}

// A2 — TryMarkConsumed succeeds once, then fails (cannot claim twice).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGroundItemClaimOnceTest,
    "WTBR.Inventory.GroundItem.TryMarkConsumedOnceThenFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGroundItemClaimOnceTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_GI_ClaimOnce"));
    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        Fixture.GetWorld(), TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 1, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    TestTrue(TEXT("First claim succeeds"), GroundItem->TryMarkConsumed());
    TestFalse(TEXT("Second claim fails"), GroundItem->TryMarkConsumed());
    TestTrue(TEXT("Reports consumed"), GroundItem->IsConsumed());
    return true;
}

// A3 — ClearConsumedForFailedPickup rolls the claim back to available.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRGroundItemRollbackTest,
    "WTBR.Inventory.GroundItem.ClearConsumedRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRGroundItemRollbackTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_GI_Rollback"));
    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        Fixture.GetWorld(), TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 1, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    TestTrue(TEXT("Claim"), GroundItem->TryMarkConsumed());
    GroundItem->ClearConsumedForFailedPickup();
    TestFalse(TEXT("Rolled back to available"), GroundItem->IsConsumed());
    TestTrue(TEXT("Can be claimed again after rollback"), GroundItem->TryMarkConsumed());
    return true;
}

// ── Character RPC: Server_RequestPickupGroundItem ───────────────────────────

// P1 — Valid pickup adds to inventory and destroys the ground item.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupValidAddsAndConsumesTest,
    "WTBR.Inventory.Pickup.ValidAddsAndConsumes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupValidAddsAndConsumesTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_Valid"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;
    TestTrue(TEXT("GameState registered as world GameState"), World->GetGameState<AWTBRGameState>() == GameState);

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 2, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("One inventory slot filled"), CountFilledSlots(Character->InventoryComponent), 1);
    TestEqual(TEXT("Slot 0 holds quantity 2"), Character->InventoryComponent->GetInventorySlots()[0].Quantity, 2);
    TestTrue(TEXT("Slot 0 identity matches item"),
        Character->InventoryComponent->GetInventorySlots()[0].ItemData.Get() == Item.Get());
    TestFalse(TEXT("Ground item destroyed on success"), IsValid(GroundItem));
    return true;
}

// P2 — Full inventory rejects the pickup and leaves the ground item available.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupFullInventoryRejectsTest,
    "WTBR.Inventory.Pickup.FullInventoryRejectsAndLeavesAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupFullInventoryRejectsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_Full"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;

    // Single-slot inventory, stack limit 2 → capacity 2; pre-fill it completely.
    Character->InventoryComponent->InventorySlotCount = 1;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(2));
    TestTrue(TEXT("Pre-fill single slot"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));

    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 2, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestFalse(TEXT("Ground item not consumed on full inventory"), GroundItem->IsConsumed());
    TestTrue(TEXT("Ground item still available"), IsValid(GroundItem));
    TestEqual(TEXT("Inventory unchanged: still 1 slot"), CountFilledSlots(Character->InventoryComponent), 1);
    TestEqual(TEXT("Slot 0 unchanged at 2"), Character->InventoryComponent->GetInventorySlots()[0].Quantity, 2);
    return true;
}

// P3 — Already-consumed ground item cannot be picked up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupAlreadyConsumedTest,
    "WTBR.Inventory.Pickup.AlreadyConsumedCannotPickTwice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupAlreadyConsumedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_Consumed"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 2, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    TestTrue(TEXT("Pre-claim the ground item"), GroundItem->TryMarkConsumed());

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("Nothing added"), CountFilledSlots(Character->InventoryComponent), 0);
    TestTrue(TEXT("Ground item remains consumed"), GroundItem->IsConsumed());
    TestTrue(TEXT("Ground item not destroyed"), IsValid(GroundItem));
    return true;
}

// P4 — Null ground item is rejected without crashing or mutating.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupNullGroundItemTest,
    "WTBR.Inventory.Pickup.NullGroundItemRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupNullGroundItemTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_Null"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    Character->Server_RequestPickupGroundItem_Implementation(nullptr);

    TestEqual(TEXT("Nothing added on null pickup"), CountFilledSlots(Character->InventoryComponent), 0);
    return true;
}

// P5 — Ground item with null item data is rejected and left available.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupNullItemDataTest,
    "WTBR.Inventory.Pickup.NullItemDataRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupNullItemDataTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_NullData"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    // Initialize with a null item reference (quantity valid).
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(), 1, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("Nothing added on null item data"), CountFilledSlots(Character->InventoryComponent), 0);
    TestFalse(TEXT("Ground item not consumed"), GroundItem->IsConsumed());
    TestTrue(TEXT("Ground item still available"), IsValid(GroundItem));
    return true;
}

// P6 — Non-positive quantity is rejected and the ground item is left available.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupNonPositiveQuantityTest,
    "WTBR.Inventory.Pickup.NonPositiveQuantityRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupNonPositiveQuantityTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_ZeroQty"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 0, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("Nothing added on zero quantity"), CountFilledSlots(Character->InventoryComponent), 0);
    TestFalse(TEXT("Ground item not consumed"), GroundItem->IsConsumed());
    TestTrue(TEXT("Ground item still available"), IsValid(GroundItem));
    return true;
}

// P7 — Distance gate rejects a too-far ground item and leaves it available.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupDistanceGateTest,
    "WTBR.Inventory.Pickup.DistanceGateRejectsTooFar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupDistanceGateTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_Distance"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    // Well beyond the 300-unit pickup range.
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 2, FVector(10000.0f, 0.0f, 0.0f));
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("Nothing added when too far"), CountFilledSlots(Character->InventoryComponent), 0);
    TestFalse(TEXT("Ground item not consumed when too far"), GroundItem->IsConsumed());
    TestTrue(TEXT("Ground item still available when too far"), IsValid(GroundItem));
    return true;
}

// P8 — Ground item pickup uses inventory only and does not touch the trigger set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRPickupDoesNotMutateTriggerSetTest,
    "WTBR.Inventory.Pickup.DoesNotMutateDroppedTriggerPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRPickupDoesNotMutateTriggerSetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPickupWorldFixture Fixture(TEXT("WTBR_Pickup_NoTriggerMutate"));
    UWorld* World = Fixture.GetWorld();
    AWTBRGameState* GameState = SpawnInMatchGameState(World);
    TestNotNull(TEXT("GameState spawns"), GameState);
    if (!GameState) return false;

    AWTBRCharacter* Character = SpawnCharacter(World, FVector::ZeroVector);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->TriggerSetComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    const int32 MainIndexBefore = Character->TriggerSetComponent->GetActiveMainIndex();
    const int32 SubIndexBefore = Character->TriggerSetComponent->GetActiveSubIndex();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));
    AWTBRGroundItemActor* GroundItem = SpawnGroundItem(
        World, TSoftObjectPtr<UWTBRItemDataAsset>(Item.Get()), 2, FVector::ZeroVector);
    TestNotNull(TEXT("Ground item spawns"), GroundItem);
    if (!GroundItem) return false;

    Character->Server_RequestPickupGroundItem_Implementation(GroundItem);

    TestEqual(TEXT("Item went to inventory"), CountFilledSlots(Character->InventoryComponent), 1);
    TestEqual(TEXT("Active main slot index unchanged"),
        Character->TriggerSetComponent->GetActiveMainIndex(), MainIndexBefore);
    TestEqual(TEXT("Active sub slot index unchanged"),
        Character->TriggerSetComponent->GetActiveSubIndex(), SubIndexBefore);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
