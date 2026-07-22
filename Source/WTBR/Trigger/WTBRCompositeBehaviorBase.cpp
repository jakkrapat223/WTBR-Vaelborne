// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"
#include "EngineUtils.h"

namespace
{
    // Diagnostics only. Reports how far the nearest living enemy actually is, so a
    // committed reach can be compared against the fight it was fired into.
    float WTBRNearestHostileDistance(const AWTBRCharacter* Shooter)
    {
        if (!IsValid(Shooter) || !Shooter->GetWorld()) return -1.0f;

        float NearestSq = TNumericLimits<float>::Max();
        for (TActorIterator<AWTBRCharacter> It(Shooter->GetWorld()); It; ++It)
        {
            const AWTBRCharacter* Candidate = *It;
            if (!IsValid(Candidate) || Candidate == Shooter) continue;
            if (!IsValid(Candidate->HealthComponent) || !Candidate->HealthComponent->IsAlive()) continue;
            if (Shooter->IsSameTeamAs(Candidate)) continue;
            NearestSq = FMath::Min(NearestSq,
                FVector::DistSquared(Shooter->GetActorLocation(), Candidate->GetActorLocation()));
        }
        return (NearestSq < TNumericLimits<float>::Max()) ? FMath::Sqrt(NearestSq) : -1.0f;
    }
}

void UWTBRCompositeBehaviorBase::ResolveCompositeCubePaths(
    const AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition,
    const FVector& SpawnLocation,
    const FRotator& SpawnRotation,
    TArray<TArray<FVector>>& OutCubePaths,
    TArray<FWTBRResolvedCubeLaunch>* OutCubeLaunches)
{
    OutCubePaths.Reset();
    if (OutCubeLaunches) OutCubeLaunches->Reset();
    if (!IsValid(OwningCharacter)) return;

    const int32 PresetIndex = OwningCharacter->GetPendingCompositePresetIndex();

    // Reach at zero charge, and what a TAP always fires at. Falls back to PathRange
    // when unset, so a definition that never opts into charge scaling behaves
    // exactly as it did before charge existed.
    const float MinRange = (Definition.PathRangeMin > 0.0f)
        ? FMath::Min(Definition.PathRangeMin, Definition.PathRange)
        : Definition.PathRange;

    // TAP: a straight two-point path at the UNCHARGED reach, built here rather than
    // read from data. Tap exists to be the reliable answer when someone is already
    // on top of you, so it must land where the player is aiming — an authored curve
    // would defeat the only reason it exists. Identity still comes through, because
    // the payload (explode / home / penetrate) is unchanged.
    //
    // It fires at MinRange, not PathRange, so that a barely-charged hold is never
    // WORSE than a tap — the design lock's "MinReach = BasicRange, MaxReach >
    // BasicRange" rule. Tap trades reach for being instant; hold buys reach and
    // shape by committing time.
    if (PresetIndex == WTBR_COMPOSITE_PRESET_TAP)
    {
        const FRotationMatrix AimMatrix(SpawnRotation);
        const FVector EndPoint =
            SpawnLocation + AimMatrix.GetUnitAxis(EAxis::X) * MinRange;

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
            // Zeroed on purpose: a tap never waves and never sweeps.
            if (OutCubeLaunches) OutCubeLaunches->AddDefaulted();
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
            /*ScatterRadius=*/Definition.TapScatterRadius, /*bIsMainSlot=*/true,
            /*TotalCubeOverride=*/FMath::Max(1, Definition.CubeCount),
            OutCubeLaunches);
        return;
    }

    const float Range = FMath::Lerp(
        MinRange, Definition.PathRange, OwningCharacter->GetPendingCompositeChargeFraction());

    // The committed reach is what every waypoint and homing radius is a fraction OF,
    // so a volley that lands nowhere near anybody is almost always this number not
    // matching the real fight distance — and the resolved radius alone cannot show
    // it, because the floor hides any range below where the floor bites.
    WTBR_VALIDATION_LOG(Log,
        TEXT("[Venyx Sweep] Volley | Owner=%s | Preset=%d | Charge=%.2f | Range=%.0fuu (min=%.0f max=%.0f) | NearestEnemy=%.0fuu"),
        *GetNameSafe(OwningCharacter), PresetIndex,
        OwningCharacter->GetPendingCompositeChargeFraction(),
        Range, MinRange, Definition.PathRange,
        WTBRNearestHostileDistance(OwningCharacter));

    // Scatter matters here for the same reason it does on a tap: without it every
    // cube is conjured on one point, they overlap, and they destroy each other
    // before InitializePathMovement ever runs.
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        (*Presets)[PresetIndex], SpawnLocation, SpawnRotation, Range, OutCubePaths,
        /*ScatterRadius=*/Definition.TapScatterRadius, /*bIsMainSlot=*/true,
        /*TotalCubeOverride=*/FMath::Max(1, Definition.CubeCount),
        OutCubeLaunches);
}

bool UWTBRCompositeBehaviorBase::FireSweptVolley(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition,
    const FRotator& SpawnRotation,
    const TArray<TArray<FVector>>& CubeWorldPaths,
    const TArray<FWTBRResolvedCubeLaunch>& CubeLaunches)
{
    return AWTBRProjectileBase::SpawnSweptVolley(
        OwningCharacter,
        Definition.ProjectileClass,
        Definition.TotalDamageBudget,
        Definition.ProjectileSpeed,
        Definition.ExplosionParams.bExplodes,
        Definition.ExplosionParams.ExplosionRadius,
        Definition.HomingParams.HomingAcceleration,
        SpawnRotation,
        CubeWorldPaths,
        CubeLaunches,
        &Definition.VFX) > 0;
}
