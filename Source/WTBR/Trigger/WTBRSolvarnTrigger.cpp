// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSolvarnTrigger.h"
#include "Actors/WTBRSolvarnField.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRSolvarnTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRSolvarnParams& Params = DataAsset->SolvarnParams;
    if (!Params.SolvarnFieldClass) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(Params.SolvarnVaelCost)) return false;

    UWorld* World = OwnerCharacter->GetWorld();
    const FTransform SpawnTF(FQuat::Identity, OwnerCharacter->GetActorLocation());

    AWTBRSolvarnField* Field = World->SpawnActorDeferred<AWTBRSolvarnField>(
        Params.SolvarnFieldClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Field)) return false;

    Field->InitializeField(OwnerCharacter.Get(),
        Params.SolvarnRadius, Params.SolvarnDuration, Params.SolvarnVaelDrainPerSec);
    Field->FinishSpawning(SpawnTF);

    return true;
}
