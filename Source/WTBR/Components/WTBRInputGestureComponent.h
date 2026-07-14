// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "WTBRInputGestureComponent.generated.h"

// Forward-declare to avoid circular include (WTBRTriggerSetComponent includes WTBRTriggerBase)
class UWTBRTriggerSetComponent;
class UWTBRTriggerBase;
class UInputAction;
class UEnhancedInputComponent;

UENUM(BlueprintType)
enum class EWTBRInputGesture : uint8
{
    None        UMETA(DisplayName="None"),
    MainTap     UMETA(DisplayName="Main Tap (LMB)"),
    MainHold    UMETA(DisplayName="Main Hold (LMB)"),
    SubTap      UMETA(DisplayName="Sub Tap (RMB)"),
    SubHold     UMETA(DisplayName="Sub Hold (RMB)"),
    DualTrigger UMETA(DisplayName="Dual Trigger (Both)"), // Pure Type-Match fires this
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGestureDetected, EWTBRInputGesture, Gesture);

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRInputGestureComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRInputGestureComponent();

    // Seconds held before classifying as "Hold" instead of "Tap"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    float HoldThreshold = 0.20f;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnGestureDetected OnGestureDetected;

    // ── Input Action References ───────────────────────────────────────────────
    // Set these in BP_WTBRCharacter → InputGestureComponent defaults.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Input | Actions")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Input | Actions")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Input | Actions")
    TObjectPtr<UInputAction> IA_Jump;

    // Called from WTBRCharacter::SetupPlayerInputComponent to wire all input
    void BindInputActions(UEnhancedInputComponent* EIC);

    FVector2D GetLastMoveAxis2D() const { return LastMoveAxis2D; }

    // Called from WTBRCharacter::SetupPlayerInputComponent
    UFUNCTION(BlueprintCallable, Category="Input")
    void NotifyMainPressed();

    UFUNCTION(BlueprintCallable, Category="Input")
    void NotifyMainReleased();

    UFUNCTION(BlueprintCallable, Category="Input")
    void NotifySubPressed();

    UFUNCTION(BlueprintCallable, Category="Input")
    void NotifySubReleased();

protected:
    virtual void BeginPlay() override;

private:
    bool  bMainDown  = false;
    bool  bSubDown   = false;
    float MainHoldStartTime = 0.f;  // GetTimeSeconds() snapshot on press
    float SubHoldStartTime  = 0.f;
    bool  bMainFired = false;
    bool  bSubFired  = false;

    UPROPERTY()
    TObjectPtr<UWTBRTriggerSetComponent> TriggerSetComp;

    // Checks Pure Type-Match dual-trigger condition and fires DualTrigger gesture
    void CheckDualGesture();

    // ── Mantorn (Feryx+Feryx) enter/exit gesture ──────────────────────────────
    // A simultaneous LMB+RMB hold (past HoldThreshold) toggles Mantorn form when
    // the active loadout is a Mantorn-capable Feryx pair. Local (owning-client)
    // detection → server RPC; the server is authoritative for the actual transform.
    FTimerHandle MantornHoldTimer;
    void StartMantornHoldWatch();
    void CancelMantornHoldWatch();
    void OnMantornHoldElapsed();

    // ── Movement ──────────────────────────────────────────────────────────────
    void OnMove_Triggered(const FInputActionValue& Value);
    void OnMove_Completed(const FInputActionValue& Value);

    FVector2D LastMoveAxis2D = FVector2D::ZeroVector;

    // ── Look (Mouse XY) ───────────────────────────────────────────────────────
    void OnLook_Triggered(const FInputActionValue& Value);

    // ── Jump ──────────────────────────────────────────────────────────────────
    void OnJump_Started(const FInputActionValue& Value);
    void OnJump_Completed(const FInputActionValue& Value);
};
