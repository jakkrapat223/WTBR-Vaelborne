#include "UMGWidgetFactory.h"

// Widget tree
#include "Blueprint/WidgetTree.h"

// UMG widgets — original 9
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"

// UMG widgets — bonus 3
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"

// Slate font / styling
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

// Texture loading for Image
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMGWidgetFactory, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Public dispatch
// ─────────────────────────────────────────────────────────────────────────────

UWidget* FUMGWidgetFactory::CreateWidget(UWidgetTree* WT, const FUMGJsonWidgetData& Data)
{
	if (Data.Type == TEXT("CanvasPanel"))     return CreateCanvasPanel  (WT, Data);
	if (Data.Type == TEXT("TextBlock"))       return CreateTextBlock    (WT, Data);
	if (Data.Type == TEXT("Button"))          return CreateButton       (WT, Data);
	if (Data.Type == TEXT("Image"))           return CreateImage        (WT, Data);
	if (Data.Type == TEXT("Border"))          return CreateBorder       (WT, Data);
	if (Data.Type == TEXT("VerticalBox"))     return CreateVerticalBox  (WT, Data);
	if (Data.Type == TEXT("HorizontalBox"))   return CreateHorizontalBox(WT, Data);
	if (Data.Type == TEXT("ProgressBar"))     return CreateProgressBar  (WT, Data);
	if (Data.Type == TEXT("ScrollBox"))       return CreateScrollBox    (WT, Data);
	if (Data.Type == TEXT("Overlay"))         return CreateOverlay      (WT, Data);
	if (Data.Type == TEXT("SizeBox"))         return CreateSizeBox      (WT, Data);
	if (Data.Type == TEXT("Spacer"))          return CreateSpacer       (WT, Data);

	UE_LOG(LogUMGWidgetFactory, Warning, TEXT("Unsupported widget type: '%s'"), *Data.Type);
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parent attachment
// ─────────────────────────────────────────────────────────────────────────────

void FUMGWidgetFactory::AttachToParent(UPanelWidget* Parent, UWidget* Child,
                                        const FUMGJsonWidgetData& Data)
{
	if (!Parent || !Child) { return; }

	// ── CanvasPanel: absolute position / size / zOrder / anchor / alignment ──
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child);
		if (Slot)
		{
			Slot->SetPosition(FVector2D(Data.Position.X, Data.Position.Y));
			Slot->SetSize(FVector2D(Data.Size.Width, Data.Size.Height));
			Slot->SetZOrder(Data.ZOrder);
			Slot->SetAutoSize(false);

			if (Data.bHasAnchor)
			{
				Slot->SetAnchors(FAnchors(
					Data.Anchor.MinX, Data.Anchor.MinY,
					Data.Anchor.MaxX, Data.Anchor.MaxY));
			}

			if (Data.bHasAlignment)
			{
				Slot->SetAlignment(FVector2D(Data.Alignment.X, Data.Alignment.Y));
			}
		}
		return;
	}

	// ── VerticalBox ────────────────────────────────────────────────────────────
	if (UVerticalBox* VBox = Cast<UVerticalBox>(Parent))
	{
		UVerticalBoxSlot* Slot = VBox->AddChildToVerticalBox(Child);
		if (Slot)
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Top);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

			ApplySlotVerticalAlign(Slot, Data);
		}
		return;
	}

	// ── HorizontalBox ─────────────────────────────────────────────────────────
	if (UHorizontalBox* HBox = Cast<UHorizontalBox>(Parent))
	{
		UHorizontalBoxSlot* Slot = HBox->AddChildToHorizontalBox(Child);
		if (Slot)
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Left);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

			ApplySlotVerticalAlign(Slot, Data);
		}
		return;
	}

	// ── Overlay ────────────────────────────────────────────────────────────────
	if (UOverlay* OL = Cast<UOverlay>(Parent))
	{
		UPanelSlot* Slot = OL->AddChild(Child);
		if (Slot)
		{
			UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot);
			if (OverlaySlot)
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		return;
	}

	// ── ScrollBox ──────────────────────────────────────────────────────────────
	if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
	{
		Scroll->AddChild(Child);
		return;
	}

	// ── Single-child containers (Border, Button, SizeBox) ──────────────────────
	if (Cast<UBorder>(Parent) || Cast<UButton>(Parent) || Cast<USizeBox>(Parent))
	{
		Parent->AddChild(Child);
		return;
	}

	// ── Generic fallback ───────────────────────────────────────────────────────
	Parent->AddChild(Child);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot alignment helper
