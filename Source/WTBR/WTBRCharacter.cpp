// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRStaminaComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Components/WTBRCharacterMovementComponent.h"
#include "Components/WTBRInputGestureComponent.h"
#include "Components/WTBRInteractionComponent.h"
#include "Components/WTBRHUDViewModelComponent.h"
#include "Components/WTBRBagLootViewModelComponent.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Actors/WTBRNexilWireActor.h"
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
#include "UI/WTBRSniperScopeWidget.h"
#include "UI/WTBRTriggerWheelWidget.h"
#include "UI/WTBRMarkPingHUDWidget.h"
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "UI/WTBRInputBindingDisplayLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

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

AWTBRCharacter::AWTBRCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UWTBRCharacterMovementComponent>(
        ACharacter::CharacterMovementComponentName))
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
    GetCharacterMovement()->NavAgentProps.bCanCrouch    = true;
    GetCharacterMovement()->SetCrouchedHalfHeight(60.0f);
    GetCharacterMovement()->MaxWalkSpeedCrouched        = 300.0f;
    CrouchKey = EKeys::C;
    ProneKey = EKeys::Z;
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

    LacernSlashTrailEffect = TSoftObjectPtr<UNiagaraSystem>(
        FSoftObjectPath(TEXT("/Game/VFX/Generated/Lacern/NS_Lacern_Slash_VaelCyan_01.NS_Lacern_Slash_VaelCyan_01")));
    LacernHitImpactEffect = TSoftObjectPtr<UNiagaraSystem>(
        FSoftObjectPath(TEXT("/Game/VFX/Lacern/NS_Lacern_Hit_Impact_01.NS_Lacern_Hit_Impact_01")));
    VoltisTapLaunchEffect = TSoftObjectPtr<UNiagaraSystem>(
        FSoftObjectPath(TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_TapPrism_P02.NS_Voltis_TapPrism_P02")));
    VoltisAllyBoostEffect = TSoftObjectPtr<UNiagaraSystem>(
        FSoftObjectPath(TEXT("/Game/VFX/Voltis/Prototype/NS_Voltis_HoldPrism_P02.NS_Voltis_HoldPrism_P02")));

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

    static ConstructorHelpers::FObjectFinder<UInputAction> CrouchActionAsset(
        TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch"));
    if (CrouchActionAsset.Succeeded())
    {
        CrouchAction = CrouchActionAsset.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> ProneActionAsset(
        TEXT("/Game/Input/Actions/IA_Prone.IA_Prone"));
    if (ProneActionAsset.Succeeded())
    {
        ProneAction = ProneActionAsset.Object;
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
    StopNativeLacernTrail();
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
        if (IsValid(DodgeAction))
        {
            EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AWTBRCharacter::DodgeStarted);
            EIC->BindAction(DodgeAction, ETriggerEvent::Completed, this, &AWTBRCharacter::DodgeReleased);
            EIC->BindAction(DodgeAction, ETriggerEvent::Canceled, this, &AWTBRCharacter::DodgeReleased);
        }
        // Use exactly one binding path. The editable direct keys are enabled by
        // default; Enhanced Input is used after those fallbacks are disabled in
        // Class Defaults and the actions are mapped in the active IMC.
        if (!bUseDirectStanceKeyBindings && IsValid(CrouchAction))
        {
            EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &AWTBRCharacter::ToggleCrouch);
        }
        if (!bUseDirectStanceKeyBindings && IsValid(ProneAction))
        {
            EIC->BindAction(ProneAction, ETriggerEvent::Started, this, &AWTBRCharacter::ToggleProne);
        }

        // These editable fallback keys keep stance controls usable before their
        // Enhanced Input mappings are authored. Disable them in Class Defaults
        // once IA_Crouch / IA_Prone are mapped in the active IMC.
        if (bUseDirectStanceKeyBindings && CrouchKey.IsValid())
        {
            PlayerInputComponent->BindKey(CrouchKey, IE_Pressed, this, &AWTBRCharacter::ToggleCrouchKey);
        }
        if (bUseDirectStanceKeyBindings && ProneKey.IsValid())
        {
            PlayerInputComponent->BindKey(ProneKey, IE_Pressed, this, &AWTBRCharacter::ToggleProneKey);
        }
        if (IsValid(SwitchTriggerAction))
        {
            EIC->BindAction(SwitchTriggerAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchTrigger);
        }
        if (IsValid(SwitchMainAction))
        {
            EIC->BindAction(SwitchMainAction, ETriggerEvent::Completed, this, &AWTBRCharacter::SwitchMainTriggerReleased);
            EIC->BindAction(SwitchMainAction, ETriggerEvent::Canceled, this, &AWTBRCharacter::SwitchMainTriggerReleased);
            EIC->BindAction(SwitchMainAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchMainTrigger);
        }
        if (IsValid(SwitchSubAction))
        {
            EIC->BindAction(SwitchSubAction, ETriggerEvent::Completed, this, &AWTBRCharacter::SwitchSubTriggerReleased);
            EIC->BindAction(SwitchSubAction, ETriggerEvent::Canceled, this, &AWTBRCharacter::SwitchSubTriggerReleased);
            EIC->BindAction(SwitchSubAction, ETriggerEvent::Started, this, &AWTBRCharacter::SwitchSubTrigger);
        }
        if (IsValid(InteractAction))
        {
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AWTBRCharacter::Interact);
            EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &AWTBRCharacter::InteractReleased);
        }
        if (IsValid(MarkPingAction))
        {
            EIC->BindAction(MarkPingAction, ETriggerEvent::Started, this, &AWTBRCharacter::Input_RequestMarkPing);
        }
        if (bUseDirectMarkPingKeyBinding && MarkPingKey.IsValid())
        {
            PlayerInputComponent->BindKey(MarkPingKey, IE_Pressed, this, &AWTBRCharacter::Input_RequestMarkPingKey);
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

    // The selection wheel steers off the same mouse movement. Without this the
    // camera would swing around while the player is flicking to pick a Trigger,
    // which makes the wheel unusable and leaves the view somewhere unintended when
    // it closes.
    if (IsValid(TriggerWheelWidgetInstance) && TriggerWheelWidgetInstance->IsWheelOpen())
    {
        return;
    }

    // While a Sniper zoom has narrowed FollowCamera's FOV, scale turn input
    // down by the same ratio so degrees-turned-per-mouse-inch stays roughly
    // constant regardless of zoom depth. Without this, a heavily zoomed shot
    // (e.g. Egret's 25 deg vs a ~90 deg default) would swing the camera
    // across a much larger fraction of the narrow view for the same mouse
    // movement, making precision aiming harder instead of easier — this is
    // the standard ADS-sensitivity convention used across the genre. Reuses
    // the existing DefaultCameraFOV/FollowCamera state from the cosmetic
    // Sniper zoom (AWTBRCharacter::UpdateSniperZoom); no new state needed,
    // and this is a no-op (scale = 1.0) whenever FOV isn't zoomed.
    float SensitivityScale = 1.0f;
    if (FollowCamera && !FMath::IsNearlyZero(DefaultCameraFOV))
    {
        SensitivityScale = FollowCamera->FieldOfView / DefaultCameraFOV;
    }

    AddControllerYawInput(Axis.X * SensitivityScale);
    AddControllerPitchInput(Axis.Y * SensitivityScale);
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

    // Whatever Sniper is actually equipped in each slot right now,
    // independent of whether this press/release wants to zoom with it —
    // needed below to record a cooldown prediction on release even though
    // bMainWantsSniperZoom/bSubWantsSniperZoom just went false.
    UWTBRSniperTrigger* ActiveMainSniper = Cast<UWTBRSniperTrigger>(TriggerSetComponent->GetActiveMainTrigger());
    UWTBRSniperTrigger* ActiveSubSniper  = Cast<UWTBRSniperTrigger>(TriggerSetComponent->GetActiveSubTrigger());
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (bZoomIn)
    {
        const UWTBRSniperTrigger* SniperForSlot = bIsMain ? ActiveMainSniper : ActiveSubSniper;
        const float CooldownPredictedUntil = bIsMain
            ? MainSniperCooldownPredictedUntil
            : SubSniperCooldownPredictedUntil;
        // Scope is a promise that releasing Fire can make this shot. Mirror
        // the HUD's per-trigger effective-cost calculation here, so an
        // unaffordable Sniper (for example 10 Vael for a 20-Vael shot) never
        // enters scope in the first place.
        const bool bCanAffordSniper = bIsMain
            ? CanAffordActiveMainTriggerForHUD()
            : CanAffordActiveSubTriggerForHUD();
        const bool bPressAccepted = SniperForSlot
            && bCanAffordSniper
            && Now >= CooldownPredictedUntil;

        if (bIsMain)
        {
            bMainSniperZoomPressAccepted = bPressAccepted;
            bMainWantsSniperZoom = bPressAccepted;
        }
        else
        {
            bSubSniperZoomPressAccepted = bPressAccepted;
            bSubWantsSniperZoom = bPressAccepted;
        }
    }
    else if (bIsMain)
    {
        // Only an accepted scope press may project the active Sniper's
        // weapon-specific cooldown. A rejected tap must never extend it.
        if (bMainSniperZoomPressAccepted && ActiveMainSniper)
        {
            MainSniperCooldownPredictedUntil = Now + ActiveMainSniper->GetCooldownDurationForHUD();
        }
        bMainSniperZoomPressAccepted = false;
        bMainWantsSniperZoom = false;
    }
    else
    {
        if (bSubSniperZoomPressAccepted && ActiveSubSniper)
        {
            SubSniperCooldownPredictedUntil = Now + ActiveSubSniper->GetCooldownDurationForHUD();
        }
        bSubSniperZoomPressAccepted = false;
        bSubWantsSniperZoom = false;
    }

    // Only a real Sniper in the slot, wanted right now, AND not predicted to
    // still be on cooldown counts as "wants zoom" — grab both up front so
    // the rest of this function can just read them.
    UWTBRSniperTrigger* MainSniper = (bMainWantsSniperZoom && Now >= MainSniperCooldownPredictedUntil)
        ? ActiveMainSniper : nullptr;
    UWTBRSniperTrigger* SubSniper = (bSubWantsSniperZoom && Now >= SubSniperCooldownPredictedUntil)
        ? ActiveSubSniper : nullptr;

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

    // Camera position switches instantly (standard ADS convention); only the
    // FOV itself lerps via the timer below.
    SetSniperScopeView(MainSniper != nullptr || SubSniper != nullptr);

    GetWorldTimerManager().SetTimer(
        SniperZoomLerpTimer,
        this, &AWTBRCharacter::TickSniperZoomLerp,
        SNIPER_ZOOM_TICK_INTERVAL, true);
}

void AWTBRCharacter::SetSniperScopeView(bool bActive)
{
    if (bActive == bSniperScopeViewActive) return;
    if (!CameraBoom) return;

    if (bActive)
    {
        // Save whatever the Character BP actually authored so release
        // restores the player's real third-person framing, not C++ defaults.
        ScopeSavedArmLength               = CameraBoom->TargetArmLength;
        ScopeSavedSocketOffset            = CameraBoom->SocketOffset;
        ScopeSavedDoCollisionTest         = CameraBoom->bDoCollisionTest;
        ScopeSavedEnableCameraLag         = CameraBoom->bEnableCameraLag;
        ScopeSavedEnableCameraRotationLag = CameraBoom->bEnableCameraRotationLag;

        // Collapse to the existing pivot instead of moving it — the boom's
        // relative location is left exactly as the Character BP authored it,
        // only the arm length goes to zero. Collision test + lag are turned
        // off: a non-zero-length collision probe starting from inside the
        // character's own capsule was the likely cause of the camera ending
        // up facing the wrong way on the first attempt.
        CameraBoom->TargetArmLength          = 0.0f;
        CameraBoom->SocketOffset             = FVector::ZeroVector;
        CameraBoom->bDoCollisionTest         = false;
        CameraBoom->bEnableCameraLag         = false;
        CameraBoom->bEnableCameraRotationLag = false;

        // Hide only for the owning viewer — everyone else still sees the
        // character standing there aiming.
        if (USkeletalMeshComponent* CharMesh = GetMesh())
        {
            CharMesh->SetOwnerNoSee(true);
        }
    }
    else
    {
        CameraBoom->TargetArmLength          = ScopeSavedArmLength;
        CameraBoom->SocketOffset             = ScopeSavedSocketOffset;
        CameraBoom->bDoCollisionTest         = ScopeSavedDoCollisionTest;
        CameraBoom->bEnableCameraLag         = ScopeSavedEnableCameraLag;
        CameraBoom->bEnableCameraRotationLag = ScopeSavedEnableCameraRotationLag;

        if (USkeletalMeshComponent* CharMesh = GetMesh())
        {
            CharMesh->SetOwnerNoSee(false);
        }
    }
    bSniperScopeViewActive = bActive;
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

void AWTBRCharacter::DodgeStarted(const FInputActionValue& Value)
{
    if (CharacterStance != EWTBRCharacterStance::Standing || !GetWorld()) return;

    bDodgeButtonHeld = true;
    bDodgeHoldStartedSprint = false;
    GetWorldTimerManager().SetTimer(
        DodgeHoldTimer, this, &AWTBRCharacter::BeginVaelSprintFromDodgeHold,
        DODGE_HOLD_THRESHOLD, false);
}

void AWTBRCharacter::DodgeReleased(const FInputActionValue& Value)
{
    if (!GetWorld()) return;

    bDodgeButtonHeld = false;
    GetWorldTimerManager().ClearTimer(DodgeHoldTimer);
    if (bDodgeHoldStartedSprint)
    {
        bDodgeHoldStartedSprint = false;
        Server_StopVaelSprint();
        return;
    }

    // A tap sends exactly one request. The server validates stamina, stance,
    // cooldown and movement state before applying any velocity.
    Server_Dodge(GetClientMoveInputDirectionForTrigger());
}

void AWTBRCharacter::BeginVaelSprintFromDodgeHold()
{
    if (!bDodgeButtonHeld || CharacterStance != EWTBRCharacterStance::Standing) return;

    bDodgeHoldStartedSprint = true;
    Server_StartVaelSprint();
}

void AWTBRCharacter::ToggleCrouch(const FInputActionValue& Value)
{
    ToggleCrouchKey();
}

void AWTBRCharacter::ToggleCrouchKey()
{
    const EWTBRCharacterStance Desired = CharacterStance == EWTBRCharacterStance::Crouching
        ? EWTBRCharacterStance::Standing
        : EWTBRCharacterStance::Crouching;
    PredictNativeCrouchForStance(Desired);
    Server_SetCharacterStance(Desired);
}

void AWTBRCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    RefreshStanceViewOffsets();
}

void AWTBRCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    RefreshStanceViewOffsets();
}

