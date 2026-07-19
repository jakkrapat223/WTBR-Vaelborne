// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRInteractionComponent.generated.h"

class AWTBRCharacter;
class AWTBRCorpseLootContainerActor;
class AWTBRDroppedTriggerActor;
class AWTBRGroundItemActor;
class AWTBRNexilWireActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWTBRCorpseLootInteractRequested, AWTBRCorpseLootContainerActor*, Container);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRInteractionComponent();

    UFUNCTION(BlueprintCallable, Category="WTBR|Interaction")
    AWTBRCorpseLootContainerActor* GetFocusedCorpseLootContainer() const;

    UFUNCTION(BlueprintCallable, Category="WTBR|Interaction")
    FText GetFocusedInteractionPromptText() const;

    // Read-only HUD query. Mirrors context-interact focus priority without
    // dispatching requests or mutating any gameplay state.
    UFUNCTION(BlueprintPure, Category="WTBR|Interaction")
    bool HasValidFocusedInteractable() const;

    // Fired on the owning client when the player requests to open a focused corpse
    // loot container. Bind in Blueprint HUD/UI to open the loot widget.
    // Broadcasts only when a valid container passes CanBeInteractedWithBy.
    // Does not mutate any gameplay state; server RPC is called only on pickup confirm.
    UPROPERTY(BlueprintAssignable, Category="WTBR | Interaction")
    FWTBRCorpseLootInteractRequested OnCorpseLootInteractRequested;

    // Called by AWTBRCharacter::Interact() on the owning client.
    // Returns true if OnCorpseLootInteractRequested was broadcast, false otherwise.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    bool RequestCorpseLootInteract();

    // F context interact dispatch (S4-A foundation). Resolves the focused
    // interaction in design-lock priority order and delegates to the matching
    // handler. Called by AWTBRCharacter::Interact() on the owning client.
    // Priority:
    //   0. downed teammate (hold)     -> RequestReviveInteract() [implemented — revive]
    //   1. corpse / container / chest -> RequestCorpseLootInteract() [implemented]
    //   2. dropped trigger            -> constraint-driven pickup dispatch [implemented S7A]:
    //                                    MainOnly -> active main, SubOnly -> active sub,
    //                                    Any -> reject AmbiguousTargetSlot (no slot guessed)
    //   3. BR ground item             -> Server_RequestPickupGroundItem [implemented S6]
    //   4. generic interactable       -> Nexil zipline wire grab [implemented] ->
    //                                    AWTBRCharacter::Server_GrabNexilWire (ally-owned,
    //                                    untriggered wires only; enemy trip mechanic untouched)
    //   5. no valid focus             -> no-op
    // Does not mutate gameplay state; server-authoritative pickup RPCs are unchanged.
    // Returns true if a branch handled the interaction, false otherwise.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    bool RequestContextInteract();

    // ── Revive (hold F on a downed teammate) ──────────────────────────────────
    // Focus-only: line-traces for a Downed teammate the owner can revive right
    // now — owner must be Alive and share a TeamId with the candidate (team-less
    // pairs are never teammates, so this is a no-op outside team modes). Checked
    // first in RequestContextInteract() because a downed-teammate focus can never
    // legitimately coincide with corpse/dropped-trigger/ground-item focus (a
    // corpse loot container only spawns on Eliminated, never on Downed).
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    AWTBRCharacter* GetFocusedRevivableTeammate() const;

    // Dispatches AWTBRCharacter::Server_RequestStartRevive for the focused
    // revivable teammate on button press (IA_Interact Started). The server owns
    // the entire non-interrupted hold-duration timer
    // (UWTBRHealthComponent::TryStartRevive/CompleteRevive) — this only needs to
    // say "start" once; it never tracks hold duration client-side. Returns true
    // if a revivable teammate was focused and the start RPC was dispatched.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    bool RequestReviveInteract();

    // Dispatches Server_RequestStopRevive for whichever teammate this client's
    // last RequestReviveInteract() call targeted, if any. Called by
    // AWTBRCharacter::InteractReleased() on IA_Interact Completed (button
    // release) — always safe to call, no-ops if no revive was started.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    void RequestStopReviveIfInProgress();

#if WITH_DEV_AUTOMATION_TESTS
    void SetFocusedCorpseLootContainerOverrideForTest(AWTBRCorpseLootContainerActor* Container);
    void ClearFocusedCorpseLootContainerOverrideForTest();
    void SetFocusedGroundItemOverrideForTest(AWTBRGroundItemActor* GroundItem);
    void ClearFocusedGroundItemOverrideForTest();
    void SetFocusedRevivableTeammateOverrideForTest(AWTBRCharacter* Teammate);
    void ClearFocusedRevivableTeammateOverrideForTest();
    AWTBRCharacter* GetCurrentReviveTargetForReleaseForTest() const { return CurrentReviveTargetForRelease.Get(); }
#endif

private:
    // Line-traces from the owner's viewpoint (same model as GetFocusedCorpseLootContainer)
    // and returns the focused dropped trigger, or null. Focus only — no mutation.
    // Used by RequestContextInteract priority 2 to decide whether to route to the
    // character's constraint-driven dropped-trigger pickup dispatch.
    AWTBRDroppedTriggerActor* GetFocusedDroppedTrigger() const;

    // Line-traces from the owner's viewpoint (same model as GetFocusedCorpseLootContainer)
    // and returns the focused BR ground item, or null. Focus only — no mutation.
    AWTBRGroundItemActor* GetFocusedGroundItem() const;

    // Nexil zipline wire grab (priority 4). AWTBRNexilWireActor has a real
    // collision box, but it's thin/rope-like — same aim-forgiveness problem
    // as dropped triggers, so this uses the same actor-iteration + view-cone
    // approach as GetFocusedDroppedTrigger instead of a line trace. Only
    // returns a wire the owner can actually grab (AWTBRNexilWireActor::
    // CanBeGrabbedBy — same team as the wire's caster, not yet triggered).
    AWTBRNexilWireActor* GetFocusedNexilWire() const;

    // Interaction focus trace length. In third person the viewpoint is the follow
    // camera (behind the pawn via a ~400-unit spring arm), so the trace must cover
    // the camera offset plus the interaction reach to hit items at/in front of the
    // pawn. The server still gates pickup independently by WTBRGroundItemPickupRange.
    UPROPERTY(EditDefaultsOnly, Category="WTBR|Interaction")
    float InteractionTraceDistance = 800.0f;

    // Reviver-side bookkeeping: which teammate the owner's last successful
    // RequestReviveInteract() targeted, so RequestStopReviveIfInProgress() knows
    // who to send the stop RPC for. Cleared whenever a stop is dispatched.
    TWeakObjectPtr<AWTBRCharacter> CurrentReviveTargetForRelease;

#if WITH_DEV_AUTOMATION_TESTS
    TWeakObjectPtr<AWTBRCorpseLootContainerActor> FocusedCorpseLootContainerOverrideForTest;
    TWeakObjectPtr<AWTBRGroundItemActor> FocusedGroundItemOverrideForTest;
    TWeakObjectPtr<AWTBRCharacter> FocusedRevivableTeammateOverrideForTest;
#endif
};
