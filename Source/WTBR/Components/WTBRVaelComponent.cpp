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
    if (bOverheated || CurrentVael < Amount) return false;

    CurrentVael -= Amount;
    OnVaelChanged.Broadcast(CurrentVael, MaxVael);

    if (CurrentVael <= 0.f)
    {
        CurrentVael = 0.f;
        bOverheated  = true;
        OnOverheatChanged.Broadcast(true);
    }
    return true;
}

void UWTBRVaelComponent::NotifyVaelLeftCharacterBounds()
{
    // Only fires when Vael energy exits the capsule — see Action Ping Rule in GDD §3.2
    OnVaelReleased.Broadcast();
}

void UWTBRVaelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRVaelComponent, CurrentVael);
    DOREPLIFETIME(UWTBRVaelComponent, bOverheated);
}
