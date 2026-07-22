// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Components/WTBRHealthComponent.h"    // EWTBRLimbType, FWTBRLimbState
#include "Trigger/WTBRTriggerSetComponent.h"   // EWTBRDualWieldState, ETriggerCategory
#include "WTBRCharacter.generated.h"

struct FWTBRPathPreset;

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
class UWTBRTriggerWheelWidget;
class UWTBRMarkPingHUDWidget;
class UWTBREscudoPresetWheelWidget;
class UWTBREscudoTrigger;
class UWTBRSerpveilTrigger;
class UWTBRVenyxTrigger;
class AWTBRAegornWallActor;
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

// Escudo Hold/Preset flow (client-local only — see
// wtbr-escudo-hold-preset-design-lock project memory for the full design and
// why none of this can live inside UWTBREscudoTrigger::Activate_Implementation).
UENUM(BlueprintType)
enum class EWTBREscudoPresetFlowState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    WaitingForHoldDecision UMETA(DisplayName = "Waiting For Hold Decision"),
    WheelOpen UMETA(DisplayName = "Wheel Open"),
    GhostPreview UMETA(DisplayName = "Ghost Preview"),
};

// Serpveil (Viper) Hold/Preset flow — client-local, same architecture and for
// the same reason as EWTBREscudoPresetFlowState above. Two-stage by owner's
// choice: hold opens the wheel, releasing arms a preset, then a SECOND
// press-and-hold charges range and its release fires.
//
// This enum covers the SELECTION phase only, which is deliberately one-at-a-time
// (two wheels open at once would be unreadable). Once a preset is armed it is
// slot-independent, and CHARGING is tracked per slot instead — Main and Sub can
// charge and fire the same armed preset simultaneously, which is the whole
// reason the Fibonacci scatter interleaves by parity.
UENUM(BlueprintType)
enum class EWTBRSerpveilPresetFlowState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    WaitingForHoldDecision UMETA(DisplayName = "Waiting For Hold Decision"),
    WheelOpen UMETA(DisplayName = "Wheel Open"),
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

// Server -> owning-client(s)-only payload for the Mark/Ping system (MMB by convention,
// see IMC). Replicated with COND_OwnerOnly on IncomingMarkPing below, so only the
// instigator's teammates ever receive a copy — enemies never see it, by construction
// of Unreal's replication conditions rather than a client-side visibility filter.
// Deliberately named "MarkPing" rather than reusing "ActionPing": that name is already
// taken by the unrelated stealth/detection system (bActionPingActive, see
// UWTBRActionPingSubsystem) that fires when Vael energy leaves a character's bounds.
USTRUCT()
struct FWTBRMarkPingPayload
{
    GENERATED_BODY()

    UPROPERTY()
    FVector_NetQuantize Location = FVector_NetQuantize::ZeroVector;

    // Set only for an enemy ping (nullptr for a plain location ping).
    UPROPERTY()
    TObjectPtr<AActor> MarkedActor = nullptr;

    UPROPERTY()
    bool bIsEnemy = false;

    UPROPERTY()
    TObjectPtr<AActor> Instigator = nullptr;

    // Server GetTimeSeconds() at the moment of the ping. Not used for client-side
    // expiry math (server/client clocks are not synced) — it exists purely so two
    // pings can never replicate as byte-identical, which would otherwise silently
    // skip OnRep on the receiving client (Unreal only fires OnRep on an actual change).
    UPROPERTY()
    double ServerTimeSeconds = 0.0;
};

