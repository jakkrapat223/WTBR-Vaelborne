// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Trigger/WTBRPathPresetTypes.h"
#include "WTBRCustomPresetSubsystem.generated.h"

// Session-only storage for player-authored composite presets (Preset Editor, v1
// scope = Venyx only — see WTBRVenyxTrigger::GetHoldPresets).
//
// Deliberately a GameInstanceSubsystem, not a WorldSubsystem: it must survive
// ordinary level travel from the lobby (where the editor lives) into a match, but
// is explicitly NOT meant to outlive the running process — Transient + no
// serialization is what makes "session-only" true for free, with zero SaveGame
// work. Nothing here is server-authoritative; the server keeps its own validated
// copy once a locally-controlled pawn uploads it (UWTBRTriggerSetComponent::
// Server_SetCustomVenyxPresets).
UCLASS()
class WTBR_API UWTBRCustomPresetSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Adds a new preset or overwrites the existing one with the same PresetId.
    // Fails (returns false, no change) if PresetId is NAME_None — every authored
    // preset needs a real name to be selectable later.
    bool AddOrUpdatePreset(const FWTBRPathPreset& InPreset);

    // Returns false if no preset with that Id existed.
    bool DeletePreset(FName PresetId);

    const TArray<FWTBRPathPreset>& GetCustomVenyxPresets() const { return CustomVenyxPresets; }

    const FWTBRPathPreset* FindPreset(FName PresetId) const;

private:
    UPROPERTY(Transient)
    TArray<FWTBRPathPreset> CustomVenyxPresets;
};
