// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "UI/WTBRGeneratedHUDWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WTBRHUDViewModelComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

void UWTBRGeneratedHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    HideUnsupportedOverlayWidgets();

    if (UWTBRHUDViewModelComponent* ViewModel = ResolveViewModel())
    {
        BoundViewModel = ViewModel;
        ViewModel->OnHUDSnapshotChanged.AddUniqueDynamic(this, &UWTBRGeneratedHUDWidget::OnViewModelSnapshotChanged);
    }

    RefreshFromViewModel();
}

void UWTBRGeneratedHUDWidget::NativeDestruct()
{
    if (UWTBRHUDViewModelComponent* ViewModel = BoundViewModel.Get())
    {
        ViewModel->OnHUDSnapshotChanged.RemoveDynamic(this, &UWTBRGeneratedHUDWidget::OnViewModelSnapshotChanged);
    }
    BoundViewModel.Reset();

    Super::NativeDestruct();
}

void UWTBRGeneratedHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshCompositeMergePresentation();
}

UWTBRHUDViewModelComponent* UWTBRGeneratedHUDWidget::ResolveViewModel() const
{
    const AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwningPlayerPawn());
    return IsValid(Character) ? Character->GetHUDViewModelComponent() : nullptr;
}

UWTBRTriggerSetComponent* UWTBRGeneratedHUDWidget::ResolveTriggerSet() const
{
    const AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwningPlayerPawn());
    return IsValid(Character) ? Character->TriggerSetComponent : nullptr;
}

void UWTBRGeneratedHUDWidget::HideUnsupportedOverlayWidgets()
{
    const auto Collapse = [](UWidget* Widget)
    {
        if (Widget)
        {
            Widget->SetVisibility(ESlateVisibility::Collapsed);
        }
    };

    // TODO(Phase4C): Kill Feed stays hidden until a real kill-event log exists.
    // The generated placeholder rows must not imply runtime event data.
    Collapse(Border_KillFeed);
    Collapse(Txt_KillFeedTitle);
    Collapse(Txt_KillFeedLine1);
    Collapse(Txt_KillFeedLine2);
    Collapse(Txt_KillFeedLine3);

    // TODO(Phase4C): The native Slate WTBRRadarWidget already owns the upper-right
    // radar presentation. Hide this JSON minimap placeholder until a distinct
    // minimap backend/design is intentionally added.
    Collapse(Border_Minimap);
    Collapse(Img_MinimapPlaceholder);
    Collapse(Txt_MinimapTitle);

    // TODO(Phase4C): no shrink-zone/safe-zone manager exists yet (see
    // BindingPlan §4) — Txt_ZoneValue/Txt_PhaseValue/Txt_ZoneTimer would only
    // ever show the "-" placeholder from ApplySnapshot. Hide the whole strip
    // rather than display an always-empty panel.
    Collapse(Border_ZoneStrip);
    Collapse(Txt_ZoneLabel);
    Collapse(Txt_ZoneValue);
    Collapse(Txt_PhaseLabel);
    Collapse(Txt_PhaseValue);
    Collapse(Txt_ZoneTimer);
}

FString UWTBRGeneratedHUDWidget::CompositeTypeName(EWTBRCompositeBulletType CompositeType)
{
    if (const UEnum* CompositeEnum = StaticEnum<EWTBRCompositeBulletType>())
    {
        return CompositeEnum->GetNameStringByValue(static_cast<int64>(CompositeType));
    }
    return TEXT("COMPOSITE");
}

