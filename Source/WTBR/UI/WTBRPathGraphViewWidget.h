// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRPathGraphViewWidget.generated.h"

/**
 * Two-pane 2D view of a path preset's lanes: PLAN (forward/lateral, looking down)
 * above ELEVATION (forward/vertical, looking from the side), sharing one forward
 * axis. The editing surface of the Composite Preset Editor.
 *
 * Fully native NativePaint with no Blueprint layout asset, matching every other
 * geometric widget in this project (UWTBRTriggerWheelWidget, the two preset wheels,
 * the radar) — see UWTBRTriggerWheelWidget's doc comment for the project rule this
 * follows.
 *
 * ⚠ Lanes are drawn as STRAIGHT POLYLINES through their waypoints, and that is not
 * a simplification: ResolvePathPreset transforms each waypoint independently and
 * InterpToMovementComponent interpolates linearly between control points, so there
 * is no curve anywhere in the real flight path. An earlier design mockup drew smooth
 * Beziers; that was visual polish that never reached gameplay, and copying it here
 * would make the preview lie about where the shot actually goes.
 *
 * Read-only in this pass. Drag editing, waypoint add/delete and the waypoint cap
 * arrive next and will hit-test against ComputeLaneScreenPositions, which paint
 * already uses — one source of truth for where a handle IS, so the drawn position
 * and the grabbable position cannot drift apart.
 */
UCLASS()
class WTBR_API UWTBRPathGraphViewWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Display bounds in normalized (fraction-of-range) space. Every authored preset
    // today sits inside these: the widest baked lane reaches forward 1.14, lateral
    // 0.54 and vertical 0.64. Purely a view concern — the data model itself has no
    // bounds, and the server clamps far wider than this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph")
    float ForwardMin = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph")
    float ForwardMax = 1.25f;

    // Half-height of a pane in normalized units: lateral spans +/-this in PLAN,
    // vertical spans +/-this in ELEVATION. One number for both so the two panes
    // share a scale and a lane's shape reads honestly between them.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph",
        meta = (ClampMin = "0.05"))
    float LateralExtent = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph",
        meta = (ClampMin = "0.0"))
    float PaneGap = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph",
        meta = (ClampMin = "0.0"))
    float PanePadding = 26.0f;

    // Must match how the previewed weapon actually flies: every non-Viper bullet
    // curves, anything with a Viper in it stays sharp. Defaults to true because the
    // editor's v1 target (Venyx) is a curved archetype — set it false before showing
    // a Viper or Viper-composite preset, or the preview draws a shape the shot will
    // not fly. See UWTBRCompositeRegistryDataAsset::UsesSharpPath.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph")
    bool bSmoothPreview = true;

    void SetPreset(const FWTBRPathPreset& InPreset);
    void SetSelectedLaneIndex(int32 InLaneIndex);

    // Free-text provenance line drawn along the top ("which preset, from which
    // Trigger"). Without it the view is anonymous, and a graph that silently shows
    // a DIFFERENT preset than the one asked for is indistinguishable from a graph
    // that is drawing the right preset wrongly.
    void SetHeaderText(const FText& InHeaderText) { HeaderText = InHeaderText; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    int32 GetSelectedLaneIndex() const { return SelectedLaneIndex; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    int32 GetLaneCount() const { return DisplayPreset.Lanes.Num(); }

    /**
     * Maps a normalized waypoint into local widget pixels inside one pane.
     *
     * Static and fully parameterised so the projection can be unit-tested without a
     * viewport — the sign of the vertical axis in particular is exactly the kind of
     * thing that looks fine until someone authors an asymmetric preset. Screen Y
     * grows DOWNWARD, so a positive lateral/vertical value must produce a SMALLER
     * screen Y.
     *
     * bElevationPane selects which normalized component is the vertical axis:
     * Z for the elevation pane, Y for the plan pane.
     */
    static FVector2D ProjectNormalizedToPane(
        const FVector& Normalized,
        const FSlateRect& PaneRect,
        bool bElevationPane,
        float InForwardMin,
        float InForwardMax,
        float InLateralExtent);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // Pane rects derived from the widget's own allotted size. Both are computed in
    // one place so paint and (next pass) hit-testing agree on where each pane is.
    void ComputePaneRects(const FVector2D& LocalSize, FSlateRect& OutPlan, FSlateRect& OutElevation) const;

    // The drawn LINE: smoothed to dense points when the previewed weapon curves, so
    // this is what the shot's real trajectory looks like. Not grabbable — a curve has
    // far more points than the player authored.
    void ComputeLaneCurvePositions(int32 LaneIndex, const FSlateRect& PaneRect,
        bool bElevationPane, TArray<FVector2D>& OutPositions) const;

    // The GRABBABLE points: exactly the waypoints the player authored, one handle
    // each, never the smoothing samples.
    //
    // Shared by paint and, from the next pass, by drag hit-testing — two
    // implementations of "where is this handle" would let the drawn position and the
    // grabbable position drift apart.
    void ComputeLaneHandlePositions(int32 LaneIndex, const FSlateRect& PaneRect,
        bool bElevationPane, TArray<FVector2D>& OutPositions) const;

    void PaintPane(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
        int32& LayerId, const FSlateRect& PaneRect, bool bElevationPane) const;

    UPROPERTY(Transient)
    FWTBRPathPreset DisplayPreset;

    FText HeaderText;

    int32 SelectedLaneIndex = 0;
};