void AWTBRCharacter::RefreshStanceViewOffsets()
{
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!IsValid(Capsule)) return;

    // How far the capsule centre currently sits below where it would be standing.
    const float Shrink = GetDefaultHalfHeight() - Capsule->GetUnscaledCapsuleHalfHeight();

    const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
    if (USkeletalMeshComponent* MeshComp = GetMesh();
        MeshComp && DefaultChar && DefaultChar->GetMesh())
    {
        // Lift the mesh by the same amount the capsule centre dropped, so the feet
        // stay on the ground. Assigned (not added) for the reason in the header.
        FVector MeshLocation = MeshComp->GetRelativeLocation();
        MeshLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z + Shrink;
        MeshComp->SetRelativeLocation(MeshLocation);
        BaseTranslationOffset.Z = MeshLocation.Z;
    }

    if (CameraBoom)
    {
        FVector BoomLocation = CameraBoom->GetRelativeLocation();
        BoomLocation.Z = Shrink * StanceCameraHeightCompensation;
        CameraBoom->SetRelativeLocation(BoomLocation);
    }
}

void AWTBRCharacter::SetWantsToProne(bool bNewWantsToProne)
{
    if (UWTBRCharacterMovementComponent* MoveComp =
            Cast<UWTBRCharacterMovementComponent>(GetCharacterMovement()))
    {
        MoveComp->bWantsToProne = bNewWantsToProne;
    }
}

