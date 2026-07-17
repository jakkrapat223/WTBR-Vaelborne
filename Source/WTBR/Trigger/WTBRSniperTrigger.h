// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRSniperTrigger.generated.h"

class AWTBRProjectileBase;
struct FWTBRProjectileVFXConfig;

// Aim-then-fire: press starts aiming (zoom-in is a separate client-cosmetic
// concern, see AWTBRCharacter::UpdateSniperZoom) but does NOT fire; release
// fires the shot using whatever direction the player is aiming at that
// moment. A press that's immediately released still fires — there's no
// minimum aim/charge time, holding just means the camera has zoomed in
// further by release time.
UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRSniperTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | State")
    bool IsOnCooldown() const { return bIsOnCooldown; }

    // Public wrapper so client-side code (AWTBRCharacter's cooldown-
    // prediction gate on UpdateSniperZoom) can read the per-weapon cooldown
    // length without needing the real (server-only, replicates nowhere)
    // bIsOnCooldown state.
    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | State")
    float GetCooldownDurationForHUD() const { return GetCooldownDuration(); }

    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | State")
    bool IsAiming() const { return bIsAiming; }

    // Client-side camera FOV to lerp toward while this Sniper's Fire button
    // is held (AWTBRCharacter::UpdateSniperZoom). Cosmetic only.
    UFUNCTION(BlueprintPure, Category = "WTBR | Sniper | Zoom")
    virtual float GetZoomFOV() const { return 60.0f; }

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue, bool bIsDualWield) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue, bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

protected:
    AWTBRProjectileBase* FireSniper(
        TSubclassOf<AWTBRProjectileBase> ProjClass,
        float Damage, float Speed, float Range,
        bool bCanPenetrate = false,
        int32 CubeSplitCount = 1,
        const FWTBRProjectileVFXConfig* VFXConfig = nullptr);

    FVector GetFireDirection() const;
    FVector GetMuzzleLocation(const FVector& AimDirection) const;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Sniper | VFX")
    void OnSniperFired(const FVector& FireDirection);

    virtual float GetCooldownDuration() const { return 1.0f; }
    void StartCooldown();

    // Subclass does the actual shot on release: validate DataAsset/Vael, call
    // FireSniper with its own weapon params. Return whether it actually
    // fired — OnReleased only starts the cooldown when this returns true, so
    // a Vael-starved release doesn't cost a cooldown for nothing.
    virtual bool ExecuteFire() { return false; }

    bool bIsOnCooldown = false;
    bool bIsAiming = false;
    FTimerHandle CooldownTimer;

private:
    UFUNCTION()
    void OnCooldownExpired();
};
