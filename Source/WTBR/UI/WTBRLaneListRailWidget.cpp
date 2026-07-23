// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRLaneListRailWidget.h"
#include "UI/WTBRPathGraphViewWidget.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"

// NOTE: SVerticalBox / SHorizontalBox live in Widgets/SBoxPanel.h, NOT in
// Widgets/Layout/SVerticalBox.h — those paths do not exist standalone in UE 5.1.1
// and including them is a build break this project has already hit once.

namespace
{
    const FLinearColor WTBRRailLabel(1.0f, 1.0f, 1.0f, 0.55f);
    const FLinearColor WTBRRailSelected(0.91f, 0.63f, 0.23f, 1.0f);

    TSharedRef<SWidget> WTBRRailLabelText(const FString& Text, const FLinearColor& Colour)
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Text))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .ColorAndOpacity(FSlateColor(Colour));
    }
}

void UWTBRLaneListRailWidget::BindToGraph(UWTBRPathGraphViewWidget* InGraph)
{
    if (UWTBRPathGraphViewWidget* Previous = Graph.Get())
    {
        Previous->OnPresetEdited.Remove(EditedHandle);
        Previous->OnLaneSelected.Remove(SelectedHandle);
        Previous->OnWaypointSelected.Remove(WaypointHandle);
    }

    Graph = InGraph;

    if (IsValid(InGraph))
    {
        // The graph can change the selection on its own (number keys, deleting a
        // lane), so the rail follows it rather than assuming it is the only source.
        EditedHandle = InGraph->OnPresetEdited.AddUObject(
            this, &UWTBRLaneListRailWidget::RefreshFromGraph);
        SelectedHandle = InGraph->OnLaneSelected.AddLambda(
            [this](int32) { RefreshFromGraph(); });
        // The panel shows a different set of fields per waypoint, so it has to follow
        // waypoint selection too — clicking a handle on the graph must change what
        // this panel offers.
        WaypointHandle = InGraph->OnWaypointSelected.AddLambda(
            [this](int32) { RefreshFromGraph(); });
    }

    RefreshFromGraph();
}

void UWTBRLaneListRailWidget::NativeDestruct()
{
    if (UWTBRPathGraphViewWidget* Bound = Graph.Get())
    {
        Bound->OnPresetEdited.Remove(EditedHandle);
        Bound->OnLaneSelected.Remove(SelectedHandle);
        Bound->OnWaypointSelected.Remove(WaypointHandle);
    }
    Super::NativeDestruct();
}

const FWTBRPathLane* UWTBRLaneListRailWidget::GetSelectedLane() const
{
    const UWTBRPathGraphViewWidget* View = Graph.Get();
    if (!View) return nullptr;

    const FWTBRPathPreset& Preset = View->GetPreset();
    const int32 Index = View->GetSelectedLaneIndex();
    return Preset.Lanes.IsValidIndex(Index) ? &Preset.Lanes[Index] : nullptr;
}

void UWTBRLaneListRailWidget::CommitScalar(TFunctionRef<void(FWTBRPathLane&)> Mutate)
{
    UWTBRPathGraphViewWidget* View = Graph.Get();
    const FWTBRPathLane* Current = GetSelectedLane();
    if (!View || !Current) return;

    // Copy-mutate-submit rather than writing into the lane directly: the graph's
    // setter is where clamping lives, and it only copies the SCALAR fields, so an
    // edit here can never disturb the waypoints the player dragged.
    FWTBRPathLane Copy = *Current;
    Mutate(Copy);
    View->TryUpdateLaneScalars(View->GetSelectedLaneIndex(), Copy);
}

TSharedRef<SWidget> UWTBRLaneListRailWidget::BuildLaneRows()
{
    LaneRowsBox = SNew(SVerticalBox);
    return LaneRowsBox.ToSharedRef();
}

TSharedRef<SWidget> UWTBRLaneListRailWidget::BuildSelectedLaneProperties()
{
    PropertiesBox = SNew(SVerticalBox);
    return PropertiesBox.ToSharedRef();
}

TSharedRef<SWidget> UWTBRLaneListRailWidget::RebuildWidget()
{
    RootBox = SNew(SVerticalBox);

    RootBox->AddSlot().AutoHeight().Padding(4.0f)
    [
        WTBRRailLabelText(TEXT("LANES   (press 1-9 to select)"), WTBRRailLabel)
    ];

    RootBox->AddSlot().AutoHeight()[ BuildLaneRows() ];

    RootBox->AddSlot().AutoHeight().Padding(4.0f, 8.0f, 4.0f, 2.0f)
    [
        WTBRRailLabelText(TEXT("SELECTED LANE"), WTBRRailLabel)
    ];

    RootBox->AddSlot().AutoHeight()[ BuildSelectedLaneProperties() ];

    RefreshFromGraph();
    return RootBox.ToSharedRef();
}

