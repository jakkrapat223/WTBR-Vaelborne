// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRMovementExtComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UWTBRMovementExtComponent::UWTBRMovementExtComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWTBRMovementExtComponent::BeginPlay()
{
    Super::BeginPlay();
    PushSpeedToMovement();
}

void UWTBRMovementExtComponent::SetLimbPenalty(float Penalty)
{
    SpeedModifiers.LimbPenalty = FMath::Clamp(Penalty, 0.f, 1.f);
    PushSpeedToMovement();
}

void UWTBRMovementExtComponent::SetStaminaPenalty(float Penalty)
{
    SpeedModifiers.StaminaPenalty = FMath::Clamp(Penalty, 0.f, 1.f);
    PushSpeedToMovement();
}

void UWTBRMovementExtComponent::SetDebuffPenalty(float Penalty)
{
    SpeedModifiers.DebuffPenalty = FMath::Clamp(Penalty, 0.f, 1.f);
    PushSpeedToMovement();
}

float UWTBRMovementExtComponent::ComputeFinalSpeed() const
{
    // GDD §2.4 — multiplicative stacking (Lock)
    return BaseWalkSpeed
        * (1.f - SpeedModifiers.LimbPenalty)
        * (1.f - SpeedModifiers.StaminaPenalty)
        * (1.f - SpeedModifiers.DebuffPenalty);
}

void UWTBRMovementExtComponent::PushSpeedToMovement()
{
    if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
    {
        Owner->GetCharacterMovement()->MaxWalkSpeed = ComputeFinalSpeed();
    }
}

void UWTBRMovementExtComponent::OnRep_SpeedModifiers()
{
    PushSpeedToMovement();
}

void UWTBRMovementExtComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRMovementExtComponent, SpeedModifiers);
}
