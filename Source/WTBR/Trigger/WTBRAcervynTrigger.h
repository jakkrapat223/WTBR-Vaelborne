// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRAcervynTrigger.generated.h"

// Acervyn — Hornet (Burst Homing Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRAcervynTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Acervyn | VFX")
    void OnAcervynFired();
};
