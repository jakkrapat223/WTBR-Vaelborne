// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRStaminaComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Components/WTBRInputGestureComponent.h"
#include "Components/WTBRInteractionComponent.h"
#include "Components/WTBRHUDViewModelComponent.h"
#include "Components/WTBRBagLootViewModelComponent.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Inventory/WTBRInventoryComponent.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "WTBRGameMode.h"
#include "WTBRGameState.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRAcervynTrigger.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "Trigger/WTBRArcvenTrigger.h"
#include "Trigger/WTBRFeryxTrigger.h"
#include "Trigger/WTBRFulgrisTrigger.h"
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Trigger/WTBRLacernTrigger.h"
#include "Trigger/WTBRMantornTrigger.h"
#include "Trigger/WTBRNexilTrigger.h"
#include "Trigger/WTBRPiercexTrigger.h"
#include "Trigger/WTBRSerpveilTrigger.h"
#include "Trigger/WTBRSniperTrigger.h"
#include "Trigger/WTBRSolvarnTrigger.h"
#include "Trigger/WTBRSoluxTrigger.h"
#include "Trigger/WTBRTelornTrigger.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Trigger/WTBRVexornTrigger.h"
#include "UI/WTBRRadarWidget.h"
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "UI/WTBRInputBindingDisplayLibrary.h"

#if !UE_BUILD_SHIPPING
// B7D client-side validation harness trigger. Detected every ~1 s by
// PollB7ClientValidationCVar on NM_Client characters after BeginPlay; the
// validation sequence runs this many seconds after the pawn is locally
// controlled. Default 0 (disabled). Not compiled in Shipping builds.
static TAutoConsoleVariable<float> CVarWTBRB7ClientValidationDelay(
    TEXT("WTBR.B7ClientValidationDelaySeconds"),
    0.0f,
    TEXT("[Dev] Set > 0 via -ExecCmds on a client to run the B7 client validation "
         "sequence (container receipt log, focus/priority check, loot RPC, post-state "
         "log) that many seconds after the local pawn is possessed. Client only. "
         "Default 0 (disabled). Not compiled in Shipping builds."));
#endif

namespace
{
    constexpr float WTBRDroppedTriggerPickupRange = 300.0f;

    // Minimum dot(view-forward, dir-to-candidate) for a dropped trigger to count as
    // "aimed at". AWTBRDroppedTriggerActor has no collision primitive, so detection
    // uses actor iteration + this view-cone gate instead of a trace/sweep. ~0.5 is a
    // forgiving ~60-degree half-cone that still works for low ground loot.
    constexpr float WTBRDroppedTriggerAimDotThreshold = 0.5f;

    // BR Ground Item pickup range. Mirrors the dropped-trigger range style;
    // final balance is tunable and may move to a DataAsset later.
    constexpr float WTBRGroundItemPickupRange = 300.0f;

    bool TryParseWTBRDebugMatchPhase(const FString& PhaseName, EWTBRMatchPhase& OutPhase)
    {
        const FString NormalizedPhase = PhaseName.TrimStartAndEnd().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

        if (NormalizedPhase.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::None;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("Lobby"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::Lobby;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("PreMatch"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::PreMatch;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("LoadoutSetup"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::LoadoutSetup;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("Countdown"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::Countdown;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("InMatch"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::InMatch;
            return true;
        }
        if (NormalizedPhase.Equals(TEXT("PostMatch"), ESearchCase::IgnoreCase))
        {
            OutPhase = EWTBRMatchPhase::PostMatch;
            return true;
        }

        return false;
    }

    FText GetHUDTriggerName(const UWTBRTriggerBase* Trigger)
    {
        const FText NoTriggerText = NSLOCTEXT("WTBRCharacter", "HUDNoTriggerFallback", "No Trigger");
        if (!IsValid(Trigger))
        {
            return NoTriggerText;
        }

        const FText FunctionalName = Trigger->GetFunctionalName();
        return FunctionalName.IsEmpty() ? NoTriggerText : FunctionalName;
    }

    FText GetHUDTriggerDataAssetName(const UWTBRTriggerDataAsset* DataAsset)
    {
        const FText NoTriggerText = NSLOCTEXT("WTBRCharacter", "HUDNoTriggerFallback", "No Trigger");
        if (!IsValid(DataAsset))
        {
            return NoTriggerText;
        }

        return DataAsset->FunctionalName.IsEmpty()
            ? NoTriggerText
            : DataAsset->FunctionalName;
    }

    FText GetHUDTriggerNameWithFallback(const UWTBRTriggerBase* Trigger, const UWTBRTriggerDataAsset* DataAsset)
    {
        if (IsValid(Trigger))
        {
            return GetHUDTriggerName(Trigger);
        }

        return GetHUDTriggerDataAssetName(DataAsset);
    }

    FText FormatHUDTriggerHintText(
        const FText& SlotLabel,
        const FText& TriggerName,
        const UInputMappingContext* MappingContext,
        const UInputAction* InputAction)
    {
        const FWTBRHUDBindingDisplay BindingDisplay =
            UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(MappingContext, InputAction);
        if (BindingDisplay.bIsBound && !BindingDisplay.DisplayName.IsEmpty())
        {
            return FText::Format(
                NSLOCTEXT("WTBRCharacter", "HUDTriggerHintWithBinding", "{0} [{1}] {2}"),
                SlotLabel,
                BindingDisplay.DisplayName,
                TriggerName);
        }

        return FText::Format(
            NSLOCTEXT("WTBRCharacter", "HUDTriggerHintWithoutBinding", "{0} {1}"),
            SlotLabel,
            TriggerName);
    }

    FText FormatHUDActionHintText(
        const FText& ActionLabel,
        const UInputMappingContext* MappingContext,
        const UInputAction* InputAction)
    {
        const FWTBRHUDBindingDisplay BindingDisplay =
            UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(MappingContext, InputAction);
        if (BindingDisplay.bIsBound && !BindingDisplay.DisplayName.IsEmpty())
        {
            return FText::Format(
                NSLOCTEXT("WTBRCharacter", "HUDActionHintWithBinding", "[{0}] {1}"),
                BindingDisplay.DisplayName,
                ActionLabel);
        }

        return ActionLabel;
    }

    bool IsKnownPerUseVaelTriggerForHUD(const UWTBRTriggerBase* Trigger)
    {
        return IsValid(Trigger)
            && (Trigger->IsA<UWTBRArcvenTrigger>()
                || Trigger->IsA<UWTBRAcervynTrigger>()
                || Trigger->IsA<UWTBRFulgrisTrigger>()
                || Trigger->IsA<UWTBRFulgrixTrigger>()
                || Trigger->IsA<UWTBRPiercexTrigger>()
                || Trigger->IsA<UWTBRSoluxTrigger>()
                || Trigger->IsA<UWTBRTelornTrigger>()
                || Trigger->IsA<UWTBRVenyxTrigger>());
    }

    bool IsKnownZeroVaelTriggerForHUD(const UWTBRTriggerBase* Trigger)
    {
        return IsValid(Trigger)
            && (Trigger->IsA<UWTBRFeryxTrigger>()
                || Trigger->IsA<UWTBRLacernTrigger>()
                || Trigger->IsA<UWTBRMantornTrigger>()
                || Trigger->IsA<UWTBRNexilTrigger>()
                || Trigger->IsA<UWTBRVexornTrigger>()
                || Trigger->IsA<UWTBRVoltisLaunchTrigger>());
    }
}

AWTBRCharacter::AWTBRCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    GetCharacterMovement()->bOrientRotationToMovement   = true;
    GetCharacterMovement()->RotationRate                = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->JumpZVelocity               = 700.f;
    GetCharacterMovement()->AirControl                  = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed                = 600.f;
    GetCharacterMovement()->MinAnalogWalkSpeed          = 20.f;
    GetCharacterMovement()->BrakingDecelerationWalking  = 2000.f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength         = 400.f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    HealthComponent       = CreateDefaultSubobject<UWTBRHealthComponent>(TEXT("HealthComponent"));
    StaminaComponent      = CreateDefaultSubobject<UWTBRStaminaComponent>(TEXT("StaminaComponent"));
    VaelComponent         = CreateDefaultSubobject<UWTBRVaelComponent>(TEXT("VaelComponent"));
    MovementExtComponent  = CreateDefaultSubobject<UWTBRMovementExtComponent>(TEXT("MovementExtComponent"));
    InputGestureComponent = CreateDefaultSubobject<UWTBRInputGestureComponent>(TEXT("InputGestureComponent"));
    InteractionComponent  = CreateDefaultSubobject<UWTBRInteractionComponent>(TEXT("InteractionComponent"));
    TriggerSetComponent   = CreateDefaultSubobject<UWTBRTriggerSetComponent>(TEXT("TriggerSetComponent"));
    InventoryComponent    = CreateDefaultSubobject<UWTBRInventoryComponent>(TEXT("InventoryComponent"));
    HUDViewModelComponent = CreateDefaultSubobject<UWTBRHUDViewModelComponent>(TEXT("HUDViewModelComponent"));
    BagLootViewModelComponent = CreateDefaultSubobject<UWTBRBagLootViewModelComponent>(TEXT("BagLootViewModelComponent"));

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextAsset(
        TEXT("/Game/Input/IMC_WTBR_Default.IMC_WTBR_Default"));
    if (DefaultMappingContextAsset.Succeeded())
    {
        DefaultMappingContext = DefaultMappingContextAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(
        TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
    if (MoveActionAsset.Succeeded())
    {
        MoveAction = MoveActionAsset.Object;
        InputGestureComponent->IA_Move = MoveActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(
        TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
    if (LookActionAsset.Succeeded())
    {
        LookAction = LookActionAsset.Object;
        InputGestureComponent->IA_Look = LookActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(
        TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
    if (JumpActionAsset.Succeeded())
    {
        JumpAction = JumpActionAsset.Object;
        InputGestureComponent->IA_Jump = JumpActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> SwitchMainActionAsset(
        TEXT("/Game/Input/Actions/IA_SwitchMain.IA_SwitchMain"));
    if (SwitchMainActionAsset.Succeeded())
    {
        SwitchMainAction = SwitchMainActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> SwitchSubActionAsset(
        TEXT("/Game/Input/Actions/IA_SwitchSub.IA_SwitchSub"));
    if (SwitchSubActionAsset.Succeeded())
    {
        SwitchSubAction = SwitchSubActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> FireMainActionAsset(
        TEXT("/Game/Input/Actions/IA_MainTrigger.IA_MainTrigger"));
    if (FireMainActionAsset.Succeeded())
    {
        FireMainAction = FireMainActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> FireSubActionAsset(
        TEXT("/Game/Input/Actions/IA_SubTrigger.IA_SubTrigger"));
    if (FireSubActionAsset.Succeeded())
    {
        FireSubAction = FireSubActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> DodgeActionAsset(
        TEXT("/Game/Input/Actions/IA_Dodge.IA_Dodge"));
    if (DodgeActionAsset.Succeeded())
    {
        DodgeAction = DodgeActionAsset.Object;
    }
}

void AWTBRCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (InputGestureComponent)
    {
        InputGestureComponent->IA_Move = MoveAction;
        InputGestureComponent->IA_Look = LookAction;
        InputGestureComponent->IA_Jump = JumpAction;
        // SwitchMain/Sub bind โดยตรงที่ Character ไม่ต้องส่งไป Component
    }
}

void AWTBRCharacter::BeginPlay()
{
    Super::BeginPlay();

    AddDefaultMappingContext();

    if (HealthComponent)
    {
        HealthComponent->OnLimbDestroyed.AddDynamic(this, &AWTBRCharacter::OnLimbDestroyedHandler);
    }
    if (StaminaComponent)
    {
        StaminaComponent->OnStaminaChanged.AddDynamic(this, &AWTBRCharacter::OnStaminaChangedHandler);
    }
    if (TriggerSetComponent)
    {
        TriggerSetComponent->OnDualWieldStateChanged.AddDynamic(
            this, &AWTBRCharacter::OnDualWieldStateChangedHandler);
        TriggerSetComponent->OnActiveTriggerChanged.AddDynamic(
            this, &AWTBRCharacter::OnActiveTriggerChangedHandler);
    }
    if (InteractionComponent)
    {
        InteractionComponent->OnCorpseLootInteractRequested.AddDynamic(
            this, &AWTBRCharacter::OnCorpseLootInteractRequestedHandler);
    }

    RefreshHUDHints();
    CreateLocalPlayerUI();

#if !UE_BUILD_SHIPPING
    // B7D: ExecCmds processes at tick [1]; client possession happens later. A 1-s
    // repeating poll catches WTBR.B7ClientValidationDelaySeconds being set and waits
    // until this character is the locally controlled client pawn before scheduling.
    if (GetNetMode() == NM_Client)
    {
        GetWorldTimerManager().SetTimer(
            B7ClientValidationPollTimerHandle,
            FTimerDelegate::CreateUObject(this, &AWTBRCharacter::PollB7ClientValidationCVar),
            1.0f,
            /*bLoop=*/true);
    }
#endif
}

void AWTBRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyLocalPlayerUI();

    Super::EndPlay(EndPlayReason);
}

void AWTBRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        AddDefaultMappingContext();
        ApplyInputActionFallbacks();

        // Move / Look / Jump are handled by WTBRInputGestureComponent (IA_Move/Look/Jump props)
        if (InputGestureComponent)
        {
            InputGestureComponent->BindInputActions(EIC);
        }

        EIC->BindAction(FireMainAction,      ETriggerEvent::Started,   this, &AWTBRCharacter::FireMain);
        EIC->BindAction(FireMainAction,      ETriggerEvent::Completed, this, &AWTBRCharacter::FireMainReleased);
        EIC->BindAction(FireSubAction,       ETriggerEvent::Started,   this, &AWTBRCharacter::FireSub);
        EIC->BindAction(FireSubAction,       ETriggerEvent::Completed, this, &AWTBRCharacter::FireSubReleased);
        EIC->BindAction(DodgeAction,         ETriggerEvent::Triggered, this, &AWTBRCharacter::Dodge);
        if (IsValid(SwitchTriggerAction))
        {
            EIC->BindAction(SwitchTriggerAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchTrigger);
        }
        if (IsValid(SwitchMainAction))
        {
            EIC->BindAction(SwitchMainAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchMainTrigger);
        }
        if (IsValid(SwitchSubAction))
        {
            EIC->BindAction(SwitchSubAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchSubTrigger);
        }
        if (IsValid(InteractAction))
        {
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AWTBRCharacter::Interact);
            EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &AWTBRCharacter::InteractReleased);
        }
        if (IsValid(CancelAction))
        {
            EIC->BindAction(CancelAction, ETriggerEvent::Started, this, &AWTBRCharacter::HandleCancelInput);
        }
        if (IsValid(CompositeMergeAction))
        {
            EIC->BindAction(CompositeMergeAction, ETriggerEvent::Started, this, &AWTBRCharacter::HandleCompositeMergeInput);
        }
        if (IsValid(BagAction))
        {
            EIC->BindAction(BagAction, ETriggerEvent::Started, this, &AWTBRCharacter::ToggleBagLootLayer);
        }

    }
}

// ─── Input ───────────────────────────────────────────────────────────────────

void AWTBRCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Dir = Value.Get<FVector2D>();
    if (!Controller) return;
    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Dir.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Dir.X);
}

void AWTBRCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void AWTBRCharacter::FireMain(const FInputActionValue& Value)
{
    const FVector ClientMoveInputDir = GetClientMoveInputDirectionForTrigger();
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=true | Pressed=true | ClientMoveInputDir=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        *ClientMoveInputDir.ToString());
    if (InputGestureComponent) InputGestureComponent->NotifyMainPressed();
    UpdateSniperZoom(true, true);
    Server_Fire(true, true, ClientMoveInputDir);
}

void AWTBRCharacter::FireMainReleased(const FInputActionValue& Value)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=true | Pressed=false"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifyMainReleased();
    UpdateSniperZoom(true, false);
    Server_Fire(true, false, FVector::ZeroVector);
}

void AWTBRCharacter::FireSub(const FInputActionValue& Value)
{
    const FVector ClientMoveInputDir = GetClientMoveInputDirectionForTrigger();
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=false | Pressed=true | ClientMoveInputDir=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        *ClientMoveInputDir.ToString());
    if (InputGestureComponent) InputGestureComponent->NotifySubPressed();
    UpdateSniperZoom(false, true);
    Server_Fire(false, true, ClientMoveInputDir);
}

void AWTBRCharacter::FireSubReleased(const FInputActionValue& Value)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=false | Pressed=false"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifySubReleased();
    UpdateSniperZoom(false, false);
    Server_Fire(false, false, FVector::ZeroVector);
}

void AWTBRCharacter::UpdateSniperZoom(bool bIsMain, bool bZoomIn)
{
    if (!IsLocallyControlled() || !FollowCamera || !TriggerSetComponent) return;

    if (bIsMain) bMainWantsSniperZoom = bZoomIn;
    else         bSubWantsSniperZoom  = bZoomIn;

    // Only a real Sniper in the slot actually counts as "wants zoom" —
    // grab both up front so the rest of this function can just read them.
    UWTBRSniperTrigger* MainSniper = bMainWantsSniperZoom
        ? Cast<UWTBRSniperTrigger>(TriggerSetComponent->GetActiveMainTrigger()) : nullptr;
    UWTBRSniperTrigger* SubSniper = bSubWantsSniperZoom
        ? Cast<UWTBRSniperTrigger>(TriggerSetComponent->GetActiveSubTrigger()) : nullptr;

    const bool bWasZoomed = SniperZoomLerpTimer.IsValid()
        || !FMath::IsNearlyEqual(FollowCamera->FieldOfView, DefaultCameraFOV, 0.1f);
    if (!bWasZoomed && (MainSniper || SubSniper))
    {
        DefaultCameraFOV = FollowCamera->FieldOfView;
    }

    // Main takes priority if both slots are held Snipers at once.
    SniperZoomTargetFOV = MainSniper ? MainSniper->GetZoomFOV()
        : SubSniper ? SubSniper->GetZoomFOV()
        : DefaultCameraFOV;

    GetWorldTimerManager().SetTimer(
        SniperZoomLerpTimer,
        this, &AWTBRCharacter::TickSniperZoomLerp,
        SNIPER_ZOOM_TICK_INTERVAL, true);
}

void AWTBRCharacter::TickSniperZoomLerp()
{
    if (!FollowCamera)
    {
        GetWorldTimerManager().ClearTimer(SniperZoomLerpTimer);
        return;
    }

    const float Current = FollowCamera->FieldOfView;
    const float NewFOV = FMath::FInterpTo(
        Current, SniperZoomTargetFOV, SNIPER_ZOOM_TICK_INTERVAL, SNIPER_ZOOM_LERP_SPEED);
    FollowCamera->SetFieldOfView(NewFOV);

    if (FMath::IsNearlyEqual(NewFOV, SniperZoomTargetFOV, 0.1f))
    {
        FollowCamera->SetFieldOfView(SniperZoomTargetFOV);
        GetWorldTimerManager().ClearTimer(SniperZoomLerpTimer);
    }
}

void AWTBRCharacter::Interact(const FInputActionValue& /*Value*/)
{
    if (InteractionComponent)
    {
        InteractionComponent->RequestContextInteract();
    }
}

void AWTBRCharacter::InteractReleased(const FInputActionValue& /*Value*/)
{
    if (InteractionComponent)
    {
        InteractionComponent->RequestStopReviveIfInProgress();
    }
}

void AWTBRCharacter::HandleCancelInput()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (IsAnyLocalUIPanelOpen())
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[UI] HandleCancelInput: local UI panel open — closing it instead of cancelling trigger action | Owner=%s"),
            *GetNameSafe(this));
        CloseAnyOpenLocalUI();
        return;
    }

