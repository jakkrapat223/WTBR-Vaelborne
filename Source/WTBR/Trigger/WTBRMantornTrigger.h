// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRMantornTrigger.generated.h"

// Mantorn — the Feryx + Feryx composite transform FORM (canon name "Mantorn").
// This is NOT a slotted/equippable Trigger. UWTBRTriggerSetComponent instantiates
// one of these as the active form's behavior executor while the player is in
// Mantorn form, initialized with the active Main Feryx DataAsset. Two moves:
//   • WhipSlash() — Tap: forward capsule sweep (segmented Trion blade whip).
//   • SpinSlash() — Hold: 360° AOE spin around the player (reuses the classic
//     Mantorn spin sweep). Each move charges its own Vael cost (⚠ Playtest,
//     DataAsset-driven via FWTBRMantornParams). Server-authoritative — the caller
//     (server input path) is responsible for only invoking these on authority.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRMantornTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    // Tap in-form: forward whip. Returns true if it actually swung (auth + off
    // cooldown + enough Vael). Consumes MantornParams.WhipVaelCost.
    bool WhipSlash();

    // Hold in-form: 360° AOE spin. Returns true if it actually spun.
    // Consumes MantornParams.SpinVaelCost.
    bool SpinSlash();

    // Called on ALL instances (server + client) via OnMeleeSwing delegate.
    // Implement VFX/SFX in BP_WTBRMantornTrigger.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornSpinStart();

    // Called on SERVER only for the AOE spin — carries damage data.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornSpinHit(const FHitResult& Hit, float DamageDealt);

    // Called on SERVER only for the forward whip — carries damage data.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Mantorn | VFX")
    void OnMantornWhipHit(const FHitResult& Hit, float DamageDealt);

private:
    // Shared preconditions for both moves: valid owner + authority + DataAsset +
    // not on cooldown. Does NOT check Vael (each move checks its own cost).
    bool CanPerformInFormAction() const;

    // 360° sphere sweep around the owner (classic Mantorn spin).
    void PerformSpinSweep(TArray<FHitResult>& OutHits);

    // Forward capsule sweep along the aim direction (whip).
    void PerformWhipSweep(TArray<FHitResult>& OutHits);
};
