// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRSerpveilTrigger.generated.h"

// Serpveil — Viper (Curved Path Gunner)
// TODO Phase 4: Implement Custom Path System
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSerpveilTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Serpveil | VFX")
    void OnSerpveilFired();
};
