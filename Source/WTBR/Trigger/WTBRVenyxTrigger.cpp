// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVenyxTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"

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

    const FWTBRVenyxParams& Params = DataAsset->VenyxParams;

    // Conjure, split, fire.
    const TArray<AWTBRProjectileBase*> Cubes = FireProjectileVolley(
        Params.VenyxProjectileClass,
        Params.VenyxTapCubeCount,
        Params.VenyxTotalDamage,
        Params.VenyxSpeed,
        Params.VenyxTapScatterRadius,
        /*ConvergeDistance=*/Params.VenyxRange);

    StartCooldown();

    // Nothing is locked at fire time. Each cube flies straight and hunts as it
    // travels: an enemy that comes within VenyxTapHomingRadius of a cube in flight
    // gets chased, and a cube that passes nobody carries on like an ordinary bullet.
    //
    // The old flow picked a target from the caster's aim cone BEFORE launching, so
    // every shot bent away from the crosshair the instant it left and a miss was not
    // really possible. Acquiring in flight is also why each cube can end up on a
    // different enemy without any target-splitting rule: they sweep different ground.
    //
    // Same machinery the Venyx presets and composites use — see
    // AWTBRProjectileBase::EnableProximityHoming.
    for (AWTBRProjectileBase* Cube : Cubes)
    {
        if (IsValid(Cube))
        {
            Cube->EnableProximityHoming(
                Params.VenyxTapHomingRadius,
                Params.VenyxHomingTurnRateDegreesPerSecond,
                /*bReacquireAfterOvershoot=*/false,
                Params.VenyxProximityDetonationRadius);
        }
    }

    return true;
}

const TArray<FWTBRPathPreset>* UWTBRVenyxTrigger::GetHoldPresets() const
{
    if (!IsValid(DataAsset)) return nullptr;

    if (!bCombinedPresetsCacheValid)
    {
        CombinedPresetsCache = DataAsset->VenyxParams.VenyxPresets;
        bCombinedPresetsCacheValid = true;
    }
    return &CombinedPresetsCache;
}

void UWTBRVenyxTrigger::RefreshCustomHoldPresets(const TArray<FWTBRPathPreset>& InCustomPresets)
{
    if (!IsValid(DataAsset)) return;

    CombinedPresetsCache = DataAsset->VenyxParams.VenyxPresets;
    CombinedPresetsCache.Append(InCustomPresets);
    bCombinedPresetsCacheValid = true;
}

float UWTBRVenyxTrigger::GetHoldVaelCost() const
{
    return IsValid(DataAsset) ? DataAsset->VenyxParams.VenyxPresetVaelCost : 0.0f;
}

float UWTBRVenyxTrigger::GetHoldChargeSeconds() const
{
    if (!IsValid(DataAsset)) return 1.2f;
    return FMath::Max(0.05f, DataAsset->VenyxParams.VenyxPresetFullChargeSeconds);
}

bool UWTBRVenyxTrigger::FireHoldPreset(
    int32 PresetIndex, float ChargeFraction, bool /*bIsMain*/)
{
    if (!IsValid(DataAsset)) return false;
    const FWTBRVenyxParams& Params = DataAsset->VenyxParams;

    FWTBRPresetShot Shot;
    // GetHoldPresets(), not &Params.VenyxPresets directly — this is what lets a
    // player-authored custom preset (Preset Editor) actually fire: the wheel and
    // the fire path must agree on the exact same array, or an index the wheel
    // showed could resolve to the wrong entry (or none) here.
    Shot.Presets = GetHoldPresets();
    Shot.ProjectileClass = Params.VenyxProjectileClass;
    Shot.VaelCost = Params.VenyxPresetVaelCost;
    Shot.TotalDamage = Params.VenyxTotalDamage;
    Shot.Speed = Params.VenyxSpeed;
    Shot.MinRange = Params.VenyxPresetMinRange;
    Shot.MaxRange = Params.VenyxRange;
    Shot.ScatterRadius = Params.VenyxPresetScatterRadius;
    Shot.MinCubeCount = Params.VenyxTapCubeCount;
    Shot.MaxCubeCount = Params.VenyxPresetMaxCubeCount;
    // Hound's payload: every cube hunts. The turn cap matters MOST here — a lane
    // authored to sweep wide and come back at the target from behind only reads that
    // way if the cube cannot pivot in place the moment it acquires.
    Shot.HomingTurnRateDegPerSec = Params.VenyxHomingTurnRateDegreesPerSecond;
    Shot.ProximityDetonationRadius = Params.VenyxProximityDetonationRadius;

    // ONE hunting radius for the weapon, used by tap and by every preset alike.
    // The Preset Editor deliberately does not author this per lane — a preset says
    // where along its path hunting is on or off, never how far it reaches.
    //
    // ⚠ The field is still named VenyxTapHomingRadius from when tap was its only
    // user. It is the weapon's radius now; the name is under-descriptive rather than
    // wrong, and renaming a UPROPERTY would drop the value already authored in
    // DA_Venyx.
    Shot.HomingRadiusUU = Params.VenyxTapHomingRadius;

    return FirePresetVolley(PresetIndex, ChargeFraction, Shot);
}

float UWTBRVenyxTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->VenyxParams.VenyxFireCooldown;
}