// Client-local, non-replicated bookkeeping for one still-visible Mark/Ping. Built
// from FWTBRMarkPingPayload inside OnRep_IncomingMarkPing / ApplyIncomingMarkPing.
USTRUCT(BlueprintType)
struct FWTBRActiveMarkPing
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Mark Ping")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Mark Ping")
    TWeakObjectPtr<AActor> MarkedActor;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Mark Ping")
    bool bIsEnemy = false;

    UPROPERTY(BlueprintReadOnly, Category="WTBR | Mark Ping")
    TWeakObjectPtr<AActor> Instigator;

    // Local (client) GetTimeSeconds() deadline; the HUD widget hides/prunes past this.
    UPROPERTY(BlueprintReadOnly, Category="WTBR | Mark Ping")
    double ExpireTimeSeconds = 0.0;
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

    // Optional — assign IA_MarkPing in BP_WTBRCharacter or IMC (default key MMB, the
    // Apex/Fortnite/PUBG convention). Defaults null; binding is skipped if not
    // assigned, so the game runs identically without it. See
    // Server_RequestMarkPing/GetMarkPingRecipients below.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UInputAction> MarkPingAction;

    // Direct-key fallback, same pattern/purpose as CrouchKey/ProneKey above: MMB
    // works with zero Input Mapping Context authoring. Unlike CrouchKey/ProneKey
    // this ships with a real default (the design lock names MMB specifically, see
    // [[wtbr-mark-ping-design-lock]] project memory — it isn't a per-project
    // choice the way the stance keys are), so Mark/Ping is live immediately.
    // Disable bUseDirectMarkPingKeyBinding once IA_MarkPing is mapped in the IMC,
    // same reasoning as bUseDirectStanceKeyBindings, to avoid firing both paths.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Mark Ping", meta=(AllowPrivateAccess="true"))
    FKey MarkPingKey = EKeys::MiddleMouseButton;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Mark Ping", meta=(AllowPrivateAccess="true"))
    bool bUseDirectMarkPingKeyBinding = true;

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

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UWTBRTriggerWheelWidget> TriggerWheelWidgetInstance;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UWTBRMarkPingHUDWidget> MarkPingHUDWidgetInstance;

    UPROPERTY(Transient, BlueprintReadOnly, Category="WTBR | UI")
    TObjectPtr<UWTBREscudoPresetWheelWidget> EscudoPresetWheelWidgetInstance;

    // Seconds Q/E must be held before the selection wheel opens. Below this the
    // release is treated as a tap and cycles the slot, preserving the existing
    // behaviour. Mirrors UWTBRInputGestureComponent's own HoldThreshold.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | UI",
        meta=(ClampMin="0.05"))
    float TriggerWheelHoldThreshold = 0.20f;

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

    // Serpveil preset the player currently has armed, if any. Composites built
    // from a Serpveil inherit this trajectory instead of their own authored one
    // — that is what makes them read as "Viper, upgraded" rather than a separate
    // weapon. Returns false when no Serpveil is held or nothing is armed, and
    // the caller falls back to the registry definition's own PathPreset.
    bool GetArmedSerpveilPathPreset(FWTBRPathPreset& OutPreset) const;

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

    // ─── Mark/Ping (team-only ground/enemy marker, MMB by convention) ─────────
    // Design: [[wtbr-mark-ping-design-lock]]. Server re-traces from the client's
    // aim (never trusts a client-supplied hit) and only ever writes into
    // IncomingMarkPing on the instigator's own teammates (self included) — so an
    // enemy team never receives a copy of the payload at all, not merely one that
    // a HUD chooses not to draw.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Mark Ping", meta=(ClampMin="500.0"))
    float MarkPingMaxRange = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Mark Ping", meta=(ClampMin="0.1"))
    float MarkPingCooldownSeconds = 1.0f;

    // ⚠ PLAYTEST PENDING: raised 5 -> 9 s alongside the Serpveil range rebase. A ping
    // is meant to survive the whole see-ping -> turn -> charge -> flight loop; at
    // Serpveil's new 70 m reach the flight alone is ~2.0 s, and a ping expiring
    // mid-charge (marker vanishing while aiming at it) feels broken.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WTBR | Mark Ping", meta=(ClampMin="1.0"))
    float MarkPingDisplayDuration = 9.0f;

    UFUNCTION(BlueprintPure, Category="WTBR | Mark Ping")
    const TArray<FWTBRActiveMarkPing>& GetActiveMarkPings() const { return ActiveMarkPings; }

    // Pure, no trace/RPC involved — true once MarkPingCooldownSeconds has elapsed
    // since the last accepted request. Exposed (not just inlined into the RPC) so
    // it can be unit-tested without going through the RPC transport, which has no
    // headless equivalent (see the Automation gotchas note in project memory).
    UFUNCTION(BlueprintPure, Category="WTBR | Mark Ping")
    bool CanRequestMarkPingNow() const;

    // True if HitActor is a living AWTBRCharacter this is not confirmed to be on
    // the same team as (IsSameTeamAs — same assigned TeamId only). A teamless
    // HitActor is NOT given the benefit of the doubt: in a no-team/solo BR match
    // TeamId is INDEX_NONE for everyone, and there everyone is hostile to
    // everyone by definition — "friendly" requires proving a shared team, not
    // just failing to prove hostility. A trace that actually reaches an enemy
    // already proves LOS by construction, so this alone is the enemy-ping/LOS
    // gate — no separate check.
    UFUNCTION(BlueprintPure, Category="WTBR | Mark Ping")
    bool ShouldTreatMarkPingHitAsEnemy(AActor* HitActor) const;

    // This character plus every other AWTBRCharacter on the same assigned team
    // (teamless matches: self only). Deliberately not filtered to player-controlled
    // characters — a bot recipient harmlessly receives IncomingMarkPing and simply
    // has no HUD to draw it with, and the unfiltered version stays trivially
    // testable without a full PlayerController/network-join fixture.
    UFUNCTION(BlueprintPure, Category="WTBR | Mark Ping")
    TArray<AWTBRCharacter*> GetMarkPingRecipients() const;

    // ─── Camera freeze (Escudo preset wheel; see below) ────────────────────────
    // Checked by UWTBRInputGestureComponent::OnLook_Triggered before forwarding
    // mouse delta into AddControllerYaw/PitchInput. Not reused by the Trigger
    // Wheel — that dialog deliberately does NOT freeze the camera (its own doc
    // comment: dual-purposing the same delta so the camera keeps turning while
    // flicking a selection). Escudo's preset wheel needs an actual freeze
    // instead (owner-confirmed design), so this is a separate mechanism.
    UFUNCTION(BlueprintPure, Category = "WTBR | Input")
    bool IsLookInputFrozen() const { return bLookInputFrozen; }

    void SetLookInputFrozen(bool bNewFrozen) { bLookInputFrozen = bNewFrozen; }

    // ─── Escudo Hold/Preset flow ────────────────────────────────────────────────
    // Design: [[wtbr-escudo-hold-preset-design-lock]]. Entirely client-local
    // until TryConfirmEscudoPlacement's RPC — the server never sees a
    // work-in-progress preset choice or ghost preview, only a final confirm.
    // Cancellable for free at any point past the tap threshold via
    // CancelEscudoPresetFlow (wired into the existing Cancel/slot-switch paths).
    UFUNCTION(BlueprintPure, Category = "WTBR | Escudo")
    EWTBREscudoPresetFlowState GetEscudoPresetFlowState() const { return EscudoFlowState; }

    // Safe to call unconditionally (e.g. from Cancel/slot-switch handlers) —
    // a no-op when no Escudo flow is in progress.
    void CancelEscudoPresetFlow();

    // Ghost preview VFX hooks — C++ owns all placement/validity logic (see
    // ComputeEscudoPanelTransforms/TickEscudoGhostPreview), Blueprint owns the
    // actual translucent panel meshes it spawns/updates in response, same
    // split as UWTBRNexilTrigger's OnNexilGhostPreviewUpdated/Hidden (no AI
    // edits to .uasset content). PanelValid.Num() == PanelTransforms.Num().
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Escudo | VFX")
    void OnEscudoGhostPreviewUpdated(const TArray<FTransform>& PanelTransforms, const TArray<bool>& PanelValid);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Escudo | VFX")
    void OnEscudoGhostPreviewHidden();

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

    // Which hand the in-flight Serpveil charge is being conjured from. Main
    // conjures right, Sub conjures left. Replicated alongside the telegraph flag
    // so remote clients attach the cosmetic to the correct socket — UE applies
    // every replicated property in a bunch before running any RepNotify, so this
    // is already current by the time OnRep_SerpveilChargeTelegraph fires.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "WTBR | Trigger | VFX")
    bool bSerpveilChargeFromMainHand = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Trigger | VFX")
    void OnSerpveilChargeTelegraphChanged(bool bActive);

    // Socket the conjure cosmetic should attach to. Blueprint reads this instead
    // of hardcoding a hand — the telegraph event signature is deliberately left
    // unchanged so existing graphs keep compiling.
    UFUNCTION(BlueprintPure, Category = "WTBR | Trigger | VFX")
    FName GetSerpveilChargeHandSocket() const;

    // Single source of truth for the Main/Sub hand mapping, shared by the
    // Serpveil trigger's spawn path and the cosmetic above so they cannot drift.
    UFUNCTION(BlueprintPure, Category = "WTBR | Trigger | VFX")
    static FName ResolveSerpveilHandSocket(bool bIsMainSlot);

    // Server-only setter, called by UWTBRSerpveilTrigger from its existing
    // authority-gated paths. Idempotent — no-ops if the value is unchanged.
    void SetSerpveilChargeTelegraphActive(bool bActive, bool bFromMainHand = true);

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
    // Q/E are press-and-release now: press starts the hold watch, release either
    // commits a wheel selection or falls back to the original tap-to-cycle.
    void SwitchMainTrigger(const FInputActionValue& Value);
    void SwitchSubTrigger(const FInputActionValue& Value);
    void SwitchMainTriggerReleased(const FInputActionValue& Value);
    void SwitchSubTriggerReleased(const FInputActionValue& Value);

    void BeginTriggerSetSelect(bool bIsMain);
    void EndTriggerSetSelect(bool bIsMain);
    void OpenTriggerWheel();
    void OnMainWheelHoldElapsed();
    void OnSubWheelHoldElapsed();

    FTimerHandle TriggerWheelHoldTimer;
    bool bTriggerWheelPendingIsMain = true;
    void DebugConsumeVaelFailTest();
    void Interact(const FInputActionValue& Value);
    // IA_Interact Completed (button release): stops any in-progress revive this
    // client started. No-op for every other interact type (tap-only, nothing to
    // release).
    void InteractReleased(const FInputActionValue& Value);

    // IA_MarkPing Started: reads the local camera's eye trace and forwards it to
    // the server. Never resolves the hit/enemy branch itself — see
    // Server_RequestMarkPing_Implementation, which is the sole authority.
    void Input_RequestMarkPing(const FInputActionValue& Value);

    // MarkPingKey direct-bind path (BindKey has a no-arg handler signature,
    // unlike the Enhanced Input BindAction path above). Same body as
    // Input_RequestMarkPing; see RequestMarkPingFromLocalAim.
    void Input_RequestMarkPingKey();

    // Shared by both binding paths above.
    void RequestMarkPingFromLocalAim();

    // ── Escudo Hold/Preset (client-local; see EWTBREscudoPresetFlowState) ──────
    // Called from the top of FireMain/FireMainReleased/FireSub/FireSubReleased.
    // Return true = fully consumed this press/release, the caller must skip its
    // normal Server_Fire call for it. Return false = not Escudo, or not
    // mid-flow — caller proceeds exactly as it always has. This is the entire
    // surface touched on that shared input path; nothing else in those four
    // functions changes.
    bool HandleEscudoFirePressed(bool bIsMain);
    bool HandleEscudoFireReleased(bool bIsMain);

    // ── Ready-composite Tap/Hold (client-local; runs BEFORE the Escudo/Serpveil
    // pairs, because a paid-for composite outranks every per-Trigger flow) ─────
    //
    // Follows the project's universal input rule rather than inventing one:
    //   Tap  = fire straight immediately at full reach (the reliable panic answer)
    //   Hold = preset wheel -> arm -> press to charge -> release to fire
    // Hold only opens a wheel for composites that actually have a preset family
    // to draw from; the rest simply fire, matching "a Trigger with no hold
    // behavior = no-op".
    bool HandleReadyCompositeFirePressed(bool bIsMain);
    bool HandleReadyCompositeFireReleased(bool bIsMain);

    UFUNCTION()
    void OnReadyCompositeHoldThresholdReached();

    void OpenReadyCompositePresetWheel();
    void ArmReadyCompositePresetAndAwaitCharge();
    void CancelReadyCompositeFlow();

    UFUNCTION(Server, Reliable)
    void Server_FireReadyComposite(int32 PresetIndex, float ChargeFraction);

    // ── Venyx Hold/Preset (client-local) ──────────────────────────────────────
    // Same two-stage contract the player already knows from Viper — hold opens the
    // wheel, release arms, press-and-hold charges, release fires — but deliberately
    // WITHOUT Viper's linked dual-slot charge. That exists so a dual-Viper player
    // can spend both hands on one volley; Hound has no equivalent case, and the
    // machinery is where most of Viper's flow complexity lives.
    bool HandleVenyxFirePressed(bool bIsMain);
    bool HandleVenyxFireReleased(bool bIsMain);

    UFUNCTION()
    void OnVenyxHoldThresholdReached();

    void OpenVenyxPresetWheel();
    void ArmVenyxPresetAndAwaitCharge();
    void CancelVenyxPresetFlow();

    UFUNCTION()
    void OnVenyxPresetLockExpired();

    UWTBRVenyxTrigger* GetActiveVenyxTrigger(bool bIsMain) const;

    UFUNCTION(Server, Reliable)
    void Server_FireVenyxPreset(bool bIsMain, int32 PresetIndex, float ChargeFraction);

    // ── Serpveil Hold/Preset (client-local; same contract as the Escudo pair) ──
    bool HandleSerpveilFirePressed(bool bIsMain);
    bool HandleSerpveilFireReleased(bool bIsMain);

    UFUNCTION()
    void OnSerpveilHoldThresholdReached();

    void OpenSerpveilPresetWheel();

    // Release while the wheel is open: arms the highlighted preset for BOTH
    // slots and waits for a stage-2 press on either. Dead zone = free cancel,
    // same rule as Escudo.
    void ArmSerpveilPresetAndAwaitCharge();

    bool IsAnySerpveilSlotCharging() const;

    // Stage-2 charge duration from whichever Serpveil DataAsset is active,
    // falling back to SERPVEIL_FULL_CHARGE_SECONDS_FALLBACK. Both slots share one
    // value on purpose: a linked dual-Viper charge must reach full charge together.
    float GetSerpveilFullChargeSeconds() const;

    UFUNCTION()
    void TickSerpveilChargePreview();

    // PHASE 1 STOPGAP: DrawDebug range line so the two-stage flow can be judged
    // by feel before any real UI exists. Compiled out in Shipping — the real
    // indicator (world marker + radar path) is deliberately deferred until the
    // flow itself is confirmed worth keeping.
    void DrawSerpveilChargePreview(float ChargeFraction) const;

    // No stage-2 press within this long after locking = the player moved on.
    // Frees the flow rather than leaving a stale preset armed.
    UFUNCTION()
    void OnSerpveilPresetLockExpired();

    void CancelSerpveilPresetFlow();

    UWTBRSerpveilTrigger* GetActiveSerpveilTrigger(bool bIsMain) const;

    UFUNCTION(Server, Reliable)
    void Server_FireSerpveilPreset(bool bIsMain, int32 PresetIndex, float ChargeFraction);

    UFUNCTION()
    void OnEscudoHoldThresholdReached();

    void OpenEscudoPresetWheel();

    // Release while the wheel is open: locks whatever is highlighted (or
    // cancels for free if nothing was — dead zone counts as "changed your
    // mind", not "pick something at random").
    void LockEscudoPresetAndEnterGhostPreview();

    UFUNCTION()
    void TickEscudoGhostPreview();

    // Draws the ghost preview directly from C++ (green = placeable, red =
    // rejected), so the preview is visible without depending on a Blueprint
    // graph existing to respond to OnEscudoGhostPreviewUpdated. Same lesson
    // UWTBRNexilTrigger already learned the hard way — its preview was moved
    // to C++ ownership after repeated BP graph-wiring breakages.
    //
    // ⚠ Uses DrawDebug, which is compiled out in Shipping (ENABLE_DRAW_DEBUG).
    // This is a working stopgap that makes the feature usable and provable
    // NOW, not the final shipping visual — a real translucent-mesh preview
    // (spawned actors, like Nexil's GhostPreviewActorClass) is still needed
    // before this ships, and belongs with the deferred visual-polish pass.
    void DrawEscudoGhostPreview(
        const UWTBREscudoTrigger* EscudoTrigger,
        const TArray<FTransform>& PanelTransforms,
        const TArray<bool>& PanelValid) const;

    // A fresh press (not the same hold) while in GhostPreview state, all
    // panels currently valid. No-op otherwise (can't confirm an invalid or
    // nonexistent preview).
    void TryConfirmEscudoPlacement();

    UWTBREscudoTrigger* GetActiveEscudoTrigger(bool bIsMain) const;

    // Aim-to-place anchor a preset is centered on (GDD §5.8: "Aim-to-Place +
    // Ghost Preview UI ก่อนวาง"). Traces the player's aim out to
    // EscudoPlacementRange and anchors the preset wherever it lands, so a
    // preset can be placed anywhere in range rather than at one fixed distance
    // in front — floors, walls and ceilings all count, and OutSurfaceNormal
    // reports which, so ComputeSurfaceAlignedRotation can sprout the panels
    // perpendicular to it.
    //
    // Aiming past every surface (at open sky) drops the anchor at max range and
    // probes downward, so sweeping across open ground still previews on the
    // floor instead of blinking out. With genuinely nothing under it either,
    // the normal comes back zero and every panel validates as invalid.
    //
    // Unlike Tap's fixed actor-forward offset (unchanged, still used by
    // Activate_Implementation), this is what makes the ghost preview actually
    // follow the crosshair.
    void ComputeEscudoAnchor(
        const UWTBREscudoTrigger* EscudoTrigger, FVector& OutLocation,
        FRotator& OutRotation, FVector& OutSurfaceNormal) const;

    // Computes every panel's placement for PresetIndex anchored at
    // AnchorLocation/Rotation: geometry (lateral/vertical offset from the
    // anchor, rotated to match AnchorRotation) plus, per panel, whether
    // EscudoTrigger->ValidatePanelPlacement currently accepts it — ground-
    // snapped when valid, raw (un-snapped) when not, so the preview never
    // just vanishes. Called identically from the client-local ghost-preview
    // tick (advisory) and the server confirm handler (authoritative) — the
    // sole source of truth for "where does panel N actually end up," so the
    // two can never silently disagree.
    void ComputeEscudoPanelPlacements(
        const UWTBREscudoTrigger* EscudoTrigger, int32 PresetIndex,
        const FVector& AnchorLocation, const FRotator& AnchorRotation,
        TArray<FTransform>& OutPanelTransforms, TArray<bool>& OutPanelValid) const;

    UFUNCTION(Server, Reliable)
    void Server_ConfirmEscudoPreset(bool bIsMain, int32 PresetIndex, FVector AnchorLocation, FRotator AnchorRotation);

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

    // Selection-wheel commit. Unlike Server_CycleTrigger this jumps straight to a
    // chosen slot; the server re-validates the index and the set it belongs to
    // rather than trusting the client's pick.
    UFUNCTION(Server, Reliable)
    void Server_SelectTriggerSlot(bool bIsMain, int32 SlotIndex);

    // Re-traces server-side from the supplied eye location/direction (never
    // trusts a client-reported hit result); silently no-ops if
    // CanRequestMarkPingNow() is false (cooldown, spam-proofing — no error
    // feedback needed, MMB just does nothing until it is ready again).
    UFUNCTION(Server, Reliable)
    void Server_RequestMarkPing(FVector_NetQuantize TraceStart, FVector_NetQuantizeNormal TraceDirection);

    // ─── Replication ─────────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing=OnRep_ActionPing)
    bool bActionPingActive = false;

    // COND_OwnerOnly (see GetLifetimeReplicatedProps): only this character's own
    // owning connection ever receives it, so a Mark/Ping written onto an enemy
    // recipient's character would be a bug at the write site, not something that
    // needs hiding again downstream.
    UPROPERTY(ReplicatedUsing=OnRep_IncomingMarkPing)
    FWTBRMarkPingPayload IncomingMarkPing;

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
    void OnRep_IncomingMarkPing();

    // Shared by OnRep_IncomingMarkPing (remote clients, fired by real
    // replication) and Server_RequestMarkPing_Implementation (called directly,
    // once per recipient, right after writing IncomingMarkPing — OnRep never
    // fires locally for the instance that owns the authoritative value, which on
    // a listen server is the host's own recipient character). Prunes expired
    // entries, then appends one built from the current IncomingMarkPing.
    void ApplyIncomingMarkPing();

    // Server-only; not replicated. Local wall-clock gate, see
    // CanRequestMarkPingNow().
    double LastMarkPingRequestTimeSeconds = -1000.0;

    // Client-local cosmetic cache read by UWTBRMarkPingHUDWidget via
    // GetActiveMarkPings(). Capped at MaxActiveMarkPings (see .cpp) — old
    // entries fall off rather than growing unbounded.
    UPROPERTY(Transient)
    TArray<FWTBRActiveMarkPing> ActiveMarkPings;

    // ── Escudo Hold/Preset state (client-local only, never replicated) ─────────
    bool bLookInputFrozen = false;
    EWTBREscudoPresetFlowState EscudoFlowState = EWTBREscudoPresetFlowState::Idle;
    bool bEscudoFlowIsMainSlot = true;
    int32 EscudoLockedPresetIndex = INDEX_NONE;
    FTimerHandle EscudoHoldThresholdTimer;
    FTimerHandle EscudoGhostPreviewTimer;
    static constexpr float ESCUDO_HOLD_THRESHOLD = 0.2f;
    // 20Hz (0.05s) was visibly steppy against a smoothly-moving 60fps camera
    // while running — owner-found: "ตอนกด preview แล้ววิ่ง ตัว preview จะกระตุกๆ".
    // ~60Hz reads as smooth; a full per-frame Tick() would be smoother still
    // but isn't worth adding permanent tick overhead to AWTBRCharacter for a
    // state that's only active during an Escudo placement flow.
    static constexpr float ESCUDO_GHOST_TICK_INTERVAL = 0.0167f;
    // Slack allowed when the server re-checks a confirm RPC's claimed anchor
    // against EscudoPlacementRange. The client aims from a camera the server
    // only knows about a round-trip late, so an exact comparison would reject
    // honest placements made while turning or running; this is wide enough to
    // absorb that but far too tight to place a preset across the map.
    static constexpr float ESCUDO_SERVER_RANGE_TOLERANCE = 1.25f;

    // ── Serpveil Hold/Preset state (client-local only, never replicated) ──────
    // Slot indexing for the per-slot arrays below: 0 = Main, 1 = Sub.
    static constexpr int32 SERPVEIL_SLOT_MAIN = 0;
    static constexpr int32 SERPVEIL_SLOT_SUB = 1;
    static constexpr int32 SERPVEIL_SLOT_COUNT = 2;
    static int32 SerpveilSlotIndex(bool bIsMain) { return bIsMain ? SERPVEIL_SLOT_MAIN : SERPVEIL_SLOT_SUB; }

    // Ready-composite Tap/Hold flow. Reuses EWTBRSerpveilPresetFlowState rather
    // than declaring a parallel enum — the state machine is identical and the two
    // flows are mutually exclusive (a ready composite suppresses the Serpveil one).
    EWTBRSerpveilPresetFlowState ReadyCompositeFlowState = EWTBRSerpveilPresetFlowState::Idle;
    bool bReadyCompositeFlowIsMainSlot = true;
    int32 ReadyCompositeArmedPresetIndex = INDEX_NONE;
    bool bReadyCompositeCharging = false;
    float ReadyCompositeChargeStartTime = 0.0f;
    FTimerHandle ReadyCompositeHoldThresholdTimer;

    // Server-side only: what the fire RPC asked for, read back by
    // UWTBRCompositeBehaviorBase::ResolveCompositeCubePaths when the behavior
    // builds its cube paths. INDEX_NONE means "tap" — fire straight.
    int32 PendingCompositePresetIndex = INDEX_NONE;
    float PendingCompositeChargeFraction = 1.0f;

