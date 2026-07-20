// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBRMarkPingHUDWidget.generated.h"

// Native, client-only overlay for the Mark/Ping system (see AWTBRCharacter's
// GetActiveMarkPings/Server_RequestMarkPing). Reads GetOwningPlayer()->GetPawn()'s
// already-filtered, already-team-only FWTBRActiveMarkPing list each paint and
// projects each one's world location to screen — this widget makes no visibility
// decisions of its own, the server already guaranteed enemies never receive a
// mark at all (see IncomingMarkPing's COND_OwnerOnly).
UCLASS()
class WTBR_API UWTBRMarkPingHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Mark Ping", meta=(ClampMin="8.0"))
    float IconSize = 28.0f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
