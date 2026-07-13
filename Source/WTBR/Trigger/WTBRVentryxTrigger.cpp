// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVentryxTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRVentryxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    // Black Trigger: consume own VaelCost from VentryxParams, not VaelCostPerUse
    if (!OwnerCharacter->VaelComponent ||
        !OwnerCharacter->VaelComponent->TryConsumeVael(
            DataAsset->VentryxParams.VentryxVaelCost))
    {
        return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->VentryxParams.VentryxProjectileClass;
    const float Damage    = DataAsset->VentryxParams.VentryxDamage;
    const float Speed     = DataAsset->VentryxParams.VentryxSpeed;
    const float Knockback = DataAsset->VentryxParams.VentryxKnockback;

    FireBlackProjectile(ProjClass, Damage, Speed, Knockback, 1);
    StartCooldown();
    return true;
}

float UWTBRVentryxTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 2.0f;
    return DataAsset->VentryxParams.VentryxFireCooldown;
}
