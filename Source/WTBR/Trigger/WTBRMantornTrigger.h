// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRMantornTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRMantornTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    // bIsDualWield is always ignored — Mantorn is single-wield only (GDD §3.4 Lock)
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornSpin();
};