    // No UI panel open — preserve existing trigger-charge cancel behavior.
    CancelCurrentAction();
}

void AWTBRCharacter::HandleCompositeMergeInput()
{
    if (TriggerSetComponent)
    {
        TriggerSetComponent->Server_StartCompositeMerge();
    }
}

void AWTBRCharacter::Dodge(const FInputActionValue& Value)
{
    if (StaminaComponent) StaminaComponent->TryConsumeDodgeStamina();
    Server_Dodge();
}

void AWTBRCharacter::CancelCurrentAction()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] InputReceived | Owner=%s | Auth=%s | Local=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));

    Server_CancelCurrentAction();
}

void AWTBRCharacter::SwitchTrigger(const FInputActionValue& Value)
{
    SwitchMainTrigger(Value);
}

void AWTBRCharacter::SwitchMainTrigger(const FInputActionValue& Value)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("WTBR SwitchMain input pressed"));
    Server_CycleTrigger(true);
}

void AWTBRCharacter::SwitchSubTrigger(const FInputActionValue& Value)
{
    Server_CycleTrigger(false);
}

void AWTBRCharacter::DebugConsumeVaelFailTest()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Debug ConsumeFail Input] Owner=%s | Auth=%s | Local=%s | Cost=999.0"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));

    Server_DebugConsumeVaelFailTest();
}

void AWTBRCharacter::WTBRDebugCharacterPrintMatchState() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintMatchState is disabled in Shipping builds."));
#else
    const UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintMatchState rejected: World is missing."));
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintMatchState rejected: WTBRGameState is missing."));
        return;
    }

    const FWTBRMatchModeRules Rules = WTBRGameState->GetCurrentMatchRules();
    UE_LOG(LogTemp, Log,
        TEXT("WTBRDebugCharacterPrintMatchState: Character=%s Mode=%s Phase=%s bEnablePassiveVaelRegen=%s VaelRegenPerSecond=%.2f bAllowTriggerSwapDuringMatch=%s"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchMode()),
        *UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()),
        Rules.bEnablePassiveVaelRegen ? TEXT("true") : TEXT("false"),
        Rules.VaelRegenPerSecond,
        Rules.bAllowTriggerSwapDuringMatch ? TEXT("true") : TEXT("false"));
#endif
}

void AWTBRCharacter::WTBRDebugCharacterPrintTriggerLoadoutGate() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintTriggerLoadoutGate is disabled in Shipping builds."));
#else
    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintTriggerLoadoutGate rejected: TriggerSetComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPrintTriggerLoadoutGate: checking character %s component %s."),
        *GetNameSafe(this),
        *GetNameSafe(TriggerSetComponent));
    TriggerSetComponent->DebugPrintTriggerLoadoutMutationGate();
#endif
}

void AWTBRCharacter::WTBRDebugCharacterSetMatchPhase(const FString& PhaseName)
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSetMatchPhase is disabled in Shipping builds."));
#else
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSetMatchPhase rejected: character does not have authority. Use standalone/listen-server/server authority for validation."));
        return;
    }

    EWTBRMatchPhase ParsedPhase = EWTBRMatchPhase::None;
    if (!TryParseWTBRDebugMatchPhase(PhaseName, ParsedPhase))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSetMatchPhase rejected: unknown phase '%s'. Valid phases: None, Lobby, PreMatch, LoadoutSetup, Countdown, InMatch, PostMatch."), *PhaseName);
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSetMatchPhase rejected: World is missing."));
        return;
    }

    AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSetMatchPhase rejected: WTBRGameState is missing."));
        return;
    }

    WTBRGameState->SetCurrentMatchPhase(ParsedPhase);
    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterSetMatchPhase: phase set to %s by character %s."),
        *UEnum::GetValueAsString(ParsedPhase),
        *GetNameSafe(this));
#endif
}

// ─── Server RPC Implementations ──────────────────────────────────────────────

void AWTBRCharacter::WTBRDebugCharacterPickupNearestDroppedTrigger(int32 TargetSlotIndex)
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPickupNearestDroppedTrigger is disabled in Shipping builds."));
#else
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPickupNearestDroppedTrigger rejected: World is missing."));
        return;
    }

    AWTBRDroppedTriggerActor* NearestDroppedTrigger = nullptr;
    float NearestDistanceSq = FMath::Square(WTBRDroppedTriggerPickupRange);

    for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
    {
        AWTBRDroppedTriggerActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsConsumed() || Candidate->GetDroppedTriggerDataAsset().IsNull())
        {
            continue;
        }

        const float CandidateDistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (CandidateDistanceSq <= NearestDistanceSq)
        {
            NearestDroppedTrigger = Candidate;
            NearestDistanceSq = CandidateDistanceSq;
        }
    }

    if (!IsValid(NearestDroppedTrigger))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPickupNearestDroppedTrigger rejected: no dropped trigger within %.0f units for character %s."),
            WTBRDroppedTriggerPickupRange,
            *GetNameSafe(this));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPickupNearestDroppedTrigger: requesting pickup of %s into slot %d for character %s."),
        *GetNameSafe(NearestDroppedTrigger),
        TargetSlotIndex,
        *GetNameSafe(this));
    Server_RequestPickupDroppedTrigger(NearestDroppedTrigger, TargetSlotIndex);
#endif
}

void AWTBRCharacter::WTBRDebugCharacterSpawnCorpseLootContainer()
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer is disabled in Shipping builds."));
#else
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected: character %s does not have authority."),
            *GetNameSafe(this));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected: World is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)
        || !WTBRGameState->IsInMatch()
        || !WTBRGameState->AllowsCorpseLoot()
        || !WTBRGameState->AllowsTriggerPickup())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected by match phase/rules for character %s."),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected: TriggerSetComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> InstalledTriggers;
    TriggerSetComponent->GetInstalledTriggerSlotSnapshots(InstalledTriggers);
    if (InstalledTriggers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected: character %s has no installed triggers."),
            *GetNameSafe(this));
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    const FVector SpawnLocation = GetActorLocation() + (GetActorForwardVector() * 120.0f) + FVector(0.0f, 0.0f, 20.0f);
    TSubclassOf<AWTBRCorpseLootContainerActor> SpawnClass = AWTBRCorpseLootContainerActor::StaticClass();
    if (const AWTBRGameMode* WTBRGameMode = World->GetAuthGameMode<AWTBRGameMode>())
    {
        SpawnClass = WTBRGameMode->GetCorpseLootContainerClass();
    }

    AWTBRCorpseLootContainerActor* LootContainer = World->SpawnActor<AWTBRCorpseLootContainerActor>(
        SpawnClass,
        SpawnLocation,
        FRotator::ZeroRotator,
        SpawnParams);

    if (!IsValid(LootContainer))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer rejected: failed to spawn container for character %s."),
            *GetNameSafe(this));
        return;
    }

    LootContainer->InitializeCorpseLootContainer(InstalledTriggers);
    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterSpawnCorpseLootContainer: spawned %s for character %s with %d entries."),
        *GetNameSafe(LootContainer),
        *GetNameSafe(this),
        LootContainer->GetLootEntryCount());
#endif
}

void AWTBRCharacter::WTBRDebugCharacterPrintFocusedInteractionPrompt() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt is disabled in Shipping builds."));
#else
    if (!IsValid(InteractionComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt rejected: InteractionComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    const AWTBRCorpseLootContainerActor* FocusedContainer = InteractionComponent->GetFocusedCorpseLootContainer();
    const FText PromptText = InteractionComponent->GetFocusedInteractionPromptText();
    const FString PromptString = PromptText.ToString();

    if (IsValid(FocusedContainer))
    {
        UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt: focused corpse loot container found: %s."),
            *GetNameSafe(FocusedContainer));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt: no focused corpse loot container found for character %s."),
            *GetNameSafe(this));
    }

    if (!PromptText.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt: prompt='%s'."),
            *PromptString);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterPrintFocusedInteractionPrompt: prompt is empty; container is missing, not eligible, or has no available prompt."));
    }
#endif
}

void AWTBRCharacter::WTBRDebugCharacterLootFocusedCorpseContainer(int32 LootEntryIndex, int32 TargetSlotIndex)
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootFocusedCorpseContainer is disabled in Shipping builds."));
#else
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootFocusedCorpseContainer rejected: World is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(InteractionComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootFocusedCorpseContainer rejected: InteractionComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    AWTBRCorpseLootContainerActor* FocusedContainer = InteractionComponent->GetFocusedCorpseLootContainer();
    if (!IsValid(FocusedContainer))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootFocusedCorpseContainer rejected: no focused corpse loot container for character %s."),
            *GetNameSafe(this));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterLootFocusedCorpseContainer: requesting container %s entry %d into slot %d for character %s."),
        *GetNameSafe(FocusedContainer),
        LootEntryIndex,
        TargetSlotIndex,
        *GetNameSafe(this));
    Server_RequestPickupCorpseLootEntry(FocusedContainer, LootEntryIndex, TargetSlotIndex);
#endif
}

void AWTBRCharacter::WTBRDebugCharacterLootNearestCorpseContainer(int32 LootEntryIndex, int32 TargetSlotIndex)
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootNearestCorpseContainer is disabled in Shipping builds."));
#else
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootNearestCorpseContainer rejected: World is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    AWTBRCorpseLootContainerActor* NearestContainer = nullptr;
    float NearestDistanceSq = FMath::Square(WTBRDroppedTriggerPickupRange);

    for (TActorIterator<AWTBRCorpseLootContainerActor> It(World); It; ++It)
    {
        AWTBRCorpseLootContainerActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->GetLootEntryCount() == 0 || Candidate->AreAllEntriesConsumed())
        {
            continue;
        }

        const float CandidateDistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (CandidateDistanceSq <= NearestDistanceSq)
        {
            NearestContainer = Candidate;
            NearestDistanceSq = CandidateDistanceSq;
        }
    }

    if (!IsValid(NearestContainer))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterLootNearestCorpseContainer rejected: no corpse loot container within %.0f units for character %s."),
            WTBRDroppedTriggerPickupRange,
            *GetNameSafe(this));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBRDebugCharacterLootNearestCorpseContainer: requesting container %s entry %d into slot %d for character %s."),
        *GetNameSafe(NearestContainer),
        LootEntryIndex,
        TargetSlotIndex,
        *GetNameSafe(this));
    Server_RequestPickupCorpseLootEntry(NearestContainer, LootEntryIndex, TargetSlotIndex);
#endif
}

