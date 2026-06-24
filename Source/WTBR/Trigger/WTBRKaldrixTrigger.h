// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRKaldrixTrigger.generated.h"

// Kaldrix — Kazan (Explosion Zone): line-traces to floor and places a timed blast zone
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRKaldrixTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Kaldrix | VFX")
    void OnKaldrixActivated();
};
