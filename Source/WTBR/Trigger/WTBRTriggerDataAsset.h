// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

class USkeletalMesh;
class UAnimMontage;
class UTexture2D;
class AWTBRProjectileBase;
class AWTBRSolvarnField;
class AWTBRKaldrixZone;

#include "WTBRTriggerDataAsset.generated.h"


UENUM(BlueprintType)
enum class ETriggerCategory : uint8
{
    None         UMETA(DisplayName = "None"),
    Melee        UMETA(DisplayName = "Melee"),


    Gunner       UMETA(DisplayName = "Gunner"),


    SniperBullet UMETA(DisplayName = "Sniper Bullet"),


    Defense      UMETA(DisplayName = "Defense"),


    Movement     UMETA(DisplayName = "Movement"),


    Trap         UMETA(DisplayName = "Trap"),


    BlackTrigger UMETA(DisplayName = "Black Trigger"),

};

UENUM(BlueprintType)
enum class ETriggerSlotConstraint : uint8
{
    MainOnly UMETA(DisplayName="Main Slot Only"),
    SubOnly  UMETA(DisplayName="Sub Slot Only"),
    Any      UMETA(DisplayName="Any Slot"),
};

// Composite Bullet types — each value maps to one Gunner × Gunner DualGesture combo
UENUM(BlueprintType)
enum class EWTBRCompositeBulletType : uint8
{
    None     UMETA(DisplayName = "None"),
    Solgrix  UMETA(DisplayName = "Solgrix  (Solux + Fulgrix)"),
    Dualux   UMETA(DisplayName = "Dualux   (Solux + Solux)"),
    Solveil  UMETA(DisplayName = "Solveil  (Solux + Serpveil)"),
    Solhunt  UMETA(DisplayName = "Solhunt  (Solux + Venyx)"),
    Acervyn  UMETA(DisplayName = "Acervyn  (Venyx + Venyx)"),
    Fulgvyn  UMETA(DisplayName = "Fulgvyn  (Fulgrix + Venyx)"),
    Coilvyn  UMETA(DisplayName = "Coilvyn  (Serpveil + Venyx)"),
    Catarix  UMETA(DisplayName = "Catarix  (Fulgrix + Fulgrix)"),
    Labyrn   UMETA(DisplayName = "Labyrn   (Serpveil + Serpveil)"),
    Ignivex  UMETA(DisplayName = "Ignivex  (Solux + Fulgrix*)"),
};

// Preset curved-flight shape for Serpveil (Viper — GDD Phase 4)
UENUM(BlueprintType)
enum class EWTBRSerpveilShape : uint8
{
    Curve     UMETA(DisplayName = "Curve"),
    SCurve    UMETA(DisplayName = "S-Curve"),
    Hook      UMETA(DisplayName = "Hook"),
    Spiral    UMETA(DisplayName = "Spiral"),
    Boomerang UMETA(DisplayName = "Boomerang"),
};

USTRUCT(BlueprintType)
struct FWTBRMeleeHitboxParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING — all values adjustable via DataAsset only
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Hitbox",
        meta = (ClampMin = "1.0"))
    float CapsuleRadius = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Hitbox",
        meta = (ClampMin = "1.0"))
    float CapsuleHalfHeight = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Hitbox",
        meta = (ClampMin = "10.0"))
    float ForwardOffset = 120.0f;

    // TD Approved: offset L/R so dual sweeps don't overlap
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Hitbox | Dual Wield",
        meta = (ClampMin = "0.0"))
    float DualWieldLateralOffset = 40.0f;

    // FTimerHandle driven — NOT Tick
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Combat",
        meta = (ClampMin = "0.1"))
    float SwingCooldown = 0.5f;

    // Mantorn: รัศมีหมุน Spin Slash
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Mantorn",
        meta = (ClampMin = "50.0"))
    float MantornSpinRadius = 200.0f;

    // Mantorn: Damage ของ Spin Slash
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Melee | Mantorn",
        meta = (ClampMin = "0.0"))
    float MantornSpinDamage = 80.0f;
};

USTRUCT(BlueprintType)
struct FWTBRDualWieldStats
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="0.1", ClampMax="5.0"))
    float DamageMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="0.1", ClampMax="1.0"))
    float SpeedMultiplier = 1.0f;

    // Multiplicative with StaminaCostPerUse when dual wielding (GDD §3.4 Lock: 1.5x)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield", meta=(ClampMin="1.0", ClampMax="3.0"))
    float StaminaCostMultiplier = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dual Wield")
    bool bHasDualWieldMontage = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRMantornParams — Mantorn (Feryx + Feryx composite transform form)
