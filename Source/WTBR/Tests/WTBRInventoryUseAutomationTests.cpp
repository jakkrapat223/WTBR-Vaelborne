// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"

// -----------------------------------------------------------------------------
// Inventory Item Use Automation Tests (WTBR.Inventory.Use)
//
// Code-only. RestoreHP tests attach a UWTBRHealthComponent to an authority actor
// in a transient world. Item-use tests drive
// AWTBRCharacter::Server_RequestUseInventoryItem_Implementation directly on an
// authority character with an in-match GameState. No PIE camera/collision, no
// Blueprint assets, no .uasset dependency.
//
// Without a CoreStatsAsset the health/Vael components fall back to MaxHP=300 and
// MaxVael=1000, and a freshly spawned character has CurrentHP=300 (full) and
// CurrentVael=0 (BeginPlay, which would set it to max, does not run in the fixture).
//
// Not covered here (reported): the not-Alive (Downed/Eliminated) branch of
// RestoreHP — forcing a valid non-Alive state in headless automation requires
// driving the death/downed machinery (stats, timers) and is brittle; it is
// deferred to the Human Test Gate. The alive-gate itself is present in code.
// -----------------------------------------------------------------------------

namespace
{
    class FWTBRUseWorldFixture
    {
    public:
        explicit FWTBRUseWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRUseWorldFixture()
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

    UWTBRItemDataAsset* MakeConsumable(int32 MaxStack, EWTBRConsumableEffectType Effect, float Magnitude)
    {
        UWTBRItemDataAsset* Item = NewObject<UWTBRItemDataAsset>(GetTransientPackage());
        Item->MaxStackSize = MaxStack;
        Item->ItemType = (Effect == EWTBRConsumableEffectType::RestoreVael)
            ? EWTBRItemType::VaelItem
            : EWTBRItemType::Consumable;
        Item->ConsumableEffectType = Effect;
        Item->EffectMagnitude = Magnitude;
        return Item;
    }

    UWTBRHealthComponent* MakeAuthorityHealth(UWorld* World)
    {
        if (!World) return nullptr;
        AActor* Owner = World->SpawnActor<AActor>(
            AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (!Owner) return nullptr;
        UWTBRHealthComponent* Health = NewObject<UWTBRHealthComponent>(Owner);
        if (!Health) return nullptr;
        Health->RegisterComponent();
        return Health;
    }

    AWTBRGameState* SpawnUseInMatchGameState(UWorld* World)
    {
        if (!World) return nullptr;
        AWTBRGameState* GameState = World->SpawnActor<AWTBRGameState>(AWTBRGameState::StaticClass());
        if (GameState)
        {
            World->SetGameState(GameState);
            GameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
        }
        return GameState;
    }

    AWTBRCharacter* SpawnUseCharacter(UWorld* World)
    {
        if (!World) return nullptr;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    int32 CountFilledUseSlots(const UWTBRInventoryComponent* Comp)
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

// ── HealthComponent::RestoreHP (component level) ────────────────────────────

// H1 — RestoreHP rejects non-positive amounts and does not change HP.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRestoreHPRejectsNonPositiveTest,
    "WTBR.Inventory.Use.RestoreHP_RejectsNonPositive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRestoreHPRejectsNonPositiveTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_HPNonPositive"));
    UWTBRHealthComponent* Health = MakeAuthorityHealth(Fixture.GetWorld());
    TestNotNull(TEXT("Authority health component"), Health);
    if (!Health) return false;

    Health->ApplyDamage(100.0f); // 300 -> 200 so a restore would have room
    const float HPBefore = Health->GetCurrentHP();

    TestFalse(TEXT("RestoreHP(0) rejected"), Health->RestoreHP(0.0f));
    TestFalse(TEXT("RestoreHP(-5) rejected"), Health->RestoreHP(-5.0f));
    TestEqual(TEXT("HP unchanged after rejected restores"), Health->GetCurrentHP(), HPBefore);
    return true;
}

// H2 — RestoreHP clamps to MaxHP and rejects when already full.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRestoreHPClampsToMaxTest,
    "WTBR.Inventory.Use.RestoreHP_ClampsToMax",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRestoreHPClampsToMaxTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_HPClamp"));
    UWTBRHealthComponent* Health = MakeAuthorityHealth(Fixture.GetWorld());
    TestNotNull(TEXT("Authority health component"), Health);
    if (!Health) return false;

