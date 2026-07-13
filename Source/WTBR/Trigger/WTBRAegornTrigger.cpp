// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAegornTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

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
    if (bShieldActive)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate NoOp | Reason=ShieldAlreadyActive"));
        return true;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=DataAssetInvalid"));
        return false;
    }

    UWTBRVaelComponent* Vael = OwnerCharacter->VaelComponent;
    if (!IsValid(Vael) || !Vael->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=VaelConsumeFailed | Cost=%.2f"),
            DataAsset->VaelCostPerUse);
        return false;
    }

    if (ShieldActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=ShieldActorClassNull"));
        return false;
    }
    TSubclassOf<AWTBRAegornWallActor> ShieldClass = ShieldActorClass.LoadSynchronous();
    if (!IsValid(ShieldClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=ShieldClassLoadFailed | Class=%s"),
            *ShieldActorClass.ToString());
        return false;
    }

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * 150.0f;
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWTBRAegornWallActor* Shield =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            ShieldClass, SpawnLoc, SpawnRot, Params);
    if (!IsValid(Shield))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] Activate Fail | Reason=SpawnFailed"));
        return false;
    }

    Shield->InitializeWall(
        DataAsset->AegornParams.ShieldHP,
        DataAsset->AegornParams.ShieldDuration);
    Shield->OnWallDestroyed.AddDynamic(this, &UWTBRAegornTrigger::NotifyShieldDestroyed);

    ActiveShieldActor = Shield;
    bShieldActive = true;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Aegorn Test] ShieldSpawned | Owner=%s | Shield=%s | Location=%s | ShieldHP=%.1f | DualWield=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(Shield),
        *SpawnLoc.ToString(),
        DataAsset->AegornParams.ShieldHP,
        bIsDualWield ? TEXT("true") : TEXT("false"));

    OnShieldChanged.Broadcast(true);
    OnAegornShieldRaised(bIsDualWield);
    return true;
}

void UWTBRAegornTrigger::Deactivate_Implementation()
{
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
    ActiveShieldActor = nullptr;
    bShieldActive = false;
    OnShieldChanged.Broadcast(false);
    OnAegornShieldLowered();
}

void UWTBRAegornTrigger::OnRep_bShieldActive()
{
    OnShieldChanged.Broadcast(bShieldActive);
}
