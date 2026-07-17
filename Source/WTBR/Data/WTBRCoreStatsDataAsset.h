// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WTBRCoreStatsDataAsset.generated.h"

// Central data asset for all tunable Phase-1 values.
// All values marked "rอ Playtest" in GDD must live here — never hardcode them.
UCLASS(BlueprintType)
class WTBR_API UWTBRCoreStatsDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // ─── HP ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HP")
    float MaxHP = 300.f;                    // Lock: Hornet 200 dmg = 66% HP

    // ─── Stamina ────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina")
    float MaxStamina = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina")
    float StaminaDodgeCost = 33.f;         // Lock: 3 dodges before exhaustion

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina")
    float StaminaRegenRate = 35.f;         // units/second

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina")
    float StaminaRegenDelay = 1.2f;        // seconds after last consume

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina")
    float StaminaExhaustedSpeedPenalty = 0.10f; // multiplicative

    // ─── Limb Destruction (⚠ Playtest: HP drain 0.5–1.0%) ─────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
    float LimbArmDamagePenalty  = 0.35f;   // multiplicative

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
    float LimbArmSpeedPenalty   = 0.35f;   // multiplicative

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
    float LimbLegSpeedPenalty   = 0.40f;   // multiplicative — disables sprint

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
    float LimbHPDrainRateMin    = 0.0050f; // fraction of MaxHP per second (0.5%)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
    float LimbHPDrainRateMax    = 0.0100f; // fraction of MaxHP per second (1.0%)

    // ─── Vael Capsule ───────────────────────────────────────────────────────
    // GDD 2.3: Cast time for Vael Capsule (limb restore item). Lock: 1.5 sec
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Core Stats | Vael Capsule",
        meta = (ClampMin = "0"))
    float VaelCapsuleCastTime  = 1.5f;    // seconds — interruptable

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Rewards", meta = (ClampMin = "0"))
    float VaelRewardOnKill = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Rewards", meta = (ClampMin = "0"))
    float VaelRewardOnAssist = 15.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Rewards", meta = (ClampMin = "0"))
    float VaelRewardOnDown = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael", meta = (ClampMin = "0.0"))
    float MaxVael = 1000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Rewards", meta = (ClampMin = "0"))
    float AssistTimeWindow = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Rewards", meta = (ClampMin = "0"))
    float AssistMinimumDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat | Downed", meta = (ClampMin = "0"))
    float MaxDownedHP = 100.0f;

    // Bleed-out: a downed combatant with no teammate reviving is Eliminated when
    // this many seconds elapse (mode design lock 2026-07-13 for TeamThree15P/BR).
    // The countdown PAUSES while a teammate is holding revive and RESUMES from the
    // remaining value on release (never resets). Tunable; placeholder = 30 s.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat | Downed", meta = (ClampMin = "0"))
    float BleedOutDuration = 30.0f;

    // Uninterrupted hold time a teammate needs to complete a revive.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat | Revive", meta = (ClampMin = "0"))
    float ReviveHoldDuration = 5.0f;

    // HP a revived combatant returns to Alive with (clamped to MaxHP). Placeholder.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat | Revive", meta = (ClampMin = "0"))
    float ReviveHPRestored = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat | Knockdown", meta = (ClampMin = "0"))
    float KnockdownIFrameDuration = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Desperation", meta = (ClampMin = "0"))
    float LowVaelThreshold = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Desperation", meta = (ClampMin = "0"))
    float DesperationDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Desperation", meta = (ClampMin = "0"))
    float DesperationCooldown = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vael | Desperation", meta = (ClampMin = "0"))
    float DesperationCostMultiplier = 0.5f;

    // ─── Combat | Hit Zones (headshot-style damage multipliers) ────────────
    // ⚠ PLAYTEST PENDING — industry-standard BR/FPS placeholder values, never
    // playtested against this game's TTK/300 MaxHP specifically.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones")
    float HeadDamageMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones")
    float TorsoDamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones")
    float ArmDamageMultiplier = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones")
    float LegDamageMultiplier = 0.85f;

    // Fraction of capsule height (0 = feet, 1 = top of head) at/above which an
    // impact point counts as a Head hit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones", meta=(ClampMin="0.0", ClampMax="1.0"))
    float HeadZoneHeightThreshold = 0.85f;

    // Below this height fraction, an impact counts as a Leg hit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones", meta=(ClampMin="0.0", ClampMax="1.0"))
    float LegZoneHeightThreshold = 0.35f;

    // Within the torso height band, an impact whose lateral distance from the
    // capsule's central vertical axis exceeds (capsule radius * this ratio)
    // counts as an Arm hit instead of Torso — there's no separate arm
    // geometry to test against, this is an approximation of "outstretched
    // limb" via how far off-center the hit landed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat | Hit Zones", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ArmLateralRadiusRatio = 0.75f;

    // ─── Movement ───────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float BaseWalkSpeed         = 600.f;   // cm/s (UE5 unit = 1 cm)

    // ─── Sprint ─────────────────────────────────────────────────────────────
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WTBR | Sprint")
    float SprintStaminaCostPerSecond = 20.0f; // drained per second (2.0 per 0.1s tick)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WTBR | Sprint")
    float VaelSprintSpeedMultiplier = 1.4f; // +40% speed
};
