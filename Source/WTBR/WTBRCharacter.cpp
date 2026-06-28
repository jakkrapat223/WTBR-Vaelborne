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
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRStaminaComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRMovementExtComponent.h"
#include "Components/WTBRInputGestureComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "Trigger/WTBRSerpveilTrigger.h"

namespace
{
    FText GetHUDTriggerName(const UWTBRTriggerBase* Trigger)
    {
        if (!IsValid(Trigger))
        {
            return FText::FromString(TEXT("None"));
        }

        const FText FunctionalName = Trigger->GetFunctionalName();
        return FunctionalName.IsEmpty() ? FText::FromString(TEXT("None")) : FunctionalName;
    }

    FText GetHUDTriggerDataAssetName(const UWTBRTriggerDataAsset* DataAsset)
    {
        if (!IsValid(DataAsset))
        {
            return FText::FromString(TEXT("None"));
        }

        return DataAsset->FunctionalName.IsEmpty()
            ? FText::FromString(TEXT("None"))
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
    TriggerSetComponent   = CreateDefaultSubobject<UWTBRTriggerSetComponent>(TEXT("TriggerSetComponent"));

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

    RefreshHUDHints();
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
    Server_Fire(false, false, FVector::ZeroVector);
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

// ─── Server RPC Implementations ──────────────────────────────────────────────

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

    if (!Trigger) return;

    const bool bIsDualWield =
        TriggerSetComponent->GetDualWieldState() != EWTBRDualWieldState::None;
    const FInputActionValue TriggerInputValue(ClientMoveInputDir);

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

    if (GEngine)
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

    if (GEngine)
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

    if (GEngine)
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

    if (GEngine)
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

    return FText::FromString(FString::Printf(
        TEXT("Main [LMB] %s"),
        *GetHUDTriggerNameWithFallback(MainTrigger, MainDataAsset).ToString()));
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

    return FText::FromString(FString::Printf(
        TEXT("Sub [RMB] %s"),
        *GetHUDTriggerNameWithFallback(SubTrigger, SubDataAsset).ToString()));
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

FText AWTBRCharacter::GetSwitchMainHintText() const
{
    return FText::FromString(TEXT("Q Switch Main"));
}

FText AWTBRCharacter::GetSwitchSubHintText() const
{
    return FText::FromString(TEXT("E Switch Sub"));
}

FText AWTBRCharacter::GetCancelHintText() const
{
    return FText::FromString(TEXT("X Cancel"));
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

void AWTBRCharacter::OnRep_ActionPing()
{
    // Client-side visual feedback for radar action ping
}

void AWTBRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWTBRCharacter, bActionPingActive);
    DOREPLIFETIME(AWTBRCharacter, bIsStaggered);
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
