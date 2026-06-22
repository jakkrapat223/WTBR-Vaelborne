// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRStaminaComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Net/UnrealNetwork.h"

static const float REGEN_TICK_INTERVAL = 0.05f;

UWTBRStaminaComponent::UWTBRStaminaComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWTBRStaminaComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentStamina = GetMaxStamina();
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool UWTBRStaminaComponent::TryConsumeStamina(float Amount)
{
    if (bExhausted || CurrentStamina < Amount) return false;

    CurrentStamina -= Amount;

    if (CurrentStamina <= 0.f)
    {
        CurrentStamina = 0.f;
        bExhausted = true;
    }
    OnStaminaChanged.Broadcast(CurrentStamina, bExhausted);

    // Authority only: reset regen pipeline — delay fires once, then tick regen loops
    if (GetOwnerRole() == ROLE_Authority && GetWorld())
    {
        FTimerManager& TM = GetWorld()->GetTimerManager();
        TM.ClearTimer(RegenDelayTimer);
        TM.ClearTimer(RegenTickTimer);

        const float Delay = StatsData ? StatsData->StaminaRegenDelay : 1.2f;
        TM.SetTimer(RegenDelayTimer, this, &UWTBRStaminaComponent::StartRegenTick, Delay, false);
    }
    return true;
}

bool UWTBRStaminaComponent::TryConsumeDodgeStamina()
{
    return TryConsumeStamina(GetDodgeCost());
}

float UWTBRStaminaComponent::GetMaxStamina() const
{
    return StatsData ? StatsData->MaxStamina : 100.f;
}

float UWTBRStaminaComponent::GetDodgeCost() const
{
    return StatsData ? StatsData->StaminaDodgeCost : 33.f;
}

float UWTBRStaminaComponent::GetSpeedPenalty() const
{
    if (!bExhausted) return 0.f;
    return StatsData ? StatsData->StaminaExhaustedSpeedPenalty : 0.10f;
}

// ─── Timer Regen ─────────────────────────────────────────────────────────────

void UWTBRStaminaComponent::StartRegenTick()
{
    if (!GetWorld()) return;
    GetWorld()->GetTimerManager().SetTimer(
        RegenTickTimer, this, &UWTBRStaminaComponent::TickRegen, REGEN_TICK_INTERVAL, true);
}

void UWTBRStaminaComponent::TickRegen()
{
    const float Max        = GetMaxStamina();
    const float RegenRate  = StatsData ? StatsData->StaminaRegenRate : 35.f;
    const float RegenDelta = RegenRate * REGEN_TICK_INTERVAL;

    CurrentStamina = FMath::Min(Max, CurrentStamina + RegenDelta);

    if (bExhausted && CurrentStamina >= Max)
    {
        bExhausted = false;
    }
    OnStaminaChanged.Broadcast(CurrentStamina, bExhausted);

    if (CurrentStamina >= Max)
    {
        GetWorld()->GetTimerManager().ClearTimer(RegenTickTimer);
    }
}

// ─── Replication ─────────────────────────────────────────────────────────────

void UWTBRStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRStaminaComponent, CurrentStamina);
    DOREPLIFETIME(UWTBRStaminaComponent, bExhausted);
}
