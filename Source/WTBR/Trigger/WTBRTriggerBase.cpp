// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerBase.h"

bool UWTBRTriggerBase::Activate_Implementation(const FInputActionValue& InputValue, bool bIsDualWield) { return true; }
void UWTBRTriggerBase::OnTriggerActivated_Implementation(AActor* OwnerActor, bool bIsMain) {}
void UWTBRTriggerBase::OnTriggerDeactivated_Implementation(AActor* OwnerActor, bool bIsMain) {}
void UWTBRTriggerBase::OnVaelLeftCharacterBounds_Implementation(AActor* OwnerActor) {}