AWTBRDroppedTriggerActor* AWTBRCharacter::FindAimedDroppedTriggerForPickup() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR aimed dropped trigger pickup rejected: World is missing for character %s."),
            *GetNameSafe(this));
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (Controller)
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ViewForward = ViewRotation.Vector();

    // AWTBRDroppedTriggerActor is spawned with only a USceneComponent root and has no
    // collision primitive, so line traces / channel sweeps cannot detect it. Iterate the
    // dropped-trigger actors directly and pick the nearest not-yet-consumed candidate that
    // is within pickup range and in front of the view (aimed). Mirrors the collision-
    // independent lookup used by the nearest-pickup debug command. Detection only — the
    // server still re-validates distance on pickup.
    AWTBRDroppedTriggerActor* BestDroppedTrigger = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    const float MaxRangeSq = FMath::Square(WTBRDroppedTriggerPickupRange);

    for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
    {
        AWTBRDroppedTriggerActor* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsConsumed() || Candidate->GetDroppedTriggerDataAsset().IsNull())
        {
            continue;
        }

        const float CandidateDistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (CandidateDistanceSq > MaxRangeSq)
        {
            continue;
        }

        const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
        if (FVector::DotProduct(ToCandidate.GetSafeNormal(), ViewForward) < WTBRDroppedTriggerAimDotThreshold)
        {
            continue;
        }

        if (CandidateDistanceSq < BestDistanceSq)
        {
            BestDroppedTrigger = Candidate;
            BestDistanceSq = CandidateDistanceSq;
        }
    }

    if (!IsValid(BestDroppedTrigger))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR aimed dropped trigger pickup rejected: no aimed dropped trigger within %.0f units for character %s."),
            WTBRDroppedTriggerPickupRange,
            *GetNameSafe(this));
        return nullptr;
    }

    return BestDroppedTrigger;
}

AWTBRCharacter* AWTBRCharacter::FindBestHomingTarget(
    AWTBRCharacter* QueryingCharacter,
    float SearchRadius,
    float AimConeHalfAngleDegrees)
{
    if (!IsValid(QueryingCharacter) || SearchRadius <= 0.0f)
    {
        return nullptr;
    }

    UWorld* World = QueryingCharacter->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FVector EyeLocation = FVector::ZeroVector;
    FRotator AimRotation = FRotator::ZeroRotator;
    QueryingCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);

    const FVector AimDirection = AimRotation.Vector().GetSafeNormal();
    if (AimDirection.IsNearlyZero())
    {
        return nullptr;
    }

    const float SearchRadiusSq = FMath::Square(SearchRadius);
    const float AimConeDotThreshold = FMath::Cos(FMath::DegreesToRadians(
        FMath::Clamp(AimConeHalfAngleDegrees, 0.0f, 180.0f)));

    AWTBRCharacter* BestTarget = nullptr;
    float BestAimDot = -1.0f;
    float BestDistanceSq = TNumericLimits<float>::Max();

    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        AWTBRCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == QueryingCharacter ||
            !IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsAlive() ||
            QueryingCharacter->IsSameTeamAs(Candidate))
        {
            continue;
        }

        const float CandidateDistanceSq = FVector::DistSquared(
            QueryingCharacter->GetActorLocation(), Candidate->GetActorLocation());
        if (CandidateDistanceSq > SearchRadiusSq)
        {
            continue;
        }

        const FVector ToCandidate = Candidate->GetActorLocation() - EyeLocation;
        const FVector CandidateDirection = ToCandidate.GetSafeNormal();
        if (CandidateDirection.IsNearlyZero())
        {
            continue;
        }

        const float AimDot = FVector::DotProduct(AimDirection, CandidateDirection);
        if (AimDot < AimConeDotThreshold)
        {
            continue;
        }

        FHitResult VisibilityHit;
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WTBRHomingTargetLOS), false);
        TraceParams.AddIgnoredActor(QueryingCharacter);
        TraceParams.AddIgnoredActor(Candidate);
        if (World->LineTraceSingleByChannel(
                VisibilityHit, EyeLocation, Candidate->GetActorLocation(), ECC_Visibility, TraceParams))
        {
            continue;
        }

        const bool bBetterAim = AimDot > BestAimDot + KINDA_SMALL_NUMBER;
        const bool bEqualAimCloser = FMath::IsNearlyEqual(AimDot, BestAimDot) &&
            CandidateDistanceSq < BestDistanceSq - KINDA_SMALL_NUMBER;
        const bool bCompleteTieWithEarlierName = FMath::IsNearlyEqual(AimDot, BestAimDot) &&
            FMath::IsNearlyEqual(CandidateDistanceSq, BestDistanceSq) &&
            (!BestTarget || Candidate->GetName() < BestTarget->GetName());
        if (bBetterAim || bEqualAimCloser || bCompleteTieWithEarlierName)
        {
            BestTarget = Candidate;
            BestAimDot = AimDot;
            BestDistanceSq = CandidateDistanceSq;
        }
    }

    return BestTarget;
}

void AWTBRCharacter::FindBestHomingTargets(
    AWTBRCharacter* QueryingCharacter,
    float SearchRadius,
    float AimConeHalfAngleDegrees,
    int32 MaxTargets,
    TArray<AWTBRCharacter*>& OutTargets)
{
    OutTargets.Reset();
    if (!IsValid(QueryingCharacter) || SearchRadius <= 0.0f || MaxTargets <= 0)
    {
        return;
    }

    UWorld* World = QueryingCharacter->GetWorld();
    if (!World)
    {
        return;
    }

    FVector EyeLocation = FVector::ZeroVector;
    FRotator AimRotation = FRotator::ZeroRotator;
    QueryingCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);

    const FVector AimDirection = AimRotation.Vector().GetSafeNormal();
    if (AimDirection.IsNearlyZero())
    {
        return;
    }

    const float SearchRadiusSq = FMath::Square(SearchRadius);
    const float AimConeDotThreshold = FMath::Cos(FMath::DegreesToRadians(
        FMath::Clamp(AimConeHalfAngleDegrees, 0.0f, 180.0f)));

    struct FWTBRHomingTargetCandidate
    {
        AWTBRCharacter* Character = nullptr;
        float AimDot = -1.0f;
        float DistanceSq = TNumericLimits<float>::Max();
    };
    TArray<FWTBRHomingTargetCandidate> Candidates;

    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        AWTBRCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == QueryingCharacter ||
            !IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsAlive() ||
            QueryingCharacter->IsSameTeamAs(Candidate))
        {
            continue;
        }

        const float CandidateDistanceSq = FVector::DistSquared(
            QueryingCharacter->GetActorLocation(), Candidate->GetActorLocation());
        if (CandidateDistanceSq > SearchRadiusSq)
        {
            continue;
        }

        const FVector ToCandidate = Candidate->GetActorLocation() - EyeLocation;
        const FVector CandidateDirection = ToCandidate.GetSafeNormal();
        if (CandidateDirection.IsNearlyZero())
        {
            continue;
        }

        const float AimDot = FVector::DotProduct(AimDirection, CandidateDirection);
        if (AimDot < AimConeDotThreshold)
        {
            continue;
        }

        FHitResult VisibilityHit;
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WTBRHomingTargetLOS), false);
        TraceParams.AddIgnoredActor(QueryingCharacter);
        TraceParams.AddIgnoredActor(Candidate);
        if (World->LineTraceSingleByChannel(
                VisibilityHit, EyeLocation, Candidate->GetActorLocation(), ECC_Visibility, TraceParams))
        {
            continue;
        }

        Candidates.Add({Candidate, AimDot, CandidateDistanceSq});
    }

    Candidates.Sort([](const FWTBRHomingTargetCandidate& A, const FWTBRHomingTargetCandidate& B)
    {
        if (!FMath::IsNearlyEqual(A.AimDot, B.AimDot))
        {
            return A.AimDot > B.AimDot;
        }
        if (!FMath::IsNearlyEqual(A.DistanceSq, B.DistanceSq))
        {
            return A.DistanceSq < B.DistanceSq;
        }
        return A.Character->GetName() < B.Character->GetName();
    });

    const int32 TargetCount = FMath::Min(FMath::Max(0, MaxTargets), Candidates.Num());
    OutTargets.Reserve(TargetCount);
    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        OutTargets.Add(Candidates[Index].Character);
    }
}

void AWTBRCharacter::RequestPickupAimedDroppedTriggerIntoActiveMainSlot()
{
    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR aimed dropped trigger pickup rejected: TriggerSetComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    AWTBRDroppedTriggerActor* AimedDroppedTrigger = FindAimedDroppedTriggerForPickup();
    if (!IsValid(AimedDroppedTrigger))
    {
        return;
    }

    const int32 TargetSlotIndex = TriggerSetComponent->GetActiveMainIndex();
    UE_LOG(LogTemp, Log, TEXT("WTBR aimed dropped trigger pickup: requesting pickup of %s into active main slot %d for character %s."),
        *GetNameSafe(AimedDroppedTrigger),
        TargetSlotIndex,
        *GetNameSafe(this));
    Server_RequestPickupDroppedTrigger(AimedDroppedTrigger, TargetSlotIndex);
}

void AWTBRCharacter::RequestPickupAimedDroppedTriggerIntoActiveSubSlot()
{
    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR aimed dropped trigger pickup rejected: TriggerSetComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    AWTBRDroppedTriggerActor* AimedDroppedTrigger = FindAimedDroppedTriggerForPickup();
    if (!IsValid(AimedDroppedTrigger))
    {
        return;
    }

    const int32 TargetSlotIndex = TriggerSetComponent->GetActiveSubIndex();
    UE_LOG(LogTemp, Log, TEXT("WTBR aimed dropped trigger pickup: requesting pickup of %s into active sub slot %d for character %s."),
        *GetNameSafe(AimedDroppedTrigger),
        TargetSlotIndex,
        *GetNameSafe(this));
    Server_RequestPickupDroppedTrigger(AimedDroppedTrigger, TargetSlotIndex);
}

void AWTBRCharacter::RequestPickupAimedDroppedTriggerByConstraint()
{
    // Client-side resolver. Dispatches only; the server re-validates authority,
    // match state, distance, and slot compatibility before any mutation.
    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR constraint dropped trigger pickup rejected: TriggerSetComponent is missing for character %s."),
            *GetNameSafe(this));
        return;
    }

    AWTBRDroppedTriggerActor* AimedDroppedTrigger = FindAimedDroppedTriggerForPickup();
    if (!IsValid(AimedDroppedTrigger))
    {
        // FindAimedDroppedTriggerForPickup already logged the no-target reason.
        return;
    }

    const TSoftObjectPtr<UWTBRTriggerDataAsset> DroppedDataAsset = AimedDroppedTrigger->GetDroppedTriggerDataAsset();
    if (DroppedDataAsset.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR constraint dropped trigger pickup rejected: %s has no DataAsset for character %s."),
            *GetNameSafe(AimedDroppedTrigger),
            *GetNameSafe(this));
        return;
    }

    // Read-only load to inspect the slot constraint. No mutation happens here;
    // the server independently reloads and re-validates the asset on dispatch.
    const UWTBRTriggerDataAsset* LoadedDataAsset = DroppedDataAsset.LoadSynchronous();
    if (!IsValid(LoadedDataAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR constraint dropped trigger pickup rejected: failed to load DataAsset for %s (character %s)."),
            *GetNameSafe(AimedDroppedTrigger),
            *GetNameSafe(this));
        return;
    }

    // S7A Policy v1 — constraint-driven single-valid-target resolution.
    int32 TargetSlotIndex = INDEX_NONE;
    switch (LoadedDataAsset->SlotConstraint)
    {
        case ETriggerSlotConstraint::MainOnly:
            TargetSlotIndex = TriggerSetComponent->GetActiveMainIndex();
            break;

        case ETriggerSlotConstraint::SubOnly:
            TargetSlotIndex = TriggerSetComponent->GetActiveSubIndex();
            break;

        case ETriggerSlotConstraint::Any:
            // Two valid active targets (active main and active sub). Per policy we
            // must not blindly choose; defer to a future slot-selection UI.
            UE_LOG(LogTemp, Warning, TEXT("WTBR constraint dropped trigger pickup: AmbiguousTargetSlot for %s (SlotConstraint=Any) — not dispatching for character %s."),
                *GetNameSafe(AimedDroppedTrigger),
                *GetNameSafe(this));
            return;

        default:
            UE_LOG(LogTemp, Warning, TEXT("WTBR constraint dropped trigger pickup rejected: unhandled SlotConstraint for %s (character %s)."),
                *GetNameSafe(AimedDroppedTrigger),
                *GetNameSafe(this));
            return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR constraint dropped trigger pickup: requesting pickup of %s into active slot %d (SlotConstraint resolved) for character %s."),
        *GetNameSafe(AimedDroppedTrigger),
        TargetSlotIndex,
        *GetNameSafe(this));

#if WITH_DEV_AUTOMATION_TESTS
    if (bCaptureDroppedTriggerRouteForTest)
    {
        bDroppedTriggerRouteCapturedForTest = true;
        CapturedDroppedTriggerForTest = AimedDroppedTrigger;
        CapturedDroppedTriggerSlotForTest = TargetSlotIndex;
        CapturedDroppedTriggerConstraintForTest = LoadedDataAsset->SlotConstraint;
        UE_LOG(LogTemp, Log, TEXT("WTBR constraint dropped trigger pickup captured for automation: trigger=%s slot=%d constraint=%d character=%s."),
            *GetNameSafe(AimedDroppedTrigger),
            TargetSlotIndex,
            static_cast<int32>(LoadedDataAsset->SlotConstraint),
            *GetNameSafe(this));
        return;
    }
#endif

    Server_RequestPickupDroppedTrigger(AimedDroppedTrigger, TargetSlotIndex);
}

#if WITH_DEV_AUTOMATION_TESTS
void AWTBRCharacter::SetDroppedTriggerRouteCaptureForTest(bool bEnable)
{
    bCaptureDroppedTriggerRouteForTest = bEnable;
    ClearDroppedTriggerRouteCaptureForTest();
}

void AWTBRCharacter::ClearDroppedTriggerRouteCaptureForTest()
{
    bDroppedTriggerRouteCapturedForTest = false;
    CapturedDroppedTriggerForTest.Reset();
    CapturedDroppedTriggerSlotForTest = INDEX_NONE;
    CapturedDroppedTriggerConstraintForTest = ETriggerSlotConstraint::Any;
}
#endif

void AWTBRCharacter::WTBRDebugCharacterPickupAimedDroppedTriggerActiveMain()
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPickupAimedDroppedTriggerActiveMain is disabled in Shipping builds."));
#else
    RequestPickupAimedDroppedTriggerIntoActiveMainSlot();
#endif
}

void AWTBRCharacter::WTBRDebugCharacterPickupAimedDroppedTriggerActiveSub()
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPickupAimedDroppedTriggerActiveSub is disabled in Shipping builds."));
#else
    RequestPickupAimedDroppedTriggerIntoActiveSubSlot();
#endif
}

