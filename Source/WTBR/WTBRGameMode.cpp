// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Inventory/WTBRGroundItemActor.h"
#include "Inventory/WTBRItemDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#if !UE_BUILD_SHIPPING
// Detected every ~1 s by PollValidationSpawnCVar after BeginPlay.
// ExecCmds fires at tick [1], after BeginPlay, so this poll catches it.
static TAutoConsoleVariable<float> CVarWTBRB7ValidationSpawnDelay(
	TEXT("WTBR.B7ValidationSpawnCorpseContainerDelaySeconds"),
	0.0f,
	TEXT("[Dev] Set > 0 via -ExecCmds on a dedicated server to schedule a B7 validation "
		 "corpse loot container spawn that many seconds after detection. Authority only. "
		 "Default 0 (disabled). Not compiled in Shipping builds."));

// B7D: runtime-only match override for the validation session. Applied once when
// the validation spawn finds its authority pawn. Changes nothing in source
// defaults — the production default rules and phases are untouched.
static TAutoConsoleVariable<int32> CVarWTBRB7ValidationForceInMatchRules(
	TEXT("WTBR.B7ValidationForceInMatchRules"),
	0,
	TEXT("[Dev] Set 1 together with the B7 validation spawn CVar to apply a runtime "
		 "match override on the validation server before the spawn: Phase=InMatch and "
		 "bAllowCorpseLoot/bAllowTriggerPickup/bAllowTriggerSwapDuringMatch=true. "
		 "Validation session only; production defaults unchanged. Default 0 (disabled). "
		 "Not compiled in Shipping builds."));

// B7D: lower-priority interaction candidates for the context-interact priority and
// non-mutation checks after late join.
static TAutoConsoleVariable<int32> CVarWTBRB7ValidationSpawnLowerPriorityActors(
	TEXT("WTBR.B7ValidationSpawnLowerPriorityActors"),
	0,
	TEXT("[Dev] Set 1 together with the B7 validation spawn CVar to also spawn one "
		 "dropped trigger and one BR ground item near the validation corpse container "
		 "as lower-priority context-interact candidates. Authority only. Default 0 "
		 "(disabled). Not compiled in Shipping builds."));
#endif

AWTBRGameMode::AWTBRGameMode()
{
	GameStateClass = AWTBRGameState::StaticClass();
	DefaultMatchRules = MakeDefaultRulesForMode(DefaultMatchMode);

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

TSubclassOf<AWTBRCorpseLootContainerActor> AWTBRGameMode::GetCorpseLootContainerClass() const
{
	return CorpseLootContainerClass
		? CorpseLootContainerClass
		: AWTBRCorpseLootContainerActor::StaticClass();
}

void AWTBRGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AWTBRGameState* WTBRGameState = GetGameState<AWTBRGameState>();
	if (!IsValid(WTBRGameState))
	{
		return;
	}

	WTBRGameState->SetCurrentMatchRules(ResolveDefaultMatchRules());

	// Wait-for-players (design gap 2026-07-13), then auto-advance the round loop
	// without needing console commands: Lobby -> LoadoutSetup -> Countdown ->
	// InMatch. Fixed loadout only this phase.
	EnterLobbyAndWaitForPlayers();

#if !UE_BUILD_SHIPPING
	// ExecCmds processes at tick [1], which is after BeginPlay fires during LoadMap.
	// A 1-s repeating poll catches WTBR.B7ValidationSpawnCorpseContainerDelaySeconds
	// being set without requiring the ExecCmds command to reach a PlayerController.
	GetWorldTimerManager().SetTimer(
		ValidationCVarPollTimerHandle,
		FTimerDelegate::CreateUObject(this, &AWTBRGameMode::PollValidationSpawnCVar),
		1.0f,
		/*bLoop=*/true);
#endif
}

void AWTBRGameMode::WTBRDebugSetMatchPhase(const FString& PhaseName)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRDebugSetMatchPhase is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugSetMatchPhase rejected: GameMode does not have authority."));
		return;
	}

	EWTBRMatchPhase ParsedPhase = EWTBRMatchPhase::None;
	if (!TryParseDebugMatchPhase(PhaseName, ParsedPhase))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugSetMatchPhase rejected: unknown phase '%s'. Valid phases: None, Lobby, PreMatch, LoadoutSetup, Countdown, InMatch, PostMatch."), *PhaseName);
		return;
	}

	AWTBRGameState* WTBRGameState = GetGameState<AWTBRGameState>();
	if (!IsValid(WTBRGameState))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugSetMatchPhase rejected: WTBRGameState is missing."));
		return;
	}

	WTBRGameState->SetCurrentMatchPhase(ParsedPhase);
	UE_LOG(LogTemp, Log, TEXT("WTBRDebugSetMatchPhase: phase set to %s."), *UEnum::GetValueAsString(ParsedPhase));
#endif
}

void AWTBRGameMode::BeginLoadoutSetupCountdown()
{
	// World-based lookup, not GetGameState<T>() (see AdvanceToCountdown/
	// AdvanceToInMatch/WTBRRestartRound): correct in real gameplay either way,
	// but also correct in headless automation fixtures that register a
	// GameState via World->SetGameState() without driving the full InitGame
	// flow — that path never populates the GameModeBase-cached member. Reached
	// by EnterLobbyAndWaitForPlayers/WTBRForceStartMatch, both test-seamed.
	if (AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr)
	{
		WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::LoadoutSetup);
	}
	bMatchResultResolved = false;

	GetWorldTimerManager().SetTimer(
		LoadoutSetupTimerHandle,
		FTimerDelegate::CreateUObject(this, &AWTBRGameMode::AdvanceToCountdown),
		FMath::Max(0.01f, LoadoutSetupDuration),
		/*bLoop=*/false);
}

