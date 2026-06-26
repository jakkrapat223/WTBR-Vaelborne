// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFeryxTrigger.h"
#include "WTBRCharacter.h"

void UWTBRFeryxTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority())
    {
        OutHits.Reset();
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Sweep] Owner=%s | Auth=false | Dual=false | RawHits=0"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(false, false);
        return;
    }

    Super::PerformSingleSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=false | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
    OnFeryxSwing(false, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
            OnFeryxHit(Hit, DataAsset->BaseDamage);
    }
}

void UWTBRFeryxTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority())
    {
        OutHits.Reset();
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Sweep] Owner=%s | Auth=false | Dual=true | RawHits=0"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(true, false);
        return;
    }

    Super::PerformDualSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=true | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
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
