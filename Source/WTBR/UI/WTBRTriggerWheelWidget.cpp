// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRTriggerWheelWidget.h"
#include "WTBRCharacter.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
    // Segment 0 points straight up and they run clockwise, so the wheel reads like a
    // compass rather than starting at an arbitrary angle.
    constexpr float WTBRWheelStartAngleDeg = -90.0f;

    // Points used to draw one segment's arc. Enough that the curve reads as smooth at
    // the sizes this widget is drawn at, without generating pointless geometry.
    constexpr int32 WTBRWheelArcSegments = 16;

    FVector2D WTBRWheelPointOnCircle(const FVector2D& Center, float Radius, float AngleDeg)
    {
        const float Rad = FMath::DegreesToRadians(AngleDeg);
        return Center + FVector2D(FMath::Cos(Rad) * Radius, FMath::Sin(Rad) * Radius);
    }

    void WTBRWheelBuildArc(TArray<FVector2D>& OutPoints, const FVector2D& Center,
        float Radius, float StartAngleDeg, float EndAngleDeg)
    {
        OutPoints.Reset(WTBRWheelArcSegments + 1);
        for (int32 i = 0; i <= WTBRWheelArcSegments; ++i)
        {
            const float Alpha = static_cast<float>(i) / static_cast<float>(WTBRWheelArcSegments);
            OutPoints.Add(WTBRWheelPointOnCircle(Center, Radius,
                FMath::Lerp(StartAngleDeg, EndAngleDeg, Alpha)));
        }
    }
}

TSharedRef<SWidget> UWTBRTriggerWheelWidget::RebuildWidget()
{
    // Same as UWTBRRadarWidget: an empty box that NativePaint draws into. Nothing
    // here is layout, so there is no child hierarchy to build.
    return SNew(SBox);
}

AWTBRCharacter* UWTBRTriggerWheelWidget::GetOwningWTBRCharacter() const
{
    const APlayerController* PC = GetOwningPlayer();
    return PC ? Cast<AWTBRCharacter>(PC->GetPawn()) : nullptr;
}

int32 UWTBRTriggerWheelWidget::GetSetBaseSlotIndex() const
{
    return bIsMainSet ? 0 : SlotsPerSet;
}