void AWTBRGameMode::EnterLobbyAndWaitForPlayers()
{
	// World-based lookup — see the comment in BeginLoadoutSetupCountdown; test
	// seam EnterLobbyAndWaitForPlayersForTest calls this directly.
	if (AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr)
	{
		WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::Lobby);
	}

	if (const UWorld* World = GetWorld())
	{
		LobbyEnteredWorldTimeSeconds = World->GetTimeSeconds();
	}

	// Check immediately: covers the common MinPlayersToStart=1 solo/PIE/1v1-
	// harness case without waiting a full poll interval if a player is already
	// connected by the time BeginPlay fires.
	if (IsLobbyReadyToStart())
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Lobby ready immediately (%d/%d players) — advancing to LoadoutSetup."),
			CountConnectedPlayers(GetWorld()), MinPlayersToStart);
		BeginLoadoutSetupCountdown();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Phase -> Lobby, waiting for %d/%d players (polling every %.1fs)."),
		CountConnectedPlayers(GetWorld()), MinPlayersToStart, LobbyPollIntervalSeconds);

	GetWorldTimerManager().SetTimer(
		LobbyPollTimerHandle,
		this, &AWTBRGameMode::PollLobbyReadyToStart,
		LobbyPollIntervalSeconds,
		/*bLoop=*/true);
}

int32 AWTBRGameMode::CountConnectedPlayers(UWorld* World)
{
	int32 PlayerCount = 0;
	if (!World)
	{
		return PlayerCount;
	}

	for (TActorIterator<APlayerController> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			++PlayerCount;
		}
	}

	return PlayerCount;
}

bool AWTBRGameMode::IsLobbyReadyToStart() const
{
	if (CountConnectedPlayers(GetWorld()) >= MinPlayersToStart)
	{
		return true;
	}

	if (LobbyMaxWaitSeconds > 0.0f)
	{
		if (const UWorld* World = GetWorld())
		{
			if ((World->GetTimeSeconds() - LobbyEnteredWorldTimeSeconds) >= LobbyMaxWaitSeconds)
			{
				UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Lobby wait timed out after %.1fs with %d/%d players — starting anyway."),
					LobbyMaxWaitSeconds, CountConnectedPlayers(GetWorld()), MinPlayersToStart);
				return true;
			}
		}
	}

	return false;
}

void AWTBRGameMode::PollLobbyReadyToStart()
{
	if (!HasAuthority())
	{
		return;
	}

	const AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState) || WTBRGameState->GetCurrentMatchPhase() != EWTBRMatchPhase::Lobby)
	{
		// Already left Lobby (e.g. WTBRForceStartMatch fired) — stop polling.
		GetWorldTimerManager().ClearTimer(LobbyPollTimerHandle);
		return;
	}

	if (IsLobbyReadyToStart())
	{
		GetWorldTimerManager().ClearTimer(LobbyPollTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Lobby ready (%d/%d players) — advancing to LoadoutSetup."),
			CountConnectedPlayers(GetWorld()), MinPlayersToStart);
		BeginLoadoutSetupCountdown();
	}
}

void AWTBRGameMode::WTBRForceStartMatch()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRForceStartMatch rejected: GameMode does not have authority."));
		return;
	}

	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState) || WTBRGameState->GetCurrentMatchPhase() != EWTBRMatchPhase::Lobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRForceStartMatch rejected: only valid from Lobby (current Phase=%s)."),
			IsValid(WTBRGameState) ? *UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()) : TEXT("NoGameState"));
		return;
	}

	GetWorldTimerManager().ClearTimer(LobbyPollTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: WTBRForceStartMatch invoked (%d/%d players) — advancing to LoadoutSetup."),
		CountConnectedPlayers(GetWorld()), MinPlayersToStart);
	BeginLoadoutSetupCountdown();
}

void AWTBRGameMode::WTBRRestartRound()
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRRestartRound is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRRestartRound rejected: GameMode does not have authority."));
		return;
	}

	// World-based lookup, not GetGameState<T>() (see AdvanceToCountdown/
	// AdvanceToInMatch): identical in real gameplay once InitGameState() has run,
	// but also correct in headless automation fixtures that register a GameState
	// via World->SetGameState() without driving the full InitGame flow — that
	// path never populates the GameModeBase-cached GameState member.
	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRRestartRound rejected: WTBRGameState is missing."));
		return;
	}

	if (WTBRGameState->GetCurrentMatchPhase() != EWTBRMatchPhase::PostMatch)
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRRestartRound rejected: only valid from PostMatch (current Phase=%s)."),
			*UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()));
		return;
	}

	GetWorldTimerManager().ClearTimer(LoadoutSetupTimerHandle);
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(LobbyPollTimerHandle);
	ClearMatchTimeLimitTimer();

	WTBRGameState->SetMatchWinner(nullptr);
	WTBRGameState->ResetTeamScores();

	// Restore every combatant to a clean baseline before the next round begins.
	// ResetCombatRewardState() mirrors what AdvanceToInMatch already does every
	// round (HP/Alive/invulnerability/damage history); Vael/desperation are reset
	// here too since the existing round loop never touches them.
	int32 ResetCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
		{
			AWTBRCharacter* Character = *It;
			if (!IsValid(Character))
			{
				continue;
			}

			if (IsValid(Character->HealthComponent))
			{
				Character->HealthComponent->ResetCombatRewardState();
			}

			if (IsValid(Character->VaelComponent))
			{
				Character->VaelComponent->ResetDesperationState();
				Character->VaelComponent->GrantVael(Character->VaelComponent->GetMaxVael());
			}

			++ResetCount;
		}
	}

	BeginLoadoutSetupCountdown();

	UE_LOG(LogTemp, Log,
		TEXT("WTBR Match Flow: WTBRRestartRound invoked — winner cleared, CombatantsReset=%d, Phase -> LoadoutSetup (auto-advance to Countdown/InMatch)."),
		ResetCount);
#endif
}

void AWTBRGameMode::WTBRDebugPrintMatchState() const
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintMatchState is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintMatchState rejected: GameMode does not have authority."));
		return;
	}

	const AWTBRGameState* WTBRGameState = GetGameState<AWTBRGameState>();
	if (!IsValid(WTBRGameState))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintMatchState rejected: WTBRGameState is missing."));
		return;
	}

	const FWTBRMatchModeRules Rules = WTBRGameState->GetCurrentMatchRules();
	UE_LOG(LogTemp, Log,
		TEXT("WTBRDebugPrintMatchState: Mode=%s Phase=%s bEnablePassiveVaelRegen=%s VaelRegenPerSecond=%.2f bAllowTriggerSwapDuringMatch=%s"),
		*UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchMode()),
		*UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()),
		Rules.bEnablePassiveVaelRegen ? TEXT("true") : TEXT("false"),
		Rules.VaelRegenPerSecond,
		Rules.bAllowTriggerSwapDuringMatch ? TEXT("true") : TEXT("false"));
