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

    const FWTBRFulgrixParams& Params = DataAsset->FulgrixParams;

    UWorld* World = OwnerCharacter->GetWorld();

    // Conjure, split, fire — and every cube detonates (canon: Meteor's cubes each
    // explode). The blast radius drops and the impacts spread on purpose; both are
    // explained on FulgrixTapExplosionRadius / FulgrixTapImpactSpread. Together they
    // turn one big boom into a cluster instead of eight boxes hitting one spot.
    FireProjectileVolley(
        Params.FulgrixProjectileClass,
        Params.FulgrixTapCubeCount,
        Params.FulgrixTapTotalDamage,
        Params.FulgrixSpeed,
        Params.FulgrixTapScatterRadius,
        /*ConvergeDistance=*/Params.FulgrixRange,
        Params.FulgrixTapImpactSpread,
        /*bExplode=*/true,
        Params.FulgrixTapExplosionRadius);

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
