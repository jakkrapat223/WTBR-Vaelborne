// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCatarixCompositeBehavior.h"

#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRCompositeRegistryDataAsset.h"
#include "WTBRCharacter.h"

bool UWTBRCatarixCompositeBehavior::ExecuteComposite(
    AWTBRCharacter* OwningCharacter,
    const FWTBRCompositeDefinition& Definition)
{
    if (!IsValid(OwningCharacter) || !Definition.ProjectileClass) return false;

    UWorld* World = OwningCharacter->GetWorld();
    if (!World) return false;

    const float SecondRatio = FMath::Clamp(
        Definition.ExplosionParams.SecondBlastDamageRatio, 0.0f, 1.0f);
    const float FirstBlastDamage = Definition.TotalDamageBudget * (1.0f - SecondRatio);
    const float SecondBlastDamage = Definition.TotalDamageBudget * SecondRatio;
    // Aim from the camera view, not the capsule — GetActorRotation() has no pitch,
    // so firing upward/downward is impossible with it (same convention as Serpveil).
    FVector EyeLocation;
    FRotator AimRotation;
    OwningCharacter->GetActorEyesViewPoint(EyeLocation, AimRotation);
    AimRotation.Roll = 0.0f;
    const FVector AimDirection = AimRotation.Vector();
    const FVector SpawnLocation = EyeLocation + AimDirection * 100.0f;
    const FTransform SpawnTransform(AimRotation, SpawnLocation);

    AWTBRProjectileBase* Projectile = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Definition.ProjectileClass,
        SpawnTransform,
        OwningCharacter,
        OwningCharacter,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Projectile)) return false;

    Projectile->InitializeProjectile(
        FirstBlastDamage,
        Definition.ProjectileSpeed,
        ETriggerCategory::Gunner,
        false,
        Definition.ExplosionParams.bExplodes,
        Definition.ExplosionParams.ExplosionRadius);
    if (Definition.ExplosionParams.bHasSecondBlast)
    {
        Projectile->bHasDelayedSecondExplosion = true;
        Projectile->SecondExplosionDelay = Definition.ExplosionParams.SecondBlastDelay;
        Projectile->SecondExplosionRadius = Definition.ExplosionParams.SecondBlastRadius;
        Projectile->SecondExplosionDamage = SecondBlastDamage;
    }
    Projectile->FinishSpawning(SpawnTransform);
    Projectile->Launch(AimDirection, OwningCharacter);
    return true;
}
