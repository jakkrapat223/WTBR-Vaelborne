// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void UWTBRVoltisLaunchTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] Initialize | Trigger=%s | TriggerClass=%s | Owner=%s | HasAuthority=%s | DataAsset=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(InOwnerCharacter),
        IsValid(InOwnerCharacter) && InOwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(InDataAsset));

    if (!IsValid(InDataAsset))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Function=Initialize | Trigger=%s"),
            *GetNameSafe(this));
        return;
    }

    RemainingLaunches = InDataAsset->VoltisParams.MaxAirLaunches;

    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] Params | Trigger=%s | VaelCost=%.2f | Cooldown=None | VerticalForce=%.1f | HorizontalForce=%.1f | DirectionalHorizontalForce=%.1f | DirectionalVerticalForce=%.1f | MaxAirLaunches=%d | TrapRadius=%.1f | TrapDamage=%.1f | TrapLifetime=%.1f"),
        *GetNameSafe(this),
        InDataAsset->VaelCostPerUse,
        InDataAsset->VoltisParams.VerticalLaunchForce,
        InDataAsset->VoltisParams.HorizontalLaunchForce,
        InDataAsset->VoltisParams.DirectionalHorizontalForce,
        InDataAsset->VoltisParams.DirectionalVerticalForce,
        InDataAsset->VoltisParams.MaxAirLaunches,
        InDataAsset->VoltisParams.TrapExplosionRadius,
        InDataAsset->VoltisParams.TrapDamage,
        InDataAsset->VoltisParams.TrapLifetime);

    if (OwnerCharacter.IsValid())
    {
        ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
        if (IsValid(Char))
        {
            Char->LandedDelegate.AddDynamic(
                this, &UWTBRVoltisLaunchTrigger::OnCharacterLanded);
        }
    }
}

void UWTBRVoltisLaunchTrigger::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRVoltisLaunchTrigger, bIsStaggered);
}

bool UWTBRVoltisLaunchTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    const float CurrentVael = OwnerCharacter.IsValid() && IsValid(OwnerCharacter->VaelComponent)
        ? OwnerCharacter->VaelComponent->GetCurrentVael()
        : -1.0f;
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] Activate Start | Trigger=%s | TriggerClass=%s | Owner=%s | HasAuthority=%s | DataAsset=%s | DualWield=%s | CurrentVael=%.2f | RemainingLaunches=%d | Staggered=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(DataAsset),
        bIsDualWield ? TEXT("true") : TEXT("false"),
        CurrentVael,
        RemainingLaunches,
        bIsStaggered ? TEXT("true") : TEXT("false"));

    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=Activate"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=NoAuthority | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!IsValid(DataAsset))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (bIsStaggered)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=Staggered | Owner=%s | RemainingLaunches=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            RemainingLaunches);
        return false;
    }
    if (RemainingLaunches <= 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=NoRemainingLaunches | Owner=%s | RemainingLaunches=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            RemainingLaunches);
        return false;
    }

    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=SuperActivateFalse | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    // Server may not have a reliable last input vector for remote clients; use velocity as a fallback.
    UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
    FVector ClientInputDir = InputValue.Get<FVector>();
    ClientInputDir.Z = 0.0f;
    if (ClientInputDir.ContainsNaN() || ClientInputDir.SizeSquared2D() <= KINDA_SMALL_NUMBER)
    {
        ClientInputDir = FVector::ZeroVector;
    }
    else
    {
        ClientInputDir = ClientInputDir.GetClampedToMaxSize2D(1.0f).GetSafeNormal2D();
    }

    const FVector RawInputVec = IsValid(MoveComp)
        ? MoveComp->GetLastInputVector()
        : FVector::ZeroVector;
    const FVector Velocity2D = IsValid(MoveComp)
        ? FVector(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.0f)
        : FVector::ZeroVector;

    FVector FinalHorizontalDir = FVector::ZeroVector;
    const TCHAR* DirectionSource = TEXT("None");
    if (ClientInputDir.SizeSquared2D() > 0.01f)
    {
        FinalHorizontalDir = ClientInputDir;
        DirectionSource = TEXT("ClientInput");
    }
    else if (RawInputVec.SizeSquared2D() > 0.01f)
    {
        FinalHorizontalDir = RawInputVec.GetSafeNormal2D();
        DirectionSource = TEXT("RawInput");
    }
    else if (Velocity2D.SizeSquared2D() > 0.01f)
    {
        FinalHorizontalDir = Velocity2D.GetSafeNormal2D();
        DirectionSource = TEXT("VelocityFallback");
    }

    const bool bDirectionalLaunch = !FinalHorizontalDir.IsNearlyZero();
    const float DirectionalHorizontalForce = DataAsset->VoltisParams.DirectionalHorizontalForce;
    const float DirectionalVerticalForce = DataAsset->VoltisParams.DirectionalVerticalForce;
    const float HorizontalForceUsed = bDirectionalLaunch
        ? DirectionalHorizontalForce
        : 0.0f;
    const float VerticalForceUsed = bDirectionalLaunch
        ? DirectionalVerticalForce
        : DataAsset->VoltisParams.VerticalLaunchForce;

    FVector LaunchDir;
    if (bDirectionalLaunch)
    {
        LaunchDir = (FinalHorizontalDir * HorizontalForceUsed) + FVector(0.0f, 0.0f, VerticalForceUsed);
    }
    else
    {
        LaunchDir = FVector(0.0f, 0.0f, VerticalForceUsed);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] MovementStart | Owner=%s | ClientInputDir=%s | RawInputVec=%s | Velocity2D=%s | FinalHorizontalDir=%s | bDirectionalLaunch=%s | HorizontalForceUsed=%.1f | VerticalForceUsed=%.1f | LaunchDir=%s | DirectionSource=%s | DirectionalHorizontalForce=%.1f | DirectionalVerticalForce=%.1f | RemainingBefore=%d | VaelCost=%.2f | Cooldown=None"),
        *GetNameSafe(OwnerCharacter.Get()),
        *ClientInputDir.ToString(),
        *RawInputVec.ToString(),
        *Velocity2D.ToString(),
        *FinalHorizontalDir.ToString(),
        bDirectionalLaunch ? TEXT("true") : TEXT("false"),
        HorizontalForceUsed,
        VerticalForceUsed,
        *LaunchDir.ToString(),
        DirectionSource,
        DirectionalHorizontalForce,
        DirectionalVerticalForce,
        RemainingLaunches,
        DataAsset->VaelCostPerUse);

    if (HasCeilingNearby(LaunchDir))
    {
        bLastLandingWasCeilingBounce = true;
        ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
        if (IsValid(Char) && IsValid(Char->GetCharacterMovement()))
        {
            FVector Vel = Char->GetCharacterMovement()->Velocity;
            Vel.Z = 0.0f;
            Char->GetCharacterMovement()->Velocity = Vel;
        }
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=LaunchPathBlocked | Owner=%s | LaunchDir=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            *LaunchDir.ToString());
        return false;
    }

    PerformLaunch(bIsDualWield, LaunchDir);
    RemainingLaunches--;
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] MovementApplied | Owner=%s | LaunchDir=%s | RemainingAfter=%d | DualWield=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *LaunchDir.ToString(),
        RemainingLaunches,
        bIsDualWield ? TEXT("true") : TEXT("false"));
    OnVoltisLaunch.Broadcast(bIsDualWield);
    OnVoltisLaunched(bIsDualWield);
    return true;
}

void UWTBRVoltisLaunchTrigger::OnReleased_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] Release | Trigger=%s | Owner=%s | HasAuthority=%s | DualWield=%s | Behavior=None"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsDualWield ? TEXT("true") : TEXT("false"));
    Super::OnReleased_Implementation(InputValue, bIsDualWield);
}

