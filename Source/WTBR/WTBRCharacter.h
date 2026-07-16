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
class UTexture2D;
class AWTBRDroppedTriggerActor;
class AWTBRCorpseLootContainerActor;
class AWTBRGroundItemActor;
class UUserWidget;

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

    // Server-only; AWTBRGameMode team assignment is the intended caller.
    void SetTeamId(int32 NewTeamId);

    UPROPERTY(ReplicatedUsing = OnRep_bIsStaggered, BlueprintReadOnly,
        Category = "WTBR | Character | Stagger")
    bool bIsStaggered = false;

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

protected:
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
    void Server_Dodge();

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
    void OnRep_SerpveilChargeTelegraph();

    UFUNCTION()
    void OnRep_LacernExtendTelegraph();

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
