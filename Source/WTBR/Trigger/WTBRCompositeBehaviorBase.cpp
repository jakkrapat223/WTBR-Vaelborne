// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRCharacter.h"

void UWTBRCompositeBehaviorBase::ResolveCompositeCubePaths(
    const AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition,
    const FVector& SpawnLocation,
    const FRotator& SpawnRotation,
    TArray<TArray<FVector>>& OutCubePaths)
{
    OutCubePaths.Reset();
    if (!IsValid(OwningCharacter)) return;

    const int32 PresetIndex = OwningCharacter->GetPendingCompositePresetIndex();

    // TAP (INDEX_NONE): a straight two-point path at FULL reach, built here rather
    // than read from data. Tap exists to be the reliable answer when someone is
    // already on top of you, so it must land where the player is aiming — an
    // authored curve would defeat the only reason it exists. Identity still comes
    // through, because the payload (explode / home / penetrate) is unchanged.
    if (PresetIndex == WTBR_COMPOSITE_PRESET_TAP)
    {
        const FRotationMatrix AimMatrix(SpawnRotation);
        const FVector EndPoint =
            SpawnLocation + AimMatrix.GetUnitAxis(EAxis::X) * Definition.PathRange;

        // Every cube starts somewhere on a sphere around the caster and ends on the
        // SAME aim point. That keeps tap perfectly reliable — whatever the spread
        // looks like leaving the body, it converges exactly where the player aimed.
        const int32 CubeCount = FMath::Max(1, Definition.CubeCount);
        for (int32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
        {
            const FVector Origin = SpawnLocation + AimMatrix.TransformVector(
                UWTBRCompositeRegistryDataAsset::ComputeFibonacciSphereOffset(
                    CubeIndex, CubeCount, Definition.TapScatterRadius));
            OutCubePaths.Add({Origin, EndPoint});
        }
        return;
    }

    // HOLD: fly the preset the player picked from the wheel, at a reach scaled by
    // how long they charged — the same bargain Serpveil itself offers.
    const TArray<FWTBRPathPreset>* Presets = OwningCharacter->GetReadyCompositePresets();
    if (!Presets || !Presets->IsValidIndex(PresetIndex))
    {
        UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
            Definition.PathPreset, SpawnLocation, SpawnRotation,
            Definition.PathRange, OutCubePaths,
            /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true,
            /*TotalCubeOverride=*/FMath::Max(1, Definition.CubeCount));
        return;
    }

    const float MinRange = (Definition.PathRangeMin > 0.0f)
        ? FMath::Min(Definition.PathRangeMin, Definition.PathRange)
        : Definition.PathRange;
    const float Range = FMath::Lerp(
        MinRange, Definition.PathRange, OwningCharacter->GetPendingCompositeChargeFraction());

    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        (*Presets)[PresetIndex], SpawnLocation, SpawnRotation, Range, OutCubePaths,
        /*ScatterRadius=*/0.0f, /*bIsMainSlot=*/true,
        /*TotalCubeOverride=*/FMath::Max(1, Definition.CubeCount));
}
