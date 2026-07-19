// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVoltisLaunchTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Actors/WTBRVoltisTrapActor.h"
#include "Components/WTBRVaelComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

UWTBRVoltisLaunchTrigger::UWTBRVoltisLaunchTrigger()
{
    // Default straight to the native Trap actor — no BP subclass of this Trigger
    // is required to exist for Hold's ground-placement branch to work, since
    // AWTBRVoltisTrapActor already carries its own placeholder mesh + VFX defaults
    // natively (see its constructor). A BP subclass of THIS class can still
    // override TrapActorClass if the owner wants a different trap class later.
    TrapActorClass = AWTBRVoltisTrapActor::StaticClass();

}

void UWTBRVoltisLaunchTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Initialize | Trigger=%s | TriggerClass=%s | Owner=%s | HasAuthority=%s | DataAsset=%s"),
        *GetNameSafe(this),
        *GetNameSafe(GetClass()),
        *GetNameSafe(InOwnerCharacter),
        IsValid(InOwnerCharacter) && InOwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(InDataAsset));

    if (!IsValid(InDataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Function=Initialize | Trigger=%s"),
            *GetNameSafe(this));
        return;
    }

    RemainingLaunches = InDataAsset->VoltisParams.MaxAirLaunches;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Params | Trigger=%s | VaelCost=%.2f | Cooldown=None | VerticalForce=%.1f | HorizontalForce=%.1f | DirectionalHorizontalForce=%.1f | DirectionalVerticalForce=%.1f | MaxAirLaunches=%d | TrapRadius=%.1f | TrapDamage=%.1f | TrapLifetime=%.1f"),
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
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Activate Start | Trigger=%s | TriggerClass=%s | Owner=%s | HasAuthority=%s | DataAsset=%s | DualWield=%s | CurrentVael=%.2f | RemainingLaunches=%d | Staggered=%s"),
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
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=Activate"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=NoAuthority | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (bIsStaggered)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=Staggered | Owner=%s | RemainingLaunches=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            RemainingLaunches);
        return false;
    }
    if (RemainingLaunches <= 0)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=NoRemainingLaunches | Owner=%s | RemainingLaunches=%d"),
            *GetNameSafe(OwnerCharacter.Get()),
            RemainingLaunches);
        return false;
    }
    if (bWaitingForHoldDecision)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=AlreadyWaitingForHoldDecision | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=SuperActivateFalse | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    // Don't dash or resolve Hold yet — wait to see if this is a tap or a hold, same
    // HOLD_THRESHOLD-timer pattern as Aegorn's tap-vs-hold shield. Cache InputValue at
    // PRESS time for PerformTap to use — OnReleased_Implementation gets its OWN fresh
    // InputValue built from a separate release-time ClientMoveInputDir RPC snapshot
    // (see AWTBRCharacter::ExecuteServerTriggerInput), which is NOT what a tap's dash
    // direction should be read from (see CachedPressInputValue's header comment).
    CachedPressInputValue = InputValue;
    bWaitingForHoldDecision = true;
    GetWorld()->GetTimerManager().SetTimer(
        HoldThresholdTimer,
        this, &UWTBRVoltisLaunchTrigger::OnHoldThresholdReached,
        HOLD_THRESHOLD, false);
    return true;
}

void UWTBRVoltisLaunchTrigger::OnReleased_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Release | Trigger=%s | Owner=%s | HasAuthority=%s | DualWield=%s | WaitingForHoldDecision=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsDualWield ? TEXT("true") : TEXT("false"),
        bWaitingForHoldDecision ? TEXT("true") : TEXT("false"));

    if (bWaitingForHoldDecision)
    {
        bWaitingForHoldDecision = false;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(HoldThresholdTimer);
        }
        PerformTap(CachedPressInputValue, bIsDualWield);
    }
    // If Hold already resolved (threshold fired before release), there's nothing
    // left to do on release — unlike Aegorn's shield, Voltis Hold is a one-shot
    // resolution at the threshold, not a continuously-tracked held state.

    Super::OnReleased_Implementation(InputValue, bIsDualWield);
}

