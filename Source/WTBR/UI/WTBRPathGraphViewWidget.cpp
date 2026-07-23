// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRPathGraphViewWidget.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
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

FVector UWTBRPathGraphViewWidget::UnprojectPaneToNormalized(
    const FVector2D& LocalPoint,
    const FSlateRect& PaneRect,
    bool bElevationPane,
    float InForwardMin,
    float InForwardMax,
    float InLateralExtent,
    const FVector& PreservedFrom)
{
    const float PaneWidth = FMath::Max(PaneRect.Right - PaneRect.Left, KINDA_SMALL_NUMBER);
    const float PaneHeight = FMath::Max(PaneRect.Bottom - PaneRect.Top, KINDA_SMALL_NUMBER);

    const float ForwardAlpha = FMath::Clamp(
        static_cast<float>(LocalPoint.X - PaneRect.Left) / PaneWidth, 0.0f, 1.0f);
    const float Forward = FMath::Lerp(InForwardMin, InForwardMax, ForwardAlpha);

    // Inverse of the Bottom->Top lerp in ProjectNormalizedToPane: screen Y grows
    // downward, so alpha is measured up from the pane's bottom edge.
    const float LateralAlpha = FMath::Clamp(
        static_cast<float>(PaneRect.Bottom - LocalPoint.Y) / PaneHeight, 0.0f, 1.0f);
    const float Extent = FMath::Max(InLateralExtent, KINDA_SMALL_NUMBER);
    const float Lateral = FMath::Lerp(-Extent, Extent, LateralAlpha);

    // The axis this pane does not show is carried over untouched.
    return bElevationPane
        ? FVector(Forward, PreservedFrom.Y, Lateral)
        : FVector(Forward, Lateral, PreservedFrom.Z);
}

TSharedRef<SWidget> UWTBRPathGraphViewWidget::RebuildWidget()
{
    return SNew(SBox);
}

void UWTBRPathGraphViewWidget::SetPreset(const FWTBRPathPreset& InPreset)
{
    DisplayPreset = InPreset;
    SelectedLaneIndex = FMath::Clamp(SelectedLaneIndex, 0, FMath::Max(0, DisplayPreset.Lanes.Num() - 1));
    ActiveDrag = FWTBRPathGraphHandleHit();
    ApplyEditingVisibility();
}

void UWTBRPathGraphViewWidget::SetAllowEditing(bool bInAllowEditing)
{
    bAllowEditing = bInAllowEditing;
    if (!bAllowEditing)
    {
        ActiveDrag = FWTBRPathGraphHandleHit();
    }
    ApplyEditingVisibility();
}

