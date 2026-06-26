// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRVaelComponent.h"
#include "Net/UnrealNetwork.h"

UWTBRVaelComponent::UWTBRVaelComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    CurrentVael = MaxVael;
}

void UWTBRVaelComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentVael = MaxVael;
}

bool UWTBRVaelComponent::TryConsumeVael(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Amount <= 0.0f) return false;

    // RawCost is the caller's requested spend; EffectiveCost is the actual Vael deducted.
    const float RawCost = Amount;
    const float CostMultiplier = GetVaelCostMultiplier();
    const float EffectiveCost = FMath::Max(0.0f, RawCost * CostMultiplier);

    if (bOverheated || CurrentVael < EffectiveCost) return false;

    const float OldVael = CurrentVael;
    CurrentVael = FMath::Clamp(CurrentVael - EffectiveCost, 0.0f, MaxVael);
    OnVaelChanged.Broadcast(CurrentVael, MaxVael);

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
    CurrentVael = FMath::Clamp(CurrentVael + Amount, 0.0f, MaxVael);

    const bool bWasOverheated = bOverheated;
    if (CurrentVael > 0.0f)
    {
        bOverheated = false;
    }

    if (!FMath::IsNearlyEqual(CurrentVael, PreviousVael))
    {
        OnVaelChanged.Broadcast(CurrentVael, MaxVael);
    }

    if (bOverheated != bWasOverheated)
    {
        OnOverheatChanged.Broadcast(bOverheated);
    }

    return CurrentVael > PreviousVael;
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

float UWTBRVaelComponent::GetVaelCostMultiplier() const
{
    if (!bIsDesperationActive) return 1.0f;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    if (!S) return 1.0f;

    return FMath::Clamp(S->DesperationCostMultiplier, 0.0f, 1.0f);
}

void UWTBRVaelComponent::NotifyVaelLeftCharacterBounds()
{
    // Only fires when Vael energy exits the capsule — see Action Ping Rule in GDD §3.2
    OnVaelReleased.Broadcast();
}

void UWTBRVaelComponent::OnRep_CurrentVael()
{
    OnVaelChanged.Broadcast(CurrentVael, MaxVael);
}

void UWTBRVaelComponent::OnRep_bOverheated()
{
    OnOverheatChanged.Broadcast(bOverheated);
}

void UWTBRVaelComponent::OnRep_IsDesperationActive()
{
    OnDesperationActiveChanged.Broadcast(bIsDesperationActive);
}

void UWTBRVaelComponent::OnRep_IsDesperationOnCooldown()
{
    OnDesperationCooldownChanged.Broadcast(bIsDesperationOnCooldown);
}

void UWTBRVaelComponent::HandleVaelChanged(float OldVael, float NewVael)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsDesperationActive || bIsDesperationOnCooldown) return;

    const UWTBRCoreStatsDataAsset* S = GetStats();
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

    bIsDesperationActive = bNewActive;
    OnDesperationActiveChanged.Broadcast(bIsDesperationActive);
}

void UWTBRVaelComponent::SetDesperationOnCooldown(bool bNewOnCooldown)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsDesperationOnCooldown == bNewOnCooldown) return;

    bIsDesperationOnCooldown = bNewOnCooldown;
    OnDesperationCooldownChanged.Broadcast(bIsDesperationOnCooldown);
}

void UWTBRVaelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRVaelComponent, CurrentVael);
    DOREPLIFETIME(UWTBRVaelComponent, bOverheated);
    DOREPLIFETIME(UWTBRVaelComponent, bIsDesperationActive);
    DOREPLIFETIME(UWTBRVaelComponent, bIsDesperationOnCooldown);
}