// Mantorn is NOT a standalone equippable Trigger. It is unlocked at runtime by
// holding LMB+RMB together while both the active Main AND active Sub slots hold a
// Feryx that has bCanFormMantorn = true. In-form: Tap = forward whip, Hold = AOE
// spin (reuses MeleeHitbox.MantornSpinRadius/MantornSpinDamage). Separate Vael
// cost model, mirroring Dual Senku. All values DA-driven (⚠ Playtest).
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRMantornParams
{
    GENERATED_BODY()

    // Designer gate: BOTH the active Main and active Sub Feryx DataAssets must set
    // this true for the dual-hold gesture to enter Mantorn form. Never compare by
    // class name / DisplayName — this flag replaces the retired bIsMantornPriority.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantorn | Form")
    bool bCanFormMantorn = false;

    // Vael consumed once on entering the form (charged server-side at transform
    // commit; NOT refunded on exit). Separate from per-attack costs, like Dual Senku.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Cost", meta = (ClampMin = "0.0"))
    float TransformVaelCost = 20.0f;

    // Vael per forward whip (Tap). ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Cost", meta = (ClampMin = "0.0"))
    float WhipVaelCost = 8.0f;

    // Vael per AOE spin (Hold). ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Cost", meta = (ClampMin = "0.0"))
    float SpinVaelCost = 12.0f;

    // Forward whip reach in UE units (1 unit = 1 cm). ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Whip", meta = (ClampMin = "50.0"))
    float WhipReach = 450.0f;

    // Forward whip capsule radius (sweep half-width). ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Whip", meta = (ClampMin = "10.0"))
    float WhipRadius = 60.0f;

    // Forward whip damage per hit. ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Whip", meta = (ClampMin = "0.0"))
    float WhipDamage = 90.0f;

    // Cooldown between in-form attacks (whip or spin), FTimerHandle-driven. ⚠ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Combat", meta = (ClampMin = "0.05"))
    float ActionCooldown = 0.5f;

    // Server-side hold classification: a single-side press held at least this long
    // (seconds) on release = AOE spin (Hold); shorter = forward whip (Tap). Kept
    // separate from the gesture component's enter-form threshold so feel is tunable.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Mantorn | Combat", meta = (ClampMin = "0.05"))
    float InFormHoldThreshold = 0.2f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRLacernParams — Lacern (ดาบยืด)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRLacernParams
{
    GENERATED_BODY()

    // ระยะยืดสูงสุดของใบมีด (UE units)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Lacern | Extend",
        meta = (ClampMin = "50.0"))
    float ExtendLength = 400.0f;

    // ความเร็วยืดออก (units/sec)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Lacern | Extend",
        meta = (ClampMin = "100.0"))
    float ExtendSpeed = 1200.0f;

    // ความเร็วดึงกลับ (units/sec)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Lacern | Extend",
        meta = (ClampMin = "100.0"))
    float RetractSpeed = 1800.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRVoltisParams — Voltis (กระโดด + กับดักระเบิด)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRVoltisParams
{
    GENERATED_BODY()

    // แรงดีดผู้ใช้ขึ้น/ออก (Z component)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Launch",
        meta = (ClampMin = "100.0"))
    float LaunchForce = 1500.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voltis")
    float VerticalLaunchForce = 1200.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voltis")
    float HorizontalLaunchForce = 1200.0f;

    // Directional air-step force when W/A/S/D is held during Voltis activation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voltis | Launch")
    float DirectionalHorizontalForce = 1200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voltis | Launch")
    float DirectionalVerticalForce = 300.0f;

    // จำนวนครั้งสูงสุดที่ดีดได้ก่อนลงพื้น (GDD: ไม่จำกัด แต่รอ Playtest)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Launch",
        meta = (ClampMin = "1"))
    int32 MaxAirLaunches = 3;

    // รัศมีระเบิดของ Voltis Trap
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Trap",
        meta = (ClampMin = "50.0"))
    float TrapExplosionRadius = 300.0f;

    // Damage ของ Voltis Trap (GDD: Launch เท่านั้น ไม่มี Damage — 0 default)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Trap",
        meta = (ClampMin = "0.0"))
    float TrapDamage = 0.0f;

    // ระยะเวลาที่ Trap อยู่บนพื้นก่อนหมดอายุ (วินาที)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Trap",
        meta = (ClampMin = "1.0"))
    float TrapLifetime = 30.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRAegornParams — Aegorn / Shield (โล่เคลื่อนที่ได้ โปร่งแสง)
