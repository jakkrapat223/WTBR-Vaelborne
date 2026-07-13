// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRKaldrixTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRKaldrixZone.h"
#include "Kismet/GameplayStatics.h"

bool UWTBRKaldrixTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp)) return false;
    if (!VaelComp->TryConsumeVael(DataAsset->KaldrixParams.KaldrixVaelCost)) return false;

    const TSubclassOf<AWTBRKaldrixZone> ZoneClass    = DataAsset->KaldrixParams.KaldrixZoneClass;
    const float Damage          = DataAsset->KaldrixParams.KaldrixDamage;
    const float Radius          = DataAsset->KaldrixParams.KaldrixRadius;
    const float ArmTime         = DataAsset->KaldrixParams.KaldrixArmTime;
    const float StaggerDuration = DataAsset->KaldrixParams.KaldrixStaggerDuration;

    if (!IsValid(ZoneClass)) return false;

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation();
    const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLoc);

    AWTBRKaldrixZone* Zone = GetWorld()->SpawnActorDeferred<AWTBRKaldrixZone>(
        ZoneClass, SpawnTransform, OwnerCharacter.Get(), OwnerCharacter.Get(),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Zone)) return false;

    Zone->InitializeZone(Damage, Radius, ArmTime, StaggerDuration, OwnerCharacter.Get());

    UGameplayStatics::FinishSpawningActor(Zone, SpawnTransform);

    // Kaldrix doesn't call FireBlackProjectile — manually fire the VFX event
    OnBlackTriggerFired(OwnerCharacter->GetActorForwardVector());

    StartCooldown();
    return true;
}

float UWTBRKaldrixTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 2.0f;
    return DataAsset->KaldrixParams.KaldrixFireCooldown;
}