#endif
}

void AWTBRGameMode::WTBRDebugPrintTriggerLoadoutGate() const
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate rejected: GameMode does not have authority."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate rejected: World is missing."));
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate rejected: player 0 controller is missing."));
		return;
	}

	AWTBRCharacter* WTBRCharacter = Cast<AWTBRCharacter>(PlayerController->GetPawn());
	if (!IsValid(WTBRCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate rejected: player 0 pawn is not AWTBRCharacter. Pawn=%s"),
			*GetNameSafe(PlayerController->GetPawn()));
		return;
	}

	if (!IsValid(WTBRCharacter->TriggerSetComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugPrintTriggerLoadoutGate rejected: TriggerSetComponent is missing for pawn %s."),
			*GetNameSafe(WTBRCharacter));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("WTBRDebugPrintTriggerLoadoutGate: checking player 0 pawn %s component %s."),
		*GetNameSafe(WTBRCharacter),
		*GetNameSafe(WTBRCharacter->TriggerSetComponent));
	WTBRCharacter->TriggerSetComponent->DebugPrintTriggerLoadoutMutationGate();
#endif
}

void AWTBRGameMode::AdvanceToCountdown()
{
	if (!HasAuthority())
	{
		return;
	}

	// World-based lookup (not the AGameModeBase-cached GameState member): identical
	// in real gameplay once InitGameState() has run, but also works in headless
	// automation fixtures that register a GameState via World->SetGameState()
	// without driving the full InitGame/InitGameState flow.
	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState))
	{
		return;
	}

	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::Countdown);
	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Phase -> Countdown (auto-advance, no console command required)."));

	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle,
		FTimerDelegate::CreateUObject(this, &AWTBRGameMode::AdvanceToInMatch),
		FMath::Max(0.01f, CountdownDuration),
		/*bLoop=*/false);
}

void AWTBRGameMode::AdvanceToInMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState))
	{
		return;
	}

	bMatchResultResolved = false;
	WTBRGameState->SetMatchWinner(nullptr);
	WTBRGameState->ResetTeamScores();
	AssignTeamsForRound(WTBRGameState->GetCurrentMatchRules());
	if (WTBRGameState->GetCurrentMatchRules().bUseRandomSpawnPositions)
	{
		ApplyRandomSpawnPositions();
	}

	// Guarantee a clean start of round: every combatant enters InMatch at full HP,
	// Alive, damage history cleared. Fixed loadout is unaffected (TriggerSlots are
	// not touched here).
	int32 ResetCount = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
		{
			AWTBRCharacter* Character = *It;
			if (IsValid(Character) && IsValid(Character->HealthComponent))
			{
				Character->HealthComponent->ResetCombatRewardState();
				++ResetCount;
			}
		}
	}

	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
	StartMatchTimeLimitTimer(WTBRGameState->GetCurrentMatchRules());
	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Phase -> InMatch (auto-advance, no console command required). CombatantsReset=%d."),
		ResetCount);
}

void AWTBRGameMode::AssignTeamsForRound(const FWTBRMatchModeRules& Rules)
{
	if (!Rules.bAssignTeamsAtMatchStart || !Rules.bUseTeams)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 TeamSize = FMath::Max(1, Rules.TeamSize);
	int32 CombatantIndex = 0;
	for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
	{
		AWTBRCharacter* Character = *It;
		if (!IsValid(Character))
		{
			continue;
		}

		Character->SetTeamId(CombatantIndex / TeamSize);
		++CombatantIndex;
	}

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: teams assigned — %d combatants block-filled into teams of %d (%d teams)."),
		CombatantIndex,
		TeamSize,
		FMath::DivideAndRoundUp(CombatantIndex, TeamSize));
}

TArray<FVector> AWTBRGameMode::GenerateRandomSpawnPoints(const FVector& Center, float AreaRadius, float MinDistance, int32 Count, FRandomStream& RandomStream)
{
	TArray<FVector> Points;
	if (Count <= 0)
	{
		return Points;
	}
	Points.Reserve(Count);

	// Rejection sampling, per point: try several random candidates uniformly
	// distributed inside the disc (sqrt(FRand()) keeps the distribution area-
	// uniform rather than center-biased) and take the first one that clears
	// MinDistance from every point already placed. If none of the attempts
	// clear it — the area is too small for Count points at this spacing — fall
	// back to whichever candidate achieved the largest minimum distance, so the
	// function always terminates and always returns exactly Count points.
	constexpr int32 MaxAttemptsPerPoint = 50;
	const float SafeAreaRadius = FMath::Max(0.0f, AreaRadius);

	for (int32 PointIndex = 0; PointIndex < Count; ++PointIndex)
	{
		FVector BestCandidate = Center;
		float BestCandidateMinDistance = -1.0f;
		bool bSatisfiedMinDistance = false;

		for (int32 Attempt = 0; Attempt < MaxAttemptsPerPoint && !bSatisfiedMinDistance; ++Attempt)
		{
			const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
			const float Radius = SafeAreaRadius * FMath::Sqrt(RandomStream.FRand());
			const FVector Candidate = Center + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.0f);

			float CandidateMinDistance = TNumericLimits<float>::Max();
			for (const FVector& Existing : Points)
			{
				CandidateMinDistance = FMath::Min(CandidateMinDistance, FVector::Dist(Candidate, Existing));
			}

			if (CandidateMinDistance > BestCandidateMinDistance)
			{
				BestCandidateMinDistance = CandidateMinDistance;
				BestCandidate = Candidate;
			}

			if (CandidateMinDistance >= MinDistance)
			{
				bSatisfiedMinDistance = true;
			}
		}

		Points.Add(BestCandidate);
	}

	return Points;
}

