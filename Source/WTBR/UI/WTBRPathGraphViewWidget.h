// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRPathGraphViewWidget.generated.h"

// Broadcast after any successful edit. Non-dynamic because every consumer is C++
// (the editor root widget owns the draft this view mutates).
DECLARE_MULTICAST_DELEGATE(FWTBRPathGraphPresetEdited);

// Broadcast when the selected lane changes, so the lane rail's highlight follows a
// number-key press made while the graph has focus.
DECLARE_MULTICAST_DELEGATE_OneParam(FWTBRPathGraphLaneSelected, int32 /*LaneIndex*/);

// Broadcast when the selected WAYPOINT changes. The properties panel shows a
// different set of fields per waypoint — the start carries the shot's cube count and
// launch delay, a mid waypoint carries what happens when the cube reaches it, and
// the last carries nothing at all — so it has to follow this.
DECLARE_MULTICAST_DELEGATE_OneParam(FWTBRPathGraphWaypointSelected, int32 /*WaypointIndex*/);

/** Which handle, in which pane, a screen point landed on. */
struct FWTBRPathGraphHandleHit
{
    int32 LaneIndex = INDEX_NONE;
    int32 WaypointIndex = INDEX_NONE;
    bool bElevationPane = false;

    bool IsValid() const { return LaneIndex != INDEX_NONE && WaypointIndex != INDEX_NONE; }
};

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
 * ⚠ The drawn line comes from the SAME sampler the flight path uses
 * (UWTBRCompositeRegistryDataAsset::SampleSmoothPath, via bSmoothPreview), so the
 * shape on screen is the shape that flies. Anything that changes how a bullet moves
 * has to change here too, or the preview quietly starts lying.
 *
 * Editing is split deliberately:
 *  - The RULES (what may move, what may be added or deleted, the waypoint cap) are
 *    geometry-free public methods — TryMoveWaypoint / TryInsertWaypoint /
 *    TryDeleteWaypoint / CanAddWaypointToLane — so they are unit-testable without a
 *    viewport, and a future toolbar can grey buttons out using the same checks the
 *    mouse path enforces.
 *  - The PIXELS (hit-testing, unprojection) are the thin layer on top, and are the
 *    only part that genuinely needs PIE.
 *
 * This is the project's first cursor-driven UI. Every other interactive widget here
 * (both preset wheels, the trigger wheel) reads accumulated GetInputMouseDelta with
 * the OS cursor hidden, because they run during gameplay. This one needs real click
 * positions, which is only safe because the editor is lobby-only — see the Preset
 * Editor design notes for why that scope was chosen.
 */
