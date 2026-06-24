// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRVentryxTrigger.generated.h"

// Ventryx — Fujin (Wind): fires a forward projectile with knockback
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVentryxTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Ventryx | VFX")
    void OnVentryxActivated();
};
