// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Subsystem/WTBRCustomPresetSubsystem.h"

// USubsystem enforces that a GameInstanceSubsystem's Outer is a real UGameInstance
// (fatal-errors otherwise), so every test needs one — a bare NewObject<> with no
// Outer is not enough. Init() is deliberately NOT called: these tests only exercise
// the subsystem's own data methods, not GameInstance lifecycle.
static UWTBRCustomPresetSubsystem* MakeTestSubsystem()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    return NewObject<UWTBRCustomPresetSubsystem>(GameInstance);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetSubsystemAddFindTest,
    "WTBR.CustomPreset.Subsystem.AddCreatesAFindablePreset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetSubsystemAddFindTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCustomPresetSubsystem* Subsystem = MakeTestSubsystem();

    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("MyFirstSweep"));
    Preset.DisplayName = FText::FromString(TEXT("My First Sweep"));

    TestTrue(TEXT("Adding a named preset succeeds"), Subsystem->AddOrUpdatePreset(Preset));
    TestEqual(TEXT("One preset stored"), Subsystem->GetCustomVenyxPresets().Num(), 1);

    const FWTBRPathPreset* Found = Subsystem->FindPreset(FName(TEXT("MyFirstSweep")));
    TestNotNull(TEXT("Preset is findable by Id"), Found);
    if (Found)
    {
        TestEqual(TEXT("Found preset has the authored display name"),
            Found->DisplayName.ToString(), FString(TEXT("My First Sweep")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetSubsystemRejectsUnnamedTest,
    "WTBR.CustomPreset.Subsystem.RejectsPresetWithNoId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetSubsystemRejectsUnnamedTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCustomPresetSubsystem* Subsystem = MakeTestSubsystem();

    FWTBRPathPreset Preset;
    Preset.PresetId = NAME_None;

    TestFalse(TEXT("A preset with no PresetId is rejected"), Subsystem->AddOrUpdatePreset(Preset));
    TestEqual(TEXT("Nothing was stored"), Subsystem->GetCustomVenyxPresets().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetSubsystemUpdateOverwritesTest,
    "WTBR.CustomPreset.Subsystem.SameIdOverwritesInsteadOfDuplicating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetSubsystemUpdateOverwritesTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCustomPresetSubsystem* Subsystem = MakeTestSubsystem();

    FWTBRPathPreset First;
    First.PresetId = FName(TEXT("Sweep"));
    First.DisplayName = FText::FromString(TEXT("Version One"));
    Subsystem->AddOrUpdatePreset(First);

    FWTBRPathPreset Second;
    Second.PresetId = FName(TEXT("Sweep"));
    Second.DisplayName = FText::FromString(TEXT("Version Two"));
    Subsystem->AddOrUpdatePreset(Second);

    TestEqual(TEXT("Re-saving the same PresetId does not create a duplicate"),
        Subsystem->GetCustomVenyxPresets().Num(), 1);
    const FWTBRPathPreset* Found = Subsystem->FindPreset(FName(TEXT("Sweep")));
    TestNotNull(TEXT("Preset still findable"), Found);
    if (Found)
    {
        TestEqual(TEXT("The stored preset is the newer version"),
            Found->DisplayName.ToString(), FString(TEXT("Version Two")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetSubsystemDeleteTest,
    "WTBR.CustomPreset.Subsystem.DeleteRemovesAPreset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetSubsystemDeleteTest::RunTest(const FString& /*Parameters*/)
{
    UWTBRCustomPresetSubsystem* Subsystem = MakeTestSubsystem();

    FWTBRPathPreset Preset;
    Preset.PresetId = FName(TEXT("Encircle"));
    Subsystem->AddOrUpdatePreset(Preset);

    TestFalse(TEXT("Deleting an unknown Id reports no change"),
        Subsystem->DeletePreset(FName(TEXT("NotThere"))));
    TestTrue(TEXT("Deleting the known Id succeeds"),
        Subsystem->DeletePreset(FName(TEXT("Encircle"))));
    TestEqual(TEXT("Preset list is empty afterwards"), Subsystem->GetCustomVenyxPresets().Num(), 0);
    TestNull(TEXT("Deleted preset is no longer findable"), Subsystem->FindPreset(FName(TEXT("Encircle"))));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
