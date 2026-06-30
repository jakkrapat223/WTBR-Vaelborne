// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/WTBRMatchModeRulesDataAsset.h"
#include "WTBRGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FWTBRMatchModeRulesChanged,
    EWTBRMatchMode, MatchMode,
    const FWTBRMatchModeRules&, Rules);

UCLASS()
class WTBR_API AWTBRGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AWTBRGameState();

    UFUNCTION(BlueprintCallable, Category="WTBR | Match Mode")
    void SetCurrentMatchRules(const FWTBRMatchModeRules& NewRules);

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode")
    EWTBRMatchMode GetCurrentMatchMode() const { return CurrentMatchMode; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode")
    FWTBRMatchModeRules GetCurrentMatchRules() const { return CurrentMatchRules; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Economy")
    bool IsPassiveVaelRegenEnabled() const { return CurrentMatchRules.bEnablePassiveVaelRegen; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Economy")
    float GetVaelRegenPerSecond() const { return CurrentMatchRules.VaelRegenPerSecond; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loot")
    bool AllowsCorpseLoot() const { return CurrentMatchRules.bAllowCorpseLoot; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loot")
    bool AllowsTriggerPickup() const { return CurrentMatchRules.bAllowTriggerPickup; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loadout")
    bool AllowsTriggerSwapDuringMatch() const { return CurrentMatchRules.bAllowTriggerSwapDuringMatch; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Loadout")
    bool AllowsLoadoutEditDuringMatch() const { return CurrentMatchRules.bAllowLoadoutEditDuringMatch; }

    UFUNCTION(BlueprintPure, Category="WTBR | Match Mode | Teams")
    bool UsesTeams() const { return CurrentMatchRules.bUseTeams; }

    UPROPERTY(BlueprintAssignable, Category="WTBR | Match Mode")
    FWTBRMatchModeRulesChanged OnMatchModeRulesChanged;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_CurrentMatchMode, BlueprintReadOnly, Category="WTBR | Match Mode")
    EWTBRMatchMode CurrentMatchMode = EWTBRMatchMode::ThreeVThree;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentMatchRules, BlueprintReadOnly, Category="WTBR | Match Mode")
    FWTBRMatchModeRules CurrentMatchRules;

    UFUNCTION()
    void OnRep_CurrentMatchMode();

    UFUNCTION()
    void OnRep_CurrentMatchRules();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    void BroadcastMatchModeRulesChanged();
};
