// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRTelornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRTelornTrigger::ExecuteFire()
{
    if (!IsValid(DataAsset)) return false;

    if (!OwnerCharacter->VaelComponent ||
        !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        return false;
    }

    const FWTBRTelornParams& Params = DataAsset->TelornParams;
    FireSniper(Params.TelornProjectileClass, Params.TelornDamage, Params.TelornSpeed,
        Params.TelornRange, false, Params.TelornCubeSplitCount);
    return true;
}

float UWTBRTelornTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->TelornParams.TelornFireCooldown;
}

float UWTBRTelornTrigger::GetZoomFOV() const
{
    if (!IsValid(DataAsset)) return Super::GetZoomFOV();
    return DataAsset->TelornParams.TelornZoomFOV;
}
