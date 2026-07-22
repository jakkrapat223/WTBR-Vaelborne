// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRLabyrnCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"

namespace
{
    // ⚠ PLAYTEST PENDING. Where the ease-off begins, as a fraction of the lane's OWN
    // path, and how slow the cube is at the very end relative to its cruise speed.
    // Owner's stated intent: faster than a Viper for the first four fifths, then slow
    // enough at the end that arrival is something a target can see coming.
    //
    // Both are fractions of each lane individually, not of the volley. A lane
    // authored to cut in close therefore eases off over a short distance and a long
    // reaching lane over a longer one, which is the intended reading — the shape of
    // the profile stays the same however the lane was drawn.
    constexpr float LABYRN_SLOW_FROM_FRACTION = 0.80f;
    constexpr float LABYRN_END_SPEED_FACTOR   = 0.35f;
}

bool UWTBRLabyrnCompositeBehavior::ExecuteComposite(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition)
{
    if (!IsValid(OwningCharacter) || !Definition.ProjectileClass) return false;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World) return false;

    // Aim from the camera view, not the capsule — GetActorRotation() has no pitch,
    // so firing up or down is impossible with it (same convention as Serpveil).
    FVector EyeLocation;
    FRotator AimRotation;
    OwningCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    AimRotation.Roll = 0.0f;
    const FVector SpawnLocation = EyeLocation + AimRotation.Vector() * 100.0f;

    TArray<TArray<FVector>> CubeWorldPaths;
    TArray<FWTBRResolvedCubeLaunch> CubeLaunches;
    ResolveCompositeCubePaths(
        OwningCharacter, Definition, SpawnLocation, AimRotation, CubeWorldPaths, &CubeLaunches);
    if (CubeWorldPaths.Num() == 0) return false;

    // One speed for every cube. Lanes of different lengths land at different times,
    // and that is the design — see the header.
    //
    // NOTE this is the AVERAGE speed, not the cruise speed. SetPathSpeedProfile
    // redistributes the flight time without changing its total, so the ease-off at
    // the end is paid for by the earlier stretch running faster than this number.
    // Definition.ProjectileSpeed must therefore be authored as the average a Labyrn
    // cube should hold over its whole flight, not as the speed it looks like it is
    // travelling at when it leaves.
    const float Speed = Definition.ProjectileSpeed;

    // Budget is SPLIT across the volley, never multiplied.
    const float PerCubeDamage =
        Definition.TotalDamageBudget / static_cast<float>(CubeWorldPaths.Num());

    WTBR_VALIDATION_LOG(Log,
        TEXT("[Labyrn] Volley | Owner=%s | Cubes=%d | Speed=%.0f"),
        *GetNameSafe(OwningCharacter), CubeWorldPaths.Num(), Speed);

    bool bAnySpawned = false;
    for (int32 CubeIndex = 0; CubeIndex < CubeWorldPaths.Num(); ++CubeIndex)
    {
        const TArray<FVector>& PathPoints = CubeWorldPaths[CubeIndex];
        if (PathPoints.Num() < 2) continue;

        // Spawn on this cube's own path start, not a shared muzzle point: cubes
        // conjured on one spot overlap and destroy each other before they move.
        const FTransform CubeSpawnTransform(AimRotation, PathPoints[0]);

        AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Definition.ProjectileClass,
            CubeSpawnTransform,
            OwningCharacter,
            OwningCharacter,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Projectile)) continue;

        Projectile->InitializeProjectile(
            PerCubeDamage,
            Speed,
            ETriggerCategory::Gunner,
            /*bSniper=*/false,
            // Labyrn is pure geometry: no homing anywhere, and explosions belong to
            // Ignivex, Catarix and Solgrix.
            /*bExplode=*/false,
            /*ExplodeRadius=*/0.0f);

        Projectile->SetPathSpeedProfile(LABYRN_SLOW_FROM_FRACTION, LABYRN_END_SPEED_FACTOR);

        // Per-composite look from the registry — keeps one shared projectile BP viable.
        Projectile->ApplyVFXConfig(Definition.VFX);
        Projectile->FinishSpawning(CubeSpawnTransform);
        Projectile->SchedulePathMovement(
            PathPoints, Speed, OwningCharacter,
            CubeLaunches.IsValidIndex(CubeIndex) ? CubeLaunches[CubeIndex].DelaySeconds : 0.0f);
        bAnySpawned = true;
    }

    return bAnySpawned;
}
