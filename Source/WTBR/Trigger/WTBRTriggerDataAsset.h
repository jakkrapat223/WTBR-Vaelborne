// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "VFX/WTBRProjectileVFXTypes.h"

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
// The 4 Gunner archetypes that can be paired into a Composite Bullet (see
// EWTBRCompositeBulletType / the composite pair resolver, Step 4 of the build
// order). NonCombinable = a Gunner-category Trigger deliberately excluded from
// merging (e.g. Acervyn — standalone advanced Gunner, see WTBRAcervynTrigger.h).
// None = not yet classified; treat the same as NonCombinable until an owner
// sets a real value on that Trigger's DataAsset.
UENUM(BlueprintType)
enum class EWTBRBulletArchetype : uint8
{
    None          UMETA(DisplayName = "None (unclassified)"),
    Solux         UMETA(DisplayName = "Solux"),
    Fulgrix       UMETA(DisplayName = "Fulgrix"),
    Venyx         UMETA(DisplayName = "Venyx"),
    Serpveil      UMETA(DisplayName = "Serpveil"),
    NonCombinable UMETA(DisplayName = "Non-Combinable"),
};

UENUM(BlueprintType)
enum class EWTBRCompositeBulletType : uint8
{
    None     UMETA(DisplayName = "None"),
    Solgrix  UMETA(DisplayName = "Solgrix  (Solux + Fulgrix)"),
    Dualux   UMETA(DisplayName = "Dualux   (Solux + Solux)"),
    Solveil  UMETA(DisplayName = "Solveil  (Solux + Serpveil)"),
    Solhunt  UMETA(DisplayName = "Solhunt  (Solux + Venyx)"),
    Venspire UMETA(DisplayName = "Venspire (Venyx + Venyx)"),
    Fulgvyn  UMETA(DisplayName = "Fulgvyn  (Fulgrix + Venyx)"),
    Coilvyn  UMETA(DisplayName = "Coilvyn  (Serpveil + Venyx)"),
    Catarix  UMETA(DisplayName = "Catarix  (Fulgrix + Fulgrix)"),
    Labyrn   UMETA(DisplayName = "Labyrn   (Serpveil + Serpveil)"),
    Ignivex  UMETA(DisplayName = "Ignivex  (Fulgrix + Serpveil)"),
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

// FWTBRFeryxParams — Feryx short-blade tap / thrown blade-star hold.
// Hold values live here (rather than in code) so combat tuning stays DataAsset-driven.
USTRUCT(BlueprintType)
struct FWTBRFeryxParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold", meta = (ClampMin = "0.05"))
    float HoldThreshold = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold")
    TSubclassOf<AWTBRProjectileBase> StarProjectileClass;

    // Single large blade-star per throw (canon: Team Yuma vs Team Ninomiya) —
    // NOT a multi-projectile fan spread.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold", meta = (ClampMin = "0.0"))
    float StarThrowVaelCost = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold", meta = (ClampMin = "0.0"))
    float StarDamage = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold", meta = (ClampMin = "100.0"))
    float StarSpeed = 2600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Hold", meta = (ClampMin = "100.0"))
    float StarRange = 1800.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Bleed", meta = (ClampMin = "0.0"))
    float BleedDamagePerTick = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Bleed", meta = (ClampMin = "0.0"))
    float BleedDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Brittle", meta = (ClampMin = "1.0"))
    float BrittleDamageMultiplier = 1.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feryx | Brittle", meta = (ClampMin = "0.0"))
    float BrittleDuration = 2.0f;
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

    // จำนวน Trap ที่วางพร้อมกันได้สูงสุด — เกินแล้วอันเก่าสุดจะถูกลบ (FIFO, ตาม
    // convention เดียวกับ NexilParams.MaxWires) เจ้าของโปรเจกต์บอกว่า "2-3 อัน"
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Trap",
        meta = (ClampMin = "1"))
    int32 MaxActiveTraps = 3;

    // ระยะสูงสุดที่จะยิงเส้นเล็งไปข้างหน้าเพื่อหาจุดวาง Trap (กรณีเล็งพื้น/สภาพแวดล้อม)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Trap",
        meta = (ClampMin = "100.0"))
    float TrapPlacementRange = 1000.0f;

    // รัศมีค้นหาเพื่อนร่วมทีมตอนกด Hold (apply ตรงให้เพื่อน ไม่ต้อง surface check)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Hold",
        meta = (ClampMin = "100.0"))
    float FriendlySearchRadius = 800.0f;

    // มุมกรวยเล็ง (องศา) สำหรับตรวจว่ากำลังเล็งเพื่อนร่วมทีมอยู่หรือไม่
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Voltis | Hold",
        meta = (ClampMin = "1.0", ClampMax = "180.0"))
    float FriendlyAimConeHalfAngleDegrees = 25.0f;
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

