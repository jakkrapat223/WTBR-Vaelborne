// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRCharacter.h"
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
    }
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
    UE_LOG(LogTemp, Warning,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=true | Pressed=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifyMainPressed();
    Server_Fire(true, true);
}

void AWTBRCharacter::FireMainReleased(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=true | Pressed=false"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifyMainReleased();
    Server_Fire(true, false);
}

void AWTBRCharacter::FireSub(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=false | Pressed=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifySubPressed();
    Server_Fire(false, true);
}

void AWTBRCharacter::FireSubReleased(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Trigger Input] Owner=%s | Auth=%s | Local=%s | Main=false | Pressed=false"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    if (InputGestureComponent) InputGestureComponent->NotifySubReleased();
    Server_Fire(false, false);
}

void AWTBRCharacter::Dodge(const FInputActionValue& Value)
{
    if (StaminaComponent) StaminaComponent->TryConsumeDodgeStamina();
    Server_Dodge();
}

void AWTBRCharacter::SwitchTrigger(const FInputActionValue& Value)
{
    SwitchMainTrigger(Value);
}

void AWTBRCharacter::SwitchMainTrigger(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Log, TEXT("WTBR SwitchMain input pressed"));
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("Switch Main Trigger"));
    }
    Server_CycleTrigger(true);
}

void AWTBRCharacter::SwitchSubTrigger(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Log, TEXT("WTBR SwitchSub input pressed"));
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, TEXT("Switch Sub Trigger"));
    }
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

void AWTBRCharacter::Server_Fire_Implementation(bool bIsMain, bool bIsPressed)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=%s | Pressed=%s"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsMain ? TEXT("true") : TEXT("false"),
        bIsPressed ? TEXT("true") : TEXT("false"));

    ExecuteServerTriggerInput(bIsMain, bIsPressed);
}

void AWTBRCharacter::ExecuteServerTriggerInput(bool bIsMain, bool bIsPressed)
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

    UE_LOG(LogTemp, Log, TEXT("Active Trigger: %s"),
        Trigger ? *Trigger->GetClass()->GetName() : TEXT("nullptr"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f,
            Trigger ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("[Fire] %s"),
                Trigger ? *Trigger->GetClass()->GetName() : TEXT("NO TRIGGER - slot empty?")));
    }

    if (!Trigger) return;

    const bool bIsDualWield =
        TriggerSetComponent->GetDualWieldState() != EWTBRDualWieldState::None;
    const FInputActionValue EmptyInputValue;

    if (bIsPressed)
    {
        Trigger->OnTriggerActivated(this, bIsMain);
        const bool bActivated = Trigger->Activate(EmptyInputValue, bIsDualWield);
        UE_LOG(LogTemp, Log, TEXT("Trigger Activate result: %d"), bActivated);
    }
    else
    {
        Trigger->OnTriggerDeactivated(this, bIsMain);
        Trigger->OnReleased(EmptyInputValue, bIsDualWield);
    }
}

void AWTBRCharacter::Server_Dodge_Implementation()
{
    // Authoritative dodge validation / animation notify hook — extend in Phase 2
}

void AWTBRCharacter::Server_SwitchTrigger_Implementation(int32 SlotIndex, bool bIsMain)
{
    if (!TriggerSetComponent) return;
    if (bIsMain) TriggerSetComponent->SwitchMainSlot(SlotIndex);
    else         TriggerSetComponent->SwitchSubSlot(SlotIndex);
}

void AWTBRCharacter::Server_CycleTrigger_Implementation(bool bIsMain)
{
    if (!TriggerSetComponent) return;
    if (bIsMain) TriggerSetComponent->CycleMainSlot();
    else         TriggerSetComponent->CycleSubSlot();
}

void AWTBRCharacter::Server_DebugConsumeVaelFailTest_Implementation()
{
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Debug ConsumeFail Result] Owner=%s | VaelComponent=None"),
            *GetNameSafe(this));
        return;
    }

    const float BeforeVael = VaelComponent->GetCurrentVael();
    const bool bBeforeActive = VaelComponent->IsDesperationActive();
    const bool bBeforeCooldown = VaelComponent->IsDesperationOnCooldown();

    const bool bConsumed = VaelComponent->TryConsumeVael(999.0f);

    UE_LOG(LogTemp, Warning,
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
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        UE_LOG(LogTemp, Warning,
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

    UE_LOG(LogTemp, Warning,
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
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        UE_LOG(LogTemp, Warning,
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

    UE_LOG(LogTemp, Warning,
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
    if (!HasAuthority()) return;

    if (!IsValid(VaelComponent))
    {
        UE_LOG(LogTemp, Warning,
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

    UE_LOG(LogTemp, Warning,
        TEXT("[Debug ResetDesperationStateTest Before] Owner=%s | CurrentVael=%.1f | LowVaelThreshold=%.1f | Active=%s | Cooldown=%s | ActiveTimerActive=%s | CooldownTimerActive=%s"),
        *GetNameSafe(this),
        BeforeVael,
        LowVaelThreshold,
        bBeforeActive ? TEXT("true") : TEXT("false"),
        bBeforeCooldown ? TEXT("true") : TEXT("false"),
        bBeforeActiveTimer ? TEXT("true") : TEXT("false"),
        bBeforeCooldownTimer ? TEXT("true") : TEXT("false"));

    VaelComponent->ResetDesperationState();

    UE_LOG(LogTemp, Warning,
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
    if (!HasAuthority()) return;
    if (Duration <= 0.0f) return;
    if (TriggerSetComponent) TriggerSetComponent->CancelMerge();
    bIsStaggered = true;
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
    // Blueprint: true=disable input+stagger anim | false=re-enable input
}

// ─── Trigger Activation RPCs ─────────────────────────────────────────────────

void AWTBRCharacter::Server_ActivateMainTrigger_Implementation(bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=true | Pressed=true | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(true, true);
}

void AWTBRCharacter::Server_ReleaseMainTrigger_Implementation(bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=true | Pressed=false | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(true, false);
}

void AWTBRCharacter::Server_ActivateSubTrigger_Implementation(bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Server Trigger RPC] Owner=%s | Auth=%s | Main=false | Pressed=true | LegacyActivate=true"),
        *GetNameSafe(this),
        HasAuthority() ? TEXT("true") : TEXT("false"));
    ExecuteServerTriggerInput(false, true);
}

void AWTBRCharacter::Server_ReleaseSubTrigger_Implementation(bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning,
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
