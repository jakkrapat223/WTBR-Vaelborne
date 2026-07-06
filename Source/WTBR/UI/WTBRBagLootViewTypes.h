// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WTBRBagLootViewTypes.generated.h"

class UTexture2D;

// ─────────────────────────────────────────────────────────────────────────────
// Read-only UI snapshot structs for the Bag panel and Corpse Loot panel, mirroring
// the style of UI/WTBRHUDViewTypes.h. None of these carry authority; they are
// display-only and must never be used to drive gameplay mutation.
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWTBRBagSlotSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    bool bIsEmpty = true;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    bool bHasItem = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    FText CountText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    TSoftObjectPtr<UTexture2D> Icon;

    // Derived from UWTBRItemDataAsset::ItemType's enum display name (e.g.
    // "Consumable", "Vael Item"). No rarity/tier concept exists anywhere in the
    // item data model — do not add one here.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    FText ItemTypeText;

    // TODO(Phase4C-BagLoot): no "usable"/"equippable" flag exists yet on
    // UWTBRItemDataAsset or FWTBRInventorySlot. Left false rather than guessed;
    // wire this up once such a field is added to the data model.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    bool bIsUsable = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Slot")
    bool bIsEquippable = false;
};

USTRUCT(BlueprintType)
struct FWTBRBagPanelSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    int32 CurrentSlotsUsed = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    int32 MaxSlots = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    FText CapacityText;

    // TODO(Phase4C-BagLoot): no weight/volume system exists anywhere in
    // UWTBRInventoryComponent (confirmed by a full-module source audit — see
    // WBP_BagLoot_BindingPlan.md §1/§5). This always renders a neutral placeholder
    // ("-") — never fabricate a number here.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    FText WeightText;

    // Empty unless CurrentSlotsUsed >= MaxSlots (bag full).
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    FText WarningText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag | Panel")
    TArray<FWTBRBagSlotSnapshot> Slots;
};

USTRUCT(BlueprintType)
struct FWTBRLootEntrySnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    int32 LootIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    bool bHasEntry = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    FText DisplayName;

    // See WBP_BagLoot_BindingPlan.md §1 "Type column" tradeoff note: sourced from
    // the Trigger DataAsset's SlotConstraint display text when the asset is
    // resident (non-blocking .Get()), falling back to the entry's own
    // CachedCategory enum display name when the DataAsset is not yet loaded.
    // Never forces a synchronous load during a display refresh.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    FText TypeText;

    // "Request" when bCanRequestTake is true, empty otherwise. Static label only —
    // this is not itself a request dispatch.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    FText ActionText;

    // Corpse loot in this container is Trigger-only (no stacking); always 0 or 1.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    TSoftObjectPtr<UTexture2D> Icon;

    // Mirrors FWTBRCorpseLootEntryViewModel::bIsAvailable (false if consumed or asset null).
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Entry")
    bool bCanRequestTake = false;
};

USTRUCT(BlueprintType)
struct FWTBRCorpseLootPanelSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Panel")
    bool bHasFocusedContainer = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Panel")
    FText TitleText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Panel")
    FText SubtitleText;

    // Mirrors AWTBRCorpseLootContainerActor::GetInteractionPromptText() (e.g. "Open
    // Trigger Cache").
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Panel")
    FText PromptText;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Loot | Panel")
    TArray<FWTBRLootEntrySnapshot> Entries;
};

USTRUCT(BlueprintType)
struct FWTBRBagLootSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag Loot")
    FWTBRBagPanelSnapshot Bag;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag Loot")
    FWTBRCorpseLootPanelSnapshot CorpseLoot;

    // TODO(Phase4C-BagLoot): no existing "is Bag UI open" state source was found
    // anywhere in the inventory/interaction modules. NOT derived or faked — always
    // false until a real open/close state exists (e.g. a future input-driven local
    // UI-state flag on a Bag/Loot widget or controller).
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag Loot")
    bool bBagVisible = false;

    // Derived only from CorpseLoot.bHasFocusedContainer — a focused, interactable
    // container implies the loot panel is currently relevant to show. This is a
    // reasonable read-only derivation, not invented gameplay state.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Bag Loot")
    bool bCorpseLootVisible = false;
};
