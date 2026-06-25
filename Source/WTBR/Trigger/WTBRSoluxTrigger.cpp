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
    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->SoluxParams.SoluxProjectileClass;
    const float Damage = DataAsset->SoluxParams.SoluxDamage;
    const float Speed  = DataAsset->SoluxParams.SoluxSpeed;

    FireProjectile(ProjClass, Damage, Speed, 0.0f);
    StartCooldown();
    return true;
}

float UWTBRSoluxTrigger::GetCooldownDuration() const
{
    return IsValid(DataAsset) ? DataAsset->SoluxParams.SoluxFireCooldown : 0.5f;
}
