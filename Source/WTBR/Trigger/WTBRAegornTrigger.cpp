// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAegornTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

void UWTBRAegornTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);
}

void UWTBRAegornTrigger::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRAegornTrigger, bShieldActive);
}

void UWTBRAegornTrigger::OnTriggerActivated_Implementation(
    AActor* OwnerActor, bool bIsMain)
{
    Super::OnTriggerActivated_Implementation(OwnerActor, bIsMain);
    bIsMainSlot = bIsMain;
}

FVector UWTBRAegornTrigger::GetAimDirection() const
{
    if (!OwnerCharacter.IsValid()) return FVector::ForwardVector;
    return OwnerCharacter->GetController()
        ? OwnerCharacter->GetController()->GetControlRotation().Vector()
        : OwnerCharacter->GetActorForwardVector();
}

bool UWTBRAegornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=OwnerInvalid"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=NoAuthority"));
        return false;
    }
    if (bShieldActive || bWaitingForHoldDecision)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate NoOp | Reason=AlreadyActiveOrPending"));
        return true;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=DataAssetInvalid"));
        return false;
    }

    // Don't spawn anything yet — wait to see if this is a tap or a hold.
    bWaitingForHoldDecision = true;
    GetWorld()->GetTimerManager().SetTimer(
        HoldThresholdTimer,
        this, &UWTBRAegornTrigger::OnHoldThresholdReached,
        HOLD_THRESHOLD, false);
    return true;
}

void UWTBRAegornTrigger::OnReleased_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (bWaitingForHoldDecision)
    {
        bWaitingForHoldDecision = false;
        if (GetWorld())
            GetWorld()->GetTimerManager().ClearTimer(HoldThresholdTimer);
        PerformTap();
        return;
    }
    if (bIsHeldMode)
    {
        ExitHeldMode();
    }
}

void UWTBRAegornTrigger::OnHoldThresholdReached()
{
    if (!bWaitingForHoldDecision) return; // released early, already handled
    bWaitingForHoldDecision = false;
    EnterHeldMode();
}

AWTBRAegornWallActor* UWTBRAegornTrigger::SpawnShieldActor(
    const FVector& Loc, const FRotator& Rot, const TCHAR* LogTag)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return nullptr;
    if (!IsValid(DataAsset)) return nullptr;

    UWTBRVaelComponent* Vael = OwnerCharacter->VaelComponent;
    if (!IsValid(Vael) || !Vael->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] %s Fail | Reason=VaelConsumeFailed | Cost=%.2f"),
            LogTag, DataAsset->VaelCostPerUse);
        return nullptr;
    }

    if (ShieldActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] %s Fail | Reason=ShieldActorClassNull"), LogTag);
        return nullptr;
    }
    TSubclassOf<AWTBRAegornWallActor> ShieldClass = ShieldActorClass.LoadSynchronous();
    if (!IsValid(ShieldClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] %s Fail | Reason=ShieldClassLoadFailed | Class=%s"),
            LogTag, *ShieldActorClass.ToString());
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWTBRAegornWallActor* Shield =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(ShieldClass, Loc, Rot, Params);
    if (!IsValid(Shield))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] %s Fail | Reason=SpawnFailed"), LogTag);
        return nullptr;
    }

    Shield->InitializeWall(
        DataAsset->AegornParams.ShieldHP,
        DataAsset->AegornParams.ShieldDuration);
    Shield->OnWallDestroyed.AddDynamic(this, &UWTBRAegornTrigger::NotifyShieldDestroyed);

    ActiveShieldActor = Shield;
    bShieldActive = true;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] %s ShieldSpawned | Owner=%s | Shield=%s | Location=%s | ShieldHP=%.1f"),
        LogTag, *GetNameSafe(OwnerCharacter.Get()), *GetNameSafe(Shield), *Loc.ToString(),
        DataAsset->AegornParams.ShieldHP);

    OnShieldChanged.Broadcast(true);
    return Shield;
}

void UWTBRAegornTrigger::PerformTap()
{
    if (!OwnerCharacter.IsValid() || bShieldActive) return;

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * SPAWN_FORWARD_OFFSET;
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    if (SpawnShieldActor(SpawnLoc, SpawnRot, TEXT("Tap")))
    {
        OnAegornShieldRaised(false);
    }
}