void UWTBRVoltisLaunchTrigger::Deactivate_Implementation()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Cancel | Trigger=%s | Owner=%s | HasAuthority=%s | RemainingLaunches=%d | Staggered=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        RemainingLaunches,
        bIsStaggered ? TEXT("true") : TEXT("false"));

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HoldThresholdTimer);
    }
    bWaitingForHoldDecision = false;

    // Placed traps are independent world objects (same as Nexil wires) — clean them
    // up when this Trigger is switched away from, rather than leaving them orphaned.
    TArray<TWeakObjectPtr<AWTBRVoltisTrapActor>> TrapsToDestroy = MoveTemp(ActiveTraps);
    ActiveTraps.Empty();
    for (TWeakObjectPtr<AWTBRVoltisTrapActor>& Trap : TrapsToDestroy)
    {
        if (Trap.IsValid())
        {
            Trap->Destroy();
        }
    }

    Super::Deactivate_Implementation();
}

void UWTBRVoltisLaunchTrigger::PerformTap(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerOrDataAssetInvalid | Function=PerformTap"));
        return;
    }

    // Tap now costs Vael like every other Trigger (locked design 2026-07-18: passive
    // Vael regen made a permanently-free Voltis stop making sense). Fail-closed: not
    // enough Vael = no dash, no launch consumed.
    if (!IsValid(OwnerCharacter->VaelComponent)
        || !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=InsufficientVael | Owner=%s | Cost=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            DataAsset->VaelCostPerUse);
        return;
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

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] MovementStart | Owner=%s | ClientInputDir=%s | RawInputVec=%s | Velocity2D=%s | FinalHorizontalDir=%s | bDirectionalLaunch=%s | HorizontalForceUsed=%.1f | VerticalForceUsed=%.1f | LaunchDir=%s | DirectionSource=%s | DirectionalHorizontalForce=%.1f | DirectionalVerticalForce=%.1f | RemainingBefore=%d | VaelCost=%.2f | Cooldown=None"),
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

    PerformLaunch(bIsDualWield, LaunchDir);
    OwnerCharacter->Multicast_VoltisBurst(OwnerCharacter.Get(), false, LaunchDir);
    RemainingLaunches--;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] MovementApplied | Owner=%s | LaunchDir=%s | RemainingAfter=%d | DualWield=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *LaunchDir.ToString(),
        RemainingLaunches,
        bIsDualWield ? TEXT("true") : TEXT("false"));
    OnVoltisLaunch.Broadcast(bIsDualWield);
    OnVoltisLaunched(bIsDualWield);
}

void UWTBRVoltisLaunchTrigger::OnHoldThresholdReached()
{
    if (!bWaitingForHoldDecision)
    {
        return; // released early, already handled as a tap
    }
    bWaitingForHoldDecision = false;
    ResolveHold();
}

void UWTBRVoltisLaunchTrigger::ResolveHold()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerOrDataAssetInvalid | Function=ResolveHold"));
        return;
    }

    // Branch 1: aiming at a teammate in range -> direct apply, no object spawned,
    // no surface check needed (works mid-air — deliberate exception, locked design).
    AWTBRCharacter* Teammate = AWTBRCharacter::FindBestFriendlyTarget(
        OwnerCharacter.Get(),
        DataAsset->VoltisParams.FriendlySearchRadius,
        DataAsset->VoltisParams.FriendlyAimConeHalfAngleDegrees);
    if (IsValid(Teammate))
    {
        PerformDirectApply(Teammate);
        return;
    }

    // Branch 2: not aiming at a teammate -> treat as ground/environment placement.
    // Aiming directly at an ENEMY character is explicitly banned (locked design: would
    // be an unconditional free hard-CC with no counterplay) — PerformTrapPlacement's own
    // forward aim-trace rejects placement if it hits ANY character (ally or enemy) rather
    // than bare surface, which also covers this case; see its comment for why.
    PerformTrapPlacement();
}