void UWTBRVoltisLaunchTrigger::Deactivate_Implementation()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] Cancel | Trigger=%s | Owner=%s | HasAuthority=%s | RemainingLaunches=%d | Staggered=%s | Behavior=None"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        RemainingLaunches,
        bIsStaggered ? TEXT("true") : TEXT("false"));
    Super::Deactivate_Implementation();
}

bool UWTBRVoltisLaunchTrigger::HasCeilingNearby(const FVector& LaunchDir) const
{
    if (!OwnerCharacter.IsValid() || !GetWorld())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=PathBlockCheckInvalid | Owner=%s | WorldValid=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            GetWorld() ? TEXT("true") : TEXT("false"));
        return false;
    }

    const FVector Start = OwnerCharacter->GetActorLocation();
    const FVector End   = Start + (LaunchDir.GetSafeNormal() * MIN_CEILING_CLEARANCE);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Start, End, ECC_WorldStatic, Params);

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
        bHit ? FColor::Red : FColor::Green, false, 0.5f, 0, 2.0f);
#endif

    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] PathBlockCheck | Owner=%s | Start=%s | End=%s | Hit=%s | HitActor=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *Start.ToString(),
        *End.ToString(),
        bHit ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Hit.GetActor()));

    return bHit;
}

void UWTBRVoltisLaunchTrigger::PerformLaunch(bool bIsDualWield, const FVector& LaunchDir)
{
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=PerformLaunch"));
        return;
    }
    if (!IsValid(DataAsset))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Function=PerformLaunch | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }
    ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
    if (!IsValid(Char))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=OwnerNotCharacter | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] LaunchCharacter | Owner=%s | LaunchDir=%s | DualWield=%s"),
        *GetNameSafe(Char),
        *LaunchDir.ToString(),
        bIsDualWield ? TEXT("true") : TEXT("false"));
    Char->LaunchCharacter(LaunchDir, true, true);
}

void UWTBRVoltisLaunchTrigger::OnCharacterLanded(const FHitResult& Hit)
{
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=OnCharacterLanded"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Movement Test] Blocked | Reason=NoAuthority | Function=OnCharacterLanded | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    if (IsValid(DataAsset))
        RemainingLaunches = DataAsset->VoltisParams.MaxAirLaunches;

    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] MovementEnd | Owner=%s | HitActor=%s | ResetRemaining=%d | CeilingBounce=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Hit.GetActor()),
        RemainingLaunches,
        bLastLandingWasCeilingBounce ? TEXT("true") : TEXT("false"));

    if (bLastLandingWasCeilingBounce)
    {
        bLastLandingWasCeilingBounce = false;
        StartStagger();
        OnVoltisLand.Broadcast(true);
        OnVoltisHardLanding();
    }
    else
    {
        OnVoltisLand.Broadcast(false);
    }
}

void UWTBRVoltisLaunchTrigger::StartStagger()
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Movement Test] Blocked | Reason=WorldInvalid | Function=StartStagger"));
        return;
    }
    bIsStaggered = true;
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] StaggerStart | Owner=%s | Duration=%.3f"),
        *GetNameSafe(OwnerCharacter.Get()),
        STAGGER_DURATION);
    GetWorld()->GetTimerManager().SetTimer(
        StaggerTimer,
        this, &UWTBRVoltisLaunchTrigger::OnStaggerExpired,
        STAGGER_DURATION,
        false);
}

void UWTBRVoltisLaunchTrigger::OnStaggerExpired()
{
    bIsStaggered = false;
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] StaggerEnd | Owner=%s"),
        *GetNameSafe(OwnerCharacter.Get()));
}

void UWTBRVoltisLaunchTrigger::OnRep_bIsStaggered()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Movement Test] ReplicationObserved | Trigger=%s | Owner=%s | bIsStaggered=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsStaggered ? TEXT("true") : TEXT("false"));
    // Blueprint: bIsStaggered=true → disable input + play stagger anim
    // Blueprint: bIsStaggered=false → re-enable input
}
