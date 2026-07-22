// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRHomingExplosiveCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRHomingExplosiveCompositeBehavior::ExecuteComposite(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition)
{
    if (!IsValid(OwningCharacter) || !Definition.ProjectileClass) return false;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World) return false;

    // Aim from the camera view, not the capsule — GetActorRotation() has no pitch,
    // so firing upward/downward is impossible with it (same convention as Serpveil).
    FVector EyeLocation;
    FRotator AimRotation;
    OwningCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    AimRotation.Roll = 0.0f;
    const FVector AimDirection = AimRotation.Vector();
    const FVector SpawnLocation = EyeLocation + AimDirection * 100.0f;
    const FTransform SpawnTransform(AimRotation, SpawnLocation);

    // A Hound preset turns this composite into a swept volley instead of one
    // missile: cubes fly the authored arcs, in waves, and take whoever they pass
    // near. Only a HOLD with a Venyx preset produces a non-zero radius, so every
    // other caller of this shared behaviour — including its own tap — falls through
    // to the single-projectile route below untouched.
    TArray<TArray<FVector>> CubeWorldPaths;
    TArray<FWTBRResolvedCubeLaunch> CubeLaunches;
    ResolveCompositeCubePaths(
        OwningCharacter, Definition, SpawnLocation, AimRotation, CubeWorldPaths, &CubeLaunches);

    const bool bIsSweep = CubeLaunches.ContainsByPredicate(
        [](const FWTBRResolvedCubeLaunch& Launch) { return Launch.HomingRadiusUU > 0.0f; });

    if (bIsSweep)
    {
        return FireSweptVolley(
            OwningCharacter, Definition, AimRotation, CubeWorldPaths, CubeLaunches);
    }

    AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Definition.ProjectileClass,
        SpawnTransform,
        OwningCharacter,
        OwningCharacter,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Projectile)) return false;

    Projectile->InitializeProjectile(
        Definition.TotalDamageBudget,
        Definition.ProjectileSpeed,
        ETriggerCategory::Gunner,
        false,
        Definition.ExplosionParams.bExplodes,
        Definition.ExplosionParams.ExplosionRadius);
    // Per-composite look from the registry — keeps one shared projectile BP viable.
    Projectile->ApplyVFXConfig(Definition.VFX);
    Projectile->FinishSpawning(SpawnTransform);
    Projectile->Launch(AimDirection, OwningCharacter);

    if (Definition.HomingParams.bEnableHoming)
    {
        if (AWTBRCharacter* Target = AWTBRCharacter::FindBestHomingTarget(
            OwningCharacter,
            Definition.HomingParams.TargetAcquisitionRange,
            Definition.HomingParams.AimConeHalfAngleDegrees))
        {
            Projectile->EnableHoming(Target->GetRootComponent(), Definition.HomingParams.HomingAcceleration);
        }
    }

    return true;
}