public:
    // All three read by the composite behaviors while ExecuteComposite runs.
    int32 GetPendingCompositePresetIndex() const { return PendingCompositePresetIndex; }
    float GetPendingCompositeChargeFraction() const { return PendingCompositeChargeFraction; }

    // Presets the ready composite may choose from, by archetype precedence
    // (Serpveil > Venyx > Fulgrix). Null when the composite has no preset family
    // — Dualux (Solux+Solux) is the only such pair, and stays tap-only.
    const TArray<FWTBRPathPreset>* GetReadyCompositePresets() const;

private:

    // Venyx Hold/Preset. Reuses EWTBRSerpveilPresetFlowState rather than declaring
    // a third identical enum, exactly as the ready-composite flow above does.
    EWTBRSerpveilPresetFlowState VenyxFlowState = EWTBRSerpveilPresetFlowState::Idle;
    bool bVenyxFlowIsMainSlot = true;
    int32 VenyxArmedPresetIndex = INDEX_NONE;
    bool bVenyxCharging = false;
    bool bVenyxChargeIsMainSlot = true;
    float VenyxChargeStartTime = 0.0f;
    bool bVenyxSwallowNextRelease[2] = {false, false};
    FTimerHandle VenyxHoldThresholdTimer;
    FTimerHandle VenyxPresetLockTimer;

    EWTBRSerpveilPresetFlowState SerpveilFlowState = EWTBRSerpveilPresetFlowState::Idle;
    bool bSerpveilFlowIsMainSlot = true;
    // Armed preset is shared by BOTH slots, not owned by the one that picked it.
    int32 SerpveilArmedPresetIndex = INDEX_NONE;
    bool bSerpveilChargingSlot[SERPVEIL_SLOT_COUNT] = {false, false};
    float SerpveilChargeStartTimeSlot[SERPVEIL_SLOT_COUNT] = {0.0f, 0.0f};
    // With Serpveil in both slots, ONE button charges and fires both — holding
    // and releasing LMB+RMB in sync is awkward on a mouse. The button that
    // started the charge is the only one whose release fires it; the other
    // button's press/release is swallowed so it cannot double-fire or leak
    // through to the legacy Server_Fire path.
    bool bSerpveilChargeInitiatorIsMain = true;
    // A press we swallowed must have its matching release swallowed too. By
    // release time the flow may already be back to Idle, so without this the
    // release slips through to the legacy Server_Fire path on its own.
    bool bSerpveilSwallowNextRelease[SERPVEIL_SLOT_COUNT] = {false, false};
    FTimerHandle SerpveilHoldThresholdTimer;
    FTimerHandle SerpveilChargeTickTimer;
    FTimerHandle SerpveilPresetLockTimer;
    // Matches the Escudo wheel's feel deliberately — both dialogs open off the
    // same gesture on the same button, so a different threshold would read as
    // inconsistent rather than as a distinct mechanic.
    static constexpr float SERPVEIL_HOLD_THRESHOLD = 0.2f;
    static constexpr float SERPVEIL_CHARGE_TICK_INTERVAL = 0.0167f;
    // Seconds of charge to reach maximum range in stage 2. Only a fallback for
    // when no Serpveil DataAsset is reachable — the real value is DA-driven via
    // FWTBRSerpveilParams::SerpveilChargeTime, read by GetSerpveilFullChargeSeconds().
    static constexpr float SERPVEIL_FULL_CHARGE_SECONDS_FALLBACK = 0.8f;
    // Grace period after locking a preset before the flow releases itself.
    static constexpr float SERPVEIL_PRESET_LOCK_TIMEOUT = 3.0f;

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
