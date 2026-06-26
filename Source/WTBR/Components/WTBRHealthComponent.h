// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "WTBRHealthComponent.generated.h"

class AController;
class AWTBRCharacter;

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

UENUM(BlueprintType)
enum class EWTBRCombatState : uint8
{
    Alive      UMETA(DisplayName="Alive"),
    Downed     UMETA(DisplayName="Downed"),
    Eliminated UMETA(DisplayName="Eliminated"),
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

struct FWTBRDamageContributionRecord
{
    TWeakObjectPtr<AWTBRCharacter> ContributorCharacter;
    TWeakObjectPtr<AController> ContributorController;
    float TotalDamage = 0.f;
    float LastDamageTime = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLimbDestroyed, EWTBRLimbType, Limb, FWTBRLimbState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, NewHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDowned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatStateChanged, EWTBRCombatState, NewState, EWTBRCombatState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInvulnerabilityChanged, bool, bNewInvulnerable);

// ─── Component ──────────────────────────────────────────────────────────────

UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRHealthComponent();

    // ── DataAsset Reference ──────────────────────────────────────────────────
    // Single source of truth for all tunable stats.
    // Set this reference in BP_WTBRCharacter component defaults (Details panel).
    // All gameplay values are read from this asset at BeginPlay — never hardcoded.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Config")
    TSoftObjectPtr<UWTBRCoreStatsDataAsset> CoreStatsAsset;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHPChanged OnHPChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnLimbDestroyed OnLimbDestroyed;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDeath OnDeath;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDowned OnDowned;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCombatStateChanged OnCombatStateChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnInvulnerabilityChanged OnInvulnerabilityChanged;

    UFUNCTION(BlueprintCallable, Category="Health")
    void ApplyDamage(float DamageAmount, AActor* DamageInstigator = nullptr);

    // Call from respawn, revive, or round reset before this character can receive rewards again.
    UFUNCTION(BlueprintCallable, Category="Health")
    void ResetCombatRewardState();

    UFUNCTION(BlueprintCallable, Category="Health")
    void DestroyLimb(EWTBRLimbType Limb);

    UFUNCTION(BlueprintCallable, Category="Health")
    void RestoreLimb(EWTBRLimbType Limb);

    UFUNCTION(BlueprintPure, Category="Health")
    float GetCurrentHP() const { return CurrentHP; }

    UFUNCTION(BlueprintPure, Category="Health")
    float GetMaxHP() const;

    UFUNCTION(BlueprintPure, Category="Health")
    bool IsAlive() const { return CurrentCombatState == EWTBRCombatState::Alive; }

    UFUNCTION(BlueprintPure, Category="Health")
    bool IsDowned() const { return CurrentCombatState == EWTBRCombatState::Downed; }

    UFUNCTION(BlueprintPure, Category="Health")
    bool IsEliminated() const { return CurrentCombatState == EWTBRCombatState::Eliminated; }

    UFUNCTION(BlueprintPure, Category="Health")
    bool IsInvulnerable() const { return bIsInvulnerable; }

    UFUNCTION(BlueprintPure, Category="Health")
    EWTBRCombatState GetCombatState() const { return CurrentCombatState; }

    UFUNCTION(BlueprintPure, Category="Health")
    float GetCurrentDownedHP() const { return CurrentDownedHP; }

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

    UPROPERTY(ReplicatedUsing=OnRep_CurrentHP)
    float CurrentHP = 300.f;

    UPROPERTY(ReplicatedUsing=OnRep_CombatState)
    EWTBRCombatState CurrentCombatState = EWTBRCombatState::Alive;

    UPROPERTY(ReplicatedUsing=OnRep_IsInvulnerable)
    bool bIsInvulnerable = false;

    UPROPERTY(Replicated)
    float CurrentDownedHP = 0.f;

    FTimerHandle LimbDrainTimerHandle;
    FTimerHandle KnockdownIFrameTimerHandle;
    TArray<FWTBRDamageContributionRecord> DamageHistory;
    bool bDownRewardResolved = false;
    bool bDeathRewardsResolved = false;

    const UWTBRCoreStatsDataAsset* GetStats() const
    {
        ensure(CoreStatsAsset.IsValid());
        return CoreStatsAsset.Get();
    }

    // Starts (or keeps) the drain timer running; called whenever a limb is destroyed
    void RefreshLimbDrainTimer();

    // Timer callback — fires every LIMB_DRAIN_TICK_INTERVAL seconds on Authority
    void TickLimbDrain();

    AWTBRCharacter* ResolveContributorCharacter(AActor* DamageInstigator) const;
    AController* ResolveContributorController(AWTBRCharacter* ContributorCharacter) const;
    void RecordDamageContribution(float DamageAmount, AActor* DamageInstigator);
    void PruneDamageHistory(float CurrentTime, float AssistWindow);
    void ResolveDeathRewards(AActor* FinalDamageInstigator);
    void ResolveDownReward(AActor* DownInstigator);
    void GrantVaelReward(AWTBRCharacter* RecipientCharacter, float Amount) const;
    void ClearDamageHistory();
    void EnterDownedState(AActor* DownInstigator);
    void EnterEliminatedState(AActor* FinalDamageInstigator);
    void SetCombatState(EWTBRCombatState NewState);
    void SetInvulnerable(bool bNewInvulnerable);
    void StartKnockdownIFrame();
    void EndKnockdownIFrame();

    UFUNCTION()
    void OnRep_CurrentHP();

    UFUNCTION()
    void OnRep_CombatState(EWTBRCombatState OldState);

    UFUNCTION()
    void OnRep_IsInvulnerable();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
