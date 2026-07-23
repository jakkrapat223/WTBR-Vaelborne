// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRVaelComponent.h"
#include "WTBRGameState.h"
#include "WTBRValidationLog.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
constexpr float PASSIVE_REGEN_TICK_INTERVAL = 1.0f;

void LogTest32VaelCoreStats(const UWTBRVaelComponent* Component, const UWTBRCoreStatsDataAsset* Stats, const TCHAR* Requester)
{
    if (Stats)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Loaded] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsAsset=%s | CoreStatsPath=%s | StatsValid=true | MaxVael=%.1f | LowVaelThreshold=%.1f | DesperationCostMultiplier=%.2f"),
            *GetNameSafe(Component),
            *GetNameSafe(Component ? Component->GetOwner() : nullptr),
            Component ? (int32)Component->GetNetMode() : -1,
            Component ? (int32)Component->GetOwnerRole() : -1,
            Requester ? Requester : TEXT("Unknown"),
            *GetNameSafe(Stats),
            Component ? *Component->CoreStatsAsset.ToSoftObjectPath().ToString() : TEXT("None"),
            Stats->MaxVael,
            Stats->LowVaelThreshold,
            Stats->DesperationCostMultiplier);
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

UWTBRVaelComponent::UWTBRVaelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    CurrentVael = 1000.0f;
}

void UWTBRVaelComponent::BeginPlay()
{
    Super::BeginPlay();
    const UWTBRCoreStatsDataAsset* LoadedStats = GetStats();
    LogTest32VaelCoreStats(this, LoadedStats, TEXT("BeginPlay"));
    const float LoadedMaxVael = GetMaxVael();
    UE_LOG(LogTemp, Warning,
        TEXT("[Vael CoreStatsCheck] Owner=%s | Component=%s | CoreStatsAsset=%s | CoreStatsPath=%s | Loaded=%s | MaxVael=%.1f"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(this),
        *GetNameSafe(LoadedStats),
        *CoreStatsAsset.ToSoftObjectPath().ToString(),
        LoadedStats ? TEXT("true") : TEXT("false"),
        LoadedMaxVael);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CurrentVael = LoadedMaxVael;
        StartPassiveRegenTimer();
    }
}

void UWTBRVaelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPassiveRegenTimer();
    Super::EndPlay(EndPlayReason);
}

float UWTBRVaelComponent::GetMaxVael() const
{
    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("GetMaxVael"));
    return S ? FMath::Max(0.0f, S->MaxVael) : 1000.0f;
}

float UWTBRVaelComponent::GetLowVaelThreshold() const
{
    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("GetLowVaelThreshold"));
    return S ? S->LowVaelThreshold : 25.0f;
}

bool UWTBRVaelComponent::DebugIsDesperationActiveTimerActive() const
{
    UWorld* World = GetWorld();
    return World && World->GetTimerManager().IsTimerActive(DesperationActiveTimerHandle);
}

bool UWTBRVaelComponent::DebugIsDesperationCooldownTimerActive() const
{
    UWorld* World = GetWorld();
    return World && World->GetTimerManager().IsTimerActive(DesperationCooldownTimerHandle);
}

bool UWTBRVaelComponent::TryConsumeVael(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Amount <= 0.0f) return false;

    // RawCost is the caller's requested spend; EffectiveCost is the actual Vael deducted.
    const float RawCost = Amount;
    const float CostMultiplier = GetVaelCostMultiplier();
    const float EffectiveCost = FMath::Max(0.0f, RawCost * CostMultiplier);

    if (bOverheated || GetAvailableVael() < EffectiveCost) return false;

    const float OldVael = CurrentVael;
    const float LoadedMaxVael = GetMaxVael();
    CurrentVael = FMath::Clamp(CurrentVael - EffectiveCost, 0.0f, LoadedMaxVael);
    OnVaelChanged.Broadcast(CurrentVael, LoadedMaxVael);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test26 Server TryConsumeVael] Owner=%s | NetMode=%d | Role=%d | RawCost=%.1f | Multiplier=%.2f | EffectiveCost=%.1f | OldVael=%.1f | NewVael=%.1f | MaxVael=%.1f | Overheated=%s | Active=%s | Cooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        RawCost,
        CostMultiplier,
        EffectiveCost,
        OldVael,
        CurrentVael,
        LoadedMaxVael,
        bOverheated ? TEXT("true") : TEXT("false"),
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));

    if (CurrentVael <= 0.f)
    {
        CurrentVael = 0.f;
        bOverheated  = true;
        OnOverheatChanged.Broadcast(true);
    }
    HandleVaelChanged(OldVael, CurrentVael);
    return true;
}

