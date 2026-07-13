// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBREscudoTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBREscudoTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    const AWTBRCharacter* OwnerChar = OwnerCharacter.Get();
    const bool bHasAuthority = OwnerChar && OwnerChar->HasAuthority();
    const float Cost = IsValid(DataAsset) ? DataAsset->VaelCostPerUse : 0.0f;
    UWTBRVaelComponent* Vael = OwnerCharacter.IsValid() ? OwnerCharacter->VaelComponent : nullptr;
    const float CurrentVael = IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] Activate Start | Owner=%s | Authority=%s | CurrentVael=%.2f | Cost=%.2f"),
        *GetNameSafe(OwnerChar),
        bHasAuthority ? TEXT("true") : TEXT("false"),
        CurrentVael,
        Cost);

    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] Activate Abort | Owner=%s | Reason=DataAsset invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    if (!IsValid(Vael) || !Vael->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] ConsumeFail | CurrentVael=%.2f | Cost=%.2f"),
            IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f,
            DataAsset->VaelCostPerUse);
        return false;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] ConsumeSuccess | NewVael=%.2f | Cost=%.2f"),
        Vael->GetCurrentVael(),
        DataAsset->VaelCostPerUse);

    if (WallActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=null | Reason=WallActorClass null"));
        return false;
    }
    TSubclassOf<AWTBRAegornWallActor> WallClass =
        WallActorClass.LoadSynchronous();
    if (!IsValid(WallClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s | Reason=Load failed"),
            *WallActorClass.ToString());
        return false;
    }

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * 150.0f;
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnAttempt | Class=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(WallClass.Get()),
        *SpawnLoc.ToString(),
        *SpawnRot.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWTBRAegornWallActor* Wall =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            WallClass, SpawnLoc, SpawnRot, Params);

    if (!IsValid(Wall))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s"),
            *GetNameSafe(WallClass.Get()));
        return false;
    }

    Wall->InitializeWall(DataAsset->EscudoParams.EscudoWallHP);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnSuccess | Wall=%s | Location=%s | Rotation=%s | WallHP=%.1f | MaxWallHP=%.1f | Replicates=%s"),
        *GetNameSafe(Wall),
        *Wall->GetActorLocation().ToString(),
        *Wall->GetActorRotation().ToString(),
        Wall->WallHP,
        Wall->MaxWallHP,
        Wall->GetIsReplicated() ? TEXT("true") : TEXT("false"));

    OnWallPlaced.Broadcast();
    OnEscudoWallSpawned(Wall);
    return true;
}
