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