void AWTBRCharacter::WTBRDebugCharacterPrintTriggerSlots() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterPrintTriggerSlots is disabled in Shipping builds."));
#else
    UE_LOG(LogTemp, Log, TEXT("[WTBR TriggerSlots] Character=%s HasAuthority=%d LocalRole=%d RemoteRole=%d"),
        *GetNameSafe(this),
        HasAuthority() ? 1 : 0,
        (int32)GetLocalRole(),
        (int32)GetRemoteRole());

    const UWorld* World = GetWorld();
    const AWTBRGameState* WTBRGameState = World ? World->GetGameState<AWTBRGameState>() : nullptr;
    UE_LOG(LogTemp, Log, TEXT("[WTBR TriggerSlots] GameStateValid=%d IsInMatch=%d AllowsCorpseLoot=%d AllowsTriggerPickup=%d"),
        IsValid(WTBRGameState) ? 1 : 0,
        (IsValid(WTBRGameState) && WTBRGameState->IsInMatch()) ? 1 : 0,
        (IsValid(WTBRGameState) && WTBRGameState->AllowsCorpseLoot()) ? 1 : 0,
        (IsValid(WTBRGameState) && WTBRGameState->AllowsTriggerPickup()) ? 1 : 0);

    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("[WTBR TriggerSlots] TriggerSetComponent is missing for %s."),
            *GetNameSafe(this));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[WTBR TriggerSlots] ActiveMainIndex=%d ActiveSubIndex=%d"),
        TriggerSetComponent->GetActiveMainIndex(),
        TriggerSetComponent->GetActiveSubIndex());

    TArray<FWTBRInstalledTriggerSlotSnapshot> InstalledTriggers;
    TriggerSetComponent->GetInstalledTriggerSlotSnapshots(InstalledTriggers);

    if (InstalledTriggers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WTBR TriggerSlots] 0 installed trigger snapshots; server TriggerSlots may be empty (no loadout on server) for %s."),
            *GetNameSafe(this));
        return;
    }

    for (const FWTBRInstalledTriggerSlotSnapshot& Snapshot : InstalledTriggers)
    {
        const UWTBRTriggerDataAsset* LoadedDataAsset = Snapshot.DataAsset.IsNull()
            ? nullptr
            : Snapshot.DataAsset.LoadSynchronous();
        const FString SlotConstraintText = IsValid(LoadedDataAsset)
            ? UEnum::GetValueAsString(LoadedDataAsset->SlotConstraint)
            : FString(TEXT("<unloaded>"));

        UE_LOG(LogTemp, Log, TEXT("[WTBR TriggerSlots] SlotIndex=%d DataAsset=%s CachedCategory=%s SlotConstraint=%s"),
            Snapshot.SlotIndex,
            *Snapshot.DataAsset.ToSoftObjectPath().ToString(),
            *UEnum::GetValueAsString(Snapshot.CachedCategory),
            *SlotConstraintText);
    }
#endif
}

void AWTBRCharacter::WTBRDebugCharacterListNearbyDroppedTriggers() const
{
#if UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Warning, TEXT("WTBRDebugCharacterListNearbyDroppedTriggers is disabled in Shipping builds."));
#else
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WTBR DroppedList] World is missing for %s."), *GetNameSafe(this));
        return;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    if (Controller)
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector CharacterLocation = GetActorLocation();
    int32 Count = 0;
    for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
    {
        const AWTBRDroppedTriggerActor* DroppedTrigger = *It;
        if (!IsValid(DroppedTrigger))
        {
            continue;
        }
        ++Count;

        const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(DroppedTrigger->GetRootComponent());
        const FString CollisionText = RootPrimitive
            ? FString::Printf(TEXT("CollisionEnabled=%d Profile=%s"),
                (int32)RootPrimitive->GetCollisionEnabled(),
                *RootPrimitive->GetCollisionProfileName().ToString())
            : FString(TEXT("root has no UPrimitiveComponent (no collision)"));

        UE_LOG(LogTemp, Log, TEXT("[WTBR DroppedList] %s Loc=%s DistFromChar=%.1f DistFromCam=%.1f DataAsset=%s IsConsumed=%d %s"),
            *GetNameSafe(DroppedTrigger),
            *DroppedTrigger->GetActorLocation().ToCompactString(),
            FVector::Dist(CharacterLocation, DroppedTrigger->GetActorLocation()),
            FVector::Dist(ViewLocation, DroppedTrigger->GetActorLocation()),
            *DroppedTrigger->GetDroppedTriggerDataAsset().ToSoftObjectPath().ToString(),
            DroppedTrigger->IsConsumed() ? 1 : 0,
            *CollisionText);
    }

    UE_LOG(LogTemp, Log, TEXT("[WTBR DroppedList] Total AWTBRDroppedTriggerActor in world = %d (character=%s)."),
        Count,
        *GetNameSafe(this));
#endif
}

void AWTBRCharacter::AddDefaultMappingContext()
{
    if (!IsValid(DefaultMappingContext) || !IsLocallyControlled())
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!IsValid(PC))
    {
        return;
    }

    ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
    if (!LocalPlayer)
    {
        return;
    }

    if (UEnhancedInputLocalPlayerSubsystem* Sub =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
    {
        if (!Sub->HasMappingContext(DefaultMappingContext))
        {
            Sub->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void AWTBRCharacter::ApplyInputActionFallbacks()
{
    if (!InputGestureComponent)
    {
        return;
    }

    if (!IsValid(InputGestureComponent->IA_Move))
    {
        InputGestureComponent->IA_Move = MoveAction;
    }
    if (!IsValid(InputGestureComponent->IA_Look))
    {
        InputGestureComponent->IA_Look = LookAction;
    }
    if (!IsValid(InputGestureComponent->IA_Jump))
    {
        InputGestureComponent->IA_Jump = JumpAction;
    }
}

FVector AWTBRCharacter::GetClientMoveInputDirectionForTrigger() const
{
    if (!IsValid(InputGestureComponent))
    {
        return FVector::ZeroVector;
    }

    FVector2D MoveAxis = InputGestureComponent->GetLastMoveAxis2D();
    if (MoveAxis.SizeSquared() > 1.0f)
    {
        MoveAxis = MoveAxis.GetSafeNormal();
    }
    if (MoveAxis.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }

    const FRotator ControlRot = Controller ? Controller->GetControlRotation() : GetActorRotation();
    const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);
    const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
    return ((ForwardDir * MoveAxis.Y) + (RightDir * MoveAxis.X)).GetSafeNormal2D();
}

FVector AWTBRCharacter::SanitizeClientMoveInputDirection(FVector ClientMoveInputDir)
{
    ClientMoveInputDir.Z = 0.0f;
    if (ClientMoveInputDir.ContainsNaN() || ClientMoveInputDir.SizeSquared2D() <= KINDA_SMALL_NUMBER)
    {
        return FVector::ZeroVector;
    }

    ClientMoveInputDir = ClientMoveInputDir.GetClampedToMaxSize2D(1.0f);
    return ClientMoveInputDir.GetSafeNormal2D();
}

void AWTBRCharacter::Server_RequestPickupDroppedTrigger_Implementation(AWTBRDroppedTriggerActor* DroppedTrigger, int32 TargetSlotIndex)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: character %s is not alive"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: TriggerSetComponent is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(DroppedTrigger) || DroppedTrigger->IsConsumed())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: dropped actor is invalid or already consumed for character %s"),
            *GetNameSafe(this));
        return;
    }

    const TSoftObjectPtr<UWTBRTriggerDataAsset> DroppedDataAsset = DroppedTrigger->GetDroppedTriggerDataAsset();
    if (DroppedDataAsset.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: dropped actor %s has no DataAsset"),
            *GetNameSafe(DroppedTrigger));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: World is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)
        || !WTBRGameState->IsInMatch()
        || !WTBRGameState->AllowsTriggerPickup()
        || !WTBRGameState->AllowsTriggerSwapDuringMatch())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected by match phase/rules for character %s"),
            *GetNameSafe(this));
        return;
    }

    const float DistanceSq = FVector::DistSquared(GetActorLocation(), DroppedTrigger->GetActorLocation());
    const float MaxDistanceSq = FMath::Square(WTBRDroppedTriggerPickupRange);
    if (DistanceSq > MaxDistanceSq)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: character %s is too far from %s (Distance=%.1f Max=%.1f)"),
            *GetNameSafe(this),
            *GetNameSafe(DroppedTrigger),
            FMath::Sqrt(DistanceSq),
            WTBRDroppedTriggerPickupRange);
        return;
    }

    if (!DroppedTrigger->TryMarkConsumed())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: dropped actor %s was already consumed"),
            *GetNameSafe(DroppedTrigger));
        return;
    }

    if (!TriggerSetComponent->ReplaceTriggerSlotFromDataAsset(TargetSlotIndex, DroppedDataAsset))
    {
        DroppedTrigger->ClearConsumedForFailedPickup();
        UE_LOG(LogTemp, Warning, TEXT("WTBR dropped trigger pickup rejected: replacement failed for character %s slot %d"),
            *GetNameSafe(this),
            TargetSlotIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR dropped trigger pickup succeeded: character %s consumed %s into slot %d"),
        *GetNameSafe(this),
        *GetNameSafe(DroppedTrigger),
        TargetSlotIndex);
    DroppedTrigger->Destroy();
}

void AWTBRCharacter::Server_RequestPickupCorpseLootEntry_Implementation(
    AWTBRCorpseLootContainerActor* LootContainer,
    int32 LootEntryIndex,
    int32 TargetSlotIndex)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: character %s is not alive"),
            *GetNameSafe(this));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: World is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)
        || !WTBRGameState->IsInMatch()
        || !WTBRGameState->AllowsCorpseLoot()
        || !WTBRGameState->AllowsTriggerPickup()
        || !WTBRGameState->AllowsTriggerSwapDuringMatch())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected by match phase/rules for character %s"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(TriggerSetComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: TriggerSetComponent is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(LootContainer))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: invalid container for character %s"),
            *GetNameSafe(this));
        return;
    }

    if (LootContainer->GetLootEntryCount() == 0 || LootContainer->AreAllEntriesConsumed())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: container %s is empty or fully consumed"),
            *GetNameSafe(LootContainer));
        return;
    }

    if (LootEntryIndex < 0 || LootEntryIndex >= LootContainer->GetLootEntryCount())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: invalid entry index %d for container %s"),
            LootEntryIndex,
            *GetNameSafe(LootContainer));
        return;
    }

    if (LootContainer->IsEntryConsumed(LootEntryIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: entry %d in container %s is already consumed"),
            LootEntryIndex,
            *GetNameSafe(LootContainer));
        return;
    }

    const TSoftObjectPtr<UWTBRTriggerDataAsset> EntryDataAsset = LootContainer->GetEntryTriggerDataAsset(LootEntryIndex);
    if (EntryDataAsset.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: entry %d in container %s has invalid DataAsset"),
            LootEntryIndex,
            *GetNameSafe(LootContainer));
        return;
    }

    const float DistanceSq = FVector::DistSquared(GetActorLocation(), LootContainer->GetActorLocation());
    const float MaxDistanceSq = FMath::Square(WTBRDroppedTriggerPickupRange);
    if (DistanceSq > MaxDistanceSq)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: character %s is too far from %s (Distance=%.1f Max=%.1f)"),
            *GetNameSafe(this),
            *GetNameSafe(LootContainer),
            FMath::Sqrt(DistanceSq),
            WTBRDroppedTriggerPickupRange);
        return;
    }

    FWTBRInstalledTriggerSlotSnapshot ReplacedTargetSlotSnapshot;
    const bool bTargetSlotHadTrigger = TriggerSetComponent->GetTriggerSlotSnapshot(
        TargetSlotIndex,
        ReplacedTargetSlotSnapshot);

    if (!LootContainer->TryMarkEntryConsumed(LootEntryIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: failed to consume entry %d in container %s"),
            LootEntryIndex,
            *GetNameSafe(LootContainer));
        return;
    }

    if (!TriggerSetComponent->ReplaceTriggerSlotFromDataAsset(TargetSlotIndex, EntryDataAsset))
    {
        LootContainer->ClearEntryConsumedForFailedPickup(LootEntryIndex);
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot pickup rejected: replacement failed for character %s entry %d slot %d"),
            *GetNameSafe(this),
            LootEntryIndex,
            TargetSlotIndex);
        return;
    }

    if (bTargetSlotHadTrigger)
    {
        if (!LootContainer->ReplaceEntryWithSnapshot(LootEntryIndex, ReplacedTargetSlotSnapshot))
        {
            // Roll the whole transaction back — without this the player's previous
            // trigger exists nowhere (not in the slot, not in the container) and is
            // permanently lost.
            const bool bSlotRestored = TriggerSetComponent->ReplaceTriggerSlotFromDataAsset(
                TargetSlotIndex, ReplacedTargetSlotSnapshot.DataAsset);
            LootContainer->ClearEntryConsumedForFailedPickup(LootEntryIndex);

            UE_LOG(LogTemp, Error, TEXT("WTBR corpse loot pickup swap failed after replacement, rolled back: character %s container %s entry %d slot %d SlotRestored=%s"),
                *GetNameSafe(this),
                *GetNameSafe(LootContainer),
                LootEntryIndex,
                TargetSlotIndex,
                bSlotRestored ? TEXT("true") : TEXT("false"));
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot pickup swapped: character %s took container %s entry %d into slot %d and returned previous slot trigger to same entry"),
            *GetNameSafe(this),
            *GetNameSafe(LootContainer),
            LootEntryIndex,
            TargetSlotIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot pickup succeeded: character %s consumed container %s entry %d into empty slot %d"),
        *GetNameSafe(this),
        *GetNameSafe(LootContainer),
        LootEntryIndex,
        TargetSlotIndex);

    if (LootContainer->AreAllEntriesConsumed())
    {
        UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot container empty/destroyed: %s"),
            *GetNameSafe(LootContainer));
        LootContainer->Destroy();
    }
}

void AWTBRCharacter::Server_RequestPickupGroundItem_Implementation(AWTBRGroundItemActor* GroundItem)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: character %s is not alive"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(InventoryComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: InventoryComponent is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(GroundItem) || GroundItem->IsConsumed())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: ground item is invalid or already consumed for character %s"),
            *GetNameSafe(this));
        return;
    }

    const TSoftObjectPtr<UWTBRItemDataAsset> ItemDataRef = GroundItem->GetItemData();
    if (ItemDataRef.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: ground item %s has no ItemData"),
            *GetNameSafe(GroundItem));
        return;
    }

    const int32 Quantity = GroundItem->GetQuantity();
    if (Quantity <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: ground item %s has non-positive quantity %d"),
            *GetNameSafe(GroundItem),
            Quantity);
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: World is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    // Match gate (I-2 MVP): ground item pickup is gated by IsInMatch() only and
    // must NOT reuse AllowsTriggerPickup / AllowsTriggerSwapDuringMatch.
    // TODO(S5+): replace with dedicated AllowsGroundItemPickup / AllowsInventoryItemUse
    // rules on UWTBRMatchModeRulesDataAsset once those fields exist.
    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState) || !WTBRGameState->IsInMatch())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected by match phase for character %s"),
            *GetNameSafe(this));
        return;
    }

    const float DistanceSq = FVector::DistSquared(GetActorLocation(), GroundItem->GetActorLocation());
    const float MaxDistanceSq = FMath::Square(WTBRGroundItemPickupRange);
    if (DistanceSq > MaxDistanceSq)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: character %s is too far from %s (Distance=%.1f Max=%.1f)"),
            *GetNameSafe(this),
            *GetNameSafe(GroundItem),
            FMath::Sqrt(DistanceSq),
            WTBRGroundItemPickupRange);
        return;
    }

    // Atomic claim before mutating inventory so a failed add can roll back cleanly.
    if (!GroundItem->TryMarkConsumed())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: ground item %s was already consumed"),
            *GetNameSafe(GroundItem));
        return;
    }

    const UWTBRItemDataAsset* ItemDataObj = ItemDataRef.LoadSynchronous();
    if (!IsValid(ItemDataObj))
    {
        GroundItem->ClearConsumedForFailedPickup();
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: ground item %s ItemData failed to load"),
            *GetNameSafe(GroundItem));
        return;
    }

    // All-or-nothing add (TryAddItem is transactional from S5-B). Do not stack here.
    if (!InventoryComponent->TryAddItem(ItemDataObj, Quantity))
    {
        GroundItem->ClearConsumedForFailedPickup();
        UE_LOG(LogTemp, Warning, TEXT("WTBR ground item pickup rejected: inventory full for character %s (item %s x%d)"),
            *GetNameSafe(this),
            *GetNameSafe(GroundItem),
            Quantity);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR ground item pickup succeeded: character %s picked up %s x%d"),
        *GetNameSafe(this),
        *GetNameSafe(GroundItem),
        Quantity);
    GroundItem->Destroy();
}

