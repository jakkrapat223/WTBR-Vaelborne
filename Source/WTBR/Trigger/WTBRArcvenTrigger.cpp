// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRArcvenTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRArcvenTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRArcvenParams& Params = DataAsset->ArcvenParams;
    if (!Params.ArcProjectileClass) return false;

    const FVector Forward    = OwnerCharacter->GetActorForwardVector();
    const FVector Right      = OwnerCharacter->GetActorRightVector();
    // Spawn 100u in front of character centre to avoid self-overlap
    const FVector BaseOrigin = OwnerCharacter->GetActorLocation() + Forward * 100.0f;

    if (bIsDualWield)
    {
        // Option A: two parallel waves offset left and right along the Right vector
        constexpr float LateralOffset = 60.0f;
        const float SingleDamage = Params.DualArcTotalDamage * 0.5f;

        FireArcWave(BaseOrigin - Right * LateralOffset, Forward, SingleDamage);
        FireArcWave(BaseOrigin + Right * LateralOffset, Forward, SingleDamage);
    }
    else
    {
        FireArcWave(BaseOrigin, Forward, Params.ArcDamage);
    }

    // Dispatch radar ping — Arcven Vael exits character capsule on every fire
    if (DataAsset->bDispatchesActionPing || Params.bPingOnFire)
    {
        if (UWTBRActionPingSubsystem* PingSys =
            OwnerCharacter->GetWorld()->GetSubsystem<UWTBRActionPingSubsystem>())
        {
            PingSys->RegisterActionPing(OwnerCharacter.Get());
        }
    }

    OnArcvenFired(bIsDualWield);
    return true;
}

void UWTBRArcvenTrigger::FireArcWave(FVector Origin, FVector Direction, float Damage)
{
    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) return;

    const FWTBRArcvenParams& Params = DataAsset->ArcvenParams;
    const FTransform SpawnTF(Direction.Rotation(), Origin);

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.ArcProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return;

    Proj->InitializeProjectile(
        Damage,
        Params.ArcWaveSpeed,
        ETriggerCategory::Melee,
        false,  // bIsSniper
        false,  // bExplodeOnImpact
        0.0f);  // ExplosionRadius

    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(Direction, OwnerCharacter.Get());
}
