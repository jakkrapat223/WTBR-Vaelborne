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

void AWTBRGameState::AddTeamKillPoint(int32 TeamId)
{
    if (!HasAuthority() || TeamId == INDEX_NONE)
    {
        return;
    }

    FWTBRTeamScore& Score = FindOrAddTeamScore(TeamId);
    ++Score.KillPoints;
    OnTeamScoresChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("WTBR Team Score: Team %d kill point -> Kills=%d Total=%d."),
        TeamId, Score.KillPoints, Score.GetTotalScore());
}

void AWTBRGameState::AddTeamSurvivorBonus(int32 TeamId, int32 BonusPoints)
{
    if (!HasAuthority() || TeamId == INDEX_NONE || BonusPoints <= 0)
    {
        return;
    }

    FWTBRTeamScore& Score = FindOrAddTeamScore(TeamId);
    Score.SurvivorBonus += BonusPoints;
    OnTeamScoresChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("WTBR Team Score: Team %d survivor bonus +%d -> Total=%d."),
        TeamId, BonusPoints, Score.GetTotalScore());
}

void AWTBRGameState::ResetTeamScores()
{
    if (!HasAuthority())
    {
        return;
    }

    if (TeamScores.Num() == 0 && WinningTeamId == INDEX_NONE)
    {
        return;
    }

    TeamScores.Reset();
    OnTeamScoresChanged.Broadcast();
    SetWinningTeamId(INDEX_NONE);
}

void AWTBRGameState::SetWinningTeamId(int32 NewWinningTeamId)
{
    if (!HasAuthority())
    {
        return;
    }

    if (WinningTeamId == NewWinningTeamId)
    {
        return;
    }

    WinningTeamId = NewWinningTeamId;
    OnWinningTeamChanged.Broadcast(WinningTeamId);
}

FWTBRTeamScore AWTBRGameState::GetTeamScore(int32 TeamId) const
{
    for (const FWTBRTeamScore& Score : TeamScores)
    {
        if (Score.TeamId == TeamId)
        {
            return Score;
        }
    }

    FWTBRTeamScore Empty;
    Empty.TeamId = TeamId;
    return Empty;
}

FWTBRTeamScore& AWTBRGameState::FindOrAddTeamScore(int32 TeamId)
{
    for (FWTBRTeamScore& Score : TeamScores)
    {
        if (Score.TeamId == TeamId)
        {
            return Score;
        }
    }

    FWTBRTeamScore& NewScore = TeamScores.AddDefaulted_GetRef();
    NewScore.TeamId = TeamId;
    return NewScore;
}

void AWTBRGameState::OnRep_TeamScores()
{
    OnTeamScoresChanged.Broadcast();
}

void AWTBRGameState::OnRep_WinningTeamId()
{
    OnWinningTeamChanged.Broadcast(WinningTeamId);
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
    DOREPLIFETIME(AWTBRGameState, TeamScores);
    DOREPLIFETIME(AWTBRGameState, WinningTeamId);
}
