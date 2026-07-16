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
    UWTBRCompositeRegistryDataAsset::ResolvePathPreset(
        Definition.PathPreset, SpawnLocation, SpawnRotation, Definition.PathRange, CubeWorldPaths);
    if (CubeWorldPaths.Num() == 0) return false;

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
    Projectile->FinishSpawning(SpawnTransform);
    Projectile->InitializePathMovement(CubeWorldPaths[0], Definition.ProjectileSpeed, OwningCharacter);
    return true;
}
