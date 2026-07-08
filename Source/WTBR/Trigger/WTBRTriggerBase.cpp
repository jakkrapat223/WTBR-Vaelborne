// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRTriggerBase.h"
#include "WTBRCharacter.h"

void UWTBRTriggerBase::InitializeTrigger(AWTBRCharacter* InOwnerCharacter, UWTBRTriggerDataAsset* InDataAsset)
{
    OwnerCharacter = InOwnerCharacter;
    DataAsset = InDataAsset;
    if (DataAsset)
    {
        Category = DataAsset->Category;
    }
}

AWTBRCharacter* UWTBRTriggerBase::GetOwningWTBRCharacter() const
{
    return OwnerCharacter.Get();
}

FText UWTBRTriggerBase::GetFunctionalName() const
{
    return IsValid(DataAsset) ? DataAsset->FunctionalName : FText::FromString("Unknown Trigger");
}

FText UWTBRTriggerBase::GetFunctionalDescription() const
{
    return IsValid(DataAsset) ? DataAsset->FunctionalDescription : FText::FromString("Missing Description");
}

bool UWTBRTriggerBase::Activate_Implementation(const FInputActionValue& InputValue, bool bIsDualWield) { return true; }
void UWTBRTriggerBase::OnTriggerActivated_Implementation(AActor* OwnerActor, bool bIsMain) {}
void UWTBRTriggerBase::OnTriggerDeactivated_Implementation(AActor* OwnerActor, bool bIsMain) {}
void UWTBRTriggerBase::OnVaelLeftCharacterBounds_Implementation(AActor* OwnerActor) {}
void UWTBRTriggerBase::OnReleased_Implementation(const FInputActionValue& InputValue, bool bIsDualWield) {}
void UWTBRTriggerBase::Deactivate_Implementation() {}
