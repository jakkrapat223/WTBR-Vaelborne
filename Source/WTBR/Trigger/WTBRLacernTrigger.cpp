// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRLacernTrigger.h"
#include "WTBRCharacter.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool UWTBRLacernTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown()) return false;
    if (bIsExtending) return false;
    if (!IsValid(DataAsset)) return false;

    StartExtend(bIsDualWield);
    return true;
}

void UWTBRLacernTrigger::Deactivate_Implementation()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ExtendTimer);
        GetWorld()->GetTimerManager().ClearTimer(RetractTimer);
    }
    CurrentExtendDist = 0.0f;
    bIsExtending      = false;
    HitActorsThisSwing.Empty();
    PreviousBladePos  = FVector::ZeroVector;
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
        StartRetract();
    }
}

// ─── Retract ─────────────────────────────────────────────────────────────────

void UWTBRLacernTrigger::StartRetract()
{
    if (!GetWorld()) return;
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
    StartCooldown();
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
        if (HitActorsThisSwing.Contains(HitActor)) continue;
        HitActorsThisSwing.Add(HitActor);
        OutNewHits.Add(Hit);
    }
}
