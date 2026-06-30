// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRGameState.h"

#include "Net/UnrealNetwork.h"

AWTBRGameState::AWTBRGameState()
{
    CurrentMatchRules.MatchMode = CurrentMatchMode;
}

void AWTBRGameState::SetCurrentMatchRules(const FWTBRMatchModeRules& NewRules)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentMatchRules = NewRules;
    CurrentMatchMode = NewRules.MatchMode;
    BroadcastMatchModeRulesChanged();
}

void AWTBRGameState::OnRep_CurrentMatchMode()
{
    BroadcastMatchModeRulesChanged();
}

void AWTBRGameState::OnRep_CurrentMatchRules()
{
    CurrentMatchMode = CurrentMatchRules.MatchMode;
    BroadcastMatchModeRulesChanged();
}

void AWTBRGameState::BroadcastMatchModeRulesChanged()
{
    OnMatchModeRulesChanged.Broadcast(CurrentMatchMode, CurrentMatchRules);
}

void AWTBRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWTBRGameState, CurrentMatchMode);
    DOREPLIFETIME(AWTBRGameState, CurrentMatchRules);
}