void AWTBRCharacter::Server_RequestStartRevive_Implementation(AWTBRCharacter* Target)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR revive start rejected: reviver %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(Target) || !IsValid(Target->HealthComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR revive start rejected: invalid target for reviver %s"),
            *GetNameSafe(this));
        return;
    }

    // All remaining validation (Downed state, same-team living reviver, one
    // reviver at a time) lives in UWTBRHealthComponent::TryStartRevive — this RPC
    // is a thin authority-gated dispatch, not a second copy of the rules.
    Target->HealthComponent->TryStartRevive(this);
}

void AWTBRCharacter::Server_RequestStopRevive_Implementation(AWTBRCharacter* Target)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR revive stop rejected: reviver %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(Target) || !IsValid(Target->HealthComponent))
    {
        return;
    }

    // Only stop if THIS actor is actually the active reviver — guards against a
    // stale/rejected start's later release RPC cancelling a different
    // teammate's legitimate in-progress revive on the same target.
    if (Target->HealthComponent->GetActiveReviver() == this)
    {
        Target->HealthComponent->StopRevive();
    }
}

void AWTBRCharacter::Server_RequestUseInventoryItem_Implementation(int32 SlotIndex)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: character %s is not alive"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(InventoryComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: InventoryComponent is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    const TArray<FWTBRInventorySlot>& Slots = InventoryComponent->GetInventorySlots();
    if (!Slots.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: invalid slot %d for character %s"),
            SlotIndex,
            *GetNameSafe(this));
        return;
    }

    const FWTBRInventorySlot& Slot = Slots[SlotIndex];
    if (Slot.IsEmpty() || Slot.ItemData.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: slot %d is empty for character %s"),
            SlotIndex,
            *GetNameSafe(this));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: World is missing for character %s"),
            *GetNameSafe(this));
        return;
    }

    // Match gate (I-2 MVP): inventory item use is gated by IsInMatch() only and must
    // NOT reuse trigger pickup/swap gates.
    // TODO(S5+): replace with a dedicated AllowsInventoryItemUse rule on
    // UWTBRMatchModeRulesDataAsset once that field exists.
    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState) || !WTBRGameState->IsInMatch())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected by match phase for character %s"),
            *GetNameSafe(this));
        return;
    }

    const UWTBRItemDataAsset* ItemData = Slot.ItemData.LoadSynchronous();
    if (!IsValid(ItemData))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: slot %d ItemData failed to load for character %s"),
            SlotIndex,
            *GetNameSafe(this));
        return;
    }

    // Apply the MVP effect. The item is consumed only if the effect was actually
    // applied (e.g. HealHP fails at full HP; RestoreVael fails when Vael is full).
    bool bEffectApplied = false;
    switch (ItemData->ConsumableEffectType)
    {
    case EWTBRConsumableEffectType::HealHP:
        bEffectApplied = HealthComponent->RestoreHP(ItemData->EffectMagnitude);
        break;

    case EWTBRConsumableEffectType::RestoreVael:
        bEffectApplied = IsValid(VaelComponent) && VaelComponent->GrantVael(ItemData->EffectMagnitude);
        break;

    default:
        // None, VaelOvercharge (parked), and any future/unsupported effect: reject
        // without consuming.
        UE_LOG(LogTemp, Warning, TEXT("WTBR inventory item use rejected: unsupported effect type %d in slot %d for character %s"),
            static_cast<int32>(ItemData->ConsumableEffectType),
            SlotIndex,
            *GetNameSafe(this));
        return;
    }

    if (!bEffectApplied)
    {
        UE_LOG(LogTemp, Log, TEXT("WTBR inventory item use no-op: effect not applied for slot %d (character %s); item not consumed"),
            SlotIndex,
            *GetNameSafe(this));
        return;
    }

    InventoryComponent->ConsumeItemAtSlot(SlotIndex, 1);
    UE_LOG(LogTemp, Log, TEXT("WTBR inventory item use succeeded: character %s used slot %d"),
        *GetNameSafe(this),
        SlotIndex);
}

void AWTBRCharacter::Server_Fire_Implementation(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir)
{
    const FVector SafeClientMoveInputDir = SanitizeClientMoveInputDirection(ClientMoveInputDir);
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=%s | Pressed=%s | ClientMoveInputDir=%s | SafeClientMoveInputDir=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsMain ? TEXT("true") : TEXT("false"),
        bIsPressed ? TEXT("true") : TEXT("false"),
        *ClientMoveInputDir.ToString(),
        *SafeClientMoveInputDir.ToString());
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] TriggerRPC | Owner=%s | Auth=%s | Local=%s | Main=%s | Pressed=%s | ServerRPCReceived=true | ClientMoveInputDir=%s | SafeClientMoveInputDir=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        bIsMain ? TEXT("true") : TEXT("false"),
        bIsPressed ? TEXT("true") : TEXT("false"),
        *ClientMoveInputDir.ToString(),
        *SafeClientMoveInputDir.ToString());

    ExecuteServerTriggerInput(bIsMain, bIsPressed, SafeClientMoveInputDir);
}

void AWTBRCharacter::ExecuteBotTriggerInput(bool bIsMain, bool bIsPressed)
{
    if (!HasAuthority()) return;
    ExecuteServerTriggerInput(bIsMain, bIsPressed, FVector::ZeroVector);
}

void AWTBRCharacter::ExecuteServerTriggerInput(bool bIsMain, bool bIsPressed, FVector ClientMoveInputDir)
{
    if (!HasAuthority()) return;

    if (!TriggerSetComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("TriggerSetComponent is NULL"));
        return;
    }

    UWTBRTriggerBase* Trigger = bIsMain
        ? TriggerSetComponent->GetActiveMainTrigger()
        : TriggerSetComponent->GetActiveSubTrigger();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ActiveTrigger | Owner=%s | Auth=%s | Local=%s | Slot=%s | Pressed=%s | Trigger=%s | TriggerClass=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        bIsMain ? TEXT("Main") : TEXT("Sub"),
        bIsPressed ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Trigger),
        IsValid(Trigger) ? *GetNameSafe(Trigger->GetClass()) : TEXT("None"));

    // Mantorn (Feryx+Feryx) form intercepts LMB/RMB before the normal slot path:
    // Tap = forward whip, Hold = AOE spin. A simultaneous two-side hold is the
    // enter/exit toggle and is handled separately via Server_RequestMantornToggle.
    if (TriggerSetComponent->IsMantornFormActive())
    {
        HandleMantornFormInput(bIsMain, bIsPressed);
        return;
    }
    if (bIsPressed && TriggerSetComponent->HasReadyComposite())
    {
        TriggerSetComponent->FireReadyComposite();
        return;
    }
    if (bIsPressed && TriggerSetComponent->GetCurrentMergeState() != EWTBRCompositeBulletType::None)
    {
        TriggerSetComponent->CancelMerge();
        return;
    }
    // Not in form — clear any leftover in-form attack state (e.g. the buttons that
    // were still held when the form was exited) so it can't go stale into next entry.
    bMantornAttackHeld = false;

    if (!Trigger) return;

    const bool bIsDualWield =
        TriggerSetComponent->GetDualWieldState() != EWTBRDualWieldState::None;
    const FInputActionValue TriggerInputValue(ClientMoveInputDir);

    // A Mantorn-capable Feryx pair (both active Main+Sub) must NEVER resolve on
    // PRESS — Feryx's normal Activate() swings instantly on press, which would
    // fire a normal hit on the very press that's supposed to start the
    // both-hold-to-transform gesture, before the hold has any chance to land.
    // Defer to RELEASE instead: if the hold became the transform mid-press
    // (Server_RequestMantornToggle, sent independently by the gesture component),
    // IsMantornFormActive() will already be true by the time release arrives and
    // this trigger's swing never fires. If the hold never completed (quick tap,
    // or only one side was ever pressed), fire the deferred tap on release.
    const bool bMantornToggleCandidate = TriggerSetComponent->CanToggleMantornForm();
    if (bMantornToggleCandidate)
    {
        if (bIsPressed)
        {
            // Record the individual press too: if the player releases without
            // completing the two-button gesture, Feryx still needs to resolve
            // that button as either its normal tap or its blade-star hold.
            Trigger->OnTriggerActivated(this, bIsMain);
            Trigger->Activate(TriggerInputValue, bIsDualWield);
            return;
        }
        if (UWTBRFeryxTrigger* FeryxTrigger = Cast<UWTBRFeryxTrigger>(Trigger))
        {
            FeryxTrigger->OnTriggerDeactivated(this, bIsMain);
        }
        else
        {
            Trigger->OnTriggerActivated(this, bIsMain);
            const bool bActivated = Trigger->Activate(TriggerInputValue, bIsDualWield);
            WTBR_VALIDATION_LOG(Verbose, TEXT("Trigger Activate result (deferred Mantorn-candidate tap): %d"), bActivated);
        }
        return;
    }

    // Trigger Option attached to this slot (canon Senkū: Arcven attached to a
    // Lacern slot). Same instant-on-press problem as Mantorn — Lacern's normal
    // swing fires on press, so it must defer to release to give a hold-charge
    // a chance to land instead. Per-side (Main/Sub independent), unlike Mantorn.
    UWTBRTriggerBase* OptionTrigger = bIsMain
        ? TriggerSetComponent->GetActiveMainOptionTrigger()
        : TriggerSetComponent->GetActiveSubOptionTrigger();
    if (IsValid(OptionTrigger))
    {
        HandleLacernHoldOptionInput(bIsMain, bIsPressed, Trigger, OptionTrigger, bIsDualWield);
        return;
    }

    if (bIsPressed)
    {
        Trigger->OnTriggerActivated(this, bIsMain);
        const bool bActivated = Trigger->Activate(TriggerInputValue, bIsDualWield);
        WTBR_VALIDATION_LOG(Verbose, TEXT("Trigger Activate result: %d"), bActivated);
    }
    else
    {
        Trigger->OnTriggerDeactivated(this, bIsMain);
        Trigger->OnReleased(TriggerInputValue, bIsDualWield);
    }
}

bool AWTBRCharacter::HandleMantornFormInput(bool bIsMain, bool bIsPressed)
{
    if (!HasAuthority() || !TriggerSetComponent) return false;

    if (bIsPressed)
    {
        // First press starts the attack; a second side pressing while one is held
        // is ignored — a two-side hold is the exit gesture, not a double attack.
        if (!bMantornAttackHeld)
        {
            bMantornAttackHeld       = true;
            bMantornAttackSideIsMain = bIsMain;
            MantornAttackPressTime   = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        }
        return true;
    }

    // Release: only the side that started the attack fires it.
    if (!bMantornAttackHeld || bMantornAttackSideIsMain != bIsMain)
    {
        return true;
    }
    bMantornAttackHeld = false;

    UWTBRMantornTrigger* Form = TriggerSetComponent->GetActiveMantornForm();
    if (!IsValid(Form)) return true;

    const UWTBRTriggerDataAsset* MainDA = TriggerSetComponent->GetActiveMainDataAsset();
    const float HoldThreshold = IsValid(MainDA)
        ? MainDA->MantornParams.InFormHoldThreshold : 0.2f;

    const float HeldFor = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) - MantornAttackPressTime;
    if (HeldFor >= HoldThreshold)
    {
        Form->SpinSlash();   // Hold = AOE spin
    }
    else
    {
        Form->WhipSlash();   // Tap = forward whip
    }
    return true;
}

bool AWTBRCharacter::HandleLacernHoldOptionInput(
    bool bIsMain, bool bIsPressed,
    UWTBRTriggerBase* Trigger, UWTBRTriggerBase* OptionTrigger,
    bool bIsDualWield)
{
    if (!HasAuthority() || !TriggerSetComponent || !IsValid(Trigger) || !IsValid(OptionTrigger))
    {
        return false;
    }

    bool&  bHeld     = bIsMain ? bLacernChargeHeldMain     : bLacernChargeHeldSub;
    float& PressTime = bIsMain ? LacernChargePressTimeMain : LacernChargePressTimeSub;

    if (bIsPressed)
    {
        bHeld     = true;
        PressTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        return true; // Defer entirely — resolved on release below.
    }

    if (!bHeld)
    {
        return true; // Stale release (e.g. slot changed mid-hold) — nothing to resolve.
    }
    bHeld = false;

    const UWTBRTriggerDataAsset* OptionDA = OptionTrigger->DataAsset;
    const float MinChargeTime       = IsValid(OptionDA) ? OptionDA->ArcvenParams.MinChargeTime : 0.2f;
    const float FullChargeThreshold = IsValid(OptionDA) ? OptionDA->ArcvenParams.FullChargeThreshold : 1.0f;
    const float HeldFor = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) - PressTime;

    if (HeldFor < MinChargeTime)
    {
        // Charge never committed — falls back to Lacern's normal tap swing
        // (Tap must always have meaning, per the input design lock).
        Trigger->OnTriggerActivated(this, bIsMain);
        Trigger->Activate(FInputActionValue(), bIsDualWield);
        return true;
    }

    UWTBRArcvenTrigger* Arcven = Cast<UWTBRArcvenTrigger>(OptionTrigger);
    if (!IsValid(Arcven) || !IsValid(OptionDA) || !IsValid(VaelComponent))
    {
        return true;
    }

    // Insufficient Vael at release = fail-closed, no fire, no fallback swing —
    // matches the existing Telorn/Piercex aim-then-fire convention.
    if (!VaelComponent->TryConsumeVael(OptionDA->VaelCostPerUse))
    {
        return true;
    }

    const bool bFullCharge = HeldFor >= FullChargeThreshold;
    const float Damage = bFullCharge ? OptionDA->ArcvenParams.ArcDamage : OptionDA->ArcvenParams.WeakArcDamage;
    const float Range  = bFullCharge ? OptionDA->ArcvenParams.ArcRange  : OptionDA->ArcvenParams.WeakArcRange;
    Arcven->FireChargedWave(Damage, Range, bIsDualWield);
    return true;
}

void AWTBRCharacter::Server_Dodge_Implementation()
{
    // Authoritative dodge validation / animation notify hook — extend in Phase 2
}

