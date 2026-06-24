// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRFulgornTrigger.generated.h"

// Fulgorn — Artemis (Multi Homing): locks onto nearby pawns and fires one projectile per target
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFulgornTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Called with the number of targets found before firing
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgorn | VFX")
    void OnFulgornLockOn(int32 TargetCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgorn | VFX")
    void OnFulgornFired();
};
