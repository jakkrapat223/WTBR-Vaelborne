// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBREscudoTrigger.generated.h"

class AWTBRAegornWallActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEscudoWallPlaced);

// Escudo — fixed-position Trion wall (canon: erected by touching a surface,
// opaque, cannot be moved/reshaped, far more durable than Shield/Aegorn and
// can stop attacks Shield can't, at a high Vael cost). Split out from Aegorn
// because canon treats these as two distinct Triggers, not tap/hold of one.
//
// Tap (< hold threshold) places one panel instantly, unchanged since the
// 2026-07-13 Phase 1 pass. Hold opens a preset wheel + ghost preview — that
// whole flow lives in AWTBRCharacter (client-local, see
// wtbr-escudo-hold-preset-design-lock project memory for why it can't live
// in Activate_Implementation, which never even runs on a real remote client
// until the server round-trip completes). This class only owns the pieces
// both Tap and the Hold flow's confirm step need to agree on: per-panel
// placement validation and the actual spawn.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBREscudoTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Escudo | Events")
    FOnEscudoWallPlaced OnWallPlaced;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Escudo | VFX")
    void OnEscudoWallSpawned(AWTBRAegornWallActor* WallActor);

    // Builds the rotation a panel anchored to SurfaceNormal should spawn with.
    // Canon rule (owner-locked 2026-07-20): Escudo sprouts PERPENDICULAR to
    // whatever surface it grows from — straight up out of a floor, horizontally
    // out of a wall, hanging down from a ceiling. The panel's local +Z (its
    // "grow"/height axis) is therefore always aligned to the surface normal,
    // and the eruption in SpawnOnePanel travels along that same axis.
    //
    // The one remaining degree of freedom is spin about the normal. It prefers
    // to aim the panel's thin local +X along ViewDirection, so a floor-placed
    // wall turns its wide face toward the player exactly as the old yaw-only
    // placement did. Looking straight down the normal (facing a wall head-on)
    // makes that degenerate, so it falls back to world up — which is what
    // gives a wall-mounted panel its horizontal, shelf-like jut.
    static FRotator ComputeSurfaceAlignedRotation(
        const FVector& SurfaceNormal, const FVector& ViewDirection);

    // Finds the supporting surface for PanelLocation by tracing along
    // SurfaceNormal (within EscudoSurfaceSnapRange either side, so a panel
    // offset onto a bump or a dip still snaps), checks it isn't too close to an
    // existing wall (EscudoMinWallSpacing), AND rejects it if the wall's real
    // footprint (from WallClass's own collision, not the unused EscudoWallSize
    // field) would overlap any Pawn — a wall that traps a living character
    // inside solid geometry is never a legal placement, even when nothing else
    // about the spot is wrong (owner-found PIE bug: a multi-panel preset
    // placed blind, with no ghost-preview visual up yet, spawned overlapping
    // the placing character and got them physically stuck). Shared by real
    // placement (Tap and the preset-confirm handler) and the client-local
    // ghost preview, which uses it advisory-only — the server call from the
    // confirm RPC is the only one that is ever authoritative (project's
    // "preview is always advisory" input design lock).
    //
    // SurfaceNormal is the anchor's normal, not a per-panel one: every panel in
    // a preset shares the anchor's orientation (same pre-existing limitation
    // that makes an L-preset's side panel face the same way as its front
    // panels), and only snaps its POSITION to its own local surface.
    //
    // OutSurfacePoint is filled in whenever a supporting surface is found, even
    // when the function goes on to return false — the ghost preview needs a
    // real surface position to draw a REJECTED panel at too, rather than
    // leaving it floating at raw aim height.
    UFUNCTION(BlueprintPure, Category = "WTBR | Escudo | Placement")
    bool ValidatePanelPlacement(
        TSubclassOf<AWTBRAegornWallActor> WallClass, const FVector& PanelLocation,
        const FVector& SurfaceNormal, const FRotator& PanelRotation,
        FVector& OutSurfacePoint) const;

    // Loads + null-checks WallActorClass (logs and returns nullptr on
    // failure). Deliberately separate from SpawnOnePanel and called BEFORE
    // any Vael is spent, same as the original Tap ordering — a load failure
    // must never cost the player anything, whether it's one panel or a whole
    // preset's worth.
    TSubclassOf<AWTBRAegornWallActor> LoadWallActorClass() const;

    // Spawns and erupts one real wall at an already-validated surface point +
    // rotation, using an already-loaded class (see LoadWallActorClass). The
    // eruption travels along Rotation's local +Z, which
    // ComputeSurfaceAlignedRotation has already aligned to the surface normal,
    // so the wall rises out of a floor, juts out of a wall, or drops out of a
    // ceiling without this function needing the normal passed separately.
    // Does not consume Vael or validate placement — callers own that
    // (Activate_Implementation for Tap; AWTBRCharacter's confirm RPC handler
    // for a whole preset's worth of panels).
    AWTBRAegornWallActor* SpawnOnePanel(
        TSubclassOf<AWTBRAegornWallActor> WallClass, const FVector& SurfacePoint, const FRotator& Rotation);

private:
    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Escudo | Wall")
    TSoftClassPtr<AWTBRAegornWallActor> WallActorClass;
};
