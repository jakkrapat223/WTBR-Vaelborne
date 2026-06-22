// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Components/WTBRHealthComponent.h"    // EWTBRLimbType, FWTBRLimbState
#include "Trigger/WTBRTriggerSetComponent.h"   // EWTBRDualWieldState, ETriggerCategory
#include "WTBRCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UWTBRStaminaComponent;
class UWTBRVaelComponent;
class UWTBRMovementExtComponent;
class UWTBRInputGestureComponent;

UCLASS(config=Game)
class AWTBRCharacter : public ACharacter
{
    GENERATED_BODY()

    // ─── Camera ──────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    // ─── Input Actions ────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> FireMainAction;   // LMB — Main Trigger (Lock)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> FireSubAction;    // RMB — Sub Trigger (Lock)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> DodgeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchTriggerAction;

public:
    AWTBRCharacter();

    // ─── Components (public — TriggerBase needs direct access) ───────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRHealthComponent> HealthComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRVaelComponent> VaelComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRMovementExtComponent> MovementExtComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRInputGestureComponent> InputGestureComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRTriggerSetComponent> TriggerSetComponent;

    FORCEINLINE TObjectPtr<USpringArmComponent> GetCameraBoom()    const { return CameraBoom; }
    FORCEINLINE TObjectPtr<UCameraComponent>    GetFollowCamera()  const { return FollowCamera; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // ─── Input Handlers ──────────────────────────────────────────────────────
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void FireMain(const FInputActionValue& Value);
    void FireMainReleased(const FInputActionValue& Value);
    void FireSub(const FInputActionValue& Value);
    void FireSubReleased(const FInputActionValue& Value);
    void Dodge(const FInputActionValue& Value);
    void SwitchTrigger(const FInputActionValue& Value);

    // ─── Server RPCs ──────────────────────────────────────────────────────────
    UFUNCTION(Server, Unreliable)
    void Server_Fire(bool bIsMain, bool bIsPressed);

    UFUNCTION(Server, Unreliable)
    void Server_Dodge();

    UFUNCTION(Server, Reliable)
    void Server_SwitchTrigger(int32 SlotIndex, bool bIsMain);

    // ─── Replication ─────────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing=OnRep_ActionPing)
    bool bActionPingActive = false;

    UFUNCTION()
    void OnRep_ActionPing();

private:
    // Dynamic delegate callbacks — UFUNCTION required for AddDynamic binding
    UFUNCTION()
    void OnStaminaChangedHandler(float NewStamina, bool bIsExhausted);

    UFUNCTION()
    void OnLimbDestroyedHandler(EWTBRLimbType Limb, FWTBRLimbState NewState);

    UFUNCTION()
    void OnDualWieldStateChangedHandler(EWTBRDualWieldState NewState, ETriggerCategory ActiveCategory);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
