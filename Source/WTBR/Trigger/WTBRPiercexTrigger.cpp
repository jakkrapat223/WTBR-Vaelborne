// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRPiercexTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRPiercexTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRPiercexParams& Params = DataAsset->PiercexParams;
    if (!Params.PiercexProjectileClass) return false;

    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Origin  = OwnerCharacter->GetActorLocation() + Forward * 100.0f;
    const FTransform SpawnTF(Forward.Rotation(), Origin);

    UWorld* World = OwnerCharacter->GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.PiercexProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return false;

    Proj->MaxRange      = Params.PiercexRange;
    Proj->bCanPenetrate = true; // Ibis: bullet passes through characters
    Proj->InitializeProjectile(Params.PiercexDamage, Params.PiercexSpeed,
        ETriggerCategory::SniperBullet, /*bSniper=*/true, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(Forward, OwnerCharacter.Get());

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnPiercexFired();
    return true;
}
