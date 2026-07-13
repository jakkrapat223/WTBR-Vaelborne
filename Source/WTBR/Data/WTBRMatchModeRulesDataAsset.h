// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WTBRMatchModeRulesDataAsset.generated.h"

UENUM(BlueprintType)
enum class EWTBRMatchMode : uint8
{
    None UMETA(DisplayName="None"),
    ThreeVThree UMETA(DisplayName="3v3"),
    RankWar UMETA(DisplayName="Rank War"),
    TeamThree15P UMETA(DisplayName="15 Players Team-3"),
    BattleRoyale UMETA(DisplayName="Battle Royale"),
};

USTRUCT(BlueprintType)
struct FWTBRMatchModeRules
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
    EWTBRMatchMode MatchMode = EWTBRMatchMode::ThreeVThree;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode", meta=(ClampMin="0"))
    int32 TeamSize = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode", meta=(ClampMin="0"))
    int32 MaxPlayers = 6;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Economy")
    bool bEnablePassiveVaelRegen = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Economy", meta=(ClampMin="0.0"))
    float VaelRegenPerSecond = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Loot")
    bool bAllowCorpseLoot = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Loot")
    bool bAllowTriggerPickup = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Loot")
    bool bUseCorpseLootContainerOnDeath = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Loadout")
    bool bAllowTriggerSwapDuringMatch = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Loadout")
    bool bAllowLoadoutEditDuringMatch = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Teams")
    bool bUseTeams = true;

    // When true the GameMode block-fills every AWTBRCharacter into teams of
    // TeamSize at InMatch entry. Off by default so the 1v1 test harness and the
    // legacy individual round loop keep their exact behavior (characters stay at
    // TeamId INDEX_NONE and the individual winner path runs).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Teams")
    bool bAssignTeamsAtMatchStart = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Teams")
    bool bAllowFriendlyFire = false;

    // TeamThree15P: 1 kill = 1 team point, last surviving team gets +1 per member
    // still alive, and the WINNER is the team with the highest total — a wiped-out
    // team with more kills can beat the survivors. When false (BR), the surviving
    // team simply wins.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Teams")
    bool bScoreBasedTeamWinner = false;

    // Match time limit for team modes (design lock 2026-07-13): when set (>0),
    // the round force-ends this many seconds into InMatch even if MORE than one
    // team still has a living member (e.g. 3 teams left) — it does NOT wait for
    // last-team-standing. Scoring at that point works exactly as a natural round
    // end: every team with a living member gets its survivor bonus, and (when
    // bScoreBasedTeamWinner) the highest total still wins. 0 = no time limit (BR
    // relies on the zone instead, once implemented). Placeholder value on
    // TeamThree15P — exact duration TBD via playtest.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Teams", meta=(ClampMin="0.0"))
    float MatchTimeLimitSeconds = 0.0f;

    // When true, AWTBRGameMode scatters every combatant to a random point (with
    // minimum spacing — see AWTBRGameMode::RandomSpawnAreaRadius/MinSpawnDistance,
    // which are per-map GameMode knobs, not per-mode-ruleset) at InMatch entry
    // instead of using normal PlayerStart placement. Off by default so the 1v1
    // harness and legacy modes keep exact existing spawn behavior.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode | Spawn")
    bool bUseRandomSpawnPositions = false;
};

UCLASS(BlueprintType)
class WTBR_API UWTBRMatchModeRulesDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
    EWTBRMatchMode MatchMode = EWTBRMatchMode::ThreeVThree;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | Match Mode")
    FWTBRMatchModeRules Rules;
};
