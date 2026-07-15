// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRGunnerTrigger.h"
#include "WTBRAcervynTrigger.generated.h"

// Acervyn — standalone Burst Homing Gunner, NonCombinable (NOT the "Hornet" composite;
// that canon name belongs to Venspire = Venyx+Venyx)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRAcervynTrigger : public UWTBRGunnerTrigger
{
    GENERATED_BODY()

public:
    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Acervyn | VFX")
    void OnAcervynFired();

private:
    FTimerHandle BurstTimer;
    int32 BurstShotsRemaining = 0;
    TWeakObjectPtr<AActor> BurstTarget;

    void FireNextBurstShot();
};
