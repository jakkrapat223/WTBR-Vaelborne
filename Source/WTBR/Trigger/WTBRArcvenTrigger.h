// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRArcvenTrigger.generated.h"

class AWTBRProjectileBase;

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRArcvenTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    // bIsDualWield=true → parallel dual waves (Option A)
    // bIsDualWield=false → single forward wave
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Blueprint plays spawn VFX/sound on owner
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Arcven | VFX")
    void OnArcvenFired(bool bIsDual);

    // Called by the Projectile Blueprint when it hits a target
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Arcven | VFX")
    void OnArcvenHit(FVector ImpactPoint);

private:
    // Spawn, initialize, and launch one Arc Wave projectile
    void FireArcWave(FVector Origin, FVector Direction, float Damage);
};
