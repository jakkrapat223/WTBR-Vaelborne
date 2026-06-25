// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRPiercexTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRPiercexTrigger::Activate_Implementation(
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

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->PiercexParams.PiercexProjectileClass;
    const float Damage    = DataAsset->PiercexParams.PiercexDamage;
    const float Speed     = DataAsset->PiercexParams.PiercexSpeed;
    const float Range     = DataAsset->PiercexParams.PiercexRange;
    const int32 CubeSplit = DataAsset->PiercexParams.PiercexCubeSplitCount;

    FireSniper(ProjClass, Damage, Speed, Range, true, CubeSplit);
    StartCooldown();
    return true;
}

float UWTBRPiercexTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->PiercexParams.PiercexFireCooldown;
}
