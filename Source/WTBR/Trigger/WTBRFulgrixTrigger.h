// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRFulgrixTrigger.generated.h"

// Fulgrix — Meteor (Explosive Bullet Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFulgrixTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;
    virtual float GetCooldownDuration() const override;

    // HOLD. Meteor's presets are about DIRECTION — where the blasts land — not the
    // curve control Viper owns. The cubes still each detonate; only the shape moves.
    virtual const TArray<FWTBRPathPreset>* GetHoldPresets() const override;
    virtual float GetHoldVaelCost() const override;
    virtual float GetHoldChargeSeconds() const override;
    virtual bool FireHoldPreset(int32 PresetIndex, float ChargeFraction, bool bIsMain) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgrix | VFX")
    void OnFulgrixFired();

    // Called by the Projectile Blueprint when the explosion resolves
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgrix | VFX")
    void OnFulgrixExplode(FVector Center);
};