FVector AWTBRGameMode::SnapSpawnPointToGround(UWorld* World, const FVector& InPoint, const AActor* IgnoreActor)
{
	if (!World) return InPoint;

	constexpr float TraceUpOffset = 1000.0f;
	constexpr float TraceDownDistance = 20000.0f;
	constexpr float GroundClearance = 92.0f; // roughly a standing capsule half-height

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (IgnoreActor) Params.AddIgnoredActor(IgnoreActor);

	const FVector TraceStart = InPoint + FVector(0.0f, 0.0f, TraceUpOffset);
	const FVector TraceEnd   = TraceStart - FVector(0.0f, 0.0f, TraceDownDistance);

	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint + FVector(0.0f, 0.0f, GroundClearance);
	}

	UE_LOG(LogTemp, Warning, TEXT("WTBR Match Flow: random spawn point %s found no ground within %.0f units — spawning unadjusted (character may fall). Check RandomSpawnConfig's Center/Radius against the level's actual floor bounds."),
		*InPoint.ToString(), TraceDownDistance);
	return InPoint;
}

void AWTBRGameMode::ApplyRandomSpawnPositions()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsValid(RandomSpawnConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBR Match Flow: bUseRandomSpawnPositions is set but no RandomSpawnConfig DataAsset is assigned on this GameMode — skipping random spawn, using default PlayerStart placement instead."));
		return;
	}

	TArray<AWTBRCharacter*> Characters;
	for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Characters.Add(*It);
		}
	}

	if (Characters.Num() == 0)
	{
		return;
	}

	FRandomStream RandomStream;
	RandomStream.GenerateNewSeed();

	const FVector Center      = RandomSpawnConfig->SpawnAreaCenter;
	const float   AreaRadius  = RandomSpawnConfig->SpawnAreaRadius;
	const float   MinDistance = RandomSpawnConfig->MinSpawnDistance;

	const TArray<FVector> SpawnPoints = GenerateRandomSpawnPoints(
		Center, AreaRadius, MinDistance, Characters.Num(), RandomStream);

	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		const FVector GroundedPoint = SnapSpawnPointToGround(World, SpawnPoints[i], Characters[i]);
		Characters[i]->SetActorLocation(GroundedPoint, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
	}

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: random spawn positions applied to %d combatants (Center=%s AreaRadius=%.0f MinDistance=%.0f)."),
		Characters.Num(),
		*Center.ToString(),
		AreaRadius,
		MinDistance);
}

void AWTBRGameMode::CollectAliveCombatants(UWorld* World, TArray<AWTBRCharacter*>& OutAlive)
{
	OutAlive.Reset();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
	{
		AWTBRCharacter* Character = *It;
		if (IsValid(Character) && IsValid(Character->HealthComponent)
			&& Character->HealthComponent->GetCombatState() != EWTBRCombatState::Eliminated)
		{
			OutAlive.Add(Character);
		}
	}
}

void AWTBRGameMode::NotifyCombatantEliminated(AWTBRCharacter* EliminatedCharacter, AActor* FinalDamageInstigator)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bMatchResultResolved)
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: NotifyCombatantEliminated(%s) ignored — result already resolved this round."),
			*GetNameSafe(EliminatedCharacter));
		return;
	}

	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState) || WTBRGameState->GetCurrentMatchPhase() != EWTBRMatchPhase::InMatch)
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: NotifyCombatantEliminated(%s) ignored — not InMatch (Phase=%s)."),
			*GetNameSafe(EliminatedCharacter),
			IsValid(WTBRGameState) ? *UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()) : TEXT("NoGameState"));
		return;
	}

	// Team mode engages only when teams were actually assigned this round —
	// bUseTeams defaults true across existing rules structs, so the flag alone
	// must not reroute the legacy individual path (1v1 harness, old fixtures).
	const bool bTeamsActive = WTBRGameState->GetCurrentMatchRules().bUseTeams
		&& IsValid(EliminatedCharacter) && EliminatedCharacter->HasTeam();

	if (bTeamsActive)
	{
		// Kill point at Eliminated only (design lock 2026-07-13). Enemy kills only:
		// team kills, self-elimination, and environment deaths score nothing.
		AWTBRCharacter* KillerCharacter = Cast<AWTBRCharacter>(FinalDamageInstigator);
		if (IsValid(KillerCharacter)
			&& KillerCharacter != EliminatedCharacter
			&& KillerCharacter->HasTeam()
			&& !KillerCharacter->IsSameTeamAs(EliminatedCharacter))
		{
			WTBRGameState->AddTeamKillPoint(KillerCharacter->GetTeamId());
		}
	}

	TArray<AWTBRCharacter*> AliveCombatants;
	CollectAliveCombatants(GetWorld(), AliveCombatants);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: combatant eliminated (%s). AliveCombatantsRemaining=%d."),
		*GetNameSafe(EliminatedCharacter),
		AliveCombatants.Num());

	if (bTeamsActive)
	{
		ResolveTeamRoundIfOver(*WTBRGameState, AliveCombatants);
		return;
	}

	if (AliveCombatants.Num() > 1)
	{
		// Match continues — more than one combatant still standing.
		return;
	}

	bMatchResultResolved = true;
	ClearMatchTimeLimitTimer();

	AWTBRCharacter* WinnerCharacter = AliveCombatants.Num() == 1 ? AliveCombatants[0] : nullptr;
	APlayerState* WinnerPlayerState = IsValid(WinnerCharacter) ? WinnerCharacter->GetPlayerState() : nullptr;

	WTBRGameState->SetMatchWinner(WinnerPlayerState);
	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::PostMatch);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: ROUND OVER. Winner=%s (PlayerState=%s). Phase -> PostMatch."),
		*GetNameSafe(WinnerCharacter),
		*GetNameSafe(WinnerPlayerState));
}

