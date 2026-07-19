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
class UWTBRBagLootViewModelComponent;
class UWTBRRadarWidget;
class UWTBRSniperScopeWidget;
class UTexture2D;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class AWTBRDroppedTriggerActor;
class AWTBRCorpseLootContainerActor;
class AWTBRGroundItemActor;
class AWTBRNexilWireActor;
class UUserWidget;

UENUM(BlueprintType)
enum class EWTBRCharacterStance : uint8
{
    Standing UMETA(DisplayName = "Standing"),
    Crouching UMETA(DisplayName = "Crouching"),
    Prone UMETA(DisplayName = "Prone"),
};

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
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> ProneAction;

    // Keyboard fallbacks for the stance actions. Change these in the character
    // Blueprint's Class Defaults; Enhanced Input actions remain available for
    // gamepad and future player-remapping UI.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Stance", meta=(AllowPrivateAccess="true"))
    FKey CrouchKey;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Stance", meta=(AllowPrivateAccess="true"))
    FKey ProneKey;

    // Leave this enabled until IA_Crouch and IA_Prone have mappings in the
    // active Input Mapping Context. Disable it once those mappings are authored
    // to avoid one key firing both binding paths.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Stance", meta=(AllowPrivateAccess="true"))
    bool bUseDirectStanceKeyBindings = true;

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

    // Optional — assign IA_Cancel in BP_WTBRCharacter or IMC (IA_Cancel already
    // exists and is mapped in IMC_WTBR_Default; only the CDO/BP reference needs
    // assigning). Defaults null; binding is skipped if not assigned, so the game
    // runs identically without it. Priority-chain behavior: closes local UI if a
    // panel is open, otherwise falls back to the existing trigger-charge cancel.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> CancelAction;

    // Optional — owner must author IA_CompositeMerge (suggested default key: V;
    // MMB is reserved for a future Ping system), add it to the default IMC, and
    // assign it on BP_WTBRCharacter. Defaults null; binding is skipped when unassigned.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> CompositeMergeAction;

    // Optional -- assign IA_Bag in BP_WTBRCharacter or IMC (IA_Bag is mapped to Z
    // in IMC_WTBR_Default). Defaults null; binding is skipped if not assigned, so
    // the game runs identically without it. Routes to the existing
    // ToggleBagLootLayer() Bag/Loot presentation toggle.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> BagAction;

