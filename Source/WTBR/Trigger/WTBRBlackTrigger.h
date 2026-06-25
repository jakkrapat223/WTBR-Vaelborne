// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRBlackTrigger.generated.h"

class AWTBRProjectileBase;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRBlackTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "WTBR | BlackTrigger | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

protected:
    AWTBRProjectileBase* FireBlackProjectile(
        TSubclassOf<AWTBRProjectileBase> ProjClass,
        float Damage,
        float Speed,
        float KnockbackForce = 0.0f,
        int32 CubeSplitCount = 1);

    FVector GetFireDirection() const;
    FVector GetMuzzleLocation(const FVector& AimDirection) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | BlackTrigger | VFX")
    void OnBlackTriggerFired(const FVector& FireDirection);

    virtual float GetCooldownDuration() const { return 2.0f; }
    void StartCooldown();

    bool bIsOnCooldown = false;
    FTimerHandle CooldownTimer;

private:
    UFUNCTION()
    void OnCooldownEnd();
};
