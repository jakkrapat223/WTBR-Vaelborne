// Copyright Vaelborne: Dominion. All Rights Reserved.

// Include TriggerSetComponent in .cpp only — avoids circular header dependency
#include "Components/WTBRInputGestureComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
    CancelMantornHoldWatch();
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
    CancelMantornHoldWatch();
}

void UWTBRInputGestureComponent::BindInputActions(UEnhancedInputComponent* EIC)
{
    if (!EIC) return;

    // ── Move (WASD) ───────────────────────────────────────────────────────────
    if (IsValid(IA_Move))
    {
        EIC->BindAction(IA_Move, ETriggerEvent::Triggered,
            this, &UWTBRInputGestureComponent::OnMove_Triggered);
        EIC->BindAction(IA_Move, ETriggerEvent::Completed,
            this, &UWTBRInputGestureComponent::OnMove_Completed);
        EIC->BindAction(IA_Move, ETriggerEvent::Canceled,
            this, &UWTBRInputGestureComponent::OnMove_Completed);
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

    // Nexil zipline: hanging is meant to hold the character in place (camera
    // still free) — MOVE_Flying otherwise responds to AddMovementInput like
    // any other flight, letting WASD drift the character away from the wire.
    if (Char->IsHangingOnNexilWire()) return;

    // UE5 Enhanced Input + Axis2D: X = strafe, Y = forward/backward
    const FVector2D MoveAxis = Value.Get<FVector2D>();
    LastMoveAxis2D = MoveAxis.SizeSquared() > 1.0f
        ? MoveAxis.GetSafeNormal()
        : MoveAxis;
    if (LastMoveAxis2D.IsNearlyZero())
    {
        LastMoveAxis2D = FVector2D::ZeroVector;
        return;
    }

    const FRotator ControlRot = Char->GetControlRotation();
    const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);

    // Forward/Backward from Y axis
    if (!FMath::IsNearlyZero(LastMoveAxis2D.Y))
    {
        const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        Char->AddMovementInput(ForwardDir, LastMoveAxis2D.Y);
    }

    // Left/Right from X axis
    if (!FMath::IsNearlyZero(LastMoveAxis2D.X))
    {
        const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
        Char->AddMovementInput(RightDir, LastMoveAxis2D.X);
    }
}

void UWTBRInputGestureComponent::OnMove_Completed(const FInputActionValue& Value)
{
    LastMoveAxis2D = FVector2D::ZeroVector;
}

void UWTBRInputGestureComponent::OnLook_Triggered(const FInputActionValue& Value)
{
    AWTBRCharacter* Char = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(Char)) return;

    // Escudo preset wheel freezes the camera while open (owner-locked design;
    // deliberately different from the Trigger Wheel, which does NOT freeze —
    // see AWTBRCharacter::IsLookInputFrozen's doc comment for why). The wheel
    // widget still reads raw mouse delta itself for selection, independent of
    // this Enhanced Input path, so freezing this alone is sufficient.
    if (Char->IsLookInputFrozen()) return;

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
    if (!IsValid(Char)) return;

    // Nexil zipline: Jump while hanging releases + launches toward the current
    // camera direction instead of a normal jump.
    if (Char->IsHangingOnNexilWire())
    {
        Char->Server_ReleaseNexilWireAndLaunch();
        return;
    }

    Char->Jump();
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
    if (!Main || !Sub) return;

    // A Mantorn-capable Feryx pair uses a HOLD (both sides, past threshold) to
    // toggle the composite form — start a watch rather than firing immediately.
    if (TriggerSetComp->CanToggleMantornForm())
    {
        StartMantornHoldWatch();
    }

    // GDD §3.4 Lock — Pure Type-Match only, no timing check. (Cosmetic gesture;
    // the authoritative dual-wield effect is driven by GetDualWieldState server-side.)
    if (Main->Category == Sub->Category)
    {
        bMainFired = true;
        bSubFired  = true;
        OnGestureDetected.Broadcast(EWTBRInputGesture::DualTrigger);
    }
}

void UWTBRInputGestureComponent::StartMantornHoldWatch()
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().SetTimer(
        MantornHoldTimer, this,
        &UWTBRInputGestureComponent::OnMantornHoldElapsed,
        HoldThreshold, false);
}

void UWTBRInputGestureComponent::CancelMantornHoldWatch()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(MantornHoldTimer);
    }
}

void UWTBRInputGestureComponent::OnMantornHoldElapsed()
{
    // Both sides must still be held, and the loadout must still be toggle-eligible.
    if (!bMainDown || !bSubDown || !TriggerSetComp) return;
    if (!TriggerSetComp->CanToggleMantornForm()) return;

    // Suppress the per-side hold gestures — this hold was the form toggle.
    bMainFired = true;
    bSubFired  = true;

    // Server is authoritative for the actual transform (validates + charges Vael).
    TriggerSetComp->Server_RequestMantornToggle();
}
