// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WTBRTriggerWheelWidget.generated.h"

class AWTBRCharacter;

/**
 * Shared radial selection wheel: Trigger slots on Q/E, or Feryx forms while
 * holding the active Feryx attack input.
 *
 * Fully native like UWTBRRadarWidget: a wheel is drawn geometry, not layout, so
 * there is nothing for UMG to lay out — segments, the highlight and the labels are
 * all produced in NativePaint. That also keeps it inside the project rule against
 * AI-authored .uasset content.
 *
 * Selection is driven by accumulated mouse delta rather than an OS cursor: the game
 * runs with the cursor hidden and the mouse bound to look, so showing a pointer (and
 * switching input mode) for a hold-to-open wheel would fight the camera. The player
 * flicks in a direction; the segment under that direction highlights; releasing the
 * key commits it. This is how weapon wheels in shooters behave.
 *
 * Camera rotation IS frozen while this wheel is open (AWTBRCharacter::
 * SetLookInputFrozen, driven from OpenTriggerWheel/EndTriggerSetSelect) —
 * owner's call 2026-07-20, matching the Escudo preset wheel. Note this widget
 * reads raw mouse delta itself via GetInputMouseDelta, which is unaffected by
 * that freeze (the freeze only gates the Enhanced Input Look path feeding
 * AddControllerYaw/PitchInput), so selection still works normally.
 */
UCLASS()
class WTBR_API UWTBRTriggerWheelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Outer radius of the wheel in pixels, at 1080p. Scaled by viewport height.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Wheel", meta = (ClampMin = "40.0"))
    float WheelRadius = 190.0f;

    // Inner cut-out. The dead zone in the middle where no segment is selected.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Wheel", meta = (ClampMin = "10.0"))
    float WheelInnerRadius = 62.0f;

    // How far the player must push the mouse before a segment is picked. Below this
    // the wheel stays on the current slot, so a hold with no flick is a no-op rather
    // than a random switch.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Wheel", meta = (ClampMin = "1.0"))
    float SelectionDeadZone = 25.0f;

    // Mouse delta is multiplied by this before being accumulated. Higher = flick less.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WTBR | Wheel", meta = (ClampMin = "0.1"))
    float SelectionSensitivity = 6.0f;

    UFUNCTION(BlueprintCallable, Category = "WTBR | Wheel")
    void OpenWheel(bool bInIsMainSet);

    // Reuses the exact Trigger-wheel interaction and fixed four-slot layout for
    // Feryx form selection. Empty future-form slots remain dimmed.
    void OpenFeryxFormWheel(const TArray<FText>& InFormNames, int32 InCurrentFormIndex);

    UFUNCTION(BlueprintCallable, Category = "WTBR | Wheel")
    void CloseWheel();

    UFUNCTION(BlueprintPure, Category = "WTBR | Wheel")
    bool IsWheelOpen() const { return bIsOpen; }

    // Absolute slot index (0-3 Main, 4-7 Sub) the player is pointing at, or
    // INDEX_NONE while inside the dead zone. Callers treat INDEX_NONE as "keep the
    // slot you already had".
    UFUNCTION(BlueprintPure, Category = "WTBR | Wheel")
    int32 GetHighlightedSlotIndex() const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Wheel")
    int32 GetHighlightedFeryxFormIndex() const;

    UFUNCTION(BlueprintPure, Category = "WTBR | Wheel")
    bool IsFeryxFormWheelOpen() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    enum class EWheelContent : uint8
    {
        TriggerSlots,
        FeryxForms
    };

    AWTBRCharacter* GetOwningWTBRCharacter() const;

    // Index into the open set (0..SlotsPerSet-1), or INDEX_NONE inside the dead zone.
    int32 GetHighlightedSetIndex() const;

    // First absolute slot index of the set currently open.
    int32 GetSetBaseSlotIndex() const;

    bool bIsOpen = false;
    bool bIsMainSet = true;
    EWheelContent WheelContent = EWheelContent::TriggerSlots;
    TArray<FText> FeryxFormNames;
    int32 CurrentFeryxFormIndex = INDEX_NONE;

    // Accumulated mouse delta since the wheel opened, in wheel-space pixels.
    FVector2D SelectionVector = FVector2D::ZeroVector;

    static constexpr int32 SlotsPerSet = 4;
};
