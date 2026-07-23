// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRPathGraphViewWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

namespace
{
    const FLinearColor WTBRGraphPaneBg(0.04f, 0.06f, 0.09f, 0.85f);
    const FLinearColor WTBRGraphGrid(1.0f, 1.0f, 1.0f, 0.07f);
    const FLinearColor WTBRGraphAxis(1.0f, 1.0f, 1.0f, 0.22f);
    const FLinearColor WTBRGraphLane(0.25f, 0.45f, 0.89f, 0.75f);
    const FLinearColor WTBRGraphLaneSelected(0.91f, 0.63f, 0.23f, 1.0f);
    const FLinearColor WTBRGraphCaster(0.30f, 0.85f, 0.95f, 1.0f);
    const FLinearColor WTBRGraphLabel(1.0f, 1.0f, 1.0f, 0.55f);

    constexpr int32 WTBRGraphForwardGridLines = 5;
    constexpr int32 WTBRGraphLateralGridLines = 4;
}

FVector2D UWTBRPathGraphViewWidget::ProjectNormalizedToPane(
    const FVector& Normalized,
    const FSlateRect& PaneRect,
    bool bElevationPane,
    float InForwardMin,
    float InForwardMax,
    float InLateralExtent)
{
    const float ForwardSpan = FMath::Max(InForwardMax - InForwardMin, KINDA_SMALL_NUMBER);
    const float ForwardAlpha = (Normalized.X - InForwardMin) / ForwardSpan;

    const float Extent = FMath::Max(InLateralExtent, KINDA_SMALL_NUMBER);
    const float LateralValue = bElevationPane ? Normalized.Z : Normalized.Y;
    const float LateralAlpha = (LateralValue + Extent) / (2.0f * Extent);

    // Bottom -> Top as alpha rises, because Slate's Y axis grows downward and a
    // positive lateral/vertical value has to read as "up" to the player.
    return FVector2D(
        FMath::Lerp(PaneRect.Left, PaneRect.Right, ForwardAlpha),
        FMath::Lerp(PaneRect.Bottom, PaneRect.Top, LateralAlpha));
}

TSharedRef<SWidget> UWTBRPathGraphViewWidget::RebuildWidget()
{
    return SNew(SBox);
}

void UWTBRPathGraphViewWidget::SetPreset(const FWTBRPathPreset& InPreset)
{
    DisplayPreset = InPreset;
    SelectedLaneIndex = FMath::Clamp(SelectedLaneIndex, 0, FMath::Max(0, DisplayPreset.Lanes.Num() - 1));
}

void UWTBRPathGraphViewWidget::SetSelectedLaneIndex(int32 InLaneIndex)
{
    SelectedLaneIndex = FMath::Clamp(InLaneIndex, 0, FMath::Max(0, DisplayPreset.Lanes.Num() - 1));
}

void UWTBRPathGraphViewWidget::ComputePaneRects(
    const FVector2D& LocalSize, FSlateRect& OutPlan, FSlateRect& OutElevation) const
{
    const float Left = PanePadding;
    const float Right = FMath::Max(LocalSize.X - PanePadding, Left + 1.0f);

    const float UsableTop = PanePadding;
    const float UsableBottom = FMath::Max(LocalSize.Y - PanePadding, UsableTop + 1.0f);
    const float PaneHeight = FMath::Max((UsableBottom - UsableTop - PaneGap) * 0.5f, 1.0f);

    OutPlan = FSlateRect(Left, UsableTop, Right, UsableTop + PaneHeight);
    OutElevation = FSlateRect(Left, UsableTop + PaneHeight + PaneGap, Right,
        UsableTop + PaneHeight + PaneGap + PaneHeight);
}

void UWTBRPathGraphViewWidget::ComputeLaneScreenPositions(
    int32 LaneIndex, const FSlateRect& PaneRect, bool bElevationPane,
    TArray<FVector2D>& OutPositions) const
{
    OutPositions.Reset();
    if (!DisplayPreset.Lanes.IsValidIndex(LaneIndex)) return;

    const TArray<FVector>& Waypoints = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;
    OutPositions.Reserve(Waypoints.Num());
    for (const FVector& Waypoint : Waypoints)
    {
        OutPositions.Add(ProjectNormalizedToPane(
            Waypoint, PaneRect, bElevationPane, ForwardMin, ForwardMax, LateralExtent));
    }
}

