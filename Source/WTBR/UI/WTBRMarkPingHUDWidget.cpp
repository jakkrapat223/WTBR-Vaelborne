// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRMarkPingHUDWidget.h"
#include "WTBRCharacter.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UWTBRMarkPingHUDWidget::RebuildWidget()
{
    return SNew(SBox);
}

int32 UWTBRMarkPingHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    APlayerController* PC = GetOwningPlayer();
    const AWTBRCharacter* LocalCharacter = PC ? Cast<AWTBRCharacter>(PC->GetPawn()) : nullptr;
    if (!IsValid(LocalCharacter) || !IsValid(PC))
    {
        return LayerId;
    }

    const UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;
    const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
    if (ScreenSize.X <= 0.0f || ScreenSize.Y <= 0.0f)
    {
        return LayerId;
    }

    const APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
    const FVector CameraLocation = CameraManager ? CameraManager->GetCameraLocation() : LocalCharacter->GetActorLocation();
    const FVector CameraForward = CameraManager
        ? CameraManager->GetCameraRotation().Vector()
        : LocalCharacter->GetActorForwardVector();

    const FSlateBrush* WhiteBrush = FAppStyle::Get().GetBrush("WhiteBrush");
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    const FVector LocalLocation = LocalCharacter->GetActorLocation();

    for (const FWTBRActiveMarkPing& Ping : LocalCharacter->GetActiveMarkPings())
    {
        if (Ping.ExpireTimeSeconds <= Now)
        {
            continue;
        }

        // An enemy ping follows its target for free (marked actor's live location),
        // matching the genre convention that a ping shows where the enemy actually
        // is, not just where they stood the instant they were spotted. A discarded/
        // eliminated MarkedActor falls back to the frozen spot it was last valid at.
        const FVector WorldLocation = Ping.MarkedActor.IsValid()
            ? Ping.MarkedActor->GetActorLocation() + FVector(0.0, 0.0, 90.0)
            : Ping.Location;

        const FVector ToPing = WorldLocation - CameraLocation;
        if (FVector::DotProduct(CameraForward, ToPing.GetSafeNormal()) <= 0.0)
        {
            // Behind the camera. ProjectWorldLocationToScreen can still report a
            // "success" screen position for this case (mirrored through the
            // origin), so it must be rejected before trusting the projection.
            continue;
        }

        // ProjectWorldLocationToScreen returns coordinates in raw VIEWPORT PIXELS.
        // AllottedGeometry/ToPaintGeometry work in Slate LOCAL units, which UMG's
        // DPI scale curve (Project Settings > User Interface) can make smaller
        // than the raw viewport — using the pixel value unscaled made every mark
        // that projected past the (smaller) local size get culled by the on-
        // screen check below, leaving only whatever happened to land inside that
        // shrunken box, which reads as "stuck near one corner." Dividing by the
        // viewport scale converts pixels -> local units, matching what
        // AllottedGeometry.GetLocalSize() actually measures.
        FVector2D ScreenPos;
        if (!PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPos, /*bPlayerViewportRelative=*/true))
        {
            continue;
        }
        const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
        ScreenPos /= ViewportScale;

        if (ScreenPos.X < 0.0f || ScreenPos.Y < 0.0f || ScreenPos.X > ScreenSize.X || ScreenPos.Y > ScreenSize.Y)
        {
            // v1: hide off-screen marks rather than clamping to an edge arrow.
            continue;
        }

        const FLinearColor Colour = Ping.bIsEnemy
            ? FLinearColor(1.0f, 0.18f, 0.18f, 1.0f)
            : FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
        const float HalfSize = IconSize * 0.5f;

        FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(ScreenPos - FVector2D(HalfSize, HalfSize), FVector2D(IconSize, IconSize)),
            WhiteBrush, ESlateDrawEffect::None, Colour);

        const float DistanceMeters = static_cast<float>(FVector::Dist(LocalLocation, WorldLocation)) / 100.0f;
        const FText DistanceLabel = FText::AsNumber(FMath::RoundToInt(DistanceMeters));
        FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(ScreenPos + FVector2D(-12.0f, HalfSize + 2.0f), FVector2D(48.0f, 14.0f)),
            DistanceLabel, LabelFont, ESlateDrawEffect::None, FLinearColor::White);
    }

    return LayerId;
}
