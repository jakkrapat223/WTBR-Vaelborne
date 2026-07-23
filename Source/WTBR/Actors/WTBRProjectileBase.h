// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "VFX/WTBRProjectileVFXTypes.h"
#include "Components/WTBRHealthComponent.h"
#include "WTBRProjectileBase.generated.h"

class UBoxComponent;
class UProjectileMovementComponent;
class UInterpToMovementComponent;
class UMaterialInterface;
class USceneComponent;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;
class UWTBRCoreStatsDataAsset;

UENUM(BlueprintType)
enum class EProjectileState : uint8
{
    Active    UMETA(DisplayName = "Active"),
    Exploded  UMETA(DisplayName = "Exploded"),
    Destroyed UMETA(DisplayName = "Destroyed"),
};

UCLASS(BlueprintType, Blueprintable)
class WTBR_API AWTBRProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    AWTBRProjectileBase();

    // ── Components ─────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UBoxComponent> BoxCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "WTBR | Projectile | Components")
    TObjectPtr<UInterpToMovementComponent> InterpMovement;

    // ── Config (set via InitializeProjectile) ──────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float BaseDamage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float MaxRange = 3000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    ETriggerCategory OwnerCategory = ETriggerCategory::None;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bIsCubeSplit = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    int32 CubeSplitCount = 4;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bIsSniper = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bExplodeOnImpact = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ExplosionRadius = 0.0f;

    // When true the bullet passes through characters but stops at geometry (Sniper only)
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bCanPenetrate = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bHasDelayedSecondExplosion = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float SecondExplosionDelay = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float SecondExplosionRadius = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float SecondExplosionDamage = 0.0f;

    // Coilvyn: if true, OnInterpMovementEnd transitions to homing instead of destroying.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bHomeAfterPathEnd = false;

    // Target acquired at cast time; it is never re-queried at path end.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    TWeakObjectPtr<USceneComponent> PathEndHomingTarget;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float PathEndHomingAcceleration = 0.0f;

    // Direction from the resolved path's final segment before homing curves its flight.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    FVector PathEndContinueDirection = FVector::ForwardVector;

    // Venyx (Hound): unlike bHomeAfterPathEnd above, nothing is chosen when the shot
    // is fired. A non-zero radius makes the cube sweep for enemies as it flies its
    // path and peel off at the first one it passes near. The arc is a search pattern,
    // not an approach to somebody already picked.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ProximityHomingRadius = 0.0f;

    // ProximityHomingAcceleration was REMOVED. Steering stopped being acceleration
    // toward a target when the turn cap went in — a chasing cube now rotates its
    // heading at a bounded rate and holds a constant speed, so an acceleration value
    // had nothing left to act on. It survived as a field nothing read, which is the
    // kind of dead knob someone eventually tunes and wonders why nothing happens.

    // Radius to restore when a cube is allowed to hunt again after overshooting.
    // Zero means a miss is final, which is Venyx's rule.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ReacquireRadius = 0.0f;

    // Degrees per second the cube may swing its heading while chasing. Zero means
    // uncapped, which is UProjectileMovementComponent's own homing behaviour.
    //
    // A cap is what separates a chase from a yo-yo. Uncapped, a cube that overshot
    // simply pivoted in place and came straight back, over and over until it expired
    // — the owner saw four cubes doing laps around one target. Capped, an overshoot
    // costs a real arc to correct, so the cube either comes back around the long way
    // or loses the target entirely.
    //
    // Turn radius is roughly Speed / TurnRate(radians): at 3500uu/s and 180 deg/s
    // that is about 1100uu, a visibly wide sweep rather than a pivot. The wide
    // return arc is also what Venyx presets are being designed around, so this
    // number is a design knob, not just a safety rail.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float HomingTurnRateDegreesPerSecond = 0.0f;

    // Proximity fuse for a committed chase. Zero disables the whole capability (a cube
    // must then land a real overlap to deal damage, the old behaviour) AND opts the
    // weapon into the relentless drop-and-reacquire sweep instead.
    //
    // A fast cube with a bounded turn rate physically cannot pinpoint-collide a target
    // closer than its own turn radius (~Speed / TurnRate(radians)): it flies past, and
    // then either loops in place forever or spirals outward reacquiring — never
    // actually connecting. Owner saw exactly this in a PIE log: Venspire cubes
    // acquired a stationary target at ~250uu, overshot, and re-acquired at 269 → 310 →
    // 365uu, drifting out until they expired having touched nobody.
    //
    // Non-zero fixes that in two ways, both in TickProximityChase:
    //   1. Early fuse — a cube that closes inside THIS radius detonates at once (the
    //      tightest airburst, for a head-on approach).
    //   2. Closest approach — at the overshoot moment (the closest the cube will get
    //      on this pass) a committed chase detonates if it is still within the radius
    //      it ACQUIRED at (ArmedHomingRadius), so a glancing approach that never
    //      reaches the tight fuse still resolves into exactly one hit instead of
    //      spiralling. The exact fuse value therefore only tunes how tight the airburst
    //      is, not whether an acquired chase connects.
    // LOS is required to acquire and the cube still stops on geometry, so cover
    // remains the counterplay.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ProximityDetonationRadius = 0.0f;

    /**
     * Rotates Current toward Desired by at most MaxRadians. Both must be normalized;
     * returns a normalized direction.
     *
     * Pure vector maths on purpose — steering is the part worth testing and it needs
     * no world, no physics scene and no frames to check.
     */
    static FVector SteerTowards(const FVector& Current, const FVector& Desired, float MaxRadians);

    // Test seams. Lane events are armed from inside InitializePathMovement, which is
    // the only place the path's real length is known, so a test needs a way in ahead
    // of it and a way to read the queue afterwards.
    void SetAuthoredLaneEventsForTest(const TArray<FWTBRLaneEvent>& Events)
    {
        AuthoredLaneEvents = Events;
    }
    int32 GetPendingLaneEventCountForTest() const { return PendingLaneEvents.Num(); }
    float GetNextLaneEventDistanceForTest() const
    {
        return PendingLaneEventDistances.Num() > 0 ? PendingLaneEventDistances[0] : -1.0f;
    }

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    bool bIsShapedCharge = false;

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float ShapedChargeConeHalfAngleDegrees = 45.0f;

    // Which way the shaped-charge cone points. Set once at fire time and never
    // changed, rather than read back from GetActorForwardVector().
    //
    // The actor's own facing is not a safe source: a path-driven projectile keeps
    // whatever rotation it spawned with (InterpToMovement translates, it does not
    // turn the actor), and even a launched one only faces its travel direction by
    // side effect. Worse, the cone is evaluated at the instant of impact, when the
    // projectile and its target are practically overlapping — so the vector to the
    // target is nearly arbitrary and a fractional difference in impact position
    // flips the dot product past the threshold. That made damage land or not land
    // depending on which machine fired the shot.
    //
    // Zero falls back to the old actor-forward behaviour for any caller that has
    // not opted in.
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    FVector ShapedChargeDirection = FVector::ZeroVector;

    // Ventryx (Black Trigger) — non-zero launches hit character away from impact point
    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    float KnockbackForce = 0.0f;

    float BleedDamagePerTick = 0.0f;
    float BleedDuration = 0.0f;
    float ShieldBrittleDamageMultiplier = 1.0f;
    float ShieldBrittleDuration = 0.0f;

    void ConfigureOnHitEffects(float InBleedDamagePerTick, float InBleedDuration,
        float InShieldBrittleDamageMultiplier, float InShieldBrittleDuration);

    UPROPERTY(BlueprintReadOnly, Category = "WTBR | Projectile | Config")
    TObjectPtr<AActor> OwnerInstigator;

    // Actors already damaged this flight — prevents re-hit on re-entry (penetration)
    UPROPERTY()
    TArray<TObjectPtr<AActor>> DamagedActors;

    // ── State (Replicated) ─────────────────────────────────────────────────────
    UPROPERTY(ReplicatedUsing = OnRep_ProjectileState, BlueprintReadOnly,
        Category = "WTBR | Projectile | State")
    EProjectileState ProjectileState = EProjectileState::Active;

    // ── Core Interface ─────────────────────────────────────────────────────────
    // Called by spawning Trigger immediately after SpawnActorDeferred
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void InitializeProjectile(float InDamage, float InSpeed,
        ETriggerCategory InCategory, bool bSniper,
        bool bExplode, float ExplodeRadius);

    // Called after FinishSpawning — sets velocity and lifespan
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void Launch(FVector Direction, AActor* InInstigator);

    // Enable homing after Launch() — call only on authority
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void EnableHoming(USceneComponent* Target, float Accel);

    virtual void Tick(float DeltaSeconds) override;

    // Serpveil curved path — deactivates ProjectileMovement, sets InterpToMovement control points
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void InitializePathMovement(const TArray<FVector>& Points, float Speed, AActor* InInstigator);

    // Same as InitializePathMovement, but the cube hovers inert at its path start for
    // DelaySeconds first. A whole volley can then leave in waves from one shot.
    // Deliberately on the projectile rather than the firing behaviour: this is an
    // actor with its own timer manager, so a wave cannot be orphaned by the
    // behaviour UObject going away underneath it.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void SchedulePathMovement(const TArray<FVector>& Points, float Speed,
        AActor* InInstigator, float DelaySeconds);

    /**
     * Labyrn — makes the cube ease off over the last stretch of its path instead of
     * flying it at one flat speed.
     *
     * InterpToMovement gives each segment a slice of the total time proportional to
     * its LENGTH, which is what makes the speed constant. Rewriting those slices
     * after the path is finalised buys a speed profile without replacing the whole
     * movement component: give the closing segments a bigger share of the time and
     * the cube arrives slowly enough to be seen and answered.
     *
     * SlowFromFraction is where the ease-off begins (0.75 = the last quarter).
     * EndSpeedFactor is the speed at the very end, relative to the cruise speed.
     * Call BEFORE the path starts. Zero disables it.
     */
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void SetPathSpeedProfile(float SlowFromFraction, float EndSpeedFactor);

    // Venyx — arms mid-flight acquisition. Must be called before the path starts,
    // which is what begins the sweep ticking.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void EnableProximityHoming(float RadiusUU, float TurnRateDegPerSec = 0.0f,
        bool bReacquireAfterOvershoot = false, float DetonationRadiusUU = 0.0f);

    /**
     * Spawns one cube per resolved path, in waves, each hunting on proximity.
     *
     * Shared by the Venyx COMPOSITES and the standalone Venyx Trigger, which have
     * no type in common — composites carry an FWTBRCompositeDefinition, the Trigger
     * carries a DataAsset — so this takes the handful of values both can supply
     * rather than either of their structs. Returns the number of cubes spawned.
     */
    static int32 SpawnSweptVolley(
        AWTBRCharacter* OwningCharacter,
        TSubclassOf<AWTBRProjectileBase> ProjectileClass,
        float TotalDamageBudget,
        float Speed,
        bool bExplodes,
        float ExplosionRadius,
        const FRotator& SpawnRotation,
        const TArray<TArray<FVector>>& CubeWorldPaths,
        const TArray<FWTBRResolvedCubeLaunch>& CubeLaunches,
        const FWTBRProjectileVFXConfig* VFXConfig = nullptr,
        float HomingTurnRateDegPerSec = 0.0f,
        float MaxRangeUU = 0.0f,
        bool bReacquireAfterOvershoot = false,
        float DetonationRadiusUU = 0.0f,
        // Weapon-supplied hunting radius. Non-zero arms EVERY cube with it and
        // overrides the per-lane radius the resolver produced — the Preset Editor
        // stopped authoring hunting range per lane, so the gun supplies it instead.
        // Zero keeps the old per-lane behaviour, which is what composites still use.
        float OverrideHomingRadiusUU = 0.0f);

    bool IsWaitingToLaunchForTest() const
    {
        return GetWorldTimerManager().IsTimerActive(DelayedLaunchTimer);
    }

    // Exposed for tests: the acquisition rule with no world or physics needed —
    // alive, hostile, inside the radius, nearest wins. Line of sight is NOT checked
    // here because it needs a real trace; the tick applies it on top.
    static AWTBRCharacter* FindProximityHomingTarget(
        const AWTBRCharacter* Shooter,
        const FVector& CubeLocation,
        float RadiusUU,
        const TArray<AWTBRCharacter*>& Candidates);

    // ── VFX Hooks ──────────────────────────────────────────────────────────────
    // Optional in-flight trail (e.g. Egret/Ibis/Lightning's visible bullet-light
    // counterplay — owner's replacement for the GDD's original "Glint"
    // pre-warning-the-target concept, dropped 2026-07-17 as too intrusive; see
    // wtbr-project-state memory for the design discussion). Null by default =
    // no trail, zero visual change for every existing projectile. Spawned in
    // BeginPlay (not gated on HasAuthority) so it renders on every machine the
    // actor replicates to, not just the server. Per-weapon BP subclasses (e.g.
    // BP_Telorn_Projectile) set
    // their own asset as a class default; there is deliberately no DataAsset
    // plumbing for this — it is a visual choice tied to the projectile class,
    // not a balance value.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TObjectPtr<UNiagaraSystem> TrailEffect;

    // Data Asset-driven VFX. Kept opt-in so existing Blueprint event graphs
    // continue to work until a weapon is explicitly migrated.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TObjectPtr<UNiagaraSystem> DefaultImpactEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TArray<FWTBRSurfaceImpactVFX> SurfaceImpactOverrides;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TArray<FWTBRNiagaraAssetParameter> ImpactAssetParameters;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    bool bUseBuiltInImpactVFX = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX | Performance",
        meta = (ClampMin = "0.0"))
    float MaxImpactVFXDistance = 20000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TObjectPtr<USoundBase> ImpactSound = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TObjectPtr<UMaterialInterface> ImpactDecalMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    FVector ImpactDecalSize = FVector(12.0f, 12.0f, 12.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    float ImpactDecalLifeSpan = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX")
    TSubclassOf<UCameraShakeBase> ImpactCameraShake;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WTBR | Projectile | VFX | Debug")
    bool bDrawImpactDebug = false;

    void ApplyVFXConfig(const FWTBRProjectileVFXConfig& Config);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileLaunched();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileHitVFX(FVector ImpactPoint, FVector ImpactNormal);

    // Optional Blueprint hook for surface-aware effects. The existing two-pin
    // hit event remains intact for backwards compatibility.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnProjectileSurfaceHitVFX(FVector ImpactPoint, FVector ImpactNormal,
        uint8 SurfaceType);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnExplosionVFX(FVector Center, float Radius);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Projectile | VFX")
    void OnCubeSplitVFX();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ProjectileHitVFX(FVector ImpactPoint, FVector ImpactNormal,
        uint8 SurfaceType);

    // Spawn sub-bullets for a cube-split burst; called immediately by Labyrn FireComposite.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Projectile")
    void SpawnCubeSplits();

    void TriggerExplosionForTest() { TriggerExplosion(); }
    void TriggerSecondExplosionForTest() { TriggerSecondExplosion(); }
    void OnInterpMovementEndForTest() { OnInterpMovementEnd(FHitResult(), 0.0f); }
    void BeginProximityChaseForTest(AWTBRCharacter* Target) { BeginProximityChase(Target); }
    bool IsSecondExplosionTimerActiveForTest() const
    {
        return GetWorldTimerManager().IsTimerActive(SecondExplosionTimer);
    }

    bool IsLocationWithinShapedChargeCone(const FVector& TargetLocation) const;

    // Headshot-style damage classification. Pure geometry approximation —
    // this codebase has no per-bone hit detection (target collision is a
    // single capsule, no physics asset with separate head/torso/arm/leg
    // shapes), so it derives a zone purely from where ImpactPoint falls
    // relative to the target's own capsule: height splits Head/Torso/Leg
    // bands, and within the torso band, lateral distance from the capsule's
    // central vertical axis further splits out Arm hits. Deliberately takes
    // plain values (not a live UCapsuleComponent*) so it's testable with no
    // world/physics/registration at all.
    static EWTBRHitZone ClassifyHitZone(const FVector& CapsuleCenter, float CapsuleHalfHeight,
        float CapsuleRadius, const FVector& ImpactPoint, float HeadHeightThreshold,
        float LegHeightThreshold, float ArmLateralRadiusRatio);

    // Select the contact point used by hit-zone classification. Overlap events
    // use bFromSweep rather than FHitResult::bBlockingHit; unset sweep fields
    // safely fall back to the projectile location.
    static FVector ResolveHitZoneImpactPoint(bool bFromSweep, const FHitResult& SweepResult,
        const FVector& FallbackLocation);

    static float GetHitZoneDamageMultiplier(EWTBRHitZone Zone, const UWTBRCoreStatsDataAsset* Stats);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_ProjectileState();

    UFUNCTION()
    void OnInterpMovementEnd(const FHitResult& ImpactResult, float Time);

    // Hands a sweeping cube over to homing mid-path, keeping its current heading.
    void BeginProximityChase(AWTBRCharacter* Target);

    UFUNCTION()
    void OnDelayedLaunchElapsed();

    FTimerHandle DelayedLaunchTimer;
    TArray<FVector> PendingPathPoints;
    float PendingPathSpeed = 0.0f;

    // Sweep diagnostics fire once per cube, never per frame — a volley ticking every
    // frame would bury the log it is supposed to make readable.
    bool bLoggedSweepLOSBlock = false;

    // Closest any enemy came to this cube across its whole sweep. Reported once at
    // expiry, because a first-frame reading says nothing about where the arc went.
    float ClosestSweepApproachSq = TNumericLimits<float>::Max();

    // True only while BeginProximityChase is mid-handoff, so the OnInterpToStop it
    // provokes cannot be mistaken for the path genuinely finishing.
    bool bTransitioningToProximityChase = false;

    // Who this cube committed to. Steering is driven from Tick rather than handed to
    // UProjectileMovementComponent's own homing, because that homing has no turn
    // limit and cannot be given one.
    TWeakObjectPtr<AWTBRCharacter> ProximityChaseTarget;

    // Per-frame steering toward ProximityChaseTarget, clamped by the turn rate.
    void TickProximityChase(float DeltaSeconds);

    // Detonates on a chase target the cube has closed inside ProximityDetonationRadius
    // of, reusing the real overlap path so damage, hit VFX and destruction are
    // identical to a direct hit. Impact point is the target centre, so the fuse always
    // classifies as a torso hit rather than randomly catching a head/limb multiplier.
    void DetonateOnProximityTarget(AWTBRCharacter* Target);

    // ── Lane events ──────────────────────────────────────────────────────────
    //
    // Markers the player placed on the drawn lane. Fired by DISTANCE along the path
    // rather than by elapsed time, because the player placed them by clicking a
    // position on a line — and because a hover or a speed change would otherwise
    // shift every later marker.
    void ArmLaneEvents(const TArray<FWTBRLaneEvent>& Events, float PathLength);
    void TickLaneEvents();
    void ApplyLaneEvent(const FWTBRLaneEvent& Event);
    void EndHover();

    // Set at spawn, armed once InitializePathMovement knows the path's real length.
    TArray<FWTBRLaneEvent> AuthoredLaneEvents;

    // Sorted by distance; consumed front to back.
    TArray<FWTBRLaneEvent> PendingLaneEvents;
    TArray<float> PendingLaneEventDistances;

    // Distance covered along the path so far, accumulated per frame.
    float PathDistanceTravelled = 0.0f;
    FVector LastEventSampleLocation = FVector::ZeroVector;

    // Radius to restore when a SetHoming event switches hunting back on.
    float ArmedHomingRadius = 0.0f;

    FTimerHandle HoverTimer;
    bool bHoveringMidPath = false;

    // Reshapes the finalised control-point timing into the speed profile above.
    void ApplyPathSpeedProfile();

    float PathSlowFromFraction = 0.0f;
    float PathEndSpeedFactor = 1.0f;

    // Stamped when the path actually starts moving, so the landing log can report a
    // real flight time. Comparing those across one volley is the only direct proof
    // that lanes of different lengths finish together.
    float PathStartWorldTime = 0.0f;
    float PathTotalLength = 0.0f;

    // Where the path began, and the heading of its final leg.
    //
    // A bullet stops for exactly three reasons: it hits somebody, it hits the world,
    // or it runs out of range. Reaching the end of an AUTHORED PATH is none of those
    // — the path only describes the shaped part of the flight — so the cube carries
    // on along its last heading until one of the three actually happens. Range is
    // measured from here, so a preset that loops back toward the caster does not
    // spend reach it never used.
    FVector PathLaunchOrigin = FVector::ZeroVector;
    FVector PathFinalDirection = FVector::ForwardVector;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnProjectileHit(UPrimitiveComponent* HitComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        FVector NormalImpulse, const FHitResult& Hit);

private:
    float CachedSpeed   = 0.0f;
    bool  bHandledClash = false; // prevents double-processing when both bullets' overlaps fire
    FTimerHandle SecondExplosionTimer;

    void TriggerExplosion();
    void TriggerSecondExplosion();
    void ApplyRadialDamage(float Radius, float Damage);

public:
    // One entry per distinct character in a multi-sweep's hit list. Public and
    // static so the de-duplication can be tested without a physics scene, which a
    // headless fixture does not have.
    static void CollectUniqueRadialTargets(
        const TArray<FHitResult>& Hits, TArray<AWTBRCharacter*>& OutTargets);

protected:
    void OnBulletClash(AWTBRProjectileBase* OtherBullet);
    UNiagaraSystem* ResolveImpactEffect(uint8 SurfaceType) const;
    void SpawnBuiltInImpactVFX(FVector ImpactPoint, FVector ImpactNormal, uint8 SurfaceType) const;
};
