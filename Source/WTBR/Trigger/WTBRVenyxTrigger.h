// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRVenyxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVenyxTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    // TAP. One missile, homing at a target picked from the aim cone before it
    // leaves. Unchanged by the preset work below on purpose: tap is the instant,
    // reliable answer, so sweeps and waves belong to hold only.
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;

    /**
     * HOLD. Fires the chosen VenyxPresets entry as a swept volley: cubes fly the
     * authored arcs, in waves, each hunting whoever it passes near. Nothing is
     * locked when the shot leaves.
     *
     * Server-authoritative — the index is bounds-checked and the reach re-derived
     * here, so a client may ask for a preset but never dictate the shot.
     */
    virtual bool FireHoldPreset(int32 PresetIndex, float ChargeFraction, bool bIsMain) override;

    // Seconds of held charge that reach full range. Safe when DataAsset is null.
    virtual float GetHoldChargeSeconds() const override;

    virtual const TArray<FWTBRPathPreset>* GetHoldPresets() const override;
    virtual float GetHoldVaelCost() const override;
};
