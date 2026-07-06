#pragma once

#include "CoreMinimal.h"
#include "UMGJsonTypes.h"
#include "Framework/Text/TextLayout.h"

class UWidget;
class UWidgetTree;
class UPanelWidget;
class UPanelSlot;

// ── Creates individual UMG widgets from parsed JSON data ──────────────────────
class UMGJSONGENERATOREDITOR_API FUMGWidgetFactory
{
public:
	static UWidget* CreateWidget(UWidgetTree* WidgetTree, const FUMGJsonWidgetData& Data);

	static void AttachToParent(UPanelWidget* ParentWidget, UWidget* ChildWidget,
	                            const FUMGJsonWidgetData& Data);

	static FLinearColor HexToLinear(const FString& Hex);

private:
	// 9 original MVP types
	static UWidget* CreateCanvasPanel  (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateTextBlock    (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateButton       (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateImage        (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateBorder       (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateVerticalBox  (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateHorizontalBox(UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateProgressBar  (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateScrollBox    (UWidgetTree* WT, const FUMGJsonWidgetData& D);

	// 3 bonus types
	static UWidget* CreateOverlay      (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateSizeBox      (UWidgetTree* WT, const FUMGJsonWidgetData& D);
	static UWidget* CreateSpacer       (UWidgetTree* WT, const FUMGJsonWidgetData& D);

	// Helpers
	static void ApplySlotVerticalAlign(UPanelSlot* Slot, const FUMGJsonWidgetData& Data);
	static ETextJustify::Type ParseJustify(const FString& Value);
};