void AWTBRGameMode::ResolveTeamRoundIfOver(AWTBRGameState& WTBRGameState, const TArray<AWTBRCharacter*>& AliveCombatants)
{
	int32 TeamlessAliveCount = 0;
	const TMap<int32, int32> AliveCountByTeam = BuildAliveCountByTeam(AliveCombatants, TeamlessAliveCount);

	const int32 RemainingFactions = AliveCountByTeam.Num() + TeamlessAliveCount;
	if (RemainingFactions > 1)
	{
		// Match continues — more than one team still has a living member.
		return;
	}

	bMatchResultResolved = true;
	ClearMatchTimeLimitTimer();

	ApplyTeamRoundEndScoringAndWinner(WTBRGameState, AliveCountByTeam);
	WTBRGameState.SetCurrentMatchPhase(EWTBRMatchPhase::PostMatch);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: TEAM ROUND OVER (last team standing). AliveTeams=%d WinningTeam=%d (score-based=%s). Phase -> PostMatch."),
		AliveCountByTeam.Num(),
		WTBRGameState.GetWinningTeamId(),
		WTBRGameState.GetCurrentMatchRules().bScoreBasedTeamWinner ? TEXT("true") : TEXT("false"));
}

TMap<int32, int32> AWTBRGameMode::BuildAliveCountByTeam(const TArray<AWTBRCharacter*>& AliveCombatants, int32& OutTeamlessAliveCount)
{
	TMap<int32, int32> AliveCountByTeam;
	OutTeamlessAliveCount = 0;

	for (const AWTBRCharacter* Alive : AliveCombatants)
	{
		if (!IsValid(Alive))
		{
			continue;
		}

		if (Alive->HasTeam())
		{
			AliveCountByTeam.FindOrAdd(Alive->GetTeamId())++;
		}
		else
		{
			// A team-less character in a team round should not happen (assignment
			// covers every AWTBRCharacter), but if one slips through it counts as
			// its own faction rather than silently ending the round early, and
			// never scores (no TeamId to credit).
			++OutTeamlessAliveCount;
			UE_LOG(LogTemp, Warning, TEXT("WTBR Match Flow: alive combatant %s has no TeamId during a team round."),
				*GetNameSafe(Alive));
		}
	}

	return AliveCountByTeam;
}

void AWTBRGameMode::ApplyTeamRoundEndScoringAndWinner(AWTBRGameState& WTBRGameState, const TMap<int32, int32>& AliveCountByTeam)
{
	// Every team still holding a living member banks its survivor bonus — this is
	// the natural single-survivor case for a last-team-standing end, but a
	// time-limit end can credit several teams at once (design lock 2026-07-13:
	// "เหลือ 3 ทีม" — the match can stop with multiple teams still alive).
	for (const TPair<int32, int32>& Pair : AliveCountByTeam)
	{
		WTBRGameState.AddTeamSurvivorBonus(Pair.Key, Pair.Value);
	}

	int32 WinningTeam = INDEX_NONE;
	if (WTBRGameState.GetCurrentMatchRules().bScoreBasedTeamWinner)
	{
		// TeamThree15P: highest total score wins — an already-wiped team can beat
		// every still-alive team on kill points alone.
		int32 BestTotal = -1;
		TArray<int32> LeadingTeams;
		for (const FWTBRTeamScore& Score : WTBRGameState.GetTeamScores())
		{
			const int32 Total = Score.GetTotalScore();
			if (Total > BestTotal)
			{
				BestTotal = Total;
				LeadingTeams.Reset();
				LeadingTeams.Add(Score.TeamId);
			}
			else if (Total == BestTotal)
			{
				LeadingTeams.Add(Score.TeamId);
			}
		}

		if (LeadingTeams.Num() == 1)
		{
			WinningTeam = LeadingTeams[0];
		}
		else if (LeadingTeams.Num() > 1)
		{
			// Score tie among the leaders: break it toward whichever tied team has
			// the most living members right now (naturally reduces to "the sole
			// survivor wins the tiebreak" when only one team is alive; stays a
			// draw if the tie can't be broken by alive count either).
			int32 BestAliveCount = 0;
			for (int32 TeamId : LeadingTeams)
			{
				BestAliveCount = FMath::Max(BestAliveCount, AliveCountByTeam.FindRef(TeamId));
			}

			TArray<int32> AliveTiebreakLeaders;
			for (int32 TeamId : LeadingTeams)
			{
				if (AliveCountByTeam.FindRef(TeamId) == BestAliveCount)
				{
					AliveTiebreakLeaders.Add(TeamId);
				}
			}

			WinningTeam = (AliveTiebreakLeaders.Num() == 1) ? AliveTiebreakLeaders[0] : INDEX_NONE;
		}
	}
	else if (AliveCountByTeam.Num() == 1)
	{
		// BR-style: the sole remaining team wins outright; more than one team
		// alive under a non-score-based mode has no defined winner (draw).
		WinningTeam = AliveCountByTeam.CreateConstIterator().Key();
	}

	WTBRGameState.SetWinningTeamId(WinningTeam);
	// Individual winner field stays null in team mode — team results live in
	// WinningTeamId/TeamScores.
	WTBRGameState.SetMatchWinner(nullptr);
}

void AWTBRGameMode::StartMatchTimeLimitTimer(const FWTBRMatchModeRules& Rules)
{
	ClearMatchTimeLimitTimer();

	if (Rules.MatchTimeLimitSeconds <= 0.0f)
	{
		// 0 = no time limit for this mode (BR relies on the zone instead, once
		// implemented; the 1v1 harness and individual modes never set this).
		return;
	}

	GetWorldTimerManager().SetTimer(
		MatchTimeLimitTimerHandle,
		this, &AWTBRGameMode::OnMatchTimeLimitExpired,
		Rules.MatchTimeLimitSeconds,
		/*bLoop=*/false);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: match time limit armed for %.1fs."), Rules.MatchTimeLimitSeconds);
}

void AWTBRGameMode::ClearMatchTimeLimitTimer()
{
	GetWorldTimerManager().ClearTimer(MatchTimeLimitTimerHandle);
}

