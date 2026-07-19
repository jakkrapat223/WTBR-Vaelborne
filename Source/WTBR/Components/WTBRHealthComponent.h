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

// Damage-multiplier classification only (headshot-style bonus/penalty) — a
// separate concept from EWTBRLimbType above, which drives the (currently
// dormant, no mode enables it) Limb Destruction disable-function/HP-drain
// system. This codebase has no per-bone hit detection (character collision
// is a single capsule, no physics asset), so classification is a pure
// geometry approximation — see AWTBRProjectileBase::ClassifyHitZone.
UENUM(BlueprintType)
enum class EWTBRHitZone : uint8
{
    Torso UMETA(DisplayName="Torso"),
    Head  UMETA(DisplayName="Head"),
    Arm   UMETA(DisplayName="Arm"),
    Leg   UMETA(DisplayName="Leg"),
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
// Fires (server + clients) whenever the "is a teammate actively reviving me" flag flips.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeingRevivedChanged, bool, bBeingRevived);

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

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnBeingRevivedChanged OnBeingRevivedChanged;

    UFUNCTION(BlueprintCallable, Category="Health")
    void ApplyDamage(float DamageAmount, AActor* DamageInstigator = nullptr);

    // Feryx Star's damage-over-time effect. Reapplying refreshes the duration;
    // it never creates parallel stacks of timers on the same target.
    UFUNCTION(BlueprintCallable, Category="Health | Status")
    void ApplyBleed(float DamagePerTick, float Duration, AActor* DamageInstigator = nullptr);

    // Call from respawn, revive, or round reset before this character can receive rewards again.
    UFUNCTION(BlueprintCallable, Category="Health")
    void ResetCombatRewardState();

    UFUNCTION(BlueprintCallable, Category="Health")
    void DestroyLimb(EWTBRLimbType Limb);

    UFUNCTION(BlueprintCallable, Category="Health")
    void RestoreLimb(EWTBRLimbType Limb);

    // Server-authoritative HP restore for consumables (S5-D). Alive-only: never
    // revives Downed/Eliminated and never touches limb or downed HP. Clamps to
    // MaxHP (no overheal). Returns true only if HP actually increased. Caller is
    // expected to be server-side.
    UFUNCTION(BlueprintCallable, Category="Health")
    bool RestoreHP(float Amount);

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

    // Server-only full-damage immunity used by an active dodge. This is kept
    // here so every gameplay damage source passes through one authoritative
    // gate rather than each weapon needing special-case dodge logic.
    void StartDodgeIFrame(float Duration);

    // ── Revive (server-authoritative; teammate-driven) ───────────────────────

    // Begin a revive on this (Downed) combatant by Reviver. Server-only. Rejects
    // unless: this component is Downed, no revive is already in progress, and
    // Reviver is a valid, Alive character on the SAME team (team-less pairs are
    // never teammates). While a revive is in progress the bleed-out countdown is
    // PAUSED. Returns true if the revive actually started.
    UFUNCTION(BlueprintCallable, Category="Revive")
    bool TryStartRevive(AWTBRCharacter* Reviver);

    // Stop an in-progress revive (reviver released the button, moved away, was
    // downed, etc.). Server-only. Resumes the bleed-out countdown from its
    // remaining value — it does NOT reset. No-op if no revive is in progress.
    UFUNCTION(BlueprintCallable, Category="Revive")
    void StopRevive();

    UFUNCTION(BlueprintPure, Category="Revive")
    bool IsBeingRevived() const { return bReviveInProgress; }

    UFUNCTION(BlueprintPure, Category="Revive")
    AWTBRCharacter* GetActiveReviver() const { return CurrentReviver.Get(); }

    UFUNCTION(BlueprintPure, Category="Health")
    EWTBRCombatState GetCombatState() const { return CurrentCombatState; }

    UFUNCTION(BlueprintPure, Category="Health")
    float GetCurrentDownedHP() const { return CurrentDownedHP; }

    UFUNCTION(BlueprintPure, Category="Health")
    FWTBRLimbState GetLimbState(EWTBRLimbType Limb) const;

    // Multiplicative total — call this to feed into WTBRMovementExtComponent
    float GetTotalSpeedPenaltyFromLimbs() const;
    float GetTotalDamagePenaltyFromLimbs() const;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: invoke the eliminated-character death-drop routine directly on an
    // authority owner, without driving the full damage/downed/elimination pipeline
    // (which is timer/CoreStats-asset dependent and not headless-safe). Mirrors the
    // UWTBRTriggerSetComponent::SetSlotDataAssetForTest seam. Behaviour-neutral in
    // shipping (compiled out); still runs the same server-side drop logic.
    void SpawnDroppedTriggersForEliminatedCharacterForTest() { SpawnDroppedTriggersForEliminatedCharacter(); }

