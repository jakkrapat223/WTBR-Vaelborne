// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSerpveilTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRSerpveilTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    if (!Params.SerpveilProjectileClass) return false;

    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Origin  = OwnerCharacter->GetActorLocation() + Forward * 100.0f;
    const FTransform SpawnTF(Forward.Rotation(), Origin);

    UWorld* World = OwnerCharacter->GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.SerpveilProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return false;

    // TODO Phase 4: Implement Custom Path System (curved trajectory)
    Proj->MaxRange = Params.SerpveilRange;
    Proj->InitializeProjectile(Params.SerpveilDamage, Params.SerpveilSpeed,
        ETriggerCategory::Gunner, false, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(Forward, OwnerCharacter.Get());

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnSerpveilFired();
    return true;
}
