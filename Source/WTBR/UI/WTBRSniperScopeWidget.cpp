// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRSniperScopeWidget.h"
#include "WTBRCharacter.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UWTBRSniperScopeWidget::RebuildWidget()
{
    return SNew(SBox);
}

int32 UWTBRSniperScopeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    const APlayerController* PC = GetOwningPlayer();
    const AWTBRCharacter* LocalCharacter = PC ? Cast<AWTBRCharacter>(PC->GetPawn()) : nullptr;
    if (!IsValid(LocalCharacter))
    {
        return LayerId;
    }

    // Alpha tracks FollowCamera's live zoom lerp (0 = resting FOV, 1 = fully
    // zoomed) so the lens fades in/out with the zoom instead of popping.
    const float Alpha = LocalCharacter->GetSniperZoomAlphaForHUD();
    if (Alpha <= KINDA_SMALL_NUMBER)
    {
        return LayerId;
    }

    const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
    if (ScreenSize.X <= 1.0f || ScreenSize.Y <= 1.0f) return LayerId;
    const FVector2D Center = ScreenSize * 0.5f;
    const float LensRadius = FMath::Min(ScreenSize.X, ScreenSize.Y) * ScopeRadiusFraction;

    // ── Black mask covering everything OUTSIDE the circular lens ──────────────
    // Slate has no circle/stencil primitive, so the mask is a triangulated
    // ring: 64 quads from the lens edge out past the screen corners.
    if (FSlateApplication::IsInitialized())
    {
        const FSlateBrush* WhiteBrush = FAppStyle::Get().GetBrush("WhiteBrush");
        const FSlateResourceHandle Handle =
            FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
        const FSlateRenderTransform& MaskRenderTransform =
            AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

        const FColor MaskColor = FLinearColor(0.0f, 0.0f, 0.0f, Alpha).ToFColor(true);
        // Far enough that segment chords stay off-screen at every aspect ratio.
        const float OuterRadius = ScreenSize.Size();
        constexpr int32 NumSegments = 64;

        TArray<FSlateVertex> Verts;
        TArray<SlateIndex> Indexes;
        Verts.Reserve((NumSegments + 1) * 2);
        Indexes.Reserve(NumSegments * 6);

        for (int32 Index = 0; Index <= NumSegments; ++Index)
        {
            const float Angle = (2.0f * PI * Index) / NumSegments;
            const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D Inner = Center + Dir * LensRadius;
            const FVector2D Outer = Center + Dir * OuterRadius;
            Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                MaskRenderTransform, FVector2f(Inner.X, Inner.Y), FVector2f(0.0f, 0.0f), MaskColor));
            Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                MaskRenderTransform, FVector2f(Outer.X, Outer.Y), FVector2f(0.0f, 0.0f), MaskColor));
        }
        for (int32 Index = 0; Index < NumSegments; ++Index)
        {
            const SlateIndex Base = static_cast<SlateIndex>(Index * 2);
            Indexes.Add(Base);     Indexes.Add(Base + 1); Indexes.Add(Base + 3);
            Indexes.Add(Base);     Indexes.Add(Base + 3); Indexes.Add(Base + 2);
        }
        FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indexes,
            nullptr, 0, 0);
    }

    // ── Lens rim ───────────────────────────────────────────────────────────────
    const FLinearColor RimColour(0.0f, 0.0f, 0.0f, Alpha);
    {
        TArray<FVector2D> Ring;
        Ring.Reserve(65);
        for (int32 Index = 0; Index <= 64; ++Index)
        {
            const float Angle = (2.0f * PI * Index) / 64.0f;
            Ring.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * LensRadius);
        }
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
            Ring, ESlateDrawEffect::None, RimColour, true, 4.0f);
    }

    // ── Crosshair: thin full cross + thick stadia posts (left/right/bottom) ───
    const FLinearColor LineColour(0.0f, 0.0f, 0.0f, Alpha);

    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center - FVector2D(LensRadius, 0.0f), Center + FVector2D(LensRadius, 0.0f) },
        ESlateDrawEffect::None, LineColour, true, CrosshairLineThickness);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center - FVector2D(0.0f, LensRadius), Center + FVector2D(0.0f, LensRadius) },
        ESlateDrawEffect::None, LineColour, true, CrosshairLineThickness);

    const float StadiaInnerStop = LensRadius * StadiaLengthFraction;
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center - FVector2D(LensRadius, 0.0f), Center - FVector2D(StadiaInnerStop, 0.0f) },
        ESlateDrawEffect::None, LineColour, true, StadiaLineThickness);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center + FVector2D(StadiaInnerStop, 0.0f), Center + FVector2D(LensRadius, 0.0f) },
        ESlateDrawEffect::None, LineColour, true, StadiaLineThickness);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center + FVector2D(0.0f, StadiaInnerStop), Center + FVector2D(0.0f, LensRadius) },
        ESlateDrawEffect::None, LineColour, true, StadiaLineThickness);

    return LayerId;
}
