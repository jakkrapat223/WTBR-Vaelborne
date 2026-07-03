// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Inventory/WTBRItemDataAsset.h"

// -----------------------------------------------------------------------------
// Inventory Component Automation Tests (WTBR.Inventory)
//
// Covers UWTBRInventoryComponent server-authoritative TryAddItem transactional
// stacking. Code-only: transient UWorld fixture, transient item DataAssets, no
// map/Blueprint/WBP/UMG/.uasset dependency, no PIE camera/collision.
//
// Authority mutate-path tests spawn a plain AActor in a standalone (EWorldType::Game)
// transient world — such actors report ROLE_Authority — and attach the component
// to it. The non-authority test uses a component with no owner (transient package),
// which must never mutate. The runtime authority rule is NOT weakened for tests.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRInventoryWorldFixture
    {
    public:
        explicit FWTBRInventoryWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRInventoryWorldFixture()
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

    // Spawns an authority-capable owner actor in the fixture world and attaches a
    // registered inventory component with the requested slot count.
    UWTBRInventoryComponent* MakeAuthorityInventory(UWorld* World, int32 SlotCount)
    {
        if (!World)
        {
            return nullptr;
        }

        AActor* Owner = World->SpawnActor<AActor>(
            AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (!Owner)
        {
            return nullptr;
        }

        UWTBRInventoryComponent* Comp = NewObject<UWTBRInventoryComponent>(Owner);
        if (!Comp)
        {
            return nullptr;
        }

        Comp->InventorySlotCount = SlotCount;
        Comp->RegisterComponent();
        return Comp;
    }

    // Transient item DataAsset kept alive by the caller via TStrongObjectPtr.
    UWTBRItemDataAsset* MakeItem(int32 MaxStack)
    {
        UWTBRItemDataAsset* Item = NewObject<UWTBRItemDataAsset>(GetTransientPackage());
        Item->MaxStackSize = MaxStack;
        Item->ItemType = EWTBRItemType::Consumable;
        return Item;
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

    bool AnySlotExceedsLimit(const UWTBRInventoryComponent* Comp, int32 Limit)
    {
        for (const FWTBRInventorySlot& Slot : Comp->GetInventorySlots())
        {
            if (Slot.Quantity > Limit)
            {
                return true;
            }
        }
        return false;
    }
}

// 1 — Null item is rejected and the inventory is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryNullItemRejectedTest,
    "WTBR.Inventory.NullItemRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryNullItemRejectedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_NullItem"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    Comp->InitializeServerInventory();

    TestFalse(TEXT("TryAddItem(nullptr) is rejected"), Comp->TryAddItem(nullptr, 1));
    TestEqual(TEXT("No slots filled after null add"), CountFilledSlots(Comp), 0);
    return true;
}

// 2 — Non-positive quantity is rejected and the inventory is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryNonPositiveQuantityRejectedTest,
    "WTBR.Inventory.NonPositiveQuantityRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryNonPositiveQuantityRejectedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_NonPositive"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    TestFalse(TEXT("Quantity 0 rejected"), Comp->TryAddItem(Item.Get(), 0));
    TestFalse(TEXT("Quantity -3 rejected"), Comp->TryAddItem(Item.Get(), -3));
    TestEqual(TEXT("No slots filled after non-positive add"), CountFilledSlots(Comp), 0);
    return true;
}

// 3 — Small item stack limit (4) is respected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventorySmallStackLimitTest,
    "WTBR.Inventory.SmallStackLimitRespected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventorySmallStackLimitTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_SmallStack"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    TestTrue(TEXT("Add 4 of a stack-4 item succeeds"), Comp->TryAddItem(Item.Get(), 4));
    TestEqual(TEXT("Single slot filled"), CountFilledSlots(Comp), 1);
    TestEqual(TEXT("Slot 0 holds full stack of 4"), Comp->GetInventorySlots()[0].Quantity, 4);
    TestFalse(TEXT("No slot exceeds limit 4"), AnySlotExceedsLimit(Comp, 4));
    return true;
}

// 4 — Large item stack limit (2) is respected.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryLargeStackLimitTest,
    "WTBR.Inventory.LargeStackLimitRespected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryLargeStackLimitTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_LargeStack"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(2));

    TestTrue(TEXT("Add 2 of a stack-2 item succeeds"), Comp->TryAddItem(Item.Get(), 2));
    TestEqual(TEXT("Single slot filled"), CountFilledSlots(Comp), 1);
    TestEqual(TEXT("Slot 0 holds full stack of 2"), Comp->GetInventorySlots()[0].Quantity, 2);
    TestFalse(TEXT("No slot exceeds limit 2"), AnySlotExceedsLimit(Comp, 2));
    return true;
}

// 5 — Existing partial stack is filled before an empty slot is used.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryPartialBeforeEmptyTest,
    "WTBR.Inventory.PartialStackFilledBeforeEmptySlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryPartialBeforeEmptyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_PartialFirst"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    TestTrue(TEXT("Seed partial stack of 2"), Comp->TryAddItem(Item.Get(), 2));
    TestEqual(TEXT("Slot 0 partial at 2"), Comp->GetInventorySlots()[0].Quantity, 2);

    // Adding 3 should top slot 0 to 4 (adds 2), then spill 1 into slot 1.
    TestTrue(TEXT("Add 3 more succeeds"), Comp->TryAddItem(Item.Get(), 3));
    TestEqual(TEXT("Slot 0 filled to limit 4"), Comp->GetInventorySlots()[0].Quantity, 4);
    TestEqual(TEXT("Slot 1 holds spillover 1"), Comp->GetInventorySlots()[1].Quantity, 1);
    TestEqual(TEXT("Exactly two slots filled"), CountFilledSlots(Comp), 2);
    return true;
}

