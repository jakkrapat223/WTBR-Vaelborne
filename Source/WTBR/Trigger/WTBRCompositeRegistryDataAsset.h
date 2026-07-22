// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "Trigger/WTBRTriggerDataAsset.h"

#include "WTBRCompositeRegistryDataAsset.generated.h"

class AWTBRProjectileBase;

UENUM(BlueprintType)
enum class EWTBRCompositeMovementInterruptPolicy : uint8
{
    None UMETA(DisplayName = "None"),
    RootTranslationCameraAimFree UMETA(DisplayName = "Root Translation, Camera/Aim Free"),
};

/** Generic homing settings for a composite definition. All values are PLAYTEST PENDING. */
USTRUCT(BlueprintType)
struct FWTBRCompositeHomingParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: disabled by default until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing")
    bool bEnableHoming = false;

    // ⚠ PLAYTEST PENDING: zero means no target acquisition range is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float TargetAcquisitionRange = 0.0f;

    // ⚠ PLAYTEST PENDING: zero means no homing acceleration is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float HomingAcceleration = 0.0f;

    // ⚠ PLAYTEST PENDING: mirrors Venyx's default target-acquisition aim cone.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float AimConeHalfAngleDegrees = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing", meta = (ClampMin = "1", ClampMax = "8"))
    int32 SimultaneousTargetCount = 1;

    // ⚠ PLAYTEST PENDING: zero means no maximum homing speed is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing", meta = (ClampMin = "0.0"))
    float MaximumHomingSpeed = 0.0f;
};

/** Generic explosion settings for a composite definition. All values are PLAYTEST PENDING. */
USTRUCT(BlueprintType)
struct FWTBRCompositeExplosionParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: disabled by default until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    bool bExplodes = false;

    // ⚠ PLAYTEST PENDING: zero means no explosion radius is configured.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float ExplosionRadius = 0.0f;

    // ⚠ PLAYTEST PENDING: disabled by default — first blast behaves exactly as before.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    bool bHasSecondBlast = false;

    // ⚠ PLAYTEST PENDING: delay in seconds between the first and second blast (the "telegraph").
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float SecondBlastDelay = 0.0f;

    // ⚠ PLAYTEST PENDING: radius of the second (wide/weak) blast.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float SecondBlastRadius = 0.0f;

    // ⚠ PLAYTEST PENDING: fraction of TotalDamageBudget allocated to the second blast.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SecondBlastDamageRatio = 0.0f;

    // ⚠ PLAYTEST PENDING: disabled by default — sphere explosion behaves exactly as before.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    bool bIsShapedCharge = false;

    // ⚠ PLAYTEST PENDING: half-angle of the forward splash cone (only used when shaped-charge).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float ShapedChargeConeHalfAngleDegrees = 45.0f;

    // ⚠ PLAYTEST PENDING: neutral falloff shape for a future configured explosion.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion", meta = (ClampMin = "0.0"))
    float DamageFalloffExponent = 1.0f;
};

// Sentinel for AWTBRCharacter::GetPendingCompositePresetIndex(): this shot is a
// TAP and must fly straight. Distinct from INDEX_NONE, which means "no
// ready-composite flow set anything" and falls back to the definition's own
// authored PathPreset — that separation is what keeps direct FireComposite calls
// (legacy paths, automation fixtures) behaving exactly as they did before the
// Tap/Hold flow existed.
static constexpr int32 WTBR_COMPOSITE_PRESET_TAP = -2;

