// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRBlackTrigger.h"
#include "WTBRVentryxTrigger.generated.h"

// Ventryx — Fujin (Wind): forward knockback blast, Black Trigger
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVentryxTrigger : public UWTBRBlackTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
};
