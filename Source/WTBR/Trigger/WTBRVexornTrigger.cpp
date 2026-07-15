// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVexornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Engine/World.h"

bool UWTBRVexornTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/, bool /*bIsDualWield*/)
{
    // Bagworm is passive: equipping it, not pressing a fire button, controls cloak.
    return false;
}

void UWTBRVexornTrigger::OnReleased_Implementation(
    const FInputActionValue& /*InputValue*/, bool /*bIsDualWield*/)
{
}

void UWTBRVexornTrigger::Deactivate_Implementation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_CloakDrain);
    }
    SetCloakActive(false);
}

void UWTBRVexornTrigger::OnEquipped()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    // Charge immediately, then sustain at 0.5-second intervals. If Vael runs
    // out the cloak drops; it automatically resumes once it can be sustained.
    TickCloakDrain();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(TimerHandle_CloakDrain, this,
            &UWTBRVexornTrigger::TickCloakDrain, 0.5f, true);
    }
}

void UWTBRVexornTrigger::OnUnequipped()
{
    Deactivate_Implementation();
}

void UWTBRVexornTrigger::TickCloakDrain()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority() || !IsValid(DataAsset)) return;

    const float Drain = FMath::Max(DataAsset->VexornParams.VexornVaelDrainPerSecond, 0.0f) * 0.5f;
    UWTBRVaelComponent* Vael = OwnerCharacter->VaelComponent;
    const bool bCanSustain = Drain <= 0.0f || (IsValid(Vael) && Vael->TryConsumeVael(Drain));
    SetCloakActive(bCanSustain);
}

void UWTBRVexornTrigger::SetCloakActive(bool bNewActive)
{
    if (!OwnerCharacter.IsValid() || bSuppressionActive == bNewActive) return;
    bSuppressionActive = bNewActive;
    OwnerCharacter->SetRadarCloaked(bNewActive);
    OnRep_bSuppressionActive();
}
