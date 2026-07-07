// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "WTBRGameState.h"

#include "GameFramework/PlayerState.h"
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

void AWTBRGameState::SetCurrentMatchPhase(EWTBRMatchPhase NewPhase)
{
    if (!HasAuthority())
    {
        return;
    }

    if (CurrentMatchPhase == NewPhase)
    {
        return;
    }

    CurrentMatchPhase = NewPhase;
    BroadcastMatchPhaseChanged();
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

void AWTBRGameState::OnRep_CurrentMatchPhase()
{
    BroadcastMatchPhaseChanged();
}

void AWTBRGameState::SetMatchWinner(APlayerState* WinnerPlayerState)
{
    if (!HasAuthority())
    {
        return;
    }

    if (MatchWinnerPlayerState == WinnerPlayerState)
    {
        return;
    }

    MatchWinnerPlayerState = WinnerPlayerState;
    BroadcastMatchWinnerChanged();
}

void AWTBRGameState::OnRep_MatchWinnerPlayerState()
{
    BroadcastMatchWinnerChanged();
}

void AWTBRGameState::BroadcastMatchModeRulesChanged()
{
    OnMatchModeRulesChanged.Broadcast(CurrentMatchMode, CurrentMatchRules);
}

void AWTBRGameState::BroadcastMatchPhaseChanged()
{
    OnMatchPhaseChanged.Broadcast(CurrentMatchPhase);
}

void AWTBRGameState::BroadcastMatchWinnerChanged()
{
    OnMatchWinnerChanged.Broadcast(MatchWinnerPlayerState);
}

void AWTBRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWTBRGameState, CurrentMatchMode);
    DOREPLIFETIME(AWTBRGameState, CurrentMatchRules);
    DOREPLIFETIME(AWTBRGameState, CurrentMatchPhase);
    DOREPLIFETIME(AWTBRGameState, MatchWinnerPlayerState);
}