void UWTBRLaneListRailWidget::RefreshFromGraph()
{
    UWTBRPathGraphViewWidget* View = Graph.Get();
    if (!LaneRowsBox.IsValid() || !PropertiesBox.IsValid()) return;

    LaneRowsBox->ClearChildren();
    PropertiesBox->ClearChildren();

    if (!View)
    {
        LaneRowsBox->AddSlot().AutoHeight()[ WTBRRailLabelText(TEXT("(no preset)"), WTBRRailLabel) ];
        return;
    }

    const FWTBRPathPreset& Preset = View->GetPreset();
    const int32 Selected = View->GetSelectedLaneIndex();

    for (int32 LaneIndex = 0; LaneIndex < Preset.Lanes.Num(); ++LaneIndex)
    {
        const bool bIsSelected = (LaneIndex == Selected);
        const FWTBRPathLane& Lane = Preset.Lanes[LaneIndex];

        // The number shown is the key that selects it, so the shortcut is readable
        // off the panel instead of being something the player has to be told.
        const FString RowLabel = FString::Printf(
            TEXT("%d  -  %d pt, %d cube%s%s"),
            LaneIndex + 1,
            Lane.NormalizedWaypoints.Num(),
            Lane.CubeCount,
            Lane.LaunchDelay > 0.0f ? *FString::Printf(TEXT(", +%.2fs"), Lane.LaunchDelay) : TEXT(""),
            Lane.HomingRadius > 0.0f ? TEXT(", hunts") : TEXT(""));

        // Selection only. Adding and deleting lanes were deliberately removed on
        // 2026-07-23: a preset ships with the lanes it is meant to have, five is
        // already as many simultaneous paths as a shot reads as, and a rail that
        // could delete but not re-add would be a one-way trap.
        LaneRowsBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, LaneIndex]()
            {
                if (UWTBRPathGraphViewWidget* Bound = Graph.Get())
                {
                    Bound->SetSelectedLaneIndex(LaneIndex);
                }
                return FReply::Handled();
            })
            [
                WTBRRailLabelText(RowLabel, bIsSelected ? WTBRRailSelected : WTBRRailLabel)
            ]
        ];
    }

    // The budget is shared across every lane, so it belongs with the list rather
    // than with any one lane's fields.
    LaneRowsBox->AddSlot().AutoHeight().Padding(4.0f, 4.0f)
    [
        WTBRRailLabelText(
            FString::Printf(TEXT("Cubes used: %d / %d"),
                View->GetTotalCubeCount(), WTBR_MAX_CUSTOM_PRESET_CUBES),
            View->GetTotalCubeCount() >= WTBR_MAX_CUSTOM_PRESET_CUBES
                ? WTBRRailSelected : WTBRRailLabel)
    ];

    const FWTBRPathLane* Lane = GetSelectedLane();
    if (!Lane)
    {
        PropertiesBox->AddSlot().AutoHeight()[ WTBRRailLabelText(TEXT("(no lane)"), WTBRRailLabel) ];
        return;
    }

    // What a waypoint can carry depends on where it sits along the lane, so the panel
    // shows a different set of fields per waypoint rather than one lane-wide block.
    const int32 WaypointIndex = View->GetSelectedWaypointIndex();
    const int32 WaypointCount = Lane->NormalizedWaypoints.Num();
    const bool bIsStart = (WaypointIndex == 0);
    const bool bIsEnd = (WaypointIndex == WaypointCount - 1);

    PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
    [
        WTBRRailLabelText(
            FString::Printf(TEXT("point %d/%d  -  %s"), WaypointIndex + 1, WaypointCount,
                bIsStart ? TEXT("START") : (bIsEnd ? TEXT("END") : TEXT("MID"))),
            WTBRRailSelected)
    ];

    // One labelled spin box per scalar. Ranges mirror the server sanitizer exactly,
    // so a value typed here is never one the upload would quietly change.
    auto AddFloatRow =
        [this](const FString& Label, float Value, float Min, float Max,
               TFunction<void(FWTBRPathLane&, float)> Apply)
    {
        PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
            [
                WTBRRailLabelText(Label, WTBRRailLabel)
            ]
            + SHorizontalBox::Slot().FillWidth(0.45f)
            [
                SNew(SSpinBox<float>)
                .MinValue(Min).MaxValue(Max)
                .MinSliderValue(Min).MaxSliderValue(Max)
                .Value(Value)
                .OnValueChanged_Lambda([this, Apply](float NewValue)
                {
                    CommitScalar([&Apply, NewValue](FWTBRPathLane& L) { Apply(L, NewValue); });
                })
            ]
        ];
    };

    if (bIsStart)
    {
        // The start is the moment the shot leaves: how many cubes it divides into,
        // and how long the whole lane waits before any of them launch.
        PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
            [
                WTBRRailLabelText(TEXT("Cubes"), WTBRRailLabel)
            ]
            + SHorizontalBox::Slot().FillWidth(0.45f)
            [
                SNew(SSpinBox<int32>)
                .MinValue(1)
                // Ceiling is what the OTHER lanes leave unspent, so the slider itself
                // cannot be dragged past the preset's shared budget.
                .MaxValue(View->GetRemainingCubeBudgetForLane(Selected))
                .MinSliderValue(1)
                .MaxSliderValue(View->GetRemainingCubeBudgetForLane(Selected))
                .Value(Lane->CubeCount)
                .OnValueChanged_Lambda([this](int32 NewValue)
                {
                    CommitScalar([NewValue](FWTBRPathLane& L) { L.CubeCount = NewValue; });
                })
            ]
        ];

        // The wave mechanic: a lane that waits lets a first volley draw a shield and
        // a second arrive after it drops.
        AddFloatRow(TEXT("Launch delay (s)"), Lane->LaunchDelay, 0.0f, 5.0f,
            [](FWTBRPathLane& L, float V) { L.LaunchDelay = V; });
    }
    else if (bIsEnd)
    {
        // Nothing to schedule at a destination — the lane has arrived. The end
        // waypoint is still draggable, which is how its reach is set.
        PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
        [
            WTBRRailLabelText(TEXT("Drag this point to set where the lane reaches."), WTBRRailLabel)
        ];
    }
    else
    {
        // A middle waypoint is somewhere the cube PASSES THROUGH, so what it can
        // carry is what can happen on the way: pause here, or change whether it is
        // hunting from here on.
        PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
            [
                WTBRRailLabelText(TEXT("Wait here (s)"), WTBRRailLabel)
            ]
            + SHorizontalBox::Slot().FillWidth(0.45f)
            [
                SNew(SSpinBox<float>)
                .MinValue(0.0f).MaxValue(5.0f)
                .MinSliderValue(0.0f).MaxSliderValue(5.0f)
                .Value(View->GetWaypointHoverSeconds(Selected, WaypointIndex))
                .OnValueChanged_Lambda([this, Selected, WaypointIndex](float NewValue)
                {
                    if (UWTBRPathGraphViewWidget* Bound = Graph.Get())
                    {
                        Bound->TrySetWaypointHoverSeconds(Selected, WaypointIndex, NewValue);
                    }
                })
            ]
        ];
    }

    // Homing is a three-state marker rather than a number, and it is offered at every
    // waypoint EXCEPT the destination — switching hunting on at the moment the lane
    // arrives would change nothing.
    //
    // How FAR a bullet hunts is not authored anywhere here: that belongs to the
    // weapon. A preset only says where along its path hunting is on or off, which is
    // what makes a drawn shape mean something — a lane that hunts from the muzzle
    // abandons the shape at the first target it passes.
    if (!bIsEnd)
    {
        const bool bHasMarker = View->HasWaypointHomingMarker(Selected, WaypointIndex);
        const bool bEnabled = View->GetWaypointHomingEnabled(Selected, WaypointIndex);

        const FString HomingLabel =
            !bHasMarker ? TEXT("Homing: unchanged")
            : bEnabled  ? TEXT("Homing: turn ON here")
                        : TEXT("Homing: turn OFF here");

        PropertiesBox->AddSlot().AutoHeight().Padding(4.0f, 4.0f)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, Selected, WaypointIndex, bHasMarker, bEnabled]()
            {
                UWTBRPathGraphViewWidget* Bound = Graph.Get();
                if (!Bound) return FReply::Handled();

                // Cycles unchanged -> ON -> OFF -> unchanged. One button because the
                // three states are mutually exclusive and the label always says which
                // one is active.
                if (!bHasMarker)      Bound->TrySetWaypointHoming(Selected, WaypointIndex, true, true);
                else if (bEnabled)    Bound->TrySetWaypointHoming(Selected, WaypointIndex, true, false);
                else                  Bound->TrySetWaypointHoming(Selected, WaypointIndex, false, false);
                return FReply::Handled();
            })
            [
                WTBRRailLabelText(HomingLabel, bHasMarker ? WTBRRailSelected : WTBRRailLabel)
            ]
        ];
    }
}
