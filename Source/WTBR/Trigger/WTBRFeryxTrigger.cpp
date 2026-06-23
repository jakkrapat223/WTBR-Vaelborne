// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFeryxTrigger.h"

void UWTBRFeryxTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits)
{
    Super::PerformSingleSweep(OutHits);
    OnFeryxSwing(false, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
            OnFeryxHit(Hit, DataAsset->BaseDamage);
    }
}

void UWTBRFeryxTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)
{
    Super::PerformDualSweep(OutHits);
    OnFeryxSwing(true, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
        {
            const float DualDmg = DataAsset->BaseDamage
                * DataAsset->DualWieldStats.DamageMultiplier;
            OnFeryxHit(Hit, DualDmg);
        }
    }
}
