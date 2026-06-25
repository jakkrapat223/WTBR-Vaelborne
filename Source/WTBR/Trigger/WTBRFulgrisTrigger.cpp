// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgrisTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRFulgrisTrigger::Activate_Implementation(
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

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->FulgrisParams.FulgrisProjectileClass;
    const float Damage    = DataAsset->FulgrisParams.FulgrisDamage;
    const float Speed     = DataAsset->FulgrisParams.FulgrisSpeed;
    const float Range     = DataAsset->FulgrisParams.FulgrisRange;
    const int32 CubeSplit = DataAsset->FulgrisParams.FulgrisCubeSplitCount;

    FireSniper(ProjClass, Damage, Speed, Range, false, CubeSplit);
    StartCooldown();
    return true;
}

float UWTBRFulgrisTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 1.0f;
    return DataAsset->FulgrisParams.FulgrisFireCooldown;
}
