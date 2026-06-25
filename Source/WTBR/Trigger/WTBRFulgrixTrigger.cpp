// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgrixTrigger.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRFulgrixTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;
    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->FulgrixParams.FulgrixProjectileClass;
    const float Damage = DataAsset->FulgrixParams.FulgrixDamage;
    const float Speed = DataAsset->FulgrixParams.FulgrixSpeed;
    const float ExplodeRadius = DataAsset->FulgrixParams.FulgrixExplosionRadius;

    UWorld* World = OwnerCharacter->GetWorld();

    FireProjectile(ProjClass, Damage, Speed, 0.0f, true, ExplodeRadius);

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnFulgrixFired();
    StartCooldown();
    return true;
}

float UWTBRFulgrixTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->FulgrixParams.FulgrixFireCooldown;
}
