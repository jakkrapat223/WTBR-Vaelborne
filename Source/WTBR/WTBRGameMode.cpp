// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
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
	Rules.bUseCorpseLootContainerOnDeath = false;
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

	const FVector SpawnLocation = Pawn->GetActorLocation()
		+ (Pawn->GetActorForwardVector() * 120.0f)
		+ FVector(0.0f, 0.0f, 20.0f);

	// Synthetic snapshots — non-null soft paths to non-existent validation assets,
	// identical in pattern to WTBR.CorpseLoot automation test helpers. No real
	// DataAsset is loaded; InitializeCorpseLootContainer only requires IsNull()==false
	// to include an entry. Two slots (main 0, sub 4) give Entries=2 for validation.
	TArray<FWTBRInstalledTriggerSlotSnapshot> SyntheticSnapshots;
	{
		FWTBRInstalledTriggerSlotSnapshot S0;
		S0.SlotIndex = 0;
		S0.DataAsset = TSoftObjectPtr<UWTBRTriggerDataAsset>(
			FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_ValEntry_0.B7_ValEntry_0")));
		S0.CachedCategory = ETriggerCategory::None;
		SyntheticSnapshots.Add(S0);

		FWTBRInstalledTriggerSlotSnapshot S4;
		S4.SlotIndex = 4;
		S4.DataAsset = TSoftObjectPtr<UWTBRTriggerDataAsset>(
			FSoftObjectPath(TEXT("/Game/WTBR/Validation/B7_ValEntry_4.B7_ValEntry_4")));
		S4.CachedCategory = ETriggerCategory::None;
		SyntheticSnapshots.Add(S4);
	}

	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Created synthetic snapshots (%d entries). Spawn attempt at %s."),
		SyntheticSnapshots.Num(),
		*SpawnLocation.ToString());

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

	Container->InitializeCorpseLootContainer(SyntheticSnapshots);

	UE_LOG(LogTemp, Log,
		TEXT("B7Validation: Corpse container spawned — Actor=%s Location=%s Entries=%d. Will replicate to all connected clients."),
		*GetNameSafe(Container),
		*SpawnLocation.ToString(),
		Container->GetLootEntryCount());
}
#endif  // !UE_BUILD_SHIPPING
