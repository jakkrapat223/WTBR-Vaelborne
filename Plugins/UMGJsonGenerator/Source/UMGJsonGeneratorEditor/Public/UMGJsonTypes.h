#pragma once

#include "CoreMinimal.h"

// ── Parsed position from JSON ─────────────────────────────────────────────────
struct FUMGJsonPosition
{
	float X = 0.f;
	float Y = 0.f;
};

// ── Parsed size from JSON ─────────────────────────────────────────────────────
struct FUMGJsonSize
{
	float Width  = 100.f;
	float Height = 30.f;
};

// ── Anchor for CanvasPanelSlot ────────────────────────────────────────────────
struct FUMGJsonAnchor
{
	float MinX = 0.f;
	float MinY = 0.f;
	float MaxX = 0.f;
	float MaxY = 0.f;
};

// ── Alignment for CanvasPanelSlot ─────────────────────────────────────────────
struct FUMGJsonAlignment
{
	float X = 0.f;
	float Y = 0.f;
};

// ── One widget entry from the "widgets" array ─────────────────────────────────
struct FUMGJsonWidgetData
{
	FString Type;
	FString Name;
	FString Parent;
	FString Text;
	FString ImagePath;
	FString Color;       // "#RRGGBB" or "#RRGGBBAA"
	FUMGJsonPosition Position;
	FUMGJsonSize     Size;
	bool bHasPosition = false;
	bool bHasSize     = false;
	int32  FontSize = 12;
	int32  ZOrder   = 0;

	// Anchor & alignment (CanvasPanel children)
	FUMGJsonAnchor    Anchor;
	FUMGJsonAlignment Alignment;
	bool bHasAnchor    = false;
	bool bHasAlignment = false;

	// Text alignment
	FString Justify;        // "left", "center", "right"
	FString VerticalAlign;  // "top", "center", "bottom"

	// Button styles
	FString NormalColor;
	FString HoveredColor;
	FString PressedColor;
	FString DisabledColor;
	FString TextColor;

	// Image brush
	FString TintColor;  // brush tint (separate from Color which is widget ColorAndOpacity)
	FString DrawAs;     // "image", "box", "border", "roundedBox", "none"

	// Expose as Blueprint variable (explicit override; unset = auto by name prefix)
	bool bIsVariable    = false;
	bool bHasIsVariable = false;
};

// ── Root widget declaration ("root" field) ────────────────────────────────────
struct FUMGJsonRootData
{
	FString Type;
	FString Name;
};

// ── Top-level layout document ─────────────────────────────────────────────────
struct FUMGJsonLayoutData
{
	FString WidgetName;
	FString ParentClassPath;  // optional native UUserWidget subclass, e.g. "/Script/WTBR.WTBRGeneratedHUDWidget"
	FUMGJsonSize Resolution;
	FUMGJsonRootData Root;
	TArray<FUMGJsonWidgetData> Widgets;
};

// ── Import result tracking ────────────────────────────────────────────────────
struct FUMGJsonImportStats
{
	int32 TotalWidgets  = 0;
	int32 SuccessCount  = 0;
	int32 WarningCount  = 0;
	int32 SkippedCount  = 0;
	TArray<FString> Warnings;

	void AddWarning(const FString& Msg)
	{
		Warnings.Add(Msg);
		++WarningCount;
	}
};