void AWTBRCharacter::PredictNativeCrouchForStance(EWTBRCharacterStance Desired)
{
    // Owning clients must raise bWantsToCrouch themselves. It travels to the server
    // in the saved move's compressed flags, so both sides simulate the same crouch
    // state. Without it the client keeps predicting a standing capsule and speed and
    // gets corrected on every stance change — the rubber-banding seen in NetMode
    // Client. The authority already does this inside TrySetCharacterStance.
    if (HasAuthority() || !IsLocallyControlled()) return;

    // bWantsToProne is the predicted half: it rides to the server in the saved
    // move's compressed flags, so the client's own simulation shrinks the capsule
    // at the same point the server does instead of waiting for the replicated stance.
    SetWantsToProne(Desired == EWTBRCharacterStance::Prone);

    if (Desired == EWTBRCharacterStance::Standing)
    {
        UnCrouch(false);
    }
    else
    {
        Crouch(false);
    }
}

void AWTBRCharacter::ToggleProne(const FInputActionValue& Value)
{
    ToggleProneKey();
}

void AWTBRCharacter::ToggleProneKey()
{
    const EWTBRCharacterStance Desired = CharacterStance == EWTBRCharacterStance::Prone
        ? EWTBRCharacterStance::Standing
        : EWTBRCharacterStance::Prone;
    PredictNativeCrouchForStance(Desired);
    Server_SetCharacterStance(Desired);
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
    BeginTriggerSetSelect(true);
}

void AWTBRCharacter::SwitchMainTriggerReleased(const FInputActionValue& Value)
{
    EndTriggerSetSelect(true);
}

void AWTBRCharacter::SwitchSubTriggerReleased(const FInputActionValue& Value)
{
    EndTriggerSetSelect(false);
}

void AWTBRCharacter::OnMainWheelHoldElapsed()
{
    bTriggerWheelPendingIsMain = true;
    OpenTriggerWheel();
}

void AWTBRCharacter::OnSubWheelHoldElapsed()
{
    bTriggerWheelPendingIsMain = false;
    OpenTriggerWheel();
}

void AWTBRCharacter::BeginTriggerSetSelect(bool bIsMain)
{
    // The slot switch itself deliberately does NOT happen here any more. It moved to
    // release, because a press can still turn into a hold — cycling on press would
    // change the slot and then open the wheel on top of the slot it just left.
    bTriggerWheelPendingIsMain = bIsMain;

    if (!IsLocallyControlled())
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        TriggerWheelHoldTimer,
        this,
        bIsMain ? &AWTBRCharacter::OnMainWheelHoldElapsed : &AWTBRCharacter::OnSubWheelHoldElapsed,
        FMath::Max(TriggerWheelHoldThreshold, 0.05f),
        false);
}

void AWTBRCharacter::EndTriggerSetSelect(bool bIsMain)
{
    GetWorldTimerManager().ClearTimer(TriggerWheelHoldTimer);

    if (IsValid(TriggerWheelWidgetInstance) && TriggerWheelWidgetInstance->IsWheelOpen())
    {
        // INDEX_NONE means the player never flicked far enough, or aimed at an empty
        // slot — close without changing anything rather than guessing.
        const int32 ChosenSlot = TriggerWheelWidgetInstance->GetHighlightedSlotIndex();
        TriggerWheelWidgetInstance->CloseWheel();

        if (ChosenSlot != INDEX_NONE)
        {
            Server_SelectTriggerSlot(bIsMain, ChosenSlot);
        }
        return;
    }

    // Tap: the original cycle behaviour, now on release.
    Server_CycleTrigger(bIsMain);
}

void AWTBRCharacter::OpenTriggerWheel()
{
    if (!IsLocallyControlled() || !IsValid(TriggerWheelWidgetInstance))
    {
        return;
    }

    // Nothing to pick from while dead or hanging — and opening a wheel mid-zipline
    // would eat the release that is supposed to launch.
    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive() || bIsHangingOnNexilWire)
    {
        return;
    }

    TriggerWheelWidgetInstance->OpenWheel(bTriggerWheelPendingIsMain);
}

void AWTBRCharacter::Server_SelectTriggerSlot_Implementation(bool bIsMain, int32 SlotIndex)
{
    if (!HasAuthority() || !IsValid(TriggerSetComponent))
    {
        return;
    }

    // Re-validate rather than trusting the client: the index must exist, be occupied,
    // and belong to the set the player was actually holding.
    if (!TriggerSetComponent->IsSlotOccupied(SlotIndex))
    {
        return;
    }

    const bool bSlotIsMainSet = SlotIndex < UWTBRTriggerSetComponent::MainSlotCount;
    if (bSlotIsMainSet != bIsMain)
    {
        return;
    }

    if (bIsMain)
    {
        TriggerSetComponent->SwitchMainSlot(SlotIndex);
    }
    else
    {
        TriggerSetComponent->SwitchSubSlot(SlotIndex);
    }
}

// ─── Mark/Ping ──────────────────────────────────────────────────────────────

void AWTBRCharacter::Input_RequestMarkPing(const FInputActionValue& /*Value*/)
{
    RequestMarkPingFromLocalAim();
}

void AWTBRCharacter::Input_RequestMarkPingKey()
{
    RequestMarkPingFromLocalAim();
}

void AWTBRCharacter::RequestMarkPingFromLocalAim()
{
    FVector EyeLocation = FVector::ZeroVector;
    FRotator EyeRotation = FRotator::ZeroRotator;
    GetActorEyesViewPoint(EyeLocation, EyeRotation);

    const FVector AimDirection = EyeRotation.Vector().GetSafeNormal();
    if (AimDirection.IsNearlyZero())
    {
        return;
    }

    Server_RequestMarkPing(EyeLocation, AimDirection);
}

bool AWTBRCharacter::CanRequestMarkPingNow() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    return World->GetTimeSeconds() - LastMarkPingRequestTimeSeconds >= MarkPingCooldownSeconds;
}

bool AWTBRCharacter::ShouldTreatMarkPingHitAsEnemy(AActor* HitActor) const
{
    const AWTBRCharacter* HitCharacter = Cast<AWTBRCharacter>(HitActor);
    // Deliberately NOT gated on HitCharacter->HasTeam(): in a no-team/solo BR
    // match nobody has an assigned TeamId, and there everyone is hostile to
    // everyone — requiring the target to HasTeam() before calling it an enemy
    // silently vetoed every enemy ping in exactly that mode (owner-found PIE
    // bug, confirmed against a solo BR match: ALIVE 42/KILL 03, both characters
    // teamless). IsSameTeamAs already requires proving a matching assigned team
    // to call two characters friendly; anything that fails to prove that is
    // correctly hostile by default, teamless-vs-teamless included.
    return IsValid(HitCharacter) && HitCharacter != this
        && IsValid(HitCharacter->HealthComponent) && HitCharacter->HealthComponent->IsAlive()
        && !IsSameTeamAs(HitCharacter);
}

