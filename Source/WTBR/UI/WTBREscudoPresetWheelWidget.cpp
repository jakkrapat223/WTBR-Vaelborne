// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBREscudoPresetWheelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
    constexpr float WTBREscudoWheelStartAngleDeg = -90.0f;
    constexpr int32 WTBREscudoWheelArcSegments = 16;

    FVector2D WTBREscudoWheelPointOnCircle(const FVector2D& Center, float Radius, float AngleDeg)
    {
        const float Rad = FMath::DegreesToRadians(AngleDeg);
        return Center + FVector2D(FMath::Cos(Rad) * Radius, FMath::Sin(Rad) * Radius);
    }

    void WTBREscudoWheelBuildArc(TArray<FVector2D>& OutPoints, const FVector2D& Center,
        float Radius, float StartAngleDeg, float EndAngleDeg)
    {
        OutPoints.Reset(WTBREscudoWheelArcSegments + 1);
        for (int32 i = 0; i <= WTBREscudoWheelArcSegments; ++i)
        {
            const float Alpha = static_cast<float>(i) / static_cast<float>(WTBREscudoWheelArcSegments);
            OutPoints.Add(WTBREscudoWheelPointOnCircle(Center, Radius,
                FMath::Lerp(StartAngleDeg, EndAngleDeg, Alpha)));
        }
    }
}

TSharedRef<SWidget> UWTBREscudoPresetWheelWidget::RebuildWidget()
{
    return SNew(SBox);
}

void UWTBREscudoPresetWheelWidget::OpenWheel(const TArray<FWTBREscudoPresetWheelOption>& InOptions)
{
    Options = InOptions;
    bIsOpen = true;
    SelectionVector = FVector2D::ZeroVector;
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWTBREscudoPresetWheelWidget::CloseWheel()
{
    bIsOpen = false;
    Options.Reset();
    SelectionVector = FVector2D::ZeroVector;
    SetVisibility(ESlateVisibility::Collapsed);
}

int32 UWTBREscudoPresetWheelWidget::GetRawHighlightedIndex() const
{
    if (!bIsOpen || SelectionVector.Size() < SelectionDeadZone)
    {
        return INDEX_NONE;
    }

    const float AngleDeg = FMath::RadiansToDegrees(
        FMath::Atan2(SelectionVector.Y, SelectionVector.X));
    const float SegmentArc = 360.0f / static_cast<float>(WheelSlotCount);

    float Normalized = AngleDeg - WTBREscudoWheelStartAngleDeg + (SegmentArc * 0.5f);
    Normalized = FMath::Fmod(Normalized + 360.0f, 360.0f);

    return FMath::Clamp(static_cast<int32>(Normalized / SegmentArc), 0, WheelSlotCount - 1);
}

int32 UWTBREscudoPresetWheelWidget::GetHighlightedPresetIndex() const
{
    const int32 RawIndex = GetRawHighlightedIndex();
    if (RawIndex == INDEX_NONE || !Options.IsValidIndex(RawIndex) || !Options[RawIndex].bAffordable)
    {
        // Also covers "slot has no preset at all" — Options.IsValidIndex
        // already returns false for an empty/unfilled slot.
        return INDEX_NONE;
    }
    return RawIndex;
}

void UWTBREscudoPresetWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

    float DeltaX = 0.0f;
    float DeltaY = 0.0f;
    PC->GetInputMouseDelta(DeltaX, DeltaY);

    SelectionVector += FVector2D(DeltaX, -DeltaY) * SelectionSensitivity;

    const float MaxLength = FMath::Max(WheelRadius, SelectionDeadZone + 1.0f);
    if (SelectionVector.Size() > MaxLength)
    {
        SelectionVector = SelectionVector.GetSafeNormal() * MaxLength;
    }
}

int32 UWTBREscudoPresetWheelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    if (!bIsOpen)
    {
        return LayerId;
    }

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FVector2D Center = Size * 0.5f;

    const float Scale = FMath::Max(Size.Y, 1.0f) / 1080.0f;
    const float OuterR = WheelRadius * Scale;
    const float InnerR = FMath::Min(WheelInnerRadius * Scale, OuterR - 1.0f);

    const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);

    const FLinearColor DimColor(1.0f, 1.0f, 1.0f, 0.35f);
    const FLinearColor ActiveColor(0.30f, 0.85f, 0.95f, 1.0f);
    const FLinearColor UnaffordableColor(0.6f, 0.1f, 0.1f, 0.5f);
    const FLinearColor EmptyColor(1.0f, 1.0f, 1.0f, 0.15f);
    const FLinearColor TextColor(1.0f, 1.0f, 1.0f, 0.9f);

    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(), WhiteBrush,
        ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));

    const int32 RawHighlighted = GetRawHighlightedIndex();
    const float SegmentArc = 360.0f / static_cast<float>(WheelSlotCount);

    TArray<FVector2D> Points;

    for (int32 Index = 0; Index < WheelSlotCount; ++Index)
    {
        const bool bHasPreset = Options.IsValidIndex(Index);
        const FWTBREscudoPresetWheelOption* Option = bHasPreset ? &Options[Index] : nullptr;
        const bool bHighlighted = (Index == RawHighlighted) && Option && Option->bAffordable;

        const float StartAngle = WTBREscudoWheelStartAngleDeg + (SegmentArc * Index) - (SegmentArc * 0.5f);
        const float EndAngle = StartAngle + SegmentArc;

        const FLinearColor SegmentColor =
            !bHasPreset ? EmptyColor
            : !Option->bAffordable ? UnaffordableColor
            : bHighlighted ? ActiveColor
                           : DimColor;
        const float SegmentThickness = bHighlighted ? 5.0f : 2.0f;

        WTBREscudoWheelBuildArc(Points, Center, OuterR, StartAngle, EndAngle);
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, SegmentColor, true, SegmentThickness);

        Points.Reset(2);
        Points.Add(WTBREscudoWheelPointOnCircle(Center, InnerR, StartAngle));
        Points.Add(WTBREscudoWheelPointOnCircle(Center, OuterR, StartAngle));
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, DimColor, true, 1.0f);

        if (bHasPreset && !Option->Name.IsEmpty())
        {
            const float MidAngle = StartAngle + (SegmentArc * 0.5f);
            const FVector2D LabelCenter =
                WTBREscudoWheelPointOnCircle(Center, (OuterR + InnerR) * 0.5f, MidAngle);
            const FVector2D LabelOffset(Option->Name.ToString().Len() * 3.6f, 9.0f);

            FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2D(220.0f, 24.0f),
                    FSlateLayoutTransform(LabelCenter - LabelOffset)),
                Option->Name, LabelFont, ESlateDrawEffect::None,
                bHighlighted ? ActiveColor : (Option->bAffordable ? TextColor : UnaffordableColor));
        }
    }

    WTBREscudoWheelBuildArc(Points, Center, InnerR, 0.0f, 360.0f);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(), Points,
        ESlateDrawEffect::None, DimColor, true, 1.0f);

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
