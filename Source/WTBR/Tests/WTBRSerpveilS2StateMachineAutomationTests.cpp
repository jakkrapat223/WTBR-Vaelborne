// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRSerpveilTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "WTBRCharacter.h"

namespace
{
    class FWTBRSerpveilS2WorldFixture
    {
    public:
        explicit FWTBRSerpveilS2WorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRSerpveilS2WorldFixture()
        {
            if (World)
            {
                World->DestroyWorld(false);
                if (GEngine)
                {
                    GEngine->DestroyWorldContext(World);
                }
            }
        }

        UWorld* GetWorld() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    AWTBRCharacter* SpawnSerpveilS2Character(UWorld* World)
    {
        if (!World) return nullptr;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<AWTBRCharacter>(
            AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    }

    int32 CountSerpveilS2Projectiles(UWorld* World)
    {
        int32 Count = 0;
        if (!World) return Count;

        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It)) ++Count;
        }
        return Count;
    }

    UWTBRTriggerDataAsset* MakeSerpveilS2DataAsset(
        bool bAutoFireAtFullCharge = true, float SplitDelay = 0.4f)
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
        Params.SerpveilPerCubeDamage = 15.0f;
        Params.SerpveilSpeed = 2800.0f;
        Params.SerpveilMaxRange = 1500.0f;
        Params.SerpveilPresetMaxRange = 2500.0f;
        Params.SerpveilProjectileClass = AWTBRProjectileBase::StaticClass();
        Params.SerpveilFireCooldown = 0.0f;
        Params.SerpveilSplitDelay = SplitDelay;
        Params.SerpveilHoldThresholdSeconds = 0.15f;
        Params.bSerpveilAutoFireAtFullCharge = bAutoFireAtFullCharge;
        Params.SerpveilFormationSpacing = 25.0f;
        Params.SerpveilFormationStagger = 20.0f;
        Params.SerpveilVaelCostPerShot = 1.0f;
        Params.SerpveilCubeSplitCount = 1;
        return DataAsset;
    }

    UWTBRSerpveilTrigger* MakeSerpveilS2Trigger(
        AWTBRCharacter* Owner, UWTBRTriggerDataAsset* DataAsset)
    {
        if (!Owner || !DataAsset || !Owner->VaelComponent) return nullptr;

        Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
        UWTBRSerpveilTrigger* Trigger = NewObject<UWTBRSerpveilTrigger>(Owner);
        Trigger->InitializeTrigger(Owner, DataAsset);
        return Trigger;
    }

    bool BeginSerpveilS2Charge(UWTBRSerpveilTrigger* Trigger, AWTBRCharacter* Owner)
    {
        if (!Trigger || !Owner) return false;
        Trigger->OnTriggerActivated_Implementation(Owner, true);
        return Trigger->IsCharging();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2QuickReleaseTest,
    "WTBR.Serpveil.S2.QuickReleaseKeepsBasicTapReach",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2QuickReleaseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_QuickRelease"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset();
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    Trigger->HandleReleaseAtElapsedForTest(0.05f);
    TestFalse(TEXT("Quick release remains Basic mode"), Trigger->GetModeIsPresetForTest());
    TestEqual(TEXT("Quick release keeps the S1 range"), Trigger->GetCommittedReachForTest(), 1500.0f);
    Trigger->OnWindupCompleteForTest();
    TestEqual(TEXT("Quick release fires one cube"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2InterpolatedReachTest,
    "WTBR.Serpveil.S2.ReleasePastThresholdInterpolatesReach",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2InterpolatedReachTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_InterpolatedReach"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset();
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    Trigger->HandleReleaseAtElapsedForTest(0.275f);
    const float ExpectedReach = FMath::Lerp(1500.0f, 2500.0f, (0.275f - 0.15f) / (0.4f - 0.15f));
    TestTrue(TEXT("Release past threshold enters Preset mode"), Trigger->GetModeIsPresetForTest());
    TestTrue(TEXT("Reach is interpolated from threshold to windup"),
        FMath::IsNearlyEqual(Trigger->GetCommittedReachForTest(), ExpectedReach));
    Trigger->OnWindupCompleteForTest();
    TestEqual(TEXT("Interpolated release fires one cube"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2FullReachReleaseTest,
    "WTBR.Serpveil.S2.ReleaseAtWindupGetsFullReach",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2FullReachReleaseTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_FullReachRelease"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset();
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    Trigger->HandleReleaseAtElapsedForTest(0.4f);
    TestEqual(TEXT("Release at windup gets full Preset reach"), Trigger->GetCommittedReachForTest(), 2500.0f);
    Trigger->OnWindupCompleteForTest();
    TestEqual(TEXT("Full-reach release fires one cube"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2AutoFireTest,
    "WTBR.Serpveil.S2.AutoFireAtFullCharge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2AutoFireTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_AutoFire"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset(true);
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    Trigger->OnWindupCompleteForTest();
    TestTrue(TEXT("Auto-fire commits Preset mode"), Trigger->GetModeIsPresetForTest());
    TestEqual(TEXT("Auto-fire uses full Preset reach"), Trigger->GetCommittedReachForTest(), 2500.0f);
    TestFalse(TEXT("Auto-fire bypasses WindupReady"), Trigger->GetWindupReadyForTest());
    TestEqual(TEXT("Auto-fire spawns one cube"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2WindupReadyTest,
    "WTBR.Serpveil.S2.WindupReadyFiresOnceOnRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2WindupReadyTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_WindupReady"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset(false);
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    Trigger->OnWindupCompleteForTest();
    TestTrue(TEXT("Windup completion enters WindupReady"), Trigger->GetWindupReadyForTest());
    TestTrue(TEXT("WindupReady keeps charge active"), Trigger->IsCharging());
    TestEqual(TEXT("WindupReady has not fired"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 0);

    Trigger->HandleReleaseAtElapsedForTest(0.4f);
    TestEqual(TEXT("WindupReady release uses full reach"), Trigger->GetCommittedReachForTest(), 2500.0f);
    TestEqual(TEXT("WindupReady release fires exactly once"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2ZeroDelayTest,
    "WTBR.Serpveil.S2.ZeroDelayFiresBasicImmediately",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2ZeroDelayTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_ZeroDelay"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset(true, 0.0f);
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger) return false;

    Trigger->OnTriggerActivated_Implementation(Owner, true);
    TestEqual(TEXT("Zero-delay path keeps Basic reach"), Trigger->GetCommittedReachForTest(), 1500.0f);
    TestEqual(TEXT("Zero-delay path fires immediately"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRSerpveilS2CancelTest,
    "WTBR.Serpveil.S2.CancelResetsStateWithoutFiring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRSerpveilS2CancelTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRSerpveilS2WorldFixture Fixture(TEXT("WTBR_SerpveilS2_Cancel"));
    AWTBRCharacter* Owner = SpawnSerpveilS2Character(Fixture.GetWorld());
    UWTBRTriggerDataAsset* DataAsset = MakeSerpveilS2DataAsset();
    UWTBRSerpveilTrigger* Trigger = MakeSerpveilS2Trigger(Owner, DataAsset);
    if (!Owner || !Trigger || !Owner->VaelComponent) return false;

    const float VaelBefore = Owner->VaelComponent->GetCurrentVael();
    TestTrue(TEXT("Charge begins"), BeginSerpveilS2Charge(Trigger, Owner));
    TestTrue(TEXT("Cancel succeeds"), Trigger->CancelCharge());
    TestFalse(TEXT("Cancel resets Preset mode"), Trigger->GetModeIsPresetForTest());
    TestFalse(TEXT("Cancel resets WindupReady"), Trigger->GetWindupReadyForTest());
    TestEqual(TEXT("Cancel resets committed reach"), Trigger->GetCommittedReachForTest(), 0.0f);
    TestEqual(TEXT("Cancel spawns no projectile"), CountSerpveilS2Projectiles(Fixture.GetWorld()), 0);
    TestEqual(TEXT("Cancel spends no Vael"), Owner->VaelComponent->GetCurrentVael(), VaelBefore);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
