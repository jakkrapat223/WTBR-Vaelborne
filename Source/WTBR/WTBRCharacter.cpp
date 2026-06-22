// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
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
}

void AWTBRCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Sub->AddMappingContext(DefaultMappingContext, 0);
        }
    }

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
        EIC->BindAction(JumpAction,          ETriggerEvent::Triggered, this, &ACharacter::Jump);
        EIC->BindAction(JumpAction,          ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        EIC->BindAction(MoveAction,          ETriggerEvent::Triggered, this, &AWTBRCharacter::Move);
        EIC->BindAction(LookAction,          ETriggerEvent::Triggered, this, &AWTBRCharacter::Look);
        EIC->BindAction(FireMainAction,      ETriggerEvent::Started,   this, &AWTBRCharacter::FireMain);
        EIC->BindAction(FireMainAction,      ETriggerEvent::Completed, this, &AWTBRCharacter::FireMainReleased);
        EIC->BindAction(FireSubAction,       ETriggerEvent::Started,   this, &AWTBRCharacter::FireSub);
        EIC->BindAction(FireSubAction,       ETriggerEvent::Completed, this, &AWTBRCharacter::FireSubReleased);
        EIC->BindAction(DodgeAction,         ETriggerEvent::Triggered, this, &AWTBRCharacter::Dodge);
        EIC->BindAction(SwitchTriggerAction, ETriggerEvent::Triggered, this, &AWTBRCharacter::SwitchTrigger);
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
    if (InputGestureComponent) InputGestureComponent->NotifyMainPressed();
    Server_Fire(true, true);
}

void AWTBRCharacter::FireMainReleased(const FInputActionValue& Value)
{
    if (InputGestureComponent) InputGestureComponent->NotifyMainReleased();
    Server_Fire(true, false);
}

void AWTBRCharacter::FireSub(const FInputActionValue& Value)
{
    if (InputGestureComponent) InputGestureComponent->NotifySubPressed();
    Server_Fire(false, true);
}

void AWTBRCharacter::FireSubReleased(const FInputActionValue& Value)
{
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
    // Slot index from axis value (extend as needed for radial/scroll wheel)
    Server_SwitchTrigger(0, true);
}

// ─── Server RPC Implementations ──────────────────────────────────────────────

void AWTBRCharacter::Server_Fire_Implementation(bool bIsMain, bool bIsPressed)
{
    if (!TriggerSetComponent) return;
    UWTBRTriggerBase* Trigger = bIsMain
        ? TriggerSetComponent->GetActiveMainTrigger()
        : TriggerSetComponent->GetActiveSubTrigger();

    if (!Trigger) return;
    if (bIsPressed) Trigger->OnTriggerActivated(this, bIsMain);
    else            Trigger->OnTriggerDeactivated(this, bIsMain);
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

// ─── Delegate Handlers ───────────────────────────────────────────────────────

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
}
