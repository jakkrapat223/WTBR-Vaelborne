// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "GameFramework/PlayerController.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "UObject/ConstructorHelpers.h"

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
