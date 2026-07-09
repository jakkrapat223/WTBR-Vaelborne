// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRLacernTrigger.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool UWTBRLacernTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Activate | Owner=%s | Auth=%s | Dual=%s | Cooldown=%s | Extending=%s | DataAsset=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsDualWield ? TEXT("true") : TEXT("false"),
        IsOnCooldown() ? TEXT("true") : TEXT("false"),
        bIsExtending ? TEXT("true") : TEXT("false"),
        *GetNameSafe(DataAsset));

    if (!OwnerCharacter.IsValid())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Rejected | Reason=OwnerInvalid"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Rejected | Reason=NoAuthority | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (IsOnCooldown())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Rejected | Reason=Cooldown | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (bIsExtending)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Rejected | Reason=AlreadyExtending | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Rejected | Reason=DataAssetInvalid | Owner=%s"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    StartExtend(bIsDualWield);
    return true;
}

void UWTBRLacernTrigger::Deactivate_Implementation()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] Cleanup | Owner=%s | Extending=%s | CurrentExtendDist=%.1f | HitsThisSwing=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        bIsExtending ? TEXT("true") : TEXT("false"),
        CurrentExtendDist,
        HitActorsThisSwing.Num());

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ExtendTimer);
        GetWorld()->GetTimerManager().ClearTimer(RetractTimer);
    }
    CurrentExtendDist = 0.0f;
    bIsExtending      = false;
    HitActorsThisSwing.Empty();
    PreviousBladePos  = FVector::ZeroVector;

    // Deactivate can short-circuit an in-progress swing (e.g. weapon-switch
    // via UWTBRTriggerSetComponent::SwapTrigger) without ever reaching
    // OnRetractComplete(). Without this, bLacernExtendTelegraphActive would
    // stay stuck true on every client, leaving NS_Lacern_Extend stuck/orphaned.
    if (OwnerCharacter.IsValid())
    {
        OwnerCharacter->SetLacernExtendTelegraphActive(false, false);
    }

    Super::Deactivate_Implementation();
}

// PerformSingleSweep / PerformDualSweep are unused for Lacern —
// extension is driven by TickExtend timers, not a single-frame sweep.
void UWTBRLacernTrigger::PerformSingleSweep(TArray<FHitResult>& OutHits) {}
void UWTBRLacernTrigger::PerformDualSweep(TArray<FHitResult>& OutHits)   {}

// ─── Extend ──────────────────────────────────────────────────────────────────

void UWTBRLacernTrigger::StartExtend(bool bDualWield)
{
    // Reset anti-tunneling tracker so first tick uses a static sweep (Fix 3B)
    PreviousBladePos = FVector::ZeroVector;

    CurrentExtendDist = 0.0f;
    bIsExtending      = true;
    bIsDualWieldSwing = bDualWield;
    HitActorsThisSwing.Empty();

    OnLacernExtendStart(bDualWield);
    if (OwnerCharacter.IsValid())
    {
        OwnerCharacter->SetLacernExtendTelegraphActive(true, bDualWield);
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] ExtendStart | Owner=%s | Dual=%s | ExtendLength=%.1f | ExtendSpeed=%.1f"),
        *GetNameSafe(OwnerCharacter.Get()),
        bDualWield ? TEXT("true") : TEXT("false"),
        IsValid(DataAsset) ? DataAsset->LacernParams.ExtendLength : 0.0f,
        IsValid(DataAsset) ? DataAsset->LacernParams.ExtendSpeed : 0.0f);

    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().SetTimer(
        ExtendTimer,
        this, &UWTBRLacernTrigger::TickExtend,
        EXTEND_TICK,
        true);
}

void UWTBRLacernTrigger::TickExtend()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset))
    {
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ExtendTimer);
        return;
    }

    const FWTBRLacernParams& LP = DataAsset->LacernParams;
    CurrentExtendDist += LP.ExtendSpeed * EXTEND_TICK;
    OnLacernExtending(CurrentExtendDist, LP.ExtendLength);

    TArray<FHitResult> NewHits;
    SweepAtCurrentPosition(0.0f, NewHits);

    if (bIsDualWieldSwing)
    {
        const FWTBRMeleeHitboxParams& P = GetHitboxParams();
        TArray<FHitResult> RightHits;
        SweepAtCurrentPosition(P.DualWieldLateralOffset, RightHits);
        NewHits.Append(RightHits);
    }

    if (NewHits.Num() > 0)
    {
        const float DmgMult = bIsDualWieldSwing
            ? DataAsset->DualWieldStats.DamageMultiplier : 1.0f;
        ApplyDamageToHits(NewHits, DmgMult);
    }

    // อัปเดต PreviousBladePos สำหรับ tick ถัดไป
    // ใช้ตำแหน่งตรงหน้า (ไม่รวม RightOffset เพราะเก็บ Forward-only)
    if (OwnerCharacter.IsValid())
    {
        PreviousBladePos = OwnerCharacter->GetActorLocation()
            + OwnerCharacter->GetActorForwardVector() * CurrentExtendDist;
    }

    if (CurrentExtendDist >= LP.ExtendLength)
    {
        CurrentExtendDist = LP.ExtendLength;
        OnLacernFullExtend();
        StartRetract();
    }
}