void UWTBRAegornTrigger::EnterHeldMode()
{
    if (!OwnerCharacter.IsValid() || bShieldActive) return;

    const FVector AimDir = GetAimDirection();
    const FVector SpawnLoc = OwnerCharacter->GetActorLocation() + AimDir * SPAWN_FORWARD_OFFSET;

    if (!SpawnShieldActor(SpawnLoc, AimDir.Rotation(), TEXT("HoldEnter"))) return;

    bIsHeldMode = true;
    GetWorld()->GetTimerManager().SetTimer(
        HoldTrackingTimer,
        this, &UWTBRAegornTrigger::TickHeldShield,
        HOLD_TRACK_INTERVAL, true);

    OnAegornShieldRaised(true);
}

void UWTBRAegornTrigger::TickHeldShield()
{
    if (!bIsHeldMode || !OwnerCharacter.IsValid() || !ActiveShieldActor.IsValid())
    {
        if (GetWorld())
            GetWorld()->GetTimerManager().ClearTimer(HoldTrackingTimer);
        return;
    }
    if (!OwnerCharacter->HasAuthority()) return;

    // Full Guard: when Aegorn is held in both Main and Sub simultaneously,
    // the Sub instance flips to cover the caster's back instead of also
    // facing forward — together the pair gives front+back coverage with
    // zero extra input, matching canon's two-hemisphere Full Guard.
    const bool bFlipToRear = !bIsMainSlot && IsSiblingAegornHeld();
    FVector AimDir = GetAimDirection();
    if (bFlipToRear) AimDir = -AimDir;

    const FVector NewLoc = OwnerCharacter->GetActorLocation() + AimDir * SPAWN_FORWARD_OFFSET;
    ActiveShieldActor->SetActorLocationAndRotation(NewLoc, AimDir.Rotation());
}

bool UWTBRAegornTrigger::IsSiblingAegornHeld() const
{
    if (!OwnerCharacter.IsValid()) return false;
    UWTBRTriggerSetComponent* TSC = OwnerCharacter->TriggerSetComponent;
    if (!IsValid(TSC)) return false;

    UWTBRTriggerBase* Sibling = bIsMainSlot
        ? TSC->GetActiveSubTrigger()
        : TSC->GetActiveMainTrigger();
    const UWTBRAegornTrigger* SiblingAegorn = Cast<UWTBRAegornTrigger>(Sibling);
    return IsValid(SiblingAegorn) && SiblingAegorn->bIsHeldMode;
}

void UWTBRAegornTrigger::ExitHeldMode()
{
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(HoldTrackingTimer);
    bIsHeldMode = false;
    CancelShield();
}

void UWTBRAegornTrigger::Deactivate_Implementation()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HoldThresholdTimer);
        GetWorld()->GetTimerManager().ClearTimer(HoldTrackingTimer);
    }
    bWaitingForHoldDecision = false;
    bIsHeldMode = false;
    CancelShield();
    Super::Deactivate_Implementation();
}

bool UWTBRAegornTrigger::CancelShield()
{
    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=InvalidOwner | Trigger=Aegorn"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] Blocked | Reason=NoAuthority | Trigger=Aegorn | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!bShieldActive || !ActiveShieldActor.IsValid())
    {
        return false;
    }

    // Collapse the shield by draining its own remaining HP — reuses the same
    // break path a projectile hit would take (TakeDamageFromProjectile ->
    // DestroyWall -> OnWallDestroyed -> NotifyShieldDestroyed clears state).
    ActiveShieldActor->TakeDamageFromProjectile(ActiveShieldActor->WallHP);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Cancel Test] AegornShieldCanceled | Owner=%s | ShieldActive=false"),
        *GetNameSafe(OwnerCharacter.Get()));
    return true;
}

void UWTBRAegornTrigger::NotifyShieldDestroyed()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] ShieldDestroyed Notify | Shield=%s"),
        *GetNameSafe(ActiveShieldActor.Get()));
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(HoldTrackingTimer);
    ActiveShieldActor = nullptr;
    bShieldActive = false;
    bIsHeldMode = false;
    OnShieldChanged.Broadcast(false);
    OnAegornShieldLowered();
}

void UWTBRAegornTrigger::OnRep_bShieldActive()
{
    OnShieldChanged.Broadcast(bShieldActive);
}
