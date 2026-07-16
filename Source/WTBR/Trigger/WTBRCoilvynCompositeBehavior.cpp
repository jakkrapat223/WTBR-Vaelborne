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
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Definition.PathPreset, SpawnLocation, SpawnRotation, Definition.PathRange, CubeWorldPaths);
    if (CubeWorldPaths.Num() == 0) return false;

    const TArray<FVector>& PathPoints = CubeWorldPaths[0];

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
        false,
        0.0f);

    if (Definition.HomingParams.bEnableHoming)
    {
        if (AWTBRCharacter* Target = AWTBRCharacter::FindBestHomingTarget(
            OwningCharacter,
            Definition.HomingParams.TargetAcquisitionRange,
            Definition.HomingParams.AimConeHalfAngleDegrees))
        {
            FVector ContinueDirection = AimDirection;
            if (PathPoints.Num() >= 2)
            {
                const FVector LastSegment = PathPoints.Last() - PathPoints[PathPoints.Num() - 2];
                if (!LastSegment.IsNearlyZero())
                {
                    ContinueDirection = LastSegment.GetSafeNormal();
                }
            }

            Projectile->bHomeAfterPathEnd = true;
            Projectile->PathEndHomingTarget = Target->GetRootComponent();
            Projectile->PathEndHomingAcceleration = Definition.HomingParams.HomingAcceleration;
            Projectile->PathEndContinueDirection = ContinueDirection;
        }
    }

    Projectile->FinishSpawning(SpawnTransform);
    Projectile->InitializePathMovement(PathPoints, Definition.ProjectileSpeed, OwningCharacter);
    return true;
}
