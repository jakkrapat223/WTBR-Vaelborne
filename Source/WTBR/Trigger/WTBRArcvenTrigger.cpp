// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRArcvenTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

bool UWTBRArcvenTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown()) return false;
    if (!IsValid(DataAsset)) return false;

    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    FVector EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
    const FVector Forward = EyeRot.Vector();

    if (bIsDualWield)
    {
        const float WaveDamage = DataAsset->ArcvenParams.DualArcTotalDamage * 0.5f;
        const float SpreadRad  = FMath::DegreesToRadians(DataAsset->ArcvenParams.DualWaveSpreadAngle);
        const FVector Right    = FRotationMatrix(EyeRot).GetUnitAxis(EAxis::Y);

        FireArcWave((Forward - Right * FMath::Tan(SpreadRad)).GetSafeNormal(), WaveDamage);
        FireArcWave((Forward + Right * FMath::Tan(SpreadRad)).GetSafeNormal(), WaveDamage);
        OnArcvenFire(true, Forward);
    }
    else
    {
        FireArcWave(Forward, DataAsset->ArcvenParams.ArcDamage);
        OnArcvenFire(false, Forward);
    }

    OnMeleeSwing.Broadcast(bIsDualWield);
    StartCooldown();
    return true;
}

void UWTBRArcvenTrigger::FireArcWave(const FVector& Direction, float Damage)
{
    if (!OwnerCharacter.IsValid() || !GetWorld()) return;
    if (!IsValid(DataAsset)) return;

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->ArcvenParams.ArcProjectileClass;
    if (!IsValid(ProjClass))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBRArcvenTrigger: ArcProjectileClass not set in DataAsset — skipping fire"));
        return;
    }

    const FVector  SpawnLoc = OwnerCharacter->GetActorLocation() + Direction * 60.0f;
    const FRotator SpawnRot = Direction.Rotation();

    AWTBRProjectileBase* Proj = GetWorld()->SpawnActorDeferred<AWTBRProjectileBase>(
        ProjClass,
        FTransform(SpawnRot, SpawnLoc),
        OwnerCharacter.Get(),
        OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!IsValid(Proj)) return;

    Proj->MaxRange = DataAsset->ArcvenParams.ArcRange;
    Proj->InitializeProjectile(
        Damage,
        DataAsset->ArcvenParams.ArcWaveSpeed,
        ETriggerCategory::Melee,
        false, false, 0.0f);

    Proj->FinishSpawning(FTransform(SpawnRot, SpawnLoc));
    Proj->Launch(Direction, OwnerCharacter.Get());

    UE_LOG(LogTemp, Log,
        TEXT("WTBRArcvenTrigger: Arc wave fired — damage %.0f speed %.0f"),
        Damage, DataAsset->ArcvenParams.ArcWaveSpeed);
}