// ─── Retract ─────────────────────────────────────────────────────────────────

void UWTBRLacernTrigger::StartRetract()
{
    if (!GetWorld()) return;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] RetractStart | Owner=%s | CurrentExtendDist=%.1f | HitsThisSwing=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        CurrentExtendDist,
        HitActorsThisSwing.Num());
    GetWorld()->GetTimerManager().ClearTimer(ExtendTimer);
    GetWorld()->GetTimerManager().SetTimer(
        RetractTimer,
        this, &UWTBRLacernTrigger::TickRetract,
        RETRACT_TICK,
        true);
}

void UWTBRLacernTrigger::TickRetract()
{
    if (!IsValid(DataAsset))
    {
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RetractTimer);
        return;
    }

    const FWTBRLacernParams& LP = DataAsset->LacernParams;
    CurrentExtendDist -= LP.RetractSpeed * RETRACT_TICK;

    if (CurrentExtendDist <= 0.0f)
    {
        CurrentExtendDist = 0.0f;
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RetractTimer);
        OnRetractComplete();
    }
}

void UWTBRLacernTrigger::OnRetractComplete()
{
    bIsExtending = false;
    HitActorsThisSwing.Empty();
    PreviousBladePos = FVector::ZeroVector;

    // เรียก StartCooldown() ตรงๆ (ได้หลังย้าย protected ใน MeleeTrigger) — Fix 3C
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] End | Owner=%s | CooldownStart=true"),
        *GetNameSafe(OwnerCharacter.Get()));
    StartCooldown();
    OnLacernRetractComplete();
    if (OwnerCharacter.IsValid())
    {
        OwnerCharacter->SetLacernExtendTelegraphActive(false, false);
    }
}

// ─── Sweep ───────────────────────────────────────────────────────────────────

void UWTBRLacernTrigger::SweepAtCurrentPosition(
    float RightOffset,
    TArray<FHitResult>& OutNewHits)
{
    if (!OwnerCharacter.IsValid()) return;

    const FVector Origin  = OwnerCharacter->GetActorLocation();
    const FVector Forward = OwnerCharacter->GetActorForwardVector();
    const FVector Right   = OwnerCharacter->GetActorRightVector();

    // ตำแหน่ง blade tip ปัจจุบัน
    const FVector CurrentBladePos = Origin
        + Forward * CurrentExtendDist
        + Right   * RightOffset;

    const FWTBRMeleeHitboxParams& P = GetHitboxParams();

    // Anti-tunneling: sweep จาก PreviousBladePos → CurrentBladePos
    // ครอบคลุมทุกจุดที่ blade เคลื่อนผ่านระหว่าง tick นี้
    // ถ้าเป็น tick แรก (PreviousBladePos == Zero) ใช้ CurrentBladePos
    // เป็นทั้ง Start และ End (static sweep ปกติ)
    const FVector SweepStart = PreviousBladePos.IsNearlyZero()
        ? CurrentBladePos
        : PreviousBladePos + Right * RightOffset;

    TArray<FHitResult> Hits;
    SweepCapsuleFromTo(
        SweepStart,
        CurrentBladePos,
        OwnerCharacter->GetActorQuat(),
        P.CapsuleRadius,
        P.CapsuleHalfHeight,
        Hits
    );

    // กรองเฉพาะ Actor ที่ยังไม่โดนใน swing นี้
    for (const FHitResult& Hit : Hits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!IsValid(HitActor)) continue;
        if (HitActorsThisSwing.Contains(HitActor))
        {
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] HitRejected | Reason=AlreadyHit | Owner=%s | Target=%s"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(HitActor));
            continue;
        }
        HitActorsThisSwing.Add(HitActor);

        // VFX must reflect an actual damaged target, not just anything the sweep
        // capsule blocks against (walls/static geo also respond to ECC_Pawn traces).
        // Mirrors the same AWTBRCharacter gate ApplyDamageToHits() uses on OutNewHits.
        if (AWTBRCharacter* HitCharacter = Cast<AWTBRCharacter>(HitActor))
        {
            // Raw Hit.ImpactPoint/ImpactNormal from the capsule sweep is not a reliable
            // cosmetic spawn point (sweep contact can land inside geometry or at glancing
            // angles) — use a stable point offset from the target's torso instead.
            const FVector ToTarget = (HitCharacter->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
            const FVector CosmeticImpactPoint =
                HitCharacter->GetActorLocation()
                - ToTarget * 35.0f
                + FVector(0.0f, 0.0f, 80.0f);
            const FVector CosmeticImpactNormal = -ToTarget;

            OnLacernHit(CosmeticImpactPoint, CosmeticImpactNormal, bIsDualWieldSwing);
            OwnerCharacter->Multicast_LacernHit(CosmeticImpactPoint, CosmeticImpactNormal, bIsDualWieldSwing);
        }
        OutNewHits.Add(Hit);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Lacern Test] HitAttempt | Owner=%s | Target=%s | CurrentExtendDist=%.1f | RightOffset=%.1f"),
            *GetNameSafe(OwnerCharacter.Get()),
            *GetNameSafe(HitActor),
            CurrentExtendDist,
            RightOffset);
    }
}