// Wall placement now lives on Escudo (FWTBREscudoParams) — canon splits these
// into two distinct Triggers, not tap/hold of the same one.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRAegornParams
{
    GENERATED_BODY()

    // HP ของโล่ Aegorn (ถูกยิงซ้ำลด HP จนหมด)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Aegorn | Shield",
        meta = (ClampMin = "1.0"))
    float ShieldHP = 200.0f;

    // ความกว้างของโล่ (องศา — 90 = กั้นได้ 1 ทิศ, 180 = Dual Wield)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Aegorn | Shield",
        meta = (ClampMin = "45.0", ClampMax = "180.0"))
    float ShieldCoverageAngle = 90.0f;

    // อายุโล่ (วินาที) — ครบเวลาสลายเอง (Pre-Lock Patch v0.2 #8:
    // upfront Vael cost + ShieldHP + Duration)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Aegorn | Shield",
        meta = (ClampMin = "0.5"))
    float ShieldDuration = 8.0f;

    // Cooldown after ANY shield ends (tap breaks/expires, hold released, hold
    // shield destroyed early by damage or Duration while still held) before
    // another tap or hold can start. Aegorn had zero cooldown before this —
    // a shield could be re-raised the instant the previous one ended.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Aegorn | Shield",
        meta = (ClampMin = "0.0"))
    float AegornCooldownAfterUse = 1.5f;

};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBREscudoParams — Escudo (กำแพง Trion แข็ง วางตรึงจุด, แยกจาก Aegorn/Shield
// ตาม canon: Escudo วางแล้วขยับไม่ได้ ทนทานกว่า Shield มาก กิน Vael สูง)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBREscudoParams
{
    GENERATED_BODY()

    // HP ของกำแพง Escudo
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "1.0"))
    float EscudoWallHP = 300.0f;

    // อายุกำแพง (วินาที) — ครบเวลาสลายเอง (anti-abuse: ซื้อเวลา ไม่ใช่บล็อกถาวร)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "1.0"))
    float EscudoWallDuration = 15.0f;

    // ระยะ line-trace หาพื้นรองรับใต้จุดวาง — ไม่เจอพื้นในระยะนี้ = วางไม่ได้ ไม่เสีย Vael
    // (canon: Escudo ต้องงอกจากพื้นผิว ห้ามลอยกลางอากาศ)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "50.0"))
    float EscudoSurfaceSnapRange = 300.0f;

    // แรงผลักเพื่อนร่วมทีมที่ยืนขวางจุดวาง ให้มาอยู่ฝั่งหลังกำแพง (แนวนอน, u/s)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Displacement",
        meta = (ClampMin = "0.0"))
    float EscudoAllyPushImpulse = 600.0f;

    // แรงดีดศัตรูที่ยืนขวางจุดวางขึ้นฟ้า (แนวตั้ง, u/s) — canon: Hyuse ดีด Ikoma
    // damage = 0 เสมอ (Escudo Slam lock: displacement, NOT damage)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Displacement",
        meta = (ClampMin = "0.0"))
    float EscudoEnemyLaunchImpulse = 1200.0f;

    // เวลาสร้างกำแพง (วินาที)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "0.1"))
    float EscudoWallBuildTime = 0.3f;

    // ขนาดกำแพง (ความสูง × ความกว้าง)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall")
    FVector2D EscudoWallSize = FVector2D(300.0f, 200.0f);
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRArcvenParams — Arcven (คลื่น Arc) + Arcven Dual
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRArcvenParams
{
    GENERATED_BODY()

    // Damage ของ Arc Wave เดี่ยว (GDD Lock: 120)
    // ⚠ PLAYTEST PENDING — GDD เขียน 120 แต่รอ Playtest
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Single",
        meta = (ClampMin = "0.0"))
    float ArcDamage = 120.0f;

    // ระยะยิง Arc Wave
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Single",
        meta = (ClampMin = "100.0"))
    float ArcRange = 800.0f;

    // ความเร็ว Arc Wave (units/sec)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Single",
        meta = (ClampMin = "100.0"))
    float ArcWaveSpeed = 1200.0f;

    // Damage Arcven Dual รวม (GDD: 200 = 100/เส้น)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Dual",
        meta = (ClampMin = "0.0"))
    float DualArcTotalDamage = 200.0f;

    // Shield Penetration ตอน Dual Cross-Slash (GDD: 50%)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Dual",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DualCrossSlashShieldPen = 0.5f;

    // มุมเบี่ยงของ Dual Wave ซ้าย/ขวา (องศา)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Dual",
        meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float DualWaveSpreadAngle = 15.0f;

    // Projectile class to spawn on fire (set to BP_WTBRArcvenProjectile in DA_Arcven)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Projectile")
    TSubclassOf<AWTBRProjectileBase> ArcProjectileClass;

    // True → dispatch radar ping via UWTBRActionPingSubsystem on every fire
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Action Ping")
    bool bPingOnFire = true;

    // ── Lacern-hold charge tiers (Senkū Trigger Option) ──────────────────────
    // Canon-correct Kogetsu+Senku pairing (§5.4): Arcven attaches to Lacern's
    // slot via FWTBRTriggerSlot::OptionDataAsset. Tap on that slot = normal
    // Lacern swing; Hold = charge this Arc Wave. Two discrete tiers (not a
    // continuous lerp, agreed 2026-07-14) — hold longer = stronger, the safer
    // (non-canon-inverse) direction, since real Senkū's short-hold-is-stronger
    // quirk has no downside/risk trade-off in PVP. ArcDamage/ArcRange above
    // ARE the full-charge tier already (GDD-locked 120/800) — unchanged.

    // Release before this = cancel, no Vael cost, falls back to a normal
    // Lacern tap swing instead. ⚠ Playtest pending.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Lacern Hold", meta = (ClampMin = "0.0"))
    float MinChargeTime = 0.2f;

    // Hold duration needed to reach the full-power tier (ArcDamage/ArcRange).
    // ⚠ Playtest pending.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Lacern Hold", meta = (ClampMin = "0.0"))
    float FullChargeThreshold = 1.0f;

    // Damage for a release between MinChargeTime and FullChargeThreshold.
    // ⚠ Playtest pending.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Lacern Hold", meta = (ClampMin = "0.0"))
    float WeakArcDamage = 100.0f;

    // Range for a release between MinChargeTime and FullChargeThreshold.
    // ⚠ Playtest pending.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Arcven | Lacern Hold", meta = (ClampMin = "100.0"))
    float WeakArcRange = 650.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRNexilParams — Nexil (กับดักเส้น)
