// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRStaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, bool, bIsExhausted);

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRStaminaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRStaminaComponent();

    // ── DataAsset Reference ──────────────────────────────────────────────────
    // Single source of truth for all tunable stats.
    // Set this reference in BP_WTBRCharacter component defaults (Details panel).
    // All gameplay values are read from this asset at BeginPlay — never hardcoded.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Config")
    TSoftObjectPtr<UWTBRCoreStatsDataAsset> CoreStatsAsset;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnStaminaChanged OnStaminaChanged;

    // Returns false if exhausted or insufficient stamina; also resets the regen timer
    UFUNCTION(BlueprintCallable, Category="Stamina")
    bool TryConsumeStamina(float Amount);

    // Convenience: consumes the dodge cost defined in CoreStatsAsset
    UFUNCTION(BlueprintCallable, Category="Stamina")
    bool TryConsumeDodgeStamina();

    UFUNCTION(BlueprintPure, Category="Stamina")
    float GetCurrentStamina() const { return CurrentStamina; }

    UFUNCTION(BlueprintPure, Category="Stamina")
    float GetMaxStamina() const;

    UFUNCTION(BlueprintPure, Category="Stamina")
    float GetDodgeCost() const;

    UFUNCTION(BlueprintPure, Category="Stamina")
    bool IsExhausted() const { return bExhausted; }

    // Returns the speed penalty multiplier to feed into WTBRMovementExtComponent
    float GetSpeedPenalty() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Replicated)
    float CurrentStamina = 100.f;

    UPROPERTY(Replicated)
    bool bExhausted = false;

    // One-shot timer: fires StartRegenTick() after StaminaRegenDelay
    FTimerHandle RegenDelayTimer;

    // Looping timer: fires TickRegen() every 0.05 s until stamina is full
    FTimerHandle RegenTickTimer;

    const UWTBRCoreStatsDataAsset* GetStats() const
    {
        if (CoreStatsAsset.IsNull())
        {
            return nullptr;
        }

        return CoreStatsAsset.LoadSynchronous();
    }

    // Starts the looping regen tick — called by RegenDelayTimer
    void StartRegenTick();

    // Adds RegenRate * 0.05 s of stamina; stops itself when full
    void TickRegen();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