    const float MaxHP = Health->GetMaxHP();
    Health->ApplyDamage(50.0f); // 300 -> 250

    TestTrue(TEXT("Over-restore succeeds"), Health->RestoreHP(1000.0f));
    TestEqual(TEXT("HP clamped to MaxHP"), Health->GetCurrentHP(), MaxHP);
    TestFalse(TEXT("RestoreHP at full is rejected (no overheal)"), Health->RestoreHP(10.0f));
    TestEqual(TEXT("HP still MaxHP"), Health->GetCurrentHP(), MaxHP);
    return true;
}

// H3 — RestoreHP restores a partial amount while damaged and alive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRRestoreHPPartialTest,
    "WTBR.Inventory.Use.RestoreHP_RestoresPartialWhenDamaged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRRestoreHPPartialTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_HPPartial"));
    UWTBRHealthComponent* Health = MakeAuthorityHealth(Fixture.GetWorld());
    TestNotNull(TEXT("Authority health component"), Health);
    if (!Health) return false;

    Health->ApplyDamage(100.0f); // 300 -> 200
    TestTrue(TEXT("Partial restore succeeds"), Health->RestoreHP(50.0f));
    TestEqual(TEXT("HP restored to 250"), Health->GetCurrentHP(), 250.0f);
    return true;
}

// ── Character RPC: Server_RequestUseInventoryItem ───────────────────────────

// U1 — HealHP item restores HP and consumes exactly one item.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseHealHPConsumesOneTest,
    "WTBR.Inventory.Use.HealHPRestoresAndConsumesOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseHealHPConsumesOneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_HealOne"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->HealthComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::HealHP, 50.0f));
    TestTrue(TEXT("Seed 2 HP items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));
    Character->HealthComponent->ApplyDamage(200.0f); // 300 -> 100

    Character->Server_RequestUseInventoryItem_Implementation(0);

    TestEqual(TEXT("HP restored to 150"), Character->HealthComponent->GetCurrentHP(), 150.0f);
    TestEqual(TEXT("Slot 0 decremented to 1"), Character->InventoryComponent->GetInventorySlots()[0].Quantity, 1);
    return true;
}

// U2 — HealHP item at full HP applies nothing and is not consumed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseHealHPFullNoConsumeTest,
    "WTBR.Inventory.Use.HealHPFullDoesNotConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseHealHPFullNoConsumeTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_HealFull"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->HealthComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::HealHP, 50.0f));
    TestTrue(TEXT("Seed 2 HP items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));

    const float HPBefore = Character->HealthComponent->GetCurrentHP(); // full (300)
    Character->Server_RequestUseInventoryItem_Implementation(0);

    TestEqual(TEXT("HP unchanged at full"), Character->HealthComponent->GetCurrentHP(), HPBefore);
    TestEqual(TEXT("Slot 0 not consumed"), Character->InventoryComponent->GetInventorySlots()[0].Quantity, 2);
    return true;
}

// U3 — RestoreVael item grants Vael and consumes exactly one item.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseRestoreVaelConsumesOneTest,
    "WTBR.Inventory.Use.RestoreVaelGrantsAndConsumes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseRestoreVaelConsumesOneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_Vael"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->VaelComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    // The character's VaelComponent BeginPlay sets CurrentVael to max, so seed a
    // lower value to leave room for the grant to actually increase it.
    Character->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
    const float VaelBefore = Character->VaelComponent->GetCurrentVael();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::RestoreVael, 50.0f));
    TestTrue(TEXT("Seed 2 Vael items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));

    Character->Server_RequestUseInventoryItem_Implementation(0);

    TestEqual(TEXT("Vael increased by 50"), Character->VaelComponent->GetCurrentVael(), VaelBefore + 50.0f);
    TestEqual(TEXT("Slot 0 decremented to 1"), Character->InventoryComponent->GetInventorySlots()[0].Quantity, 1);
    return true;
}

// U4 — Unsupported effect type is not consumed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseUnsupportedEffectTest,
    "WTBR.Inventory.Use.UnsupportedEffectDoesNotConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseUnsupportedEffectTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_Unsupported"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    // EffectType None is unsupported.
    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::None, 50.0f));
    TestTrue(TEXT("Seed 2 items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));

    Character->Server_RequestUseInventoryItem_Implementation(0);

    TestEqual(TEXT("Slot 0 not consumed for unsupported effect"),
        Character->InventoryComponent->GetInventorySlots()[0].Quantity, 2);
    return true;
}

// U5 — Invalid slot index does not consume or crash.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseInvalidSlotTest,
    "WTBR.Inventory.Use.InvalidSlotDoesNotConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseInvalidSlotTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_InvalidSlot"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->HealthComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::HealHP, 50.0f));
    TestTrue(TEXT("Seed 2 HP items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));
    Character->HealthComponent->ApplyDamage(200.0f); // 300 -> 100
    const float HPBefore = Character->HealthComponent->GetCurrentHP();

    Character->Server_RequestUseInventoryItem_Implementation(999);

    TestEqual(TEXT("HP unchanged for invalid slot"), Character->HealthComponent->GetCurrentHP(), HPBefore);
    TestEqual(TEXT("Slot 0 unchanged for invalid slot"),
        Character->InventoryComponent->GetInventorySlots()[0].Quantity, 2);
    return true;
}

// U6 — Empty slot does not consume or crash.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseEmptySlotTest,
    "WTBR.Inventory.Use.EmptySlotDoesNotConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseEmptySlotTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_EmptySlot"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    Character->Server_RequestUseInventoryItem_Implementation(0); // slot 0 is empty

    TestEqual(TEXT("Inventory stays empty"), CountFilledUseSlots(Character->InventoryComponent), 0);
    return true;
}