void AWTBRGameMode::OnMatchTimeLimitExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bMatchResultResolved)
	{
		// The round already ended naturally (last team standing) before the
		// time-limit timer fired — nothing to do.
		return;
	}

	AWTBRGameState* WTBRGameState = GetWorld() ? GetWorld()->GetGameState<AWTBRGameState>() : nullptr;
	if (!IsValid(WTBRGameState) || WTBRGameState->GetCurrentMatchPhase() != EWTBRMatchPhase::InMatch)
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: match time limit expired but ignored — not InMatch (Phase=%s)."),
			IsValid(WTBRGameState) ? *UEnum::GetValueAsString(WTBRGameState->GetCurrentMatchPhase()) : TEXT("NoGameState"));
		return;
	}

	if (!WTBRGameState->GetCurrentMatchRules().bUseTeams)
	{
		// Time-limit end-of-match is a team-mode feature (design lock
		// 2026-07-13, TeamThree15P) — the individual round loop / 1v1 harness
		// has no time limit and should never reach here with one configured.
		UE_LOG(LogTemp, Warning, TEXT("WTBR Match Flow: match time limit expired but bUseTeams is false — ignored."));
		return;
	}

	TArray<AWTBRCharacter*> AliveCombatants;
	CollectAliveCombatants(GetWorld(), AliveCombatants);

	int32 TeamlessAliveCount = 0;
	const TMap<int32, int32> AliveCountByTeam = BuildAliveCountByTeam(AliveCombatants, TeamlessAliveCount);

	bMatchResultResolved = true;
	ApplyTeamRoundEndScoringAndWinner(*WTBRGameState, AliveCountByTeam);
	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::PostMatch);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: TEAM ROUND OVER (time limit). AliveTeams=%d WinningTeam=%d (score-based=%s). Phase -> PostMatch."),
		AliveCountByTeam.Num(),
		WTBRGameState->GetWinningTeamId(),
		WTBRGameState->GetCurrentMatchRules().bScoreBasedTeamWinner ? TEXT("true") : TEXT("false"));
}

FWTBRMatchModeRules AWTBRGameMode::ResolveDefaultMatchRules() const
{
	for (const UWTBRMatchModeRulesDataAsset* RulesAsset : MatchModeRuleAssets)
	{
		if (IsValid(RulesAsset) && RulesAsset->MatchMode == DefaultMatchMode)
		{
			FWTBRMatchModeRules Rules = RulesAsset->Rules;
			Rules.MatchMode = RulesAsset->MatchMode;
			return Rules;
		}
	}

	if (DefaultMatchRules.MatchMode == DefaultMatchMode)
	{
		return DefaultMatchRules;
	}

	return MakeDefaultRulesForMode(DefaultMatchMode);
}

bool AWTBRGameMode::TryParseDebugMatchPhase(const FString& PhaseName, EWTBRMatchPhase& OutPhase)
{
	const FString NormalizedPhase = PhaseName.TrimStartAndEnd().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));

	if (NormalizedPhase.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::None;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("Lobby"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::Lobby;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("PreMatch"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::PreMatch;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("LoadoutSetup"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::LoadoutSetup;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("Countdown"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::Countdown;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("InMatch"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::InMatch;
		return true;
	}
	if (NormalizedPhase.Equals(TEXT("PostMatch"), ESearchCase::IgnoreCase))
	{
		OutPhase = EWTBRMatchPhase::PostMatch;
		return true;
	}

	return false;
}

FWTBRMatchModeRules AWTBRGameMode::MakeDefaultRulesForMode(EWTBRMatchMode MatchMode)
{
	FWTBRMatchModeRules Rules;
	Rules.MatchMode = MatchMode;
	Rules.TeamSize = 3;
	Rules.MaxPlayers = 6;
	Rules.bEnablePassiveVaelRegen = false;
	Rules.VaelRegenPerSecond = 0.0f;
	Rules.bAllowCorpseLoot = false;
	Rules.bAllowTriggerPickup = false;
	Rules.bUseCorpseLootContainerOnDeath = true;
	Rules.bAllowTriggerSwapDuringMatch = false;
	Rules.bAllowLoadoutEditDuringMatch = false;
	Rules.bUseTeams = true;

	switch (MatchMode)
	{
	case EWTBRMatchMode::RankWar:
		Rules.TeamSize = 3;
		Rules.MaxPlayers = 6;
		break;
	case EWTBRMatchMode::TeamThree15P:
		// Mode design lock 2026-07-13: 5 teams of 3, no zone, no corpse loot/bag,
		// kill+survivor point scoring where the highest total wins (a wiped team
		// can beat the survivors on kills). Match also force-ends at a time limit
		// even with several teams still alive (does not wait for last team
		// standing) — every living team still banks its survivor bonus at that
		// point. MatchTimeLimitSeconds is a placeholder; exact duration TBD via
		// playtest.
		Rules.TeamSize = 3;
		Rules.MaxPlayers = 15;
		Rules.bAssignTeamsAtMatchStart = true;
		Rules.bScoreBasedTeamWinner = true;
		Rules.bAllowCorpseLoot = false;
		Rules.bUseCorpseLootContainerOnDeath = false;
		Rules.MatchTimeLimitSeconds = 900.0f;
		Rules.bUseRandomSpawnPositions = true;
		break;
	case EWTBRMatchMode::BattleRoyale:
		// Mode design lock 2026-07-13: ~60 players in teams of 3 (20 teams),
		// standard last-team-standing winner (not score-based).
		Rules.TeamSize = 3;
		Rules.MaxPlayers = 60;
		Rules.bEnablePassiveVaelRegen = true;
		Rules.VaelRegenPerSecond = 2.0f;
		Rules.bAllowCorpseLoot = true;
		Rules.bAllowTriggerPickup = true;
		Rules.bAllowTriggerSwapDuringMatch = true;
		Rules.bAllowLoadoutEditDuringMatch = true;
		Rules.bUseTeams = true;
		Rules.bAssignTeamsAtMatchStart = true;
		Rules.bScoreBasedTeamWinner = false;
		Rules.bUseRandomSpawnPositions = true;
		break;
	case EWTBRMatchMode::None:
	case EWTBRMatchMode::ThreeVThree:
	default:
		Rules.MatchMode = EWTBRMatchMode::ThreeVThree;
		break;
	}

	return Rules;
}

void AWTBRGameMode::WTBRDebugServerSpawnCorpseLootContainerForValidation(float DelaySeconds)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerSpawnCorpseLootContainerForValidation is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerSpawnCorpseLootContainerForValidation rejected: no server authority."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerSpawnCorpseLootContainerForValidation rejected: World is missing."));
		return;
	}

	ValidationSpawnRetryCount = 0;

	if (DelaySeconds > 0.0f)
	{
		UE_LOG(LogTemp, Log,
			TEXT("WTBR B7 validation: WTBRDebugServerSpawnCorpseLootContainerForValidation: scheduling container spawn in %.1f s."),
			DelaySeconds);
		FTimerDelegate TimerDel;
		TimerDel.BindUObject(this, &AWTBRGameMode::SpawnCorpseLootContainerForValidation_Authority);
		GetWorldTimerManager().SetTimer(ValidationSpawnTimerHandle, TimerDel, DelaySeconds, false);
	}
	else
	{
		SpawnCorpseLootContainerForValidation_Authority();
	}
#endif
}

