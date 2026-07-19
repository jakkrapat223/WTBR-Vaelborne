// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "WTBRNexilTrigger.generated.h"

class AWTBRNexilWireActor;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNexilWirePlaced);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnNexilWireTriggered, AWTBRNexilWireActor*, TriggeredWire);

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRNexilTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    UWTBRNexilTrigger();

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Nexil | Events")
    FOnNexilWirePlaced OnWirePlaced;

    UPROPERTY(BlueprintAssignable, Category = "WTBR | Nexil | Events")
    FOnNexilWireTriggered OnWireTriggered;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION()
    void NotifyWireTriggered(AWTBRNexilWireActor* TriggeredWire);

    UFUNCTION()
    void NotifyWireDestroyed(AActor* DestroyedActor);

    UFUNCTION(BlueprintPure, Category = "WTBR | Nexil | State")
    int32 GetActiveWireCount() const;

    // Blueprint helper for the ghost-preview actor: UWTBRNexilTrigger is a plain
    // UObject (not an Actor), so the Blueprint Editor's "Spawn Actor from Class"
    // convenience node can't verify an implicit World Context here and hides it
    // from the node search even though GetWorld() resolves fine in C++ (Outer is
    // the owning AWTBRCharacter). Call this instead — a normal BlueprintCallable
    // function has no such ambiguity.
    UFUNCTION(BlueprintCallable, Category = "WTBR | Nexil | VFX")
    AActor* SpawnNexilGhostPreviewActor(TSubclassOf<AActor> GhostActorClass);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilWirePlaced(AWTBRNexilWireActor* WireActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilWireTriggeredVFX(AWTBRNexilWireActor* WireActor);

    // Cypher-style two-point ghost preview (owner's own design ask): fires
    // continuously (~20Hz, via GhostPreviewTimer) whenever this Trigger instance is
    // the character's active Main or Sub — no button needs to be held first.
    // Before the first anchor (IsPlacingWire()==false) WireTransform is a short
    // marker at the aimed floor point; after the first anchor it is the full wire
    // spanning anchor→current-aim, and WireLength is that span's actual length.
    // Blueprint owns the translucent mesh + green/red material swap (no AI edits to
    // .uasset); this only hands over the transform the real placement would use
    // plus whether it currently passes the same validity check that gates real
    // placement (see ComputeGhostPreviewTransform). WireLength lets Blueprint scale
    // the ghost mesh to match without a DataAsset lookup — same value
    // AWTBRNexilWireActor::InitializeWire uses to scale the real wire's mesh.
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilGhostPreviewUpdated(const FTransform& WireTransform, float WireLength, bool bPlacementValid);

    // Fired when the preview should be hidden entirely (this Trigger stopped
    // being the active Main/Sub, or the owner/world became invalid).
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nexil | VFX")
    void OnNexilGhostPreviewHidden();

    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    virtual void Deactivate_Implementation() override;

#if WITH_DEV_AUTOMATION_TESTS
    // Test-only: inject the wire actor class (normally set on BP_WTBRNexilTrigger)
    // so headless tests can exercise placement/FIFO/cleanup without the Blueprint.
    void SetWireActorClassForTest(TSoftClassPtr<AWTBRNexilWireActor> InClass) { WireActorClass = InClass; }

    // Test-only: headless fixture worlds have no initialized physics scene, so the
    // aim / opposite-surface traces can never hit anything. Inject the endpoints
    // ComputeWirePlacement should return so placement stays exercisable.
    void SetTestPlacement(const FVector& A, const FVector& B)
    {
        TestPlacementOverride = TPair<FVector, FVector>(A, B);
    }
#endif

private:
    // TD Fix 1: TWeakObjectPtr prevents crash when Wire destroyed early
    TArray<TWeakObjectPtr<AWTBRNexilWireActor>> ActiveWires;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Nexil | Wire")
    TSoftClassPtr<AWTBRNexilWireActor> WireActorClass;

    // Ghost preview assets. C++ owns the whole preview (spawn, transform, length
    // scale, valid/invalid material swap) — the Blueprint event graph is no longer
    // involved, after four separate graph-wiring breakages (wrong class, dead
    // branch, disconnected Spawn exec pin). These are data-only, so there are no
    // exec wires left to come loose. Defaults point at the existing assets in the
    // constructor, so nothing needs assigning in BP for this to work.
    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Nexil | Ghost Preview")
    TSoftClassPtr<AActor> GhostPreviewActorClass;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Nexil | Ghost Preview")
    TSoftObjectPtr<UMaterialInterface> GhostValidMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Nexil | Ghost Preview")
    TSoftObjectPtr<UMaterialInterface> GhostInvalidMaterial;

    // Spawns (if needed) and drives the ghost actor for this frame's preview.
    void UpdateGhostPreviewActor(const FTransform& Transform, float PreviewLength, bool bValid);

    void PlaceWire(const FTransform& SpawnTransform, float WireSpanLength);
    void RemoveOldestWire();

    // True Cypher trapwire placement, resolved fresh every preview tick and on the
    // single Activate press (no anchor state, no second press): an aim-trace from
    // the camera finds the surface under the crosshair (endpoint A), then a second
    // trace fired perpendicularly out of that surface (along its impact normal) up
    // to NexilMaxWireSpan finds the facing surface across the gap (endpoint B).
    // Returns false when there's no surface under the crosshair, or nothing to
    // anchor to on the far side — a trapwire needs two surfaces, so open air is not
    // placeable. Because B comes from a trace that started at A, the span between
    // them is clear by construction; ValidateWireSpan only has to bound the length.
    bool ComputeWirePlacement(FVector& OutA, FVector& OutB) const;

    // Throttles the (verbose, ~20Hz-tick-rate) placement failure logs to roughly
    // twice a second so `wtbr.Debug.ValidationLogs 1` stays readable.
    mutable float LastAimPointFailLogTime = -1.0f;

    // Same throttle idea for the ghost scale readout in UpdateGhostPreviewActor.
    float LastGhostScaleLogTime = -1.0f;

    // Span A→B is placeable when its length is within [MinWireSpan, NexilMaxWireSpan].
    // Clearance between the endpoints is guaranteed by how ComputeWirePlacement finds
    // B (a trace outward from A), so it isn't re-checked here.
    bool ValidateWireSpan(const FVector& A, const FVector& B) const;

    // Builds the spawn transform for a wire spanning A→B: located at the midpoint,
    // rotated so the wire's local Y axis (its length axis) points along A→B. OutLength
    // is |B−A|. Used identically by the ghost preview and real placement.
    void BuildSpanTransform(const FVector& A, const FVector& B, FTransform& OutTransform, float& OutLength) const;

    // Ghost preview transform for the wire that a press right now would place — the
    // full A→B span, shown continuously with no press required. OutLength is what the
    // ghost mesh scales to. Returns false when there's nothing placeable under the
    // crosshair (TickGhostPreview hides the ghost); otherwise bOutValid carries the
    // green/red validity.
    bool ComputeGhostPreviewTransform(FTransform& OutTransform, float& OutLength, bool& bOutValid) const;

#if WITH_DEV_AUTOMATION_TESTS
    TOptional<TPair<FVector, FVector>> TestPlacementOverride;
#endif

    UFUNCTION()
    void OnActiveTriggerSlotChanged(ETriggerSlot NewSlot);

    void RefreshGhostPreviewActiveState();
    void StartGhostPreview();
    void StopGhostPreview();
    void TickGhostPreview();

    FTimerHandle GhostPreviewTimer;

    // C++ owns the ghost's on/off lifecycle so hiding never depends on the BP's
    // OnNexilGhostPreviewHidden being wired correctly (the previous "preview stays
    // stuck after a slot switch" bug): SpawnNexilGhostPreviewActor caches whatever
    // it hands back to Blueprint here, and StopGhostPreview destroys it. The BP's
    // existing lazy-spawn (IsValid(GhostPreviewActor)? → spawn) transparently
    // re-creates it the next time the preview starts.
    TWeakObjectPtr<AActor> GhostPreviewActorInstance;
};
