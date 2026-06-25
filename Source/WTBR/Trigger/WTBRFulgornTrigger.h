// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRBlackTrigger.h"
#include "WTBRFulgornTrigger.generated.h"

// Fulgorn — Artemis (Multi Homing): locks onto nearby pawns and fires one projectile per target
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFulgornTrigger : public UWTBRBlackTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
};
