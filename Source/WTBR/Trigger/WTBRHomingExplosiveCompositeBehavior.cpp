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

    const FVector SpawnLocation = OwningCharacter->GetActorLocation()
        + OwningCharacter->GetActorForwardVector() * 100.0f;
    const FRotator SpawnRotation = OwningCharacter->GetActorRotation();
    const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

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
    Projectile->Launch(OwningCharacter->GetActorForwardVector(), OwningCharacter);

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
