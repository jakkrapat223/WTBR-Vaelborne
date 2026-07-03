// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/WTBRItemDataAsset.h" // full type for TSoftObjectPtr<T> parity with FWTBRTriggerSlot
#include "WTBRInventorySlot.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRInventorySlot — one slot in the BR inventory (S5-A data model only).
//
// Mirrors FWTBRTriggerSlot's soft-reference storage style. Holds an item
// reference plus a stack quantity. Item identity (for stacking/compare) MUST be
// determined by the DataAsset pointer / PrimaryAssetId / soft path — NEVER by
// DisplayName.
//
// S5-A provides only trivial struct helpers. Gameplay mutation lives on the
// server-authoritative inventory component in later phases:
//   * S5-B adds UWTBRInventoryComponent + TryAddItem stacking.
//   * Locked stacking rule (future TryAddItem): fill partial stacks of the same
//     item first, then the first empty slot, reject when full. Ground-item pickup
//     must be ALL-OR-NOTHING in MVP — if the inventory cannot hold the full
//     quantity, it is not mutated and the ground item is not consumed.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRInventorySlot
{
    GENERATED_BODY()

    // Item occupying this slot. Null = empty slot.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WTBR | Inventory")
    TSoftObjectPtr<UWTBRItemDataAsset> ItemData;

    // Stack count for this slot. 0 = empty.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WTBR | Inventory")
    int32 Quantity = 0;

    // Empty when there is no item reference or the stack has been depleted.
    bool IsEmpty() const { return ItemData.IsNull() || Quantity <= 0; }

    // Valid when an item is referenced and the stack holds at least one unit.
    bool IsValid() const { return !ItemData.IsNull() && Quantity > 0; }

    // Reset to the empty state.
    void Clear()
    {
        ItemData.Reset();
        Quantity = 0;
    }
};