UCLASS()
class WTBR_API UWTBRPathGraphViewWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UWTBRPathGraphViewWidget(const FObjectInitializer& ObjectInitializer);

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

    // Editing is OFF by default so a preview can never be nudged by a stray click.
    // Turning it on also flips the widget to a hit-testable visibility.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph")
    bool bAllowEditing = false;

    // Local-space pixels within which a click counts as grabbing a handle. Generous
    // on purpose: a handle is drawn ~8px across, and demanding pixel accuracy on a
    // point the player is trying to drag is the fastest way to make an editor feel
    // broken.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph",
        meta = (ClampMin = "1.0"))
    float HandleGrabRadius = 14.0f;

    // Only the selected lane may be grabbed; the others are drawn for context and
    // are inert. Owner's call after driving Step 4: lanes overlap constantly in the
    // plan view, and a click meant for the lane being edited would otherwise pick up
    // a neighbour and move somebody else's shape. Select with the number keys or the
    // lane rail instead of by grabbing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Path Graph")
    bool bLockUnselectedLanes = true;

    FWTBRPathGraphPresetEdited OnPresetEdited;
    FWTBRPathGraphLaneSelected OnLaneSelected;
    FWTBRPathGraphWaypointSelected OnWaypointSelected;

    // Use this rather than writing bAllowEditing directly: the flag also decides the
    // widget's visibility, and a HitTestInvisible widget receives no mouse events at
    // all, so setting the bool alone produces an "editable" graph that ignores every
    // click.
    void SetAllowEditing(bool bInAllowEditing);

    void SetPreset(const FWTBRPathPreset& InPreset);
    void SetSelectedLaneIndex(int32 InLaneIndex);

    const FWTBRPathPreset& GetPreset() const { return DisplayPreset; }

    // ── Editing rules. Geometry-free on purpose: every constraint lives here, so the
    // mouse path and any future toolbar enforce the same thing, and all of it is
    // testable without a viewport. Each returns false and changes nothing on refusal.

    /**
     * Moves one waypoint, clamped to the view's own bounds.
     *
     * Waypoint 0 is refused outright: it is the caster, and ResolvePathPreset treats
     * a lane starting anywhere else as invalid (the server's sanitizer drops such a
     * lane entirely rather than guessing). Letting it move would author presets that
     * silently vanish on upload.
     */
    bool TryMoveWaypoint(int32 LaneIndex, int32 WaypointIndex, const FVector& NewNormalized);

    /** Inserts a bend. Refused once the lane is at WTBR_MAX_CUSTOM_LANE_WAYPOINTS. */
    bool TryInsertWaypoint(int32 LaneIndex, int32 InsertBeforeIndex, const FVector& Normalized);

    /**
     * Deletes a bend. Waypoint 0 is refused (see TryMoveWaypoint) and so is any
     * delete that would leave fewer than two waypoints, because a lane needs a start
     * and an end to describe a path at all.
     */
    bool TryDeleteWaypoint(int32 LaneIndex, int32 WaypointIndex);

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool CanAddWaypointToLane(int32 LaneIndex) const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool CanDeleteWaypoint(int32 LaneIndex, int32 WaypointIndex) const;

    /**
     * Adds a lane, seeded as a straight shot to the front of the view so it is
     * immediately visible and draggable rather than a zero-length stub the player
     * has to find. Refused at WTBR_MAX_CUSTOM_LANES, which is the same ceiling the
     * server's sanitizer truncates at.
     */
    bool TryAddLane();

    /** Refused on the last lane: a preset with no lanes fires nothing. */
    bool TryDeleteLane(int32 LaneIndex);

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool CanAddLane() const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool CanDeleteLane() const;

    /**
     * Copies the SCALAR settings of a lane — count, delay, formation — from Source,
     * clamping each. Deliberately ignores Source's waypoints and events: the rail
     * edits numbers and the graph edits shape, and one panel silently overwriting
     * the other's work is the bug this shape prevents outright.
     *
     * CubeCount is additionally clamped against the WHOLE preset's remaining budget,
     * not just its own range — see GetRemainingCubeBudgetForLane.
     */
    bool TryUpdateLaneScalars(int32 LaneIndex, const FWTBRPathLane& Source);

    /**
     * Largest CubeCount this lane may take without the preset exceeding
     * WTBR_MAX_CUSTOM_PRESET_CUBES across every lane. Always at least 1: a lane that
     * fires nothing is not a lane.
     */
    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    int32 GetRemainingCubeBudgetForLane(int32 LaneIndex) const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    int32 GetTotalCubeCount() const;

    // ── Waypoint selection and the markers pinned to a waypoint ────────────────
    //
    // What a waypoint can carry depends on WHERE it sits, and that is the whole
    // model: the start is where the shot leaves (how many cubes, how long it waits),
    // a middle waypoint is somewhere the cube passes through (so it can pause there,
    // or change whether it is hunting), and the last is only a destination — there
    // is nothing left to schedule once the lane has arrived.

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    int32 GetSelectedWaypointIndex() const { return SelectedWaypointIndex; }

    void SetSelectedWaypointIndex(int32 InWaypointIndex);

    /** True for a waypoint that is neither the caster nor the landing point. */
    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool IsMidWaypoint(int32 LaneIndex, int32 WaypointIndex) const;

    /** Seconds the cube pauses at this waypoint; zero when it has no pause marker. */
    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    float GetWaypointHoverSeconds(int32 LaneIndex, int32 WaypointIndex) const;

    /** Zero removes the marker entirely rather than leaving a pause of no length. */
    bool TrySetWaypointHoverSeconds(int32 LaneIndex, int32 WaypointIndex, float Seconds);

    /**
     * Homing at this waypoint, as three states rather than two: no marker at all
     * (whatever was happening continues), switch hunting ON here, or switch it OFF.
     *
     * "Off until half way, then on" — the shape flies as drawn and only commits
     * late — needs exactly this: an OFF marker at the start and an ON marker part
     * way along. A plain bool could not say "leave it alone".
     */
    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool HasWaypointHomingMarker(int32 LaneIndex, int32 WaypointIndex) const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Path Graph")
    bool GetWaypointHomingEnabled(int32 LaneIndex, int32 WaypointIndex) const;

    bool TrySetWaypointHoming(int32 LaneIndex, int32 WaypointIndex, bool bMarkerPresent, bool bEnable);

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

    /**
     * The exact inverse of ProjectNormalizedToPane, and the reason dragging works.
     *
     * A pane only shows two of the three axes, so a drag must not touch the third.
     * PreservedFrom supplies it: a plan-pane drag writes X and Y and keeps that
     * waypoint's existing Z, an elevation drag writes X and Z and keeps Y. Doing the
     * merge in here rather than at each call site is what stops a drag in one view
     * from flattening the shape in the other.
     *
     * Clamped to the view's bounds, so a handle cannot be dragged off the pane into
     * a value the player cannot see.
     */
    static FVector UnprojectPaneToNormalized(
        const FVector2D& LocalPoint,
        const FSlateRect& PaneRect,
        bool bElevationPane,
        float InForwardMin,
        float InForwardMax,
        float InLateralExtent,
        const FVector& PreservedFrom);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // Number keys 1-9 select a lane. Chosen over cycling because the rail shows the
    // numbers, so the shortcut is discoverable from the panel rather than learned.
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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

    // Nearest grabbable handle to a local-space point, within HandleGrabRadius.
    // The SELECTED lane is searched first, so overlapping handles resolve in favour
    // of the lane the player is already working on instead of jumping selection.
    bool HitTestHandle(const FVector2D& LocalPoint, const FVector2D& LocalSize,
        FWTBRPathGraphHandleHit& OutHit) const;

    // Index to insert BEFORE, chosen as the authored segment whose drawn line passes
    // nearest the click — so clicking a stretch of the path adds a bend there rather
    // than lengthening the lane. Appending instead would move where the lane LANDS,
    // silently changing the shot's reach every time someone adds a corner.
    int32 FindInsertIndexForPoint(int32 LaneIndex, const FVector2D& LocalPoint,
        const FSlateRect& PaneRect, bool bElevationPane) const;

    // Which pane contains a point, if any.
    bool FindPaneForPoint(const FVector2D& LocalPoint, const FVector2D& LocalSize,
        FSlateRect& OutPaneRect, bool& bOutElevationPane) const;

    void ApplyEditingVisibility();

    UPROPERTY(Transient)
    FWTBRPathPreset DisplayPreset;

    FText HeaderText;

    int32 SelectedLaneIndex = 0;

    // Starts on the caster, which is the one waypoint that always exists.
    int32 SelectedWaypointIndex = 0;

    // In-flight drag. Waypoint index is INDEX_NONE when nothing is being dragged.
    FWTBRPathGraphHandleHit ActiveDrag;

    // Finds the marker of a given type pinned to a waypoint, or nullptr.
    FWTBRLaneEvent* FindWaypointEvent(int32 LaneIndex, int32 WaypointIndex, EWTBRLaneEventType Type);
    const FWTBRLaneEvent* FindWaypointEvent(int32 LaneIndex, int32 WaypointIndex, EWTBRLaneEventType Type) const;

    // Keeps every marker pinned to the waypoint it was placed on after the list
    // shifts. Without it, inserting a bend early in a lane silently moves every
    // marker after it onto the wrong corner.
    void RemapWaypointEventsAfterInsert(int32 LaneIndex, int32 InsertedAt);
    void RemapWaypointEventsAfterDelete(int32 LaneIndex, int32 DeletedAt);
};
