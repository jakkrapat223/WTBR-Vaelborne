// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRTelornTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRTelornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRTelornParams& Params = DataAsset->TelornParams;
    if (!Params.TelornProjectileClass) return false;

    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Origin  = OwnerCharacter->GetActorLocation() + Forward * 100.0f;
    const FTransform SpawnTF(Forward.Rotation(), Origin);

    UWorld* World = OwnerCharacter->GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.TelornProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return false;

    Proj->MaxRange = Params.TelornRange;
    Proj->InitializeProjectile(Params.TelornDamage, Params.TelornSpeed,
        ETriggerCategory::SniperBullet, /*bSniper=*/true, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(Forward, OwnerCharacter.Get());

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnTelornFired();
    return true;
}
