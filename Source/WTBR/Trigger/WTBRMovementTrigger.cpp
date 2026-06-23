// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRMovementTrigger.h"
#include "GameFramework/Character.h"
#include "WTBRCharacter.h"

bool UWTBRMovementTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    return Super::Activate_Implementation(InputValue, bIsDualWield);
}

ACharacter* UWTBRMovementTrigger::GetOwnerAsCharacter() const
{
    return OwnerCharacter.IsValid()
        ? Cast<ACharacter>(OwnerCharacter.Get())
        : nullptr;
}
