// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRMovementTrigger.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRMovementTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

protected:
    ACharacter* GetOwnerAsCharacter() const;
};
