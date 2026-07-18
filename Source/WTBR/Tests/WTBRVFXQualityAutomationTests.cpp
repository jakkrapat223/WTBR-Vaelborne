// Copyright Vaelborne: Dominion. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VFX/WTBRVFXManagerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRVFXQualityProfileTest,
    "WTBR.VFX.QualityProfile.TrimsOptionalFeedbackBeforePrimaryImpact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRVFXQualityProfileTest::RunTest(const FString& /*Parameters*/)
{
    const FWTBRVFXQualityProfile Low =
        UWTBRVFXManagerSubsystem::GetQualityProfileForEffectsQuality(0);
    const FWTBRVFXQualityProfile Medium =
        UWTBRVFXManagerSubsystem::GetQualityProfileForEffectsQuality(1);
    const FWTBRVFXQualityProfile Epic =
        UWTBRVFXManagerSubsystem::GetQualityProfileForEffectsQuality(3);

    TestEqual(TEXT("Low uses its intended cull-distance multiplier"), Low.DistanceMultiplier, 0.45f);
    TestFalse(TEXT("Low omits decals"), Low.bSpawnDecals);
    TestFalse(TEXT("Low omits camera shake"), Low.bPlayCameraShake);
    TestTrue(TEXT("Medium retains decals"), Medium.bSpawnDecals);
    TestFalse(TEXT("Medium still omits camera shake"), Medium.bPlayCameraShake);
    TestEqual(TEXT("Epic preserves the configured cull distance"), Epic.DistanceMultiplier, 1.0f);
    TestTrue(TEXT("Epic retains decals"), Epic.bSpawnDecals);
    TestTrue(TEXT("Epic retains camera shake"), Epic.bPlayCameraShake);
    return true;
}

#endif
