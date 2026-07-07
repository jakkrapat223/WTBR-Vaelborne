// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/WTBRMatchModeRulesDataAsset.h"
#include "WTBRGameState.generated.h"

class APlayerState;

UENUM(BlueprintType)
enum class EWTBRMatchPhase : uint8
{
    None UMETA(DisplayName="None"),
    Lobby UMETA(DisplayName="Lobby"),
    PreMatch UMETA(DisplayName="Pre Match"),
    LoadoutSetup UMETA(DisplayName="Loadout Setup"),
    Countdown UMETA(DisplayName="Countdown"),
    InMatch UMETA(DisplayName="In Match"),
    PostMatch UMETA(DisplayName="Post Match"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FWTBRMatchModeRulesChanged,
    EWTBRMatchMode, MatchMode,
    const FWTBRMatchModeRules&, Rules);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FWTBRMatchPhaseChanged,
    EWTBRMatchPhase, MatchPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FWTBRMatchWinnerChanged,
    APlayerState*, WinnerPlayerState);

UCLASS()
class WTBR_API AWTBRGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AWTBRGameState();

    UFUNCTION(BlueprintCallable, Category="WTBR | Match Mode")
    void SetCurrentMatchRules(const FWTBRMatchModeRules& NewRules);

    UFUNCTION(BlueprintCallable, Category="WTBR | Match Phase")
    void SetCurrentMatchPhase(EWTBRMatchPhase NewPhase);

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode")
    EWTBRMatchMode GetCurrentMatchMode() const { return CurrentMatchMode; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode")
    FWTBRMatchModeRules GetCurrentMatchRules() const { return CurrentMatchRules; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Phase")
    EWTBRMatchPhase GetCurrentMatchPhase() const { return CurrentMatchPhase; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Phase")
    bool IsInMatch() const { return CurrentMatchPhase == EWTBRMatchPhase::InMatch; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Phase")
    bool IsPreMatchPhase() const
    {
        return CurrentMatchPhase == EWTBRMatchPhase::Lobby
            || CurrentMatchPhase == EWTBRMatchPhase::PreMatch
            || CurrentMatchPhase == EWTBRMatchPhase::LoadoutSetup
            || CurrentMatchPhase == EWTBRMatchPhase::Countdown;
    }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Phase")
    bool IsLoadoutSetupPhase() const { return CurrentMatchPhase == EWTBRMatchPhase::LoadoutSetup; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Phase")
    bool IsLoadoutSetupAllowedPhase() const
    {
        return CurrentMatchPhase == EWTBRMatchPhase::Lobby
            || CurrentMatchPhase == EWTBRMatchPhase::PreMatch
            || CurrentMatchPhase == EWTBRMatchPhase::LoadoutSetup;
    }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Economy")
    bool IsPassiveVaelRegenEnabled() const { return CurrentMatchRules.bEnablePassiveVaelRegen; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Economy")
    float GetVaelRegenPerSecond() const { return CurrentMatchRules.VaelRegenPerSecond; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loot")
    bool AllowsCorpseLoot() const { return CurrentMatchRules.bAllowCorpseLoot; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loot")
    bool AllowsTriggerPickup() const { return CurrentMatchRules.bAllowTriggerPickup; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loot")
    bool UsesCorpseLootContainerOnDeath() const { return CurrentMatchRules.bUseCorpseLootContainerOnDeath; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loadout")
    bool AllowsTriggerSwapDuringMatch() const { return CurrentMatchRules.bAllowTriggerSwapDuringMatch; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loadout")
    bool AllowsLoadoutEditDuringMatch() const { return CurrentMatchRules.bAllowLoadoutEditDuringMatch; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Teams")
    bool UsesTeams() const { return CurrentMatchRules.bUseTeams; }

    // Phase 7B: server-authoritative round result. Set by AWTBRGameMode once the
    // round-loop winner check resolves (at most one non-eliminated combatant
    // remaining). Null winner with Phase==PostMatch means a draw (e.g. simultaneous
    // elimination). HUD/UI can bind OnMatchWinnerChanged and read GetMatchWinner()
    // once replicated; this struct/state does not itself grant rewards.
    UFUNCTION(BlueprintCallable, Category="WTBR | Match Result")
    void SetMatchWinner(APlayerState* WinnerPlayerState);

    UFUNCTION(BlueprintPure, Category="WTBR | Match Result")
    APlayerState* GetMatchWinner() const { return MatchWinnerPlayerState; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Result")
    bool HasMatchWinner() const { return MatchWinnerPlayerState != nullptr; }

    UPROPERTY(BlueprintAssignable, Category="WTBR | Match Mode")
    FWTBRMatchModeRulesChanged OnMatchModeRulesChanged;

    UPROPERTY(BlueprintAssignable, Category="WTBR | Match Phase")
    FWTBRMatchPhaseChanged OnMatchPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category="WTBR | Match Result")
    FWTBRMatchWinnerChanged OnMatchWinnerChanged;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_CurrentMatchMode, BlueprintReadOnly, Category="WTBR | Match Mode")
    EWTBRMatchMode CurrentMatchMode = EWTBRMatchMode::ThreeVThree;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentMatchRules, BlueprintReadOnly, Category="WTBR | Match Mode")
    FWTBRMatchModeRules CurrentMatchRules;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentMatchPhase, BlueprintReadOnly, Category="WTBR | Match Phase")
    EWTBRMatchPhase CurrentMatchPhase = EWTBRMatchPhase::None;

    UPROPERTY(ReplicatedUsing=OnRep_MatchWinnerPlayerState, BlueprintReadOnly, Category="WTBR | Match Result")
    TObjectPtr<APlayerState> MatchWinnerPlayerState = nullptr;

    UFUNCTION()
    void OnRep_CurrentMatchMode();

    UFUNCTION()
    void OnRep_CurrentMatchRules();

    UFUNCTION()
    void OnRep_CurrentMatchPhase();

    UFUNCTION()
    void OnRep_MatchWinnerPlayerState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void BroadcastMatchModeRulesChanged();
    void BroadcastMatchPhaseChanged();
    void BroadcastMatchWinnerChanged();
};