bool UWTBRVaelComponent::GrantVael(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Amount <= 0.0f) return false;

    const float PreviousVael = CurrentVael;
    const float LoadedMaxVael = GetMaxVael();
    CurrentVael = FMath::Clamp(CurrentVael + Amount, 0.0f, LoadedMaxVael);

    const bool bWasOverheated = bOverheated;
    if (CurrentVael > 0.0f)
    {
        bOverheated = false;
    }

    if (!FMath::IsNearlyEqual(CurrentVael, PreviousVael))
    {
        OnVaelChanged.Broadcast(CurrentVael, LoadedMaxVael);
    }

    if (bOverheated != bWasOverheated)
    {
        OnOverheatChanged.Broadcast(bOverheated);
    }

    return CurrentVael > PreviousVael;
}

float UWTBRVaelComponent::GetTotalReservedVael() const
{
    float Total = 0.0f;
    for (const auto& Pair : ActiveVaelReservations)
    {
        Total += Pair.Value;
    }
    return Total;
}

float UWTBRVaelComponent::GetAvailableVael() const
{
    return FMath::Max(0.0f, CurrentVael - GetTotalReservedVael());
}

bool UWTBRVaelComponent::TryReserveVael(float Amount, FGuid& OutReservationHandle)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Amount <= 0.0f) return false;

    const float EffectiveCost = FMath::Max(0.0f, Amount * GetVaelCostMultiplier());
    if (bOverheated || EffectiveCost > GetAvailableVael()) return false;

    OutReservationHandle = FGuid::NewGuid();
    ActiveVaelReservations.Add(OutReservationHandle, EffectiveCost);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Vael Reserve] Owner=%s | NetMode=%d | Role=%d | RawCost=%.1f | EffectiveCost=%.1f | CurrentVael=%.1f | AvailableVael=%.1f | Reservation=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        Amount,
        EffectiveCost,
        CurrentVael,
        GetAvailableVael(),
        *OutReservationHandle.ToString());
    return true;
}

bool UWTBRVaelComponent::CommitReservation(const FGuid& ReservationHandle)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    float EffectiveCost = 0.0f;
    if (!ActiveVaelReservations.RemoveAndCopyValue(ReservationHandle, EffectiveCost)) return false;

    const float OldVael = CurrentVael;
    const float LoadedMaxVael = GetMaxVael();
    CurrentVael = FMath::Clamp(CurrentVael - EffectiveCost, 0.0f, LoadedMaxVael);
    OnVaelChanged.Broadcast(CurrentVael, LoadedMaxVael);

    if (CurrentVael <= 0.f)
    {
        CurrentVael = 0.f;
        bOverheated = true;
        OnOverheatChanged.Broadcast(true);
    }
    HandleVaelChanged(OldVael, CurrentVael);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Vael Commit] Owner=%s | NetMode=%d | Role=%d | EffectiveCost=%.1f | OldVael=%.1f | NewVael=%.1f | AvailableVael=%.1f | Reservation=%s | Overheated=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        EffectiveCost,
        OldVael,
        CurrentVael,
        GetAvailableVael(),
        *ReservationHandle.ToString(),
        bOverheated ? TEXT("true") : TEXT("false"));
    return true;
}