    // Test-only seams for bleed-out + revive: real FTimerManager timers do not
    // fire in the headless transient-world fixtures, so drive the callbacks
    // directly. These run the exact production handlers (compiled out of shipping).
    void ForceBleedOutExpiryForTest() { OnBleedOutExpired(); }
    void ForceReviveCompleteForTest() { CompleteRevive(); }
    bool IsBleedOutTimerActiveForTest() const;
    bool IsBleedOutTimerPausedForTest() const;
    // True whenever the timer handle is set (Active OR Paused) — unlike
    // IsBleedOutTimerActiveForTest(), which UE's FTimerManager defines as false
    // while Paused. Use this to assert the countdown survives a pause instead of
    // being cleared.
    bool IsBleedOutTimerSetForTest() const;
    AActor* GetLastDamageInstigatorForTest() const { return LastDamageInstigator.Get(); }
#endif

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

    // Replicated so clients can show a "being revived" indicator on downed allies.
    UPROPERTY(ReplicatedUsing=OnRep_bReviveInProgress)
    bool bReviveInProgress = false;

    FTimerHandle LimbDrainTimerHandle;
    FTimerHandle KnockdownIFrameTimerHandle;
    FTimerHandle DodgeIFrameTimerHandle;
    FTimerHandle BleedOutTimerHandle;
    FTimerHandle ReviveTimerHandle;
    FTimerHandle BleedStatusTimerHandle;
    FTimerHandle BleedStatusExpiryTimerHandle;
    float BleedDamagePerTick = 0.0f;
    TWeakObjectPtr<AActor> BleedDamageInstigator;
    TArray<FWTBRDamageContributionRecord> DamageHistory;
    // The most recent actor to deal actual damage to this component. Drives the
    // kill credit when a downed combatant bleeds out with no finishing blow —
    // design lock 2026-07-13: the LAST DAMAGER earns the point.
    TWeakObjectPtr<AActor> LastDamageInstigator;
    // The teammate currently holding a revive on this component (server-side).
    TWeakObjectPtr<AWTBRCharacter> CurrentReviver;
    bool bDownRewardResolved = false;
    bool bDeathRewardsResolved = false;

    const UWTBRCoreStatsDataAsset* GetStats() const
    {
        if (CoreStatsAsset.IsNull())
        {
            return nullptr;
        }

        return CoreStatsAsset.LoadSynchronous();
    }

    // Starts (or keeps) the drain timer running; called whenever a limb is destroyed
    void RefreshLimbDrainTimer();

    // Timer callback — fires every LIMB_DRAIN_TICK_INTERVAL seconds on Authority
    void TickLimbDrain();

    AWTBRCharacter* ResolveContributorCharacter(AActor* DamageInstigator) const;
    AController* ResolveContributorController(AWTBRCharacter* ContributorCharacter) const;
    void RecordDamageContribution(float DamageAmount, AActor* DamageInstigator);
    void ResolveDeathRewards(AActor* FinalDamageInstigator);
    void ResolveDownReward(AActor* DownInstigator);
    void GrantVaelReward(AWTBRCharacter* RecipientCharacter, float Amount) const;
    void SpawnDroppedTriggersForEliminatedCharacter();
    void SpawnLegacyDroppedTriggers_Internal();
    void SpawnCorpseLootContainer_Internal();
    void ClearDamageHistory(const TCHAR* Source);
    void EnterDownedState(AActor* DownInstigator);
    void EnterEliminatedState(AActor* FinalDamageInstigator);
    void SetCombatState(EWTBRCombatState NewState);
    void SetInvulnerable(bool bNewInvulnerable);
    void StartKnockdownIFrame();
    void EndKnockdownIFrame();
    void EndDodgeIFrame();

    // Bleed-out: starts on EnterDownedState (authority, requires CoreStats). On
    // expiry the combatant is Eliminated crediting LastDamageInstigator.
    void StartBleedOutTimer();
    void OnBleedOutExpired();
    void ClearBleedOutTimer();

    // Revive completion (authority). Restores the combatant to Alive with
    // ReviveHPRestored, clears bleed-out + revive state, re-arms the down reward.
    void CompleteRevive();
    // Shared teardown for both StopRevive and CompleteRevive.
    void ClearReviveState(bool bResumeBleedOut);
    void TickBleedStatus();
    void ClearBleedStatus();
    void SetReviveInProgress(bool bNewInProgress);

    UFUNCTION()
    void OnRep_CurrentHP();

    UFUNCTION()
    void OnRep_CombatState(EWTBRCombatState OldState);

    UFUNCTION()
    void OnRep_IsInvulnerable();

    UFUNCTION()
    void OnRep_bReviveInProgress();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