/** One DataAsset-authored composite recipe. Values are intentionally unbalanced placeholders. */
USTRUCT(BlueprintType)
struct FWTBRCompositeDefinition
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING: identifies the composite produced by this definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRCompositeBulletType CompositeType = EWTBRCompositeBulletType::None;

    // ⚠ PLAYTEST PENDING: the first source archetype is authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRBulletArchetype RequiredArchetypeA = EWTBRBulletArchetype::None;

    // ⚠ PLAYTEST PENDING: the second source archetype is authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    EWTBRBulletArchetype RequiredArchetypeB = EWTBRBulletArchetype::None;

    // ⚠ PLAYTEST PENDING: no projectile is assigned until an owner authors a registry asset.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    TSubclassOf<AWTBRProjectileBase> ProjectileClass = nullptr;

    // ⚠ PLAYTEST PENDING: selected concrete behavior is authored by the registry asset.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    TSubclassOf<UWTBRCompositeBehaviorBase> BehaviorClass = nullptr;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Balance", meta = (ClampMin = "0.0"))
    float TotalDamageBudget = 0.0f;

    // ⚠ PLAYTEST PENDING: mirrors per-archetype projectile speed conventions.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Balance", meta = (ClampMin = "100.0"))
    float ProjectileSpeed = 2500.0f;

    // ⚠ PLAYTEST PENDING: enables character pass-through for composites such as Dualux.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Definition")
    bool bCanPenetrate = false;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Balance", meta = (ClampMin = "0.0"))
    float VaelCost = 0.0f;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Timing", meta = (ClampMin = "0.0"))
    float MergeTime = 0.0f;

    // ⚠ PLAYTEST PENDING: balance is authored per composite definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Timing", meta = (ClampMin = "0.0"))
    float CompositeCooldown = 0.0f;

    // ⚠ PLAYTEST PENDING: no movement restriction is applied until a definition opts in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Movement")
    EWTBRCompositeMovementInterruptPolicy MovementInterruptPolicy =
        EWTBRCompositeMovementInterruptPolicy::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path")
    FWTBRPathPreset PathPreset;

    // ⚠ PLAYTEST PENDING: scales PathPreset's NormalizedWaypoints into world-space distance —
    // passed as ResolvePathPreset's Range parameter. This is the FULL-charge reach.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "0.0"))
    float PathRange = 1000.0f;

    // ⚠ PLAYTEST PENDING: how many cubes this composite conjures, for BOTH tap and
    // hold. One number on purpose — "set 8, get 8" either way, so the player never
    // has to learn that one mode quietly spawns a different volley than the other.
    //
    // Tap spends them on a straight converging volley. Hold spreads them across the
    // chosen preset's lanes, using each lane's own CubeCount as a WEIGHT rather than
    // a literal count, so an authored 3:1 split stays 3:1 at any total.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "1"))
    int32 CubeCount = 5;

    // ⚠ PLAYTEST PENDING: seconds a READY composite survives before it is discarded
    // with NO refund. Holding a loaded composite is meant to cost something — long
    // enough to find cover or wait out a peek, short enough that it cannot be
    // carried around all match as a free standing threat. Zero disables expiry.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Timing", meta = (ClampMin = "0.0"))
    float ReadyCompositeHoldSeconds = 8.0f;

    // ⚠ PLAYTEST PENDING: how far apart the cubes are conjured around the caster,
    // for BOTH tap and hold, matching Serpveil's own SerpveilScatterRadius.
    //
    // This is not only cosmetic. Spawning every cube of a volley on one point makes
    // them overlap and destroy each other on contact — which is exactly what broke
    // hold-fired composites until they were given the same spread tap already had.
    // Keep it comfortably above the projectile's own collision extent.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "0.0"))
    float TapScatterRadius = 135.0f;

    // ⚠ PLAYTEST PENDING: the UNCHARGED reach — what a tap fires at, and where a hold
    // starts before charging. A held composite lerps this -> PathRange by charge,
    // mirroring Serpveil's own SerpveilMaxRange -> SerpveilPresetMaxRange.
    //
    // Tap deliberately uses this rather than the full PathRange, so a barely-charged
    // hold is never WORSE than just tapping (the design lock's "MinReach =
    // BasicRange, MaxReach > BasicRange"). Tap trades reach for being instant.
    // Values <= 0 mean "no charge scaling" and every shot uses PathRange.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Path", meta = (ClampMin = "0.0"))
    float PathRangeMin = 0.0f;

    // ⚠ PLAYTEST PENDING: generic homing values are authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Homing")
    FWTBRCompositeHomingParams HomingParams;

    // ⚠ PLAYTEST PENDING: generic explosion values are authored per definition.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Explosion")
    FWTBRCompositeExplosionParams ExplosionParams;

    // Per-composite look, applied to the spawned projectile at fire time exactly
    // like the Sniper DAs already do (see UWTBRSniperTrigger's ApplyVFXConfig
    // call). This is what lets every composite share ONE projectile Blueprint:
    // damage/speed/explosion/path are already runtime-driven from this struct, so
    // the only thing a per-composite Blueprint could still carry was its visuals
    // — and now those live here too, next to the balance they belong with.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | VFX")
    FWTBRProjectileVFXConfig VFX;
};