bool UWTBRVaelComponent::ReleaseReservation(const FGuid& ReservationHandle)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    const bool bRemoved = ActiveVaelReservations.Remove(ReservationHandle) > 0;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Vael Release] Owner=%s | NetMode=%d | Role=%d | Reservation=%s | Removed=%s | CurrentVael=%.1f | AvailableVael=%.1f"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        *ReservationHandle.ToString(),
        bRemoved ? TEXT("true") : TEXT("false"),
        CurrentVael,
        GetAvailableVael());
    return bRemoved;
}

void UWTBRVaelComponent::StartPassiveRegenTimer()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    World->GetTimerManager().SetTimer(
        PassiveRegenTimerHandle,
        this,
        &UWTBRVaelComponent::ApplyPassiveRegenTick,
        PASSIVE_REGEN_TICK_INTERVAL,
        true);
}

void UWTBRVaelComponent::StopPassiveRegenTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PassiveRegenTimerHandle);
    }
}

void UWTBRVaelComponent::ApplyPassiveRegenTick()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)) return;
    if (!WTBRGameState->IsPassiveVaelRegenEnabled()) return;

    const float RegenPerSecond = WTBRGameState->GetVaelRegenPerSecond();
    if (RegenPerSecond <= 0.0f) return;

    GrantVael(RegenPerSecond * PASSIVE_REGEN_TICK_INTERVAL);
}

void UWTBRVaelComponent::ResetDesperationState()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DesperationActiveTimerHandle);
        World->GetTimerManager().ClearTimer(DesperationCooldownTimerHandle);
    }

    SetDesperationActive(false);
    SetDesperationOnCooldown(false);
}

void UWTBRVaelComponent::DebugSetCurrentVaelDirect(float NewVael)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    const float LoadedMaxVael = GetMaxVael();
    CurrentVael = FMath::Clamp(NewVael, 0.0f, LoadedMaxVael);
    OnVaelChanged.Broadcast(CurrentVael, LoadedMaxVael);
}

float UWTBRVaelComponent::GetVaelCostMultiplier() const
{
    if (!bIsDesperationActive) return 1.0f;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("GetVaelCostMultiplier"));
    if (!S) return 1.0f;

    return FMath::Clamp(S->DesperationCostMultiplier, 0.0f, 1.0f);
}

void UWTBRVaelComponent::NotifyVaelLeftCharacterBounds()
{
    // This is an action marker, not a radar-detection event. Baseline radar visibility
    // comes from the trion body and is suppressed only by Bagworm/Vexorn cloak.
    OnVaelReleased.Broadcast();
}

void UWTBRVaelComponent::OnRep_CurrentVael()
{
    const float LoadedMaxVael = GetMaxVael();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test26 Client OnRep_CurrentVael] Owner=%s | NetMode=%d | Role=%d | CurrentVael=%.1f | MaxVael=%.1f | Overheated=%s | Active=%s | Cooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        CurrentVael,
        LoadedMaxVael,
        bOverheated ? TEXT("true") : TEXT("false"),
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));

    OnVaelChanged.Broadcast(CurrentVael, LoadedMaxVael);
}

void UWTBRVaelComponent::OnRep_bOverheated()
{
    OnOverheatChanged.Broadcast(bOverheated);
}

void UWTBRVaelComponent::OnRep_IsDesperationActive()
{
    const float LoadedMaxVael = GetMaxVael();
    const float LowVaelThreshold = GetLowVaelThreshold();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test29 Client OnRep_IsDesperationActive] Owner=%s | NetMode=%d | Role=%d | CurrentVael=%.1f | MaxVael=%.1f | LowVaelThreshold=%.1f | Active=%s | Cooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        CurrentVael,
        LoadedMaxVael,
        LowVaelThreshold,
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));

    OnDesperationActiveChanged.Broadcast(bIsDesperationActive);
}

