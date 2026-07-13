// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRSniperTrigger.generated.h"

class AWTBRProjectileBase;

UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRSniperTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

    // Client-side camera FOV to lerp toward while this Sniper's Fire button
    // is held (AWTBRCharacter::UpdateSniperZoom). Cosmetic only.
    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | Zoom")
    virtual float GetZoomFOV() const { return 60.0f; }

protected:
    AWTBRProjectileBase* FireSniper(
        TSubclassOf<AWTBRProjectileBase> ProjClass,
        float Damage, float Speed, float Range,
        bool bCanPenetrate = false,
        int32 CubeSplitCount = 1);

    FVector GetFireDirection() const;
    FVector GetMuzzleLocation(const FVector& AimDirection) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Sniper | VFX")
    void OnSniperFired(const FVector& FireDirection);

    virtual float GetCooldownDuration() const { return 1.0f; }
    void StartCooldown();

    bool bIsOnCooldown = false;
    FTimerHandle CooldownTimer;

private:
    UFUNCTION()
    void OnCooldownExpired();
};
