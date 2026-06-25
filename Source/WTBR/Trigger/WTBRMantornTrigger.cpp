// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRMantornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

bool UWTBRMantornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    // Mantorn ignores bIsDualWield — always spins (GDD §3.4 Lock)
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown()) return false;
    if (!IsValid(DataAsset)) return false;

    TArray<FHitResult> Hits;
    PerformSpinSweep(Hits);
    FilterHits(Hits, OwnerCharacter.Get());

    const float SpinDamage = DataAsset->MeleeHitbox.MantornSpinDamage;
    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar)) continue;
        if (!IsValid(HitChar->HealthComponent)) continue;
        HitChar->HealthComponent->ApplyDamage(SpinDamage, OwnerCharacter.Get());
        OnMantornSpinHit(Hit, SpinDamage);
    }

    // OnMeleeSwing broadcasts to all listeners including client delegates.
    // Blueprint on client should bind OnMeleeSwing to play local VFX.
    OnMeleeSwing.Broadcast(false);

    // OnMantornSpinStart is also called here (server).
    // For client VFX, bind to OnMeleeSwing in Blueprint instead.
    OnMantornSpinStart();

    StartCooldown();
    return true;
}

void UWTBRMantornTrigger::PerformSpinSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid() || !GetWorld()) return;
    if (!IsValid(DataAsset)) return;

    const float Radius = DataAsset->MeleeHitbox.MantornSpinRadius;
    const FVector Center = OwnerCharacter->GetActorLocation();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());
    Params.bTraceComplex = false;

    GetWorld()->SweepMultiByChannel(
        OutHits, Center, Center, FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(Radius),
        Params);

    UE_LOG(LogTemp, Log,
        TEXT("WTBR Mantorn SpinSweep radius %.0f: %d hit(s)"),
        Radius, OutHits.Num());

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Center, Radius, 16,
        OutHits.Num() > 0 ? FColor::Red : FColor::Green,
        false, 0.5f);
#endif
}

// Mantorn does not use directional sweeps — redirect to SpinSweep
void UWTBRMantornTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits)
{
    PerformSpinSweep(OutHits);
}

void UWTBRMantornTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)
{
    PerformSpinSweep(OutHits);
}
