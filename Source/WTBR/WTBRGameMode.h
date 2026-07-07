// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/WTBRMatchModeRulesDataAsset.h"
#include "GameFramework/GameModeBase.h"
#include "WTBRGameState.h"
#include "WTBRGameMode.generated.h"

class AWTBRCorpseLootContainerActor;
class AWTBRCharacter;

UCLASS(minimalapi)
class AWTBRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWTBRGameMode();

	// Phase 7B: called by UWTBRHealthComponent (server-authoritative) the moment a
	// combatant enters the Eliminated combat state. Counts remaining non-eliminated
	// AWTBRCharacter pawns in the world; when at most one remains, records the
	// winner (or a draw, on simultaneous elimination) on AWTBRGameState and
	// advances MatchPhase to PostMatch. No-ops outside InMatch and once the result
	// has already been resolved for the current round (idempotent).
	void NotifyCombatantEliminated(AWTBRCharacter* EliminatedCharacter);

	UFUNCTION(Exec)
	void WTBRDebugSetMatchPhase(const FString& PhaseName);

	UFUNCTION(Exec)
	void WTBRDebugPrintMatchState() const;

	UFUNCTION(Exec)
	void WTBRDebugPrintTriggerLoadoutGate() const;

	// B7 validation harness: spawn a corpse loot container with synthetic entries.
	// Dev/test-only (disabled in Shipping). Safe to call before or after players
	// join. Pass DelaySeconds > 0 to schedule via timer so the pawn is available
	// when the spawn fires (useful with -ExecCmds). Does not change production
	// defaults or normal gameplay behavior.
	UFUNCTION(Exec)
	void WTBRDebugServerSpawnCorpseLootContainerForValidation(float DelaySeconds = 0.0f);

	// B7 validation harness: print all AWTBRCorpseLootContainerActor in the world
	// with actor name, location, authority state, entry count, and available loot.
	// Dev/test-only (disabled in Shipping).
	UFUNCTION(Exec)
	void WTBRDebugServerPrintCorpseLootContainers() const;

	TSubclassOf<AWTBRCorpseLootContainerActor> GetCorpseLootContainerClass() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
	EWTBRMatchMode DefaultMatchMode = EWTBRMatchMode::ThreeVThree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
	FWTBRMatchModeRules DefaultMatchRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
	TArray<TObjectPtr<UWTBRMatchModeRulesDataAsset>> MatchModeRuleAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Corpse Loot")
	TSubclassOf<AWTBRCorpseLootContainerActor> CorpseLootContainerClass;

	// Phase 7B: minimal real combat round loop. BeginPlay enters LoadoutSetup, then
	// auto-advances Countdown -> InMatch on these timers with no console command
	// required. Fixed loadout only (no loadout UI this phase) — LoadoutSetupDuration
	// is a short grace window, not a loadout-editing phase.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Flow", meta=(ClampMin="0.0"))
	float LoadoutSetupDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Flow", meta=(ClampMin="0.0"))
	float CountdownDuration = 3.0f;

private:
	FWTBRMatchModeRules ResolveDefaultMatchRules() const;
	static FWTBRMatchModeRules MakeDefaultRulesForMode(EWTBRMatchMode MatchMode);
	static bool TryParseDebugMatchPhase(const FString& PhaseName, EWTBRMatchPhase& OutPhase);

	void AdvanceToCountdown();
	void AdvanceToInMatch();
	static void CollectAliveCombatants(UWorld* World, TArray<AWTBRCharacter*>& OutAlive);

	FTimerHandle LoadoutSetupTimerHandle;
	FTimerHandle CountdownTimerHandle;
	bool bMatchResultResolved = false;

#if WITH_DEV_AUTOMATION_TESTS
public:
	// Test-only seams: drive the phase-advance transitions directly, without
	// waiting on real timers, so automation can prove the transition logic
	// (GameState phase set, HP reset, bMatchResultResolved reset) headlessly.
	void AdvanceToCountdownForTest() { AdvanceToCountdown(); }
	void AdvanceToInMatchForTest() { AdvanceToInMatch(); }
	bool IsMatchResultResolvedForTest() const { return bMatchResultResolved; }
private:
#endif

#if !UE_BUILD_SHIPPING
	FTimerHandle ValidationSpawnTimerHandle;
	FTimerHandle ValidationCVarPollTimerHandle;
	int32 ValidationSpawnRetryCount = 0;
	// 90 retries × 2 s = 180 s (3 min). Covers the window between the scheduled
	// spawn attempt and late-joining clients in the commandlet validation setup.
	static constexpr int32 ValidationSpawnMaxRetries = 90;
	void PollValidationSpawnCVar();
	void SpawnCorpseLootContainerForValidation_Authority();
#endif
};