void UWTBRPathGraphViewWidget::PaintPane(
    const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
    int32& LayerId, const FSlateRect& PaneRect, bool bElevationPane) const
{
    const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    const FVector2D PaneSize(PaneRect.Right - PaneRect.Left, PaneRect.Bottom - PaneRect.Top);

    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(PaneSize, FSlateLayoutTransform(FVector2D(PaneRect.Left, PaneRect.Top))),
        WhiteBrush, ESlateDrawEffect::None, WTBRGraphPaneBg);

    TArray<FVector2D> Points;

    for (int32 i = 0; i <= WTBRGraphForwardGridLines; ++i)
    {
        const float X = FMath::Lerp(PaneRect.Left, PaneRect.Right,
            static_cast<float>(i) / static_cast<float>(WTBRGraphForwardGridLines));
        Points.Reset(2);
        Points.Add(FVector2D(X, PaneRect.Top));
        Points.Add(FVector2D(X, PaneRect.Bottom));
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
            WTBRGraphGrid, true, 1.0f);
    }

    for (int32 i = 0; i <= WTBRGraphLateralGridLines; ++i)
    {
        const float Alpha = static_cast<float>(i) / static_cast<float>(WTBRGraphLateralGridLines);
        const float Y = FMath::Lerp(PaneRect.Top, PaneRect.Bottom, Alpha);
        // The centre line is the zero axis — the lane's own reference, so it is
        // drawn brighter than the rest of the grid.
        const bool bIsZeroAxis = FMath::IsNearlyEqual(Alpha, 0.5f, 0.01f);
        Points.Reset(2);
        Points.Add(FVector2D(PaneRect.Left, Y));
        Points.Add(FVector2D(PaneRect.Right, Y));
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
            bIsZeroAxis ? WTBRGraphAxis : WTBRGraphGrid, true, bIsZeroAxis ? 1.5f : 1.0f);
    }

    FText PaneLabel = bElevationPane
        ? FText::FromString(TEXT("ELEVATION  ->forward / ^up"))
        : FText::FromString(TEXT("PLAN  ->forward / ^left"));
    FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(FVector2D(260.0f, 16.0f),
            FSlateLayoutTransform(FVector2D(PaneRect.Left + 6.0f, PaneRect.Top + 4.0f))),
        PaneLabel, SmallFont, ESlateDrawEffect::None, WTBRGraphLabel);

    for (int32 LaneIndex = 0; LaneIndex < DisplayPreset.Lanes.Num(); ++LaneIndex)
    {
        const bool bSelected = (LaneIndex == SelectedLaneIndex);
        const FLinearColor LaneColor = bSelected ? WTBRGraphLaneSelected : WTBRGraphLane;

        TArray<FVector2D> LanePoints;
        ComputeLaneScreenPositions(LaneIndex, PaneRect, bElevationPane, LanePoints);
        if (LanePoints.Num() < 2) continue;

        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), LanePoints, ESlateDrawEffect::None,
            LaneColor, true, bSelected ? 2.6f : 1.6f);

        for (int32 PointIndex = 0; PointIndex < LanePoints.Num(); ++PointIndex)
        {
            // Waypoint 0 is the caster and can never be dragged — it is drawn as a
            // distinct anchor rather than a handle so that reads before anyone tries.
            const bool bIsCaster = (PointIndex == 0);
            const float HandleSize = bIsCaster ? 7.0f : (bSelected ? 8.0f : 6.0f);
            const FLinearColor HandleColor = bIsCaster ? WTBRGraphCaster : LaneColor;

            FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(HandleSize, HandleSize),
                    FSlateLayoutTransform(LanePoints[PointIndex] - FVector2D(HandleSize * 0.5f, HandleSize * 0.5f))),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HandleColor);
        }
    }
}

int32 UWTBRPathGraphViewWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    FSlateRect PlanRect;
    FSlateRect ElevationRect;
    ComputePaneRects(AllottedGeometry.GetLocalSize(), PlanRect, ElevationRect);

    PaintPane(AllottedGeometry, OutDrawElements, LayerId, PlanRect, /*bElevationPane=*/false);
    PaintPane(AllottedGeometry, OutDrawElements, LayerId, ElevationRect, /*bElevationPane=*/true);

    if (!HeaderText.IsEmpty())
    {
        FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(FVector2D(900.0f, 20.0f),
                FSlateLayoutTransform(FVector2D(PlanRect.Left, FMath::Max(PlanRect.Top - 18.0f, 0.0f)))),
            HeaderText, FCoreStyle::GetDefaultFontStyle("Regular", 11),
            ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
    }

    return LayerId;
}
