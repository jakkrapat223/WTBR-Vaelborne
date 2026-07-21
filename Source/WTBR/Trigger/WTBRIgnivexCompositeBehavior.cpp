// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRIgnivexCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRIgnivexCompositeBehavior::ExecuteComposite(
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
    const FVector SpawnLocation = EyeLocation + AimRotation.Vector() * 100.0f;
    const FRotator SpawnRotation = AimRotation;
    const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

    TArray<TArray<FVector>> CubeWorldPaths;
    ResolveCompositeCubePaths(
        OwningCharacter, Definition, SpawnLocation, SpawnRotation, CubeWorldPaths);
    if (CubeWorldPaths.Num() == 0) return false;

    // Budget is SPLIT across cubes, never multiplied. Each cube keeps the full
    // ExplosionRadius, so a multi-lane preset turns Tomahawk into a cluster of
    // small blasts covering more ground rather than one bigger blast.
    const float PerCubeDamage =
        Definition.TotalDamageBudget / static_cast<float>(CubeWorldPaths.Num());

    bool bAnySpawned = false;
    for (const TArray<FVector>& PathPoints : CubeWorldPaths)
    {
        if (PathPoints.Num() < 2) continue;

        AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Definition.ProjectileClass,
            SpawnTransform,
            OwningCharacter,
            OwningCharacter,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Projectile)) continue;

        Projectile->InitializeProjectile(
            PerCubeDamage,
            Definition.ProjectileSpeed,
            ETriggerCategory::Gunner,
            false,
            Definition.ExplosionParams.bExplodes,
            Definition.ExplosionParams.ExplosionRadius);
        // Per-composite look from the registry — keeps one shared projectile BP viable.
        Projectile->ApplyVFXConfig(Definition.VFX);
        Projectile->FinishSpawning(SpawnTransform);
        Projectile->InitializePathMovement(PathPoints, Definition.ProjectileSpeed, OwningCharacter);
        bAnySpawned = true;
    }
    return bAnySpawned;
}