// One entry in an Escudo hold-mode preset: a named group of panels, each
// placed at a lateral(right)/vertical(up) offset from the placement anchor,
// in the plane perpendicular to the placement aim direction. Owner-authored
// in the DataAsset (see FWTBREscudoParams::EscudoPresets) — Claude does not
// design preset shapes, only the data structure and the system that uses it
// (wtbr-escudo-hold-preset-design-lock project memory).
USTRUCT(BlueprintType)
struct FWTBREscudoPreset
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Escudo | Preset")
    FText PresetName;

    // One entry per panel, in world units, on the GROUND PLANE relative to the
    // placement anchor: X = lateral (local right), Y = forward (local
    // forward). Deliberately not a vertical axis — Escudo panels must sprout
    // from a supporting surface (EscudoSurfaceSnapRange), so a stacked/
    // floating panel could never validate anyway, and a ground-plane layout
    // is what the on-floor placement preview shows.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Escudo | Preset")
    TArray<FVector2D> PanelOffsets;
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

    // ระยะ line-trace หาพื้นผิวรองรับจุดวาง (ยิงสวนทางกับ normal ของพื้นผิว ไม่ใช่ลงล่าง
    // อย่างเดียว) — ไม่เจอพื้นผิวในระยะนี้ = วางไม่ได้ ไม่เสีย Vael
    // (canon: Escudo ต้องงอกจากพื้นผิว ห้ามลอยกลางอากาศ)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "50.0"))
    float EscudoSurfaceSnapRange = 300.0f;

    // ระยะเล็ง Aim-to-Place สูงสุดของ Hold/Preset mode (GDD §5.8 "Aim-to-Place +
    // Ghost Preview UI ก่อนวาง"). Tap ไม่ใช้ค่านี้ — Tap วางหน้าตัวละครระยะคงที่เสมอ
    //
    // ⚠ PLAYTEST PENDING — canon ไม่เคยระบุระยะ Escudo เป็นตัวเลข ค่านี้ประมาณจาก
    // Map Metrics ใน GDD: ครอบคลุม CQC zone ทั้งช่วง (0-800u) บวกระยะเผื่อให้ปิด
    // Choke Point (120-200u) ได้ก่อนศัตรูเข้าถึง แต่ไม่ยาวถึง Mid-range (800-3,000u)
    // ซึ่งเกินบทบาท Escudo ที่เป็นเครื่องมือป้องกันระยะประชิด-กลาง
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Placement",
        meta = (ClampMin = "100.0"))
    float EscudoPlacementRange = 1200.0f;

    // แรงกระแทกตอนกำแพงงอก (u/s) — canon: Hyuse ดีด Ikoma
    // damage = 0 เสมอ (Escudo Slam lock: displacement, NOT damage)
    //
    // ใช้ค่าเดียวกับทุกคนที่โดนกำแพงงอกผ่าน ทั้งตัวเอง เพื่อน และศัตรู
    // (owner's call 2026-07-20) — เดิมแยกเป็น EscudoAllyPushImpulse (600) กับ
    // EscudoEnemyLaunchImpulse (1200) ตอนที่เพื่อนถูกผลักไปหลังกำแพงแทนที่จะถูกดีด
    // พอทุกคนโดนเหมือนกันหมดแล้ว การมีสองค่าก็ไม่มีความหมาย
    //
    // ทิศทางไม่ได้ตายตัวเป็นแนวตั้ง — มันวิ่งตามแนวที่กำแพงงอกออกมา (surface normal)
    // งอกจากพื้น = ดีดขึ้นฟ้า, งอกจากกำแพง = กระแทกแนวนอนเหมือนรถชน, งอกจากเพดาน = อัดลงพื้น
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Displacement",
        meta = (ClampMin = "0.0"))
    float EscudoEruptionImpulse = 1200.0f;

    // สัดส่วนแรงผลักออกด้านข้าง เทียบกับ EscudoEruptionImpulse (0 = ไม่ผลักออกเลย)
    //
    // จำเป็นเฉพาะตอนกำแพงงอกขึ้นแนวตั้ง เพราะแรงโน้มถ่วงจะดึงเป้าหมายกลับลงมา
    // ตกใส่กำแพงที่งอกเสร็จแล้วพอดี (ฝังอยู่ในกำแพง) แรงผลักออกช่วยให้พ้นแนวกำแพงก่อนตก
    // ตอนงอกแนวนอนไม่ต้องใช้ เพราะเป้าหมายลอยพ้นไปแล้วและแรงโน้มถ่วงไม่ดึงกลับ —
    // โค้ดจึงลดค่านี้ลงอัตโนมัติตามความชันของแนวที่กำแพงงอก
    // ⚠ PLAYTEST PENDING — ลดจาก 0.3 เพราะ owner พบว่าดีดไปไกลเกินไป (2026-07-20)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Displacement",
        meta = (ClampMin = "0.0"))
    float EscudoEruptionClearanceRatio = 0.12f;

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

    // ระยะห่างขั้นต่ำ (3 มิติ) จากกำแพง Escudo/Aegorn ที่มีอยู่แล้ว — วางใกล้
    // กว่านี้ = วางไม่ได้ ไม่เสีย Vael (anti-abuse: ป้องกันการวางกำแพงซ้อนกันจน
    // ล้อมตัวเองเป็นที่หลบซ่อนถาวร). วางเรียงข้างกันแบบไม่ทับกันยังทำได้ตามปกติ
    // วัดแบบ 3 มิติ ไม่ใช่แนวราบ — พอรองรับการวางบนกำแพง/เพดานแล้ว การวัดแนวราบ
    // จะทำให้แผงบนเพดานบล็อกแผงบนพื้นที่อยู่ใต้มันพอดี ทั้งที่ห่างกันคนละชั้น
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Wall",
        meta = (ClampMin = "0.0"))
    float EscudoMinWallSpacing = 300.0f;

    // Hold-mode preset library (radial wheel, see wtbr-escudo-hold-preset-
    // design-lock project memory). Owner-authored in the DataAsset — the
    // shapes/layouts are a design decision, not something this array
    // hardcodes. Seeded with exactly one default entry (below) purely so the
    // hold→wheel→ghost-preview→confirm pipeline is testable before the owner
    // designs real presets.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Escudo | Preset")
    TArray<FWTBREscudoPreset> EscudoPresets;

    FWTBREscudoParams()
    {
        FWTBREscudoPreset SinglePanel;
        SinglePanel.PresetName = FText::FromString(TEXT("Single Panel"));
        SinglePanel.PanelOffsets.Add(FVector2D::ZeroVector);
        EscudoPresets.Add(SinglePanel);

        // Temporary test presets (owner's own request, 2026-07-20, to prove
        // the wheel/ghost-preview/confirm pipeline end-to-end with more than
        // one preset before designing real ones) — NOT final game design.
        // 220 units of lateral spacing sits panels roughly edge-to-edge for
        // the default 200-wide EscudoWallSize. Panels within one preset are
        // never spacing-checked against each other (ValidatePanelPlacement
        // only compares against already-SPAWNED walls, and none of a
        // preset's own panels exist yet at validation time) — worth knowing
        // when authoring real presets, since a too-tight preset can still
        // spawn visibly overlapping panels.
        FWTBREscudoPreset LineOfThree;
        LineOfThree.PresetName = FText::FromString(TEXT("Line of 3"));
        LineOfThree.PanelOffsets = { FVector2D(-220.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(220.0f, 0.0f) };
        EscudoPresets.Add(LineOfThree);

        // Ground-plane L: two panels across the front, one turning back along
        // the right side. NOTE all panels in a preset currently share the
        // anchor's single facing rotation, so the side panel faces the same
        // way as the front ones rather than turning to face outward — a real
        // per-panel rotation would be needed for a true wrap-around corner.
        // Flagging as a known limitation for whoever designs the real presets.
        FWTBREscudoPreset LShape;
        LShape.PresetName = FText::FromString(TEXT("L Shape"));
        LShape.PanelOffsets = { FVector2D(-110.0f, 0.0f), FVector2D(110.0f, 0.0f), FVector2D(110.0f, -220.0f) };
        EscudoPresets.Add(LShape);
    }
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

    // ระยะเล็งสูงสุดจากมุมกล้องถึงจุดปักหมุดแต่ละจุด — Cypher-style two-point
    // placement (owner ask 2026-07-19): เล็งไปที่พื้น/กำแพงข้างหน้าแล้วปักหมุด
    // ทีละจุด. Convention เดียวกับ VoltisParams.TrapPlacementRange.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "100.0"))
    float NexilPlacementRange = 800.0f;

    // ระยะห่างสูงสุดระหว่างหมุด A กับ B (ความยาว wire สูงสุดที่ลากได้) — Cypher
    // two-point placement. มุม/ความยาวจริงมาจากตำแหน่งสองหมุด, ค่านี้คือเพดาน.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "100.0"))
    float NexilMaxWireSpan = 800.0f;

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

    // ระยะเวลาที่เส้นอยู่บนพื้นก่อน auto-despawn. Owner set 20-30s target
    // 2026-07-15 (default 30 here; DA-overridable). GDD reference was 45s.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "1.0"))
    float WireDuration = 30.0f;

    // HP ของเส้น (GDD: 1 — ฟันหรือยิงขาดได้). Hit-count, not a damage-amount pool:
    // any qualifying hit removes exactly 1, regardless of the weapon's own damage
    // number. Wired to gunfire/explosion contact (AWTBRNexilWireActor::
    // OnWireOverlapBegin) and melee contact (UWTBRMeleeTrigger::ApplyDamageToHits
    // + UWTBRMantornTrigger's Whip/Spin loops) — both routes call the same
    // AWTBRNexilWireActor::TakeHit().
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Wire",
        meta = (ClampMin = "1"))
    int32 WireHP = 1;

    // Vael charged per wire placed. Canon GDD §5.2 (Spider tripwire) = 15/wire.
    // Base UWTBRTriggerBase::Activate consumes nothing, so this is charged
    // explicitly in UWTBRNexilTrigger::Activate (fail-closed: no Vael → no wire).
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Cost",
        meta = (ClampMin = "0.0"))
    float NexilVaelCost = 15.0f;

    // Zipline ability: F-grab a placed wire (owner + teammates only — the trip
    // mechanic for enemies is unaffected), release via Jump to launch toward
    // wherever the camera is aimed at that moment. Wire persists after use —
    // reusable by anyone on the team until it expires or an enemy cuts it.
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Nexil | Zipline",
        meta = (ClampMin = "0.0"))
    float NexilZiplineLaunchSpeed = 2200.0f;
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

    // GDD Sub-18.1 states Egret's Max Range as a flat 8,000u with no falloff
    // (not flagged playtest-pending in that doc, unlike its charge-damage/Glint
    // Cone numbers) — synced to that value 2026-07-17, was drifted to 10000.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | Projectile", meta = (ClampMin = "100.0"))
    float TelornRange = 8000.0f;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Telorn | VFX")
    FWTBRProjectileVFXConfig VFX;
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

    // GDD Sub-18.2 states Ibis's projectile speed as a flat 3,000u/s ("slower
    // than Egret — dodgeable"), not flagged playtest-pending in that doc —
    // synced to that value 2026-07-17, was drifted to 5000.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | Projectile", meta = (ClampMin = "100.0"))
    float PiercexSpeed = 3000.0f;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Piercex | VFX")
    FWTBRProjectileVFXConfig VFX;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRFulgrisParams — Fulgris (Lightning — Fastest Sniper, lower damage)