void AWTBRGameMode::WTBRDebugServerPrintCorpseLootContainers() const
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerPrintCorpseLootContainers is disabled in Shipping builds."));
#else
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerPrintCorpseLootContainers rejected: no server authority."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("WTBRDebugServerPrintCorpseLootContainers rejected: World is missing."));
		return;
	}

	int32 Count = 0;
	for (TActorIterator<AWTBRCorpseLootContainerActor> It(World); It; ++It)
	{
		AWTBRCorpseLootContainerActor* Container = *It;
		if (!IsValid(Container))
		{
			continue;
		}
		++Count;
		UE_LOG(LogTemp, Log,
			TEXT("WTBR B7 validation: container[%d] Actor=%s Location=%s Authority=%s Entries=%d HasAvailableLoot=%s"),
			Count,
			*GetNameSafe(Container),
			*Container->GetActorLocation().ToString(),
			Container->HasAuthority() ? TEXT("true") : TEXT("false"),
			Container->GetLootEntryCount(),
			Container->HasAvailableLootEntries() ? TEXT("true") : TEXT("false"));
	}

	if (Count == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR B7 validation: WTBRDebugServerPrintCorpseLootContainers: no AWTBRCorpseLootContainerActor found in world."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("WTBR B7 validation: WTBRDebugServerPrintCorpseLootContainers: found %d container(s)."), Count);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void AWTBRGameMode::PollValidationSpawnCVar()
{
	const float Delay = CVarWTBRB7ValidationSpawnDelay.GetValueOnGameThread();
	if (Delay <= 0.0f)
	{
		return;
	}

	// CVar is set — stop polling and reset to prevent re-triggering.
	GetWorldTimerManager().ClearTimer(ValidationCVarPollTimerHandle);
	CVarWTBRB7ValidationSpawnDelay->Set(0.0f, ECVF_SetByCode);

	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: CVar requested spawn delay %.1f s."),
		Delay);
	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Scheduling corpse container spawn in %.1f s."),
		Delay);

	ValidationSpawnRetryCount = 0;

	FTimerDelegate TimerDel;
	TimerDel.BindUObject(this, &AWTBRGameMode::SpawnCorpseLootContainerForValidation_Authority);
	GetWorldTimerManager().SetTimer(ValidationSpawnTimerHandle, TimerDel, Delay, /*bLoop=*/false);
}

