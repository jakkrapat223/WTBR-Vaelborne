// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRVenyxTrigger.generated.h"

// Venyx — Hound (Homing Bullet Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVenyxTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Venyx | VFX")
    void OnVenyxFired();

    // Fired when a homing target is acquired
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Venyx | VFX")
    void OnVenyxHoming();
};
