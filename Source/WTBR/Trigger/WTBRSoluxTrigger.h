// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRSoluxTrigger.generated.h"

// Solux — Asteroid (Normal Bullet Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSoluxTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Solux | VFX")
    void OnSoluxFired();
};
