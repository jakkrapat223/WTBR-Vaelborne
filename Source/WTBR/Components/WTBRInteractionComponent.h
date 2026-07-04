// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRInteractionComponent.generated.h"

class AWTBRCorpseLootContainerActor;
class AWTBRDroppedTriggerActor;
class AWTBRGroundItemActor;

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
    //   1. corpse / container / chest -> RequestCorpseLootInteract() [implemented]
    //   2. dropped trigger            -> constraint-driven pickup dispatch [implemented S7A]:
    //                                    MainOnly -> active main, SubOnly -> active sub,
    //                                    Any -> reject AmbiguousTargetSlot (no slot guessed)
    //   3. BR ground item             -> Server_RequestPickupGroundItem [implemented S6]
    //   4. generic interactable       -> future: waits for interactable interface pass
    //   5. no valid focus             -> no-op
    // Does not mutate gameplay state; server-authoritative pickup RPCs are unchanged.
    // Returns true if a branch handled the interaction, false otherwise.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    bool RequestContextInteract();

#if WITH_DEV_AUTOMATION_TESTS
    void SetFocusedCorpseLootContainerOverrideForTest(AWTBRCorpseLootContainerActor* Container);
    void ClearFocusedCorpseLootContainerOverrideForTest();
    void SetFocusedGroundItemOverrideForTest(AWTBRGroundItemActor* GroundItem);
    void ClearFocusedGroundItemOverrideForTest();
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

    // Interaction focus trace length. In third person the viewpoint is the follow
    // camera (behind the pawn via a ~400-unit spring arm), so the trace must cover
    // the camera offset plus the interaction reach to hit items at/in front of the
    // pawn. The server still gates pickup independently by WTBRGroundItemPickupRange.
    UPROPERTY(EditDefaultsOnly, Category="WTBR|Interaction")
    float InteractionTraceDistance = 800.0f;

#if WITH_DEV_AUTOMATION_TESTS
    TWeakObjectPtr<AWTBRCorpseLootContainerActor> FocusedCorpseLootContainerOverrideForTest;
    TWeakObjectPtr<AWTBRGroundItemActor> FocusedGroundItemOverrideForTest;
#endif
};