void AWTBRCharacter::Server_CancelCurrentAction_Implementation()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] ServerCancelStart | Owner=%s | Auth=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    if (!HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=NoAuthority | Owner=%s"),
            *GetNameSafe(this));
        return;
    }
    if (!IsValid(TriggerSetComponent))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=InvalidOwner | Owner=%s | TriggerSet=null"),
            *GetNameSafe(this));
        return;
    }

    UWTBRTriggerBase* MainTrigger = TriggerSetComponent->GetActiveMainTrigger();
    UWTBRTriggerBase* SubTrigger = TriggerSetComponent->GetActiveSubTrigger();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] ActiveTrigger | Owner=%s | Main=%s | Sub=%s | MergeState=%d"),
        *GetNameSafe(this),
        *GetNameSafe(MainTrigger),
        *GetNameSafe(SubTrigger),
        static_cast<int32>(TriggerSetComponent->GetCurrentMergeState()));

    if (TriggerSetComponent->GetCurrentMergeState() != EWTBRCompositeBulletType::None)
    {
        const EWTBRCompositeBulletType OldMergeState = TriggerSetComponent->GetCurrentMergeState();
        TriggerSetComponent->CancelMerge();
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] CompositeMergeCanceled | Owner=%s | MergeState=%d | NoRefund=true"),
            *GetNameSafe(this),
            static_cast<int32>(OldMergeState));
        return;
    }

    auto TryCancelTrigger = [this](UWTBRTriggerBase* Trigger, const TCHAR* SlotName) -> bool
    {
        if (!IsValid(Trigger))
        {
            return false;
        }

        if (UWTBRSerpveilTrigger* SerpveilTrigger = Cast<UWTBRSerpveilTrigger>(Trigger))
        {
            if (SerpveilTrigger->CancelCharge())
            {
                return true;
            }
        }

        if (UWTBRAegornTrigger* AegornTrigger = Cast<UWTBRAegornTrigger>(Trigger))
        {
            if (AegornTrigger->CancelShield())
            {
                return true;
            }
        }

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] NoEffect | Owner=%s | Slot=%s | Trigger=%s | Reason=CompletedActionOrUnsupported"),
            *GetNameSafe(this),
            SlotName,
            *GetNameSafe(Trigger));
        return false;
    };

    if (TryCancelTrigger(MainTrigger, TEXT("Main")))
    {
        return;
    }
    if (TryCancelTrigger(SubTrigger, TEXT("Sub")))
    {
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Resolved | Reason=NoRemainingAction | Owner=%s"),
        *GetNameSafe(this));
}

void AWTBRCharacter::Server_SwitchTrigger_Implementation(int32 SlotIndex, bool bIsMain)
{
    if (!TriggerSetComponent) return;
    if (bIsMain) TriggerSetComponent->SwitchMainSlot(SlotIndex);
    else         TriggerSetComponent->SwitchSubSlot(SlotIndex);
}

void AWTBRCharacter::Server_CycleTrigger_Implementation(bool bIsMain)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test34 Server CycleTrigger] Owner=%s | Auth=%s | Main=%s | TriggerSet=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsMain ? TEXT("true") : TEXT("false"),
        IsValid(TriggerSetComponent) ? TEXT("valid") : TEXT("null"));

    if (!TriggerSetComponent) return;
    if (bIsMain) TriggerSetComponent->CycleMainSlot();
    else         TriggerSetComponent->CycleSubSlot();
}

void AWTBRCharacter::Server_DebugConsumeVaelFailTest_Implementation()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return; // Debug-only RPC; no-op in Shipping/Test builds.
#endif
    if (!WTBRShouldLogValidation()) return;
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Debug ConsumeFail Result] Owner=%s | VaelComponent=None"),
            *GetNameSafe(this));
        return;
    }

    const float BeforeVael = VaelComponent->GetCurrentVael();
    const bool bBeforeActive = VaelComponent->IsDesperationActive();
    const bool bBeforeCooldown = VaelComponent->IsDesperationOnCooldown();

    const bool bConsumed = VaelComponent->TryConsumeVael(999.0f);

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Debug ConsumeFail Result] Owner=%s | Cost=999.0 | Consumed=%s | VaelBefore=%.1f | VaelAfter=%.1f | ActiveBefore=%s | ActiveAfter=%s | CooldownBefore=%s | CooldownAfter=%s"),
        *GetNameSafe(this),
        bConsumed ? TEXT("true") : TEXT("false"),
        BeforeVael,
        VaelComponent->GetCurrentVael(),
        bBeforeActive ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationActive() ? TEXT("true") : TEXT("false"),
        bBeforeCooldown ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationOnCooldown() ? TEXT("true") : TEXT("false"));

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            3.0f,
            bConsumed ? FColor::Red : FColor::Green,
            FString::Printf(TEXT("[Debug ConsumeFail] TryConsumeVael(999) returned %s"),
                bConsumed ? TEXT("true") : TEXT("false")));
    }
}

// ─── Delegate Handlers ───────────────────────────────────────────────────────

void AWTBRCharacter::Server_DebugStartBelowThresholdTest_Implementation()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return; // Debug-only RPC; no-op in Shipping/Test builds.
#endif
    if (!WTBRShouldLogValidation()) return;
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Debug StartBelowThreshold Result] Owner=%s | VaelComponent=None"),
            *GetNameSafe(this));
        return;
    }

    const float BeforeVael = VaelComponent->GetCurrentVael();
    const float LowVaelThreshold = VaelComponent->GetLowVaelThreshold();
    const bool bBeforeActive = VaelComponent->IsDesperationActive();
    const bool bBeforeCooldown = VaelComponent->IsDesperationOnCooldown();

    VaelComponent->ResetDesperationState();
    VaelComponent->DebugSetCurrentVaelDirect(15.0f);

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Debug StartBelowThreshold Result] Owner=%s | VaelBefore=%.1f | VaelAfter=%.1f | LowVaelThreshold=%.1f | ActiveBefore=%s | ActiveAfter=%s | CooldownBefore=%s | CooldownAfter=%s | TryConsumeUsed=false"),
        *GetNameSafe(this),
        BeforeVael,
        VaelComponent->GetCurrentVael(),
        LowVaelThreshold,
        bBeforeActive ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationActive() ? TEXT("true") : TEXT("false"),
        bBeforeCooldown ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationOnCooldown() ? TEXT("true") : TEXT("false"));

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            3.0f,
            FColor::Green,
            FString::Printf(TEXT("[Debug StartBelowThreshold] Vael %.0f -> %.0f, TryConsumeUsed=false"),
                BeforeVael,
                VaelComponent->GetCurrentVael()));
    }
}

void AWTBRCharacter::Server_DebugRefillVaelNoDesperationReset_Implementation()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return; // Debug-only RPC; no-op in Shipping/Test builds.
#endif
    if (!WTBRShouldLogValidation()) return;
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Debug RefillNoDesperationReset Result] Owner=%s | VaelComponent=None"),
            *GetNameSafe(this));
        return;
    }

    const float BeforeVael = VaelComponent->GetCurrentVael();
    const float MaxVael = VaelComponent->GetMaxVael();
    const float LowVaelThreshold = VaelComponent->GetLowVaelThreshold();
    const bool bBeforeActive = VaelComponent->IsDesperationActive();
    const bool bBeforeCooldown = VaelComponent->IsDesperationOnCooldown();

    VaelComponent->DebugSetCurrentVaelDirect(MaxVael);

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Debug RefillNoDesperationReset Result] Owner=%s | VaelBefore=%.1f | VaelAfter=%.1f | MaxVael=%.1f | LowVaelThreshold=%.1f | ActiveBefore=%s | ActiveAfter=%s | CooldownBefore=%s | CooldownAfter=%s | CooldownPreserved=true | TryConsumeUsed=false"),
        *GetNameSafe(this),
        BeforeVael,
        VaelComponent->GetCurrentVael(),
        MaxVael,
        LowVaelThreshold,
        bBeforeActive ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationActive() ? TEXT("true") : TEXT("false"),
        bBeforeCooldown ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationOnCooldown() ? TEXT("true") : TEXT("false"));

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            3.0f,
            FColor::Green,
            FString::Printf(TEXT("[Debug RefillNoReset] Vael %.0f -> %.0f, cooldown preserved"),
                BeforeVael,
                VaelComponent->GetCurrentVael()));
    }
}

void AWTBRCharacter::Server_DebugResetDesperationStateTest_Implementation()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return; // Debug-only RPC; no-op in Shipping/Test builds.
#endif
    if (!WTBRShouldLogValidation()) return;
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[Debug ResetDesperationStateTest] Owner=%s | VaelComponent=None"),
            *GetNameSafe(this));
        return;
    }

    const float BeforeVael = VaelComponent->GetCurrentVael();
    const float LowVaelThreshold = VaelComponent->GetLowVaelThreshold();
    const bool bBeforeActive = VaelComponent->IsDesperationActive();
    const bool bBeforeCooldown = VaelComponent->IsDesperationOnCooldown();
    const bool bBeforeActiveTimer = VaelComponent->DebugIsDesperationActiveTimerActive();
    const bool bBeforeCooldownTimer = VaelComponent->DebugIsDesperationCooldownTimerActive();

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Debug ResetDesperationStateTest Before] Owner=%s | CurrentVael=%.1f | LowVaelThreshold=%.1f | Active=%s | Cooldown=%s | ActiveTimerActive=%s | CooldownTimerActive=%s"),
        *GetNameSafe(this),
        BeforeVael,
        LowVaelThreshold,
        bBeforeActive ? TEXT("true") : TEXT("false"),
        bBeforeCooldown ? TEXT("true") : TEXT("false"),
        bBeforeActiveTimer ? TEXT("true") : TEXT("false"),
        bBeforeCooldownTimer ? TEXT("true") : TEXT("false"));

    VaelComponent->ResetDesperationState();

    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Debug ResetDesperationStateTest After] Owner=%s | CurrentVael=%.1f | Active=%s | Cooldown=%s | ActiveTimerActive=%s | CooldownTimerActive=%s"),
        *GetNameSafe(this),
        VaelComponent->GetCurrentVael(),
        VaelComponent->IsDesperationActive() ? TEXT("true") : TEXT("false"),
        VaelComponent->IsDesperationOnCooldown() ? TEXT("true") : TEXT("false"),
        VaelComponent->DebugIsDesperationActiveTimerActive() ? TEXT("true") : TEXT("false"),
        VaelComponent->DebugIsDesperationCooldownTimerActive() ? TEXT("true") : TEXT("false"));

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            3.0f,
            FColor::Green,
            TEXT("[Debug ResetDesperationStateTest] Reset called on server"));
    }
}

void AWTBRCharacter::OnStaminaChangedHandler(float NewStamina, bool bIsExhausted)
{
    if (MovementExtComponent && StaminaComponent)
    {
        MovementExtComponent->SetStaminaPenalty(StaminaComponent->GetSpeedPenalty());
    }
}

void AWTBRCharacter::OnLimbDestroyedHandler(EWTBRLimbType Limb, FWTBRLimbState NewState)
{
    if (MovementExtComponent && HealthComponent)
    {
        MovementExtComponent->SetLimbPenalty(HealthComponent->GetTotalSpeedPenaltyFromLimbs());
    }
}

void AWTBRCharacter::OnDualWieldStateChangedHandler(EWTBRDualWieldState NewState, ETriggerCategory ActiveCategory)
{
    // Notify Animation Blueprint / UI — extend in Phase 2
}

// ─── Replication ─────────────────────────────────────────────────────────────

void AWTBRCharacter::OnActiveTriggerChangedHandler(ETriggerSlot NewSlot)
{
    RefreshHUDHints();
}

FText AWTBRCharacter::GetMainTriggerHintText() const
{
    const UWTBRTriggerBase* MainTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainTrigger()
        : nullptr;
    const UWTBRTriggerDataAsset* MainDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainDataAsset()
        : nullptr;

    return FormatHUDTriggerHintText(
        NSLOCTEXT("WTBRCharacter", "HUDMainTriggerSlotLabel", "Main"),
        GetHUDTriggerNameWithFallback(MainTrigger, MainDataAsset),
        DefaultMappingContext,
        FireMainAction);
}

FText AWTBRCharacter::GetMainTriggerNameText() const
{
    const UWTBRTriggerBase* MainTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainTrigger()
        : nullptr;
    const UWTBRTriggerDataAsset* MainDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainDataAsset()
        : nullptr;

    return GetHUDTriggerNameWithFallback(MainTrigger, MainDataAsset);
}

FText AWTBRCharacter::GetSubTriggerHintText() const
{
    const UWTBRTriggerBase* SubTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubTrigger()
        : nullptr;
    const UWTBRTriggerDataAsset* SubDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubDataAsset()
        : nullptr;

    return FormatHUDTriggerHintText(
        NSLOCTEXT("WTBRCharacter", "HUDSubTriggerSlotLabel", "Sub"),
        GetHUDTriggerNameWithFallback(SubTrigger, SubDataAsset),
        DefaultMappingContext,
        FireSubAction);
}

FText AWTBRCharacter::GetSubTriggerNameText() const
{
    const UWTBRTriggerBase* SubTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubTrigger()
        : nullptr;
    const UWTBRTriggerDataAsset* SubDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubDataAsset()
        : nullptr;

    return GetHUDTriggerNameWithFallback(SubTrigger, SubDataAsset);
}

UTexture2D* AWTBRCharacter::GetMainTriggerHUDIcon() const
{
    const UWTBRTriggerDataAsset* MainDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainDataAsset()
        : nullptr;

    if (!IsValid(MainDataAsset) || MainDataAsset->HUDIcon.IsNull())
    {
        return nullptr;
    }

    // HUD hot path (rebuilt on every HP/Vael/inventory/trigger refresh): use the
    // already-resolved texture only. Never force a synchronous streaming load here;
    // an unloaded icon degrades to null (no icon shown) instead of stalling the frame.
    return MainDataAsset->HUDIcon.Get();
}

UTexture2D* AWTBRCharacter::GetSubTriggerHUDIcon() const
{
    const UWTBRTriggerDataAsset* SubDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubDataAsset()
        : nullptr;

    if (!IsValid(SubDataAsset) || SubDataAsset->HUDIcon.IsNull())
    {
        return nullptr;
    }

    // HUD hot path (see GetMainTriggerHUDIcon): already-resolved texture only, no sync load.
    return SubDataAsset->HUDIcon.Get();
}

int32 AWTBRCharacter::GetActiveMainTriggerSlotIndex() const
{
    return IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainIndex()
        : INDEX_NONE;
}

int32 AWTBRCharacter::GetActiveSubTriggerSlotIndex() const
{
    return IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubIndex()
        : INDEX_NONE;
}

FWTBRHUDTriggerVaelAffordability AWTBRCharacter::GetActiveMainTriggerVaelAffordabilityForHUD() const
{
    return GetActiveTriggerVaelAffordabilityForHUD(true);
}

FWTBRHUDTriggerVaelAffordability AWTBRCharacter::GetActiveSubTriggerVaelAffordabilityForHUD() const
{
    return GetActiveTriggerVaelAffordabilityForHUD(false);
}

bool AWTBRCharacter::CanAffordActiveMainTriggerForHUD() const
{
    return GetActiveMainTriggerVaelAffordabilityForHUD().bCanAfford;
}

bool AWTBRCharacter::CanAffordActiveSubTriggerForHUD() const
{
    return GetActiveSubTriggerVaelAffordabilityForHUD().bCanAfford;
}

float AWTBRCharacter::GetActiveMainTriggerBaseVaelCostForHUD() const
{
    return GetActiveMainTriggerVaelAffordabilityForHUD().BaseVaelCost;
}