void UWTBRPathGraphViewWidget::ApplyEditingVisibility()
{
    // A HitTestInvisible widget never receives mouse events at all, so an editable
    // graph has to be fully Visible. Left non-interactive otherwise so a read-only
    // preview cannot swallow clicks meant for whatever is behind it.
    SetVisibility(bAllowEditing ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
}

bool UWTBRPathGraphViewWidget::CanAddWaypointToLane(int32 LaneIndex) const
{
    return DisplayPreset.Lanes.IsValidIndex(LaneIndex)
        && DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints.Num() < WTBR_MAX_CUSTOM_LANE_WAYPOINTS;
}

bool UWTBRPathGraphViewWidget::CanDeleteWaypoint(int32 LaneIndex, int32 WaypointIndex) const
{
    if (!DisplayPreset.Lanes.IsValidIndex(LaneIndex)) return false;

    const TArray<FVector>& Waypoints = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;
    if (!Waypoints.IsValidIndex(WaypointIndex)) return false;

    // Index 0 is the caster anchor, and two points are the minimum that describes a
    // path — ResolvePathPreset skips any lane with fewer.
    return WaypointIndex > 0 && Waypoints.Num() > 2;
}

bool UWTBRPathGraphViewWidget::TryMoveWaypoint(
    int32 LaneIndex, int32 WaypointIndex, const FVector& NewNormalized)
{
    if (!DisplayPreset.Lanes.IsValidIndex(LaneIndex)) return false;

    TArray<FVector>& Waypoints = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;
    if (!Waypoints.IsValidIndex(WaypointIndex)) return false;
    if (WaypointIndex == 0) return false;

    const float Extent = FMath::Max(LateralExtent, KINDA_SMALL_NUMBER);
    Waypoints[WaypointIndex] = FVector(
        FMath::Clamp(NewNormalized.X, ForwardMin, ForwardMax),
        FMath::Clamp(NewNormalized.Y, -Extent, Extent),
        FMath::Clamp(NewNormalized.Z, -Extent, Extent));

    OnPresetEdited.Broadcast();
    return true;
}

bool UWTBRPathGraphViewWidget::TryInsertWaypoint(
    int32 LaneIndex, int32 InsertBeforeIndex, const FVector& Normalized)
{
    if (!CanAddWaypointToLane(LaneIndex)) return false;

    TArray<FVector>& Waypoints = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;

    // Never before the caster and never past the end: a new bend belongs somewhere
    // along the existing path, not in place of where the lane starts.
    const int32 ClampedIndex = FMath::Clamp(InsertBeforeIndex, 1, Waypoints.Num());

    const float Extent = FMath::Max(LateralExtent, KINDA_SMALL_NUMBER);
    Waypoints.Insert(FVector(
        FMath::Clamp(Normalized.X, ForwardMin, ForwardMax),
        FMath::Clamp(Normalized.Y, -Extent, Extent),
        FMath::Clamp(Normalized.Z, -Extent, Extent)), ClampedIndex);

    OnPresetEdited.Broadcast();
    return true;
}

bool UWTBRPathGraphViewWidget::TryDeleteWaypoint(int32 LaneIndex, int32 WaypointIndex)
{
    if (!CanDeleteWaypoint(LaneIndex, WaypointIndex)) return false;

    DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints.RemoveAt(WaypointIndex);
    ActiveDrag = FWTBRPathGraphHandleHit();

    OnPresetEdited.Broadcast();
    return true;
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

void UWTBRPathGraphViewWidget::ComputeLaneCurvePositions(
    int32 LaneIndex, const FSlateRect& PaneRect, bool bElevationPane,
    TArray<FVector2D>& OutPositions) const
{
    OutPositions.Reset();
    if (!DisplayPreset.Lanes.IsValidIndex(LaneIndex)) return;

    // Smoothed with the SAME function the flight path uses, in the same (normalized)
    // space, so the drawn line is the flown line rather than an approximation of it.
    // The projection below is affine, so sampling before it gives the identical curve
    // ResolvePathPreset produces by sampling before its own world transform.
    const TArray<FVector>& Authored = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;
    TArray<FVector> Sampled;
    if (bSmoothPreview)
    {
        UWTBRCompositeRegistryDataAsset::SampleSmoothPath(Authored, Sampled);
    }
    const TArray<FVector>& Waypoints = bSmoothPreview ? Sampled : Authored;

    OutPositions.Reserve(Waypoints.Num());
    for (const FVector& Waypoint : Waypoints)
    {
        OutPositions.Add(ProjectNormalizedToPane(
            Waypoint, PaneRect, bElevationPane, ForwardMin, ForwardMax, LateralExtent));
    }
}

void UWTBRPathGraphViewWidget::ComputeLaneHandlePositions(
    int32 LaneIndex, const FSlateRect& PaneRect, bool bElevationPane,
    TArray<FVector2D>& OutPositions) const
{
    OutPositions.Reset();
    if (!DisplayPreset.Lanes.IsValidIndex(LaneIndex)) return;

    const TArray<FVector>& Authored = DisplayPreset.Lanes[LaneIndex].NormalizedWaypoints;
    OutPositions.Reserve(Authored.Num());
    for (const FVector& Waypoint : Authored)
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

    // "right", not "left": +Y is the character's RIGHT in Unreal's left-handed axes
    // (GetActorRightVector returns the Y axis), and this pane draws +Y upward. So
    // dragging a handle toward the top of the plan pane moves the lane to the
    // player's right — which is what Step 4's drag has to agree with.
    //
    // Whether up SHOULD be right is a UX call, not a correctness one: mirroring the
    // pane (drawing +Y downward) gives the more familiar "looking down at a map"
    // reading, at the cost of no longer matching the axis sign directly. Flip
    // ProjectNormalizedToPane's lateral term if that feels better once dragging is
    // in — but flip this label with it.
    FText PaneLabel = bElevationPane
        ? FText::FromString(TEXT("ELEVATION  ->forward / ^up (+Z)"))
        : FText::FromString(TEXT("PLAN  ->forward / ^right (+Y)"));
    FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(FVector2D(260.0f, 16.0f),
            FSlateLayoutTransform(FVector2D(PaneRect.Left + 6.0f, PaneRect.Top + 4.0f))),
        PaneLabel, SmallFont, ESlateDrawEffect::None, WTBRGraphLabel);

    for (int32 LaneIndex = 0; LaneIndex < DisplayPreset.Lanes.Num(); ++LaneIndex)
    {
        const bool bSelected = (LaneIndex == SelectedLaneIndex);
        const FLinearColor LaneColor = bSelected ? WTBRGraphLaneSelected : WTBRGraphLane;

        TArray<FVector2D> CurvePoints;
        ComputeLaneCurvePositions(LaneIndex, PaneRect, bElevationPane, CurvePoints);
        if (CurvePoints.Num() < 2) continue;

        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(), CurvePoints, ESlateDrawEffect::None,
            LaneColor, true, bSelected ? 2.6f : 1.6f);

        // Handles come from the AUTHORED waypoints, never the curve samples — one
        // handle per point the player actually placed.
        TArray<FVector2D> LanePoints;
        ComputeLaneHandlePositions(LaneIndex, PaneRect, bElevationPane, LanePoints);

        for (int32 PointIndex = 0; PointIndex < LanePoints.Num(); ++PointIndex)
        {
            // Waypoint 0 is the caster and can never be dragged — it is drawn as a
            // distinct anchor rather than a handle so that reads before anyone tries.
            const bool bIsCaster = (PointIndex == 0);
            const bool bIsDragged = ActiveDrag.IsValid()
                && ActiveDrag.LaneIndex == LaneIndex
                && ActiveDrag.WaypointIndex == PointIndex
                && ActiveDrag.bElevationPane == bElevationPane;

            const float HandleSize = bIsCaster ? 7.0f : (bIsDragged ? 11.0f : (bSelected ? 8.0f : 6.0f));
            const FLinearColor HandleColor = bIsCaster ? WTBRGraphCaster
                : (bIsDragged ? FLinearColor::White : LaneColor);

            FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(HandleSize, HandleSize),
                    FSlateLayoutTransform(LanePoints[PointIndex] - FVector2D(HandleSize * 0.5f, HandleSize * 0.5f))),
                FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, HandleColor);
        }
    }
}