/** Central, DataAsset-authored source of composite definitions. */
UCLASS(BlueprintType)
class WTBR_API UWTBRCompositeRegistryDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Empty by default; real definitions are authored after the vertical slice validates this shape.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Composite | Registry")
    TArray<FWTBRCompositeDefinition> Definitions;

    /** Resolves an unordered pair, or None when the pair is invalid or unregistered. */
    UFUNCTION(BlueprintPure, Category = "Composite | Registry")
    EWTBRCompositeBulletType ResolveCompositeType(
        EWTBRBulletArchetype ArchetypeA,
        EWTBRBulletArchetype ArchetypeB) const;

    /** Finds the full definition for an already-resolved Type. False if Type is None or unregistered. */
    UFUNCTION(BlueprintPure, Category = "Composite | Registry")
    bool FindDefinition(EWTBRCompositeBulletType Type, FWTBRCompositeDefinition& OutDefinition) const;

    // Evenly-spaced point on a sphere via the golden-angle spiral. Deterministic
    // by design — server and client must agree, and it keeps the scatter
    // automation-testable. Returns ZeroVector when Total < 2.
    static FVector ComputeFibonacciSphereOffset(int32 Index, int32 Total, float Radius);

    // Resolves a preset into one world-space waypoint list per (lane, cube) pair.
    //
    // ScatterRadius > 0 replaces the per-cube FormationOffset line-up with a
    // Fibonacci sphere centred on SpawnOrigin (Viper's around-the-body split).
    // It is opt-in and defaults OFF so the composite behaviours that share this
    // resolver — Coilvyn, Ignivex, Solveil — keep their existing layout
    // untouched until Composite Bullet ships.
    // TotalCubeOverride > 0 redistributes that many cubes across the preset's lanes,
    // treating each lane's authored CubeCount as a relative weight. Zero (the
    // default) uses the authored counts literally, which is what Serpveil's own
    // presets rely on — composites pass their single Definition.CubeCount instead.
    // OutCubeLaunches, when supplied, receives one entry per emitted path carrying
    // that cube's authored launch delay and its homing radius already scaled out of
    // range fractions into world units. It is a separate array rather than a field
    // on the path because the paths themselves are plain point lists shared with
    // callers that do not care.
    // MaxTurns caps how many intermediate waypoints a lane may steer through; zero
    // means uncapped, which is what every non-Viper family wants. See
    // WTBR_TURNS_PER_VIPER, and ComputeTurnBudget below for the composite rule.
    static void ResolvePathPreset(
        const FWTBRPathPreset& Preset,
        const FVector& SpawnOrigin,
        const FRotator& AimRotation,
        float Range,
        TArray<TArray<FVector>>& OutCubeWorldPaths,
        float ScatterRadius = 0.0f,
        bool bIsMainSlot = true,
        int32 TotalCubeOverride = 0,
        TArray<FWTBRResolvedCubeLaunch>* OutCubeLaunches = nullptr,
        int32 MaxTurns = 0);

    /**
     * Drops the turns a lane is not entitled to, keeping the first MaxTurns of them.
     *
     * The FINAL waypoint always survives. It is where the lane lands, so discarding
     * it would quietly shorten the shot's reach — the player would lose range as a
     * side effect of drawing too many corners, which is not what the cap is for.
     * They lose the extra corners and nothing else.
     *
     * MaxTurns <= 0 passes the lane through untouched.
     */
    static void ClampLaneTurns(
        const TArray<FVector>& InWaypoints, int32 MaxTurns, TArray<FVector>& OutWaypoints);

    /**
     * Turn budget for a composite: WTBR_TURNS_PER_VIPER per Serpveil that went into
     * it. Labyrn is Viper + Viper, so it resolves to 8 without being named here —
     * the rule is read off the definition's own archetypes rather than hardcoded per
     * composite, so authoring a new Viper pairing gets the right budget for free.
     *
     * Returns 0 (uncapped) for composites with no Viper in them. Their presets are
     * not Viper presets and the cap has nothing to say about them.
     */
    static int32 ComputeTurnBudget(const FWTBRCompositeDefinition& Definition);

    /**
     * True when this composite fires ONE projectile instead of splitting into a
     * volley.
     *
     * Canon rule: an Asteroid or Meteor component makes the merged round a single
     * mass — UNLESS a Viper went into it, because Viper is the archetype that
     * splits, and the anime shows a Meteor+Viper merge dividing into a grid before
     * it flies.
     *
     * So: single when it contains Solux or Fulgrix AND no Serpveil. Read off the
     * definition's own archetypes rather than listed per composite, so authoring a
     * new pairing gets the right answer without touching this code.
     */
    static bool FiresSingleProjectile(const FWTBRCompositeDefinition& Definition);
};