TArray<AWTBRCharacter*> AWTBRCharacter::GetMarkPingRecipients() const
{
    TArray<AWTBRCharacter*> Recipients;
    UWorld* World = GetWorld();
    if (!World)
    {
        return Recipients;
    }

    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        AWTBRCharacter* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        if (Candidate == this || IsSameTeamAs(Candidate))
        {
            Recipients.Add(Candidate);
        }
    }
    return Recipients;
}

void AWTBRCharacter::Server_RequestMarkPing_Implementation(
    FVector_NetQuantize TraceStart, FVector_NetQuantizeNormal TraceDirection)
{
    if (!CanRequestMarkPingNow())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector Direction = FVector(TraceDirection).GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        return;
    }

    const FVector Start(TraceStart);
    const FVector End = Start + Direction * MarkPingMaxRange;

    // ECC_Pawn, not ECC_Visibility: this codebase's character capsules deliberately
    // do NOT block Visibility (see e.g. WTBRNexilTrigger's wall-anchor placement
    // trace, which uses Visibility specifically so it passes through characters).
    // Pawn is the channel every other hit-detecting trace here uses (melee sweeps,
    // projectile overlaps) — world static geometry responds to it too, so location
    // pings against walls are unaffected.
    FHitResult Hit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WTBRMarkPingTrace), false);
    TraceParams.AddIgnoredActor(this);
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, TraceParams);

    AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
    const bool bIsEnemy = ShouldTreatMarkPingHitAsEnemy(HitActor);

    LastMarkPingRequestTimeSeconds = World->GetTimeSeconds();

    FWTBRMarkPingPayload Payload;
    Payload.Location = bHit ? Hit.ImpactPoint : End;
    Payload.MarkedActor = bIsEnemy ? HitActor : nullptr;
    Payload.bIsEnemy = bIsEnemy;
    Payload.Instigator = this;
    Payload.ServerTimeSeconds = LastMarkPingRequestTimeSeconds;

    // Written directly onto each recipient's own IncomingMarkPing (COND_OwnerOnly),
    // then applied immediately here too: real replication only calls OnRep on a
    // TRULY remote connection, never on the server's own local copy of an actor it
    // owns (relevant on a listen server, where the host's own character is one of
    // the recipients) — so the shared apply step has to run from both places.
    for (AWTBRCharacter* Recipient : GetMarkPingRecipients())
    {
        Recipient->IncomingMarkPing = Payload;
        Recipient->ApplyIncomingMarkPing();
    }
}

void AWTBRCharacter::OnRep_IncomingMarkPing()
{
    ApplyIncomingMarkPing();
}

void AWTBRCharacter::ApplyIncomingMarkPing()
{
    UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;

    ActiveMarkPings.RemoveAll([Now](const FWTBRActiveMarkPing& Ping)
    {
        return Ping.ExpireTimeSeconds <= Now;
    });

    FWTBRActiveMarkPing NewPing;
    NewPing.Location = IncomingMarkPing.Location;
    NewPing.MarkedActor = IncomingMarkPing.MarkedActor;
    NewPing.bIsEnemy = IncomingMarkPing.bIsEnemy;
    NewPing.Instigator = IncomingMarkPing.Instigator;
    NewPing.ExpireTimeSeconds = Now + MarkPingDisplayDuration;

    constexpr int32 MaxActiveMarkPings = 6;
    if (ActiveMarkPings.Num() >= MaxActiveMarkPings)
    {
        ActiveMarkPings.RemoveAt(0);
    }
    ActiveMarkPings.Add(NewPing);

    WTBR_VALIDATION_LOG(Log,
        TEXT("[MarkPing] ApplyIncomingMarkPing | Recipient=%s | Location=%s | bIsEnemy=%s | MarkedActor=%s | ActiveCount=%d"),
        *GetNameSafe(this), *NewPing.Location.ToString(), NewPing.bIsEnemy ? TEXT("true") : TEXT("false"),
        *GetNameSafe(IncomingMarkPing.MarkedActor.Get()), ActiveMarkPings.Num());
}

void AWTBRCharacter::SwitchSubTrigger(const FInputActionValue& Value)
{
    BeginTriggerSetSelect(false);
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

AWTBRCharacter* AWTBRCharacter::FindBestFriendlyTarget(
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
        // Only difference from FindBestHomingTarget: require SAME team instead of
        // excluding it (also excludes self via IsSameTeamAs never matching itself
        // being redundant with the explicit Candidate == QueryingCharacter check).
        if (!IsValid(Candidate) || Candidate == QueryingCharacter ||
            !IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsAlive() ||
            !QueryingCharacter->IsSameTeamAs(Candidate))
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
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WTBRFriendlyTargetLOS), false);
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

namespace
{
    // Sane server-side re-validation range for a Nexil zipline grab — client
    // focus (UWTBRInteractionComponent::GetFocusedNexilWire) is advisory only.
    constexpr float WTBRNexilZiplineGrabRange = 500.0f;
}

void AWTBRCharacter::Server_GrabNexilWire_Implementation(AWTBRNexilWireActor* Wire)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Nexil zipline grab rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Nexil zipline grab rejected: character %s is not alive"),
            *GetNameSafe(this));
        return;
    }

    if (bIsHangingOnNexilWire)
    {
        return;
    }

    if (!IsValid(Wire) || !Wire->CanBeGrabbedBy(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Nexil zipline grab rejected: wire invalid or not grabbable by character %s"),
            *GetNameSafe(this));
        return;
    }

    if (FVector::Dist(GetActorLocation(), Wire->GetActorLocation()) > WTBRNexilZiplineGrabRange)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Nexil zipline grab rejected: character %s too far from wire %s"),
            *GetNameSafe(this),
            *GetNameSafe(Wire));
        return;
    }

    bIsHangingOnNexilWire = true;
    HangingNexilWire = Wire;

    // A wire can expire (WireDuration) or be cut while someone is hanging on it —
    // drop the rider instead of leaving them stuck in MOVE_Flying in mid-air.
    Wire->OnDestroyed.AddDynamic(this, &AWTBRCharacter::HandleHangingNexilWireDestroyed);

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Flying);
        MoveComp->StopMovementImmediately();
    }
    SetActorLocation(Wire->GetActorLocation());
}

void AWTBRCharacter::HandleHangingNexilWireDestroyed(AActor* /*DestroyedActor*/)
{
    if (!bIsHangingOnNexilWire)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR Nexil zipline: wire destroyed while %s was hanging — dropping"),
        *GetNameSafe(this));

    // No launch: the wire is gone, so there is nothing to push off from.
    EndNexilWireHang(false);
}

void AWTBRCharacter::EndNexilWireHang(bool bLaunch)
{
    if (!bIsHangingOnNexilWire)
    {
        return;
    }

    const float LaunchSpeed = (bLaunch && HangingNexilWire.IsValid())
        ? HangingNexilWire->GetZiplineLaunchSpeed()
        : 0.0f;

    if (HangingNexilWire.IsValid())
    {
        HangingNexilWire->OnDestroyed.RemoveDynamic(
            this, &AWTBRCharacter::HandleHangingNexilWireDestroyed);
    }

    bIsHangingOnNexilWire = false;
    HangingNexilWire = nullptr;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Falling);
    }

    if (LaunchSpeed > 0.0f)
    {
        // Read fresh at release time (server-authoritative control rotation), not
        // whatever direction was aimed at grab time — same convention as Sniper
        // aim-then-fire (WTBRFulgrisTrigger/Piercex/Telorn ExecuteFire()).
        const FVector LaunchDir = GetControlRotation().Vector();
        LaunchCharacter(LaunchDir * LaunchSpeed, true, true);
    }
}