// 6 — Overflow beyond one stack goes into the first empty slot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryOverflowIntoEmptyTest,
    "WTBR.Inventory.OverflowIntoFirstEmptySlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryOverflowIntoEmptyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_Overflow"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    // 6 units into empty inventory: slot 0 = 4 (full), slot 1 = 2.
    TestTrue(TEXT("Add 6 succeeds"), Comp->TryAddItem(Item.Get(), 6));
    TestEqual(TEXT("Slot 0 full at 4"), Comp->GetInventorySlots()[0].Quantity, 4);
    TestEqual(TEXT("Slot 1 holds overflow 2"), Comp->GetInventorySlots()[1].Quantity, 2);
    TestEqual(TEXT("Exactly two slots filled"), CountFilledSlots(Comp), 2);
    return true;
}

// 7 — Full-inventory reject is all-or-nothing and mutates nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryFullRejectAllOrNothingTest,
    "WTBR.Inventory.FullInventoryRejectAllOrNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryFullRejectAllOrNothingTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_FullReject"));
    // Two slots, stack limit 4 → total capacity 8.
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 2);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    // Fill to slot 0 = 4, slot 1 = 3 (only 1 free unit remaining).
    TestTrue(TEXT("Add 7 succeeds"), Comp->TryAddItem(Item.Get(), 7));
    TestEqual(TEXT("Slot 0 at 4"), Comp->GetInventorySlots()[0].Quantity, 4);
    TestEqual(TEXT("Slot 1 at 3"), Comp->GetInventorySlots()[1].Quantity, 3);

    // Requesting 3 units when only 1 fits must reject entirely and not touch slot 1.
    TestFalse(TEXT("Add 3 rejected (insufficient capacity)"), Comp->TryAddItem(Item.Get(), 3));
    TestEqual(TEXT("Slot 0 unchanged at 4"), Comp->GetInventorySlots()[0].Quantity, 4);
    TestEqual(TEXT("Slot 1 unchanged at 3 (all-or-nothing)"), Comp->GetInventorySlots()[1].Quantity, 3);
    return true;
}

// 8 — Different items do not stack together.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryDifferentItemsNoStackTest,
    "WTBR.Inventory.DifferentItemsDoNotStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryDifferentItemsNoStackTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_DiffItems"));
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> ItemA(MakeItem(4));
    TStrongObjectPtr<UWTBRItemDataAsset> ItemB(MakeItem(4));

    TestTrue(TEXT("Add 2 of item A"), Comp->TryAddItem(ItemA.Get(), 2));
    TestTrue(TEXT("Add 2 of item B"), Comp->TryAddItem(ItemB.Get(), 2));

    // A distinct item must not merge into A's partial stack; it takes a new slot.
    TestEqual(TEXT("Two distinct slots filled"), CountFilledSlots(Comp), 2);
    TestEqual(TEXT("Slot 0 is item A at 2"), Comp->GetInventorySlots()[0].Quantity, 2);
    TestEqual(TEXT("Slot 1 is item B at 2"), Comp->GetInventorySlots()[1].Quantity, 2);
    TestTrue(TEXT("Slot 0 identity is item A"), Comp->GetInventorySlots()[0].ItemData.Get() == ItemA.Get());
    TestTrue(TEXT("Slot 1 identity is item B"), Comp->GetInventorySlots()[1].ItemData.Get() == ItemB.Get());
    return true;
}

// 9 — Non-authority TryAddItem must not mutate (safe no-owner seam).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryNonAuthorityNoMutationTest,
    "WTBR.Inventory.NonAuthorityDoesNotMutate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryNonAuthorityNoMutationTest::RunTest(const FString& /*Parameters*/)
{
    // No owner → GetOwner() null → HasServerAuthority() false.
    UWTBRInventoryComponent* Comp =
        NewObject<UWTBRInventoryComponent>(GetTransientPackage());
    TestNotNull(TEXT("Inventory component created"), Comp);
    if (!Comp) return false;

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeItem(4));

    TestFalse(TEXT("TryAddItem without authority returns false"), Comp->TryAddItem(Item.Get(), 1));
    TestEqual(TEXT("No slots created without authority"), Comp->GetInventorySlots().Num(), 0);
    return true;
}

// 10 — Inventory initializes to the default 14 slots.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRInventoryDefaultSlotCountTest,
    "WTBR.Inventory.DefaultInitializesTo14Slots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRInventoryDefaultSlotCountTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRInventoryWorldFixture Fixture(TEXT("WTBR_Inv_DefaultCount"));
    // Default InventorySlotCount (14) — do not override.
    UWTBRInventoryComponent* Comp = MakeAuthorityInventory(Fixture.GetWorld(), 14);
    TestNotNull(TEXT("Authority inventory created"), Comp);
    if (!Comp) return false;

    Comp->InitializeServerInventory();

    TestEqual(TEXT("Inventory initializes to 14 slots"), Comp->GetInventorySlots().Num(), 14);
    TestEqual(TEXT("All 14 slots start empty"), CountFilledSlots(Comp), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
