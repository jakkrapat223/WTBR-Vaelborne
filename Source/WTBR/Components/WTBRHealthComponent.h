// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRHealthComponent.generated.h"

// ─── Enums & Structs (used by WTBRCharacter and WTBRTriggerBase) ────────────

UENUM(BlueprintType)
enum class EWTBRLimbType : uint8
{
    None     UMETA(DisplayName="None"),
    LeftArm  UMETA(DisplayName="Left Arm (Main)"),
    RightArm UMETA(DisplayName="Right Arm (Sub)"),
    LeftLeg  UMETA(DisplayName="Left Leg"),
    RightLeg UMETA(DisplayName="Right Leg"),
};

USTRUCT(BlueprintType)
struct FWTBRLimbState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bDestroyed = false;

    UPROPERTY(BlueprintReadOnly)
    float HPDrainRateMultiplier = 0.f; // fraction of MaxHP drained per second

    UPROPERTY(BlueprintReadOnly)
    float DamagePenalty = 0.f;         // [0,1] multiplicative

    UPROPERTY(BlueprintReadOnly)
    float SpeedPenalty  = 0.f;         // [0,1] multiplicative
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLimbDestroyed, EWTBRLimbType, Limb, FWTBRLimbState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, NewHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDeath);

// ─── Component ──────────────────────────────────────────────────────────────

class UWTBRCoreStatsDataAsset;

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRHealthComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
    TObjectPtr<UWTBRCoreStatsDataAsset> StatsData;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHPChanged OnHPChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnLimbDestroyed OnLimbDestroyed;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDeath OnDeath;

    UFUNCTION(BlueprintCallable, Category="Health")
    void ApplyDamage(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category="Health")
    void DestroyLimb(EWTBRLimbType Limb);

    UFUNCTION(BlueprintCallable, Category="Health")
    void RestoreLimb(EWTBRLimbType Limb);

    UFUNCTION(BlueprintPure, Category="Health")
    float GetCurrentHP() const { return CurrentHP; }

    UFUNCTION(BlueprintPure, Category="Health")
    float GetMaxHP() const;

    UFUNCTION(BlueprintPure, Category="Health")
    bool IsAlive() const { return CurrentHP > 0.f; }

    UFUNCTION(BlueprintPure, Category="Health")
    FWTBRLimbState GetLimbState(EWTBRLimbType Limb) const;

    // Multiplicative total — call this to feed into WTBRMovementExtComponent
    float GetTotalSpeedPenaltyFromLimbs() const;
    float GetTotalDamagePenaltyFromLimbs() const;

protected:
    virtual void BeginPlay() override;

private:
    // Index 0=None(unused), 1=LeftArm, 2=RightArm, 3=LeftLeg, 4=RightLeg
    UPROPERTY(Replicated)
    TArray<FWTBRLimbState> LimbStates;

    UPROPERTY(Replicated)
    float CurrentHP = 300.f;

    FTimerHandle LimbDrainTimerHandle;

    // Starts (or keeps) the drain timer running; called whenever a limb is destroyed
    void RefreshLimbDrainTimer();

    // Timer callback — fires every LIMB_DRAIN_TICK_INTERVAL seconds on Authority
    void TickLimbDrain();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
