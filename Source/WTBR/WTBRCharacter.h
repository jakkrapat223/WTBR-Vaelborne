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
class UWTBRInteractionComponent;
class UWTBRInventoryComponent;
class UWTBRHUDViewModelComponent;
class UTexture2D;
class AWTBRDroppedTriggerActor;
class AWTBRCorpseLootContainerActor;
class AWTBRGroundItemActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWTBRHUDHintsChanged);

USTRUCT(BlueprintType)
struct FWTBRHUDTriggerVaelAffordability
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    bool bIsCostKnownForHUD = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    bool bHasVaelCost = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    bool bCanAfford = true;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    float BaseVaelCost = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    float EffectiveVaelCost = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    float CurrentVael = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    bool bUsesDrain = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | HUD")
    bool bUsesChargeOrVariableCost = false;
};

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
    TObjectPtr<UInputAction> FireMainAction;   // Main Trigger (Lock)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> FireSubAction;    // Sub Trigger (Lock)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> DodgeAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchTriggerAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchMainAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> SwitchSubAction;

    // Optional — assign IA_Interact in BP_WTBRCharacter or IMC. Defaults null;
    // binding is skipped if not assigned, so the game runs identically without it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> InteractAction;

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
    TObjectPtr<UWTBRInteractionComponent> InteractionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRTriggerSetComponent> TriggerSetComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRInventoryComponent> InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRHUDViewModelComponent> HUDViewModelComponent;

    UPROPERTY(BlueprintAssignable, Category="WTBR | HUD")
    FWTBRHUDHintsChanged OnHUDHintsChanged;

    FORCEINLINE TObjectPtr<USpringArmComponent> GetCameraBoom()    const { return CameraBoom; }
    FORCEINLINE TObjectPtr<UCameraComponent>    GetFollowCamera()  const { return FollowCamera; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetMainTriggerHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetMainTriggerNameText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSubTriggerHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSubTriggerNameText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    UTexture2D* GetMainTriggerHUDIcon() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    UTexture2D* GetSubTriggerHUDIcon() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    int32 GetActiveMainTriggerSlotIndex() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    int32 GetActiveSubTriggerSlotIndex() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FWTBRHUDTriggerVaelAffordability GetActiveMainTriggerVaelAffordabilityForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FWTBRHUDTriggerVaelAffordability GetActiveSubTriggerVaelAffordabilityForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    bool CanAffordActiveMainTriggerForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    bool CanAffordActiveSubTriggerForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveMainTriggerBaseVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveSubTriggerBaseVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveMainTriggerEffectiveVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveSubTriggerEffectiveVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetFireMainInputAction() const { return FireMainAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetFireSubInputAction() const { return FireSubAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetSwitchMainInputAction() const { return SwitchMainAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetSwitchSubInputAction() const { return SwitchSubAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetInteractInputAction() const { return InteractAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    UWTBRHUDViewModelComponent* GetHUDViewModelComponent() const { return HUDViewModelComponent; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSwitchMainHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSwitchSubHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetCancelHintText() const;

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void RefreshHUDHints();

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

    UFUNCTION(BlueprintCallable, Category="WTBR | Input")
    void CancelCurrentAction();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Input")
    void Server_CancelCurrentAction();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Debug")
    void Server_DebugConsumeVaelFailTest();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Debug")
    void Server_DebugStartBelowThresholdTest();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Debug")
    void Server_DebugRefillVaelNoDesperationReset();

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Debug")
    void Server_DebugResetDesperationStateTest();

    UFUNCTION(Exec)
    void WTBRDebugCharacterPrintMatchState() const;

    UFUNCTION(Exec)
    void WTBRDebugCharacterPrintTriggerLoadoutGate() const;

    UFUNCTION(Exec)
    void WTBRDebugCharacterSetMatchPhase(const FString& PhaseName);

    UFUNCTION(Server, Reliable)
    void Server_RequestPickupDroppedTrigger(AWTBRDroppedTriggerActor* DroppedTrigger, int32 TargetSlotIndex);

    UFUNCTION(Server, Reliable)
    void Server_RequestPickupCorpseLootEntry(AWTBRCorpseLootContainerActor* LootContainer, int32 LootEntryIndex, int32 TargetSlotIndex);

    // BR Ground Item pickup (S5-C). Server-authoritative; adds the item to
    // InventoryComponent via the all-or-nothing TryAddItem. Separate from
    // dropped-trigger pickup and does not use trigger pickup/swap match gates.
    UFUNCTION(Server, Reliable)
    void Server_RequestPickupGroundItem(AWTBRGroundItemActor* GroundItem);

    // BR inventory item use (S5-D). Server-authoritative MVP consumable use:
    // HealHP -> HealthComponent->RestoreHP, RestoreVael -> VaelComponent->GrantVael.
    // Consumes exactly one item only if the effect was actually applied.
    UFUNCTION(Server, Reliable)
    void Server_RequestUseInventoryItem(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    AWTBRDroppedTriggerActor* FindAimedDroppedTriggerForPickup() const;

    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    void RequestPickupAimedDroppedTriggerIntoActiveMainSlot();

    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    void RequestPickupAimedDroppedTriggerIntoActiveSubSlot();

    // S7A — constraint-driven single-valid-target dropped trigger pickup.
    // Client-side resolver used by the F context interact (branch 2). Reads the
    // aimed dropped trigger's ETriggerSlotConstraint and dispatches the existing
    // Server_RequestPickupDroppedTrigger to the single valid ACTIVE target slot:
    //   MainOnly -> ActiveMainIndex, SubOnly -> ActiveSubIndex.
    // Constraint Any resolves to two valid active targets, so it is rejected as
    // AmbiguousTargetSlot (dev log only) and no request is dispatched. Invalid,
    // null, or incompatible cases dispatch nothing. This function only requests;
    // the server remains authoritative and re-validates before any mutation.
    // Does not remove or replace the legacy ActiveMain/ActiveSub pickup functions.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    void RequestPickupAimedDroppedTriggerByConstraint();

#if WITH_DEV_AUTOMATION_TESTS
    void SetDroppedTriggerRouteCaptureForTest(bool bEnable);
    void ClearDroppedTriggerRouteCaptureForTest();
    bool WasDroppedTriggerRouteCapturedForTest() const { return bDroppedTriggerRouteCapturedForTest; }
    AWTBRDroppedTriggerActor* GetCapturedDroppedTriggerForTest() const { return CapturedDroppedTriggerForTest.Get(); }
    int32 GetCapturedDroppedTriggerSlotForTest() const { return CapturedDroppedTriggerSlotForTest; }
    ETriggerSlotConstraint GetCapturedDroppedTriggerConstraintForTest() const { return CapturedDroppedTriggerConstraintForTest; }
#endif

    UFUNCTION(Exec)
    void WTBRDebugCharacterPickupNearestDroppedTrigger(int32 TargetSlotIndex);

    UFUNCTION(Exec)
    void WTBRDebugCharacterSpawnCorpseLootContainer();

    UFUNCTION(Exec)
    void WTBRDebugCharacterPrintFocusedInteractionPrompt() const;

    UFUNCTION(Exec)
    void WTBRDebugCharacterLootFocusedCorpseContainer(int32 LootEntryIndex, int32 TargetSlotIndex);

    UFUNCTION(Exec)
    void WTBRDebugCharacterLootNearestCorpseContainer(int32 LootEntryIndex, int32 TargetSlotIndex);

    UFUNCTION(Exec)
    void WTBRDebugCharacterPickupAimedDroppedTriggerActiveMain();

    UFUNCTION(Exec)
    void WTBRDebugCharacterPickupAimedDroppedTriggerActiveSub();

    // Dev-only diagnostic: dumps this character's server-authoritative trigger
    // loadout so PIE can confirm whether death-drop has any DataAssets to drop.
    // Prints name, roles, match gate flags, active main/sub index, and every
    // installed trigger snapshot (SlotIndex / DataAsset path / CachedCategory /
    // SlotConstraint). Logs a clear message when zero snapshots exist.
    UFUNCTION(Exec)
    void WTBRDebugCharacterPrintTriggerSlots() const;

    // Dev-only diagnostic: lists every AWTBRDroppedTriggerActor in the world with
    // name, location, distance from character, distance from camera, DataAsset path,
    // IsConsumed, and root collision info. Used to debug why F focus / aimed pickup
    // fails to detect spawned dropped triggers.
    UFUNCTION(Exec)
    void WTBRDebugCharacterListNearbyDroppedTriggers() const;

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
    void DebugConsumeVaelFailTest();
    void Interact(const FInputActionValue& Value);

    // ─── Server RPCs ──────────────────────────────────────────────────────────
    UFUNCTION(Server, Reliable)
    void Server_Fire(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir);

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

    UFUNCTION()
    void OnActiveTriggerChangedHandler(ETriggerSlot NewSlot);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FTimerHandle StaggerTimer;

    void AddDefaultMappingContext();
    void ApplyInputActionFallbacks();
    void ExecuteServerTriggerInput(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir = FVector::ZeroVector);
    FWTBRHUDTriggerVaelAffordability GetActiveTriggerVaelAffordabilityForHUD(bool bIsMain) const;
    FVector GetClientMoveInputDirectionForTrigger() const;
    static FVector SanitizeClientMoveInputDirection(FVector ClientMoveInputDir);

#if WITH_DEV_AUTOMATION_TESTS
    bool bCaptureDroppedTriggerRouteForTest = false;
    bool bDroppedTriggerRouteCapturedForTest = false;
    TWeakObjectPtr<AWTBRDroppedTriggerActor> CapturedDroppedTriggerForTest;
    int32 CapturedDroppedTriggerSlotForTest = INDEX_NONE;
    ETriggerSlotConstraint CapturedDroppedTriggerConstraintForTest = ETriggerSlotConstraint::Any;
#endif

#if !UE_BUILD_SHIPPING
    // B7D client-side validation harness. Mirrors the B7C server CVar-poll pattern:
    // WTBR.B7ClientValidationDelaySeconds is set via -ExecCmds on a client process,
    // a 1-s poll on NM_Client characters detects it once the pawn is locally
    // controlled, and the sequence runs after the requested delay. Read/log +
    // client-side view rotation + existing server RPC requests only — no client-side
    // inventory/slot/loot/ground-item mutation.
    FTimerHandle B7ClientValidationPollTimerHandle;
    FTimerHandle B7ClientValidationTimerHandle;
    int32 B7ClientValidationRetryCount = 0;
    // 60 retries × 2 s = 120 s window for the validation container to replicate in.
    static constexpr int32 B7ClientValidationMaxRetries = 60;
    void PollB7ClientValidationCVar();
    void RunB7ClientValidationSequence();
    void ContinueB7ClientValidationSequence();
    void FinishB7ClientValidationSequence();
    void LogB7ClientValidationWorldState(const TCHAR* PhaseTag) const;
#endif

    UFUNCTION()
    void OnStaggerExpired();
};