// ─────────────────────────────────────────────────────────────────────────────

void FUMGWidgetFactory::ApplySlotVerticalAlign(UPanelSlot* Slot,
                                                const FUMGJsonWidgetData& Data)
{
	if (Data.VerticalAlign.IsEmpty()) { return; }

	EVerticalAlignment VAlign = VAlign_Top;
	if (Data.VerticalAlign == TEXT("center"))      VAlign = VAlign_Center;
	else if (Data.VerticalAlign == TEXT("bottom")) VAlign = VAlign_Bottom;

	if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Slot))
	{
		VSlot->SetVerticalAlignment(VAlign);
	}
	else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Slot))
	{
		HSlot->SetVerticalAlignment(VAlign);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Hex color helper
// ─────────────────────────────────────────────────────────────────────────────

FLinearColor FUMGWidgetFactory::HexToLinear(const FString& Hex)
{
	if (Hex.IsEmpty()) { return FLinearColor::White; }
	FString Clean = Hex.StartsWith(TEXT("#")) ? Hex.Mid(1) : Hex;
	return FLinearColor(FColor::FromHex(Clean));
}

// ─────────────────────────────────────────────────────────────────────────────
// Text justification helper
// ─────────────────────────────────────────────────────────────────────────────

ETextJustify::Type FUMGWidgetFactory::ParseJustify(const FString& Value)
{
	if (Value == TEXT("center")) return ETextJustify::Center;
	if (Value == TEXT("right"))  return ETextJustify::Right;
	return ETextJustify::Left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-type creators
// ─────────────────────────────────────────────────────────────────────────────

UWidget* FUMGWidgetFactory::CreateCanvasPanel(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	return WT->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*D.Name));
}