void AWTBRGameMode::SpawnCorpseLootContainerForValidation_Authority()
{
	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Spawn attempt (try %d/%d)."),
		ValidationSpawnRetryCount + 1,
		ValidationSpawnMaxRetries + 1);

	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("B7Validation: Spawn failed reason — world or authority missing."));
		return;
	}

	// Wait for a joined, authority-controlled pawn before spawning.
	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;

	if (!IsValid(Pawn))
	{
		if (ValidationSpawnRetryCount < ValidationSpawnMaxRetries)
		{
			++ValidationSpawnRetryCount;
			UE_LOG(LogTemp, Log,
				TEXT("B7Validation: Spawn failed reason — no authority pawn yet. Retry %d/%d in 2 s."),
				ValidationSpawnRetryCount,
				ValidationSpawnMaxRetries);
			FTimerDelegate RetryDel;
			RetryDel.BindUObject(this, &AWTBRGameMode::SpawnCorpseLootContainerForValidation_Authority);
			GetWorldTimerManager().SetTimer(ValidationSpawnTimerHandle, RetryDel, 2.0f, /*bLoop=*/false);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("B7Validation: Spawn failed reason — max retries (%d) reached, no authority pawn found."),
				ValidationSpawnMaxRetries);
		}
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Found authority character %s at %s."),
		*GetNameSafe(Pawn),
		*Pawn->GetActorLocation().ToString());

	// B7D: runtime-only match override so the corpse loot pickup RPC gates
	// (IsInMatch + AllowsCorpseLoot/TriggerPickup/TriggerSwapDuringMatch) pass in
	// the validation session. Applied here (once) — retries only re-enter before
	// the pawn is found. Production defaults in source are unchanged.
	if (CVarWTBRB7ValidationForceInMatchRules.GetValueOnGameThread() != 0)
	{
		if (AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>())
		{
			FWTBRMatchModeRules OverrideRules = WTBRGameState->GetCurrentMatchRules();
			OverrideRules.bAllowCorpseLoot = true;
			OverrideRules.bAllowTriggerPickup = true;
			OverrideRules.bAllowTriggerSwapDuringMatch = true;
			WTBRGameState->SetCurrentMatchRules(OverrideRules);
			WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::InMatch);
			UE_LOG(LogTemp, Log,
				TEXT("B7Validation: Runtime match override applied — Phase=InMatch, corpse loot/trigger pickup/swap enabled (validation session only; production defaults unchanged)."));
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("B7Validation: Runtime match override requested but WTBRGameState is missing."));
		}
	}

	const FVector SpawnLocation = Pawn->GetActorLocation()
		+ (Pawn->GetActorForwardVector() * 120.0f)
		+ FVector(0.0f, 0.0f, 20.0f);

	// B7D: prefer the found character's real installed trigger snapshots so the
	// loot pickup RPC can complete against loadable DataAssets. Fall back to the
	// B7C synthetic non-existent validation paths when no installed trigger exists.
	TArray<FWTBRInstalledTriggerSlotSnapshot> ValidationSnapshots;
	if (const AWTBRCharacter* WTBRPawn = Cast<AWTBRCharacter>(Pawn))
	{
		if (IsValid(WTBRPawn->TriggerSetComponent))
		{
			TArray<FWTBRInstalledTriggerSlotSnapshot> InstalledSnapshots;
			WTBRPawn->TriggerSetComponent->GetInstalledTriggerSlotSnapshots(InstalledSnapshots);
			for (const FWTBRInstalledTriggerSlotSnapshot& Snapshot : InstalledSnapshots)
			{
				if (Snapshot.IsValid())
				{
					ValidationSnapshots.Add(Snapshot);
					if (ValidationSnapshots.Num() >= 2)
					{
						break;
					}
				}
			}
		}
	}

	if (ValidationSnapshots.Num() > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("B7Validation: Created snapshots from installed triggers (%d entries, first=%s). Spawn attempt at %s."),
			ValidationSnapshots.Num(),
			*ValidationSnapshots[0].DataAsset.ToSoftObjectPath().ToString(),
			*SpawnLocation.ToString());
	}
	else
	{
		// Synthetic snapshots — non-null soft paths to non-existent validation assets,
		// identical in pattern to WTBR.CorpseLoot automation test helpers. No real
		// DataAsset is loaded; InitializeCorpseLootContainer only requires IsNull()==false
		// to include an entry. Two slots (main 0, sub 4) give Entries=2 for validation.
		FWTBRInstalledTriggerSlotSnapshot S0;
		S0.SlotIndex = 0;
		S0.DataAsset = TSoftObjectPtr<UWTBRTriggerDataAsset>(
			FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_ValEntry_0.B7_ValEntry_0")));
		S0.CachedCategory = ETriggerCategory::None;
		ValidationSnapshots.Add(S0);

		FWTBRInstalledTriggerSlotSnapshot S4;
		S4.SlotIndex = 4;
		S4.DataAsset = TSoftObjectPtr<UWTBRTriggerDataAsset>(
			FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_ValEntry_4.B7_ValEntry_4")));
		S4.CachedCategory = ETriggerCategory::None;
		ValidationSnapshots.Add(S4);

		UE_LOG(LogTemp, Log,
			TEXT("B7Validation: Created synthetic snapshots (%d entries). Spawn attempt at %s."),
			ValidationSnapshots.Num(),
			*SpawnLocation.ToString());
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AWTBRCorpseLootContainerActor* Container = World->SpawnActor<AWTBRCorpseLootContainerActor>(
		GetCorpseLootContainerClass(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!IsValid(Container))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("B7Validation: Spawn failed reason — SpawnActor returned null at %s."),
			*SpawnLocation.ToString());
		return;
	}

	Container->InitializeCorpseLootContainer(ValidationSnapshots);

	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Corpse container spawned — Actor=%s Location=%s Entries=%d. Will replicate to all connected clients."),
		*GetNameSafe(Container),
		*SpawnLocation.ToString(),
		Container->GetLootEntryCount());

	// B7D: lower-priority context-interact candidates for the priority and
	// non-mutation checks. Both use non-null soft paths to non-existent validation
	// assets — presence and consumed-state are what the checks read; neither can be
	// picked up into a slot/inventory, so they cannot mutate the character.
	if (CVarWTBRB7ValidationSpawnLowerPriorityActors.GetValueOnGameThread() != 0)
	{
		// Dropped trigger: lateral offset from the pawn->container axis. Cone-based
		// focus (dot >= 0.5 within trace range) still sees it when a client faces
		// the container, so it is a live lower-priority candidate.
		const FVector DroppedTriggerLocation = SpawnLocation + (Pawn->GetActorRightVector() * 100.0f);
		AWTBRDroppedTriggerActor* DroppedTrigger = World->SpawnActor<AWTBRDroppedTriggerActor>(
			AWTBRDroppedTriggerActor::StaticClass(),
			DroppedTriggerLocation,
			FRotator::ZeroRotator,
			SpawnParams);
		if (IsValid(DroppedTrigger))
		{
			DroppedTrigger->InitializeDroppedTrigger(
				TSoftObjectPtr<UWTBRTriggerDataAsset>(
					FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_LowerPrio_Dropped.B7_LowerPrio_Dropped"))),
				0,
				ETriggerCategory::None);
			UE_LOG(LogTemp, Log,
				TEXT("B7Validation: Lower-priority dropped trigger spawned — Actor=%s Location=%s."),
				*GetNameSafe(DroppedTrigger),
				*DroppedTriggerLocation.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("B7Validation: Lower-priority dropped trigger spawn returned null at %s."),
				*DroppedTriggerLocation.ToString());
		}

		// BR ground item: further along the pawn->container axis, in view behind
		// the container.
		const FVector GroundItemLocation = SpawnLocation + (Pawn->GetActorForwardVector() * 150.0f);
		AWTBRGroundItemActor* GroundItem = World->SpawnActor<AWTBRGroundItemActor>(
			AWTBRGroundItemActor::StaticClass(),
			GroundItemLocation,
			FRotator::ZeroRotator,
			SpawnParams);
		if (IsValid(GroundItem))
		{
			GroundItem->InitializeGroundItem(
				TSoftObjectPtr<UWTBRItemDataAsset>(
					FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_LowerPrio_GroundItem.B7_LowerPrio_GroundItem"))),
				1);
			UE_LOG(LogTemp, Log,
				TEXT("B7Validation: Lower-priority ground item spawned — Actor=%s Location=%s."),
				*GetNameSafe(GroundItem),
				*GroundItemLocation.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("B7Validation: Lower-priority ground item spawn returned null at %s."),
				*GroundItemLocation.ToString());
		}
	}
}
#endif  // !UE_BUILD_SHIPPING
