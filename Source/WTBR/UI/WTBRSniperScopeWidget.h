// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBRSniperScopeWidget.generated.h"

// Native, client-only BR/FPS-style scope lens shown while the local player is
// aiming a Sniper Trigger (Egret/Ibis/Lightning). Everything outside a
// circular lens is masked black, with a crosshair + stadia posts inside —
// paired with AWTBRCharacter::SetSniperScopeView, which snaps the camera to
// the character's eyes while aiming. The whole overlay fades in with the
// existing FOV zoom lerp (AWTBRCharacter::GetSniperZoomAlphaForHUD). Reads
// local state only; never touches gameplay/server state.
UCLASS()
class WTBR_API UWTBRSniperScopeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Lens radius as a fraction of the shorter screen axis.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Sniper Scope", meta=(ClampMin="0.10", ClampMax="0.50"))
    float ScopeRadiusFraction = 0.42f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Sniper Scope", meta=(ClampMin="0.5"))
    float CrosshairLineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Sniper Scope", meta=(ClampMin="1.0"))
    float StadiaLineThickness = 6.0f;

    // Thick stadia posts run inward from the lens edge, stopping at this
    // fraction of the radius from center.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Sniper Scope", meta=(ClampMin="0.0", ClampMax="1.0"))
    float StadiaLengthFraction = 0.45f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
