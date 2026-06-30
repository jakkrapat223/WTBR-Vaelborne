// Copyright Epic Games, Inc. All Rights Reserved.

#include "WTBRGameMode.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
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
