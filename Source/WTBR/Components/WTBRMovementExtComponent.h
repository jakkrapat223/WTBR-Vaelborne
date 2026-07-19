// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRMovementExtComponent.generated.h"

class UWTBRStaminaComponent;

// All speed penalties stack MULTIPLICATIVELY: FinalSpeed = Base × (1-Limb) × (1-Stamina) × (1-Debuff)
// Never additive — prevents negative speed under combined debuffs (GDD §2.4, Lock)
USTRUCT(BlueprintType)
struct FWTBRSpeedModifiers
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float LimbPenalty    = 0.f; // composite from all destroyed limbs

    UPROPERTY(BlueprintReadOnly)
    float StaminaPenalty = 0.f; // from stamina exhaustion

    UPROPERTY(BlueprintReadOnly)
    float DebuffPenalty  = 0.f; // external debuffs (e.g. Kazan Entropy)
};

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRMovementExtComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRMovementExtComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
    float BaseWalkSpeed = 600.f; // override per DataAsset in BeginPlay

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Sprint")
    TObjectPtr<UWTBRCoreStatsDataAsset> CoreStatsAsset;

    UFUNCTION(BlueprintCallable, Category="Movement")
    void SetLimbPenalty(float Penalty);

    UFUNCTION(BlueprintCallable, Category="Movement")
    void SetStaminaPenalty(float Penalty);

    UFUNCTION(BlueprintCallable, Category="Movement")
    void SetDebuffPenalty(float Penalty);

    UFUNCTION(BlueprintPure, Category="Movement")
    float ComputeFinalSpeed() const;

    UFUNCTION(BlueprintPure, Category="Movement")
    FWTBRSpeedModifiers GetSpeedModifiers() const { return SpeedModifiers; }

    UFUNCTION(BlueprintPure, Category="WTBR | Movement")
    float GetNonSprintingSpeed() const { return ComputeFinalSpeed(); }

    UFUNCTION(BlueprintPure, Category="WTBR | Sprint")
    bool IsSprinting() const { return bIsSprinting; }

    // 1.0 when there is no CoreStats asset, so callers can multiply unconditionally.
    UFUNCTION(BlueprintPure, Category="WTBR | Sprint")
    float GetSprintSpeedMultiplier() const;

    UFUNCTION(BlueprintCallable, Category="WTBR | Movement")
    void StartVaelSprint();

    UFUNCTION(BlueprintCallable, Category="WTBR | Movement")
    void StopVaelSprint();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(ReplicatedUsing=OnRep_SpeedModifiers)
    FWTBRSpeedModifiers SpeedModifiers;

    UFUNCTION()
    void OnRep_SpeedModifiers();

    void PushSpeedToMovement();

    void DrainSprintStamina();

    UPROPERTY()
    FTimerHandle TimerHandle_SprintStaminaDrain;

    UPROPERTY()
    TObjectPtr<UWTBRStaminaComponent> StaminaComponent;

    UPROPERTY(ReplicatedUsing=OnRep_bIsSprinting)
    bool bIsSprinting = false;

    UFUNCTION()
    void OnRep_bIsSprinting();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
