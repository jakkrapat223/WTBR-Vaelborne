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

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgrix | VFX")
    void OnFulgrixFired();

    // Called by the Projectile Blueprint when the explosion resolves
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgrix | VFX")
    void OnFulgrixExplode(FVector Center);
};
