// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVenyxTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRVenyxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->VenyxParams.VenyxProjectileClass;
    const float Damage        = DataAsset->VenyxParams.VenyxDamage;
    const float Speed         = DataAsset->VenyxParams.VenyxSpeed;
    const float HomingAccel   = DataAsset->VenyxParams.VenyxHomingAcceleration;
    const float SearchRadius  = DataAsset->VenyxParams.VenyxRange;
    const float AimConeHalfAngle = DataAsset->VenyxParams.VenyxAimConeHalfAngleDegrees;

    AWTBRProjectileBase* SpawnedProj =
        FireProjectile(ProjClass, Damage, Speed, 0.0f, false, 0.0f);

    StartCooldown();

    if (!IsValid(SpawnedProj)) return true;

    AWTBRCharacter* TargetCharacter = AWTBRCharacter::FindBestHomingTarget(
        OwnerCharacter.Get(), SearchRadius, AimConeHalfAngle);

    if (IsValid(TargetCharacter))
    {
        SpawnedProj->EnableHoming(TargetCharacter->GetRootComponent(), HomingAccel);
    }

    return true;
}

float UWTBRVenyxTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->VenyxParams.VenyxFireCooldown;
}
