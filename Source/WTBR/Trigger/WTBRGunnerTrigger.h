// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRPathPresetTypes.h"
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

    // ─── Hold / preset contract ──────────────────────────────────────────────
    //
    // Solux, Fulgrix and Venyx all hold to CHOOSE A PATTERN — never to buy damage
    // or speed. The whole player-facing flow (hold gesture, wheel, charge, fire)
    // lives on AWTBRCharacter and talks to this base rather than to any one
    // archetype, so an archetype opts in by overriding these three and nothing on
    // the character has to learn about it.
    //
    // Serpveil deliberately does NOT ride this: Viper authors turn points and has
    // its own older flow.

    // Deliberately NOT named FireSelectedPreset/HasPresets. UWTBRSerpveilTrigger
    // already declares its own FireSelectedPreset with a different return type, and
    // a same-name base virtual would be HIDDEN by it rather than overridden — a
    // silent dispatch bug waiting for someone to call it through a base pointer.

    // Null, or empty, leaves the trigger tap-only and the character flow ignores it.
    virtual const TArray<FWTBRPathPreset>* GetHoldPresets() const { return nullptr; }

    // Flat cost per shot regardless of which shape is chosen, so affordability is
    // all-or-nothing rather than per-option.
    virtual float GetHoldVaelCost() const { return 0.0f; }

    // Seconds of held charge that reach full range. Must be safe with a null DataAsset.
    virtual float GetHoldChargeSeconds() const { return 1.2f; }

    /**
     * Fires the chosen preset. SERVER-AUTHORITATIVE: the index arrives from a
     * client, so implementations bounds-check it and re-derive cost and reach here
     * rather than trusting anything sent.
     */
    virtual bool FireHoldPreset(int32 PresetIndex, float ChargeFraction, bool bIsMain)
    {
        return false;
    }

    bool HasHoldPresets() const
    {
        const TArray<FWTBRPathPreset>* Presets = GetHoldPresets();
        return Presets && Presets->Num() > 0;
    }

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

    /**
     * Everything a preset shot needs that is NOT the shape. The shape comes from the
     * preset itself; this is the archetype's own payload and economy.
     */
    struct FWTBRPresetShot
    {
        const TArray<FWTBRPathPreset>* Presets = nullptr;
        TSubclassOf<AWTBRProjectileBase> ProjectileClass;
        float VaelCost = 0.0f;
        float TotalDamage = 0.0f;
        float Speed = 0.0f;
        float MinRange = 0.0f;
        float MaxRange = 0.0f;
        float ScatterRadius = 0.0f;
        // Floor and ceiling for a preset's own CubeCount. The floor is the count the
        // archetype already uses on tap: a preset may buy MORE coverage by dividing
        // the same budget further, never fewer, heavier cubes — that would be a power
        // increase wearing a pattern's clothes.
        int32 MinCubeCount = 0;
        int32 MaxCubeCount = 0;
        // Payload. Left at zero/false by an archetype that has none — Solux carries
        // nothing at all, and that absence is its identity.
        bool bExplodes = false;
        float ExplosionRadius = 0.0f;
        float HomingTurnRateDegPerSec = 0.0f;
        bool bReacquireAfterOvershoot = false;
        // Proximity fuse so a turn-capped chasing cube can connect with a target
        // tighter than its turn radius. Zero = contact-only, the old behaviour.
        float ProximityDetonationRadius = 0.0f;

        // How far this WEAPON hunts, in world units. Non-zero arms every cube of the
        // volley with it, overriding whatever the lane carried.
        //
        // Hunting range is a property of the gun, not of a shape the player drew, so
        // the Preset Editor does not author it per lane — a preset only says WHERE
        // along its path hunting is on or off, via SetHoming markers. Those markers
        // RESTORE the radius the cube was armed with, so without a weapon-supplied
        // value here a player-authored preset would arm at zero and every SetHoming
        // marker in it would do nothing.
        //
        // Zero for archetypes that do not hunt at all (Solux, Fulgrix), which is the
        // default and leaves them exactly as they were.
        float HomingRadiusUU = 0.0f;
    };

    /**
     * Shared hold-fire path: validate, charge Vael, aim from the camera, scale the
     * reach by charge, resolve the preset into cube paths, spawn the volley.
     *
     * Every line of this used to live in UWTBRVenyxTrigger and none of it was
     * Venyx-specific — only the payload was. Copying it per archetype would mean
     * fixing the next bug in one of three places.
     */
    bool FirePresetVolley(int32 PresetIndex, float ChargeFraction, const FWTBRPresetShot& Shot);

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
