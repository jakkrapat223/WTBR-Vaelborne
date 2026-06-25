// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRGunnerTrigger.generated.h"

class AWTBRProjectileBase;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRGunnerTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintPure, Category="WTBR | Gunner | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Gunner | VFX")
    void OnGunnerFired(bool bIsDualWield, const FVector& FireDirection);

protected:
    // NOTE: Pass parameters directly so child classes can provide their specific struct data
    AWTBRProjectileBase* FireProjectile(
        TSubclassOf<AWTBRProjectileBase> ProjClass,
        float Damage,
        float Speed,
        float SpreadAngle,
        bool bExplode = false,
        float ExplodeRadius = 0.0f);
    FVector GetFireDirection() const;
    FVector GetMuzzleLocation(const FVector& AimDirection) const;

    // Override in each child to return trigger-specific fire cooldown (DataAsset-driven)
    virtual float GetCooldownDuration() const { return 0.5f; }

    void StartCooldown();

    bool bIsOnCooldown = false;
    FTimerHandle CooldownTimer;

private:
    UFUNCTION()
    void OnCooldownExpired();
};