float AWTBRCharacter::GetActiveSubTriggerBaseVaelCostForHUD() const
{
    return GetActiveSubTriggerVaelAffordabilityForHUD().BaseVaelCost;
}

float AWTBRCharacter::GetActiveMainTriggerEffectiveVaelCostForHUD() const
{
    return GetActiveMainTriggerVaelAffordabilityForHUD().EffectiveVaelCost;
}

float AWTBRCharacter::GetActiveSubTriggerEffectiveVaelCostForHUD() const
{
    return GetActiveSubTriggerVaelAffordabilityForHUD().EffectiveVaelCost;
}

FWTBRHUDTriggerVaelAffordability AWTBRCharacter::GetActiveTriggerVaelAffordabilityForHUD(bool bIsMain) const
{
    FWTBRHUDTriggerVaelAffordability Result;

    const UWTBRTriggerBase* Trigger = nullptr;
    const UWTBRTriggerDataAsset* DataAsset = nullptr;
    if (IsValid(TriggerSetComponent))
    {
        Trigger = bIsMain
            ? TriggerSetComponent->GetActiveMainTrigger()
            : TriggerSetComponent->GetActiveSubTrigger();
        DataAsset = bIsMain
            ? TriggerSetComponent->GetActiveMainDataAsset()
            : TriggerSetComponent->GetActiveSubDataAsset();
    }

    if (IsValid(VaelComponent))
    {
        Result.CurrentVael = VaelComponent->GetCurrentVael();
    }

    if (!IsValid(Trigger) || !IsValid(DataAsset))
    {
        Result.bCanAfford = false;
        return Result;
    }

    if (Trigger->IsA<UWTBRSerpveilTrigger>())
    {
        Result.bUsesChargeOrVariableCost = true;
        Result.BaseVaelCost = DataAsset->SerpveilParams.SerpveilVaelPerSecond;
        return Result;
    }

    if (Trigger->IsA<UWTBRAegornTrigger>())
    {
        Result.bUsesDrain = true;
        Result.BaseVaelCost = DataAsset->VaelCostPerUse;
        return Result;
    }

    if (DataAsset->Category == ETriggerCategory::BlackTrigger)
    {
        Result.bUsesDrain = Trigger->IsA<UWTBRSolvarnTrigger>();
        return Result;
    }

    if (IsKnownZeroVaelTriggerForHUD(Trigger))
    {
        Result.bIsCostKnownForHUD = true;
        Result.bHasVaelCost = false;
        Result.bCanAfford = true;
        return Result;
    }

    if (!IsKnownPerUseVaelTriggerForHUD(Trigger))
    {
        return Result;
    }

    Result.bIsCostKnownForHUD = true;
    Result.BaseVaelCost = FMath::Max(0.0f, DataAsset->VaelCostPerUse);
    Result.bHasVaelCost = Result.BaseVaelCost > 0.0f;

    const float CostMultiplier = IsValid(VaelComponent)
        ? VaelComponent->GetVaelCostMultiplier()
        : 1.0f;
    Result.EffectiveVaelCost = FMath::Max(0.0f, Result.BaseVaelCost * CostMultiplier);
    Result.bCanAfford = !Result.bHasVaelCost
        || (IsValid(VaelComponent)
            && !VaelComponent->IsOverheated()
            && Result.CurrentVael >= Result.EffectiveVaelCost);

    return Result;
}

FText AWTBRCharacter::GetSwitchMainHintText() const
{
    return FormatHUDActionHintText(
        NSLOCTEXT("WTBRCharacter", "HUDSwitchMainHintLabel", "Switch Main"),
        DefaultMappingContext,
        SwitchMainAction);
}

FText AWTBRCharacter::GetSwitchSubHintText() const
{
    return FormatHUDActionHintText(
        NSLOCTEXT("WTBRCharacter", "HUDSwitchSubHintLabel", "Switch Sub"),
        DefaultMappingContext,
        SwitchSubAction);
}

FText AWTBRCharacter::GetCancelHintText() const
{
    return NSLOCTEXT("WTBRCharacter", "HUDCancelHintLabel", "Cancel");
}

void AWTBRCharacter::RefreshHUDHints()
{
    const UWTBRTriggerBase* MainTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainTrigger()
        : nullptr;
    const UWTBRTriggerBase* SubTrigger = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubTrigger()
        : nullptr;
    const UWTBRTriggerDataAsset* MainDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainDataAsset()
        : nullptr;
    const UWTBRTriggerDataAsset* SubDataAsset = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubDataAsset()
        : nullptr;
    const int32 ActiveMainIndex = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveMainIndex()
        : INDEX_NONE;
    const int32 ActiveSubIndex = IsValid(TriggerSetComponent)
        ? TriggerSetComponent->GetActiveSubIndex()
        : INDEX_NONE;

    const FText MainName = GetHUDTriggerNameWithFallback(MainTrigger, MainDataAsset);
    const FText SubName = GetHUDTriggerNameWithFallback(SubTrigger, SubDataAsset);
    const FText MainHint = GetMainTriggerHintText();
    const FText SubHint = GetSubTriggerHintText();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[HUD Hint Test] Refresh | Owner=%s | Auth=%s | Local=%s | ActiveMainIndex=%d | ActiveSubIndex=%d | MainTriggerValid=%s | SubTriggerValid=%s | MainFallbackDA=%s | SubFallbackDA=%s | MainClass=%s | MainName=%s | SubClass=%s | SubName=%s | MainHint=%s | SubHint=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        ActiveMainIndex,
        ActiveSubIndex,
        IsValid(MainTrigger) ? TEXT("true") : TEXT("false"),
        IsValid(SubTrigger) ? TEXT("true") : TEXT("false"),
        IsValid(MainDataAsset) ? TEXT("true") : TEXT("false"),
        IsValid(SubDataAsset) ? TEXT("true") : TEXT("false"),
        *GetNameSafe(MainTrigger ? MainTrigger->GetClass() : nullptr),
        *MainName.ToString(),
        *GetNameSafe(SubTrigger ? SubTrigger->GetClass() : nullptr),
        *SubName.ToString(),
        *MainHint.ToString(),
        *SubHint.ToString());

    OnHUDHintsChanged.Broadcast();
}

// ─── Runtime UI (Phase 6A smoke-test scaffold) ───────────────────────────────
// TEMPORARY: Character-owned UI, accepted only for the first PIE smoke test
// because no custom APlayerController/AHUD subclass exists yet. Migrate to a
// PlayerController/HUD/UI-manager owner once BR respawn/pawn-swap exists.

bool AWTBRCharacter::ValidateLocalUISetup(bool bLogIfMissing) const
{
    const bool bHUDReady = IsValid(HUDWidgetClass);
    const bool bBagLootReady = IsValid(BagLootWidgetClass);

    if (bLogIfMissing)
    {
        if (!bHUDReady)
        {
            WTBR_VALIDATION_LOG(Warning,
                TEXT("[UI] ValidateLocalUISetup: HUDWidgetClass is not assigned yet (assign it on the Character Blueprint/CDO once WBP_HUD_Generated's Parent Class is set) | Owner=%s"),
                *GetNameSafe(this));
        }
        if (!bBagLootReady)
        {
            WTBR_VALIDATION_LOG(Warning,
                TEXT("[UI] ValidateLocalUISetup: BagLootWidgetClass is not assigned yet (assign it on the Character Blueprint/CDO once WBP_BagLootLayer_Generated's Parent Class is set) | Owner=%s"),
                *GetNameSafe(this));
        }
    }

    return bHUDReady && bBagLootReady;
}

void AWTBRCharacter::CreateLocalPlayerUI()
{
    if (!IsLocallyControlled())
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[UI] CreateLocalPlayerUI: skipped (not locally controlled) | Owner=%s"),
            *GetNameSafe(this));
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!IsValid(PC))
    {
        WTBR_VALIDATION_LOG(Warning,
            TEXT("[UI] CreateLocalPlayerUI: skipped (locally controlled but no valid PlayerController yet) | Owner=%s"),
            *GetNameSafe(this));
        return;
    }

    // Logs which widget classes (if any) are still unassigned; never blocks
    // creation of whichever widget class IS already set.
    ValidateLocalUISetup(/*bLogIfMissing=*/true);

    if (!IsValid(HUDWidgetInstance) && IsValid(HUDWidgetClass))
    {
        HUDWidgetInstance = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
        if (IsValid(HUDWidgetInstance))
        {
            HUDWidgetInstance->AddToViewport(0);
            WTBR_VALIDATION_LOG(Log,
                TEXT("[UI] CreateLocalPlayerUI: HUD widget created and added to viewport | Class=%s | Owner=%s"),
                *GetNameSafe(HUDWidgetClass), *GetNameSafe(this));
        }
    }

    if (!IsValid(BagLootWidgetInstance) && IsValid(BagLootWidgetClass))
    {
        BagLootWidgetInstance = CreateWidget<UUserWidget>(PC, BagLootWidgetClass);
        if (IsValid(BagLootWidgetInstance))
        {
            // Higher Z-order than the HUD so the Bag/Loot layer draws on top.
            BagLootWidgetInstance->AddToViewport(10);
            BagLootWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
            WTBR_VALIDATION_LOG(Log,
                TEXT("[UI] CreateLocalPlayerUI: BagLoot widget created, added to viewport, hidden initially | Class=%s | Owner=%s"),
                *GetNameSafe(BagLootWidgetClass), *GetNameSafe(this));
        }
    }

    if (!IsValid(RadarWidgetInstance))
    {
        RadarWidgetInstance = CreateWidget<UWTBRRadarWidget>(PC, UWTBRRadarWidget::StaticClass());
        if (IsValid(RadarWidgetInstance))
        {
            RadarWidgetInstance->AddToViewport(1);
            RadarWidgetInstance->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            RadarWidgetInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
            RadarWidgetInstance->SetPositionInViewport(FVector2D::ZeroVector);
        }
    }
}

void AWTBRCharacter::DestroyLocalPlayerUI()
{
    if (IsValid(HUDWidgetInstance))
    {
        HUDWidgetInstance->RemoveFromParent();
    }
    HUDWidgetInstance = nullptr;

    if (IsValid(BagLootWidgetInstance))
    {
        BagLootWidgetInstance->RemoveFromParent();
    }
    BagLootWidgetInstance = nullptr;

    if (IsValid(RadarWidgetInstance))
    {
        RadarWidgetInstance->RemoveFromParent();
    }
    RadarWidgetInstance = nullptr;
}

void AWTBRCharacter::ShowBagLootLayer()
{
    if (!IsValid(BagLootWidgetInstance))
    {
        CreateLocalPlayerUI();
    }

    if (IsValid(BagLootWidgetInstance))
    {
        BagLootWidgetInstance->SetVisibility(ESlateVisibility::Visible);

        // Phase 6D: force a fresh snapshot now, rather than waiting for the next
        // reactive trigger (OnInventoryChanged / the previously-focused
        // container's OnCorpseLootEntriesChanged). UWTBRBagLootViewModelComponent
        // has no focus-changed delegate (documented TODO in
        // WTBRBagLootViewModelComponent.h) and only re-resolves the focused
        // corpse loot container when RefreshBagLootSnapshot() runs — without this
        // call, showing the panel right after an interact could display a stale
        // snapshot from before this container was focused. Read-only refresh;
        // does not mutate inventory, loot, or trigger state.
        if (IsValid(BagLootViewModelComponent))
        {
            BagLootViewModelComponent->RefreshBagLootSnapshot();
        }

        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[UI] ShowBagLootLayer: Bag/Loot layer now visible | Owner=%s"),
            *GetNameSafe(this));
    }
    else if (!IsLocallyControlled())
    {
        WTBR_VALIDATION_LOG(Verbose,
            TEXT("[UI] ShowBagLootLayer: skipped (not locally controlled) | Owner=%s"),
            *GetNameSafe(this));
    }
    else if (!IsValid(BagLootWidgetClass))
    {
        WTBR_VALIDATION_LOG(Warning,
            TEXT("[UI] ShowBagLootLayer: BagLootWidgetClass is not assigned — nothing to show | Owner=%s"),
            *GetNameSafe(this));
    }
    else
    {
        WTBR_VALIDATION_LOG(Warning,
            TEXT("[UI] ShowBagLootLayer: BagLootWidgetInstance still invalid after CreateLocalPlayerUI (unexpected) | Owner=%s"),
            *GetNameSafe(this));
    }
}

void AWTBRCharacter::HideBagLootLayer()
{
    if (IsValid(BagLootWidgetInstance))
    {
        BagLootWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void AWTBRCharacter::ToggleBagLootLayer()
{
    if (IsBagLootLayerVisible())
    {
        HideBagLootLayer();
        return;
    }

    if (!IsValid(BagLootWidgetInstance) && !IsValid(BagLootWidgetClass))
    {
        WTBR_VALIDATION_LOG(Warning,
            TEXT("[UI] ToggleBagLootLayer: no BagLootWidgetInstance and no BagLootWidgetClass assigned — nothing to toggle | Owner=%s"),
            *GetNameSafe(this));
        return;
    }

    ShowBagLootLayer();
}

bool AWTBRCharacter::IsBagLootLayerVisible() const
{
    return IsValid(BagLootWidgetInstance)
        && BagLootWidgetInstance->IsInViewport()
        && BagLootWidgetInstance->GetVisibility() != ESlateVisibility::Collapsed;
}

void AWTBRCharacter::CloseAnyOpenLocalUI()
{
    // Today, Bag/Loot is the only local panel this scaffold owns. Written as a
    // single entry point (rather than callers calling HideBagLootLayer()
    // directly) so a future second panel (e.g. the Q/E hold trigger-selection
    // wheel) only needs to be added here, not at every call site.
    if (IsBagLootLayerVisible())
    {
        HideBagLootLayer();
    }
}

bool AWTBRCharacter::IsAnyLocalUIPanelOpen() const
{
    return IsBagLootLayerVisible();
}

void AWTBRCharacter::OnCorpseLootInteractRequestedHandler(AWTBRCorpseLootContainerActor* Container)
{
    WTBR_VALIDATION_LOG(Log,
        TEXT("[UI] OnCorpseLootInteractRequestedHandler: received for container %s | Owner=%s"),
        *GetNameSafe(Container), *GetNameSafe(this));

    // Presentation-only: shows the existing Bag/Loot layer. Does not request
    // loot or mutate any gameplay state — the player still confirms pickup via
    // the existing server-authoritative RequestPickupCorpseLootEntry path.
    ShowBagLootLayer();
}

void AWTBRCharacter::OnRep_ActionPing()
{
    // Client-side visual feedback for radar action ping
}

void AWTBRCharacter::SetRadarCloaked(bool bNewCloaked)
{
    if (!HasAuthority() || bRadarCloaked == bNewCloaked) return;
    bRadarCloaked = bNewCloaked;
    OnRep_RadarCloaked();
}

void AWTBRCharacter::OnRep_RadarCloaked()
{
    // The native radar reads this replicated state directly. Blueprint may add
    // Bagworm VFX here without changing gameplay visibility.
}

void AWTBRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRCharacter, TeamId);
    DOREPLIFETIME(AWTBRCharacter, bActionPingActive);
    DOREPLIFETIME(AWTBRCharacter, bRadarCloaked);
    DOREPLIFETIME(AWTBRCharacter, bIsStaggered);
    DOREPLIFETIME(AWTBRCharacter, bSerpveilChargeTelegraphActive);
    DOREPLIFETIME(AWTBRCharacter, bLacernExtendTelegraphActive);
    DOREPLIFETIME(AWTBRCharacter, bLacernExtendDualWield);
}