bool UWTBRPathGraphViewWidget::FindPaneForPoint(
    const FVector2D& LocalPoint, const FVector2D& LocalSize,
    FSlateRect& OutPaneRect, bool& bOutElevationPane) const
{
    FSlateRect PlanRect;
    FSlateRect ElevationRect;
    ComputePaneRects(LocalSize, PlanRect, ElevationRect);

    auto Contains = [](const FSlateRect& Rect, const FVector2D& Point)
    {
        return Point.X >= Rect.Left && Point.X <= Rect.Right
            && Point.Y >= Rect.Top && Point.Y <= Rect.Bottom;
    };

    if (Contains(PlanRect, LocalPoint))
    {
        OutPaneRect = PlanRect;
        bOutElevationPane = false;
        return true;
    }
    if (Contains(ElevationRect, LocalPoint))
    {
        OutPaneRect = ElevationRect;
        bOutElevationPane = true;
        return true;
    }
    return false;
}

bool UWTBRPathGraphViewWidget::HitTestHandle(
    const FVector2D& LocalPoint, const FVector2D& LocalSize,
    FWTBRPathGraphHandleHit& OutHit) const
{
    FSlateRect PaneRect;
    bool bElevationPane = false;
    if (!FindPaneForPoint(LocalPoint, LocalSize, PaneRect, bElevationPane)) return false;

    const float RadiusSq = HandleGrabRadius * HandleGrabRadius;

    // Selected lane first, then the rest — a click on overlapping handles should keep
    // the player on the lane they are already editing rather than switching under them.
    TArray<int32> SearchOrder;
    SearchOrder.Reserve(DisplayPreset.Lanes.Num());
    if (DisplayPreset.Lanes.IsValidIndex(SelectedLaneIndex))
    {
        SearchOrder.Add(SelectedLaneIndex);
    }
    for (int32 LaneIndex = 0; LaneIndex < DisplayPreset.Lanes.Num(); ++LaneIndex)
    {
        if (LaneIndex != SelectedLaneIndex) SearchOrder.Add(LaneIndex);
    }

    for (const int32 LaneIndex : SearchOrder)
    {
        TArray<FVector2D> Handles;
        ComputeLaneHandlePositions(LaneIndex, PaneRect, bElevationPane, Handles);

        int32 BestIndex = INDEX_NONE;
        float BestDistSq = RadiusSq;
        // From 1: waypoint 0 is the caster and is never grabbable.
        for (int32 PointIndex = 1; PointIndex < Handles.Num(); ++PointIndex)
        {
            const float DistSq = static_cast<float>(
                FVector2D::DistSquared(Handles[PointIndex], LocalPoint));
            if (DistSq <= BestDistSq)
            {
                BestDistSq = DistSq;
                BestIndex = PointIndex;
            }
        }

        if (BestIndex != INDEX_NONE)
        {
            OutHit.LaneIndex = LaneIndex;
            OutHit.WaypointIndex = BestIndex;
            OutHit.bElevationPane = bElevationPane;
            return true;
        }
    }
    return false;
}