// Note: distinct from FWTBRFulgrixParams (Fulgrix = explosive Gunner)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRFulgrisParams
{
    GENERATED_BODY()

    // Lowest damage among snipers — compensated by highest speed (GDD). GDD
    // Sub-18.3 states Lightning's damage as a flat 30 (tap and hold equal, it
    // never had charge tiers even before aim-then-fire superseded the other
    // two snipers' charge concept) — synced to that value 2026-07-17, was
    // drifted to 40.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | Combat", meta = (ClampMin = "0.0"))
    float FulgrisDamage = 30.0f;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgris | VFX")
    FWTBRProjectileVFXConfig VFX;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRSoluxParams — Solux (Asteroid — Normal Bullet)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRSoluxParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING. Damage for ONE tap, SPLIT across the volley below —
    // never per cube. Replaces the old single-shot SoluxDamage (30), which was
    // deleted rather than reused: an existing DA would have carried that stale
    // single-shot number in as the new total and quietly made Solux deal a third
    // of what it should, with nothing to show for it.
    //
    // Solux is the raw-damage archetype and must stay clearly above Fulgrix, which
    // also has AOE. See the locked hierarchy: Solux > Fulgrix > Venyx = Serpveil.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "0.0"))
    float SoluxTotalDamage = 100.0f;

    // ⚠ PLAYTEST PENDING: cubes the conjured block splits into on tap.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "1"))
    int32 SoluxTapCubeCount = 8;

    // ⚠ PLAYTEST PENDING: radius of the sphere the cubes are conjured on, around
    // the muzzle.
    //
    // Deliberately TIGHT. A tap is meant to read as one shot that happens to be made
    // of cubes; conjuring them a metre and a half apart made the volley visibly
    // funnel inward on its way to the target, which the owner read as the shot
    // drifting rather than as a formation.
    //
    // It used to be 135 to keep sibling cubes from spawning inside each other and
    // clashing. That was a symptom of OwnerInstigator being assigned after
    // FinishSpawning; with the assignment moved earlier the siblings recognise each
    // other immediately and the spread no longer has to carry that job.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "0.0"))
    float SoluxTapScatterRadius = 70.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile", meta = (ClampMin = "100.0"))
    float SoluxSpeed = 3000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile", meta = (ClampMin = "100.0"))
    float SoluxRange = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Projectile")
    TSubclassOf<AWTBRProjectileBase> SoluxProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Combat", meta = (ClampMin = "0.05"))
    float SoluxFireCooldown = 0.5f;

    // ── Hold / presets ───────────────────────────────────────────────────────
    //
    // Solux carries no per-cube ability at all, so its presets are pure SHAPE and
    // TIMING — where each lane ends and when it launches. That is the whole point:
    // hold buys control, never power, so the damage above is what a preset spends
    // too.
    //
    // Every lane's final waypoint is a FRACTION of the committed range, so a lane
    // ending at 0.5 stops halfway however far the player charged. One lane can run
    // to the full reach while another waits short to catch someone walking in.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Presets")
    TArray<FWTBRPathPreset> SoluxPresets;

    // ⚠ PLAYTEST PENDING: reach at ZERO charge; SoluxRange is the full-charge reach.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Presets",
        meta = (ClampMin = "100.0"))
    float SoluxPresetMinRange = 2000.0f;

    // ⚠ PLAYTEST PENDING: seconds of held charge that reach full range.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Presets",
        meta = (ClampMin = "0.05"))
    float SoluxPresetFullChargeSeconds = 1.2f;

    // ⚠ PLAYTEST PENDING.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Presets",
        meta = (ClampMin = "0.0"))
    float SoluxPresetVaelCost = 20.0f;

    // ⚠ PLAYTEST PENDING: spread of the conjure points. Non-zero is not cosmetic —
    // cubes spawned on one spot overlap and destroy each other before they move.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Solux | Presets",
        meta = (ClampMin = "0.0"))
    float SoluxPresetScatterRadius = 135.0f;

    FWTBRSoluxParams()
    {
        // ⚠ PLACEHOLDER TEST DATA, NOT FINAL DESIGN. Waypoints are fractions of the
        // committed range: X forward, Y lateral, Z vertical.
        auto MakeLane = [](const TArray<FVector>& Waypoints, int32 CubeCount, float Delay)
        {
            FWTBRPathLane Lane;
            Lane.NormalizedWaypoints = Waypoints;
            Lane.CubeCount = CubeCount;
            Lane.LaunchDelay = Delay;
            return Lane;
        };

        // Lance — four cubes now, four more two seconds later, along the same line.
        // The delayed half is the whole idea: the first volley makes someone commit
        // to cover or a dodge, and the second arrives after they have.
        FWTBRPathPreset Lance;
        Lance.PresetId = FName(TEXT("Lance"));
        Lance.DisplayName = FText::FromString(TEXT("Lance (four now, four late)"));
        Lance.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)}, 4, 0.0f));
        Lance.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)}, 4, 2.0f));
        SoluxPresets.Add(Lance);

        // Fan — flat spread, everything at once. Kept deliberately plain: it exists
        // to prove the preset flow end to end, not because a fan is good play.
        FWTBRPathPreset Fan;
        Fan.PresetId = FName(TEXT("Fan"));
        Fan.DisplayName = FText::FromString(TEXT("Fan (test shape)"));
        for (int32 i = 0; i < 8; ++i)
        {
            const float Lateral = (i - 3.5f) * 0.09f;
            Fan.Lanes.Add(MakeLane(
                {FVector::ZeroVector, FVector(1.0f, Lateral, 0.0f)}, 1, 0.0f));
        }
        SoluxPresets.Add(Fan);

        // Hammer — straight up, then down onto the target. Clears anything the
        // player cannot shoot through, and the lanes deliberately end at DIFFERENT
        // fractions of the range so the shot covers a depth rather than a point:
        // the short lanes come down in front of someone advancing, the long one
        // lands on where they are now.
        // The arc ends pointing DOWN and the cube carries on along that heading once
        // the authored path runs out — a bullet only ever stops by hitting somebody,
        // hitting the world, or running out of range. So the last leg only has to
        // aim the dive; it does not have to reach the floor by itself.
        FWTBRPathPreset Hammer;
        Hammer.PresetId = FName(TEXT("Hammer"));
        Hammer.DisplayName = FText::FromString(TEXT("Hammer (over cover, from above)"));
        Hammer.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.12f, 0.00f, 0.70f), FVector(0.45f, 0.00f, 0.0f)}, 3, 0.0f));
        Hammer.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.20f, -0.06f, 0.78f), FVector(0.70f, -0.06f, 0.0f)}, 3, 0.35f));
        Hammer.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.28f, 0.06f, 0.86f), FVector(1.00f, 0.06f, 0.0f)}, 2, 0.70f));
        SoluxPresets.Add(Hammer);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRFulgrixParams — Fulgrix (Meteor — Explosive Bullet)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRFulgrixParams
{
    GENERATED_BODY()

    // ⚠ PLAYTEST PENDING. Damage for ONE tap, SPLIT across the volley — never per
    // cube. Replaces the old single-shot FulgrixDamage (80); see the note on
    // SoluxTotalDamage for why the old field was deleted rather than reused.
    //
    // Must stay clearly BELOW Solux: Fulgrix gets AOE, the slowest speed and the
    // shortest range as its compensation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Combat", meta = (ClampMin = "0.0"))
    float FulgrixTotalDamage = 90.0f;

    // ⚠ PLAYTEST PENDING: cubes the conjured block splits into on tap.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Combat", meta = (ClampMin = "1"))
    int32 FulgrixTapCubeCount = 8;

    // ⚠ PLAYTEST PENDING: radius of the conjure sphere around the muzzle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Combat", meta = (ClampMin = "0.0"))
    float FulgrixTapScatterRadius = 70.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Projectile", meta = (ClampMin = "100.0"))
    float FulgrixSpeed = 2500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Projectile", meta = (ClampMin = "100.0"))
    float FulgrixRange = 4000.0f;

    // ⚠ PLAYTEST PENDING. Kept for the hold/preset and composite paths, which still
    // fire a single large blast.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Explosion", meta = (ClampMin = "50.0"))
    float FulgrixExplosionRadius = 300.0f;

    // ⚠ PLAYTEST PENDING. Per-cube blast radius on tap. Every cube explodes (canon:
    // Meteor cubes each detonate), so the radius must come DOWN or eight overlapping
    // 300uu blasts would cover far more ground than the one they replaced.
    //
    // 110 is not arbitrary: blast area scales with r^2, so eight blasts matching the
    // total area of one 300uu blast want 300/sqrt(8) ~= 106.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Explosion", meta = (ClampMin = "10.0"))
    float FulgrixTapExplosionRadius = 110.0f;

    // ⚠ PLAYTEST PENDING: how far off the aim point the cubes land, in world units.
    //
    // Solux and Venyx converge on exactly one point, which is what makes a tap
    // reliable. Fulgrix must NOT: cubes landing on one spot would stack eight
    // reduced blasts into the footprint of a single small one, leaving Fulgrix
    // strictly worse than before it split. Spreading the impacts is what turns the
    // volley into the cluster the reduced radius is priced for.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Explosion", meta = (ClampMin = "0.0"))
    float FulgrixTapImpactSpread = 200.0f;

    // ⚠ PLACEHOLDER TEST DATA, NOT GAME DESIGN. Meteor's hold presets are about
    // DIRECTION and ANGLE (the design lock's "front-2/back-2"), not about the
    // curving trajectory control Viper owns — waypoints are 3D, so a lane can send
    // its cubes behind the caster just as easily as ahead.
    //
    // Consumed by the Meteo composites (Solgrix, Catarix) through the same
    // FWTBRPathPreset resolver Viper uses. Leave FormationOffset at zero — it is
    // dead weight on this path anyway: composites always pass a non-zero
    // TapScatterRadius, and ResolvePathPreset uses scatter OR FormationOffset,
    // never both. Waypoints are measured from each cube's own scattered origin,
    // so "shared" waypoints converge to a cluster the width of that scatter
    // radius rather than to a single point. That is deliberate: cubes conjured on
    // top of each other destroy each other on contact (see TapScatterRadius).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Presets")
    TArray<FWTBRPathPreset> FulgrixPresets;

    FWTBRFulgrixParams()
    {
        auto MakeLane = [](const TArray<FVector>& Waypoints, int32 Weight)
        {
            FWTBRPathLane Lane;
            Lane.NormalizedWaypoints = Waypoints;
            Lane.CubeCount = Weight;
            return Lane;
        };

        // All cubes are conjured at one point and fly OUTWARD from it — the lanes are
        // launch directions, not curves. Viper owns trajectory shaping; Meteor owns
        // where the blasts land.
        //
        // ⚠ These SPREADING shapes suit Catarix (round blasts, area denial) far
        // better than Solgrix. Solgrix is a shaped charge: its cone points along
        // each cube's own travel and only damages what is AHEAD of that blast, so
        // cubes fanned past a target explode with it 60-95 degrees off-axis and deal
        // nothing. A real shaped charge fires forward, it does not sweep across.
        // Solgrix wants shapes that CONVERGE on the aim point — exactly what its own
        // tap already does. Confirmed in PIE 2026-07-21: the cone was rejecting
        // correctly, the preset was just the wrong tool for that composite.
        FWTBRPathPreset Spread;
        Spread.PresetId = FName(TEXT("WideSpread"));
        Spread.DisplayName = FText::FromString(TEXT("Wide Spread"));
        Spread.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f, -0.45f, 0.0f)}, 1));
        Spread.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f,  0.00f, 0.0f)}, 1));
        Spread.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f,  0.45f, 0.0f)}, 1));
        FulgrixPresets.Add(Spread);

        // The canon shot from the owner's reference: cubes are conjured at one point,
        // then half fire FORWARD and half fire BACKWARD to catch enemies on both
        // sides at once. This is the design lock's "front-2/back-2", generalised —
        // at CubeCount 8 the four equal lanes give exactly 4 ahead and 4 behind.
        //
        // Each side keeps a slight fan so the blasts cover ground instead of stacking
        // on one line. Only the sign of X separates the two halves.
        FWTBRPathPreset Pincer;
        Pincer.PresetId = FName(TEXT("Pincer"));
        Pincer.DisplayName = FText::FromString(TEXT("Pincer (front and back)"));
        Pincer.Lanes.Add(MakeLane({FVector::ZeroVector, FVector( 1.0f, -0.20f, 0.0f)}, 1));
        Pincer.Lanes.Add(MakeLane({FVector::ZeroVector, FVector( 1.0f,  0.20f, 0.0f)}, 1));
        Pincer.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(-1.0f, -0.20f, 0.0f)}, 1));
        Pincer.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(-1.0f,  0.20f, 0.0f)}, 1));
        FulgrixPresets.Add(Pincer);

        // Same idea turned into a full ring — every direction at once, for being
        // surrounded rather than pinched from two sides.
        FWTBRPathPreset Ring;
        Ring.PresetId = FName(TEXT("Ring"));
        Ring.DisplayName = FText::FromString(TEXT("Ring"));
        Ring.Lanes.Add(MakeLane({FVector::ZeroVector, FVector( 1.0f,  0.0f, 0.0f)}, 1));
        Ring.Lanes.Add(MakeLane({FVector::ZeroVector, FVector( 0.0f,  1.0f, 0.0f)}, 1));
        Ring.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(-1.0f,  0.0f, 0.0f)}, 1));
        Ring.Lanes.Add(MakeLane({FVector::ZeroVector, FVector( 0.0f, -1.0f, 0.0f)}, 1));
        FulgrixPresets.Add(Ring);

        // Converge-then-diverge: fan out wide, cross at one point, fan out again.
        // The only SHAPE in this list that suits Solgrix — its shaped charge fires
        // along each cube's own travel, so cubes still closing on the aim point at
        // detonation all point the same way, which the spreading presets above
        // cannot do. On Catarix it reads as a focused two-beat instead of area
        // denial.
        //
        // Three properties are load-bearing; changing a number without keeping them
        // breaks the shape rather than retuning it:
        //
        // 1. Every lane is the same LENGTH. Duration is TotalDist/Speed per cube, so
        //    equal length is the only reason the cubes reach the crossing together
        //    instead of trickling through it. The four spread directions are all the
        //    same magnitude for exactly this reason — they differ in angle only.
        // 2. Nothing dips below the aim line. The spread fans across the UPPER half
        //    only (0 / 60 / 120 / 180 degrees). A symmetric full circle would be the
        //    prettier shape, but at PathRange a downward lane puts cubes hundreds of
        //    units under the muzzle and they detonate on terrain before crossing.
        // 3. Each lane exits MIRRORED across the fan, so the paths genuinely cross.
        //    Exit on the same side and the shape is a pinch, not a crossing.
        //
        // Cubes do not collide at the crossing: each carries its own scatter offset,
        // so they pass through a cluster the width of TapScatterRadius, not a point.
        FWTBRPathPreset Converge;
        Converge.PresetId = FName(TEXT("Converge"));
        Converge.DisplayName = FText::FromString(TEXT("Converge (cross and split)"));
        Converge.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.28f,  0.350f, 0.000f), FVector(0.60f, 0.0f, 0.0f),
            FVector(1.00f, -0.300f, 0.000f)}, 1));
        Converge.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.28f,  0.175f, 0.303f), FVector(0.60f, 0.0f, 0.0f),
            FVector(1.00f, -0.150f, 0.260f)}, 1));
        Converge.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.28f, -0.175f, 0.303f), FVector(0.60f, 0.0f, 0.0f),
            FVector(1.00f,  0.150f, 0.260f)}, 1));
        Converge.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.28f, -0.350f, 0.000f), FVector(0.60f, 0.0f, 0.0f),
            FVector(1.00f,  0.300f, 0.000f)}, 1));
        FulgrixPresets.Add(Converge);
    }

    // ⚠ PLAYTEST PENDING: reach at ZERO charge; FulgrixRange is the full-charge reach.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Presets",
        meta = (ClampMin = "100.0"))
    float FulgrixPresetMinRange = 1600.0f;

    // ⚠ PLAYTEST PENDING: seconds of held charge that reach full range.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Presets",
        meta = (ClampMin = "0.05"))
    float FulgrixPresetFullChargeSeconds = 1.2f;

    // ⚠ PLAYTEST PENDING.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Presets",
        meta = (ClampMin = "0.0"))
    float FulgrixPresetVaelCost = 24.0f;

    // ⚠ PLAYTEST PENDING: spread of the conjure points. Non-zero is not cosmetic —
    // cubes spawned on one spot overlap and destroy each other before they move.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fulgrix | Presets",
        meta = (ClampMin = "0.0"))
    float FulgrixPresetScatterRadius = 135.0f;

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

    // ⚠ PLAYTEST PENDING. Damage for ONE tap, SPLIT across the volley — never per
    // cube. Replaces the old single-shot VenyxDamage (25); see the note on
    // SoluxTotalDamage for why the old field was deleted rather than reused.
    //
    // Venyx and Serpveil are deliberate stat twins — equal damage, equal range —
    // differentiated purely by behaviour: Venyx homes, Serpveil's path is authored.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "0.0"))
    float VenyxTotalDamage = 85.0f;

    // ⚠ PLAYTEST PENDING: cubes the conjured block splits into on tap. Each one
    // homes independently, so this is also an actor-count number, not just balance.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "1"))
    int32 VenyxTapCubeCount = 8;

    // ⚠ PLAYTEST PENDING: radius of the conjure sphere around the muzzle.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "0.0"))
    float VenyxTapScatterRadius = 70.0f;

    // ⚠ PLAYTEST PENDING: how close an enemy must come to a travelling cube before
    // it peels off and chases.
    //
    // This is what makes a tap fly STRAIGHT and only hook at the end. The cube
    // acquires nothing at fire time; it sweeps as it goes, and a shot that passes
    // nobody simply flies on like an ordinary bullet. Locking a target up front
    // instead — which is what tap used to do — made every shot curve away from the
    // crosshair the instant it left, and made missing impossible.
    //
    // Measured in PIE on the preset sweeps: radii of 160-260uu missed by as little
    // as 6uu, so this wants to start generous rather than clever.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "0.0"))
    float VenyxTapHomingRadius = 500.0f;

    // ⚠ PLAYTEST PENDING: degrees per second a chasing cube may swing its heading.
    //
    // This is what makes an overshoot cost something. Uncapped, a cube that missed
    // pivoted on the spot and came straight back, again and again until it expired —
    // owner-observed in PIE, four cubes doing laps around one target.
    //
    // Turn radius is roughly Speed / TurnRate(radians): at 3500uu/s and 180 deg/s a
    // cube needs about 1100uu to come around. That wide return arc is deliberate and
    // is the shape the Venyx presets are being designed around, so treat this as a
    // design knob rather than a safety limit. Lower turns wider.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Combat", meta = (ClampMin = "0.0"))
    float VenyxHomingTurnRateDegreesPerSecond = 180.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile", meta = (ClampMin = "100.0"))
    float VenyxSpeed = 3500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile", meta = (ClampMin = "100.0"))
    float VenyxRange = 6000.0f;

    // Turn radius ≈ VenyxSpeed² / VenyxHomingAcceleration. Original 5000 was
    // ~24.5m radius (wider than Hound's own 800-3000u mid-range band — whiffed
    // on minor aim error). Tuned per GDD 5.7 "Soft-lock Pursuit" + Artemis's Eye
    // "Limited Turn Rate" philosophy (sharp right-angle turns still dodgeable,
    // but no whiff on slightly-off aim). Iterated in PIE: 5000→14000 (~8.75m)
    // visibly homed but owner still felt it weak → bumped to 22000 (~5.6m radius)
    // at owner request 2026-07-15 (this exact value not yet re-PIE'd).
    // ⚠ PLAYTEST PENDING (further tuning ok)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Homing", meta = (ClampMin = "100.0"))
    float VenyxHomingAcceleration = 22000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Projectile")
    TSubclassOf<AWTBRProjectileBase> VenyxProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Venyx | Combat",
        meta = (ClampMin = "0.1"))
    float VenyxFireCooldown = 0.6f;

    // Keeps Hound's soft-lock intentional rather than acquiring peripheral targets.
    // âš  PLAYTEST PENDING (35 degrees is a starting point, not final tuning)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Venyx | Homing",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float VenyxAimConeHalfAngleDegrees = 35.0f;

    // ⚠ PLACEHOLDER TEST DATA, NOT GAME DESIGN. Hound's hold presets are SEARCH
    // SWEEPS, not approaches to a target the shot already knows about. Nothing is
    // locked at fire time: each cube carries FWTBRPathLane::HomingRadius along its
    // arc and takes the first enemy that falls inside. The shape therefore decides
    // what ground gets hunted, which is also why these need no target-distribution
    // rule — lanes that sweep different ground find different people on their own.
    //
    // Two rules to keep when retuning, both found by simulating these shapes rather
    // than reasoning about them:
    //  1. A sweep must come back DOWN. An arc that stays airborne passes over
    //     everyone and hits nothing; under the old locked-target model the cube
    //     always dove at the end, so this failure could not happen.
    //  2. A late lane wants a WIDER radius than an early one. It arrives after the
    //     targets have scattered, so it is covering ground rather than a body.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Presets")
    TArray<FWTBRPathPreset> VenyxPresets;

    // ⚠ PLAYTEST PENDING: reach of a hold at ZERO charge; VenyxRange above is the
    // full-charge reach. Mirrors Serpveil's own MinRange -> MaxRange lerp, and for
    // the same reason: waypoints and homing radius are authored as fractions of
    // range, so charge scales the whole shape rather than only its length.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Presets",
        meta = (ClampMin = "100.0"))
    float VenyxPresetMinRange = 2000.0f;

    // ⚠ PLAYTEST PENDING: seconds of held charge to reach VenyxRange.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Presets",
        meta = (ClampMin = "0.05"))
    float VenyxPresetFullChargeSeconds = 1.2f;

    // ⚠ PLAYTEST PENDING: a swept volley costs more than the single tap missile,
    // since it covers ground and can catch several people.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Presets",
        meta = (ClampMin = "0.0"))
    float VenyxPresetVaelCost = 20.0f;

    // VenyxPresetTotalDamage was DELETED, not set equal to VenyxTotalDamage.
    //
    // Hold chooses a PATTERN, it does not buy power, so tap and hold spend the same
    // budget. Two fields that must always match is a field that will eventually stop
    // matching — someone tunes one in the editor and the lock quietly dies. One
    // number per archetype makes the rule structural instead of a comment.
    //
    // (It sat at 100 against a tap of 76 before this, which is exactly the drift
    // being designed out.)

    // ⚠ PLAYTEST PENDING: spread of the conjure points. Non-zero is not cosmetic —
    // cubes spawned on one spot overlap and destroy each other before they move.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Venyx | Presets",
        meta = (ClampMin = "0.0"))
    float VenyxPresetScatterRadius = 135.0f;

    FWTBRVenyxParams()
    {
        auto MakeLane = [](const TArray<FVector>& Waypoints, float Delay, float Radius)
        {
            FWTBRPathLane Lane;
            Lane.NormalizedWaypoints = Waypoints;
            Lane.CubeCount = 1;
            Lane.LaunchDelay = Delay;
            Lane.HomingRadius = Radius;
            return Lane;
        };

        // The owner's own scenario: fire high, drop three immediately so the enemy
        // commits to a block, then bring two more down wide and late onto whatever
        // broke away. Each lane is caster -> apex -> ground.
        FWTBRPathPreset Skyfall;
        Skyfall.PresetId = FName(TEXT("Skyfall"));
        Skyfall.DisplayName = FText::FromString(TEXT("Skyfall (bait then punish)"));
        Skyfall.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.34f, -0.18f, 0.58f), FVector(0.74f, -0.10f, 0.02f)}, 0.00f, 0.16f));
        Skyfall.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.35f,  0.00f, 0.64f), FVector(0.78f,  0.02f, 0.02f)}, 0.00f, 0.16f));
        Skyfall.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.34f,  0.18f, 0.58f), FVector(0.74f,  0.12f, 0.02f)}, 0.00f, 0.16f));
        Skyfall.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.34f, -0.34f, 0.52f), FVector(1.06f, -0.22f, 0.02f)}, 1.60f, 0.26f));
        Skyfall.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.34f,  0.34f, 0.52f), FVector(1.06f,  0.22f, 0.02f)}, 1.60f, 0.26f));
        VenyxPresets.Add(Skyfall);

        // Flat and wide, rippling outside-in. For flushing a room rather than
        // punishing one body: the staggered launches mean the sweep crosses the
        // ground over about a second instead of all at once.
        FWTBRPathPreset Sweep;
        Sweep.PresetId = FName(TEXT("Sweep"));
        Sweep.DisplayName = FText::FromString(TEXT("Sweep (lateral fan)"));
        Sweep.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.40f, -0.48f, 0.06f), FVector(0.92f, -0.26f, 0.05f)}, 0.00f, 0.18f));
        Sweep.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.40f, -0.22f, 0.22f), FVector(0.94f, -0.12f, 0.05f)}, 0.35f, 0.18f));
        Sweep.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.42f,  0.00f, 0.30f), FVector(0.96f,  0.00f, 0.05f)}, 0.70f, 0.18f));
        Sweep.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.40f,  0.22f, 0.22f), FVector(0.94f,  0.12f, 0.05f)}, 0.35f, 0.18f));
        Sweep.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.40f,  0.48f, 0.06f), FVector(0.92f,  0.26f, 0.05f)}, 0.00f, 0.18f));
        VenyxPresets.Add(Sweep);

        // Two lanes run PAST the aim point and hunt back from behind it, so cover
        // that stops a frontal sweep does not stop this one. The late overhead lane
        // covers the gap between them.
        FWTBRPathPreset Encircle;
        Encircle.PresetId = FName(TEXT("Encircle"));
        Encircle.DisplayName = FText::FromString(TEXT("Encircle (from behind)"));
        Encircle.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.30f, -0.54f, 0.12f), FVector(1.14f, -0.30f, 0.04f)}, 0.00f, 0.20f));
        Encircle.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.30f,  0.54f, 0.12f), FVector(1.14f,  0.30f, 0.04f)}, 0.00f, 0.20f));
        Encircle.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.46f,  0.00f, 0.48f), FVector(1.10f,  0.00f, 0.04f)}, 1.10f, 0.24f));
        Encircle.Lanes.Add(MakeLane({FVector::ZeroVector,
            FVector(0.34f, -0.30f, 0.32f), FVector(1.02f, -0.16f, 0.04f)}, 2.20f, 0.24f));
        VenyxPresets.Add(Encircle);
    }
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

    // Low per-cube damage is deliberate — the Fibonacci scatter pass raised
    // SerpveilCubeSplitCount to 20, so total volley damage stays comparable.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Combat", meta = (ClampMin = "0.0"))
    float SerpveilPerCubeDamage = 3.5f;

    // ⚠ PLAYTEST PENDING: raised 2800 -> 3500 alongside the range rebase below, so a
    // max-range shot lands in ~2.0s. Left at 2800 the longer shot would take ~2.9s
    // and be trivially walked out of. Owner PIE-tested 4200 first and pulled it
    // back to 3500 — dodgeable at distance is intended, this is a committed shot.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Projectile", meta = (ClampMin = "100.0"))
    float SerpveilSpeed = 3500.0f;

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
    // ⚠ PLAYTEST PENDING: raised 1500 -> 4000 (15m -> 40m). Serpveil was the only
    // projectile Trigger in the game under 40m while its peers (Fulgrix 4000,
    // Solux 5000, Acervyn 5000, Venyx 6000) sat at 40-60m — an early placeholder
    // never revisited. 4000 puts the un-charged tap at the bottom of its own band.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "100.0"))
    float SerpveilMaxRange = 4000.0f;

    // DEPRECATED by S1 rework
    // Vael drained per second while charging (determines affordable range)
    // ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Resources", meta = (ClampMin = "0.1"))
    float SerpveilVaelPerSecond = 10.0f;

    // Seconds of hold needed to charge from SerpveilMaxRange to SerpveilPresetMaxRange.
    // ⚠ PLAYTEST PENDING
    //
    // Was "DEPRECATED by S1 rework / informational only" and read solely by dead
    // code; repurposed as the single authoritative charge duration so the value
    // is DA-driven per project rule. AWTBRCharacter's stage-2 flow reads it via
    // GetSerpveilFullChargeSeconds() and falls back to
    // SERPVEIL_FULL_CHARGE_SECONDS_FALLBACK when no Serpveil DataAsset is active.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "0.1"))
    float SerpveilChargeTime = 0.8f;

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

    // ⚠ PLAYTEST PENDING: elapsed hold time past which a hold commits to Preset mode.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Windup", meta = (ClampMin = "0.0"))
    float SerpveilHoldThresholdSeconds = 0.15f;

    // ⚠ PLAYTEST PENDING: maximum reach in Preset mode at full charge.
    // Raised 2200 -> 7000 (22m -> 70m). Sits just above Venyx (6000) to give Viper
    // "longest Shooter" identity in exchange for having no homing, while staying
    // clear of sniper range (Telorn/Fulgris 8000, Piercex 12000). Also widens the
    // charge window from 7m to 30m, which is what makes a reach indicator worth
    // building at all — see the charge-range-indicator design notes.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path", meta = (ClampMin = "100.0"))
    float SerpveilPresetMaxRange = 7000.0f;

    // ⚠ PLAYTEST PENDING: authored curved path for Preset mode. Empty lanes fall back to straight fire.
    // Used only when the hold flow did not select from SerpveilPresets below.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path")
    FWTBRPathPreset SerpveilPresetPath;

    // Choices offered by the hold wheel. Seeded below with obvious placeholders
    // purely so the flow is testable before any real authoring — designing the
    // actual paths is the owner's job, not code's (same rule as EscudoPresets).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Path")
    TArray<FWTBRPathPreset> SerpveilPresets;

    FWTBRSerpveilParams()
    {
        // ⚠ PLACEHOLDER TEST DATA, NOT GAME DESIGN. Three obviously-different
        // shapes so the hold→wheel→charge→fire flow can be judged by feel.
        // Waypoints are fractions of the committed range, X forward / Y lateral
        // / Z vertical, and each lane's cubes ride the shape in parallel.
        auto MakeLane = [](const TArray<FVector>& Waypoints, int32 CubeCount)
        {
            FWTBRPathLane Lane;
            Lane.NormalizedWaypoints = Waypoints;
            Lane.CubeCount = CubeCount;
            return Lane;
        };

        FWTBRPathPreset Straight;
        Straight.PresetId = FName(TEXT("Straight"));
        Straight.DisplayName = FText::FromString(TEXT("Straight"));
        Straight.Lanes.Add(MakeLane({FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)}, 15));
        SerpveilPresets.Add(Straight);

        FWTBRPathPreset HookLeft;
        HookLeft.PresetId = FName(TEXT("HookLeft"));
        HookLeft.DisplayName = FText::FromString(TEXT("Hook Left"));
        HookLeft.Lanes.Add(MakeLane({
            FVector::ZeroVector,
            FVector(0.40f, -0.10f, 0.05f),
            FVector(0.70f, -0.35f, 0.10f),
            FVector(1.00f, -0.70f, 0.00f)}, 15));
        SerpveilPresets.Add(HookLeft);

        FWTBRPathPreset SplitFan;
        SplitFan.PresetId = FName(TEXT("SplitFan"));
        SplitFan.DisplayName = FText::FromString(TEXT("Split Fan"));
        SplitFan.Lanes.Add(MakeLane({
            FVector::ZeroVector, FVector(0.50f, -0.20f, 0.05f), FVector(1.00f, -0.45f, 0.00f)}, 5));
        SplitFan.Lanes.Add(MakeLane({
            FVector::ZeroVector, FVector(0.50f, 0.00f, 0.10f), FVector(1.00f, 0.00f, 0.00f)}, 5));
        SplitFan.Lanes.Add(MakeLane({
            FVector::ZeroVector, FVector(0.50f, 0.20f, 0.05f), FVector(1.00f, 0.45f, 0.00f)}, 5));
        SerpveilPresets.Add(SplitFan);
    }

    // true fires immediately at full charge; false waits for release.
    //
    // Flipped true -> false to match the locked design ("the player always
    // releases the shot themselves; no auto-fire"). This is a zero-risk change
    // today because the branch that reads this flag
    // (UWTBRSerpveilTrigger::OnWindupComplete) is UNREACHABLE via the shipped
    // input flow: reaching it needs the button still held when the windup timer
    // completes, but any hold past SERPVEIL_HOLD_THRESHOLD is captured by the
    // preset wheel in AWTBRCharacter::HandleSerpveilFirePressed and never calls
    // Server_Fire at all, while a tap replays press+release in the same frame so
    // bButtonReleased is already true and OnWindupComplete returns early.
    // Kept (rather than deleted) so that if the S1 windup path is ever made
    // reachable again it obeys the design lock instead of silently auto-firing.
    // NOTE: WTBRSerpveilS2StateMachineAutomationTests sets this explicitly and
    // drives the trigger directly, so it covers a path real gameplay cannot hit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Serpveil | Windup")
    bool bSerpveilAutoFireAtFullCharge = false;

    // DEPRECATED by zigzag tap pattern — fan spread let only one cube land at range
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilTapSpreadDeg = 24.0f;

    // DEAD for Viper since the Fibonacci scatter pass — both offsets are now
    // produced by ComputeFibonacciSphereOffset instead. Kept (not deleted, not
    // marked DeprecatedProperty) purely so authored DA values are not silently
    // lost if the scatter is ever reverted.
    // Sideways gap between neighbouring cubes in the launch formation
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilFormationSpacing = 25.0f;

    // DEAD for Viper — see SerpveilFormationSpacing above.
    // Vertical saw-tooth offset: even cubes sit up, odd cubes sit down (▲▼▲)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilFormationStagger = 20.0f;

    // Radius of the Fibonacci sphere the split cubes appear on, centred on the
    // caster's body (NOT the conjure hand). ⚠ PLAYTEST PENDING — PIE-tunable.
    // Character capsule is r=42, so 135 clears the body by ~93uu.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilScatterRadius = 135.0f;

    // Lifts the scatter sphere above the capsule centre so its underside does not
    // sink through the floor. Capsule half-height is 96, so with the default
    // radius the sphere's lowest cube sits at -50 from centre — about knee level.
    // Keep >= (SerpveilScatterRadius - 96) or cubes spawn underground.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "0.0"))
    float SerpveilScatterHeightOffset = 85.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Resources",
        meta = (ClampMin = "0.0"))
    float SerpveilVaelCostPerShot = 6.0f;

    // 20 is the locked Viper count (per slot — Main + Sub firing together is 40).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Serpveil | Path",
        meta = (ClampMin = "1"))
    int32 SerpveilCubeSplitCount = 20;
};

