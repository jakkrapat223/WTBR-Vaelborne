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
        Params.SoluxTotalDamage,
        Params.SoluxSpeed,
        Params.SoluxTapScatterRadius,
        /*ConvergeDistance=*/Params.SoluxRange);

    StartCooldown();
    return true;
}

const TArray<FWTBRPathPreset>* UWTBRSoluxTrigger::GetHoldPresets() const
{
    return IsValid(DataAsset) ? &DataAsset->SoluxParams.SoluxPresets : nullptr;
}

float UWTBRSoluxTrigger::GetHoldVaelCost() const
{
    return IsValid(DataAsset) ? DataAsset->SoluxParams.SoluxPresetVaelCost : 0.0f;
}

float UWTBRSoluxTrigger::GetHoldChargeSeconds() const
{
    if (!IsValid(DataAsset)) return 1.2f;
    return FMath::Max(0.05f, DataAsset->SoluxParams.SoluxPresetFullChargeSeconds);
}

bool UWTBRSoluxTrigger::FireHoldPreset(
    int32 PresetIndex, float ChargeFraction, bool /*bIsMain*/)
{
    if (!IsValid(DataAsset)) return false;
    const FWTBRSoluxParams& Params = DataAsset->SoluxParams;

    FWTBRPresetShot Shot;
    Shot.Presets = &Params.SoluxPresets;
    Shot.ProjectileClass = Params.SoluxProjectileClass;
    Shot.VaelCost = Params.SoluxPresetVaelCost;
    // Same number tap spends. Hold changes the shape, never the budget.
    Shot.TotalDamage = Params.SoluxTotalDamage;
    Shot.Speed = Params.SoluxSpeed;
    Shot.MinRange = Params.SoluxPresetMinRange;
    Shot.MaxRange = Params.SoluxRange;
    Shot.ScatterRadius = Params.SoluxPresetScatterRadius;
    // No payload fields set on purpose: Asteroid cubes do not explode and do not
    // hunt. Leaving them at zero IS the archetype.

    return FirePresetVolley(PresetIndex, ChargeFraction, Shot);
}

float UWTBRSoluxTrigger::GetCooldownDuration() const
{
    return IsValid(DataAsset) ? DataAsset->SoluxParams.SoluxFireCooldown : 0.5f;
}
