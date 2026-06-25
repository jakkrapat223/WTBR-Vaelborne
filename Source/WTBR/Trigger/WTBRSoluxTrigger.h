// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRSoluxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSoluxTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

protected:
    virtual float GetCooldownDuration() const override;
};