// U7 — Quantity decrements per use and the slot clears at zero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseQuantityClearsAtZeroTest,
    "WTBR.Inventory.Use.QuantityDecrementsAndClearsAtZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseQuantityClearsAtZeroTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_ClearAtZero"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->HealthComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::HealHP, 50.0f));
    TestTrue(TEXT("Seed 2 HP items"), Character->InventoryComponent->TryAddItem(Item.Get(), 2));
    Character->HealthComponent->ApplyDamage(200.0f); // 300 -> 100, room for two 50-heals

    Character->Server_RequestUseInventoryItem_Implementation(0);
    TestEqual(TEXT("Slot 0 at 1 after first use"),
        Character->InventoryComponent->GetInventorySlots()[0].Quantity, 1);

    Character->Server_RequestUseInventoryItem_Implementation(0);
    TestEqual(TEXT("Inventory empty after second use"), CountFilledUseSlots(Character->InventoryComponent), 0);
    return true;
}

// U8 — Item use touches inventory only, not the trigger set.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRUseDoesNotMutateTriggerSetTest,
    "WTBR.Inventory.Use.DoesNotMutateTriggerOrCorpse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRUseDoesNotMutateTriggerSetTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRUseWorldFixture Fixture(TEXT("WTBR_Use_NoTriggerMutate"));
    UWorld* World = Fixture.GetWorld();
    TestNotNull(TEXT("GameState spawns"), SpawnUseInMatchGameState(World));
    AWTBRCharacter* Character = SpawnUseCharacter(World);
    TestNotNull(TEXT("Character spawns"), Character);
    if (!Character || !Character->InventoryComponent || !Character->HealthComponent
        || !Character->TriggerSetComponent) return false;
    Character->InventoryComponent->InitializeServerInventory();

    const int32 MainIndexBefore = Character->TriggerSetComponent->GetActiveMainIndex();
    const int32 SubIndexBefore = Character->TriggerSetComponent->GetActiveSubIndex();

    TStrongObjectPtr<UWTBRItemDataAsset> Item(MakeConsumable(4, EWTBRConsumableEffectType::HealHP, 50.0f));
    TestTrue(TEXT("Seed 1 HP item"), Character->InventoryComponent->TryAddItem(Item.Get(), 1));
    Character->HealthComponent->ApplyDamage(100.0f); // 300 -> 200

    Character->Server_RequestUseInventoryItem_Implementation(0);

    TestEqual(TEXT("Item consumed from inventory"), CountFilledUseSlots(Character->InventoryComponent), 0);
    TestEqual(TEXT("Active main slot index unchanged"),
        Character->TriggerSetComponent->GetActiveMainIndex(), MainIndexBefore);
    TestEqual(TEXT("Active sub slot index unchanged"),
        Character->TriggerSetComponent->GetActiveSubIndex(), SubIndexBefore);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
