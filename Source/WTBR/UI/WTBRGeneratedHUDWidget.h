// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WTBRHUDViewTypes.h"
#include "WTBRGeneratedHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UWTBRHUDViewModelComponent;

// ─────────────────────────────────────────────────────────────────────────────
// UWTBRGeneratedHUDWidget — thin native parent class for WBP_HUD_Generated
// (Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_Generated.json).
//
// Phase 4B: binds the JSON-generated widget names via BindWidgetOptional (so the
// WBP still compiles if a widget is renamed/missing mid-iteration) and refreshes
// them from the existing UWTBRHUDViewModelComponent read-only snapshot.
//
// Hard rule: this widget is READ-ONLY / REQUEST-ONLY. It must never mutate
// inventory, trigger slots, loot, ground items, Vael, HP, cooldown, or trigger
// state directly. All data comes from FWTBRHUDSnapshot (a read-only snapshot);
// the only outgoing calls are the existing request-only ViewModel functions,
// which are not invoked from this class in Phase 4B.
//
// See Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_BindingPlan.md for the
// full audit of what is/isn't wired and why.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class WTBR_API UWTBRGeneratedHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── Vitals ────────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_HP;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_HPValue;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_Vael;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_VaelValue;

    // ── Main Trigger ─────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_MainTriggerName;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_MainTriggerCost;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_MainTriggerSlotIndicator;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_MainTriggerCooldown;

    // ── Sub Trigger ──────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_SubTriggerName;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_SubTriggerCost;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_SubTriggerSlotIndicator;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_SubTriggerCooldown;

    // ── Zone / Match ─────────────────────────────────────────────────────────
    // NOTE: no shrink-zone / phase-number system exists yet (see BindingPlan §4).
    // These render a "-" placeholder until that backend exists.
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ZoneValue;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PhaseValue;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_ZoneTimer;

    // ── Match Status (Phase 7D) ──────────────────────────────────────────────
    // Match lifecycle phase (Countdown/InMatch/PostMatch/etc.) — distinct from
    // Txt_PhaseValue above, which is reserved for the BR zone-shrink phase number.
    // Not yet present in WBP_HUD_Generated as of Phase 7D — BindWidgetOptional
    // means this silently stays null (no-op) until the WBP adds a matching
    // TextBlock; see the Phase 7D report for the exact manual editor step.
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_MatchPhaseValue;

    // Round winner ("Player 1" / "Player 2" / "Draw"), only meaningful once
    // PostMatch. Same not-yet-in-WBP caveat as Txt_MatchPhaseValue above.
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_WinnerValue;

    // ── Pickup ───────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_PickupPrompt;

    // ── Quick Item ───────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_QuickItemName;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_QuickItemCount;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_QuickItemKey;

    // ── Optional / later scope (bound now, not populated by ApplySnapshot yet) ──
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_AliveValue;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_KillValue;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_KillFeedLine1;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_KillFeedLine2;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_KillFeedLine3;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_DamageFeedback;

    // ── Refresh API ──────────────────────────────────────────────────────────

    // Re-fetches the current snapshot from the owning character's
    // UWTBRHUDViewModelComponent and applies it. Safe to call manually
    // (e.g. from a WBP event) in addition to the automatic delegate-driven refresh.
    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void RefreshFromViewModel();

    // Applies a given read-only snapshot to the bound widgets. Never mutates
    // gameplay state; only calls SetText/SetPercent/SetVisibility on UI widgets.
    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void ApplySnapshot(const FWTBRHUDSnapshot& Snapshot);

    // Presentation-only visibility toggle for the pickup prompt text.
    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void SetPickupPromptVisible(bool bVisible);

    // Manual hook for a future contextual damage-feedback event; not driven by
    // the snapshot in this pass (no damage-feedback snapshot field exists yet).
    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void SetDamageFeedbackText(const FText& DamageText);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    // Resolves UWTBRHUDViewModelComponent from the owning player pawn. Read-only
    // lookup; does not create or mutate any component.
    UWTBRHUDViewModelComponent* ResolveViewModel() const;

    UFUNCTION()
    void OnViewModelSnapshotChanged();

    // ── Safe UI update helpers (never touch gameplay state) ─────────────────
    static void SetTextSafe(UTextBlock* TextBlock, const FText& Text);
    static void SetPercentSafe(UProgressBar* ProgressBar, float Percent);
    static FText FormatCurrentMax(float Current, float Max);
    static FText FormatTriggerCostText(const FWTBRHUDTriggerCardSnapshot& TriggerSnapshot);

    // Slot 1 = filled-1st-of-4, Slot 2 = filled-2nd-of-4, etc. (see BindingPlan §6).
    // Main absolute index 0-3 -> slot 1-4. Sub absolute index 4-7 -> slot 1-4.
    // Any out-of-range/invalid index renders all-empty diamonds.
    static FText FormatSlotIndicatorFromAbsoluteIndex(int32 AbsoluteSlotIndex, bool bIsMain);

    TWeakObjectPtr<UWTBRHUDViewModelComponent> BoundViewModel;
};
