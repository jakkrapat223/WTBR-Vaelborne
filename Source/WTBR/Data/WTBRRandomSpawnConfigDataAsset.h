// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WTBRRandomSpawnConfigDataAsset.generated.h"

// Per-LEVEL random spawn area config (TeamThree15P/BR). Deliberately separate
// from FWTBRMatchModeRules — the play area belongs to the LEVEL, not the mode
// ruleset, since the same mode can run on maps of very different sizes (a
// stock graybox today, ~1x1/3x3 km real maps later). Assign one of these per
// level on AWTBRGameMode::RandomSpawnConfig. If left unset,
// AWTBRGameMode::ApplyRandomSpawnPositions skips random spawn entirely (logs
// a warning) and falls back to normal PlayerStart placement rather than
// guessing map-inappropriate defaults.
UCLASS(BlueprintType)
class WTBR_API UWTBRRandomSpawnConfigDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Spawn")
    FVector SpawnAreaCenter = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Spawn", meta = (ClampMin = "0.0"))
    float SpawnAreaRadius = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Spawn", meta = (ClampMin = "0.0"))
    float MinSpawnDistance = 80.0f;

    // Optional map-authored ground-safe points. When populated, the GameMode
    // chooses among these instead of sampling the whole disc, so a combatant
    // cannot be placed on a roof, inside a building, or off the NavMesh.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Spawn | Safe Areas")
    TArray<FVector> SafeSpawnAnchors;
};
