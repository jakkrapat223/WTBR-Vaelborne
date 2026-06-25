// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRSniperTrigger.h"
#include "WTBRTelornTrigger.generated.h"

// Telorn — Egret (Long Range Sniper)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRTelornTrigger : public UWTBRSniperTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
};
