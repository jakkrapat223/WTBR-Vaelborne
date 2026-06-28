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
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Test] Sweep | Owner=%s | Auth=false | Dual=false | RawHits=0 | Result=Rejected"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(false, false);
        return;
    }

    Super::PerformSingleSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=false | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Test] Sweep | Owner=%s | Auth=true | Dual=false | RawHits=%d | Result=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num(),
        OutHits.Num() > 0 ? TEXT("Hit") : TEXT("Miss"));
    OnFeryxSwing(false, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Feryx Test] HitAttempt | Owner=%s | Target=%s | Damage=%.1f | Dual=false"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(Hit.GetActor()),
                DataAsset->BaseDamage);
            OnFeryxHit(Hit, DataAsset->BaseDamage);
        }
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
        UE_LOG(LogTemp, Warning,
            TEXT("[Feryx Test] Sweep | Owner=%s | Auth=false | Dual=true | RawHits=0 | Result=Rejected"),
            *GetNameSafe(OwnerCharacter.Get()));
        OnFeryxSwing(true, false);
        return;
    }

    Super::PerformDualSweep(OutHits);
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Sweep] Owner=%s | Auth=true | Dual=true | RawHits=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num());
    UE_LOG(LogTemp, Warning,
        TEXT("[Feryx Test] Sweep | Owner=%s | Auth=true | Dual=true | RawHits=%d | Result=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        OutHits.Num(),
        OutHits.Num() > 0 ? TEXT("Hit") : TEXT("Miss"));
    OnFeryxSwing(true, OutHits.Num() > 0);
    for (const FHitResult& Hit : OutHits)
    {
        if (IsValid(DataAsset))
        {
            const float DualDmg = DataAsset->BaseDamage
                * DataAsset->DualWieldStats.DamageMultiplier;
            UE_LOG(LogTemp, Warning,
                TEXT("[Feryx Test] HitAttempt | Owner=%s | Target=%s | Damage=%.1f | Dual=true"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(Hit.GetActor()),
                DualDmg);
            OnFeryxHit(Hit, DualDmg);
        }
    }
}
