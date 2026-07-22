// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSoluxTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRSoluxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    // CHILD CLASS extracts its specific params and passes them to BASE CLASS
    const FWTBRSoluxParams& Params = DataAsset->SoluxParams;

    // Conjure, split, fire. Solux's cubes carry no special property at all — that
    // absence IS its identity (canon: "no special properties, most powerful"), so
    // there is nothing to do after they launch.
    FireProjectileVolley(
        Params.SoluxProjectileClass,
        Params.SoluxTapCubeCount,
        Params.SoluxTapTotalDamage,
        Params.SoluxSpeed,
        Params.SoluxTapScatterRadius,
        /*ConvergeDistance=*/Params.SoluxRange);

    StartCooldown();
    return true;
}

float UWTBRSoluxTrigger::GetCooldownDuration() const
{
    return IsValid(DataAsset) ? DataAsset->SoluxParams.SoluxFireCooldown : 0.5f;
}
