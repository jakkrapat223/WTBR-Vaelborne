// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRBlackTrigger.h"
#include "WTBRKaldrixTrigger.generated.h"

// Kaldrix — Kazan (Explosion Zone): places a timed blast zone at character location
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRKaldrixTrigger : public UWTBRBlackTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
};
