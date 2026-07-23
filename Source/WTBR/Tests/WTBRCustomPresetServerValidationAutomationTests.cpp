// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "WTBRCharacter.h"

// UWTBRTriggerSetComponent::Server_SetCustomVenyxPresets is the first RPC in the
// project carrying player-authored preset DATA rather than a trusted index — these
// tests exercise UWTBRTriggerSetComponent::SanitizeCustomVenyxPreset (via the
// SanitizeCustomVenyxPresetForTest wrapper) directly, the same way other Server_*
// validation logic in this codebase is unit-tested by calling the sanitizer/
// _Implementation rather than going over a real network channel.

namespace
{
    FWTBRPathLane MakeValidLane()
    {
        FWTBRPathLane Lane;
        Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
        return Lane;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationRejectsUnnamedTest,
    "WTBR.CustomPreset.ServerValidate.RejectsPresetWithNoId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationRejectsUnnamedTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    Preset.PresetId = NAME_None;
    Preset.Lanes.Add(MakeValidLane());

    TestFalse(TEXT("A preset with no PresetId is rejected outright"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationDropsOffCasterLaneTest,
    "WTBR.CustomPreset.ServerValidate.DropsLaneNotStartingAtCaster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationDropsOffCasterLaneTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("Sneaky"));

    // A malicious/buggy lane that does not start at the caster — ResolvePathPreset
    // never itself checks this, so the validator is the only thing standing between
    // an untrusted upload and that invariant silently breaking.
    FWTBRPathLane OffCasterLane;
    OffCasterLane.NormalizedWaypoints = {FVector(0.3f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)};
    Preset.Lanes.Add(OffCasterLane);
    Preset.Lanes.Add(MakeValidLane());

    TestTrue(TEXT("Preset survives because one valid lane remains"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    TestEqual(TEXT("Only the valid lane survives sanitizing"), Preset.Lanes.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationRejectsAllLanesInvalidTest,
    "WTBR.CustomPreset.ServerValidate.RejectsPresetWhenEveryLaneIsInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationRejectsAllLanesInvalidTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("Empty"));

    FWTBRPathLane OffCasterLane;
    OffCasterLane.NormalizedWaypoints = {FVector(0.3f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)};
    Preset.Lanes.Add(OffCasterLane);

    TestFalse(TEXT("A preset left with zero lanes after sanitizing is rejected entirely"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationTruncatesWaypointsTest,
    "WTBR.CustomPreset.ServerValidate.TruncatesOversizedWaypointArray",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationTruncatesWaypointsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("TooManyTurns"));

    FWTBRPathLane Lane;
    Lane.NormalizedWaypoints.Add(FVector::ZeroVector);
    for (int32 i = 1; i <= 20; ++i)
    {
        Lane.NormalizedWaypoints.Add(FVector(0.05f * i, 0.0f, 0.0f));
    }
    const FVector FinalWaypoint = Lane.NormalizedWaypoints.Last();
    Preset.Lanes.Add(Lane);

    TestTrue(TEXT("Preset survives with a truncated lane"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    if (!TestTrue(TEXT("A lane remains"), Preset.Lanes.Num() > 0)) return false;

    TestTrue(TEXT("Waypoint count is clamped at or under the custom-lane ceiling"),
        Preset.Lanes[0].NormalizedWaypoints.Num() <= WTBR_MAX_CUSTOM_LANE_WAYPOINTS);
    TestTrue(TEXT("The final waypoint (where the lane lands) is preserved, not dropped"),
        Preset.Lanes[0].NormalizedWaypoints.Last().Equals(FinalWaypoint, KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationClampsScalarsTest,
    "WTBR.CustomPreset.ServerValidate.ClampsOutOfRangeScalars",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationClampsScalarsTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("OutOfRange"));

    FWTBRPathLane Lane = MakeValidLane();
    Lane.LaunchDelay = 999.0f;
    Lane.HomingRadius = -5.0f;
    Lane.CubeCount = 0;

    FWTBRLaneEvent Event;
    Event.AtPathFraction = 5.0f;
    Event.DurationSeconds = -1.0f;
    Lane.Events.Add(Event);

    Preset.Lanes.Add(Lane);

    TestTrue(TEXT("Preset survives sanitizing"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    if (!TestTrue(TEXT("Lane remains"), Preset.Lanes.Num() > 0)) return false;

    const FWTBRPathLane& Sanitized = Preset.Lanes[0];
    TestEqual(TEXT("LaunchDelay clamped to its documented ceiling"), Sanitized.LaunchDelay, 5.0f);
    TestEqual(TEXT("HomingRadius clamped to its documented floor"), Sanitized.HomingRadius, 0.0f);
    TestEqual(TEXT("CubeCount floored to at least one cube"), Sanitized.CubeCount, 1);

    if (TestEqual(TEXT("Event survives"), Sanitized.Events.Num(), 1))
    {
        TestEqual(TEXT("AtPathFraction clamped into 0..1"), Sanitized.Events[0].AtPathFraction, 1.0f);
        TestEqual(TEXT("DurationSeconds clamped to its floor"), Sanitized.Events[0].DurationSeconds, 0.0f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationRejectsNonFiniteWaypointTest,
    "WTBR.CustomPreset.ServerValidate.DropsLaneWithNonFiniteWaypoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationRejectsNonFiniteWaypointTest::RunTest(const FString& /*Parameters*/)
{
    // A NaN waypoint would reach an InterpToMovement control point and from there an
    // actor transform. FMath::Clamp cannot catch it (every comparison against NaN is
    // false), so the lane has to be dropped explicitly.
    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("NaNLane"));

    FWTBRPathLane NaNLane;
    NaNLane.NormalizedWaypoints = {
        FVector::ZeroVector,
        FVector(FMath::Sqrt(-1.0f), 0.0f, 0.0f)
    };
    Preset.Lanes.Add(NaNLane);

    FWTBRPathLane InfLane;
    InfLane.NormalizedWaypoints = {
        FVector::ZeroVector,
        FVector(TNumericLimits<float>::Max() * 2.0f, 0.0f, 0.0f)
    };
    Preset.Lanes.Add(InfLane);

    Preset.Lanes.Add(MakeValidLane());

    TestTrue(TEXT("Preset survives because one finite lane remains"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    TestEqual(TEXT("Both the NaN lane and the infinite lane are dropped"), Preset.Lanes.Num(), 1);

    for (const FVector& Waypoint : Preset.Lanes[0].NormalizedWaypoints)
    {
        TestFalse(TEXT("No surviving waypoint is non-finite"), Waypoint.ContainsNaN());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationReplacesNonFiniteScalarsTest,
    "WTBR.CustomPreset.ServerValidate.ReplacesNonFiniteScalarsWithDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationReplacesNonFiniteScalarsTest::RunTest(const FString& /*Parameters*/)
{
    const float NaN = FMath::Sqrt(-1.0f);

    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("NaNScalars"));

    FWTBRPathLane Lane = MakeValidLane();
    Lane.LaunchDelay = NaN;
    Lane.HomingRadius = NaN;
    Lane.HomingRadiusFloorUU = NaN;

    FWTBRLaneEvent Event;
    Event.AtPathFraction = NaN;
    Event.DurationSeconds = NaN;
    Lane.Events.Add(Event);

    Preset.Lanes.Add(Lane);

    TestTrue(TEXT("Preset survives sanitizing"),
        UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetForTest(Preset));
    if (!TestTrue(TEXT("Lane remains"), Preset.Lanes.Num() > 0)) return false;

    const FWTBRPathLane& Sanitized = Preset.Lanes[0];
    TestTrue(TEXT("LaunchDelay is finite after sanitizing"), FMath::IsFinite(Sanitized.LaunchDelay));
    TestTrue(TEXT("HomingRadius is finite after sanitizing"), FMath::IsFinite(Sanitized.HomingRadius));
    TestTrue(TEXT("HomingRadiusFloorUU is finite after sanitizing"),
        FMath::IsFinite(Sanitized.HomingRadiusFloorUU));

    if (TestEqual(TEXT("Event survives"), Sanitized.Events.Num(), 1))
    {
        TestTrue(TEXT("AtPathFraction is finite after sanitizing"),
            FMath::IsFinite(Sanitized.Events[0].AtPathFraction));
        TestTrue(TEXT("DurationSeconds is finite after sanitizing"),
            FMath::IsFinite(Sanitized.Events[0].DurationSeconds));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationListStaysAlignedTest,
    "WTBR.CustomPreset.ServerValidate.ClientAndServerListsStayIndexAligned",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationListStaysAlignedTest::RunTest(const FString& /*Parameters*/)
{
    // The fire RPC carries only an index, resolved against each side's own array.
    // If the client kept its raw list while the server kept a filtered one, index N
    // would mean different presets on each side — a wrong-shot bug. Both sides run
    // this same list sanitizer, so equal input must give equal output.
    TArray<FWTBRPathPreset> Raw;

    FWTBRPathPreset Rejected;
    Rejected.PresetId = FName(TEXT("WillBeRejected"));
    FWTBRPathLane BadLane;
    BadLane.NormalizedWaypoints = {FVector(0.3f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)};
    Rejected.Lanes.Add(BadLane);
    Raw.Add(Rejected);

    FWTBRPathPreset Kept;
    Kept.PresetId = FName(TEXT("WillSurvive"));
    Kept.Lanes.Add(MakeValidLane());
    Raw.Add(Kept);

    TArray<FWTBRPathPreset> ClientSide;
    TArray<FWTBRPathPreset> ServerSide;
    UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetListForTest(Raw, ClientSide);
    UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetListForTest(Raw, ServerSide);

    TestEqual(TEXT("Both sides drop the invalid preset"), ClientSide.Num(), 1);
    TestEqual(TEXT("Both sides agree on list length"), ServerSide.Num(), ClientSide.Num());
    if (ClientSide.Num() == 1 && ServerSide.Num() == 1)
    {
        TestEqual(TEXT("Index 0 resolves to the same preset on both sides"),
            ClientSide[0].PresetId, ServerSide[0].PresetId);
    }

    // Re-sanitizing an already-sanitized list must be a no-op, or the server's
    // second pass on receipt could shorten what the client already displayed.
    TArray<FWTBRPathPreset> Twice;
    UWTBRTriggerSetComponent::SanitizeCustomVenyxPresetListForTest(ClientSide, Twice);
    TestEqual(TEXT("Sanitizing is idempotent"), Twice.Num(), ClientSide.Num());
    return true;
}

namespace
{
    class FWTBRCustomPresetValidationWorldFixture
    {
    public:
        explicit FWTBRCustomPresetValidationWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCustomPresetValidationWorldFixture()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetValidationServerRpcReachesInstalledVenyxTest,
    "WTBR.CustomPreset.ServerValidate.UploadReachesAnAlreadyInstalledVenyxTrigger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetValidationServerRpcReachesInstalledVenyxTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCustomPresetValidationWorldFixture Fixture(TEXT("WTBRCustomPresetValidationWorld"));
    UWorld* World = Fixture.GetWorld();
    if (!TestNotNull(TEXT("Test world created"), World)) return false;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRCharacter* Owner = World->SpawnActor<AWTBRCharacter>(
        AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("Owner character spawned"), Owner)) return false;
    if (!TestNotNull(TEXT("Owner has a TriggerSetComponent"), Owner->TriggerSetComponent.Get())) return false;

    UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
    DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();
    // FWTBRVenyxParams's own constructor seeds Skyfall/Sweep/Encircle by default —
    // clear those first so this test's counts are exact, not "+3".
    DataAsset->VenyxParams.VenyxPresets.Reset();
    FWTBRPathPreset Baked;
    Baked.PresetId = FName(TEXT("Baked"));
    Baked.Lanes.Add(MakeValidLane());
    DataAsset->VenyxParams.VenyxPresets.Add(Baked);

    UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
    Venyx->InitializeTrigger(Owner, DataAsset);
    Owner->TriggerSetComponent->InstallTriggerForTest(ETriggerSlot::Main1, Venyx);

    FWTBRPathPreset Custom;
    Custom.PresetId = FName(TEXT("PlayerAuthored"));
    Custom.Lanes.Add(MakeValidLane());

    TArray<FWTBRPathPreset> Upload;
    Upload.Add(Custom);
    // Calling _Implementation directly, same as other Server_* logic in this
    // codebase — RPC marshalling itself is engine-owned, not something worth
    // exercising in a headless test.
    Owner->TriggerSetComponent->Server_SetCustomVenyxPresets_Implementation(Upload);

    const TArray<FWTBRPathPreset>* Presets = Venyx->GetHoldPresets();
    TestNotNull(TEXT("Presets still resolvable after upload"), Presets);
    if (!Presets) return false;

    TestEqual(TEXT("Baked + uploaded custom preset"), Presets->Num(), 2);
    if (Presets->Num() == 2)
    {
        TestEqual(TEXT("Uploaded preset is visible at the expected index"),
            (*Presets)[1].PresetId, FName(TEXT("PlayerAuthored")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
