// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRStaminaComponent.h"
#include "WTBRValidationLog.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Net/UnrealNetwork.h"

static const float REGEN_TICK_INTERVAL = 0.05f;

namespace
{
void LogTest32StaminaCoreStats(const UWTBRStaminaComponent* Component, const UWTBRCoreStatsDataAsset* Stats, const TCHAR* Requester)
{
    if (Stats)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Loaded] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsAsset=%s | CoreStatsPath=%s | StatsValid=true | MaxStamina=%.1f | DodgeCost=%.1f | RegenRate=%.1f | RegenDelay=%.2f | ExhaustedSpeedPenalty=%.2f"),
            *GetNameSafe(Component),
            *GetNameSafe(Component ? Component->GetOwner() : nullptr),
            Component ? (int32)Component->GetNetMode() : -1,
            Component ? (int32)Component->GetOwnerRole() : -1,
            Requester ? Requester : TEXT("Unknown"),
            *GetNameSafe(Stats),
            Component ? *Component->CoreStatsAsset.ToSoftObjectPath().ToString() : TEXT("None"),
            Stats->MaxStamina,
            Stats->StaminaDodgeCost,
            Stats->StaminaRegenRate,
            Stats->StaminaRegenDelay,
            Stats->StaminaExhaustedSpeedPenalty);
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Missing] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsPath=%s | StatsValid=false"),
        *GetNameSafe(Component),
        *GetNameSafe(Component ? Component->GetOwner() : nullptr),
        Component ? (int32)Component->GetNetMode() : -1,
        Component ? (int32)Component->GetOwnerRole() : -1,
        Requester ? Requester : TEXT("Unknown"),
        Component ? *Component->CoreStatsAsset.ToSoftObjectPath().ToString() : TEXT("None"));
}
}

UWTBRStaminaComponent::UWTBRStaminaComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWTBRStaminaComponent::BeginPlay()
{
    Super::BeginPlay();
    LogTest32StaminaCoreStats(this, GetStats(), TEXT("BeginPlay"));
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

        const auto* S = GetStats();
        LogTest32StaminaCoreStats(this, S, TEXT("TryConsumeStamina"));
        const float Delay = S ? S->StaminaRegenDelay : 1.2f;
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
    const auto* S = GetStats();
    LogTest32StaminaCoreStats(this, S, TEXT("GetMaxStamina"));
    return S ? S->MaxStamina : 100.f;
}

float UWTBRStaminaComponent::GetDodgeCost() const
{
    const auto* S = GetStats();
    LogTest32StaminaCoreStats(this, S, TEXT("GetDodgeCost"));
    return S ? S->StaminaDodgeCost : 33.f;
}

float UWTBRStaminaComponent::GetSpeedPenalty() const
{
    if (!bExhausted) return 0.f;
    const auto* S = GetStats();
    LogTest32StaminaCoreStats(this, S, TEXT("GetSpeedPenalty"));
    return S ? S->StaminaExhaustedSpeedPenalty : 0.10f;
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
    const auto* S          = GetStats();
    LogTest32StaminaCoreStats(this, S, TEXT("TickRegen"));
    const float RegenRate  = S ? S->StaminaRegenRate : 35.f;
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
