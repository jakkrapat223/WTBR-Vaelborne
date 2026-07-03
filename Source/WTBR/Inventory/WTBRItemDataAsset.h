// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

class UTexture2D;

#include "WTBRItemDataAsset.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// BR Ground Item / Inventory data model (S5-A — data only, no gameplay behavior).
//
// BR Ground Item is SEPARATE from Dropped Trigger:
//   * Dropped Trigger  = AWTBRDroppedTriggerActor, Trigger loot only.
//   * BR Ground Item   = consumables, Vael items, tuning, options, upgrade materials.
//
// S5-A implements ONLY the additive C++ data model. The ground-item actor,
// inventory component behavior, pickup RPCs, stacking algorithm, and item-use
// effects are intentionally NOT here — they arrive in later S5 phases.
// ─────────────────────────────────────────────────────────────────────────────

// High-level classification of an inventory/ground item.
UENUM(BlueprintType)
enum class EWTBRItemType : uint8
{
    None          UMETA(DisplayName = "None"),
    Consumable    UMETA(DisplayName = "Consumable"),   // MVP: HP consumable
    VaelItem      UMETA(DisplayName = "Vael Item"),     // MVP: Vael consumable

    // ── Future / parked (reserved value only — behavior NOT implemented in S5) ──
    Tuning          UMETA(DisplayName = "Trigger Tuning"),   // parked
    Option          UMETA(DisplayName = "Trigger Option"),   // parked (e.g. Senku)
    UpgradeMaterial UMETA(DisplayName = "Upgrade Material"),  // parked
};

// Effect applied when a consumable/Vael item is used.
// The actual effect application (heal HP, restore Vael, …) is server-authoritative
// and lives in S5-D — this enum only classifies the effect for the data model.
UENUM(BlueprintType)
enum class EWTBRConsumableEffectType : uint8
{
    None          UMETA(DisplayName = "None"),
    HealHP        UMETA(DisplayName = "Heal HP"),       // MVP
    RestoreVael   UMETA(DisplayName = "Restore Vael"),  // MVP

    // ── Future / parked (reserved value only — effect NOT implemented) ──
    // Design-locked but effect implementation is parked (see S5 design lock).
    VaelOvercharge UMETA(DisplayName = "Vael Overcharge"),
};

// ─────────────────────────────────────────────────────────────────────────────
// UWTBRItemDataAsset
// Single source of truth for a BR ground/inventory item. Mirrors the style of
// UWTBRTriggerDataAsset (UPrimaryDataAsset + inline GetPrimaryAssetId).
// Item identity for stacking/compare must use the DataAsset pointer /
// PrimaryAssetId / soft path — NEVER DisplayName.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class WTBR_API UWTBRItemDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ─── Identity ────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Identity")
    FText DisplayName;

    // Short in-world item name shown in HUD (e.g. "HP Capsule"). Localization-ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Identity")
    FText FunctionalName;

    // One-line description shown in inventory/tooltip UI. Localization-ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Identity")
    FText FunctionalDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Identity")
    TSoftObjectPtr<UTexture2D> HUDIcon;

    // ─── Classification ──────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Classification")
    EWTBRItemType ItemType = EWTBRItemType::None;

    // Relevant when ItemType is a consumable/Vael item; None otherwise.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Classification")
    EWTBRConsumableEffectType ConsumableEffectType = EWTBRConsumableEffectType::None;

    // ─── Tunables (final balance overrideable from DA/editor — never hardcode) ─

    // Maximum quantity per stack. Design lock: small consumable = 4, large = 2.
    // Default 1 is a safe C++ fallback; real values come from each DA.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Tunables",
        meta = (ClampMin = "1"))
    int32 MaxStackSize = 1;

    // Magnitude of the consumable effect (e.g. HP healed, Vael restored).
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Tunables",
        meta = (ClampMin = "0.0"))
    float EffectMagnitude = 0.0f;

    // Time (seconds) to use/consume the item. 0 = instant.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | Tunables",
        meta = (ClampMin = "0.0"))
    float UseTimeSeconds = 0.0f;

    // ─── Asset Manager ───────────────────────────────────────────────────────

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("WTBRItem", GetFName());
    }
};
