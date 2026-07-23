// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Subsystem/WTBRCustomPresetSubsystem.h"

bool UWTBRCustomPresetSubsystem::AddOrUpdatePreset(const FWTBRPathPreset& InPreset)
{
    if (InPreset.PresetId.IsNone())
    {
        return false;
    }

    for (FWTBRPathPreset& Existing : CustomVenyxPresets)
    {
        if (Existing.PresetId == InPreset.PresetId)
        {
            Existing = InPreset;
            return true;
        }
    }

    CustomVenyxPresets.Add(InPreset);
    return true;
}

bool UWTBRCustomPresetSubsystem::DeletePreset(FName PresetId)
{
    return CustomVenyxPresets.RemoveAll(
        [PresetId](const FWTBRPathPreset& Preset) { return Preset.PresetId == PresetId; }) > 0;
}

const FWTBRPathPreset* UWTBRCustomPresetSubsystem::FindPreset(FName PresetId) const
{
    return CustomVenyxPresets.FindByPredicate(
        [PresetId](const FWTBRPathPreset& Preset) { return Preset.PresetId == PresetId; });
}
