// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBREscudoPresetWheelWidget.generated.h"

// One radial segment: a preset's display name plus whether the player can
// currently afford it (unaffordable segments are drawn dimmed and can never
// be the highlighted selection — see GetHighlightedPresetIndex).
USTRUCT(BlueprintType)
struct FWTBREscudoPresetWheelOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Escudo Wheel")
    FText Name;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Escudo Wheel")
    bool bAffordable = true;
};

// Radial Escudo preset-selection wheel, opened by holding Fire past the hold
// threshold while Escudo is the active Trigger on that slot. Same mouse-
// delta/no-OS-cursor selection mechanic AND fixed-slot-count layout as
// UWTBRTriggerWheelWidget (see that class's doc comment for why raw delta
// instead of an OS cursor) — WheelSlotCount segments are always drawn, same
// as the Trigger Wheel's fixed 4; a slot beyond however many presets exist
// draws as empty/dimmed rather than shrinking the wheel to fit. Unlike the
// Trigger Wheel, this dialog is paired with an actual camera-rotation freeze
// owned by AWTBRCharacter — see wtbr-escudo-hold-preset-design-lock project
// memory for why the two dialogs need different camera behavior.
UCLASS()
class WTBR_API UWTBREscudoPresetWheelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Fixed number of wheel slots, matching the Trigger Wheel's fixed-4
    // layout. Presets beyond this count are simply not selectable from the
    // wheel; slots with no corresponding preset draw dimmed/empty.
    static constexpr int32 WheelSlotCount = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Escudo Wheel", meta = (ClampMin = "40.0"))
    float WheelRadius = 190.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Escudo Wheel", meta = (ClampMin = "10.0"))
    float WheelInnerRadius = 62.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Escudo Wheel", meta = (ClampMin = "1.0"))
    float SelectionDeadZone = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Escudo Wheel", meta = (ClampMin = "0.1"))
    float SelectionSensitivity = 6.0f;

    void OpenWheel(const TArray<FWTBREscudoPresetWheelOption>& InOptions);
    void CloseWheel();

    UFUNCTION(BlueprintPure, Category = "WTBR | Escudo Wheel")
    bool IsWheelOpen() const { return bIsOpen; }

    // Index into the options passed to OpenWheel, or INDEX_NONE while inside
    // the dead zone, pointing at nothing, or pointing at an unaffordable
    // option (never returned — the player cannot select what they can't pay
    // for; releasing there is treated as "select nothing").
    UFUNCTION(BlueprintPure, Category = "WTBR | Escudo Wheel")
    int32 GetHighlightedPresetIndex() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    // Raw pointed-at segment, ignoring affordability — used internally by both
    // the public getter (which applies the affordability gate) and NativePaint
    // (which needs to know the raw highlight to draw it dimmed instead of hot).
    int32 GetRawHighlightedIndex() const;

    TArray<FWTBREscudoPresetWheelOption> Options;
    bool bIsOpen = false;
    FVector2D SelectionVector = FVector2D::ZeroVector;
};
