// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRHealthComponent.h"
#include "Data/WTBRCoreStatsDataAsset.h"

// Pure geometry classification — no world/actor/physics needed at all, unlike
// most of this codebase's hit-detection tests (see the SweepMultiByChannel
// gotcha), since ClassifyHitZone deliberately takes plain values instead of a
// live UCapsuleComponent*.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHitZoneClassificationHeightBandsTest,
    "WTBR.Combat.HitZone.HeightBandsClassifyHeadTorsoLeg",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHitZoneClassificationHeightBandsTest::RunTest(const FString& /*Parameters*/)
{
    const FVector Center(0.0f, 0.0f, 96.0f); // matches AWTBRCharacter's capsule half-height
    const float HalfHeight = 96.0f;
    const float Radius = 42.0f;
    const float HeadThreshold = 0.85f;
    const float LegThreshold = 0.35f;
    const float ArmRatio = 0.75f;

    // Bottom of capsule = Z0, top = Z192. Head band >= 0.85 * 192 = 163.2.
    const FVector HeadHit(0.0f, 0.0f, 180.0f);
    TestEqual(TEXT("High Z classifies as Head"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, HeadHit,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Head);

    // Character projectiles use overlap collision. A swept overlap is not a
    // blocking hit, but its SweepResult still carries the true head contact
    // point and must take precedence over the later projectile location.
    FHitResult SweptOverlap;
    SweptOverlap.ImpactPoint = HeadHit;
    const FVector ResolvedHeadImpact = AWTBRProjectileBase::ResolveHitZoneImpactPoint(
        /*bFromSweep=*/true, SweptOverlap, FVector(0.0f, 0.0f, 96.0f));
    TestEqual(TEXT("Swept overlap preserves the actual head contact point"),
        ResolvedHeadImpact, HeadHit);
    TestEqual(TEXT("Swept head contact classifies as Head"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, ResolvedHeadImpact,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Head);

    FHitResult EmptySweptOverlap;
    const FVector FallbackHeadImpact = AWTBRProjectileBase::ResolveHitZoneImpactPoint(
        /*bFromSweep=*/true, EmptySweptOverlap, HeadHit);
    TestEqual(TEXT("Empty overlap sweep falls back instead of classifying world origin as Leg"),
        FallbackHeadImpact, HeadHit);

    // Leg band < 0.35 * 192 = 67.2.
    const FVector LegHit(0.0f, 0.0f, 30.0f);
    TestEqual(TEXT("Low Z classifies as Leg"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, LegHit,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Leg);

    // Mid-height, dead center (lateral distance zero) = Torso.
    const FVector TorsoHit(0.0f, 0.0f, 96.0f);
    TestEqual(TEXT("Mid-height center classifies as Torso"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, TorsoHit,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Torso);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHitZoneClassificationLateralArmTest,
    "WTBR.Combat.HitZone.LateralOffsetInTorsoBandClassifiesArm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHitZoneClassificationLateralArmTest::RunTest(const FString& /*Parameters*/)
{
    const FVector Center(0.0f, 0.0f, 96.0f);
    const float HalfHeight = 96.0f;
    const float Radius = 42.0f;
    const float HeadThreshold = 0.85f;
    const float LegThreshold = 0.35f;
    const float ArmRatio = 0.75f;

    // Mid-height and laterally offset beyond Radius * ArmRatio = 31.5 while
    // remaining inside the character's 42 cm collision capsule.
    const FVector ArmHit(0.0f, 36.0f, 96.0f);
    TestEqual(TEXT("Mid-height off-center classifies as Arm"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, ArmHit,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Arm);

    // Just inside the arm threshold radius stays Torso.
    const FVector NearCenterHit(0.0f, 20.0f, 96.0f);
    TestEqual(TEXT("Mid-height just off-center stays Torso"),
        AWTBRProjectileBase::ClassifyHitZone(Center, HalfHeight, Radius, NearCenterHit,
            HeadThreshold, LegThreshold, ArmRatio),
        EWTBRHitZone::Torso);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHitZoneClassificationDegenerateCapsuleTest,
    "WTBR.Combat.HitZone.DegenerateCapsuleFallsBackToTorso",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHitZoneClassificationDegenerateCapsuleTest::RunTest(const FString& /*Parameters*/)
{
    const FVector Center(0.0f, 0.0f, 96.0f);
    TestEqual(TEXT("Zero half-height is a safe Torso fallback, not a divide-by-zero"),
        AWTBRProjectileBase::ClassifyHitZone(Center, 0.0f, 42.0f, Center, 0.85f, 0.35f, 1.15f),
        EWTBRHitZone::Torso);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRHitZoneDamageMultiplierTest,
    "WTBR.Combat.HitZone.DamageMultiplierUsesStatsOrDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRHitZoneDamageMultiplierTest::RunTest(const FString& /*Parameters*/)
{
    // Null Stats falls back to industry-standard placeholder values.
    TestEqual(TEXT("Null stats Head multiplier defaults to 2.0"),
        AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Head, nullptr), 2.0f);
    TestEqual(TEXT("Null stats Torso multiplier defaults to 1.0"),
        AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Torso, nullptr), 1.0f);
    TestEqual(TEXT("Null stats Arm multiplier defaults to 0.85"),
        AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Arm, nullptr), 0.85f);
    TestEqual(TEXT("Null stats Leg multiplier defaults to 0.85"),
        AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Leg, nullptr), 0.85f);

    // A real DataAsset instance's own values take priority over the defaults.
    UWTBRCoreStatsDataAsset* Stats = NewObject<UWTBRCoreStatsDataAsset>(GetTransientPackage());
    Stats->HeadDamageMultiplier = 3.0f;
    TestEqual(TEXT("Custom Stats Head multiplier is honoured"),
        AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Head, Stats), 3.0f);

    // Telorn's direct-hit base damage is 60. Lock the approved end results
    // so a future multiplier or classification regression is visible.
    Stats->HeadDamageMultiplier = 2.0f;
    Stats->TorsoDamageMultiplier = 1.0f;
    Stats->ArmDamageMultiplier = 0.85f;
    Stats->LegDamageMultiplier = 0.85f;
    constexpr float TelornBaseDamage = 60.0f;
    TestEqual(TEXT("Head hit deals 120 from a 60-damage shot"),
        TelornBaseDamage * AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Head, Stats), 120.0f);
    TestEqual(TEXT("Torso hit deals 60 from a 60-damage shot"),
        TelornBaseDamage * AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Torso, Stats), 60.0f);
    TestEqual(TEXT("Arm hit deals 51 from a 60-damage shot"),
        TelornBaseDamage * AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Arm, Stats), 51.0f);
    TestEqual(TEXT("Leg hit deals 51 from a 60-damage shot"),
        TelornBaseDamage * AWTBRProjectileBase::GetHitZoneDamageMultiplier(EWTBRHitZone::Leg, Stats), 51.0f);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
