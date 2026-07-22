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
    /**
     * Conjure one block, split it into CubeCount cubes, fire them — the shared
     * energy-bullet mechanic. Solux, Fulgrix and Venyx all tap through here; each
     * keeps its own per-cube payload (raw / explosive / homing) untouched, because
     * only the FORMATION is shared, never the behaviour.
     *
     * TotalDamage is SPLIT across the cubes, never multiplied. Every caller passes
     * the damage of one whole shot.
     *
     * The cubes are conjured on a sphere of ScatterRadius around the muzzle and each
     * flies STRAIGHT to the matching point on a second sphere of ImpactSpread,
     * centred ConvergeDistance ahead. The two spheres are the same pattern at two
     * scales, so the volley holds its formation for the whole flight and never
     * crosses through itself.
     *
     * ImpactSpread of zero closes the volley to a single point, which is what keeps
     * a tap reliable: however wide it looks leaving the body, it arrives where the
     * player aimed. Larger than ScatterRadius opens it into a footprint instead —
     * currently Fulgrix only, see FulgrixTapImpactSpread.
     *
     * Returns the spawned cubes so a caller can post-process them (Venyx arms
     * homing on each). Empty on any failure.
     */
    TArray<AWTBRProjectileBase*> FireProjectileVolley(
        TSubclassOf<AWTBRProjectileBase> ProjClass,
        int32 CubeCount,
        float TotalDamage,
        float Speed,
        float ScatterRadius,
        float ConvergeDistance,
        float ImpactSpread = 0.0f,
        bool bExplode = false,
        float ExplodeRadius = 0.0f);

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
