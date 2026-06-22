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

    // ─── Movement ───────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float BaseWalkSpeed         = 600.f;   // cm/s (UE5 unit = 1 cm)
};
