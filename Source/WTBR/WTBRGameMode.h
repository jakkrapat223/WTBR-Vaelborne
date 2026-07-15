// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/WTBRMatchModeRulesDataAsset.h"
#include "Data/WTBRRandomSpawnConfigDataAsset.h"
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

	// Wait-for-players (design gap flagged 2026-07-13): host/dev override to skip
	// the remaining Lobby wait and start immediately, even under
	// MinPlayersToStart. Only valid from Lobby (no-op + warning otherwise).
	// BlueprintCallable so a future "Start Match" UI button can call it directly.
	UFUNCTION(Exec, BlueprintCallable, Category="WTBR | Match Flow")
	void WTBRForceStartMatch();

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

	// Wait-for-players (design gap flagged 2026-07-13): BeginPlay now enters a
	// real Lobby phase and holds there — polling GetNumPlayers() — until at
	// least MinPlayersToStart PlayerControllers are connected, instead of
	// jumping straight into LoadoutSetup with whoever (if anyone) happened to be
	// connected at that exact instant. Default 1 preserves existing solo/PIE/
	// 1v1-harness behavior (starts on the next ~1s poll after the local player
	// connects); a 15P/BR deployment should raise this via a level-specific
	// GameMode Blueprint/World Settings override. A host/dev can always skip the
	// rest of the wait via WTBRForceStartMatch. WTBRRestartRound is unaffected —
	// a round restart skips Lobby entirely (players are already connected).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Flow | Lobby", meta=(ClampMin="1"))
	int32 MinPlayersToStart = 1;

	// Safety valve: 0 = wait indefinitely for MinPlayersToStart. >0 = force-start
	// after this many seconds even if still under MinPlayersToStart, so a match
	// can never hang forever short a few players. Placeholder; TBD via playtest
	// once real 15P/BR player counts and matchmaking are in place.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Flow | Lobby", meta=(ClampMin="0.0"))
	float LobbyMaxWaitSeconds = 0.0f;

	// Random spawn (design lock 2026-07-13, TeamThree15P/BR): per-LEVEL spawn
	// area, only used when the active match rules have bUseRandomSpawnPositions.
	// A DataAsset (not raw UPROPERTY floats) so each level can get its own
	// config without a C++ recompile or a level-specific GameMode Blueprint
	// subclass — assign the DA that matches this level's actual floor size.
	// If left unset, ApplyRandomSpawnPositions skips random spawn entirely
	// (falls back to normal PlayerStart placement) rather than guessing
	// map-inappropriate defaults. Real map sizes are ~1x1 km (15P) / ~3x3 km
	// (BR) per the mode design lock; exact numbers are TBD via playtest on a
	// real (non-graybox) map — the shipped default DA values are sized for the
	// stock ThirdPersonMap graybox floor only.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Flow | Spawn")
	TObjectPtr<UWTBRRandomSpawnConfigDataAsset> RandomSpawnConfig;

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

	// Team-mode round resolution triggered by an elimination (bUseTeams + at
	// least one assigned TeamId among the alive set). Ends the round only when
	// at most one team has a non-eliminated member; otherwise the match
	// continues. See ApplyTeamRoundEndScoringAndWinner for the scoring/winner
	// logic shared with the time-limit path.
	void ResolveTeamRoundIfOver(AWTBRGameState& WTBRGameState, const TArray<AWTBRCharacter*>& AliveCombatants);

	// Builds TeamId -> living-member-count from AliveCombatants. Characters with
	// no assigned TeamId are logged and excluded (should not happen — assignment
	// covers everyone). OutTeamlessAliveCount is set to how many were excluded.
	static TMap<int32, int32> BuildAliveCountByTeam(const TArray<AWTBRCharacter*>& AliveCombatants, int32& OutTeamlessAliveCount);

	// Shared team-round-end scoring: every team present in AliveCountByTeam gets
	// +1 survivor point per living member (generalizes the old
	// single-surviving-team bonus to however many teams are alive when the round
	// ends — natural last-team-standing has exactly one, a time-limit end can
	// have several). Winner: Rules.bScoreBasedTeamWinner picks the highest total
	// score (a wiped team can still win on kill points); ties break to whichever
	// tied leader has the most living members right now, and remain a draw
	// (WinningTeamId INDEX_NONE) if that's still tied. Non-score-based modes
	// (BR) simply win with the sole alive team, drawing if more than one remains.
	void ApplyTeamRoundEndScoringAndWinner(AWTBRGameState& WTBRGameState, const TMap<int32, int32>& AliveCountByTeam);

	// Match time limit (design lock 2026-07-13, TeamThree15P): started at
	// InMatch entry when Rules.MatchTimeLimitSeconds > 0; force-ends the round
	// via ApplyTeamRoundEndScoringAndWinner even with multiple teams still
	// alive. No-op for modes with no time limit set (0, incl. BR/1v1 today).
	void StartMatchTimeLimitTimer(const FWTBRMatchModeRules& Rules);
	void ClearMatchTimeLimitTimer();
	void OnMatchTimeLimitExpired();

	// Generates Count points uniformly inside a horizontal disc (Center, AreaRadius)
	// via rejection sampling, trying to keep every pair at least MinDistance apart.
	// Always returns exactly Count points (never fails/hangs): when the area is too
	// small to satisfy MinDistance for every point, later points fall back to
	// whichever sampled candidate had the largest achieved minimum distance. Z is
	// fixed at Center.Z — this is a pure, World-independent function (kept
	// deterministic/testable via GenerateRandomSpawnPointsForTest); ground-height
	// resolution happens afterward in ApplyRandomSpawnPositions, which has a World
	// to trace against.
	static TArray<FVector> GenerateRandomSpawnPoints(const FVector& Center, float AreaRadius, float MinDistance, int32 Count, FRandomStream& RandomStream);

	static bool TryGenerateSafeAnchorLayout(const TArray<FVector>& Anchors, float MinDistance, int32 Count, FRandomStream& RandomStream, TArray<FVector>& OutPoints);

	// Teleports every AWTBRCharacter in the world to a freshly generated random
	// spawn point (see GenerateRandomSpawnPoints), snapped down onto the ground via
	// a line trace so points on sloped/uneven terrain (or a mismatched spawn-area
	// config) don't drop characters into the void. Only called when the active
	// match rules have bUseRandomSpawnPositions; a no-op otherwise so legacy modes
	// and the 1v1 harness keep using normal PlayerStart placement.
	void ApplyRandomSpawnPositions();

	// Ground-snap helper for ApplyRandomSpawnPositions: traces straight down from
	// well above InPoint and returns the impact location + a small clearance, or
	// InPoint unchanged (with a warning log) if the trace finds no ground within
	// range — e.g. the configured spawn area extends outside the level's floor.
	// All combatants are ignored, rather than only the character being placed:
	// at match start they can still be stacked at a PlayerStart, and no pawn
	// capsule may be mistaken for the map floor by this WorldStatic trace.
	static FVector SnapSpawnPointToGround(UWorld* World, const FVector& InPoint, const TArray<AWTBRCharacter*>& IgnoredCombatants);
	static bool GetNavigableSpawnLocation(UWorld* World, const FVector& GroundedPoint, FVector& OutSpawnLocation);

	// Called only by BeginLoadoutSetupCountdown's callers (BeginPlay via
	// EnterLobbyAndWaitForPlayers, WTBRRestartRound directly): enters LoadoutSetup,
	// resets the per-round result-resolved flag, and (re)starts the
	// LoadoutSetup -> Countdown timer. Does not touch match rules — callers that
	// need a fresh rules reset (BeginPlay only) apply that separately before
	// calling this.
	void BeginLoadoutSetupCountdown();

	// Enters Lobby and holds until IsLobbyReadyToStart(), polling every
	// LobbyPollIntervalSeconds, then calls BeginLoadoutSetupCountdown(). Checks
	// immediately on entry too, so the common MinPlayersToStart=1 case doesn't
	// wait a full poll interval if a player is already connected.
	void EnterLobbyAndWaitForPlayers();
	void PollLobbyReadyToStart();
	bool IsLobbyReadyToStart() const;

	// Counts live APlayerController actors in World. Deliberately NOT
	// AGameModeBase::GetNumPlayers() (reads UWorld::PlayerControllerList,
	// populated only by the real network login/join flow) and deliberately NOT
	// gated on PlayerState (AController::InitPlayerState() itself requires
	// World->GetAuthGameMode() to already be set via the full URL-based
	// UWorld::SetGameMode() flow — never true in a headless fixture that merely
	// SpawnActor's a GameMode, so PlayerState never gets created there either).
	// Plain actor-counting matches every real connected player today (this
	// project has no spectator mode yet — revisit alongside IsOnlyASpectator()
	// if one is ever added) and is verifiable headlessly with a bare
	// SpawnActor<APlayerController>().
	static int32 CountConnectedPlayers(UWorld* World);

	static constexpr float LobbyPollIntervalSeconds = 1.0f;

	FTimerHandle LoadoutSetupTimerHandle;
	FTimerHandle CountdownTimerHandle;
	FTimerHandle MatchTimeLimitTimerHandle;
	FTimerHandle LobbyPollTimerHandle;
	float LobbyEnteredWorldTimeSeconds = 0.0f;
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
	// Drives the match-time-limit expiry handler directly — real FTimerManager
	// timers don't fire in the headless transient-world fixtures.
	void ForceMatchTimeLimitExpiredForTest() { OnMatchTimeLimitExpired(); }
	bool IsMatchTimeLimitTimerActiveForTest() const { return GetWorldTimerManager().IsTimerActive(MatchTimeLimitTimerHandle); }
	// Exposes the pure spawn-point algorithm (deterministic via an explicit seed)
	// so tests can assert its spacing/count guarantees without a GameMode/world.
	static TArray<FVector> GenerateRandomSpawnPointsForTest(const FVector& Center, float AreaRadius, float MinDistance, int32 Count, int32 Seed)
	{
		FRandomStream Stream(Seed);
		return GenerateRandomSpawnPoints(Center, AreaRadius, MinDistance, Count, Stream);
	}
	static bool TryGenerateSafeAnchorLayoutForTest(const TArray<FVector>& Anchors, float MinDistance, int32 Count, int32 Seed, TArray<FVector>& OutPoints)
	{
		FRandomStream Stream(Seed);
		return TryGenerateSafeAnchorLayout(Anchors, MinDistance, Count, Stream, OutPoints);
	}
	void ApplyRandomSpawnPositionsForTest() { ApplyRandomSpawnPositions(); }
	// Builds a transient config DA and assigns it to RandomSpawnConfig — tests
	// exercise the real DA-driven production path, not a bypass of it.
	void SetRandomSpawnParamsForTest(const FVector& Center, float AreaRadius, float MinDistance)
	{
		UWTBRRandomSpawnConfigDataAsset* Config = NewObject<UWTBRRandomSpawnConfigDataAsset>(this);
		Config->SpawnAreaCenter = Center;
		Config->SpawnAreaRadius = AreaRadius;
		Config->MinSpawnDistance = MinDistance;
		RandomSpawnConfig = Config;
	}
	void SetSafeSpawnAnchorsForTest(const TArray<FVector>& Anchors, float MinDistance)
	{
		UWTBRRandomSpawnConfigDataAsset* Config = NewObject<UWTBRRandomSpawnConfigDataAsset>(this);
		Config->MinSpawnDistance = MinDistance;
		Config->SafeSpawnAnchors = Anchors;
		RandomSpawnConfig = Config;
	}
	// Simulates a level with no spawn config assigned — for the "skip cleanly,
	// don't fall back to guessed defaults" regression test.
	void ClearRandomSpawnConfigForTest() { RandomSpawnConfig = nullptr; }
	// Wait-for-players test seams: real FTimerManager polling doesn't fire in the
	// headless transient-world fixtures, and World->GetTimeSeconds() doesn't
	// advance without ticking, so both the poll and the elapsed-wait check are
	// driven directly.
	void EnterLobbyAndWaitForPlayersForTest() { EnterLobbyAndWaitForPlayers(); }
	void PollLobbyReadyToStartForTest() { PollLobbyReadyToStart(); }
	bool IsLobbyPollTimerActiveForTest() const { return GetWorldTimerManager().IsTimerActive(LobbyPollTimerHandle); }
	void SetMinPlayersToStartForTest(int32 Value) { MinPlayersToStart = Value; }
	void SetLobbyMaxWaitSecondsForTest(float Value) { LobbyMaxWaitSeconds = Value; }
	void SetLobbyEnteredWorldTimeForTest(float Value) { LobbyEnteredWorldTimeSeconds = Value; }
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