public:
    // Takes an ObjectInitializer so the character movement component can be
    // swapped for UWTBRCharacterMovementComponent, which makes prone a predicted
    // movement state instead of a server-only capsule change.
    explicit AWTBRCharacter(const FObjectInitializer& ObjectInitializer);

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
    TObjectPtr<UWTBRBagLootViewModelComponent> BagLootViewModelComponent;

    UPROPERTY(BlueprintAssignable, Category="WTBR | HUD")
    FWTBRHUDHintsChanged OnHUDHintsChanged;

    // ─── Runtime UI (Phase 6A smoke-test scaffold) ────────────────────────────
    // TEMPORARY: Character-owned UI is accepted only for the first PIE smoke
    // test because no custom APlayerController/AHUD subclass exists yet (Phase
    // 5A audit finding). Migrate ownership to a PlayerController/HUD/UI-manager
    // once BR respawn/pawn-swap exists — Character-owned widgets do not survive
    // pawn destruction.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | UI")
    TSubclassOf<UUserWidget> BagLootWidgetClass;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UUserWidget> HUDWidgetInstance;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UUserWidget> BagLootWidgetInstance;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UWTBRRadarWidget> RadarWidgetInstance;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UWTBRSniperScopeWidget> ScopeWidgetInstance;

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

    // Server-only entry point for AI controllers. It deliberately reuses the
    // exact input routing used by players (tap/release, cooldown, Vael, and
    // dual-wield validation) rather than letting bots call Trigger classes.
    void ExecuteBotTriggerInput(bool bIsMain, bool bIsPressed);

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveMainTriggerBaseVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveSubTriggerBaseVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveMainTriggerEffectiveVaelCostForHUD() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetActiveSubTriggerEffectiveVaelCostForHUD() const;

    // 0 at resting FOV, 1 at the current Sniper's fully-zoomed target FOV —
    // tracks AWTBRCharacter::UpdateSniperZoom's live lerp so scope UI (reticle
    // tighten, vignette) can fade smoothly with it rather than snapping on
    // press/release.
    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    float GetSniperZoomAlphaForHUD() const;

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

    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    UInputAction* GetCompositeMergeInputAction() const { return CompositeMergeAction; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    UWTBRHUDViewModelComponent* GetHUDViewModelComponent() const { return HUDViewModelComponent; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    UWTBRBagLootViewModelComponent* GetBagLootViewModelComponent() const { return BagLootViewModelComponent; }

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSwitchMainHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetSwitchSubHintText() const;

    UFUNCTION(BlueprintPure, Category="WTBR | HUD")
    FText GetCancelHintText() const;

    UFUNCTION(BlueprintCallable, Category="WTBR | HUD")
    void RefreshHUDHints();

    // Phase 6C: read-only readiness check for this smoke-test scaffold — true
    // only if both HUDWidgetClass and BagLootWidgetClass are assigned. Never a
    // gameplay blocker: missing classes are always a safe no-op elsewhere (see
    // CreateLocalPlayerUI/ShowBagLootLayer below), never a crash. Pass
    // bLogIfMissing=true to emit one WTBR_VALIDATION_LOG line per missing class
    // (gated behind wtbr.Debug.ValidationLogs like every other log in this
    // scaffold — off by default, no spam).
    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    bool ValidateLocalUISetup(bool bLogIfMissing = true) const;

    // ── Runtime UI lifecycle (Phase 6A smoke-test scaffold) ──────────────────
    // Creates HUDWidgetInstance/BagLootWidgetInstance (if their *WidgetClass is
    // set and not already created) and adds them to the viewport. Bag/Loot
    // starts hidden (Collapsed) above the HUD in Z-order. Locally-controlled
    // only; idempotent — safe to call more than once.
    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void CreateLocalPlayerUI();

    // Removes both widgets from the viewport and nulls the instance pointers.
    // Safe to call multiple times, including when nothing was ever created.
    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void DestroyLocalPlayerUI();

    // Presentation-only visibility toggle for the Bag/Loot layer. Never
    // mutates inventory, loot, ground items, or trigger state. Creates the UI
    // first (locally-controlled only) if it does not exist yet.
    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void ShowBagLootLayer();

    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void HideBagLootLayer();

    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void ToggleBagLootLayer();

    UFUNCTION(BlueprintPure, Category="WTBR | UI")
    bool IsBagLootLayerVisible() const;

    // Phase 6B: closes whichever locally-visible UI panel is currently open
    // (today, only the Bag/Loot layer). A single stable entry point for a
    // future Cancel/close input binding — see the manual input setup note in
    // the Phase 6A/6B test plan section. Presentation-only; safe to call on
    // non-locally-controlled/server characters (no-op if nothing is open).
    UFUNCTION(BlueprintCallable, Category="WTBR | UI")
    void CloseAnyOpenLocalUI();

    UFUNCTION(BlueprintPure, Category="WTBR | UI")
    bool IsAnyLocalUIPanelOpen() const;

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

    // The movement component owns sprint stamina, but the character owns the
    // stance restrictions that determine whether sprint is legal.
    bool CanStartVaelSprint() const;

    UFUNCTION(BlueprintPure, Category="WTBR | Movement | Stance")
    EWTBRCharacterStance GetCharacterStance() const { return CharacterStance; }

    UFUNCTION(BlueprintPure, Category="WTBR | Movement | Stance")
    bool IsProne() const { return CharacterStance == EWTBRCharacterStance::Prone; }

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="WTBR | Movement | Stance")
    void Server_SetCharacterStance(EWTBRCharacterStance NewStance);

    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Movement | Stance")
    void OnCharacterStanceChanged(EWTBRCharacterStance NewStance);

    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Movement | Dodge")
    void OnDodgeStarted(FVector Direction);

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

    // Hold-to-revive (design lock 2026-07-13): starts/stops a revive on Target.
    // Thin server-authoritative dispatch into
    // UWTBRHealthComponent::TryStartRevive/StopRevive, which own all validation
    // (Downed state, same-team living reviver, one reviver at a time) and the
    // entire non-interrupted hold-duration timer — this RPC pair never tracks
    // hold duration itself. Called by UWTBRInteractionComponent on IA_Interact
    // Started/Completed via AWTBRCharacter::Interact()/InteractReleased().
    UFUNCTION(Server, Reliable)
    void Server_RequestStartRevive(AWTBRCharacter* Target);

    UFUNCTION(Server, Reliable)
    void Server_RequestStopRevive(AWTBRCharacter* Target);

    // Nexil zipline (owner + teammates only — enemy trip mechanic is untouched):
    // F-grab dispatched from UWTBRInteractionComponent's P4 focus branch.
    // Re-validates AWTBRNexilWireActor::CanBeGrabbedBy + a sane grab range
    // server-side (client focus is advisory only). Switches movement to
    // MOVE_Flying (no gravity, camera still fully controllable, no existing
    // "suspended in place" mode in this codebase to reuse) and blocks Fire via
    // bIsHangingOnNexilWire until release.
    UFUNCTION(Server, Reliable)
    void Server_GrabNexilWire(AWTBRNexilWireActor* Wire);

    // Dispatched by UWTBRInputGestureComponent::OnJump_Started in place of a
    // normal jump while bIsHangingOnNexilWire is true. Launches toward wherever
    // the character (server-authoritative control rotation) is aimed at the
    // moment of release — read fresh at release time, same convention as the
    // Sniper aim-then-fire timing. Wire itself is untouched (persists, reusable).
    UFUNCTION(Server, Reliable)
    void Server_ReleaseNexilWireAndLaunch();

    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil Wire | Zipline")
    bool IsHangingOnNexilWire() const { return bIsHangingOnNexilWire; }

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

    // Round-scoped team identity. INDEX_NONE = no team (individual/1v1 harness).
    // Assigned server-side by AWTBRGameMode at InMatch entry when the active match
    // rules have bAssignTeamsAtMatchStart; never set from client input. Lives on
    // the Character (not PlayerState) deliberately: rounds re-assign it, the
    // headless automation fixtures spawn bare characters, and every consumer
    // (friendly fire, scoring) already holds an AWTBRCharacter.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Character | Team")
    int32 TeamId = INDEX_NONE;

    UFUNCTION(BlueprintPure, Category = "WTBR | Character | Team")
    int32 GetTeamId() const { return TeamId; }

    UFUNCTION(BlueprintPure, Category = "WTBR | Character | Team")
    bool HasTeam() const { return TeamId != INDEX_NONE; }

    // Same assigned team only — two team-less characters are NOT teammates.
    UFUNCTION(BlueprintPure, Category = "WTBR | Character | Team")
    bool IsSameTeamAs(const AWTBRCharacter* Other) const
    {
        return Other != nullptr && HasTeam() && Other->TeamId == TeamId;
    }

    // Finds the closest-to-crosshair living enemy in range, inside the supplied
    // aim cone, with an unobstructed visibility trace. Used by homing attacks.
    static AWTBRCharacter* FindBestHomingTarget(
        AWTBRCharacter* QueryingCharacter,
        float SearchRadius,
        float AimConeHalfAngleDegrees);

    // Same filters as FindBestHomingTarget (alive/enemy-team/range/aim-cone/LOS), but collects up
    // to MaxTargets DISTINCT targets ordered by the same aim-then-distance-then-name comparator.
    // Used by composites needing genuine simultaneous multi-target homing (e.g. Venspire).
    static void FindBestHomingTargets(
        AWTBRCharacter* QueryingCharacter,
        float SearchRadius,
        float AimConeHalfAngleDegrees,
        int32 MaxTargets,
        TArray<AWTBRCharacter*>& OutTargets);

    // Mirror of FindBestHomingTarget with the team filter inverted (SAME team only,
    // excludes self) — deliberately duplicated rather than parameterized, matching this
    // codebase's existing convention for FindBestHomingTargets. Used by Voltis Hold's
    // aim-at-teammate direct-apply branch.
    static AWTBRCharacter* FindBestFriendlyTarget(
        AWTBRCharacter* QueryingCharacter,
        float SearchRadius,
        float AimConeHalfAngleDegrees);

    // Server-only; AWTBRGameMode team assignment is the intended caller.
    void SetTeamId(int32 NewTeamId);

    UPROPERTY(ReplicatedUsing = OnRep_bIsStaggered, BlueprintReadOnly,
        Category = "WTBR | Character | Stagger")
    bool bIsStaggered = false;

    // Native ACharacter replicates crouch. This explicit stance also covers
    // custom prone and drives the Animation Blueprint on every client.
    UPROPERTY(ReplicatedUsing = OnRep_CharacterStance, BlueprintReadOnly,
        Category = "WTBR | Movement | Stance")
    EWTBRCharacterStance CharacterStance = EWTBRCharacterStance::Standing;

    // Nexil zipline hang state — mirrors bIsStaggered's RepNotify pattern so
    // every client (not just the owner) can react cosmetically later.
    UPROPERTY(ReplicatedUsing = OnRep_bIsHangingOnNexilWire, BlueprintReadOnly,
        Category = "WTBR | Nexil Wire | Zipline")
    bool bIsHangingOnNexilWire = false;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil Wire | Zipline")
    void OnHangingOnNexilWireChanged(bool bHanging);

    // Server-authoritative telegraph of "is this character's Serpveil actively
    // charging right now" — mirrors bIsStaggered's RepNotify pattern so ALL
    // clients (not just the locally-controlled owner) can react cosmetically.
    UPROPERTY(ReplicatedUsing = OnRep_SerpveilChargeTelegraph, BlueprintReadOnly,
        Category = "WTBR | Trigger | VFX")
    bool bSerpveilChargeTelegraphActive = false;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Trigger | VFX")
    void OnSerpveilChargeTelegraphChanged(bool bActive);

    // Server-only setter, called by UWTBRSerpveilTrigger from its existing
    // authority-gated paths. Idempotent — no-ops if the value is unchanged.
    void SetSerpveilChargeTelegraphActive(bool bActive);

    // Server-authoritative telegraph of "is this character's Lacern blade
    // currently extending" — mirrors bSerpveilChargeTelegraphActive's RepNotify
    // pattern so ALL clients (not just the locally-controlled owner) can react
    // cosmetically. UWTBRLacernTrigger's own BlueprintImplementableEvent hooks
    // only fire on the server's object graph, so they cannot drive client VFX
    // directly — this replicated bool is the bridge.
    UPROPERTY(ReplicatedUsing = OnRep_LacernExtendTelegraph, BlueprintReadOnly,
        Category = "WTBR | Trigger | VFX")
    bool bLacernExtendTelegraphActive = false;

    // Companion flag — plain Replicated, no separate OnRep needed: UE applies
    // all properties dirtied in the same net update before firing any OnRep,
    // so this is guaranteed valid by the time OnRep_LacernExtendTelegraph reads it.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Trigger | VFX")
    bool bLacernExtendDualWield = false;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Trigger | VFX")
    void OnLacernExtendTelegraphChanged(bool bActive, bool bIsDualWield);

    // Server-only setter, called by UWTBRLacernTrigger from its existing
    // authority-gated paths. Idempotent — no-ops if neither sub-value changed.
    void SetLacernExtendTelegraphActive(bool bActive, bool bIsDualWield);

    // One-shot, server->all-clients cosmetic hit burst. Unreliable: purely
    // cosmetic, an occasional dropped packet under loss is an acceptable
    // trade vs. head-of-line blocking a Reliable channel.
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_LacernHit(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_VoltisBurst(AWTBRCharacter* TargetCharacter, bool bIsAllyBoost, FVector Direction);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Trigger | VFX")
    void OnLacernHitReceived(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit);

    // Central cosmetic-only cleanup for every replicated Trigger VFX telegraph
    // on this character. Call this whenever the active trigger is switched or
    // otherwise interrupted BEFORE its own normal completion path would have
    // cleared its telegraph — e.g. UWTBRTriggerSetComponent's slot switch/cycle
    // functions, which reassign the active slot without deactivating the
    // outgoing trigger. Server-only, idempotent (each setter below already
    // no-ops if its state is already false), safe to call with nothing active.
    void ClearTriggerCosmeticVFXState();

    // Crouch-jump: jumping while crouched stands the character up first, matching
    // the shooter convention (PUBG/Apex/Valorant). Prone must be exited manually,
    // which is also the convention — see CanJumpInternal_Implementation. Public
    // like the ACharacter version it overrides, since UWTBRInputGestureComponent
    // dispatches the jump input through it.
    virtual void Jump() override;

protected:
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual bool CanJumpInternal_Implementation() const override;

    // Both call RefreshStanceViewOffsets so the camera never rides the shrinking
    // capsule down into the ground.
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    // ─── Input Handlers ──────────────────────────────────────────────────────
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void FireMain(const FInputActionValue& Value);
    void FireMainReleased(const FInputActionValue& Value);
    void FireSub(const FInputActionValue& Value);
    void FireSubReleased(const FInputActionValue& Value);
    void DodgeStarted(const FInputActionValue& Value);
    void DodgeReleased(const FInputActionValue& Value);
    void ToggleCrouch(const FInputActionValue& Value);
    void ToggleProne(const FInputActionValue& Value);
    void ToggleCrouchKey();
    void ToggleProneKey();
    void BeginVaelSprintFromDodgeHold();
    void SwitchTrigger(const FInputActionValue& Value);
    void SwitchMainTrigger(const FInputActionValue& Value);
    void SwitchSubTrigger(const FInputActionValue& Value);
    void DebugConsumeVaelFailTest();
    void Interact(const FInputActionValue& Value);
    // IA_Interact Completed (button release): stops any in-progress revive this
    // client started. No-op for every other interact type (tap-only, nothing to
    // release).
    void InteractReleased(const FInputActionValue& Value);

    // IA_Cancel handler: closes any open local UI panel (e.g. Bag/Loot) if one is
    // open; otherwise falls back to the existing CancelCurrentAction() trigger-
    // charge cancel. Presentation-only routing — no new gameplay mutation, no
    // change to Server_CancelCurrentAction's existing behavior.
    void HandleCancelInput();

    void HandleCompositeMergeInput();

    // ─── Server RPCs ──────────────────────────────────────────────────────────
    UFUNCTION(Server, Reliable)
    void Server_Fire(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir);

    UFUNCTION(Server, Unreliable)
    void Server_Dodge(FVector ClientMoveInputDir);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayDodgeCosmetic(FVector_NetQuantizeNormal Direction);

    UFUNCTION(Server, Reliable)
    void Server_SwitchTrigger(int32 SlotIndex, bool bIsMain);

    UFUNCTION(Server, Reliable)
    void Server_CycleTrigger(bool bIsMain);

    // ─── Replication ─────────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing=OnRep_ActionPing)
    bool bActionPingActive = false;

    // Set server-side by Vexorn. All clients omit a cloaked character from radar.
    UPROPERTY(ReplicatedUsing=OnRep_RadarCloaked, BlueprintReadOnly, Category="WTBR | Radar")
    bool bRadarCloaked = false;

public:
    UFUNCTION(BlueprintPure, Category="WTBR | Radar")
    bool IsRadarCloaked() const { return bRadarCloaked; }

    void SetRadarCloaked(bool bNewCloaked);

private:

    UFUNCTION()
    void OnRep_ActionPing();

    UFUNCTION()
    void OnRep_RadarCloaked();

    UFUNCTION()
    void OnRep_bIsStaggered();

    UFUNCTION()
    void OnRep_CharacterStance();

    UFUNCTION()
    void OnRep_bIsHangingOnNexilWire();

    UFUNCTION()
    void OnRep_SerpveilChargeTelegraph();

    UFUNCTION()
    void OnRep_LacernExtendTelegraph();

    // Owning-client prediction of the native crouch that accompanies a stance
    // request, so client and server simulate the same capsule/speed. No-op on the
    // authority and on non-owning clients.
    void PredictNativeCrouchForStance(EWTBRCharacterStance Desired);

    // Raises/lowers the predicted prone flag on UWTBRCharacterMovementComponent.
    // Safe no-op if the movement component was overridden with a plain one.
    void SetWantsToProne(bool bNewWantsToProne);

    bool CanResizeCapsuleTo(float NewHalfHeight) const;
    // bKeepFeetPlanted moves the actor so the capsule's base stays put. That is
    // right on the authority, but wrong on a replicated client: the location that
    // arrives from the server already accounts for the resize, so re-applying the
    // offset double-counts it and sinks the character through the floor.
    void SetCapsuleHalfHeightKeepingBase(float NewHalfHeight, bool bKeepFeetPlanted = true);
    void ApplyReplicatedStance();

public:
    // How much of the capsule's shrink the camera boom compensates for, so the view
    // does not ride the capsule centre down into the floor. 1 = the camera holds a
    // constant world height in every stance (default); 0 = it drops the full amount,
    // which is what made the spring arm collide with the ground and jam the camera
    // up against the character while crouch-walking and crawling.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Movement | Stance",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StanceCameraHeightCompensation = 1.0f;

    // Single authority for the mesh and camera-boom offsets that must track the
    // capsule height. Both are assigned absolutely from the current half-height —
    // ACharacter::OnStartCrouch overwrites the mesh offset outright, so anything
    // additive elsewhere silently loses.
    void RefreshStanceViewOffsets();

    // Server-authoritative stance change — Server_SetCharacterStance is a thin RPC
    // wrapper around this. Returns false when the stance is refused (no headroom to
    // stand, cannot crouch in the current movement state, staggered, hanging, dead).
    bool TrySetCharacterStance(EWTBRCharacterStance NewStance);

    // Single authority for MaxWalkSpeed / MaxWalkSpeedCrouched: folds the movement
    // penalties, the sprint multiplier and the crouch/prone multiplier together.
    // UWTBRMovementExtComponent calls this instead of writing MaxWalkSpeed itself,
    // so a speed recompute can never drop the stance multiplier.
    void RefreshStanceSpeeds();

private:
    void EndDodgeCooldown();

private:
    // Nexil zipline: which wire this character is currently hanging on, if any.
    TWeakObjectPtr<AWTBRNexilWireActor> HangingNexilWire;

    // Ends a zipline hang: restores falling movement, clears the state, and
    // unsubscribes from the wire. bLaunch=true fires the normal aim-directed
    // release launch; false just drops (used when the wire vanishes underneath).
    void EndNexilWireHang(bool bLaunch);

    // Bound to the hung-on wire's OnDestroyed while hanging. Without this, a wire
    // that expires (WireDuration) or is cut mid-hang left the character stuck in
    // MOVE_Flying, floating in mid-air and still able to launch off nothing.
    UFUNCTION()
    void HandleHangingNexilWireDestroyed(AActor* DestroyedActor);

    EWTBRCharacterStance LastAppliedStance = EWTBRCharacterStance::Standing;
    FTimerHandle DodgeHoldTimer;
    FTimerHandle DodgeCooldownTimer;
    bool bDodgeButtonHeld = false;
    bool bDodgeHoldStartedSprint = false;
    bool bDodgeOnCooldown = false;

    UPROPERTY(EditDefaultsOnly, Category="WTBR | Movement | Dodge", meta=(ClampMin="0.0"))
    float DodgeIFrameDuration = 0.22f;

    float StandingCapsuleHalfHeight = 96.0f;
    float ProneCapsuleHalfHeight = 36.0f;
    static constexpr float CROUCH_SPEED_MULTIPLIER = 0.40f;
    static constexpr float PRONE_SPEED_MULTIPLIER = 0.20f;
    static constexpr float DODGE_HOLD_THRESHOLD = 0.20f;
    static constexpr float DODGE_SPEED = 900.0f;
    static constexpr float DODGE_COOLDOWN = 0.35f;

    // Dynamic delegate callbacks — UFUNCTION required for AddDynamic binding
    UFUNCTION()
    void OnStaminaChangedHandler(float NewStamina, bool bIsExhausted);

    UFUNCTION()
    void OnLimbDestroyedHandler(EWTBRLimbType Limb, FWTBRLimbState NewState);

    UFUNCTION()
    void OnDualWieldStateChangedHandler(EWTBRDualWieldState NewState, ETriggerCategory ActiveCategory);

    UFUNCTION()
    void OnActiveTriggerChangedHandler(ETriggerSlot NewSlot);

    // Phase 6A: bound to InteractionComponent->OnCorpseLootInteractRequested
    // (fires on the owning client when Interact resolves to a valid focused
    // corpse/container). Presentation-only — shows the Bag/Loot layer via
    // ShowBagLootLayer(); does not request loot or mutate any gameplay state.
    UFUNCTION()
    void OnCorpseLootInteractRequestedHandler(AWTBRCorpseLootContainerActor* Container);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FTimerHandle StaggerTimer;

    // ─── Sniper zoom (client-only cosmetic, no server round trip) ─────────────
    // Holding Fire while a Sniper Trigger (Fulgris/Piercex/Telorn) is the active
    // Main/Sub narrows FollowCamera's FOV toward that weapon's GetZoomFOV().
    // Purely visual — does not touch fire timing, accuracy, or the existing
    // Server_Fire/Activate path, which still fires on press exactly as before.
    void UpdateSniperZoom(bool bIsMain, bool bZoomIn);

    UFUNCTION()
    void TickSniperZoomLerp();

    float DefaultCameraFOV = 90.0f;
    float SniperZoomTargetFOV = 90.0f;
    // Tracked per-slot (not a single bool) so releasing Main while Sub is
    // also a held Sniper doesn't incorrectly snap the FOV back to default.
    bool bMainWantsSniperZoom = false;
    bool bSubWantsSniperZoom = false;
    FTimerHandle SniperZoomLerpTimer;

    // Client-predicted Sniper cooldown gate (owner-requested 2026-07-17):
    // UWTBRSniperTrigger::bIsOnCooldown is server-only state (Activate/
    // OnReleased both early-return on !HasAuthority(), so the client's own
    // local Trigger instance never learns it), so without this a player
    // could hold Fire again immediately after a shot, see the zoom/scope
    // cosmetic play out as if aiming, then release and get no shot at all
    // (server's Activate silently rejects the still-cooling-down trigger).
    // Predicted from GetCooldownDuration() (a pure DA read, safe
    // client-side) when a gate-accepted Fire press is released — an
    // approximation, not
    // exact server truth (a Vael-starved miss still starts this timer even
    // though the server never actually set cooldown), but that only ever
    // makes the client slightly more conservative, never desyncs it into
    // showing a scope that won't fire. GetWorld()->GetTimeSeconds() compared
    // against these; tracked per-slot like the zoom-want flags above.
    float MainSniperCooldownPredictedUntil = 0.0f;
    float SubSniperCooldownPredictedUntil = 0.0f;

    // True only from a scope press that passed its slot's predicted cooldown
    // gate until the matching release. This prevents a rejected press from
    // extending the cosmetic lockout beyond the real Sniper cooldown.
    bool bMainSniperZoomPressAccepted = false;
    bool bSubSniperZoomPressAccepted = false;

    // Native Lacern VFX fallback for replicated clients. Blueprint events above
    // still fire, but this keeps the Vaelborne slash trail working even when a
    // BP has not bound the cosmetic graph yet.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true"))
    bool bUseBuiltInLacernVFX = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UNiagaraSystem> LacernSlashTrailEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UNiagaraSystem> LacernHitImpactEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Voltis",
        meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UNiagaraSystem> VoltisTapLaunchEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Voltis",
        meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UNiagaraSystem> VoltisAllyBoostEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true"))
    FName LacernBladeTipMarkerName = TEXT("BladeTipMarker");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true"))
    FName LacernWeaponSocketName = TEXT("weapon_r");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float LacernSlashWidth = 26.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Trigger | VFX | Lacern",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
    float LacernTrailLifetime = 0.18f;

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> NativeLacernBladeTipMarker;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> NativeLacernSlashTrail;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> NativeLacernDualSlashTrail;

    FTimerHandle NativeLacernTrailTimer;
    float NativeLacernTrailElapsed = 0.0f;
    bool bNativeLacernTrailDualWield = false;

    void NativeHandleLacernExtendTelegraphChanged(bool bActive, bool bIsDualWield);
    void NativeHandleLacernHitReceived(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit);
    void NativeHandleVoltisBurst(AWTBRCharacter* TargetCharacter, bool bIsAllyBoost, FVector Direction);
    void TickNativeLacernTrail();
    void StopNativeLacernTrail();
    USceneComponent* ResolveNativeLacernTrailAttachParent(FName& OutAttachSocketName) const;
    USceneComponent* EnsureNativeLacernBladeTipMarker();
    void ApplyNativeLacernTrailParameters(UNiagaraComponent* NiagaraComponent, float CurrentDist, float MaxDist) const;
    void GetNativeLacernMotionParams(float& OutExtendLength, float& OutExtendSpeed,
        float& OutRetractSpeed, float& OutDualOffset) const;
    static float ComputeNativeLacernBladeTipDistance(float Elapsed, float ExtendLength,
        float ExtendSpeed, float RetractSpeed);

    // BR/FPS-style scope view (owner request 2026-07-17, supersedes the plain
    // third-person zoom): while a Sniper is aimed, the boom collapses to
    // TargetArmLength=0 (camera sits at the existing boom pivot instead of
    // behind it) and the local player's own mesh is hidden so the view reads
    // as looking THROUGH a scope; the circular lens overlay is
    // UWTBRSniperScopeWidget's job. Client-side cosmetic only — restores the
    // exact saved boom settings on release, so whatever the Character BP
    // authored (not the C++ defaults) comes back. Collision test + camera lag
    // are disabled while scoped (2026-07-17 fix — the first attempt left them
    // on and the resulting view faced the wrong way, most likely the spring
    // arm's collision probe reacting to being started inside the character's
    // own capsule at zero arm length).
    void SetSniperScopeView(bool bActive);
    bool bSniperScopeViewActive = false;
    float ScopeSavedArmLength = 400.0f;
    FVector ScopeSavedSocketOffset = FVector::ZeroVector;
    bool ScopeSavedDoCollisionTest = true;
    bool ScopeSavedEnableCameraLag = false;
    bool ScopeSavedEnableCameraRotationLag = false;

    static constexpr float SNIPER_ZOOM_LERP_SPEED = 8.0f;
    static constexpr float SNIPER_ZOOM_TICK_INTERVAL = 0.016f;

    void AddDefaultMappingContext();
    void ApplyInputActionFallbacks();
    void ExecuteServerTriggerInput(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir = FVector::ZeroVector);

    // Server-side routing of LMB/RMB while in Mantorn (Feryx+Feryx) form.
    // Returns true if the input was consumed by the form (caller then skips the
    // normal per-slot trigger path). Classifies a single-side press as Tap=whip or
    // Hold=spin on release, using MantornParams.InFormHoldThreshold.
    bool HandleMantornFormInput(bool bIsMain, bool bIsPressed);

    // Tracks the single in-form attack press (server only). A second side pressing
    // while one is held is ignored (a two-side hold is the exit gesture instead).
    bool  bMantornAttackHeld = false;
    bool  bMantornAttackSideIsMain = false;
    float MantornAttackPressTime = 0.f;

    // Server-side routing of LMB/RMB while the active slot has a Trigger Option
    // attached (canon Senkū: Arcven attached to a Lacern slot). Defers Trigger
    // (Lacern)'s normal instant-on-press Activate to release, so a hold has a
    // chance to charge the Option (Arcven) instead. Per-side (Main/Sub each
    // track their own hold independently, unlike the single Mantorn form).
    bool HandleLacernHoldOptionInput(
        bool bIsMain, bool bIsPressed,
        UWTBRTriggerBase* Trigger, UWTBRTriggerBase* OptionTrigger,
        bool bIsDualWield);

    bool  bLacernChargeHeldMain = false;
    float LacernChargePressTimeMain = 0.f;
    bool  bLacernChargeHeldSub = false;
    float LacernChargePressTimeSub = 0.f;

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
