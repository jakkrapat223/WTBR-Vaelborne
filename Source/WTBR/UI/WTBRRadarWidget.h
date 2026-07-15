// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBRRadarWidget.generated.h"

class AWTBRCharacter;

// Native always-on radar. It renders nearby trion bodies; Bagworm/Vexorn users
// are omitted using their replicated cloak state.
UCLASS()
class WTBR_API UWTBRRadarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Radar", meta=(ClampMin="100.0"))
    float RadarRange = 6000.0f;

    // Contacts are refreshed at a fixed cadence instead of every Slate paint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Radar", meta=(ClampMin="0.02"))
    float ContactScanInterval = 0.1f;

    // Movement history displayed behind a contact, in seconds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Radar", meta=(ClampMin="0.1", ClampMax="10.0"))
    float TrailDuration = 3.0f;

    // Vertical distance before a HIGH/LOW label appears next to a contact.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Radar", meta=(ClampMin="0.0"))
    float VerticalLabelThreshold = 250.0f;

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    struct FRadarTrailSample
    {
        FVector Location = FVector::ZeroVector;
        float TimeSeconds = 0.0f;
    };

    struct FRadarContact
    {
        TWeakObjectPtr<AWTBRCharacter> Character;
        FVector Location = FVector::ZeroVector;
        bool bFriendly = false;
        TArray<FRadarTrailSample> Trail;
    };

    float ScanAccumulator = 0.0f;
    TMap<TWeakObjectPtr<AWTBRCharacter>, FRadarContact> Contacts;

    void RefreshContacts();
};