// ─── Team Identity ────────────────────────────────────────────────────────────

void AWTBRCharacter::SetTeamId(int32 NewTeamId)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Team] SetTeamId rejected on %s: no authority."), *GetNameSafe(this));
        return;
    }

    TeamId = NewTeamId;
}

// ─── Stagger System ───────────────────────────────────────────────────────────

void AWTBRCharacter::ApplyStagger(float Duration)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyStagger Start | Target=%s | Auth=%s | Duration=%.2f | ApplyStaggerCalled=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        Duration);
    if (!HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyStagger Rejected | Target=%s | Reason=NoAuthority | Duration=%.2f"),
            *GetNameSafe(this),
            Duration);
        return;
    }
    if (Duration <= 0.0f)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyStagger Rejected | Target=%s | Reason=NonPositiveDuration | Duration=%.2f"),
            *GetNameSafe(this),
            Duration);
        return;
    }
    if (TriggerSetComponent) TriggerSetComponent->CancelMerge();
    bIsStaggered = true;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyStagger Result | Target=%s | bIsStaggered=true | Duration=%.2f"),
        *GetNameSafe(this),
        Duration);
    GetWorld()->GetTimerManager().SetTimer(
        StaggerTimer,
        this, &AWTBRCharacter::OnStaggerExpired,
        Duration,
        false);
}

void AWTBRCharacter::OnStaggerExpired()
{
    bIsStaggered = false;
}

void AWTBRCharacter::OnRep_bIsStaggered()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] OnRep_bIsStaggered | Target=%s | Auth=%s | Local=%s | bIsStaggered=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"),
        bIsStaggered ? TEXT("true") : TEXT("false"));
    // Blueprint: true=disable input+stagger anim | false=re-enable input
}

void AWTBRCharacter::SetSerpveilChargeTelegraphActive(bool bActive)
{
    if (!HasAuthority()) return;
    if (bSerpveilChargeTelegraphActive == bActive) return;

    bSerpveilChargeTelegraphActive = bActive;

    // Server does not receive its own OnRep — call it directly so the host's
    // local cosmetic reacts too (replication alone only reaches remote clients).
    OnRep_SerpveilChargeTelegraph();
}

void AWTBRCharacter::OnRep_SerpveilChargeTelegraph()
{
    OnSerpveilChargeTelegraphChanged(bSerpveilChargeTelegraphActive);
}

void AWTBRCharacter::SetLacernExtendTelegraphActive(bool bActive, bool bIsDualWield)
{
    if (!HasAuthority()) return;
    if (bLacernExtendTelegraphActive == bActive &&
        bLacernExtendDualWield == bIsDualWield)
    {
        return;
    }

    bLacernExtendTelegraphActive = bActive;
    bLacernExtendDualWield = bIsDualWield;

    // Server does not receive its own OnRep — call it directly so the host's
    // local cosmetic reacts too (replication alone only reaches remote clients).
    OnRep_LacernExtendTelegraph();
}

void AWTBRCharacter::OnRep_LacernExtendTelegraph()
{
    OnLacernExtendTelegraphChanged(bLacernExtendTelegraphActive, bLacernExtendDualWield);
}

void AWTBRCharacter::Multicast_LacernHit_Implementation(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit)
{
    OnLacernHitReceived(ImpactPoint, ImpactNormal, bDualWieldHit);
}

void AWTBRCharacter::ClearTriggerCosmeticVFXState()
{
    if (!HasAuthority()) return;
    SetSerpveilChargeTelegraphActive(false);
    SetLacernExtendTelegraphActive(false, false);
}

// ─── Trigger Activation RPCs ─────────────────────────────────────────────────

void AWTBRCharacter::Server_ActivateMainTrigger_Implementation(bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=true | Pressed=true | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(true, true);
}

void AWTBRCharacter::Server_ReleaseMainTrigger_Implementation(bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=true | Pressed=false | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(true, false);
}

void AWTBRCharacter::Server_ActivateSubTrigger_Implementation(bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=false | Pressed=true | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(false, true);
}

void AWTBRCharacter::Server_ReleaseSubTrigger_Implementation(bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=false | Pressed=false | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(false, false);
}

void AWTBRCharacter::Server_CycleMainSlot_Implementation()
{
    if (!HasAuthority()) return;
    if (IsValid(TriggerSetComponent))
    {
        TriggerSetComponent->CycleMainSlot();
    }
}

void AWTBRCharacter::Server_CycleSubSlot_Implementation()
{
    if (!HasAuthority()) return;
    if (IsValid(TriggerSetComponent))
    {
        TriggerSetComponent->CycleSubSlot();
    }
}

void AWTBRCharacter::Server_StartVaelSprint_Implementation()
{
    if (!HasAuthority()) return;
    if (IsValid(MovementExtComponent))
    {
        MovementExtComponent->StartVaelSprint();
    }
}

void AWTBRCharacter::Server_StopVaelSprint_Implementation()
{
    if (!HasAuthority()) return;
    if (IsValid(MovementExtComponent))
    {
        MovementExtComponent->StopVaelSprint();
    }
}

#if !UE_BUILD_SHIPPING
// ─── B7D client-side validation harness ─────────────────────────────────────
// Read/log, client-side view rotation, and existing server RPC requests only.
// No client-side inventory/slot/loot/ground-item mutation happens here.

void AWTBRCharacter::PollB7ClientValidationCVar()
{
    const float Delay = CVarWTBRB7ClientValidationDelay.GetValueOnGameThread();
    if (Delay <= 0.0f)
    {
        return;
    }

    // Only the locally controlled client pawn runs the sequence. Remote replicas
    // and a not-yet-possessed pawn keep polling until possession replicates.
    if (!IsLocallyControlled() || !IsValid(Controller))
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(B7ClientValidationPollTimerHandle);

    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: CVar requested client validation delay %.1f s (character %s)."),
        Delay,
        *GetNameSafe(this));

    B7ClientValidationRetryCount = 0;

    FTimerDelegate TimerDel;
    TimerDel.BindUObject(this, &AWTBRCharacter::RunB7ClientValidationSequence);
    GetWorldTimerManager().SetTimer(B7ClientValidationTimerHandle, TimerDel, Delay, /*bLoop=*/false);
}

void AWTBRCharacter::RunB7ClientValidationSequence()
{
    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: Sequence attempt (try %d/%d, character %s, HasAuthority=%s)."),
        B7ClientValidationRetryCount + 1,
        B7ClientValidationMaxRetries + 1,
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));

    UWorld* World = GetWorld();
    if (!World || !IsValid(InteractionComponent))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("B7ClientValidation: Sequence failed reason — world or InteractionComponent missing."));
        return;
    }

    // Container receipt proof: enumerate the replicated corpse loot containers this
    // client actually received, with entry counts from the replicated LootEntries.
    AWTBRCorpseLootContainerActor* NearestContainer = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    int32 ContainerCount = 0;

    for (TActorIterator<AWTBRCorpseLootContainerActor> It(World); It; ++It)
    {
        AWTBRCorpseLootContainerActor* Container = *It;
        if (!IsValid(Container))
        {
            continue;
        }
        ++ContainerCount;
        UE_LOG(LogTemp, Log,
            TEXT("B7ClientValidation: Container receipt — Actor=%s Location=%s Distance=%.1f HasAuthority=%s Entries=%d HasAvailableLoot=%s"),
            *GetNameSafe(Container),
            *Container->GetActorLocation().ToString(),
            FVector::Dist(GetActorLocation(), Container->GetActorLocation()),
            Container->HasAuthority() ? TEXT("true") : TEXT("false"),
            Container->GetLootEntryCount(),
            Container->HasAvailableLootEntries() ? TEXT("true") : TEXT("false"));

        const float DistSq = FVector::DistSquared(GetActorLocation(), Container->GetActorLocation());
        if (DistSq < NearestDistanceSq)
        {
            NearestContainer = Container;
            NearestDistanceSq = DistSq;
        }
    }

    // LootEntries replicate separately from the actor channel opening, so a
    // container with 0 replicated entries is retried the same as a missing actor.
    if (!IsValid(NearestContainer) || NearestContainer->GetLootEntryCount() == 0)
    {
        if (B7ClientValidationRetryCount < B7ClientValidationMaxRetries)
        {
            ++B7ClientValidationRetryCount;
            UE_LOG(LogTemp, Log,
                TEXT("B7ClientValidation: No replicated container with entries yet (found %d actor(s)). Retry %d/%d in 2 s."),
                ContainerCount,
                B7ClientValidationRetryCount,
                B7ClientValidationMaxRetries);
            FTimerDelegate RetryDel;
            RetryDel.BindUObject(this, &AWTBRCharacter::RunB7ClientValidationSequence);
            GetWorldTimerManager().SetTimer(B7ClientValidationTimerHandle, RetryDel, 2.0f, /*bLoop=*/false);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("B7ClientValidation: Sequence failed reason — max retries (%d) reached, no replicated corpse loot container."),
                B7ClientValidationMaxRetries);
        }
        return;
    }

    LogB7ClientValidationWorldState(TEXT("Pre"));

    // Face the container. Client-side view rotation only — this is the local
    // player's own control rotation (equivalent to moving the mouse).
    const FRotator FaceRotation = (NearestContainer->GetActorLocation() - GetActorLocation()).Rotation();
    Controller->SetControlRotation(FaceRotation);
    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: Facing container %s (control rotation %s)."),
        *GetNameSafe(NearestContainer),
        *FaceRotation.ToString());

    // Give the camera manager one update so GetPlayerViewPoint reflects the new
    // control rotation before the focus traces run.
    FTimerDelegate ContinueDel;
    ContinueDel.BindUObject(this, &AWTBRCharacter::ContinueB7ClientValidationSequence);
    GetWorldTimerManager().SetTimer(B7ClientValidationTimerHandle, ContinueDel, 1.0f, /*bLoop=*/false);
}

void AWTBRCharacter::ContinueB7ClientValidationSequence()
{
    if (!IsValid(InteractionComponent))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("B7ClientValidation: Sequence failed reason — InteractionComponent missing at focus step."));
        return;
    }

    // Focus + prompt: proves this client can see/focus the replicated container.
    const FText Prompt = InteractionComponent->GetFocusedInteractionPromptText();
    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: Focused prompt='%s' (empty=%s)."),
        *Prompt.ToString(),
        Prompt.IsEmpty() ? TEXT("true") : TEXT("false"));

    // Priority: production context-interact resolution with all replicated
    // candidates present. UWTBRInteractionComponent logs which priority branch
    // handled it ("Handled by corpse/container/chest focus (priority 1)." when
    // the container wins over dropped trigger / ground item candidates).
    const bool bHandled = InteractionComponent->RequestContextInteract();
    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: RequestContextInteract handled=%s."),
        bHandled ? TEXT("true") : TEXT("false"));

    // Interaction RPC chain: existing debug bridge -> Server_RequestPickupCorpseLootEntry.
    // The server logs the authoritative verdict (accept/swap/reject/rollback).
    WTBRDebugCharacterLootNearestCorpseContainer(0, 0);

    // Post-state after the server round-trip + replication.
    FTimerDelegate FinishDel;
    FinishDel.BindUObject(this, &AWTBRCharacter::FinishB7ClientValidationSequence);
    GetWorldTimerManager().SetTimer(B7ClientValidationTimerHandle, FinishDel, 3.0f, /*bLoop=*/false);
}

void AWTBRCharacter::FinishB7ClientValidationSequence()
{
    LogB7ClientValidationWorldState(TEXT("Post"));

    // Slot state after the server round-trip (shows a swapped-in entry if the
    // pickup succeeded server-side).
    WTBRDebugCharacterPrintTriggerSlots();

    UE_LOG(LogTemp, Log,
        TEXT("B7ClientValidation: Sequence complete (character %s)."),
        *GetNameSafe(this));
}

void AWTBRCharacter::LogB7ClientValidationWorldState(const TCHAR* PhaseTag) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FVector ViewLocation = GetActorLocation();
    FRotator ViewRotation = GetActorRotation();
    if (IsValid(Controller))
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    const FVector ViewForward = ViewRotation.Vector();

    for (TActorIterator<AWTBRCorpseLootContainerActor> It(World); It; ++It)
    {
        const AWTBRCorpseLootContainerActor* Container = *It;
        if (!IsValid(Container))
        {
            continue;
        }

        TArray<FWTBRCorpseLootEntry> Entries;
        Container->GetLootEntriesForUIReadOnly(Entries);
        FString EntrySummary;
        for (int32 Index = 0; Index < Entries.Num(); ++Index)
        {
            EntrySummary += FString::Printf(TEXT("[%d]%s=%s "),
                Index,
                *Entries[Index].TriggerDataAsset.ToSoftObjectPath().GetAssetName(),
                Entries[Index].bConsumed ? TEXT("consumed") : TEXT("available"));
        }

        UE_LOG(LogTemp, Log,
            TEXT("B7ClientValidation: [%s] Container %s Distance=%.1f Entries=%d %s"),
            PhaseTag,
            *GetNameSafe(Container),
            FVector::Dist(GetActorLocation(), Container->GetActorLocation()),
            Entries.Num(),
            *EntrySummary);
    }

    for (TActorIterator<AWTBRDroppedTriggerActor> It(World); It; ++It)
    {
        const AWTBRDroppedTriggerActor* Dropped = *It;
        if (!IsValid(Dropped))
        {
            continue;
        }

        const FVector ToActor = Dropped->GetActorLocation() - ViewLocation;
        UE_LOG(LogTemp, Log,
            TEXT("B7ClientValidation: [%s] DroppedTrigger %s Distance=%.1f ViewDot=%.2f IsConsumed=%s DataAsset=%s"),
            PhaseTag,
            *GetNameSafe(Dropped),
            FVector::Dist(GetActorLocation(), Dropped->GetActorLocation()),
            FVector::DotProduct(ToActor.GetSafeNormal(), ViewForward),
            Dropped->IsConsumed() ? TEXT("true") : TEXT("false"),
            *Dropped->GetDroppedTriggerDataAsset().ToSoftObjectPath().ToString());
    }

    for (TActorIterator<AWTBRGroundItemActor> It(World); It; ++It)
    {
        const AWTBRGroundItemActor* GroundItem = *It;
        if (!IsValid(GroundItem))
        {
            continue;
        }

        const FVector ToActor = GroundItem->GetActorLocation() - ViewLocation;
        UE_LOG(LogTemp, Log,
            TEXT("B7ClientValidation: [%s] GroundItem %s Distance=%.1f ViewDot=%.2f IsConsumed=%s Quantity=%d"),
            PhaseTag,
            *GetNameSafe(GroundItem),
            FVector::Dist(GetActorLocation(), GroundItem->GetActorLocation()),
            FVector::DotProduct(ToActor.GetSafeNormal(), ViewForward),
            GroundItem->IsConsumed() ? TEXT("true") : TEXT("false"),
            GroundItem->GetQuantity());
    }
}
#endif  // !UE_BUILD_SHIPPING
