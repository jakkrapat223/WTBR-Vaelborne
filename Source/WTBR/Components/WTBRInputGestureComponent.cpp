// Copyright Vaelborne: Dominion. All Rights Reserved.

// Include TriggerSetComponent in .cpp only — avoids circular header dependency
#include "Components/WTBRInputGestureComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "WTBRCharacter.h"

UWTBRInputGestureComponent::UWTBRInputGestureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWTBRInputGestureComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AActor* Owner = GetOwner())
    {
        TriggerSetComp = Owner->FindComponentByClass<UWTBRTriggerSetComponent>();
    }
}

void UWTBRInputGestureComponent::NotifyMainPressed()
{
    bMainDown       = true;
    bMainFired      = false;
    MainHoldStartTime = GetWorld()->GetTimeSeconds();
    CheckDualGesture();
}

void UWTBRInputGestureComponent::NotifyMainReleased()
{
    if (!bMainFired)
    {
        const float HoldDuration = GetWorld()->GetTimeSeconds() - MainHoldStartTime;
        OnGestureDetected.Broadcast(
            HoldDuration >= HoldThreshold ? EWTBRInputGesture::MainHold : EWTBRInputGesture::MainTap);
    }
    bMainDown  = false;
    bMainFired = false;
}

void UWTBRInputGestureComponent::NotifySubPressed()
{
    bSubDown       = true;
    bSubFired      = false;
    SubHoldStartTime = GetWorld()->GetTimeSeconds();
    CheckDualGesture();
}

void UWTBRInputGestureComponent::NotifySubReleased()
{
    if (!bSubFired)
    {
        const float HoldDuration = GetWorld()->GetTimeSeconds() - SubHoldStartTime;
        OnGestureDetected.Broadcast(
            HoldDuration >= HoldThreshold ? EWTBRInputGesture::SubHold : EWTBRInputGesture::SubTap);
    }
    bSubDown  = false;
    bSubFired = false;
}

void UWTBRInputGestureComponent::BindInputActions(UEnhancedInputComponent* EIC)
{
    if (!EIC) return;

    // ── Move (WASD) ───────────────────────────────────────────────────────────
    if (IsValid(IA_Move))
    {
        EIC->BindAction(IA_Move, ETriggerEvent::Triggered,
            this, &UWTBRInputGestureComponent::OnMove_Triggered);
    }

    // ── Look (Mouse XY) ───────────────────────────────────────────────────────
    if (IsValid(IA_Look))
    {
        EIC->BindAction(IA_Look, ETriggerEvent::Triggered,
            this, &UWTBRInputGestureComponent::OnLook_Triggered);
    }

    // ── Jump ──────────────────────────────────────────────────────────────────
    if (IsValid(IA_Jump))
    {
        EIC->BindAction(IA_Jump, ETriggerEvent::Started,
            this, &UWTBRInputGestureComponent::OnJump_Started);
        EIC->BindAction(IA_Jump, ETriggerEvent::Completed,
            this, &UWTBRInputGestureComponent::OnJump_Completed);
    }
}

void UWTBRInputGestureComponent::OnMove_Triggered(const FInputActionValue& Value)
{
    AWTBRCharacter* Char = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Char)) return;

    // UE5 Enhanced Input + Axis2D: X = strafe, Y = forward/backward
    const FVector2D MoveAxis = Value.Get<FVector2D>();
    if (MoveAxis.IsNearlyZero()) return;

    const FRotator ControlRot = Char->GetControlRotation();
    const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

    // Forward/Backward from Y axis
    if (!FMath::IsNearlyZero(MoveAxis.Y))
    {
        const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        Char->AddMovementInput(ForwardDir, MoveAxis.Y);
    }

    // Left/Right from X axis
    if (!FMath::IsNearlyZero(MoveAxis.X))
    {
        const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
        Char->AddMovementInput(RightDir, MoveAxis.X);
    }
}

void UWTBRInputGestureComponent::OnLook_Triggered(const FInputActionValue& Value)
{
    AWTBRCharacter* Char = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Char)) return;

    const FVector2D LookAxis = Value.Get<FVector2D>();

    // X = Yaw (left/right camera), Y = Pitch (up/down camera)
    if (!FMath::IsNearlyZero(LookAxis.X))
    {
        Char->AddControllerYawInput(LookAxis.X);
    }
    if (!FMath::IsNearlyZero(LookAxis.Y))
    {
        // Negative Y = natural mouse feel (move mouse up = look up)
        Char->AddControllerPitchInput(-LookAxis.Y);
    }
}

void UWTBRInputGestureComponent::OnJump_Started(const FInputActionValue& Value)
{
    AWTBRCharacter* Char = Cast<AWTBRCharacter>(GetOwner());
    if (IsValid(Char))
    {
        Char->Jump();
    }
}

void UWTBRInputGestureComponent::OnJump_Completed(const FInputActionValue& Value)
{
    AWTBRCharacter* Char = Cast<AWTBRCharacter>(GetOwner());
    if (IsValid(Char))
    {
        Char->StopJumping();
    }
}

void UWTBRInputGestureComponent::CheckDualGesture()
{
    if (!bMainDown || !bSubDown || !TriggerSetComp) return;

    const UWTBRTriggerBase* Main = TriggerSetComp->GetActiveMainTrigger();
    const UWTBRTriggerBase* Sub  = TriggerSetComp->GetActiveSubTrigger();

    // GDD §3.4 Lock — Pure Type-Match only, no priority or timing check
    if (Main && Sub && Main->Category == Sub->Category)
    {
        bMainFired = true;
        bSubFired  = true;
        OnGestureDetected.Broadcast(EWTBRInputGesture::DualTrigger);
    }
}