void AWTBRCharacter::Server_ReleaseNexilWireAndLaunch_Implementation()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Nexil zipline release rejected: character %s has no authority"),
            *GetNameSafe(this));
        return;
    }

    // Wire persists (reusable) — this only ends the hang, never touches the wire.
    EndNexilWireHang(true);
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

    // Nexil zipline: no Trigger use while hanging on a wire (release via Jump
    // first). Fire works normally again the instant the hang ends.
    if (bIsHangingOnNexilWire) return;

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

void AWTBRCharacter::Server_Dodge_Implementation(FVector ClientMoveInputDir)
{
    if (!HasAuthority() || bDodgeOnCooldown) return;
    if (CharacterStance != EWTBRCharacterStance::Standing) return;
    if (bIsStaggered || bIsHangingOnNexilWire) return;
    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive()) return;

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!IsValid(MoveComp) || !MoveComp->IsMovingOnGround()) return;
    if (!IsValid(StaminaComponent) || !StaminaComponent->TryConsumeDodgeStamina()) return;

    if (IsValid(MovementExtComponent))
    {
        MovementExtComponent->StopVaelSprint();
    }

    FVector DodgeDirection = SanitizeClientMoveInputDirection(ClientMoveInputDir);
    if (DodgeDirection.IsNearlyZero())
    {
        DodgeDirection = GetControlRotation().Vector().GetSafeNormal2D();
    }
    if (DodgeDirection.IsNearlyZero()) return;

    bDodgeOnCooldown = true;
    GetWorldTimerManager().SetTimer(
        DodgeCooldownTimer, this, &AWTBRCharacter::EndDodgeCooldown,
        DODGE_COOLDOWN, false);

    // Health is authoritative too, so the immunity window cannot be forged by
    // a client and every damage path using ApplyDamage observes the same frame.
    HealthComponent->StartDodgeIFrame(DodgeIFrameDuration);
    LaunchCharacter(DodgeDirection * DODGE_SPEED, true, false);
    Multicast_PlayDodgeCosmetic(DodgeDirection);
}

void AWTBRCharacter::Multicast_PlayDodgeCosmetic_Implementation(
    FVector_NetQuantizeNormal Direction)
{
    OnDodgeStarted(Direction);
}

void AWTBRCharacter::EndDodgeCooldown()
{
    bDodgeOnCooldown = false;
}

void AWTBRCharacter::Server_SetCharacterStance_Implementation(
    EWTBRCharacterStance NewStance)
{
    TrySetCharacterStance(NewStance);
}

bool AWTBRCharacter::TrySetCharacterStance(EWTBRCharacterStance NewStance)
{
    if (!HasAuthority() || NewStance == CharacterStance) return false;
    if (bIsStaggered || bIsHangingOnNexilWire) return false;
    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive()) return false;

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!IsValid(MoveComp) || !IsValid(Capsule)) return false;

    if (IsValid(MovementExtComponent))
    {
        MovementExtComponent->StopVaelSprint();
    }

    // ACharacter::Crouch/UnCrouch only raise bWantsToCrouch; bIsCrouched does not
    // flip until the next character-movement update. Testing bIsCrouched right
    // after calling them therefore always failed on the first press, which is why
    // every stance change used to need the key pressed twice — and why the stance
    // never reached the code below that applies its walk speed. Gate on what can be
    // known *now* (headroom / CanCrouchInCurrentState) and let the movement
    // component apply the capsule change on its own schedule.
    const float CrouchedHalfHeight = MoveComp->GetCrouchedHalfHeight();
    switch (NewStance)
    {
    case EWTBRCharacterStance::Standing:
        // Refuse to stand up under a ceiling instead of committing the stance and
        // letting the movement component silently keep the character crouched.
        if (!CanResizeCapsuleTo(GetDefaultHalfHeight())) return false;
        SetWantsToProne(false);
        UnCrouch(false);          // lowers bWantsToCrouch
        MoveComp->UnCrouch(false); // ...and applies it this frame
        if (bIsCrouched) return false;
        break;

    case EWTBRCharacterStance::Crouching:
        // Leaving prone only needs bWantsToProne lowered — the movement component
        // grows the capsule back to crouched height on its next update.
        SetWantsToProne(false);
        if (CharacterStance != EWTBRCharacterStance::Prone)
        {
            if (!MoveComp->CanCrouchInCurrentState()) return false;
            Crouch(false);
            MoveComp->Crouch(false);
            if (!bIsCrouched) return false;
        }
        break;

    case EWTBRCharacterStance::Prone:
        if (!MoveComp->CanCrouchInCurrentState()) return false;
        // The capsule itself is no longer touched here. Raising bWantsToProne hands
        // the change to UWTBRCharacterMovementComponent, which applies it inside the
        // movement update so client and server produce the same capsule from the
        // same predicted move (see that class for why the old direct resize jittered).
        SetWantsToProne(true);
        break;

    default:
        return false;
    }

    CharacterStance = NewStance;
    LastAppliedStance = NewStance;
    RefreshStanceSpeeds();
    OnCharacterStanceChanged(CharacterStance);
    ForceNetUpdate();
    return true;
}

bool AWTBRCharacter::CanResizeCapsuleTo(float NewHalfHeight) const
{
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!IsValid(Capsule) || NewHalfHeight <= Capsule->GetUnscaledCapsuleHalfHeight()) return true;
    if (!GetWorld()) return false;

    const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    const FVector TargetLocation = GetActorLocation()
        + FVector(0.0f, 0.0f, NewHalfHeight - CurrentHalfHeight);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WTBRStanceExpansion), false, this);
    return !GetWorld()->OverlapBlockingTestByChannel(
        TargetLocation,
        GetActorQuat(),
        Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Capsule->GetUnscaledCapsuleRadius(), NewHalfHeight),
        Params);
}

void AWTBRCharacter::SetCapsuleHalfHeightKeepingBase(float NewHalfHeight, bool bKeepFeetPlanted)
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!IsValid(Capsule)) return;

    const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    if (FMath::IsNearlyEqual(CurrentHalfHeight, NewHalfHeight)) return;

    if (!bKeepFeetPlanted)
    {
        // Replicated path: resize only. The server already moved the actor and that
        // location is what replication delivers.
        Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
        return;
    }

    const FVector PreviousLocation = GetActorLocation();
    const FVector NewLocation = PreviousLocation + FVector(0.0f, 0.0f,
        NewHalfHeight - CurrentHalfHeight);
    Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
    // The old unswept move let a prone capsule pass through floors and props.
    // Retain the foot position but respect blocking geometry; if the movement
    // cannot be completed, restore the previous collision shape and location.
    if (!SetActorLocation(NewLocation, true))
    {
        Capsule->SetCapsuleHalfHeight(CurrentHalfHeight, true);
        SetActorLocation(PreviousLocation, false);
    }
}

void AWTBRCharacter::ApplyReplicatedStance()
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!IsValid(MoveComp)) return;

    // The prone capsule is no longer touched here. It is a predicted movement state
    // now (UWTBRCharacterMovementComponent), so every machine derives it from the
    // movement simulation. Simulated proxies get it from bWantsToProne below; the
    // owning client already predicted it. Resizing here as well is what previously
    // double-applied the offset and sank the character through the floor.
    if (!IsLocallyControlled())
    {
        // Simulated proxies do not run the prediction path, so mirror the intent
        // from the replicated stance and let the movement component do the resize.
        SetWantsToProne(CharacterStance == EWTBRCharacterStance::Prone);
    }

    LastAppliedStance = CharacterStance;
    RefreshStanceSpeeds();
}

