// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRMovementExtComponent.h"
#include "WTBRValidationLog.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/WTBRStaminaComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
void LogTest32MovementCoreStats(const UWTBRMovementExtComponent* Component, const UWTBRCoreStatsDataAsset* Stats, const TCHAR* Requester)
{
    if (Stats)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Loaded] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsAsset=%s | CoreStatsPath=%s | StatsValid=true | BaseWalkSpeed=%.1f | SprintCostPerSecond=%.1f | VaelSprintSpeedMultiplier=%.2f | MaxStamina=%.1f"),
            *GetNameSafe(Component),
            *GetNameSafe(Component ? Component->GetOwner() : nullptr),
            Component ? (int32)Component->GetNetMode() : -1,
            Component && Component->GetOwner() ? (int32)Component->GetOwner()->GetLocalRole() : -1,
            Requester ? Requester : TEXT("Unknown"),
            *GetNameSafe(Stats),
            Stats ? *Stats->GetPathName() : TEXT("None"),
            Stats->BaseWalkSpeed,
            Stats->SprintStaminaCostPerSecond,
            Stats->VaelSprintSpeedMultiplier,
            Stats->MaxStamina);
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Missing] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsPath=None | StatsValid=false"),
        *GetNameSafe(Component),
        *GetNameSafe(Component ? Component->GetOwner() : nullptr),
        Component ? (int32)Component->GetNetMode() : -1,
        Component && Component->GetOwner() ? (int32)Component->GetOwner()->GetLocalRole() : -1,
        Requester ? Requester : TEXT("Unknown"));
}
}

UWTBRMovementExtComponent::UWTBRMovementExtComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWTBRMovementExtComponent::BeginPlay()
{
    Super::BeginPlay();
    LogTest32MovementCoreStats(this, CoreStatsAsset.Get(), TEXT("BeginPlay"));
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

float UWTBRMovementExtComponent::GetSprintSpeedMultiplier() const
{
    return IsValid(CoreStatsAsset) ? CoreStatsAsset->VaelSprintSpeedMultiplier : 1.0f;
}

void UWTBRMovementExtComponent::PushSpeedToMovement()
{
    // Routed through the character so the crouch/prone multiplier and
    // MaxWalkSpeedCrouched are always reapplied. Writing MaxWalkSpeed directly
    // here used to clobber the stance speed on every limb/stamina/debuff change,
    // which is how a crouching or prone character ended up moving at full speed.
    if (AWTBRCharacter* Owner = Cast<AWTBRCharacter>(GetOwner()))
    {
        Owner->RefreshStanceSpeeds();
    }
    else if (ACharacter* PlainOwner = Cast<ACharacter>(GetOwner()))
    {
        PlainOwner->GetCharacterMovement()->MaxWalkSpeed = ComputeFinalSpeed();
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
    if (bIsSprinting || !Owner->CanStartVaelSprint()) return;
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
    if (!bIsSprinting) return;

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
    LogTest32MovementCoreStats(this, CoreStatsAsset.Get(), TEXT("DrainSprintStamina"));
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

    LogTest32MovementCoreStats(this, CoreStatsAsset.Get(), TEXT("OnRep_bIsSprinting"));

    // Same reasoning as PushSpeedToMovement: the character folds the sprint
    // multiplier and the stance multiplier together in one place.
    if (AWTBRCharacter* WTBROwner = Cast<AWTBRCharacter>(Owner))
    {
        WTBROwner->RefreshStanceSpeeds();
        return;
    }

    if (IsValid(CoreStatsAsset))
    {
        const float TargetSpeed = bIsSprinting
            ? ComputeFinalSpeed() * CoreStatsAsset->VaelSprintSpeedMultiplier
            : ComputeFinalSpeed();
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