int32 UWTBRPathGraphViewWidget::FindInsertIndexForPoint(
    int32 LaneIndex, const FVector2D& LocalPoint,
    const FSlateRect& PaneRect, bool bElevationPane) const
{
    TArray<FVector2D> Handles;
    ComputeLaneHandlePositions(LaneIndex, PaneRect, bElevationPane, Handles);
    if (Handles.Num() < 2) return 1;

    int32 BestSegment = 0;
    float BestDist = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < Handles.Num() - 1; ++Index)
    {
        const float Dist = FMath::PointDistToSegment(
            FVector(LocalPoint.X, LocalPoint.Y, 0.0f),
            FVector(Handles[Index].X, Handles[Index].Y, 0.0f),
            FVector(Handles[Index + 1].X, Handles[Index + 1].Y, 0.0f));
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestSegment = Index;
        }
    }

    // Insert BETWEEN the two ends of the nearest segment.
    return BestSegment + 1;
}

FReply UWTBRPathGraphViewWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bAllowEditing)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    // Slate hands mouse events in absolute space; AbsoluteToLocal is the whole of the
    // conversion and is already DPI-correct. (The manual viewport-scale divide in
    // WTBRMarkPingHUDWidget exists because ProjectWorldLocationToScreen is not a
    // Slate API — it does not apply here.)
    const FVector2D LocalPoint = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const FVector2D LocalSize = InGeometry.GetLocalSize();

    FWTBRPathGraphHandleHit Hit;
    const bool bHitHandle = HitTestHandle(LocalPoint, LocalSize, Hit);

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (bHitHandle)
        {
            SelectedLaneIndex = Hit.LaneIndex;
            TryDeleteWaypoint(Hit.LaneIndex, Hit.WaypointIndex);
        }
        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    if (bHitHandle)
    {
        SelectedLaneIndex = Hit.LaneIndex;
        ActiveDrag = Hit;

        FReply Reply = FReply::Handled();
        if (TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
        {
            // Capture so a drag that leaves the pane still tracks, and so the button-up
            // arrives here rather than wherever the cursor happened to end up.
            Reply = Reply.CaptureMouse(CachedWidget.ToSharedRef());
        }
        return Reply;
    }

    // Empty space inside a pane: add a bend to the selected lane where clicked.
    FSlateRect PaneRect;
    bool bElevationPane = false;
    if (FindPaneForPoint(LocalPoint, LocalSize, PaneRect, bElevationPane)
        && DisplayPreset.Lanes.IsValidIndex(SelectedLaneIndex))
    {
        // The caster handle is not grabbable, so a click on it would otherwise fall
        // through to here and drop a new waypoint right on top of the muzzle — which
        // reads as "clicking the caster spawns a point", the opposite of the "you
        // cannot touch the caster" rule the handle's own appearance promises.
        TArray<FVector2D> Handles;
        ComputeLaneHandlePositions(SelectedLaneIndex, PaneRect, bElevationPane, Handles);
        if (Handles.Num() > 0
            && FVector2D::DistSquared(Handles[0], LocalPoint) <= HandleGrabRadius * HandleGrabRadius)
        {
            return FReply::Handled();
        }

        const int32 InsertIndex = FindInsertIndexForPoint(
            SelectedLaneIndex, LocalPoint, PaneRect, bElevationPane);

        // Seeded from the neighbour it is inserted after, so the axis this pane does
        // not show starts somewhere sensible instead of at zero — a bend added in the
        // plan view must not flatten the lane's height.
        const TArray<FVector>& Waypoints = DisplayPreset.Lanes[SelectedLaneIndex].NormalizedWaypoints;
        const FVector Seed = Waypoints.IsValidIndex(InsertIndex - 1)
            ? Waypoints[InsertIndex - 1] : FVector::ZeroVector;

        const FVector Normalized = UnprojectPaneToNormalized(
            LocalPoint, PaneRect, bElevationPane, ForwardMin, ForwardMax, LateralExtent, Seed);

        TryInsertWaypoint(SelectedLaneIndex, InsertIndex, Normalized);
    }
    return FReply::Handled();
}

