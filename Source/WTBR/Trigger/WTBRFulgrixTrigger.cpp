// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRFulgrixTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;
    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    const FWTBRFulgrixParams& Params = DataAsset->FulgrixParams;

    UWorld* World = OwnerCharacter->GetWorld();

    // Conjure, split, fire — and every cube detonates (canon: Meteor's cubes each
    // explode). The blast radius drops and the impacts spread on purpose; both are
    // explained on FulgrixTapExplosionRadius / FulgrixTapImpactSpread. Together they
    // turn one big boom into a cluster instead of eight boxes hitting one spot.
    FireProjectileVolley(
        Params.FulgrixProjectileClass,
        Params.FulgrixTapCubeCount,
        Params.FulgrixTotalDamage,
        Params.FulgrixSpeed,
        Params.FulgrixTapScatterRadius,
        /*ConvergeDistance=*/Params.FulgrixRange,
        Params.FulgrixTapImpactSpread,
        /*bExplode=*/true,
        Params.FulgrixTapExplosionRadius);

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnFulgrixFired();
    StartCooldown();
    return true;
}

const TArray<FWTBRPathPreset>* UWTBRFulgrixTrigger::GetHoldPresets() const
{
    return IsValid(DataAsset) ? &DataAsset->FulgrixParams.FulgrixPresets : nullptr;
}

float UWTBRFulgrixTrigger::GetHoldVaelCost() const
{
    return IsValid(DataAsset) ? DataAsset->FulgrixParams.FulgrixPresetVaelCost : 0.0f;
}

float UWTBRFulgrixTrigger::GetHoldChargeSeconds() const
{
    if (!IsValid(DataAsset)) return 1.2f;
    return FMath::Max(0.05f, DataAsset->FulgrixParams.FulgrixPresetFullChargeSeconds);
}

bool UWTBRFulgrixTrigger::FireHoldPreset(
    int32 PresetIndex, float ChargeFraction, bool /*bIsMain*/)
{
    if (!IsValid(DataAsset)) return false;
    const FWTBRFulgrixParams& Params = DataAsset->FulgrixParams;

    FWTBRPresetShot Shot;
    Shot.Presets = &Params.FulgrixPresets;
    Shot.ProjectileClass = Params.FulgrixProjectileClass;
    Shot.VaelCost = Params.FulgrixPresetVaelCost;
    // Same number tap spends. Hold changes the shape, never the budget.
    Shot.TotalDamage = Params.FulgrixTotalDamage;
    Shot.Speed = Params.FulgrixSpeed;
    Shot.MinRange = Params.FulgrixPresetMinRange;
    Shot.MaxRange = Params.FulgrixRange;
    Shot.ScatterRadius = Params.FulgrixPresetScatterRadius;
    Shot.MinCubeCount = Params.FulgrixTapCubeCount;
    Shot.MaxCubeCount = Params.FulgrixPresetMaxCubeCount;
    // Meteor's payload: every cube detonates, at the same reduced radius tap uses.
    // A preset already spreads the impacts, so the full 300uu single-blast radius
    // would stack far more area than the shot is priced for.
    Shot.bExplodes = true;
    Shot.ExplosionRadius = Params.FulgrixTapExplosionRadius;

    return FirePresetVolley(PresetIndex, ChargeFraction, Shot);
}

float UWTBRFulgrixTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->FulgrixParams.FulgrixFireCooldown;
}
