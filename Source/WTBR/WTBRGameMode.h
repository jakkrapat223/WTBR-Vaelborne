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
	//
	// Team mode (any alive combatant has an assigned TeamId): FinalDamageInstigator
	// earns their team 1 kill point (enemy kills only — team kills, self-kills and
	// environment deaths score nothing), the round instead ends when at most one
	// TEAM has a living member, and the winner comes from ResolveTeamRoundIfOver.
	void NotifyCombatantEliminated(AWTBRCharacter* EliminatedCharacter, AActor* FinalDamageInstigator = nullptr);

	UFUNCTION(Exec)
	void WTBRDebugSetMatchPhase(const FString& PhaseName);

	// Phase 7E: minimal PostMatch restart for 1v1 testing. Server-authoritative,
	// console/debug command only (no UI button, no new input mapping). Only acts
	// when the current phase is PostMatch (no-op + warning log otherwise). Clears
	// the match winner, resets every AWTBRCharacter's HP/combat-state (via the
	// same HealthComponent::ResetCombatRewardState() the round loop already uses)
	// and Vael/desperation state, then re-enters the existing, already-tested
	// LoadoutSetup -> Countdown -> InMatch auto-advance. Does not touch limb
	// destruction state, corpse loot, or BagLoot.
	UFUNCTION(Exec)
	void WTBRRestartRound();

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

	// Block-fills every AWTBRCharacter in the world into teams of Rules.TeamSize
	// (spawn-iteration order: first TeamSize characters -> team 0, next -> team 1,
	// ...). Runs at InMatch entry, only when Rules.bAssignTeamsAtMatchStart; other
	// modes leave TeamId untouched (INDEX_NONE) so the individual round loop and
	// the 1v1 harness behave exactly as before.
	void AssignTeamsForRound(const FWTBRMatchModeRules& Rules);

	// Team-mode round resolution (bUseTeams + at least one assigned TeamId among
	// the alive set). Awards nothing by itself — kill points are added as
	// eliminations stream in; this only decides END + WINNER: when at most one
	// team has a non-eliminated member, the survivors' team receives +1 per living
	// member, then the winner is the highest total score if
	// Rules.bScoreBasedTeamWinner (TeamThree15P — a wiped team can win on kills),
	// else simply the surviving team (BR). Ties resolve to the surviving team if
	// it is among the leaders, otherwise a draw (WinningTeamId stays INDEX_NONE).
	void ResolveTeamRoundIfOver(AWTBRGameState& WTBRGameState, const TArray<AWTBRCharacter*>& AliveCombatants);

	// Shared by BeginPlay and WTBRRestartRound: enters LoadoutSetup, resets the
	// per-round result-resolved flag, and (re)starts the LoadoutSetup -> Countdown
	// timer. Does not touch match rules — callers that need a fresh rules reset
	// (BeginPlay only) apply that separately before calling this.
	void BeginLoadoutSetupCountdown();

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
	// Exposes the real per-mode default rules generator so tests can lock the
	// production defaults (e.g. TeamThree15P must ship with corpse loot off)
	// instead of hand-rolling a rules struct that could drift from production.
	static FWTBRMatchModeRules MakeDefaultRulesForModeForTest(EWTBRMatchMode MatchMode) { return MakeDefaultRulesForMode(MatchMode); }
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