FReply UWTBRPathGraphViewWidget::NativeOnMouseMove(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bAllowEditing || !ActiveDrag.IsValid())
    {
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    }

    const FVector2D LocalPoint = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    // The drag stays bound to the pane it STARTED in. Re-resolving the pane per frame
    // would let a drag that wanders across the gap silently switch which axis it is
    // editing, halfway through a gesture.
    FSlateRect PlanRect;
    FSlateRect ElevationRect;
    ComputePaneRects(InGeometry.GetLocalSize(), PlanRect, ElevationRect);
    const FSlateRect& PaneRect = ActiveDrag.bElevationPane ? ElevationRect : PlanRect;

    // Validated before binding a reference: the ternary form here would have bound a
    // const reference to a temporary empty array on the miss branch.
    if (!DisplayPreset.Lanes.IsValidIndex(ActiveDrag.LaneIndex))
    {
        ActiveDrag = FWTBRPathGraphHandleHit();
        return FReply::Handled();
    }

    const TArray<FVector>& Waypoints = DisplayPreset.Lanes[ActiveDrag.LaneIndex].NormalizedWaypoints;
    if (!Waypoints.IsValidIndex(ActiveDrag.WaypointIndex))
    {
        ActiveDrag = FWTBRPathGraphHandleHit();
        return FReply::Handled();
    }

    const FVector Normalized = UnprojectPaneToNormalized(
        LocalPoint, PaneRect, ActiveDrag.bElevationPane,
        ForwardMin, ForwardMax, LateralExtent, Waypoints[ActiveDrag.WaypointIndex]);

    TryMoveWaypoint(ActiveDrag.LaneIndex, ActiveDrag.WaypointIndex, Normalized);
    return FReply::Handled();
}

FReply UWTBRPathGraphViewWidget::NativeOnMouseButtonUp(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!ActiveDrag.IsValid())
    {
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    }

    ActiveDrag = FWTBRPathGraphHandleHit();
    return FReply::Handled().ReleaseMouseCapture();
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

    FString StatusLine = HeaderText.IsEmpty() ? FString() : HeaderText.ToString();

    if (bAllowEditing && DisplayPreset.Lanes.IsValidIndex(SelectedLaneIndex))
    {
        // The cap has to be visible, not just enforced. Silently ignoring a click
        // once a lane is full is indistinguishable from the editor being broken.
        const int32 Used = DisplayPreset.Lanes[SelectedLaneIndex].NormalizedWaypoints.Num();
        StatusLine += FString::Printf(
            TEXT("   |   lane %d: %d/%d waypoints%s   |   drag to move, click path to add, right-click to remove"),
            SelectedLaneIndex, Used, WTBR_MAX_CUSTOM_LANE_WAYPOINTS,
            CanAddWaypointToLane(SelectedLaneIndex) ? TEXT("") : TEXT(" (FULL)"));
    }

    if (!StatusLine.IsEmpty())
    {
        FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(FVector2D(1600.0f, 20.0f),
                FSlateLayoutTransform(FVector2D(PlanRect.Left, FMath::Max(PlanRect.Top - 18.0f, 0.0f)))),
            FText::FromString(StatusLine), FCoreStyle::GetDefaultFontStyle("Regular", 11),
            ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
    }

    return LayerId;
}
