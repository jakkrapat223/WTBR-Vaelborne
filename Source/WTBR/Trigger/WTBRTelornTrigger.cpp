// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRTelornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRTelornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    if (!OwnerCharacter->VaelComponent ||
        !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        return false;
    }

    const FWTBRTelornParams& Params = DataAsset->TelornParams;
    const TSubclassOf<AWTBRProjectileBase> ProjClass = Params.TelornProjectileClass;
    const float Damage    = Params.TelornDamage;
    const float Speed     = Params.TelornSpeed;
    const float Range     = Params.TelornRange;
    const int32 CubeSplit = Params.TelornCubeSplitCount;

    FireSniper(ProjClass, Damage, Speed, Range, false, CubeSplit);
    StartCooldown();
    return true;
}

float UWTBRTelornTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->TelornParams.TelornFireCooldown;
}
