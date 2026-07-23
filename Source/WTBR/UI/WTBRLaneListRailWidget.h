// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBRLaneListRailWidget.generated.h"

class UWTBRPathGraphViewWidget;
class SVerticalBox;

/**
 * The Preset Editor's side panel: the lane list, and the numbers a lane carries that
 * are not its shape.
 *
 * The graph answers "where does this lane go". This answers "how many cubes, how long
 * does it wait, does it hunt" — which is where a large part of a preset's behaviour
 * actually lives. LaunchDelay in particular is the entire wave mechanic: a first wave
 * that draws a shield and a second that arrives after it drops cannot be authored at
 * all without this panel.
 *
 * ⚠ Unlike every other widget in this project, this one is built from real Slate leaf
 * widgets (SSpinBox, SButton, STextBlock) rather than hand-drawn in NativePaint.
 * Owner-approved 2026-07-23. The project rule it bends is specifically "no
 * AI-authored .uasset content" (see UWTBRTriggerWheelWidget) and this still authors
 * none — everything is constructed in C++. The wheels hand-draw because a radial
 * wheel has nothing to lay out; numeric entry does, and re-implementing text editing,
 * caret handling and drag-scrub on top of raw draw calls would be a large amount of
 * risk for no gain.
 *
 * Holds no preset of its own. UWTBRPathGraphViewWidget owns the draft and this reads
 * and mutates it through that class's public rules, so the two panels cannot drift
 * out of sync — Step 7's root widget will take ownership of the draft from the graph,
 * and this keeps working unchanged.
 */
UCLASS()
class WTBR_API UWTBRLaneListRailWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Binds to the graph whose draft this panel edits, and syncs immediately. */
    void BindToGraph(UWTBRPathGraphViewWidget* InGraph);

    /** Rebuilds rows and re-reads every field. Safe to call at any time. */
    void RefreshFromGraph();

    // Homing is not authored here at all — how far a bullet hunts belongs to the
    // weapon, and where along a path it hunts belongs to a marker. See
    // RefreshFromGraph for the full reasoning.

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;

private:
    TSharedRef<SWidget> BuildLaneRows();
    TSharedRef<SWidget> BuildSelectedLaneProperties();

    // Reads the lane currently being edited, or nullptr when the graph is gone or
    // holds no lanes.
    const struct FWTBRPathLane* GetSelectedLane() const;

    // Applies one scalar change by copying the selected lane, mutating the copy, and
    // submitting it through the graph's clamping setter — so the rail never writes a
    // value the server would later alter.
    void CommitScalar(TFunctionRef<void(struct FWTBRPathLane&)> Mutate);

    TWeakObjectPtr<UWTBRPathGraphViewWidget> Graph;

    TSharedPtr<SVerticalBox> RootBox;

    // Rebuilt in place rather than recreated, so the panel does not flicker or lose
    // an in-progress spin-box drag every time the graph broadcasts an edit.
    TSharedPtr<SVerticalBox> LaneRowsBox;
    TSharedPtr<SVerticalBox> PropertiesBox;

    FDelegateHandle EditedHandle;
    FDelegateHandle SelectedHandle;
    FDelegateHandle WaypointHandle;
};