void UWTBRVaelComponent::OnRep_IsDesperationOnCooldown()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test29 Client OnRep_IsDesperationOnCooldown] Owner=%s | NetMode=%d | Role=%d | CurrentVael=%.1f | Active=%s | Cooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        CurrentVael,
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));

    OnDesperationCooldownChanged.Broadcast(bIsDesperationOnCooldown);
}

void UWTBRVaelComponent::HandleVaelChanged(float OldVael, float NewVael)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsDesperationActive || bIsDesperationOnCooldown) return;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("HandleVaelChanged"));
    if (!S) return;

    if (OldVael >= S->LowVaelThreshold && NewVael < S->LowVaelThreshold)
    {
        TriggerDesperationWindow();
    }
}

void UWTBRVaelComponent::TriggerDesperationWindow()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("TriggerDesperationWindow"));
    if (!S) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DesperationActiveTimerHandle);
        World->GetTimerManager().ClearTimer(DesperationCooldownTimerHandle);

        SetDesperationOnCooldown(false);
        SetDesperationActive(true);

        if (S->DesperationDuration > 0.0f)
        {
            World->GetTimerManager().SetTimer(
                DesperationActiveTimerHandle,
                this, &UWTBRVaelComponent::EndDesperationWindow,
                S->DesperationDuration,
                false);
        }
        else
        {
            EndDesperationWindow();
        }
    }
}

void UWTBRVaelComponent::EndDesperationWindow()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    SetDesperationActive(false);
    SetDesperationOnCooldown(true);

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32VaelCoreStats(this, S, TEXT("EndDesperationWindow"));
    UWorld* World = GetWorld();
    if (!S || !World) return;

    if (S->DesperationCooldown > 0.0f)
    {
        World->GetTimerManager().SetTimer(
            DesperationCooldownTimerHandle,
            this, &UWTBRVaelComponent::EndDesperationCooldown,
            S->DesperationCooldown,
            false);
    }
    else
    {
        EndDesperationCooldown();
    }
}

void UWTBRVaelComponent::EndDesperationCooldown()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SetDesperationOnCooldown(false);
}

void UWTBRVaelComponent::SetDesperationActive(bool bNewActive)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsDesperationActive == bNewActive) return;

    const bool bOldActive = bIsDesperationActive;
    bIsDesperationActive = bNewActive;
    OnDesperationActiveChanged.Broadcast(bIsDesperationActive);

    const float LoadedMaxVael = GetMaxVael();
    const float LowVaelThreshold = GetLowVaelThreshold();
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test29 Server DesperationActiveChanged] Owner=%s | NetMode=%d | Role=%d | CurrentVael=%.1f | MaxVael=%.1f | LowVaelThreshold=%.1f | OldActive=%s | NewActive=%s | Cooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        CurrentVael,
        LoadedMaxVael,
        LowVaelThreshold,
        bOldActive ? TEXT("true") : TEXT("false"),
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));
}

void UWTBRVaelComponent::SetDesperationOnCooldown(bool bNewOnCooldown)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsDesperationOnCooldown == bNewOnCooldown) return;

    const bool bOldCooldown = bIsDesperationOnCooldown;
    bIsDesperationOnCooldown = bNewOnCooldown;
    OnDesperationCooldownChanged.Broadcast(bIsDesperationOnCooldown);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test29 Server DesperationCooldownChanged] Owner=%s | NetMode=%d | Role=%d | CurrentVael=%.1f | Active=%s | OldCooldown=%s | NewCooldown=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        (int32)GetOwnerRole(),
        CurrentVael,
        bIsDesperationActive ? TEXT("true") : TEXT("false"),
        bOldCooldown ? TEXT("true") : TEXT("false"),
        bIsDesperationOnCooldown ? TEXT("true") : TEXT("false"));
}

void UWTBRVaelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRVaelComponent, CurrentVael);
    DOREPLIFETIME(UWTBRVaelComponent, bOverheated);
    DOREPLIFETIME(UWTBRVaelComponent, bIsDesperationActive);
    DOREPLIFETIME(UWTBRVaelComponent, bIsDesperationOnCooldown);
}