// ─────────────────────────────────────────────────────────────────────────────
// FWTBRAcervynParams — Acervyn (standalone Burst Homing Gunner, NonCombinable —
// NOT the "Hornet" composite; that canon name belongs to Venspire = Venyx+Venyx)
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

    // Turn radius ≈ AcervynSpeed² / AcervynHomingAcceleration. Same "soft-lock,
    // limited turn" rationale as VenyxHomingAcceleration above; iterated in PIE
    // 6000→18000 (~8.9m) still felt weak → bumped to 29000 (~5.5m radius) at
    // owner request 2026-07-15 (not yet re-PIE'd). ⚠ PLAYTEST PENDING
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acervyn | Homing", meta = (ClampMin = "100.0"))
    float AcervynHomingAcceleration = 29000.0f;

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
// FWTBRVexornParams — Vexorn (Bagworm-style radar cloak).
// Passive while equipped as the active Sub-Trigger; drains Vael continuously.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FWTBRVexornParams
{
    GENERATED_BODY()

    // Retained for backward-compatible DataAssets; Bagworm cloaks its owner
    // rather than jamming other combatants.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Combat")
    float VexornSuppressionRadius = 1500.0f;

    // Vael consumed per second while radar cloak is active.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Resources")
    float VexornVaelDrainPerSecond = 1.0f;

    // Retained for existing data assets and tests. Vexorn is now sustained by
    // VexornVaelDrainPerSecond rather than a one-time activation cost.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vexorn | Resources", meta=(DeprecatedProperty, DeprecationMessage="Use VexornVaelDrainPerSecond"))
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

    // Which of the 4 Composite-source archetypes this Trigger is, if any (Step
    // 3 of the Composite Bullet build order). Only meaningful for Gunner-
    // category Triggers; irrelevant for Melee/Defense/Sniper/etc., leave at
    // None for those. Owner sets this per-DataAsset in the editor — Claude/
    // Codex cannot author .uasset values.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Trigger | Identity")
    EWTBRBulletArchetype BulletArchetype = EWTBRBulletArchetype::None;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Trigger | Feryx",
        meta = (EditCondition = "Category == ETriggerCategory::Melee"))
    FWTBRFeryxParams FeryxParams;

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