// GDD: Damage = 0, CC ล้วน, ขึ้น Action Ping เมื่อสะดุด
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRNexilParams
{
    GENERATED_BODY()

    // ความยาวเส้นกับดัก
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "50.0"))
    float WireLength = 500.0f;

    // ระยะเวลา Stagger เมื่อศัตรูสะดุด (GDD: 1.5 วิ)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Effect",
        meta = (ClampMin = "0.1"))
    float StaggerDuration = 1.5f;

    // จำนวนเส้นสูงสุดต่อคน (GDD: 8 เส้น — เส้นที่ 9 ลบเส้นแรก)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "1"))
    int32 MaxWires = 8;

    // ระยะเวลาที่เส้นอยู่บนพื้น (GDD: 45 วิ)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "1.0"))
    float WireDuration = 45.0f;

    // HP ของเส้น (GDD: 1 — ฟันหรือยิงขาดได้)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "1"))
    int32 WireHP = 1;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRTelornParams — Telorn (Egret — Long Range Sniper)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRTelornParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | Combat", meta = (ClampMin = "0.0"))
    float TelornDamage = 60.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | Projectile", meta = (ClampMin = "100.0"))
    float TelornSpeed = 6000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | Projectile", meta = (ClampMin = "100.0"))
    float TelornRange = 10000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | Projectile")
    TSubclassOf<AWTBRProjectileBase> TelornProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Telorn | Combat",
        meta = (ClampMin = "0.1"))
    float TelornFireCooldown = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Telorn | VFX",
        meta = (ClampMin = "1"))
    int32 TelornCubeSplitCount = 1;

    // Camera FOV while zoomed (degrees) — Egret has the best range of all
    // snipers (canon), so the deepest zoom of the three.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Telorn | Zoom",
        meta = (ClampMin = "5.0", ClampMax = "90.0"))
    float TelornZoomFOV = 25.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRPiercexParams — Piercex (Ibis — Penetrating Sniper, highest damage)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRPiercexParams
{
    GENERATED_BODY()

    // Highest damage among snipers (GDD)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Combat", meta = (ClampMin = "0.0"))
    float PiercexDamage = 100.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Projectile", meta = (ClampMin = "100.0"))
    float PiercexSpeed = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Projectile", meta = (ClampMin = "100.0"))
    float PiercexRange = 12000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Projectile")
    TSubclassOf<AWTBRProjectileBase> PiercexProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Combat", meta = (ClampMin = "0.1"))
    float PiercexFireCooldown = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | VFX", meta = (ClampMin = "1"))
    int32 PiercexCubeSplitCount = 1;

    // Camera FOV while zoomed (degrees) — mid-depth zoom between Telorn/Fulgris.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Piercex | Zoom",
        meta = (ClampMin = "5.0", ClampMax = "90.0"))
    float PiercexZoomFOV = 35.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRFulgrisParams — Fulgris (Lightning — Fastest Sniper, lower damage)