UWidget* FUMGWidgetFactory::CreateTextBlock(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UTextBlock* Widget = WT->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*D.Name));

	if (!D.Text.IsEmpty())
	{
		Widget->SetText(FText::FromString(D.Text));
	}

	FSlateFontInfo FontInfo = Widget->GetFont();
	FontInfo.Size = D.FontSize > 0 ? D.FontSize : 12;
	Widget->SetFont(FontInfo);

	if (!D.Color.IsEmpty())
	{
		Widget->SetColorAndOpacity(FSlateColor(HexToLinear(D.Color)));
	}

	if (!D.Justify.IsEmpty())
	{
		Widget->SetJustification(ParseJustify(D.Justify));
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateButton(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UButton* Widget = WT->ConstructWidget<UButton>(UButton::StaticClass(), FName(*D.Name));

	// Base background color (backwards compat)
	if (!D.Color.IsEmpty())
	{
		Widget->BackgroundColor = HexToLinear(D.Color);
	}

	// Button style colors
	bool bHasStyleColors = !D.NormalColor.IsEmpty() || !D.HoveredColor.IsEmpty() ||
	                       !D.PressedColor.IsEmpty() || !D.DisabledColor.IsEmpty();
	if (bHasStyleColors)
	{
		FButtonStyle Style = Widget->WidgetStyle;

		if (!D.NormalColor.IsEmpty())
		{
			Style.Normal.TintColor = FSlateColor(HexToLinear(D.NormalColor));
		}
		if (!D.HoveredColor.IsEmpty())
		{
			Style.Hovered.TintColor = FSlateColor(HexToLinear(D.HoveredColor));
		}
		if (!D.PressedColor.IsEmpty())
		{
			Style.Pressed.TintColor = FSlateColor(HexToLinear(D.PressedColor));
		}
		if (!D.DisabledColor.IsEmpty())
		{
			Style.Disabled.TintColor = FSlateColor(HexToLinear(D.DisabledColor));
		}

		Widget->SetStyle(Style);
	}

	// Auto-create TextBlock child label
	if (!D.Text.IsEmpty())
	{
		FUMGJsonWidgetData LabelData;
		LabelData.Type     = TEXT("TextBlock");
		LabelData.Name     = D.Name + TEXT("_Label");
		LabelData.Text     = D.Text;
		LabelData.FontSize = D.FontSize > 0 ? D.FontSize : 14;
		LabelData.Color    = D.TextColor.IsEmpty() ? TEXT("#FFFFFF") : D.TextColor;
		LabelData.Justify  = D.Justify;

		UWidget* Label = CreateTextBlock(WT, LabelData);
		if (Label)
		{
			Widget->AddChild(Label);
		}
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateImage(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UImage* Widget = WT->ConstructWidget<UImage>(UImage::StaticClass(), FName(*D.Name));

	bool bHasTexture = false;
	if (!D.ImagePath.IsEmpty())
	{
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *D.ImagePath);
		if (Tex)
		{
			Widget->SetBrushFromTexture(Tex, false);
			bHasTexture = true;
		}
		else
		{
			UE_LOG(LogUMGWidgetFactory, Warning,
				TEXT("Image '%s': texture not found at '%s' — using placeholder."),
				*D.Name, *D.ImagePath);
		}
	}

	// DrawAs
	if (!D.DrawAs.IsEmpty())
	{
		FSlateBrush Brush = Widget->Brush;
		if (D.DrawAs == TEXT("box"))            Brush.DrawAs = ESlateBrushDrawType::Box;
		else if (D.DrawAs == TEXT("border"))    Brush.DrawAs = ESlateBrushDrawType::Border;
		else if (D.DrawAs == TEXT("roundedBox"))Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		else if (D.DrawAs == TEXT("none"))      Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		else                                    Brush.DrawAs = ESlateBrushDrawType::Image;
		Widget->SetBrush(Brush);
	}

	// Brush tint color
	if (!D.TintColor.IsEmpty())
	{
		Widget->SetBrushTintColor(FSlateColor(HexToLinear(D.TintColor)));
	}

	// Widget color and opacity
	if (!D.Color.IsEmpty())
	{
		Widget->SetColorAndOpacity(HexToLinear(D.Color));
	}

	// Placeholder: if no texture, ensure there's at least a visible color
	if (!bHasTexture && D.Color.IsEmpty() && D.TintColor.IsEmpty())
	{
		Widget->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateBorder(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UBorder* Widget = WT->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*D.Name));

	if (!D.Color.IsEmpty())
	{
		Widget->SetBrushColor(HexToLinear(D.Color));
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateVerticalBox(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	return WT->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*D.Name));
}

UWidget* FUMGWidgetFactory::CreateHorizontalBox(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	return WT->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*D.Name));
}

UWidget* FUMGWidgetFactory::CreateProgressBar(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UProgressBar* Widget = WT->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*D.Name));

	Widget->SetPercent(0.5f);

	if (!D.Color.IsEmpty())
	{
		Widget->SetFillColorAndOpacity(HexToLinear(D.Color));
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateScrollBox(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	UScrollBox* Widget = WT->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName(*D.Name));
	Widget->SetOrientation(Orient_Vertical);
	return Widget;
}

UWidget* FUMGWidgetFactory::CreateOverlay(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	return WT->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*D.Name));
}

UWidget* FUMGWidgetFactory::CreateSizeBox(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	USizeBox* Widget = WT->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*D.Name));

	if (D.Size.Width > 0.f)
	{
		Widget->SetWidthOverride(D.Size.Width);
	}
	if (D.Size.Height > 0.f)
	{
		Widget->SetHeightOverride(D.Size.Height);
	}

	return Widget;
}

UWidget* FUMGWidgetFactory::CreateSpacer(UWidgetTree* WT, const FUMGJsonWidgetData& D)
{
	USpacer* Widget = WT->ConstructWidget<USpacer>(USpacer::StaticClass(), FName(*D.Name));

	if (D.Size.Width > 0.f || D.Size.Height > 0.f)
	{
		Widget->SetSize(FVector2D(D.Size.Width, D.Size.Height));
	}

	return Widget;
}
