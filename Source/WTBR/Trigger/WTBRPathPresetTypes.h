// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WTBRPathPresetTypes.generated.h"

/**
 * Turn budget granted by ONE Viper in the weapon.
 *
 * A "turn" is an intermediate waypoint — the start and the end are where the lane
 * leaves the caster and where it lands, and neither is a course change. So a lane
 * may hold at most (budget + 2) waypoints.
 *
 * Viper alone is 4. Labyrn is Viper + Viper and therefore 8, which is the whole of
 * that composite's mechanical identity: merging buys path complexity nothing else
 * in the game can author. The rule is deliberately one the player can state in a
 * sentence — "one Viper, four turns" — rather than a per-weapon table.
 *
 * A hard ceiling rather than "unlimited" is itself a design choice. An unbounded
 * path is an unbounded LENGTH, which means unbounded flight time and a projectile
 * that must be speed-capped to keep it from tunnelling through thin geometry
 * between frames. Bounding the turns removes that entire class of problem.
 */
static constexpr int32 WTBR_TURNS_PER_VIPER = 4;

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
    // Unused whenever the caller passes a scatter radius, which every composite
    // does — ResolvePathPreset applies scatter OR this, never both.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    FVector FormationOffset = FVector::ZeroVector;

    // ⚠ PLAYTEST PENDING: seconds this lane waits before its cubes launch. Lets one
    // preset fire in waves — a first wave draws a shield, a later one arrives after
    // it drops. Lives on the shared lane struct on purpose: Serpveil, Fulgrix and
    // Venyx presets all use it, so one field covers every family.
    //
    // It does NOT mean the same thing everywhere. Venyx homes, so a late wave still
    // finds a target that moved: that is a punish. Viper cannot follow, so a late
    // Viper lane lands where the player GUESSED the enemy would be: that is zoning.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float LaunchDelay = 0.0f;

    // ⚠ PLAYTEST PENDING: fraction of the committed range within which a cube of
    // this lane acquires an enemy MID-FLIGHT and peels off to chase it. Zero leaves
    // the lane inert, which is what every non-Hound preset wants — the arc is then
    // just a path. Non-zero turns the arc into a search sweep, which is Hound's
    // whole identity: the shape decides what ground gets hunted.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HomingRadius = 0.0f;

    // ⚠ PLAYTEST PENDING: smallest the radius above may resolve to, in world units.
    //
    // A fraction of range is right for the SHAPE, but wrong on its own for the
    // radius: a short or uncharged shot then barely hunts at all. Measured in PIE
    // at range 1000 — the arcs passed 166-368uu from the target and the resolved
    // radii were 160-260uu, so cubes missed by as little as 6uu while behaving
    // exactly as designed.
    //
    // A floor fixes that end without touching the other: raising the fraction
    // instead would make a full-charge shot sweep a ~21 m radius. Ignored entirely
    // when HomingRadius is zero, so lanes that are not meant to hunt never start.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.0"))
    float HomingRadiusFloorUU = 400.0f;
};

/**
 * Per-cube launch settings that survive path resolution.
 *
 * ResolvePathPreset flattens every lane into one list of cube paths, which loses
 * the lane a cube came from — and delay and homing radius are authored per LANE.
 * This carries them across, already converted out of range fractions into the
 * world units the projectile wants.
 */
USTRUCT(BlueprintType)
struct FWTBRResolvedCubeLaunch
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Composite | Path")
    float DelaySeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Composite | Path")
    float HomingRadiusUU = 0.0f;
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