void AWTBRCharacter::RefreshStanceSpeeds()
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!IsValid(MoveComp)) return;

    float Speed = IsValid(MovementExtComponent)
        ? MovementExtComponent->GetNonSprintingSpeed()
        : 600.0f;

    // Sprinting is a standing-only ability — crouch/prone must not inherit it.
    // Read from the movement component's predicted state for the same reason the
    // stance multiplier moved to GetMaxSpeed: CharacterStance lags on the client.
    const UWTBRCharacterMovementComponent* WTBRMove =
        Cast<UWTBRCharacterMovementComponent>(MoveComp);
    const bool bGroundedStance =
        WTBRMove ? (WTBRMove->IsCrouching() || WTBRMove->IsProne())
                 : (CharacterStance != EWTBRCharacterStance::Standing);

    if (IsValid(MovementExtComponent)
        && MovementExtComponent->IsSprinting()
        && !bGroundedStance)
    {
        Speed *= MovementExtComponent->GetSprintSpeedMultiplier();
    }

    // The crouch/prone multiplier is deliberately NOT applied here. It lives in
    // UWTBRCharacterMovementComponent::GetMaxSpeed, keyed off the predicted capsule
    // state — this function runs off the replicated CharacterStance, which reaches
    // the owning client a round trip after it has already predicted the crouch.
    // Applying it here meant the client simulated at standing speed during that
    // window while the server used the crouch speed, so the client ran ahead and was
    // corrected back a step at a time.
    //
    // Both fields get the same unmultiplied base: the movement component reads
    // MaxWalkSpeedCrouched while crouched and MaxWalkSpeed otherwise, and GetMaxSpeed
    // scales whichever one it picked.
    MoveComp->MaxWalkSpeed = Speed;
    MoveComp->MaxWalkSpeedCrouched = Speed;
}

bool AWTBRCharacter::CanJumpInternal_Implementation() const
{
    // Crouch-jump is standard shooter movement, so Crouching is allowed here and
    // Jump() stands the character up alongside it. Prone stays blocked: every
    // comparable game requires standing (or crouching) first, and allowing it
    // would invite prone-jump spam.
    // Deliberately does NOT depend on the stand-up having already applied — the
    // jump must work on the pressed frame even if there is no headroom to stand.
    return CharacterStance != EWTBRCharacterStance::Prone
        && Super::CanJumpInternal_Implementation();
}

