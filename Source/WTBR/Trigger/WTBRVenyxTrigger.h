// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRVenyxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVenyxTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
};