void UWTBRVoltisLaunchTrigger::PerformDirectApply(AWTBRCharacter* Teammate)
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset) || !IsValid(Teammate))
    {
        return;
    }

    if (!IsValid(OwnerCharacter->VaelComponent)
        || !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=InsufficientVael | Function=PerformDirectApply | Owner=%s | Cost=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            DataAsset->VaelCostPerUse);
        return;
    }

    // Read the TEAMMATE's own live movement input (not the caster's) — locked design:
    // launches them along whatever direction they're currently pressing, straight up
    // if they're pressing nothing.
    UCharacterMovementComponent* TeammateMove = Teammate->GetCharacterMovement();
    FVector InputDir = IsValid(TeammateMove)
        ? TeammateMove->GetLastInputVector()
        : FVector::ZeroVector;
    InputDir.Z = 0.0f;

    const float HorizontalForce = DataAsset->VoltisParams.DirectionalHorizontalForce;
    const float VerticalForce = DataAsset->VoltisParams.DirectionalVerticalForce;

    FVector LaunchDir;
    if (InputDir.SizeSquared2D() > 0.01f)
    {
        LaunchDir = (InputDir.GetSafeNormal2D() * HorizontalForce) + FVector(0.0f, 0.0f, VerticalForce);
    }
    else
    {
        LaunchDir = FVector(0.0f, 0.0f, DataAsset->VoltisParams.VerticalLaunchForce);
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] HoldDirectApply | Owner=%s | Teammate=%s | InputDir=%s | LaunchDir=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Teammate),
        *InputDir.ToString(),
        *LaunchDir.ToString());

    Teammate->LaunchCharacter(LaunchDir, true, true);
    OwnerCharacter->Multicast_VoltisBurst(Teammate, true, LaunchDir);
    OnVoltisFriendlyApplied(Teammate);
}

void UWTBRVoltisLaunchTrigger::PerformTrapPlacement()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset) || !GetWorld())
    {
        return;
    }

    if (TrapActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=TrapActorClassNull | TriggerClass=%s | ExpectedBP=BP_WTBRVoltisLaunchTrigger_C"),
            *GetNameSafe(GetClass()));
        return;
    }

    FVector EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
    const FVector AimDir = EyeRot.Vector().GetSafeNormal();

    // Forward aim-trace finds both the placement point AND validates a real surface
    // in one pass (more accurate than a disconnected spawn-point-then-trace-straight-
    // down like Escudo's wall, since a Trap can be placed at range on a slope/stairs).
    // Rejecting on ANY character hit (ally or enemy) is a deliberate inference, not an
    // explicit part of the design lock — it keeps a Trap from being spawned directly on
    // an enemy's feet (which would near-instantly trigger, functionally equivalent to
    // the explicitly-banned direct-apply-to-enemy case) and avoids overlapping-onto-
    // an-ally weirdness. Flagged here in case the owner wants different behavior.
    FHitResult AimHit;
    FCollisionQueryParams AimParams;
    AimParams.AddIgnoredActor(OwnerCharacter.Get());
    const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
        AimHit,
        EyeLoc,
        EyeLoc + AimDir * DataAsset->VoltisParams.TrapPlacementRange,
        ECC_Visibility,
        AimParams);
    if (!bHitSomething || !IsValid(AimHit.GetActor()) || AimHit.GetActor()->IsA(AWTBRCharacter::StaticClass()))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=NoValidGroundAimPoint | Owner=%s | HitSomething=%s | HitActor=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            bHitSomething ? TEXT("true") : TEXT("false"),
            bHitSomething ? *GetNameSafe(AimHit.GetActor()) : TEXT("None"));
        return;
    }

    TSubclassOf<AWTBRVoltisTrapActor> TrapClass = TrapActorClass.LoadSynchronous();
    if (!IsValid(TrapClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=TrapClassLoadFailed | TrapActorClass=%s"),
            *TrapActorClass.ToString());
        return;
    }

    // Validate everything spawnable BEFORE consuming Vael — a failed placement must
    // never cost the player anything (matches the Escudo/Aegorn convention).
    if (!IsValid(OwnerCharacter->VaelComponent)
        || !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=InsufficientVael | Function=PerformTrapPlacement | Owner=%s | Cost=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            DataAsset->VaelCostPerUse);
        return;
    }

    const FRotator SpawnRot = AimHit.ImpactNormal.Rotation();
    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWTBRVoltisTrapActor* Trap = GetWorld()->SpawnActor<AWTBRVoltisTrapActor>(
        TrapClass, AimHit.ImpactPoint, SpawnRot, Params);
    if (!IsValid(Trap))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=TrapSpawnFailed | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    Trap->InitializeTrap(
        DataAsset->VoltisParams.TrapLifetime,
        DataAsset->VoltisParams.TrapExplosionRadius,
        DataAsset->VoltisParams.TrapDamage,
        DataAsset->VoltisParams.VerticalLaunchForce,
        this);

    ActiveTraps.Add(Trap);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] TrapPlaced | Owner=%s | Trap=%s | Location=%s | ActiveAfterAdd=%d | MaxActiveTraps=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Trap),
        *Trap->GetActorLocation().ToString(),
        GetActiveTrapCount(),
        DataAsset->VoltisParams.MaxActiveTraps);

    // FIFO cap — oldest trap destroyed when placing over the cap (owner: "2-3 อัน",
    // same convention as Nexil's MaxWires).
    while (ActiveTraps.Num() > DataAsset->VoltisParams.MaxActiveTraps)
    {
        RemoveOldestTrap();
    }

    OnVoltisTrapPlaced(Trap);
}

