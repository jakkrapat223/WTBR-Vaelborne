// Copyright Vaelborne: Dominion. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Actors/WTBRProjectileBase.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRVenyxTrigger.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRCharacter.h"

// Covers the single most important assumption the Preset Editor's server-upload
// path relies on (see UWTBRTriggerSetComponent::Server_SetCustomVenyxPresets /
// UWTBRVenyxTrigger::RefreshCustomHoldPresets): that a custom preset spliced into
// the combined cache actually FIRES at the index the wheel showed it at, not just
// that it appears in the list.

namespace
{
    class FWTBRCustomPresetCacheWorldFixture
    {
    public:
        explicit FWTBRCustomPresetCacheWorldFixture(const TCHAR* WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, FName(WorldName));
            if (World && GEngine)
            {
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            }
        }

        ~FWTBRCustomPresetCacheWorldFixture()
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

    UWTBRTriggerDataAsset* MakeTwoBakedPresetDataAsset()
    {
        UWTBRTriggerDataAsset* DataAsset = NewObject<UWTBRTriggerDataAsset>(GetTransientPackage());
        DataAsset->VaelCostPerUse = 10.0f;
        DataAsset->VenyxParams.VenyxProjectileClass = AWTBRProjectileBase::StaticClass();
        DataAsset->VenyxParams.VenyxRange = 2000.0f;
        DataAsset->VenyxParams.VenyxPresetMinRange = 1000.0f;
        DataAsset->VenyxParams.VenyxTotalDamage = 80.0f;
        DataAsset->VenyxParams.VenyxSpeed = 2500.0f;
        DataAsset->VenyxParams.VenyxTapCubeCount = 1;
        DataAsset->VenyxParams.VenyxPresetMaxCubeCount = 5;
        DataAsset->VenyxParams.VenyxPresetVaelCost = 15.0f;

        DataAsset->VenyxParams.VenyxPresets.Reset();
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FWTBRPathPreset Preset;
            Preset.PresetId = FName(*FString::Printf(TEXT("Baked%d"), Index));
            FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
            Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.0f, 0.0f)};
            DataAsset->VenyxParams.VenyxPresets.Add(Preset);
        }
        return DataAsset;
    }

    FWTBRPathPreset MakeCustomPreset()
    {
        FWTBRPathPreset Preset;
        Preset.PresetId = FName(TEXT("CustomFromEditor"));
        FWTBRPathLane& Lane = Preset.Lanes.AddDefaulted_GetRef();
        Lane.NormalizedWaypoints = {FVector::ZeroVector, FVector(1.0f, 0.2f, 0.0f)};
        return Preset;
    }

    AWTBRProjectileBase* FindSpawnedProjectile(UWorld* World)
    {
        for (TActorIterator<AWTBRProjectileBase> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                return *It;
            }
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWTBRCustomPresetGunnerCacheIncludesCustomTest,
    "WTBR.CustomPreset.GunnerCache.CombinedPresetsIncludeCustomAfterBaked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWTBRCustomPresetGunnerCacheIncludesCustomTest::RunTest(const FString& /*Parameters*/)
{
    FWTBRCustomPresetCacheWorldFixture Fixture(TEXT("WTBRCustomPresetGunnerCacheWorld"));
    UWorld* World = Fixture.GetWorld();
    if (!TestNotNull(TEXT("Test world created"), World)) return false;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AWTBRCharacter* Owner = World->SpawnActor<AWTBRCharacter>(
        AWTBRCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("Owner character spawned"), Owner)) return false;

    UWTBRTriggerDataAsset* DataAsset = MakeTwoBakedPresetDataAsset();

    UWTBRVenyxTrigger* Venyx = NewObject<UWTBRVenyxTrigger>(Owner);
    Venyx->InitializeTrigger(Owner, DataAsset);

    const TArray<FWTBRPathPreset>* BeforeRefresh = Venyx->GetHoldPresets();
    TestNotNull(TEXT("Baked presets visible before any custom refresh"), BeforeRefresh);
    if (BeforeRefresh)
    {
        TestEqual(TEXT("Only the two baked presets exist before refresh"), BeforeRefresh->Num(), 2);
    }

    TArray<FWTBRPathPreset> CustomPresets;
    CustomPresets.Add(MakeCustomPreset());
    Venyx->RefreshCustomHoldPresets(CustomPresets);

    const TArray<FWTBRPathPreset>* AfterRefresh = Venyx->GetHoldPresets();
    TestNotNull(TEXT("Combined presets still non-null after refresh"), AfterRefresh);
    if (!AfterRefresh) return false;

    TestEqual(TEXT("Baked + custom = three presets"), AfterRefresh->Num(), 3);
    if (AfterRefresh->Num() != 3) return false;

    TestEqual(TEXT("Baked presets keep their original order at the front"),
        (*AfterRefresh)[0].PresetId, FName(TEXT("Baked0")));
    TestEqual(TEXT("Custom preset lands after every baked entry"),
        (*AfterRefresh)[2].PresetId, FName(TEXT("CustomFromEditor")));

    // The list containing it is necessary but not sufficient — prove index 2
    // actually FIRES the custom preset, exactly the assumption
    // Server_SetCustomVenyxPresets/GetHoldPresets both depend on.
    Owner->VaelComponent->DebugSetCurrentVaelDirect(100.0f);
    const bool bFired = Venyx->FireHoldPreset(/*PresetIndex=*/2, /*ChargeFraction=*/1.0f, /*bIsMain=*/true);
    TestTrue(TEXT("Firing the custom preset's index succeeds"), bFired);
    TestNotNull(TEXT("Firing the custom preset spawns a projectile"), FindSpawnedProjectile(World));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
