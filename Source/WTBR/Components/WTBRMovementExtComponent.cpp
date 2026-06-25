// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRMovementExtComponent.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRStaminaComponent.h"
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
    StaminaComponent = GetOwner()->FindComponentByClass<UWTBRStaminaComponent>();
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

void UWTBRMovementExtComponent::StartVaelSprint()
{
    AWTBRCharacter* Owner = Cast<AWTBRCharacter>(GetOwner());
    if (!Owner || !Owner->HasAuthority()) return;
    if (!StaminaComponent || StaminaComponent->GetCurrentStamina() <= 0.f) return;

    bIsSprinting = true;
    OnRep_bIsSprinting();

    UWorld* World = GetWorld();
    if (!IsValid(World)) return;
    World->GetTimerManager().SetTimer(
        TimerHandle_SprintStaminaDrain,
        this, &UWTBRMovementExtComponent::DrainSprintStamina,
        0.1f, true);
}

void UWTBRMovementExtComponent::StopVaelSprint()
{
    AWTBRCharacter* Owner = Cast<AWTBRCharacter>(GetOwner());
    if (!Owner || !Owner->HasAuthority()) return;

    bIsSprinting = false;
    OnRep_bIsSprinting();

    UWorld* World = GetWorld();
    if (IsValid(World))
        World->GetTimerManager().ClearTimer(TimerHandle_SprintStaminaDrain);
}

void UWTBRMovementExtComponent::DrainSprintStamina()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!StaminaComponent) return;
    if (!IsValid(CoreStatsAsset)) return;

    const float CostThisTick = CoreStatsAsset->SprintStaminaCostPerSecond * 0.1f;
    if (!StaminaComponent->TryConsumeStamina(CostThisTick))
    {
        StopVaelSprint();
    }
}

void UWTBRMovementExtComponent::OnRep_bIsSprinting()
{
    ACharacter* Owner = Cast<ACharacter>(GetOwner());
    if (!Owner) return;

    if (IsValid(CoreStatsAsset))
    {
        const float TargetSpeed = bIsSprinting
            ? CoreStatsAsset->BaseWalkSpeed * CoreStatsAsset->VaelSprintSpeedMultiplier
            : CoreStatsAsset->BaseWalkSpeed;
        Owner->GetCharacterMovement()->MaxWalkSpeed = TargetSpeed;
    }
    else
    {
        // Fallback: use component's own BaseWalkSpeed with no multiplier
        Owner->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
    }
}

void UWTBRMovementExtComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRMovementExtComponent, SpeedModifiers);
    DOREPLIFETIME(UWTBRMovementExtComponent, bIsSprinting);
}
