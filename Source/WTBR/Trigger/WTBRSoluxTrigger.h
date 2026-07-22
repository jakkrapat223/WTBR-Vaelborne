// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRSoluxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSoluxTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // HOLD. Shape and timing only — Solux has no per-cube ability to modify, which
    // is exactly why its presets are the clearest illustration that hold buys
    // control rather than power.
    virtual const TArray<FWTBRPathPreset>* GetHoldPresets() const override;
    virtual float GetHoldVaelCost() const override;
    virtual float GetHoldChargeSeconds() const override;
    virtual bool FireHoldPreset(int32 PresetIndex, float ChargeFraction, bool bIsMain) override;

protected:
    virtual float GetCooldownDuration() const override;
};
