// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WTBRPathPresetTypes.generated.h"

/** One lane of a path preset: a shape plus its projectile formation. */
USTRUCT(BlueprintType)
struct FWTBRPathLane
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: fractions of Range in aim-local space (X forward, Y lateral, Z vertical).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    TArray<FVector> NormalizedWaypoints;

    // ⚠ PLAYTEST PENDING: number of projectiles spawned along this lane.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "1"))
    int32 CubeCount = 1;

    // ⚠ PLAYTEST PENDING: per-cube aim-local formation step in absolute units.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    FVector FormationOffset = FVector::ZeroVector;
};

/** A named, authorable multi-lane path shape. */
USTRUCT(BlueprintType)
struct FWTBRPathPreset
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    FName PresetId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    TArray<FWTBRPathLane> Lanes;
};
