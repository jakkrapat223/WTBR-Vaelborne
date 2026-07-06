// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/WTBRBagLootViewTypes.h"
#include "WTBRBagLootViewModelComponent.generated.h"

class AWTBRCharacter;
class AWTBRCorpseLootContainerActor;
struct FWTBRInventorySlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWTBRBagLootSnapshotChanged);

// ─────────────────────────────────────────────────────────────────────────────
// UWTBRBagLootViewModelComponent — read-only ViewModel foundation for the Bag and
// Corpse Loot panels, mirroring UWTBRHUDViewModelComponent's pattern.
//
// Hard rule: this component is READ-ONLY / REQUEST-ONLY. It must never mutate
// inventory, loot containers, corpse boxes, ground items, Vael, HP, or trigger
// slot state directly. All data comes from existing read-only accessors
// (UWTBRInventoryComponent::GetInventorySlots(),
// AWTBRCorpseLootContainerActor::BuildLootEntryViewModel(), etc.); the only
// outgoing calls are request-only wrappers that dispatch to EXISTING
// server-authoritative RPCs on AWTBRCharacter.
//
// See Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagLoot_BindingPlan.md for the
// full audit of what is/isn't wired and why.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup=(WTBR), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRBagLootViewModelComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRBagLootViewModelComponent();

    UPROPERTY(BlueprintAssignable, Category="WTBR | Bag Loot")
    FWTBRBagLootSnapshotChanged OnBagLootSnapshotChanged;

    UFUNCTION(BlueprintPure, Category="WTBR | Bag Loot")
    FWTBRBagLootSnapshot GetBagLootSnapshot() const;

    // Rebuilds the cached snapshot from current inventory/corpse-loot state and
    // broadcasts OnBagLootSnapshotChanged. Also opportunistically rebinds the
    // corpse-loot-entries-changed delegate to whichever container is focused right
    // now (see the focus-tracking TODO below) — safe to call manually at any time.
    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot")
    void RefreshBagLootSnapshot();

    // ── Request-only wrappers ────────────────────────────────────────────────
    // Each dispatches to an EXISTING server-authoritative RPC only. Neither of
    // these mutates inventory, loot entries, ground items, or trigger slots
    // locally — they only send a request and return whether it was dispatched.

    // Wraps AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex).
    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot | Requests")
    bool RequestUseBagItem(int32 SlotIndex);

    // Wraps AWTBRCharacter::Server_RequestPickupCorpseLootEntry(FocusedContainer,
    // LootIndex, TargetSlotIndex), resolving FocusedContainer fresh at call time
    // via UWTBRInteractionComponent::GetFocusedCorpseLootContainer(). Fails safely
    // (returns false, no server call) if nothing is currently focused.
    UFUNCTION(BlueprintCallable, Category="WTBR | Bag Loot | Requests")
    bool RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex);

    // ── Intentionally NOT implemented (documented, not stubbed) ─────────────
    //
    // RequestPickupFocusedGroundItem() / RequestPickupFocusedDroppedTrigger():
    //   UWTBRInteractionComponent::GetFocusedGroundItem() and
    //   GetFocusedDroppedTrigger() are PRIVATE — there is no public accessor to
    //   isolate "the focused ground item" or "the focused dropped trigger"
    //   specifically. The only public entry point is the unified priority dispatcher
    //   UWTBRInteractionComponent::RequestContextInteract(), which is already
    //   exposed as UWTBRHUDViewModelComponent::RequestPickupFocusedTarget(). Adding
    //   a second, differently-named wrapper around the exact same call here would
    //   be redundant, so UI should call the existing HUD ViewModel function instead.
    //
    // RequestTakeAllLoot(SlotIndex...) / RequestMoveBagItem(From, To) /
    // RequestDropBagItem(SlotIndex) / RequestEquipTriggerFromBag(SlotIndex, TargetSlot):
    //   No existing server-authoritative RPC exists for any of these (confirmed by
    //   grepping AWTBRCharacter.h for RequestDrop/RequestMove/RequestEquip — zero
    //   matches beyond the pickup/use RPCs already wrapped above). Implementing
    //   them here would require inventing new gameplay mutation, which this pass
    //   must not do. See WBP_BagLoot_BindingPlan.md §4 for the provisional names
    //   already tracked in the UI contract docs.

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FWTBRBagLootSnapshot CachedSnapshot;

    // Currently bound corpse-loot-container delegate target, if any (see the
    // focus-tracking TODO in RebindFocusedCorpseLootContainerDelegate()).
    TWeakObjectPtr<AWTBRCorpseLootContainerActor> BoundCorpseLootContainer;

    void BindOwnerDelegates();
    void UnbindOwnerDelegates();

    AWTBRCharacter* ResolveOwnerCharacter() const;
    AWTBRCorpseLootContainerActor* ResolveFocusedCorpseLootContainer(const AWTBRCharacter* Character) const;

    // TODO(Phase4C-BagLoot): UWTBRInteractionComponent has no "focused interactable
    // changed" delegate today (confirmed by audit) — only OnCorpseLootInteractRequested
    // (fires on an explicit interact key press, not on focus change) exists. This
    // means corpse-loot-container focus cannot be tracked purely reactively. As a
    // safe, non-ticking compromise, this function re-resolves the focused container
    // every time RefreshBagLootSnapshot() runs (itself triggered by
    // OnInventoryChanged or the currently-bound container's OnCorpseLootEntriesChanged)
    // and rebinds the delegate if the focused container has changed. If focus changes
    // to a different container and nothing else triggers a refresh in between, the
    // snapshot will not update until RefreshBagLootSnapshot() is called again —
    // callers (e.g. a future widget) should call it manually on relevant input
    // events (such as when a pickup/interact prompt becomes visible) until a real
    // focus-changed delegate exists.
    void RebindFocusedCorpseLootContainerDelegate(AWTBRCorpseLootContainerActor* FocusedContainer);

    FWTBRBagLootSnapshot BuildLiveSnapshot(const AWTBRCharacter* Character, const AWTBRCorpseLootContainerActor* FocusedContainer) const;
    FWTBRBagPanelSnapshot BuildBagPanelSnapshot(const AWTBRCharacter* Character) const;
    FWTBRBagSlotSnapshot BuildBagSlotSnapshot(int32 SlotIndex, const FWTBRInventorySlot& Slot) const;
    FWTBRCorpseLootPanelSnapshot BuildCorpseLootPanelSnapshot(const AWTBRCorpseLootContainerActor* FocusedContainer) const;
    FWTBRLootEntrySnapshot BuildLootEntrySnapshot(const AWTBRCorpseLootContainerActor* Container, int32 LootIndex) const;

    static FText FormatCapacityText(int32 CurrentUsed, int32 MaxSlots);
    static FText FormatCountText(int32 Quantity);
    static FText FormatUnknownWeightText();

    UFUNCTION()
    void OnInventoryChangedHandler();

    UFUNCTION()
    void OnCorpseLootEntriesChangedHandler();
};
