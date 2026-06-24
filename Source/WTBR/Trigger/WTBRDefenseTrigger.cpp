// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRCharacter.h"

bool UWTBRDefenseTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    return Super::Activate_Implementation(InputValue, bIsDualWield);
}