void UWTBRVoltisLaunchTrigger::RemoveOldestTrap()
{
    if (ActiveTraps.Num() == 0)
    {
        return;
    }
    TWeakObjectPtr<AWTBRVoltisTrapActor> Oldest = ActiveTraps[0];
    ActiveTraps.RemoveAt(0);
    if (Oldest.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] MaxActiveTraps RemovingOldest | Trap=%s | ActiveAfterRemove=%d"),
            *GetNameSafe(Oldest.Get()),
            GetActiveTrapCount());
        Oldest->Destroy();
    }
}

void UWTBRVoltisLaunchTrigger::NotifyTrapConsumed(AWTBRVoltisTrapActor* ConsumedTrap)
{
    ActiveTraps.RemoveAll([ConsumedTrap](const TWeakObjectPtr<AWTBRVoltisTrapActor>& T)
    {
        return T.Get() == ConsumedTrap;
    });
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] TrapConsumed Notify | Trap=%s | ActiveAfterRemove=%d"),
        *GetNameSafe(ConsumedTrap),
        GetActiveTrapCount());
}

int32 UWTBRVoltisLaunchTrigger::GetActiveTrapCount() const
{
    int32 Count = 0;
    for (const auto& T : ActiveTraps)
    {
        if (T.IsValid())
        {
            Count++;
        }
    }
    return Count;
}

void UWTBRVoltisLaunchTrigger::PerformLaunch(bool bIsDualWield, const FVector& LaunchDir)
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=PerformLaunch"));
        return;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=DataAssetInvalid | Function=PerformLaunch | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }
    ACharacter* Char = Cast<ACharacter>(OwnerCharacter.Get());
    if (!IsValid(Char))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerNotCharacter | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] LaunchCharacter | Owner=%s | LaunchDir=%s | DualWield=%s"),
        *GetNameSafe(Char),
        *LaunchDir.ToString(),
        bIsDualWield ? TEXT("true") : TEXT("false"));
    Char->LaunchCharacter(LaunchDir, true, true);
}

void UWTBRVoltisLaunchTrigger::OnCharacterLanded(const FHitResult& Hit)
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=OwnerInvalid | Function=OnCharacterLanded"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=NoAuthority | Function=OnCharacterLanded | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return;
    }

    if (IsValid(DataAsset))
        RemainingLaunches = DataAsset->VoltisParams.MaxAirLaunches;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] MovementEnd | Owner=%s | HitActor=%s | ResetRemaining=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Hit.GetActor()),
        RemainingLaunches);
    OnVoltisLand.Broadcast();
}

void UWTBRVoltisLaunchTrigger::StartStagger()
{
    if (!GetWorld())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] Blocked | Reason=WorldInvalid | Function=StartStagger"));
        return;
    }
    bIsStaggered = true;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] StaggerStart | Owner=%s | Duration=%.3f"),
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
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] StaggerEnd | Owner=%s"),
        *GetNameSafe(OwnerCharacter.Get()));
}

void UWTBRVoltisLaunchTrigger::OnRep_bIsStaggered()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Movement Test] ReplicationObserved | Trigger=%s | Owner=%s | bIsStaggered=%s"),
        *GetNameSafe(this),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsStaggered ? TEXT("true") : TEXT("false"));
    // Blueprint: bIsStaggered=true → disable input + play stagger anim
    // Blueprint: bIsStaggered=false → re-enable input
}