// Note: distinct from FWTBRFulgrixParams (Fulgrix = explosive Gunner)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRFulgrisParams
{
    GENERATED_BODY()

    // Lowest damage among snipers — compensated by highest speed (GDD)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Combat", meta = (ClampMin = "0.0"))
    float FulgrisDamage = 40.0f;

    // Fastest projectile in game (GDD)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Projectile", meta = (ClampMin = "100.0"))
    float FulgrisSpeed = 12000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Projectile", meta = (ClampMin = "100.0"))
    float FulgrisRange = 8000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Projectile")
    TSubclassOf<AWTBRProjectileBase> FulgrisProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Combat", meta = (ClampMin = "0.1"))
    float FulgrisFireCooldown = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | VFX", meta = (ClampMin = "1"))
    int32 FulgrisCubeSplitCount = 2;

    // Camera FOV while zoomed (degrees) — fastest/lowest-damage sniper gets
    // the shallowest zoom of the three (canon: quickest, least precise).
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Fulgris | Zoom",
        meta = (ClampMin = "5.0", ClampMax = "90.0"))
    float FulgrisZoomFOV = 45.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRSoluxParams — Solux (Asteroid — Normal Bullet)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRSoluxParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "0.0"))
    float SoluxDamage = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile", meta = (ClampMin = "100.0"))
    float SoluxSpeed = 3000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile", meta = (ClampMin = "100.0"))
    float SoluxRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile")
    TSubclassOf<AWTBRProjectileBase> SoluxProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "0.05"))
    float SoluxFireCooldown = 0.5f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRFulgrixParams — Fulgrix (Meteor — Explosive Bullet)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRFulgrixParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Combat", meta = (ClampMin = "0.0"))
    float FulgrixDamage = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Projectile", meta = (ClampMin = "100.0"))
    float FulgrixSpeed = 2500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Projectile", meta = (ClampMin = "100.0"))
    float FulgrixRange = 4000.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Explosion", meta = (ClampMin = "50.0"))
    float FulgrixExplosionRadius = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Projectile")
    TSubclassOf<AWTBRProjectileBase> FulgrixProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Fulgrix | Combat",
        meta = (ClampMin = "0.1"))
    float FulgrixFireCooldown = 0.8f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRVenyxParams — Venyx (Hound — Homing Bullet)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRVenyxParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "0.0"))
    float VenyxDamage = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile", meta = (ClampMin = "100.0"))
    float VenyxSpeed = 3500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile", meta = (ClampMin = "100.0"))
    float VenyxRange = 6000.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Homing", meta = (ClampMin = "100.0"))
    float VenyxHomingAcceleration = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile")
    TSubclassOf<AWTBRProjectileBase> VenyxProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Venyx | Combat",
        meta = (ClampMin = "0.1"))
    float VenyxFireCooldown = 0.6f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRSerpveilParams — Serpveil (Viper — Curved Path, Phase 4)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRSerpveilParams
{
    GENERATED_BODY()

    // DEPRECATED by S1 rework
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Combat", meta = (ClampMin = "0.0"))
    float SerpveilDamage = 35.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Combat", meta = (ClampMin = "0.0"))
    float SerpveilPerCubeDamage = 15.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Projectile", meta = (ClampMin = "100.0"))
    float SerpveilSpeed = 2800.0f;

    // DEPRECATED by S1 rework
    // Preset flight-path shape baked into control points at fire time
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path")
    EWTBRSerpveilShape PresetShape = EWTBRSerpveilShape::Curve;

    // DEPRECATED by S1 rework
    // Minimum flight range regardless of charge (must hold button at least this long)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "100.0"))
    float SerpveilMinRange = 400.0f;

    // Maximum flight range at full Vael charge
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "100.0"))
    float SerpveilMaxRange = 1500.0f;

    // DEPRECATED by S1 rework
    // Vael drained per second while charging (determines affordable range)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Resources", meta = (ClampMin = "0.1"))
    float SerpveilVaelPerSecond = 10.0f;

    // DEPRECATED by S1 rework
    // Intended max charge time (informational — server uses Vael formula, not this directly)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "0.1"))
    float SerpveilChargeTime = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Projectile")
    TSubclassOf<AWTBRProjectileBase> SerpveilProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Combat",
        meta = (ClampMin = "0.1"))
    float SerpveilFireCooldown = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Windup",
        meta = (ClampMin = "0.0"))
    float SerpveilSplitDelay = 0.4f;

    // DEPRECATED by zigzag tap pattern — fan spread let only one cube land at range
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilTapSpreadDeg = 24.0f;

    // Sideways gap between neighbouring cubes in the launch formation
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilFormationSpacing = 25.0f;

    // Vertical saw-tooth offset: even cubes sit up, odd cubes sit down (▲▼▲)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilFormationStagger = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Resources",
        meta = (ClampMin = "0.0"))
    float SerpveilVaelCostPerShot = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "1"))
    int32 SerpveilCubeSplitCount = 3;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRAcervynParams — Acervyn (Hornet — Burst Homing)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRAcervynParams
{
    GENERATED_BODY()

    // Damage per bullet (total = AcervynDamage * AcervynBurstCount)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Combat", meta = (ClampMin = "0.0"))
    float AcervynDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Projectile", meta = (ClampMin = "100.0"))
    float AcervynSpeed = 4000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Projectile", meta = (ClampMin = "100.0"))
    float AcervynRange = 5000.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Burst", meta = (ClampMin = "1", ClampMax = "10"))
    int32 AcervynBurstCount = 5;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Homing", meta = (ClampMin = "100.0"))
    float AcervynHomingAcceleration = 6000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Projectile")
    TSubclassOf<AWTBRProjectileBase> AcervynProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Combat", meta = (ClampMin = "0.1"))
    float AcervynFireCooldown = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Burst", meta = (ClampMin = "0.05"))
    float AcervynBurstInterval = 0.1f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRVentryxParams — Ventryx (Fujin — Wind, forward knockback blast)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRVentryxParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Combat", meta = (ClampMin = "0.0"))
    float VentryxDamage = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Projectile", meta = (ClampMin = "100.0"))
    float VentryxRange = 5000.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Projectile", meta = (ClampMin = "100.0"))
    float VentryxSpeed = 5000.0f;

    // Launch force applied to hit characters (UE units/s)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Combat", meta = (ClampMin = "0.0"))
    float VentryxKnockback = 1000.0f;

    // Black Trigger: high Vael cost (GDD §5.1)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Resources", meta = (ClampMin = "0.0"))
    float VentryxVaelCost = 80.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Projectile")
    TSubclassOf<AWTBRProjectileBase> VentryxProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ventryx | Combat", meta = (ClampMin = "0.1"))
    float VentryxFireCooldown = 3.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRFulgornParams — Fulgorn (Artemis — Multi Homing)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRFulgornParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Combat", meta = (ClampMin = "0.0"))
    float FulgornDamage = 60.0f;

    // Effective flight distance before projectile expires
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Projectile", meta = (ClampMin = "100.0"))
    float FulgornRange = 5000.0f;

    // Radius of initial sphere overlap to lock onto targets
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Lock-On", meta = (ClampMin = "50.0"))
    float FulgornLockRadius = 1500.0f;

    // Maximum simultaneous targets (GDD: 5)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Lock-On", meta = (ClampMin = "1", ClampMax = "20"))
    int32 FulgornMaxTargets = 5;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Projectile", meta = (ClampMin = "100.0"))
    float FulgornProjectileSpeed = 4000.0f;

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Homing", meta = (ClampMin = "100.0"))
    float FulgornHomingAcceleration = 8000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Projectile")
    TSubclassOf<AWTBRProjectileBase> FulgornProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Resources", meta = (ClampMin = "0.0"))
    float FulgornVaelCost = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgorn | Combat", meta = (ClampMin = "0.1"))
    float FulgornFireCooldown = 4.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRSolvarnParams — Solvarn (Suiren — Defense Field)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRSolvarnParams
{
    GENERATED_BODY()

    // Sphere radius of the blocking field (UE units)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Field", meta = (ClampMin = "50.0"))
    float SolvarnRadius = 400.0f;

    // Maximum lifetime of the field (seconds) — may end sooner if Vael runs out
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Field", meta = (ClampMin = "0.5"))
    float SolvarnDuration = 5.0f;

    // Vael drained per second while field is active (ongoing cost)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Resources", meta = (ClampMin = "0.0"))
    float SolvarnVaelDrainPerSec = 20.0f;

    // One-time Vael cost to activate (GDD §5.1 Black Trigger surcharge)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Resources", meta = (ClampMin = "0.0"))
    float SolvarnVaelCost = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Field")
    TSubclassOf<AWTBRSolvarnField> SolvarnFieldClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solvarn | Combat", meta = (ClampMin = "0.1"))
    float SolvarnFireCooldown = 5.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRKaldrixParams — Kaldrix (Kazan — Explosion Zone)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRKaldrixParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Combat", meta = (ClampMin = "0.0"))
    float KaldrixDamage = 150.0f;

    // Explosion sphere radius
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Explosion", meta = (ClampMin = "50.0"))
    float KaldrixRadius = 500.0f;

    // Delay from placement to detonation (seconds)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Explosion", meta = (ClampMin = "0.1"))
    float KaldrixArmTime = 3.0f;

    // Duration of stagger applied to characters caught in the blast
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Combat", meta = (ClampMin = "0.1"))
    float KaldrixStaggerDuration = 1.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Resources", meta = (ClampMin = "0.0"))
    float KaldrixVaelCost = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Zone")
    TSubclassOf<AWTBRKaldrixZone> KaldrixZoneClass;

    // Zone must arm before the next placement — keep >= KaldrixArmTime.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kaldrix | Combat", meta = (ClampMin = "0.1"))
    float KaldrixFireCooldown = 6.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRNyxveilParams — Nyxveil (Yomi — Scan)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRNyxveilParams
{
    GENERATED_BODY()

    // Radius of the initial pawn detection sphere
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nyxveil | Scan", meta = (ClampMin = "100.0"))
    float NyxveilScanRadius = 2000.0f;

    // Total duration of the repeating scan (seconds)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nyxveil | Scan", meta = (ClampMin = "1.0"))
    float NyxveilDuration = 10.0f;

    // How often (seconds) the scan refreshes and re-pings the radar
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nyxveil | Scan", meta = (ClampMin = "0.1"))
    float NyxveilPingInterval = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nyxveil | Resources", meta = (ClampMin = "0.0"))
    float NyxveilVaelCost = 60.0f;

    // Cooldown after the scan ends — keep >= NyxveilDuration so it can't re-fire mid-scan.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nyxveil | Combat", meta = (ClampMin = "0.1"))
    float NyxveilFireCooldown = 15.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRVexornParams — Vexorn (Signal Block — Passive Defense)
// Passive: no button press, no Vael drain. Active while Vexorn is the Sub-Trigger.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRVexornParams
{
    GENERATED_BODY()

    // Radius within which enemy pawns receive Signal Block (suppresses radar ping)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Combat")
    float VexornSuppressionRadius = 1500.0f;

    // Passive — always 0, present for DataAsset consistency
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Resources")
    float VexornVaelCost = 0.0f;

    // Informational: this trigger activates automatically on equip, not on button press
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Passive")
    bool bIsPassive = true;

    // Informational: enforced in loadout UI — Vexorn may only occupy a Sub Slot
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Passive")
    bool bSubSlotOnly = true;
};

UCLASS(BlueprintType)
class WTBR_API UWTBRTriggerDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ─── Identity ────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    FText DisplayName;

    // Short in-world weapon name shown in HUD (e.g. "Kogetsu", "Ibis"). Localization-ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    FText FunctionalName;

    // One-line description shown in loadout/tooltip UI. Localization-ready.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    FText FunctionalDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger|Identity")
    TSoftObjectPtr<UTexture2D> HUDIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    ETriggerCategory Category = ETriggerCategory::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    ETriggerSlotConstraint SlotConstraint = ETriggerSlotConstraint::Any;

    // ─── Resources ───────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "Trigger | Identity")
    TSubclassOf<class UWTBRTriggerBase> TriggerClass;
    // Vael cost per activation — validated server-side before any effect (⚠ Playtest)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Resources", meta=(ClampMin="0"))
    float VaelCostPerUse = 10.0f;

    // Stamina cost per activation — base before dual-wield multiplier (⚠ Playtest)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Resources", meta=(ClampMin="0"))
    float StaminaCostPerUse = 0.0f;

    // ─── Combat ──────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Combat", meta=(ClampMin="0"))
    float BaseDamage = 0.0f;

    // [0,1] — fraction of damage that bypasses Defense triggers (GDD §2.5)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ShieldPenetrationFactor = 0.0f;

    // ─── Dual Wield ──────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Dual Wield")
    bool bSupportsDualWield = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Dual Wield", meta=(EditCondition="bSupportsDualWield"))
    FWTBRDualWieldStats DualWieldStats;

    // ── Melee Hitbox ──────────────────────────────────────────────────────
    // Used only when Category == ETriggerCategory::Melee
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Melee",
        meta = (EditCondition = "Category == ETriggerCategory::Melee"))
    FWTBRMeleeHitboxParams MeleeHitbox;

    // ── Lacern ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Lacern",
        meta = (EditCondition = "Category == ETriggerCategory::Melee"))
    FWTBRLacernParams LacernParams;

    // ── Mantorn (Feryx + Feryx composite form) ────────────────────────────
    // Only meaningful on a Feryx DataAsset (Category == Melee). Set
    // MantornParams.bCanFormMantorn = true on DA_Feryx to allow the composite.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Mantorn",
        meta = (EditCondition = "Category == ETriggerCategory::Melee"))
    FWTBRMantornParams MantornParams;

    // ── Voltis ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Voltis",
        meta = (EditCondition = "Category == ETriggerCategory::Movement"))
    FWTBRVoltisParams VoltisParams;

    // ── Aegorn ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Aegorn",
        meta = (EditCondition = "Category == ETriggerCategory::Defense"))
    FWTBRAegornParams AegornParams;

    // ── Escudo ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Escudo",
        meta = (EditCondition = "Category == ETriggerCategory::Defense"))
    FWTBREscudoParams EscudoParams;

    // ── Arcven ────────────────────────────────────────────────────────────
    // Arcven เป็น Melee ที่ปล่อย Arc Wave ออกนอกร่าง
    // bDispatchesActionPing = true สำหรับ Arcven (Vael ออกนอกร่าง)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Arcven",
        meta = (EditCondition = "Category == ETriggerCategory::Melee"))
    FWTBRArcvenParams ArcvenParams;

    // ── Nexil ─────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Nexil",
        meta = (EditCondition = "Category == ETriggerCategory::Trap"))
    FWTBRNexilParams NexilParams;

    // ── Solux ─────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Solux",
        meta = (EditCondition = "Category == ETriggerCategory::Gunner"))
    FWTBRSoluxParams SoluxParams;

    // ── Fulgrix ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Fulgrix",
        meta = (EditCondition = "Category == ETriggerCategory::Gunner"))
    FWTBRFulgrixParams FulgrixParams;

    // ── Venyx ─────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Venyx",
        meta = (EditCondition = "Category == ETriggerCategory::Gunner"))
    FWTBRVenyxParams VenyxParams;

    // ── Serpveil ──────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Serpveil",
        meta = (EditCondition = "Category == ETriggerCategory::Gunner"))
    FWTBRSerpveilParams SerpveilParams;

    // ── Acervyn ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Acervyn",
        meta = (EditCondition = "Category == ETriggerCategory::Gunner"))
    FWTBRAcervynParams AcervynParams;

    // ── Telorn ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Telorn",
        meta = (EditCondition = "Category == ETriggerCategory::SniperBullet"))
    FWTBRTelornParams TelornParams;

    // ── Piercex ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Piercex",
        meta = (EditCondition = "Category == ETriggerCategory::SniperBullet"))
    FWTBRPiercexParams PiercexParams;

    // ── Fulgris ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Fulgris",
        meta = (EditCondition = "Category == ETriggerCategory::SniperBullet"))
    FWTBRFulgrisParams FulgrisParams;

    // ── Ventryx ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Ventryx",
        meta = (EditCondition = "Category == ETriggerCategory::BlackTrigger"))
    FWTBRVentryxParams VentryxParams;

    // ── Fulgorn ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Fulgorn",
        meta = (EditCondition = "Category == ETriggerCategory::BlackTrigger"))
    FWTBRFulgornParams FulgornParams;

    // ── Solvarn ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Solvarn",
        meta = (EditCondition = "Category == ETriggerCategory::BlackTrigger"))
    FWTBRSolvarnParams SolvarnParams;

    // ── Kaldrix ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Kaldrix",
        meta = (EditCondition = "Category == ETriggerCategory::BlackTrigger"))
    FWTBRKaldrixParams KaldrixParams;

    // ── Nyxveil ───────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Nyxveil",
        meta = (EditCondition = "Category == ETriggerCategory::BlackTrigger"))
    FWTBRNyxveilParams NyxveilParams;

    // ── Vexorn ────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Vexorn",
        meta = (EditCondition = "Category == ETriggerCategory::Defense"))
    FWTBRVexornParams VexornParams;

    // ─── Action Ping ─────────────────────────────────────────────────────────

    // True when Vael exits the character capsule on activation → radar ping (GDD §3.5)
    // False for melee, shield gank, etc.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Action Ping")
    bool bDispatchesActionPing = false;

    // ─── Composite Bullet (DualGesture Gunner × Gunner merge) ────────────────
    // All four maps keyed by EWTBRCompositeBulletType — every entry is data-driven.
    // Leave maps empty on non-Gunner DataAssets.

    // Projectile class to spawn when the merge timer completes
    UPROPERTY(EditDefaultsOnly, Category = "Trigger | Composite")
    TMap<EWTBRCompositeBulletType, TSubclassOf<AWTBRProjectileBase>> CompositeProjectileClasses;

    // Vael cost deducted immediately on merge start (non-refundable on interrupt)
    UPROPERTY(EditDefaultsOnly, Category = "Trigger | Composite")
    TMap<EWTBRCompositeBulletType, float> CompositeVaelCosts;

    // Charge duration (seconds) before the composite fires — replaces any hardcoded value
    UPROPERTY(EditDefaultsOnly, Category = "Trigger | Composite")
    TMap<EWTBRCompositeBulletType, float> CompositeMergeTimes;

    // Base damage passed to InitializeProjectile on the spawned composite projectile
    UPROPERTY(EditDefaultsOnly, Category = "Trigger | Composite")
    TMap<EWTBRCompositeBulletType, float> CompositeDamages;

    // ─── Visuals ─────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Visuals")
    TSoftObjectPtr<USkeletalMesh> TriggerMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Visuals")
    TSoftClassPtr<AActor> ProjectileClass;

    // ─── Animation ───────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Animation")
    TSoftObjectPtr<UAnimMontage> ActivationMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Animation", meta=(EditCondition="bSupportsDualWield"))
    TSoftObjectPtr<UAnimMontage> DualWieldActivationMontage;

    // ─── Asset Manager ───────────────────────────────────────────────────────

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("WTBRTrigger", GetFName());
    }
};
