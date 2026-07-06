// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WTBRBagLootViewTypes.h"
#include "WTBRBagLootWidget.generated.h"

class UTextBlock;
class UImage;
class UWTBRBagLootViewModelComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UWTBRBagLootWidget — thin native parent class for the generated Bag/Corpse-Loot
// WBPs (Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagPanel_Generated.json,
// WBP_CorpseLootPanel_Generated.json, WBP_BagLootLayer_Generated.json), mirroring
// UWTBRGeneratedHUDWidget's Phase 4B pattern.
//
// The SAME parent class is usable for all three WBPs: every binding uses
// BindWidgetOptional, so a WBP missing some of these widgets (e.g.
// WBP_BagPanel_Generated has no Txt_CorpseLoot* widgets, and vice versa) still
// compiles cleanly — only the widgets actually present in a given WBP get updated.
//
// Hard rule: this widget is READ-ONLY / REQUEST-ONLY. It must never mutate
// inventory, corpse loot, ground items, trigger slots, HP, Vael, or gameplay state
// directly. All data comes from FWTBRBagLootSnapshot (a read-only snapshot); the
// only outgoing calls are the existing request-only wrappers already exposed by
// UWTBRBagLootViewModelComponent (RequestUseBagItem, RequestPickupCorpseLootEntry).
// Take All / Move / Drop / Equip-from-bag are intentionally NOT exposed — no
// server-authoritative request path exists for them yet (see
// Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagLoot_BindingPlan.md §4).
//
// The large empty middle section of the Bag panel (reserved for a future Active
// Trigger / Reserve Trigger management area) is intentionally not touched by this
// class — there are no bound widgets for it, and none should be invented here.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class WTBR_API UWTBRBagLootWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── Bag: header / footer ─────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_BagTitle;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_BagCapacity;

    // NOTE: no weight/volume system exists anywhere in UWTBRInventoryComponent
    // (see BindingPlan §1/§5). Always shows the snapshot's placeholder ("-") —
    // never faked here.
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_BagWeight;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_BagHint;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_BagWarning;

    // ── Bag: slots 01-14 ─────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot01Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot02Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot03Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot04Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot05Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot06Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot07Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot08Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot09Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot10Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot11Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot12Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot13Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot14Name;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot01Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot02Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot03Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot04Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot05Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot06Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot07Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot08Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot09Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot10Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot11Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot12Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot13Count;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_BagSlot14Count;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot01Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot02Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot03Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot04Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot05Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot06Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot07Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot08Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot09Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot10Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot11Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot12Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot13Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_BagSlot14Icon;

    // ── Corpse Loot: header ──────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_CorpseLootTitle;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_CorpseLootSubtitle;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_CorpseLootPrompt;

    // ── Corpse Loot: entries 01-08 ───────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry01Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry02Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry03Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry04Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry05Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry06Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry07Name;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry08Name;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry01Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry02Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry03Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry04Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry05Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry06Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry07Type;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry08Type;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry01Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry02Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry03Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry04Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry05Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry06Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry07Action;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_LootEntry08Action;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry01Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry02Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry03Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry04Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry05Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry06Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry07Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Img_LootEntry08Icon;

    // ── Composite (WBP_BagLootLayer_Generated only) ─────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ContextHint;

    // ── Refresh API ──────────────────────────────────────────────────────────

    // Re-fetches the current snapshot from the owning character's
    // UWTBRBagLootViewModelComponent and applies it. Safe to call manually (e.g.
    // from a WBP event) in addition to the automatic delegate-driven refresh.
    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot")
    void RefreshFromViewModel();

    // Applies a given read-only snapshot to the bound widgets. Never mutates
    // gameplay state; only calls SetText/SetBrush/SetVisibility on UI widgets.
    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot")
    void ApplySnapshot(const FWTBRBagLootSnapshot& Snapshot);

    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot")
    void ApplyBagSnapshot(const FWTBRBagPanelSnapshot& Bag);

    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot")
    void ApplyCorpseLootSnapshot(const FWTBRCorpseLootPanelSnapshot& CorpseLoot);

    // ── Request-only wrappers (thin passthrough to the ViewModel) ───────────
    // Neither of these mutates inventory, loot entries, or trigger slots locally
    // — they only dispatch a request via UWTBRBagLootViewModelComponent.
    //
    // Take All / Move / Drop / Equip-from-bag are intentionally NOT exposed here:
    // no server-authoritative request path exists for any of them yet (see
    // WBP_BagLoot_BindingPlan.md §4). Adding them would mean inventing gameplay
    // mutation, which this pass must not do.

    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot | Requests")
    bool RequestUseBagItem(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot | Requests")
    bool RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    // Resolves UWTBRBagLootViewModelComponent from the owning player pawn. Read-only
    // lookup; does not create or mutate any component.
    UWTBRBagLootViewModelComponent* ResolveViewModel() const;

    UFUNCTION()
    void OnViewModelSnapshotChanged();

    // Maps a 0-13 slot index to its 3 named BindWidgetOptional members. Returns
    // false (all-null) for an out-of-range index.
    bool GetSlotTextWidgets(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutCount, UImage*& OutIcon) const;

    // Maps a 0-7 loot row index to its 4 named BindWidgetOptional members. Returns
    // false (all-null) for an out-of-range index.
    bool GetLootEntryTextWidgets(int32 RowIndex, UTextBlock*& OutName, UTextBlock*& OutType, UTextBlock*& OutAction, UImage*& OutIcon) const;

    // ── Safe UI update helpers (never touch gameplay state) ─────────────────
    static void SetTextSafe(UTextBlock* TextBlock, const FText& Text);
    static void SetImageSafe(UImage* Image, UTexture2D* Texture);
    static void ClearImageSafe(UImage* Image);
    static void SetVisibilitySafe(UWidget* Widget, ESlateVisibility Visibility);

    TWeakObjectPtr<UWTBRBagLootViewModelComponent> BoundViewModel;
};
