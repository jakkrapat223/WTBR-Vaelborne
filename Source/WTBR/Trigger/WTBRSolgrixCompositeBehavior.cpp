// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRSolgrixCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRSolgrixCompositeBehavior::ExecuteComposite(
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

    TArray<TArray<FVector>> CubeWorldPaths;
    ResolveCompositeCubePaths(
        OwningCharacter, Definition, SpawnLocation, AimRotation, CubeWorldPaths);
    if (CubeWorldPaths.Num() == 0) return false;

    // Budget is SPLIT across cubes, never multiplied.
    const float PerCubeDamage =
        Definition.TotalDamageBudget / static_cast<float>(CubeWorldPaths.Num());

    bool bAnySpawned = false;
    for (const TArray<FVector>& PathPoints : CubeWorldPaths)
    {
        if (PathPoints.Num() < 2) continue;

        // Spawn on THIS cube's own path start — a shared spawn point makes the
        // volley overlap and destroy itself before it can move.
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
            Definition.ProjectileSpeed,
            ETriggerCategory::Gunner,
            false,
            Definition.ExplosionParams.bExplodes,
            Definition.ExplosionParams.ExplosionRadius);
        if (Definition.ExplosionParams.bIsShapedCharge)
        {
            Projectile->bIsShapedCharge = true;
            Projectile->ShapedChargeConeHalfAngleDegrees =
                Definition.ExplosionParams.ShapedChargeConeHalfAngleDegrees;

            // Point the cone along THIS cube's own final leg, so a multi-lane preset
            // has each blast facing the way its own cube was travelling instead of
            // all of them sharing the caster's aim.
            const FVector FinalLeg = PathPoints.Last() - PathPoints[PathPoints.Num() - 2];
            Projectile->ShapedChargeDirection =
                FinalLeg.IsNearlyZero() ? AimDirection : FinalLeg.GetSafeNormal();
        }
        // Per-composite look from the registry — keeps one shared projectile BP viable.
        Projectile->ApplyVFXConfig(Definition.VFX);
        Projectile->FinishSpawning(CubeSpawnTransform);

        // A tap resolves to a straight two-point path, so this covers the old
        // single-shot behaviour as well — the shaped-charge cone still points
        // wherever the cube is travelling.
        Projectile->InitializePathMovement(PathPoints, Definition.ProjectileSpeed, OwningCharacter);
        bAnySpawned = true;
    }
    return bAnySpawned;
}