void AWTBRCharacter::Jump()
{
    if (CharacterStance == EWTBRCharacterStance::Crouching)
    {
        // Best-effort: silently stays crouched (and still jumps) when there is no
        // headroom, which is what a low ceiling should do.
        if (HasAuthority())
        {
            TrySetCharacterStance(EWTBRCharacterStance::Standing);
        }
        else
        {
            Server_SetCharacterStance(EWTBRCharacterStance::Standing);
        }
    }

    Super::Jump();
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

float AWTBRCharacter::GetSniperZoomAlphaForHUD() const
{
    if (!FollowCamera) return 0.0f;

    const float Range = DefaultCameraFOV - SniperZoomTargetFOV;
    if (FMath::IsNearlyZero(Range)) return 0.0f;

    const float Alpha = (DefaultCameraFOV - FollowCamera->FieldOfView) / Range;
    return FMath::Clamp(Alpha, 0.0f, 1.0f);
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
            // Deliberately NOT calling SetPositionInViewport here: UGameViewportSubsystem::
            // SetWidgetSlotPosition (which it calls into) unconditionally resets Anchors to
            // FAnchors(0,0) — a single-point, non-stretched anchor — discarding the full-stretch
            // anchors just set above. For a full-stretch widget with zero offsets, the anchors +
            // alignment already fully define "fill the screen"; Position has no meaning here and
            // calling it collapses the widget to its own (zero, for an empty SBox) desired size.
            // Found 2026-07-17 while root-causing why WTBRSniperScopeWidget's NativePaint always
            // saw a 0x0 AllottedGeometry — this widget had the exact same latent bug, just masked
            // because its NativePaint draws fixed-size boxes unconditionally instead of gating on
            // AllottedGeometry's size.
        }
    }

    if (!IsValid(ScopeWidgetInstance))
    {
        ScopeWidgetInstance = CreateWidget<UWTBRSniperScopeWidget>(PC, UWTBRSniperScopeWidget::StaticClass());
        if (IsValid(ScopeWidgetInstance))
        {
            // Above the Radar (1) but below the modal BagLoot layer (10) —
            // purely a fullscreen cosmetic overlay, draws nothing while not
            // aiming a Sniper (see UWTBRSniperScopeWidget::NativePaint).
            ScopeWidgetInstance->AddToViewport(2);
            ScopeWidgetInstance->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            ScopeWidgetInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
            // Deliberately NOT calling SetPositionInViewport — see the matching comment on
            // RadarWidgetInstance above. This was the actual root cause of the Scope overlay
            // never rendering: SetPositionInViewport unconditionally resets Anchors to a
            // non-stretched FAnchors(0,0) inside UGameViewportSubsystem::SetWidgetSlotPosition,
            // which combined with this widget's empty SBox (zero desired size) collapsed
            // AllottedGeometry to 0x0 in NativePaint every frame — confirmed against the UE 5.1
            // engine source, not just inferred from symptoms.
        }
    }

    if (!IsValid(TriggerWheelWidgetInstance))
    {
        TriggerWheelWidgetInstance =
            CreateWidget<UWTBRTriggerWheelWidget>(PC, UWTBRTriggerWheelWidget::StaticClass());
        if (IsValid(TriggerWheelWidgetInstance))
        {
            // Above the Scope (2) so the wheel is never hidden behind it, still under
            // the modal BagLoot layer (10). Same full-stretch anchoring, and the same
            // deliberate omission of SetPositionInViewport as the two widgets above.
            TriggerWheelWidgetInstance->AddToViewport(3);
            TriggerWheelWidgetInstance->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            TriggerWheelWidgetInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
            // Starts hidden; OpenWheel makes it visible. HitTestInvisible when shown, so
            // it never steals clicks from gameplay.
            TriggerWheelWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (!IsValid(MarkPingHUDWidgetInstance))
    {
        MarkPingHUDWidgetInstance = CreateWidget<UWTBRMarkPingHUDWidget>(PC, UWTBRMarkPingHUDWidget::StaticClass());
        if (IsValid(MarkPingHUDWidgetInstance))
        {
            // Above Radar/Scope/TriggerWheel (1/2/3) so a mark stays visible while
            // aiming or picking a Trigger, below the modal BagLoot layer (10) — a
            // fullscreen cosmetic overlay, same full-stretch anchoring/omitted-
            // SetPositionInViewport pattern as those three widgets.
            MarkPingHUDWidgetInstance->AddToViewport(4);
            MarkPingHUDWidgetInstance->SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            MarkPingHUDWidgetInstance->SetAlignmentInViewport(FVector2D::ZeroVector);
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

    if (IsValid(ScopeWidgetInstance))
    {
        ScopeWidgetInstance->RemoveFromParent();
    }
    ScopeWidgetInstance = nullptr;

    if (IsValid(TriggerWheelWidgetInstance))
    {
        TriggerWheelWidgetInstance->RemoveFromParent();
    }
    TriggerWheelWidgetInstance = nullptr;

    if (IsValid(MarkPingHUDWidgetInstance))
    {
        MarkPingHUDWidgetInstance->RemoveFromParent();
    }
    MarkPingHUDWidgetInstance = nullptr;

    // The hold watch outlives the widget otherwise and would fire into a dead pointer.
    GetWorldTimerManager().ClearTimer(TriggerWheelHoldTimer);
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
    DOREPLIFETIME(AWTBRCharacter, CharacterStance);
    DOREPLIFETIME(AWTBRCharacter, bIsHangingOnNexilWire);
    DOREPLIFETIME(AWTBRCharacter, bSerpveilChargeTelegraphActive);
    DOREPLIFETIME(AWTBRCharacter, bLacernExtendTelegraphActive);
    DOREPLIFETIME(AWTBRCharacter, bLacernExtendDualWield);
    DOREPLIFETIME_CONDITION(AWTBRCharacter, IncomingMarkPing, COND_OwnerOnly);
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

void AWTBRCharacter::OnRep_bIsHangingOnNexilWire()
{
    OnHangingOnNexilWireChanged(bIsHangingOnNexilWire);
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
    NativeHandleLacernExtendTelegraphChanged(bLacernExtendTelegraphActive, bLacernExtendDualWield);
    OnLacernExtendTelegraphChanged(bLacernExtendTelegraphActive, bLacernExtendDualWield);
}

void AWTBRCharacter::Multicast_LacernHit_Implementation(FVector ImpactPoint, FVector ImpactNormal, bool bDualWieldHit)
{
    NativeHandleLacernHitReceived(ImpactPoint, ImpactNormal, bDualWieldHit);
    OnLacernHitReceived(ImpactPoint, ImpactNormal, bDualWieldHit);
}

void AWTBRCharacter::Multicast_VoltisBurst_Implementation(
    AWTBRCharacter* TargetCharacter,
    bool bIsAllyBoost,
    FVector Direction)
{
    NativeHandleVoltisBurst(TargetCharacter, bIsAllyBoost, Direction);
}

float AWTBRCharacter::ComputeNativeLacernBladeTipDistance(
    float Elapsed,
    float ExtendLength,
    float ExtendSpeed,
    float RetractSpeed)
{
    if (ExtendLength <= 0.0f || ExtendSpeed <= 0.0f || RetractSpeed <= 0.0f)
    {
        return 0.0f;
    }

    const float ExtendDuration = ExtendLength / ExtendSpeed;
    if (Elapsed <= ExtendDuration)
    {
        return FMath::Clamp(Elapsed * ExtendSpeed, 0.0f, ExtendLength);
    }

    const float RetractElapsed = Elapsed - ExtendDuration;
    return FMath::Clamp(ExtendLength - RetractElapsed * RetractSpeed, 0.0f, ExtendLength);
}

void AWTBRCharacter::GetNativeLacernMotionParams(
    float& OutExtendLength,
    float& OutExtendSpeed,
    float& OutRetractSpeed,
    float& OutDualOffset) const
{
    OutExtendLength = 400.0f;
    OutExtendSpeed = 1200.0f;
    OutRetractSpeed = 1800.0f;
    OutDualOffset = 40.0f;

    const UWTBRTriggerBase* LacernTrigger = nullptr;
    if (IsValid(TriggerSetComponent))
    {
        if (const UWTBRTriggerBase* MainTrigger = TriggerSetComponent->GetActiveMainTrigger();
            IsValid(MainTrigger) && MainTrigger->IsA<UWTBRLacernTrigger>())
        {
            LacernTrigger = MainTrigger;
        }
        else if (const UWTBRTriggerBase* SubTrigger = TriggerSetComponent->GetActiveSubTrigger();
            IsValid(SubTrigger) && SubTrigger->IsA<UWTBRLacernTrigger>())
        {
            LacernTrigger = SubTrigger;
        }
    }

    const UWTBRTriggerDataAsset* LacernDataAsset =
        IsValid(LacernTrigger) ? LacernTrigger->DataAsset.Get() : nullptr;
    if (!IsValid(LacernDataAsset))
    {
        return;
    }

    OutExtendLength = FMath::Max(1.0f, LacernDataAsset->LacernParams.ExtendLength);
    OutExtendSpeed = FMath::Max(1.0f, LacernDataAsset->LacernParams.ExtendSpeed);
    OutRetractSpeed = FMath::Max(1.0f, LacernDataAsset->LacernParams.RetractSpeed);
    OutDualOffset = FMath::Max(0.0f, LacernDataAsset->MeleeHitbox.DualWieldLateralOffset);
}

USceneComponent* AWTBRCharacter::ResolveNativeLacernTrailAttachParent(
    FName& OutAttachSocketName) const
{
    OutAttachSocketName = NAME_None;

    TArray<USceneComponent*> SceneComponents;
    GetComponents<USceneComponent>(SceneComponents);
    for (USceneComponent* Component : SceneComponents)
    {
        if (IsValid(Component) && Component->GetFName() == LacernBladeTipMarkerName)
        {
            return Component;
        }
    }

    USkeletalMeshComponent* CharacterMesh = GetMesh();
    if (IsValid(CharacterMesh))
    {
        const FName SocketCandidates[] =
        {
            LacernBladeTipMarkerName,
            LacernWeaponSocketName,
            FName(TEXT("WeaponSocket")),
            FName(TEXT("hand_r"))
        };

        for (const FName SocketName : SocketCandidates)
        {
            if (!SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
            {
                OutAttachSocketName = SocketName;
                return CharacterMesh;
            }
        }
    }

    return RootComponent;
}

USceneComponent* AWTBRCharacter::EnsureNativeLacernBladeTipMarker()
{
    FName AttachSocketName = NAME_None;
    USceneComponent* AttachParent = ResolveNativeLacernTrailAttachParent(AttachSocketName);
    if (!IsValid(AttachParent))
    {
        return nullptr;
    }

    if (AttachParent->GetFName() == LacernBladeTipMarkerName)
    {
        return AttachParent;
    }

    if (!IsValid(NativeLacernBladeTipMarker))
    {
        NativeLacernBladeTipMarker = NewObject<USceneComponent>(this, TEXT("NativeLacernBladeTipMarker"));
        if (!IsValid(NativeLacernBladeTipMarker))
        {
            return nullptr;
        }
        NativeLacernBladeTipMarker->RegisterComponent();
    }

    if (NativeLacernBladeTipMarker->GetAttachParent() != AttachParent
        || NativeLacernBladeTipMarker->GetAttachSocketName() != AttachSocketName)
    {
        NativeLacernBladeTipMarker->AttachToComponent(
            AttachParent,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            AttachSocketName);
    }

    return NativeLacernBladeTipMarker;
}

void AWTBRCharacter::ApplyNativeLacernTrailParameters(
    UNiagaraComponent* NiagaraComponent,
    float CurrentDist,
    float MaxDist) const
{
    if (!IsValid(NiagaraComponent))
    {
        return;
    }

    const FLinearColor VaelCyan = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("27D8FF")));
    const FLinearColor HotWhite = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F2F5FF")));

    NiagaraComponent->SetVariableLinearColor(FName(TEXT("User.Color")), VaelCyan);
    NiagaraComponent->SetVariableLinearColor(FName(TEXT("User.HotCoreColor")), HotWhite);
    NiagaraComponent->SetVariableFloat(FName(TEXT("User.SlashWidth")), LacernSlashWidth);
    NiagaraComponent->SetVariableFloat(FName(TEXT("User.TrailLifetime")), LacernTrailLifetime);
    NiagaraComponent->SetVariableFloat(FName(TEXT("User.CurrentDist")), CurrentDist);
    NiagaraComponent->SetVariableFloat(FName(TEXT("User.MaxDist")), MaxDist);
}

void AWTBRCharacter::NativeHandleLacernExtendTelegraphChanged(
    bool bActive,
    bool bIsDualWield)
{
    UWorld* World = GetWorld();
    if (!bUseBuiltInLacernVFX || !World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!bActive)
    {
        StopNativeLacernTrail();
        return;
    }

    StopNativeLacernTrail();

    UNiagaraSystem* TrailSystem = LacernSlashTrailEffect.LoadSynchronous();
    USceneComponent* BladeTipMarker = EnsureNativeLacernBladeTipMarker();
    if (!IsValid(TrailSystem) || !IsValid(BladeTipMarker))
    {
        WTBR_VALIDATION_LOG(Warning,
            TEXT("[Lacern VFX] SlashTrailSpawnSkipped | Owner=%s | TrailValid=%s | MarkerValid=%s"),
            *GetNameSafe(this),
            IsValid(TrailSystem) ? TEXT("true") : TEXT("false"),
            IsValid(BladeTipMarker) ? TEXT("true") : TEXT("false"));
        return;
    }

    float ExtendLength = 400.0f;
    float ExtendSpeed = 1200.0f;
    float RetractSpeed = 1800.0f;
    float DualOffset = 40.0f;
    GetNativeLacernMotionParams(ExtendLength, ExtendSpeed, RetractSpeed, DualOffset);

    NativeLacernTrailElapsed = 0.0f;
    bNativeLacernTrailDualWield = bIsDualWield;
    BladeTipMarker->SetRelativeLocation(FVector::ZeroVector);

    NativeLacernSlashTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
        TrailSystem,
        BladeTipMarker,
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        false);
    ApplyNativeLacernTrailParameters(NativeLacernSlashTrail, 0.0f, ExtendLength);

    if (bIsDualWield)
    {
        NativeLacernDualSlashTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
            TrailSystem,
            BladeTipMarker,
            NAME_None,
            FVector(0.0f, DualOffset, 0.0f),
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            false);
        ApplyNativeLacernTrailParameters(NativeLacernDualSlashTrail, 0.0f, ExtendLength);
    }

    World->GetTimerManager().SetTimer(
        NativeLacernTrailTimer,
        this,
        &AWTBRCharacter::TickNativeLacernTrail,
        0.016f,
        true);
}

void AWTBRCharacter::TickNativeLacernTrail()
{
    UWorld* World = GetWorld();
    USceneComponent* BladeTipMarker = EnsureNativeLacernBladeTipMarker();
    if (!World || !IsValid(BladeTipMarker))
    {
        StopNativeLacernTrail();
        return;
    }

    float ExtendLength = 400.0f;
    float ExtendSpeed = 1200.0f;
    float RetractSpeed = 1800.0f;
    float DualOffset = 40.0f;
    GetNativeLacernMotionParams(ExtendLength, ExtendSpeed, RetractSpeed, DualOffset);

    NativeLacernTrailElapsed += 0.016f;
    const float CurrentDist = ComputeNativeLacernBladeTipDistance(
        NativeLacernTrailElapsed, ExtendLength, ExtendSpeed, RetractSpeed);
    BladeTipMarker->SetRelativeLocation(FVector(CurrentDist, 0.0f, 0.0f));

    ApplyNativeLacernTrailParameters(NativeLacernSlashTrail, CurrentDist, ExtendLength);
    ApplyNativeLacernTrailParameters(NativeLacernDualSlashTrail, CurrentDist, ExtendLength);

    const float TotalDuration = ExtendLength / ExtendSpeed + ExtendLength / RetractSpeed;
    if (NativeLacernTrailElapsed >= TotalDuration)
    {
        StopNativeLacernTrail();
    }
}

void AWTBRCharacter::StopNativeLacernTrail()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NativeLacernTrailTimer);
    }

    auto StopComponent = [](TObjectPtr<UNiagaraComponent>& Component)
    {
        if (IsValid(Component))
        {
            Component->Deactivate();
            Component->SetAutoDestroy(true);
        }
        Component = nullptr;
    };

    StopComponent(NativeLacernSlashTrail);
    StopComponent(NativeLacernDualSlashTrail);

    if (IsValid(NativeLacernBladeTipMarker))
    {
        NativeLacernBladeTipMarker->SetRelativeLocation(FVector::ZeroVector);
    }
    NativeLacernTrailElapsed = 0.0f;
    bNativeLacernTrailDualWield = false;
}

