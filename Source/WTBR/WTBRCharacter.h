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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchMainAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchSubAction;

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

    // ── Stagger System ────────────────────────────────────────────────────────
    // Shared between Nexil Tripwire and Voltis Indoor Hard Landing
    UFUNCTION(BlueprintCallable, Category = "WTBR | Character | Stagger")
    void ApplyStagger(float Duration);

    // ── Trigger Activation RPCs (Blueprint callable) ──────────────────────────
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_ActivateMainTrigger(bool bIsDualWield);

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_ReleaseMainTrigger(bool bIsDualWield);

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_ActivateSubTrigger(bool bIsDualWield);

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_ReleaseSubTrigger(bool bIsDualWield);

    // Blueprint calls this on Q Pressed to cycle main weapons
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_CycleMainSlot();

    // Blueprint calls this on E Pressed to cycle sub weapons
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_CycleSubSlot();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_StartVaelSprint();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_StopVaelSprint();

    UPROPERTY(ReplicatedUsing = OnRep_bIsStaggered, BlueprintReadOnly,
        Category = "WTBR | Character | Stagger")
    bool bIsStaggered = false;

protected:
    virtual void PostInitializeComponents() override;
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
    void SwitchMainTrigger(const FInputActionValue& Value);
    void SwitchSubTrigger(const FInputActionValue& Value);

    // ─── Server RPCs ──────────────────────────────────────────────────────────
    UFUNCTION(Server, Reliable)
    void Server_Fire(bool bIsMain, bool bIsPressed);

    UFUNCTION(Server, Unreliable)
    void Server_Dodge();

    UFUNCTION(Server, Reliable)
    void Server_SwitchTrigger(int32 SlotIndex, bool bIsMain);

    UFUNCTION(Server, Reliable)
    void Server_CycleTrigger(bool bIsMain);

    // ─── Replication ─────────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing=OnRep_ActionPing)
    bool bActionPingActive = false;

    UFUNCTION()
    void OnRep_ActionPing();

    UFUNCTION()
    void OnRep_bIsStaggered();

private:
    // Dynamic delegate callbacks — UFUNCTION required for AddDynamic binding
    UFUNCTION()
    void OnStaminaChangedHandler(float NewStamina, bool bIsExhausted);

    UFUNCTION()
    void OnLimbDestroyedHandler(EWTBRLimbType Limb, FWTBRLimbState NewState);

    UFUNCTION()
    void OnDualWieldStateChangedHandler(EWTBRDualWieldState NewState, ETriggerCategory ActiveCategory);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FTimerHandle StaggerTimer;

    void AddDefaultMappingContext();
    void ApplyInputActionFallbacks();

    UFUNCTION()
    void OnStaggerExpired();
};
