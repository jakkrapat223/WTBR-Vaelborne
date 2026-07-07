// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "Components/WTBRHealthComponent.h"
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
	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::LoadoutSetup);
	bMatchResultResolved = false;

	// Phase 7B: auto-advance the round loop without needing console commands.
	// LoadoutSetup -> Countdown -> InMatch. Fixed loadout only this phase.
	GetWorldTimerManager().SetTimer(
		LoadoutSetupTimerHandle,
		FTimerDelegate::CreateUObject(this, &AWTBRGameMode::AdvanceToCountdown),
		FMath::Max(0.01f, LoadoutSetupDuration),
		/*bLoop=*/false);

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
	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: Phase -> InMatch (auto-advance, no console command required). CombatantsReset=%d."),
		ResetCount);
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

void AWTBRGameMode::NotifyCombatantEliminated(AWTBRCharacter* EliminatedCharacter)
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

	TArray<AWTBRCharacter*> AliveCombatants;
	CollectAliveCombatants(GetWorld(), AliveCombatants);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: combatant eliminated (%s). AliveCombatantsRemaining=%d."),
		*GetNameSafe(EliminatedCharacter),
		AliveCombatants.Num());

	if (AliveCombatants.Num() > 1)
	{
		// Match continues — more than one combatant still standing.
		return;
	}

	bMatchResultResolved = true;

	AWTBRCharacter* WinnerCharacter = AliveCombatants.Num() == 1 ? AliveCombatants[0] : nullptr;
	APlayerState* WinnerPlayerState = IsValid(WinnerCharacter) ? WinnerCharacter->GetPlayerState() : nullptr;

	WTBRGameState->SetMatchWinner(WinnerPlayerState);
	WTBRGameState->SetCurrentMatchPhase(EWTBRMatchPhase::PostMatch);

	UE_LOG(LogTemp, Log, TEXT("WTBR Match Flow: ROUND OVER. Winner=%s (PlayerState=%s). Phase -> PostMatch."),
		*GetNameSafe(WinnerCharacter),
		*GetNameSafe(WinnerPlayerState));
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
		Rules.TeamSize = 3;
		Rules.MaxPlayers = 15;
		break;
	case EWTBRMatchMode::BattleRoyale:
		Rules.TeamSize = 1;
		Rules.MaxPlayers = 0; // Placeholder until BR target population is locked.
		Rules.bEnablePassiveVaelRegen = true;
		Rules.VaelRegenPerSecond = 2.0f;
		Rules.bAllowCorpseLoot = true;
		Rules.bAllowTriggerPickup = true;
		Rules.bAllowTriggerSwapDuringMatch = true;
		Rules.bAllowLoadoutEditDuringMatch = true;
		Rules.bUseTeams = false;
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
