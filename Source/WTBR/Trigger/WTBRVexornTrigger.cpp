// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVexornTrigger.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRVexornTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    // Passive trigger — effect is applied automatically on equip, not on button press
    return true;
}

void UWTBRVexornTrigger::OnEquipped()
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;

    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) return;

    UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!PingSys)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBRVexornTrigger::OnEquipped — UWTBRActionPingSubsystem not found; "
                 "signal block NOT applied for %s"),
            *OwnerCharacter->GetName());
        return;
    }

    PingSys->RegisterSignalBlock(OwnerCharacter.Get());
    OnVexornActivated();
}

void UWTBRVexornTrigger::OnUnequipped()
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;

    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) return;

    UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!PingSys) return;

    PingSys->UnregisterSignalBlock(OwnerCharacter.Get());
}
