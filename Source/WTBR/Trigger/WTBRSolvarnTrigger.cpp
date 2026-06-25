// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSolvarnTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRSolvarnField.h"
#include "Kismet/GameplayStatics.h"

bool UWTBRSolvarnTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp)) return false;
    if (!VaelComp->TryConsumeVael(DataAsset->SolvarnParams.SolvarnVaelCost)) return false;

    const TSubclassOf<AWTBRSolvarnField> FieldClass = DataAsset->SolvarnParams.SolvarnFieldClass;
    const float Radius          = DataAsset->SolvarnParams.SolvarnRadius;
    const float Duration        = DataAsset->SolvarnParams.SolvarnDuration;
    const float VaelDrainPerSec = DataAsset->SolvarnParams.SolvarnVaelDrainPerSec;

    if (!IsValid(FieldClass)) return false;

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation();
    const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLoc);

    AWTBRSolvarnField* Field = GetWorld()->SpawnActorDeferred<AWTBRSolvarnField>(
        FieldClass, SpawnTransform, OwnerCharacter.Get(), OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Field)) return false;

    // Actual signature: (InOwner, InRadius, InDuration, InVaelDrainPerSec)
    Field->InitializeField(OwnerCharacter.Get(), Radius, Duration, VaelDrainPerSec);

    UGameplayStatics::FinishSpawningActor(Field, SpawnTransform);

    // Solvarn doesn't call FireBlackProjectile — manually fire the VFX event
    OnBlackTriggerFired(OwnerCharacter->GetActorForwardVector());

    StartCooldown();
    return true;
}

float UWTBRSolvarnTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 2.0f;
    return 5.0f; // ⚠ Playtest placeholder
}