void UWTBRGeneratedHUDWidget::RefreshCompositeMergePresentation()
{
    UWTBRTriggerSetComponent* TriggerSet = ResolveTriggerSet();
    if (!IsValid(TriggerSet) || !GetWorld())
    {
        if (Border_CompositeMergeBar)
        {
            Border_CompositeMergeBar->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    const EWTBRCompositeBulletType MergeType = TriggerSet->GetCurrentMergeState();
    const bool bMerging = MergeType != EWTBRCompositeBulletType::None;
    const bool bReady = TriggerSet->HasReadyComposite();
    const bool bCoolingDown = TriggerSet->bCompositeCooldownActive;

    if (Border_CompositeMergeBar)
    {
        Border_CompositeMergeBar->SetVisibility(bMerging || bReady || bCoolingDown
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);
    }
    if (!bMerging && !bReady && !bCoolingDown)
    {
        bWasMerging = false;
        bWasCoolingDown = false;
        return;
    }

    const float LocalTime = GetWorld()->GetTimeSeconds();
    const FLinearColor ActiveColor(0.125f, 0.851f, 1.0f, 1.0f);
    const FLinearColor CooldownColor(0.35f, 0.40f, 0.43f, 1.0f);

    if (bMerging)
    {
        if (!bWasMerging || ObservedMergeType != MergeType)
        {
            ObservedMergeType = MergeType;
            LastCompositeType = MergeType;
            MergeStartLocalTime = LocalTime;
        }
        bWasMerging = true;
        bWasCoolingDown = false;

        const float Duration = FMath::Max(TriggerSet->CompositeMergeDuration, KINDA_SMALL_NUMBER);
        const float Progress = FMath::Clamp((LocalTime - MergeStartLocalTime) / Duration, 0.0f, 1.0f);
        SetPercentSafe(PB_CompositeMergeProgress, Progress);
        if (PB_CompositeMergeProgress)
        {
            PB_CompositeMergeProgress->SetFillColorAndOpacity(ActiveColor);
        }
        SetTextSafe(Txt_CompositeMergeStatus, FText::FromString(FString::Printf(
            TEXT("%s — MERGING %d%%"), *CompositeTypeName(MergeType), FMath::RoundToInt(Progress * 100.0f))));
        return;
    }

    bWasMerging = false;
    if (bReady)
    {
        const EWTBRCompositeBulletType ReadyType = TriggerSet->GetReadyCompositeType();
        LastCompositeType = ReadyType;
        bWasCoolingDown = false;
        SetPercentSafe(PB_CompositeMergeProgress, 1.0f);
        if (PB_CompositeMergeProgress)
        {
            PB_CompositeMergeProgress->SetFillColorAndOpacity(ActiveColor);
        }
        SetTextSafe(Txt_CompositeMergeStatus, FText::FromString(FString::Printf(
            TEXT("%s — READY [Input] Fire"), *CompositeTypeName(ReadyType))));
        return;
    }

    if (!bWasCoolingDown)
    {
        bWasCoolingDown = true;
        CooldownStartLocalTime = LocalTime;
        CooldownDurationLocal = 0.0f;
        if (UWTBRCompositeRegistryDataAsset* Registry = TriggerSet->CompositeRegistryAsset.LoadSynchronous())
        {
            FWTBRCompositeDefinition Definition;
            if (Registry->FindDefinition(LastCompositeType, Definition))
            {
                CooldownDurationLocal = Definition.CompositeCooldown;
            }
        }
    }

    const float Remaining = CooldownDurationLocal > KINDA_SMALL_NUMBER
        ? FMath::Max(CooldownDurationLocal - (LocalTime - CooldownStartLocalTime), 0.0f)
        : 0.0f;
    const float Progress = CooldownDurationLocal > KINDA_SMALL_NUMBER
        ? Remaining / CooldownDurationLocal
        : 0.0f;
    SetPercentSafe(PB_CompositeMergeProgress, Progress);
    if (PB_CompositeMergeProgress)
    {
        PB_CompositeMergeProgress->SetFillColorAndOpacity(CooldownColor);
    }
    SetTextSafe(Txt_CompositeMergeStatus, FText::FromString(FString::Printf(
        TEXT("%s — COOLDOWN %.1fs"), *CompositeTypeName(LastCompositeType), Remaining)));
}

void UWTBRGeneratedHUDWidget::OnViewModelSnapshotChanged()
{
    RefreshFromViewModel();
}

void UWTBRGeneratedHUDWidget::RefreshFromViewModel()
{
    UWTBRHUDViewModelComponent* ViewModel = BoundViewModel.Get();
    if (!IsValid(ViewModel))
    {
        // Owning pawn may not have been valid yet at NativeConstruct time
        // (e.g. HUD created before possession completes) — try again now.
        ViewModel = ResolveViewModel();
        if (IsValid(ViewModel))
        {
            BoundViewModel = ViewModel;
            ViewModel->OnHUDSnapshotChanged.AddUniqueDynamic(this, &UWTBRGeneratedHUDWidget::OnViewModelSnapshotChanged);
        }
    }

    if (!IsValid(ViewModel))
    {
        return;
    }

    ApplySnapshot(ViewModel->GetHUDSnapshot());
}

void UWTBRGeneratedHUDWidget::ApplySnapshot(const FWTBRHUDSnapshot& Snapshot)
{
    // ── Vitals ────────────────────────────────────────────────────────────────
    SetTextSafe(Txt_HPValue, FormatCurrentMax(Snapshot.Vitals.HP, Snapshot.Vitals.MaxHP));
    SetPercentSafe(PB_HP, Snapshot.Vitals.MaxHP > 0.0f ? Snapshot.Vitals.HP / Snapshot.Vitals.MaxHP : 0.0f);

    SetTextSafe(Txt_VaelValue, FormatCurrentMax(Snapshot.Vitals.Vael, Snapshot.Vitals.MaxVael));
    SetPercentSafe(PB_Vael, Snapshot.Vitals.MaxVael > 0.0f ? Snapshot.Vitals.Vael / Snapshot.Vitals.MaxVael : 0.0f);

    // ── Main Trigger ─────────────────────────────────────────────────────────
    SetTextSafe(Txt_MainTriggerName, Snapshot.MainTrigger.TriggerName);
    SetTextSafe(Txt_MainTriggerCost, FormatTriggerCostText(Snapshot.MainTrigger));
    SetTextSafe(Txt_MainTriggerSlotIndicator,
        FormatSlotIndicatorFromAbsoluteIndex(Snapshot.MainTrigger.ActiveSlotIndex, /*bIsMain=*/true));
    // TODO(Phase4C): no unified per-trigger cooldown-remaining getter exists yet
    // anywhere in the Trigger hierarchy (see WBP_HUD_BindingPlan.md §4). Left at
    // 0 (empty bar) rather than guessing a value.
    SetPercentSafe(PB_MainTriggerCooldown, 0.0f);

    // ── Sub Trigger ──────────────────────────────────────────────────────────
    SetTextSafe(Txt_SubTriggerName, Snapshot.SubTrigger.TriggerName);
    SetTextSafe(Txt_SubTriggerCost, FormatTriggerCostText(Snapshot.SubTrigger));
    SetTextSafe(Txt_SubTriggerSlotIndicator,
        FormatSlotIndicatorFromAbsoluteIndex(Snapshot.SubTrigger.ActiveSlotIndex, /*bIsMain=*/false));
    // TODO(Phase4C): same cooldown gap as Main Trigger above.
    SetPercentSafe(PB_SubTriggerCooldown, 0.0f);

    // ── Zone / Match ─────────────────────────────────────────────────────────
    // TODO(Phase4C): no shrink-zone/safe-zone manager exists yet — zone number,
    // shrink-phase number, and zone timer have no backing data source (see
    // WBP_HUD_BindingPlan.md §4). Snapshot.Match.MatchPhaseText reflects the
    // MATCH lifecycle phase (Lobby/PreMatch/Countdown/InMatch/PostMatch), which
    // is a different concept from the zone-shrink phase number — deliberately
    // NOT reused here to avoid showing misleading data.
    static const FText PlaceholderDash = FText::FromString(TEXT("-"));
    SetTextSafe(Txt_ZoneValue, PlaceholderDash);
    SetTextSafe(Txt_PhaseValue, PlaceholderDash);
    SetTextSafe(Txt_ZoneTimer, Snapshot.Match.bHasZoneTimer
        ? FText::AsNumber(Snapshot.Match.ZoneTimeRemaining)
        : PlaceholderDash);

    // ── Match Status (Phase 7D) ──────────────────────────────────────────────
    // Display-only: read from the snapshot, never mutates GameState/GameMode.
    SetTextSafe(Txt_MatchPhaseValue, Snapshot.Match.bHasMatchPhase
        ? Snapshot.Match.MatchPhaseText
        : FText::GetEmpty());
    SetTextSafe(Txt_WinnerValue, Snapshot.Match.bHasWinner
        ? Snapshot.Match.WinnerText
        : FText::GetEmpty());

    // ── Pickup ───────────────────────────────────────────────────────────────
    SetPickupPromptVisible(Snapshot.PickupPrompt.bIsVisible);
    SetTextSafe(Txt_PickupPrompt, Snapshot.PickupPrompt.PromptText);

    // ── Quick Item ───────────────────────────────────────────────────────────
    // TODO(Phase4C): FWTBRHUDQuickItemSnapshot.ItemName was added in this pass;
    // if it is still empty (e.g. an item DataAsset with no DisplayName set),
    // fall back to a generic placeholder rather than showing blank text.
    static const FText QuickItemFallbackName = FText::FromString(TEXT("Quick Item"));
    SetTextSafe(Txt_QuickItemName, Snapshot.QuickItem.bHasItem && !Snapshot.QuickItem.ItemName.IsEmpty()
        ? Snapshot.QuickItem.ItemName
        : QuickItemFallbackName);
    SetTextSafe(Txt_QuickItemCount, Snapshot.QuickItem.bHasItem
        ? FText::Format(NSLOCTEXT("WTBRHUD", "QuickItemCountFmt", "x{0}"), FText::AsNumber(Snapshot.QuickItem.Count))
        : FText::GetEmpty());
    SetTextSafe(Txt_QuickItemKey, Snapshot.QuickItem.Binding.bIsBound
        ? Snapshot.QuickItem.Binding.DisplayName
        : FText::FromString(TEXT("[Input]")));

    // Alive/Kill/KillFeed/DamageFeedback are bound (see header) but intentionally
    // not populated here — no snapshot data exists for them yet (Phase4A audit:
    // AliveCount/KillCount are never set by BuildMatchSnapshot(); kill feed and
    // damage feedback have no snapshot fields at all). Deferred to a later pass.
}

void UWTBRGeneratedHUDWidget::SetPickupPromptVisible(bool bVisible)
{
    if (IsValid(Txt_PickupPrompt))
    {
        Txt_PickupPrompt->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UWTBRGeneratedHUDWidget::SetDamageFeedbackText(const FText& DamageText)
{
    SetTextSafe(Txt_DamageFeedback, DamageText);
}

void UWTBRGeneratedHUDWidget::SetTextSafe(UTextBlock* TextBlock, const FText& Text)
{
    if (IsValid(TextBlock))
    {
        TextBlock->SetText(Text);
    }
}

void UWTBRGeneratedHUDWidget::SetPercentSafe(UProgressBar* ProgressBar, float Percent)
{
    if (IsValid(ProgressBar))
    {
        ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
    }
}

FText UWTBRGeneratedHUDWidget::FormatCurrentMax(float Current, float Max)
{
    return FText::Format(NSLOCTEXT("WTBRHUD", "CurrentMaxFmt", "{0} / {1}"),
        FText::AsNumber(FMath::RoundToInt(Current)),
        FText::AsNumber(FMath::RoundToInt(Max)));
}

FText UWTBRGeneratedHUDWidget::FormatTriggerCostText(const FWTBRHUDTriggerCardSnapshot& TriggerSnapshot)
{
    if (!TriggerSnapshot.bIsCostKnownForHUD)
    {
        // Charge/variable-cost (e.g. Serpveil) or drain-based (e.g. Aegorn,
        // Black Trigger) triggers — a single "Cost: N" number would misrepresent
        // the real resource model, so show a neutral placeholder instead.
        return FText::FromString(TEXT("Cost: -"));
    }

    if (!TriggerSnapshot.bHasVaelCost)
    {
        // Known zero-cost trigger by design (Feryx, Lacern, Mantorn, Vexorn, etc.).
        return FText::FromString(TEXT("Cost: 0"));
    }

    return FText::Format(NSLOCTEXT("WTBRHUD", "TriggerCostFmt", "Cost: {0}"),
        FText::AsNumber(FMath::RoundToInt(TriggerSnapshot.EffectiveVaelCost)));
}

FText UWTBRGeneratedHUDWidget::FormatSlotIndicatorFromAbsoluteIndex(int32 AbsoluteSlotIndex, bool bIsMain)
{
    static const TCHAR* SlotGlyphs[4] =
    {
        TEXT("◆ ◇ ◇ ◇"), // Slot 1
        TEXT("◇ ◆ ◇ ◇"), // Slot 2
        TEXT("◇ ◇ ◆ ◇"), // Slot 3
        TEXT("◇ ◇ ◇ ◆"), // Slot 4
    };
    static const TCHAR* AllEmptyGlyph = TEXT("◇ ◇ ◇ ◇");

    const int32 BaseIndex = bIsMain ? 0 : 4;
    const int32 LocalIndex = AbsoluteSlotIndex - BaseIndex;
    if (LocalIndex < 0 || LocalIndex > 3)
    {
        return FText::FromString(AllEmptyGlyph);
    }

    return FText::FromString(SlotGlyphs[LocalIndex]);
}
