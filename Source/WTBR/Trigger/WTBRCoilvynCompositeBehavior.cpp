// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCoilvynCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRCoilvynCompositeBehavior::ExecuteComposite(
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
    const FRotator SpawnRotation = AimRotation;
    const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

    TArray<TArray<FVector>> CubeWorldPaths;
    ResolveCompositeCubePaths(
        OwningCharacter, Definition, SpawnLocation, SpawnRotation, CubeWorldPaths);
    if (CubeWorldPaths.Num() == 0) return false;

    // Budget is SPLIT across cubes, never multiplied.
    const float PerCubeDamage =
        Definition.TotalDamageBudget / static_cast<float>(CubeWorldPaths.Num());

    // Acquired ONCE for the whole volley, not per cube: the design lock says
    // Coilvyn homes on one target with no reacquire, so every cube must converge
    // on the same body. Re-querying per cube could split the volley across
    // targets as the first cubes travel.
    AWTBRCharacter* HomingTarget = nullptr;
    if (Definition.HomingParams.bEnableHoming)
    {
        HomingTarget = AWTBRCharacter::FindBestHomingTarget(
            OwningCharacter,
            Definition.HomingParams.TargetAcquisitionRange,
            Definition.HomingParams.AimConeHalfAngleDegrees);
    }

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
            false,
            0.0f);

        if (IsValid(HomingTarget))
        {
            // Per-lane continue direction — each cube peels toward the target
            // from wherever its own path ended, which is what makes a multi-lane
            // preset converge instead of all turning the same way.
            FVector ContinueDirection = AimDirection;
            const FVector LastSegment = PathPoints.Last() - PathPoints[PathPoints.Num() - 2];
            if (!LastSegment.IsNearlyZero())
            {
                ContinueDirection = LastSegment.GetSafeNormal();
            }

            Projectile->bHomeAfterPathEnd = true;
            Projectile->PathEndHomingTarget = HomingTarget->GetRootComponent();
            Projectile->PathEndHomingAcceleration = Definition.HomingParams.HomingAcceleration;
            Projectile->PathEndContinueDirection = ContinueDirection;
        }

        // Per-composite look from the registry — keeps one shared projectile BP viable.
        Projectile->ApplyVFXConfig(Definition.VFX);
        Projectile->FinishSpawning(SpawnTransform);
        Projectile->InitializePathMovement(PathPoints, Definition.ProjectileSpeed, OwningCharacter);
        bAnySpawned = true;
    }
    return bAnySpawned;
}
