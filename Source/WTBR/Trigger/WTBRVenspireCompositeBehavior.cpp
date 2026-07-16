// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRVenspireCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRVenspireCompositeBehavior::ExecuteComposite(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition)
{
    if (!IsValid(OwningCharacter) || !Definition.ProjectileClass) return false;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World) return false;

    TArray<AWTBRCharacter*> Targets;
    if (Definition.HomingParams.bEnableHoming)
    {
        AWTBRCharacter::FindBestHomingTargets(
            OwningCharacter,
            Definition.HomingParams.TargetAcquisitionRange,
            Definition.HomingParams.AimConeHalfAngleDegrees,
            Definition.HomingParams.SimultaneousTargetCount,
            Targets);
    }

    const int32 NumProjectiles = FMath::Max(1, Targets.Num());
    const float DamagePerProjectile = Targets.Num() > 0
        ? FMath::Max(1.0f, Definition.TotalDamageBudget / static_cast<float>(NumProjectiles))
        : Definition.TotalDamageBudget;
    const FVector BaseSpawnLocation = OwningCharacter->GetActorLocation()
        + OwningCharacter->GetActorForwardVector() * 100.0f;
    const FRotator SpawnRotation = OwningCharacter->GetActorRotation();
    const FVector RightVector = OwningCharacter->GetActorRightVector();
    bool bSpawnedProjectile = false;

    for (int32 Index = 0; Index < NumProjectiles; ++Index)
    {
        const float LateralOffset = 50.0f * (Index - (NumProjectiles - 1) / 2.0f);
        const FTransform SpawnTransform(SpawnRotation, BaseSpawnLocation + RightVector * LateralOffset);
        AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Definition.ProjectileClass,
            SpawnTransform,
            OwningCharacter,
            OwningCharacter,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Projectile)) continue;

        Projectile->InitializeProjectile(
            DamagePerProjectile,
            Definition.ProjectileSpeed,
            ETriggerCategory::Gunner,
            false,
            Definition.ExplosionParams.bExplodes,
            Definition.ExplosionParams.ExplosionRadius);
        Projectile->FinishSpawning(SpawnTransform);
        Projectile->Launch(OwningCharacter->GetActorForwardVector(), OwningCharacter);

        if (Targets.IsValidIndex(Index))
        {
            Projectile->EnableHoming(
                Targets[Index]->GetRootComponent(),
                Definition.HomingParams.HomingAcceleration);
        }
        bSpawnedProjectile = true;
    }

    return bSpawnedProjectile;
}
