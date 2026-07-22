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

bool UWTBRVenyxTrigger::HasPresets() const
{
    return IsValid(DataAsset) && DataAsset->VenyxParams.VenyxPresets.Num() > 0;
}

float UWTBRVenyxTrigger::GetPresetFullChargeSeconds() const
{
    if (!IsValid(DataAsset)) return 1.2f;
    return FMath::Max(0.05f, DataAsset->VenyxParams.VenyxPresetFullChargeSeconds);
}

bool UWTBRVenyxTrigger::FireSelectedPreset(
    int32 PresetIndex, float ChargeFraction, bool /*bIsMain*/)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    const FWTBRVenyxParams& Params = DataAsset->VenyxParams;

    // Index comes from a client, so it is validated here rather than trusted.
    if (!Params.VenyxPresets.IsValidIndex(PresetIndex)) return false;
    if (!Params.VenyxProjectileClass) return false;

    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(Params.VenyxPresetVaelCost))
            return false;
    }

    // Aim from the camera view, not the capsule — GetActorRotation() has no pitch,
    // so firing upward is impossible with it, and firing upward is the entire point
    // of a Skyfall-shaped preset.
    FVector EyeLocation;
    FRotator AimRotation;
    OwnerCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    AimRotation.Roll = 0.0f;
    const FVector SpawnLocation = EyeLocation + AimRotation.Vector() * 100.0f;

    // Charge buys reach, and reach scales the WHOLE shape: waypoints and homing
    // radius are both authored as fractions of range, so one preset reads the same
    // at every charge level, just smaller.
    const float Range = FMath::Lerp(
        FMath::Min(Params.VenyxPresetMinRange, Params.VenyxRange),
        Params.VenyxRange,
        FMath::Clamp(ChargeFraction, 0.0f, 1.0f));

    TArray<TArray<FVector>> CubeWorldPaths;
    TArray<FWTBRResolvedCubeLaunch> CubeLaunches;
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Params.VenyxPresets[PresetIndex], SpawnLocation, AimRotation, Range,
        CubeWorldPaths,
        /*ScatterRadius=*/Params.VenyxPresetScatterRadius,
        /*bIsMainSlot=*/true,
        /*TotalCubeOverride=*/0,
        &CubeLaunches);

    const int32 Spawned = AWTBRProjectileBase::SpawnSweptVolley(
        OwnerCharacter.Get(),
        Params.VenyxProjectileClass,
        Params.VenyxPresetTotalDamage,
        Params.VenyxSpeed,
        /*bExplodes=*/false,
        /*ExplosionRadius=*/0.0f,
        Params.VenyxHomingAcceleration,
        AimRotation,
        CubeWorldPaths,
        CubeLaunches);

    if (Spawned <= 0) return false;

    StartCooldown();
    return true;
}

float UWTBRVenyxTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->VenyxParams.VenyxFireCooldown;
}