void AWTBRCharacter::NativeHandleLacernHitReceived(
    FVector ImpactPoint,
    FVector ImpactNormal,
    bool bDualWieldHit)
{
    UWorld* World = GetWorld();
    if (!bUseBuiltInLacernVFX || !World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    UNiagaraSystem* ImpactSystem = LacernHitImpactEffect.LoadSynchronous();
    if (!IsValid(ImpactSystem))
    {
        return;
    }

    const FVector SafeNormal = ImpactNormal.IsNearlyZero()
        ? FVector::UpVector : ImpactNormal.GetSafeNormal();
    UNiagaraComponent* ImpactComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        ImpactSystem,
        ImpactPoint + SafeNormal * 1.5f,
        FRotationMatrix::MakeFromZ(SafeNormal).Rotator(),
        bDualWieldHit ? FVector(1.12f) : FVector::OneVector,
        true,
        true,
        ENCPoolMethod::AutoRelease,
        true);

    ApplyNativeLacernTrailParameters(ImpactComponent, 0.0f, 1.0f);
}

void AWTBRCharacter::NativeHandleVoltisBurst(
    AWTBRCharacter* TargetCharacter,
    bool bIsAllyBoost,
    FVector Direction)
{
    UWorld* World = GetWorld();
    AWTBRCharacter* EffectTarget = IsValid(TargetCharacter) ? TargetCharacter : this;
    if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(EffectTarget))
    {
        return;
    }

    UNiagaraSystem* Effect = (bIsAllyBoost ? VoltisAllyBoostEffect : VoltisTapLaunchEffect).LoadSynchronous();
    if (!IsValid(Effect))
    {
        return;
    }

    UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        Effect,
        EffectTarget->GetActorLocation(),
        Direction.IsNearlyZero() ? EffectTarget->GetActorRotation() : Direction.Rotation(),
        FVector::OneVector,
        true,
        true,
        ENCPoolMethod::AutoRelease,
        true);
    if (!IsValid(Component))
    {
        return;
    }

    // The prototype template loops for easy authoring previews. Gameplay bursts
    // deactivate explicitly so a dash cannot leave a permanent plume behind.
    const TWeakObjectPtr<UNiagaraComponent> WeakComponent = Component;
    FTimerHandle DeactivateTimer;
    World->GetTimerManager().SetTimer(
        DeactivateTimer,
        FTimerDelegate::CreateLambda([WeakComponent]()
        {
            if (WeakComponent.IsValid())
            {
                WeakComponent->Deactivate();
            }
        }),
        bIsAllyBoost ? 0.42f : 0.32f,
        false);
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

void AWTBRCharacter::OnRep_CharacterStance()
{
    ApplyReplicatedStance();
    OnCharacterStanceChanged(CharacterStance);
}

bool AWTBRCharacter::CanStartVaelSprint() const
{
    if (CharacterStance != EWTBRCharacterStance::Standing) return false;
    if (bIsStaggered || bIsHangingOnNexilWire) return false;
    if (!IsValid(HealthComponent) || !HealthComponent->IsAlive()) return false;

    const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    return IsValid(MoveComp) && MoveComp->IsMovingOnGround();
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
