// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Trigger/WTBRTriggerBase.h"   // ETriggerCategory — needed for CachedCategory in AsyncLoadSlot
#include "WTBRTriggerDataAsset.generated.h"

// Per-trigger tunable stats.  All values with "⚠ Playtest" in GDD must live here.
UCLASS(BlueprintType)
class WTBR_API UWTBRTriggerDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText TriggerName;

    // Trigger category — used by TriggerSetComponent for Pure Type-Match dual-wield (GDD §3.4)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    ETriggerCategory Category = ETriggerCategory::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
    float BaseDamage = 100.f;

    // Vael (energy) cost per activation. Validated server-side before any effect.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Resources",
        meta = (ClampMin = "0"))
    float VaelCostPerUse = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource")
    float CooldownDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource")
    float OverheatDuration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DualWield")
    float DualWieldStaminaMultiplier = 1.5f; // Dual Wield costs 1.5x Stamina (Lock)
};
