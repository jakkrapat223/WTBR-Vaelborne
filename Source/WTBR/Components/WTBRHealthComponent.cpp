// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRHealthComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Net/UnrealNetwork.h"

// Drain fires every 0.5 s on Authority; HP removed = drainRate * MaxHP * interval per limb
static const float LIMB_DRAIN_TICK_INTERVAL = 0.5f;

UWTBRHealthComponent::UWTBRHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    // 5 slots: index = EWTBRLimbType value (0=None, 1=LeftArm … 4=RightLeg)
    LimbStates.Init(FWTBRLimbState(), 5);
}

void UWTBRHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentHP = GetMaxHP();
    if (LimbStates.Num() != 5) LimbStates.Init(FWTBRLimbState(), 5);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void UWTBRHealthComponent::ApplyDamage(float DamageAmount)
{
    if (!IsAlive()) return;
    CurrentHP = FMath::Max(0.f, CurrentHP - DamageAmount);
    OnHPChanged.Broadcast(CurrentHP, GetMaxHP());
    if (CurrentHP <= 0.f) OnDeath.Broadcast();
}

void UWTBRHealthComponent::DestroyLimb(EWTBRLimbType Limb)
{
    const int32 Idx = (int32)Limb;
    if (Limb == EWTBRLimbType::None || !LimbStates.IsValidIndex(Idx)) return;
    if (LimbStates[Idx].bDestroyed) return;

    FWTBRLimbState& State = LimbStates[Idx];
    State.bDestroyed = true;

    if (Limb == EWTBRLimbType::LeftLeg || Limb == EWTBRLimbType::RightLeg)
    {
        State.SpeedPenalty  = StatsData ? StatsData->LimbLegSpeedPenalty  : 0.40f;
        State.DamagePenalty = 0.f;
    }
    else
    {
        State.DamagePenalty = StatsData ? StatsData->LimbArmDamagePenalty : 0.35f;
        State.SpeedPenalty  = StatsData ? StatsData->LimbArmSpeedPenalty  : 0.35f;
    }

    const float RateMid = StatsData
        ? (StatsData->LimbHPDrainRateMin + StatsData->LimbHPDrainRateMax) * 0.5f
        : 0.0075f;
    State.HPDrainRateMultiplier = RateMid;

    OnLimbDestroyed.Broadcast(Limb, State);

    if (GetOwnerRole() == ROLE_Authority)
    {
        RefreshLimbDrainTimer();
    }
}

void UWTBRHealthComponent::RestoreLimb(EWTBRLimbType Limb)
{
    const int32 Idx = (int32)Limb;
    if (LimbStates.IsValidIndex(Idx))
    {
        LimbStates[Idx] = FWTBRLimbState();
    }

    // Stop drain timer if no limbs remain destroyed
    if (GetOwnerRole() == ROLE_Authority && GetWorld())
    {
        bool bAnyDestroyed = false;
        for (int32 i = 1; i < LimbStates.Num(); ++i)
        {
            if (LimbStates[i].bDestroyed) { bAnyDestroyed = true; break; }
        }
        if (!bAnyDestroyed)
        {
            GetWorld()->GetTimerManager().ClearTimer(LimbDrainTimerHandle);
        }
    }
}

float UWTBRHealthComponent::GetMaxHP() const
{
    return StatsData ? StatsData->MaxHP : 300.f;
}

FWTBRLimbState UWTBRHealthComponent::GetLimbState(EWTBRLimbType Limb) const
{
    const int32 Idx = (int32)Limb;
    return LimbStates.IsValidIndex(Idx) ? LimbStates[Idx] : FWTBRLimbState();
}

float UWTBRHealthComponent::GetTotalSpeedPenaltyFromLimbs() const
{
    // Multiplicative: final = 1 - product of (1 - each penalty)
    float Composite = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed && LimbStates[i].SpeedPenalty > 0.f)
        {
            Composite = 1.f - (1.f - Composite) * (1.f - LimbStates[i].SpeedPenalty);
        }
    }
    return Composite;
}

float UWTBRHealthComponent::GetTotalDamagePenaltyFromLimbs() const
{
    float Composite = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed && LimbStates[i].DamagePenalty > 0.f)
        {
            Composite = 1.f - (1.f - Composite) * (1.f - LimbStates[i].DamagePenalty);
        }
    }
    return Composite;
}

// ─── Timer Drain ─────────────────────────────────────────────────────────────

void UWTBRHealthComponent::RefreshLimbDrainTimer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (!World->GetTimerManager().IsTimerActive(LimbDrainTimerHandle))
    {
        World->GetTimerManager().SetTimer(
            LimbDrainTimerHandle,
            this, &UWTBRHealthComponent::TickLimbDrain,
            LIMB_DRAIN_TICK_INTERVAL,
            true  // looping
        );
    }
}

void UWTBRHealthComponent::TickLimbDrain()
{
    const float MaxHP   = GetMaxHP();
    const float RateMid = StatsData
        ? (StatsData->LimbHPDrainRateMin + StatsData->LimbHPDrainRateMax) * 0.5f
        : 0.0075f;

    float TotalDrain = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed)
        {
            TotalDrain += RateMid * MaxHP;
        }
    }
    if (TotalDrain > 0.f)
    {
        ApplyDamage(TotalDrain * LIMB_DRAIN_TICK_INTERVAL);
    }
}

// ─── Replication ─────────────────────────────────────────────────────────────

void UWTBRHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRHealthComponent, CurrentHP);
    DOREPLIFETIME(UWTBRHealthComponent, LimbStates);
}
