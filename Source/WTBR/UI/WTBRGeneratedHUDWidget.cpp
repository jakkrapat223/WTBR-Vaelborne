// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "UI/WTBRGeneratedHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WTBRHUDViewModelComponent.h"
#include "WTBRCharacter.h"

void UWTBRGeneratedHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

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

UWTBRHUDViewModelComponent* UWTBRGeneratedHUDWidget::ResolveViewModel() const
{
    const AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwningPlayerPawn());
    return IsValid(Character) ? Character->GetHUDViewModelComponent() : nullptr;
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
