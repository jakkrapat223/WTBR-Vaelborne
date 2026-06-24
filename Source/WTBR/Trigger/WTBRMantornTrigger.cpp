// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRMantornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

bool UWTBRMantornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool /*bIsDualWield*/)  // Mantorn is single-wield only — param intentionally ignored
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (bIsOnCooldown) return false;
    if (!IsValid(DataAsset)) return false;

    const float Radius  = DataAsset->MeleeHitbox.MantornSpinRadius;
    const FVector Origin = OwnerCharacter->GetActorLocation();

    // SweepSphere centred on owner — full 360° spin, no forward bias
    TArray<FHitResult> Hits;
    {
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(OwnerCharacter.Get());
        Params.bTraceComplex = false;
        GetWorld()->SweepMultiByChannel(
            Hits, Origin, Origin, FQuat::Identity,
            ECC_Pawn,
            FCollisionShape::MakeSphere(Radius),
            Params);
    }

    FilterHits(Hits, OwnerCharacter.Get());

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Origin, Radius, 12,
        Hits.Num() > 0 ? FColor::Blue : FColor::Cyan,
        false, 0.5f);
#endif

    // Use Mantorn-specific spin damage — not BaseDamage
    const float SpinDamage = DataAsset->MeleeHitbox.MantornSpinDamage;
    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar)) continue;
        UWTBRHealthComponent* Health = HitChar->HealthComponent;
        if (!IsValid(Health)) continue;
        Health->ApplyDamage(SpinDamage, OwnerCharacter.Get());
    }

    OnMeleeSwing.Broadcast(false);
    OnMantornSpin();
    StartCooldown();
    return true;
}
