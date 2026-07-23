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

/**
 * Waypoint ceiling for PLAYER-AUTHORED lanes specifically (the Preset Editor).
 *
 * Not the same rule as WTBR_TURNS_PER_VIPER above — that is a Viper/composite
 * design rule about how much path complexity a merge buys. Standalone hold-presets
 * (Venyx today) apply no turn budget at all: FirePresetVolley never passes MaxTurns
 * to ResolvePathPreset, because every existing preset is C++-authored and trusted.
 *
 * The editor is the first place an untrusted, player-controlled waypoint array can
 * reach ResolvePathPreset, and every waypoint becomes a real InterpToMovement
 * control point — a per-tick cost, not just a data-size one. This exists purely as
 * an anti-abuse / performance ceiling, chosen to sit under Labyrn's own ceiling
 * (4 turns x 2 Viper + start/end = 10). Not a playtested balance number.
 */
static constexpr int32 WTBR_MAX_CUSTOM_LANE_WAYPOINTS = 8;

/**
 * What a cube does when it reaches a point the player marked on the lane.
 *
 * The player does not type a percentage — they click the drawn line where they want
 * something to happen. Stored as a fraction of the lane's length so one preset reads
 * the same at every charge level, exactly like the waypoints themselves.
 *
 * Splitting and launch delay are deliberately NOT here: both belong to the START of
 * the shot, and both already exist as FWTBRPathPreset::CubeCount and
 * FWTBRPathLane::LaunchDelay. Keeping mid-flight events to things that change a cube
 * already in the air is also what stops actor counts multiplying.
 */
UENUM(BlueprintType)
enum class EWTBRLaneEventType : uint8
{
    // Stop dead for DurationSeconds, then carry on along the same path. Canon's
    // "tracking set by time" trick and the bait half of a two-wave shot.
    Hover     UMETA(DisplayName = "Hover in place"),

    // Turn hunting on or off from this point. Off until late is what makes a shot
    // fly straight and only commit at the end.
    //
    // HOUND LINE ONLY — Venyx and the composites built from it. Solux carries no
    // per-cube property, Meteo explodes, and Viper controls its trajectory instead;
    // none of them have hunting to switch. The marker cannot grant it either, so on
    // those archetypes it does nothing and says so in the log.
    SetHoming UMETA(DisplayName = "Enable / disable homing (Hound line only)"),

    // Scale cruise speed from this point on. Slow at the end reads as an arrival a
    // target can answer; fast at the end reads as a snap.
    SetSpeed  UMETA(DisplayName = "Change speed"),
};

/** One player-placed marker on a lane. */
USTRUCT(BlueprintType)
struct FWTBRLaneEvent
{
    GENERATED_BODY()

    // Where on the lane, 0 at the muzzle and 1 at the authored end.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AtPathFraction = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    EWTBRLaneEventType Type = EWTBRLaneEventType::Hover;

    // ⚠ PLAYTEST PENDING. Hover only: seconds to hang before continuing.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float DurationSeconds = 0.0f;

    // SetHoming only.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    bool bEnable = true;

    // SetSpeed only: multiplier on the shot's speed from here on.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path",
        meta = (ClampMin = "0.05", ClampMax = "4.0"))
    float SpeedMultiplier = 1.0f;
};

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

    // Markers the player placed along this lane. Empty is the normal case.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    TArray<FWTBRLaneEvent> Events;
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

    // Copied from the lane this cube came from. Fractions are resolved into real
    // distances by the projectile, which is the only thing that knows how long its
    // own path turned out to be.
    UPROPERTY(BlueprintReadOnly, Category = "Composite | Path")
    TArray<FWTBRLaneEvent> Events;
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

    // ⚠ PLAYTEST PENDING: how many cubes this preset spends its damage across.
    //
    // Zero uses each lane's own authored CubeCount literally. Non-zero redistributes
    // this many across the lanes, treating the authored numbers as weights — so an
    // authored 3:1 split stays 3:1 whatever total is asked for.
    //
    // The damage BUDGET does not move, so this is a coverage-versus-punch dial and
    // not a power one: more cubes means each carries less. That is what keeps it
    // inside the lock that hold changes the pattern and nothing else.
    //
    // Clamped server-side to the firing archetype's own floor and ceiling — a preset
    // may never split into FEWER cubes than the archetype already does on tap.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "0"))
    int32 CubeCount = 0;
};
