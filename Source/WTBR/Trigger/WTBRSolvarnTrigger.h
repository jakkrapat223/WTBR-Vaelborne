// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRSolvarnTrigger.generated.h"

// Solvarn — Suiren (Defense Field): spawns a projectile-destroying sphere around the user
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSolvarnTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;
};
