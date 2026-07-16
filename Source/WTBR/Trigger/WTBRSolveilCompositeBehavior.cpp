// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRSolveilCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRSolveilCompositeBehavior::ExecuteComposite(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition)
{
    if (!IsValid(OwningCharacter) || !Definition.ProjectileClass) return false;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World) return false;

    const FVector SpawnLocation = OwningCharacter->GetActorLocation()
        + OwningCharacter->GetActorForwardVector() * 100.0f;
    const FRotator SpawnRotation = OwningCharacter->GetActorRotation();
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
    Projectile->bCanPenetrate = Definition.bCanPenetrate;
    Projectile->FinishSpawning(SpawnTransform);
    Projectile->InitializePathMovement(CubeWorldPaths[0], Definition.ProjectileSpeed, OwningCharacter);
    return true;
}
