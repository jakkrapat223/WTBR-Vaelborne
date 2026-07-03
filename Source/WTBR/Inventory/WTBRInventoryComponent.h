// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/WTBRInventorySlot.h"
#include "WTBRInventoryComponent.generated.h"

class UWTBRItemDataAsset;

// Fired on the owning actor when the inventory contents change (server mutation
// or client replication). Bind in Blueprint HUD/UI to refresh the inventory view.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWTBRInventoryChanged);

// ─────────────────────────────────────────────────────────────────────────────
// UWTBRInventoryComponent — server-authoritative BR inventory foundation (S5-B).
//
// Owns a replicated fixed-size slot array and a transactional, all-or-nothing
// TryAddItem stacking helper. This pass adds storage + add-stacking ONLY:
//   * No ground-item actor / pickup RPC (S5-C).
//   * No item-use / HP restore / Vael restore (S5-D).
//   * No inventory UI, no F-context wiring.
//
// Stacking rule (locked): fill existing partial stacks of the SAME item first,
// then the first empty slot(s); reject when full. Pickup is all-or-nothing —
// if the full quantity does not fit, the inventory is not mutated.
//
// Item identity for stacking is by DataAsset pointer / soft path — NEVER DisplayName.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRInventoryComponent();

    // Broadcast after a successful server mutation and on client replication.
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnWTBRInventoryChanged OnInventoryChanged;

    // Number of inventory slots. Design range 12–16 (prototype default 14).
    // Configurable per design; final balance is editor/DA-driven, never hardcoded
    // into runtime logic.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Inventory",
        meta=(ClampMin="1"))
    int32 InventorySlotCount = 14;

    // Read-only access to the current slot array (server-authoritative source).
    const TArray<FWTBRInventorySlot>& GetInventorySlots() const { return InventorySlots; }

    // Server-only: ensure the slot array is sized to InventorySlotCount. Called
    // from BeginPlay; idempotent and safe if InventorySlotCount changes in editor.
    // No-op without authority.
    void InitializeServerInventory();

    // Non-mutating capacity check. Returns true if the full Quantity of ItemData
    // could be added under the stacking rule. Robust whether or not the slot array
    // has been sized yet.
    UFUNCTION(BlueprintCallable, Category="WTBR | Inventory")
    bool CanAddItem(const UWTBRItemDataAsset* ItemData, int32 Quantity) const;

    // Server-authoritative, transactional, all-or-nothing add. Returns true and
    // broadcasts OnInventoryChanged only when the full Quantity was placed.
    // Returns false and leaves the inventory unmutated when: not authority,
    // ItemData null, Quantity <= 0, MaxStackSize <= 0, or insufficient capacity.
    UFUNCTION(BlueprintCallable, Category="WTBR | Inventory")
    bool TryAddItem(const UWTBRItemDataAsset* ItemData, int32 Quantity);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(ReplicatedUsing=OnRep_InventorySlots)
    TArray<FWTBRInventorySlot> InventorySlots;

    UFUNCTION()
    void OnRep_InventorySlots();

    bool HasServerAuthority() const;

    // Grows/shrinks InventorySlots to InventorySlotCount, preserving existing
    // entries. Authority-guarded; call sites must already hold authority.
    void EnsureSlotsSized();

    // True when the slot already holds the same item (identity by pointer/soft path).
    bool SlotMatchesItem(const FWTBRInventorySlot& Slot, const UWTBRItemDataAsset* ItemData) const;

    // Total units of ItemData that could still be added given current slots plus
    // any not-yet-materialized empty slots (InventorySlotCount - Num()).
    int32 ComputeAddableCapacity(const UWTBRItemDataAsset* ItemData) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
