// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVentryxTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRVentryxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRVentryxParams& Params = DataAsset->VentryxParams;
    if (!Params.VentryxProjectileClass) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(Params.VentryxVaelCost)) return false;

    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Origin  = OwnerCharacter->GetActorLocation() + Forward * 100.0f;
    const FTransform SpawnTF(Forward.Rotation(), Origin);

    UWorld* World = OwnerCharacter->GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.VentryxProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return false;

    Proj->MaxRange       = Params.VentryxRange;
    Proj->KnockbackForce = Params.VentryxKnockback;
    Proj->InitializeProjectile(Params.VentryxDamage, Params.VentryxSpeed,
        ETriggerCategory::BlackTrigger, false, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(Forward, OwnerCharacter.Get());

    if (UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>())
        PingSys->RegisterActionPing(OwnerCharacter.Get());

    OnVentryxActivated();
    return true;
}
