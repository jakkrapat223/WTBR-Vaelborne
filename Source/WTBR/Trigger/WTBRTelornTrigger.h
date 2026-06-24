// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRTelornTrigger.generated.h"

// Telorn — Egret (Long Range Sniper)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRTelornTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Telorn | VFX")
    void OnTelornFired();
};
