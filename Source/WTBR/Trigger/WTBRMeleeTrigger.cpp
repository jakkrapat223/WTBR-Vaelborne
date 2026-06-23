// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool UWTBRMeleeTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (bIsOnCooldown) return false;

    if (!Super::Activate_Implementation(InputValue, bIsDualWield))
        return false;

    TArray<FHitResult> Hits;
    if (bIsDualWield)
        PerformDualSweep(Hits);
    else
        PerformSingleSweep(Hits);

    FilterHits(Hits, OwnerCharacter.Get());

    const float DmgMult = (bIsDualWield && IsValid(DataAsset))
        ? DataAsset->DualWieldStats.DamageMultiplier : 1.0f;
    ApplyDamageToHits(Hits, DmgMult);

    OnMeleeSwing.Broadcast(bIsDualWield);
    StartCooldown();
    return true;
}

void UWTBRMeleeTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    const FWTBRMeleeHitboxParams& P = GetHitboxParams();
    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Origin  = OwnerCharacter->GetActorLocation();
    const FVector Start   = Origin + Forward * (P.ForwardOffset * 0.33f);
    const FVector End     = Origin + Forward * P.ForwardOffset;

    SweepCapsuleAt(
        (Start + End) * 0.5f,
        OwnerCharacter->GetActorQuat(),
        P.CapsuleRadius, P.CapsuleHalfHeight,
        OutHits);

#if ENABLE_DRAW_DEBUG
    DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f,
        P.CapsuleHalfHeight, P.CapsuleRadius,
        OwnerCharacter->GetActorQuat(),
        OutHits.Num() > 0 ? FColor::Red : FColor::Green,
        false, 0.5f);
#endif
}

void UWTBRMeleeTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid()) return;
    const FWTBRMeleeHitboxParams& P = GetHitboxParams();
    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Right   = OwnerCharacter->GetActorRightVector();
    const FVector Center  = OwnerCharacter->GetActorLocation()
                          + Forward * (P.ForwardOffset * 0.66f);
    const FQuat Rot = OwnerCharacter->GetActorQuat();

    TArray<FHitResult> RightHits, LeftHits;
    SweepCapsuleAt(Center + Right * P.DualWieldLateralOffset,
        Rot, P.CapsuleRadius, P.CapsuleHalfHeight, RightHits);
    SweepCapsuleAt(Center - Right * P.DualWieldLateralOffset,
        Rot, P.CapsuleRadius, P.CapsuleHalfHeight, LeftHits);

    TSet<AActor*> Seen;
    auto Merge = [&](const TArray<FHitResult>& Src)
    {
        for (const FHitResult& H : Src)
        {
            if (IsValid(H.GetActor()) && !Seen.Contains(H.GetActor()))
            {
                Seen.Add(H.GetActor());
                OutHits.Add(H);
            }
        }
    };
    Merge(RightHits);
    Merge(LeftHits);

#if ENABLE_DRAW_DEBUG
    DrawDebugCapsule(GetWorld(), Center + Right * P.DualWieldLateralOffset,
        P.CapsuleHalfHeight, P.CapsuleRadius, Rot, FColor::Orange, false, 0.5f);
    DrawDebugCapsule(GetWorld(), Center - Right * P.DualWieldLateralOffset,
        P.CapsuleHalfHeight, P.CapsuleRadius, Rot, FColor::Orange, false, 0.5f);
#endif
}

void UWTBRMeleeTrigger::SweepCapsuleAt(
    const FVector& Center, const FQuat& Rotation,
    float Radius, float HalfHeight,
    TArray<FHitResult>& OutHits)
{
    if (!GetWorld()) return;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());
    Params.bTraceComplex = false;
    GetWorld()->SweepMultiByChannel(
        OutHits, Center, Center, Rotation,
        ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius, HalfHeight),
        Params);
}

void UWTBRMeleeTrigger::ApplyDamageToHits(
    const TArray<FHitResult>& Hits, float DamageMultiplier)
{
    if (!IsValid(DataAsset)) return;
    const float FinalDamage = DataAsset->BaseDamage * DamageMultiplier;
    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar)) continue;
        UWTBRHealthComponent* Health = HitChar->HealthComponent;
        if (!IsValid(Health)) continue;
        Health->ApplyDamage(FinalDamage, OwnerCharacter.Get());
    }
}

void UWTBRMeleeTrigger::FilterHits(
    TArray<FHitResult>& InOutHits, const AActor* InstigatorActor)
{
    InOutHits.RemoveAll([InstigatorActor](const FHitResult& Hit)
    {
        const AActor* A = Hit.GetActor();
        if (!IsValid(A)) return true;
        if (A == InstigatorActor) return true;
        // TODO Phase 5: Friendly filter via TeamID
        return false;
    });
}

const FWTBRMeleeHitboxParams& UWTBRMeleeTrigger::GetHitboxParams() const
{
    ensure(IsValid(DataAsset));
    return DataAsset->MeleeHitbox;
}

void UWTBRMeleeTrigger::StartCooldown()
{
    if (!GetWorld()) return;
    ensure(IsValid(DataAsset));
    bIsOnCooldown = true;
    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimer,
        this, &UWTBRMeleeTrigger::OnCooldownExpired,
        DataAsset->MeleeHitbox.SwingCooldown,
        false);
}

void UWTBRMeleeTrigger::OnCooldownExpired()
{
    bIsOnCooldown = false;
}