void UWTBRTriggerWheelWidget::OpenWheel(bool bInIsMainSet)
{
    WheelContent = EWheelContent::TriggerSlots;
    bIsMainSet = bInIsMainSet;
    FeryxFormNames.Reset();
    CurrentFeryxFormIndex = INDEX_NONE;
    bIsOpen = true;
    // Start centred so a hold with no flick selects nothing and the release falls
    // back to "keep the current slot".
    SelectionVector = FVector2D::ZeroVector;
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWTBRTriggerWheelWidget::OpenFeryxFormWheel(
    const TArray<FText>& InFormNames, int32 InCurrentFormIndex)
{
    WheelContent = EWheelContent::FeryxForms;
    FeryxFormNames = InFormNames;
    if (FeryxFormNames.Num() > SlotsPerSet)
    {
        FeryxFormNames.SetNum(SlotsPerSet);
    }
    CurrentFeryxFormIndex = FeryxFormNames.IsValidIndex(InCurrentFormIndex)
        ? InCurrentFormIndex
        : INDEX_NONE;
    bIsOpen = true;
    SelectionVector = FVector2D::ZeroVector;
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWTBRTriggerWheelWidget::CloseWheel()
{
    bIsOpen = false;
    WheelContent = EWheelContent::TriggerSlots;
    FeryxFormNames.Reset();
    CurrentFeryxFormIndex = INDEX_NONE;
    SelectionVector = FVector2D::ZeroVector;
    SetVisibility(ESlateVisibility::Collapsed);
}

int32 UWTBRTriggerWheelWidget::GetHighlightedSetIndex() const
{
    if (!bIsOpen || SelectionVector.Size() < SelectionDeadZone)
    {
        return INDEX_NONE;
    }

    // Angle of the flick, measured the same way the segments are laid out.
    const float AngleDeg = FMath::RadiansToDegrees(
        FMath::Atan2(SelectionVector.Y, SelectionVector.X));
    const float SegmentArc = 360.0f / static_cast<float>(SlotsPerSet);

    // Shift by half a segment so a flick straight at a segment's centre lands in it
    // rather than on the boundary between two.
    float Normalized = AngleDeg - WTBRWheelStartAngleDeg + (SegmentArc * 0.5f);
    Normalized = FMath::Fmod(Normalized + 360.0f, 360.0f);

    return FMath::Clamp(static_cast<int32>(Normalized / SegmentArc), 0, SlotsPerSet - 1);
}

int32 UWTBRTriggerWheelWidget::GetHighlightedSlotIndex() const
{
    if (WheelContent != EWheelContent::TriggerSlots)
    {
        return INDEX_NONE;
    }

    const int32 SetIndex = GetHighlightedSetIndex();
    if (SetIndex == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    const int32 AbsoluteIndex = GetSetBaseSlotIndex() + SetIndex;

    // Never hand back an empty slot — switching to one would leave the player
    // holding nothing.
    const AWTBRCharacter* Char = GetOwningWTBRCharacter();
    const UWTBRTriggerSetComponent* TSC = Char ? Char->TriggerSetComponent : nullptr;
    if (TSC && !TSC->IsSlotOccupied(AbsoluteIndex))
    {
        return INDEX_NONE;
    }

    return AbsoluteIndex;
}

int32 UWTBRTriggerWheelWidget::GetHighlightedFeryxFormIndex() const
{
    if (!bIsOpen || WheelContent != EWheelContent::FeryxForms)
    {
        return INDEX_NONE;
    }

    const int32 FormIndex = GetHighlightedSetIndex();
    return FeryxFormNames.IsValidIndex(FormIndex) ? FormIndex : INDEX_NONE;
}

bool UWTBRTriggerWheelWidget::IsFeryxFormWheelOpen() const
{
    return bIsOpen && WheelContent == EWheelContent::FeryxForms;
}

void UWTBRTriggerWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsOpen)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    // Raw look delta, not cursor position: the cursor is hidden during play.
    float DeltaX = 0.0f;
    float DeltaY = 0.0f;
    PC->GetInputMouseDelta(DeltaX, DeltaY);

    SelectionVector += FVector2D(DeltaX, -DeltaY) * SelectionSensitivity;

    // Clamp to the wheel so the highlight cannot drift arbitrarily far out; the
    // direction is all that matters past the rim.
    const float MaxLength = FMath::Max(WheelRadius, SelectionDeadZone + 1.0f);
    if (SelectionVector.Size() > MaxLength)
    {
        SelectionVector = SelectionVector.GetSafeNormal() * MaxLength;
    }
}

int32 UWTBRTriggerWheelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    if (!bIsOpen)
    {
        return LayerId;
    }

    const bool bDrawingFeryxForms = WheelContent == EWheelContent::FeryxForms;
    const AWTBRCharacter* Char = GetOwningWTBRCharacter();
    const UWTBRTriggerSetComponent* TSC = Char ? Char->TriggerSetComponent : nullptr;
    if (!bDrawingFeryxForms && !TSC)
    {
        return LayerId;
    }

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FVector2D Center = Size * 0.5f;

    // Authored against a 1080-tall viewport so the wheel keeps its proportions.
    const float Scale = FMath::Max(Size.Y, 1.0f) / 1080.0f;
    const float OuterR = WheelRadius * Scale;
    const float InnerR = FMath::Min(WheelInnerRadius * Scale, OuterR - 1.0f);

    const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
    const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);

    const FLinearColor DimColor(1.0f, 1.0f, 1.0f, 0.35f);
    const FLinearColor ActiveColor(0.30f, 0.85f, 0.95f, 1.0f);
    const FLinearColor EmptyColor(1.0f, 1.0f, 1.0f, 0.15f);
    const FLinearColor TextColor(1.0f, 1.0f, 1.0f, 0.9f);

    // Dim the world behind the wheel so the labels stay readable over bright terrain.
    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(), WhiteBrush,
        ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));

    const int32 BaseSlot = bDrawingFeryxForms ? 0 : GetSetBaseSlotIndex();
    const int32 HighlightedSetIndex = GetHighlightedSetIndex();
    const int32 ActiveSlot = bDrawingFeryxForms
        ? CurrentFeryxFormIndex
        : (bIsMainSet ? TSC->GetActiveMainIndex() : TSC->GetActiveSubIndex());
    const float SegmentArc = 360.0f / static_cast<float>(SlotsPerSet);

    TArray<FVector2D> Points;

    for (int32 SetIndex = 0; SetIndex < SlotsPerSet; ++SetIndex)
    {
        const int32 AbsoluteSlot = BaseSlot + SetIndex;
        const bool bOccupied = bDrawingFeryxForms
            ? FeryxFormNames.IsValidIndex(SetIndex)
            : TSC->IsSlotOccupied(AbsoluteSlot);
        const bool bHighlighted = (SetIndex == HighlightedSetIndex) && bOccupied;
        const bool bIsCurrent = bDrawingFeryxForms
            ? SetIndex == ActiveSlot
            : AbsoluteSlot == ActiveSlot;

        const float StartAngle = WTBRWheelStartAngleDeg + (SegmentArc * SetIndex) - (SegmentArc * 0.5f);
        const float EndAngle = StartAngle + SegmentArc;

        const FLinearColor SegmentColor =
            !bOccupied   ? EmptyColor
            : bHighlighted ? ActiveColor
                           : DimColor;
        const float SegmentThickness = bHighlighted ? 5.0f : 2.0f;

        // Outer arc, drawn thicker when highlighted — that alone reads as "selected"
        // at a glance without needing a filled wedge.
        WTBRWheelBuildArc(Points, Center, OuterR, StartAngle, EndAngle);
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, SegmentColor, true, SegmentThickness);

        // Divider out to the rim at the segment's leading edge.
        Points.Reset(2);
        Points.Add(WTBRWheelPointOnCircle(Center, InnerR, StartAngle));
        Points.Add(WTBRWheelPointOnCircle(Center, OuterR, StartAngle));
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, DimColor, true, 1.0f);

        const FText SlotName = bDrawingFeryxForms
            ? (FeryxFormNames.IsValidIndex(SetIndex) ? FeryxFormNames[SetIndex] : FText::GetEmpty())
            : TSC->GetSlotDisplayName(AbsoluteSlot);
        if (!SlotName.IsEmpty())
        {
            const float MidAngle = StartAngle + (SegmentArc * 0.5f);
            const FVector2D LabelCenter =
                WTBRWheelPointOnCircle(Center, (OuterR + InnerR) * 0.5f, MidAngle);

            // Rough centring: Slate text is drawn from its top-left, and measuring
            // every label each paint is not worth it for a four-item wheel.
            const FVector2D LabelOffset(SlotName.ToString().Len() * 3.6f, 9.0f);

            FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2D(220.0f, 24.0f),
                    FSlateLayoutTransform(LabelCenter - LabelOffset)),
                SlotName, LabelFont, ESlateDrawEffect::None,
                bHighlighted ? ActiveColor : TextColor);

            if (bIsCurrent)
            {
                FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
                    AllottedGeometry.ToPaintGeometry(FVector2D(160.0f, 20.0f),
                        FSlateLayoutTransform(LabelCenter - LabelOffset + FVector2D(0.0f, 18.0f))),
                    bDrawingFeryxForms
                        ? NSLOCTEXT("WTBRWheel", "CurrentFormTag", "current")
                        : NSLOCTEXT("WTBRWheel", "EquippedTag", "equipped"),
                    HintFont,
                    ESlateDrawEffect::None, DimColor);
            }
        }
    }

    // Inner ring, marking the dead zone where releasing changes nothing.
    WTBRWheelBuildArc(Points, Center, InnerR, 0.0f, 360.0f);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(), Points,
        ESlateDrawEffect::None, DimColor, true, 1.0f);

    // Which set is open, in the hub.
    const FText SetLabel = bDrawingFeryxForms
        ? NSLOCTEXT("WTBRWheel", "FeryxForms", "FERYX")
        : (bIsMainSet
            ? NSLOCTEXT("WTBRWheel", "MainSet", "MAIN")
            : NSLOCTEXT("WTBRWheel", "SubSet", "SUB"));
    FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(FVector2D(120.0f, 20.0f),
            FSlateLayoutTransform(Center - FVector2D(18.0f, 8.0f))),
        SetLabel, HintFont, ESlateDrawEffect::None, DimColor);

    // Pointer showing the flick direction, so the selection feels physical.
    if (SelectionVector.Size() >= SelectionDeadZone)
    {
        const FVector2D Dir = SelectionVector.GetSafeNormal();
        Points.Reset(2);
        Points.Add(Center + Dir * InnerR);
        Points.Add(Center + Dir * (OuterR - 6.0f));
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, ActiveColor, true, 2.0f);
    }

    return LayerId;
}
