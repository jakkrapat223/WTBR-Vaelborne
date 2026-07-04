# WTBR UI Spec — Inventory / Loot / Quick Item / Drop Flow

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Baseline: `9ae5761` — Docs: Mark B8 corpse container default flip pass

## Purpose And Status

This document locks the first player-facing UI contract for BR inventory, ground-item pickup presentation, corpse/loot containers, quick item use, drop, and move/swap flows.

Status:

- Docs-only spec pass.
- No WBP/UMG/Blueprint/.uasset/.umap/binary assets are implemented here.
- No C++ or automation tests are implemented here.
- B8 is complete: corpse loot container backend is now production-default enabled through `MatchModeRules`.
- Player-facing UI is still separate work and not implemented yet.

## Non-Goals

- No WBP/UMG widget creation.
- No Blueprint or asset edits.
- No C++ request API implementation.
- No balance/tuning changes.
- No drag/drop amount split UI for MVP.
- No visual style finalization beyond functional behavior requirements.
- No change to context-interact priority order.

## Hard Rules

- Gameplay remains server-authoritative.
- Client UI must not directly mutate inventory, trigger slots, corpse loot, ground items, or dropped triggers.
- UI may only send request calls.
- Server validates the request, mutates authoritative state only on success, then replicated state refreshes the UI.
- Rejected requests must not consume inventory items, corpse-container entries, ground items, or dropped triggers.
- Do not hardcode physical key F in gameplay logic. Input must remain action/remapping based.
- Do not edit Blueprint/WBP/UMG/.uasset/.umap/binary assets unless a human explicitly approves.
- Do not remove the legacy dropped-trigger path.

## UI Surfaces

### Bag / Inventory UI

Purpose: show owned BR inventory slots and allow use, equip/swap, move/swap, and full-slot/full-stack drop requests.

Expected content:

- Fixed slot grid matching replicated inventory slot count.
- Item icon/name/quantity/readable type.
- Empty slot state.
- Selected/hovered/focused state.
- Invalid action feedback from server rejection.
- Close affordance.

Close behavior:

- `ESC` closes Bag.
- `X` closes Bag.

### BR Ground Item Pickup Presentation

Purpose: show a lightweight prompt or item preview when the player focuses a BR ground item.

Expected behavior:

- The context interact branch remains server-authoritative.
- UI presentation may display item name/type/quantity from replicated or read-only actor state.
- UI must not remove the ground item locally.
- Pickup request uses the existing server-authoritative ground item pickup path.
- On success, ground item disappears only after server-approved pickup/removal replicates.
- On reject, ground item remains and UI shows a non-destructive failure state.

### Corpse / Loot Container UI

Purpose: display corpse-container entries and allow player to request pickup/swap into a target trigger slot.

Current backend:

- Corpse/container backend is production-default enabled after B8.
- Container entries replicate through the corpse-container actor.
- Existing read-only view model helpers are UI-facing and must remain read-only.

Expected behavior:

- Opening the UI is local/presentation only.
- Confirming a loot pickup or slot swap sends a server-authoritative request.
- UI must not directly transfer loot, consume entries, replace trigger slots, destroy containers, or spawn actors.
- Server validates entry index, target slot, range, match rules, compatibility, consume, replacement, and rollback.
- Lower-priority dropped trigger / BR ground item non-mutation behavior must remain preserved when corpse/container priority wins.

### Quick Item Dialog

Purpose: fast consumable use under combat pressure.

Design lock:

- Tap `QuickItemKey` opens radial dialog.
- `LMB` uses selected consumable.
- `RMB` cancels/closes.
- No Drop action in Quick Item Dialog, to avoid mistakes during combat.

Expected behavior:

- Quick Item may select only consumables.
- Use sends the same server-authoritative use request as Bag consumable use.
- UI closes or refreshes after replicated state confirms consumption/reject.
- Cancel must not consume or mutate state.

### Drop Flow

Purpose: drop a full inventory slot / full stack to the ground.

Design lock:

- MVP drop always drops full stack / full slot.
- No drop amount dialog in normal BR gameplay.
- `RMB` Drop and Drag Outside Drop must go through the same request path.
- Provisional request name: `RequestDropInventorySlot(SlotIndex)`.

Expected behavior:

- Client UI sends a request only.
- Server validates player state, match state, slot index, item presence, and drop eligibility.
- Server removes the full slot/full stack and spawns a ground item.
- UI refreshes from replicated inventory and replicated ground item state.
- On reject, inventory remains unchanged and no ground item is spawned.

### Move / Swap Flow

Purpose: rearrange items within the Bag.

Design lock:

- Drag within Bag = Move / Swap.
- Moving onto an empty slot moves the full slot.
- Moving onto an occupied slot swaps full slot contents.
- Stack merging can be added later only if explicitly designed; MVP move/swap should be deterministic and full-slot based.

Expected behavior:

- Provisional request name: `RequestMoveOrSwapInventorySlot(SourceSlotIndex, TargetSlotIndex)`.
- Client UI sends a request only.
- Server validates both slot indices, item presence, slot constraints, and any future bag rules.
- UI refreshes from replicated inventory.
- On reject, source and target slots remain unchanged.

## User Actions Table

| Surface | User action | Expected UI behavior | Request boundary |
| --- | --- | --- | --- |
| Bag | `LMB` on Consumable | Use item from bag | `RequestUseInventoryItem(SlotIndex)` |
| Bag | `LMB` on Trigger | Equip / swap / select target slot according to slot rule | Provisional: `RequestEquipInventoryTrigger(SlotIndex, TargetTriggerSlotIndex)` |
| Bag | `RMB` on item | Drop full slot / full stack immediately | `RequestDropInventorySlot(SlotIndex)` |
| Bag | Drag item outside Bag/Loot UI | Drop full slot / full stack | `RequestDropInventorySlot(SlotIndex)` |
| Bag | Drag within Bag | Move / swap full slot | `RequestMoveOrSwapInventorySlot(SourceSlotIndex, TargetSlotIndex)` |
| Bag | `ESC` or `X` | Close Bag | No gameplay mutation |
| Quick Item | Tap `QuickItemKey` | Open radial dialog | No gameplay mutation |
| Quick Item | `LMB` on selected consumable | Use selected consumable | `RequestUseInventoryItem(SlotIndex)` |
| Quick Item | `RMB` | Cancel / close | No gameplay mutation |
| Corpse/Loot | Select entry + target slot | Request pickup/swap | `Server_RequestPickupCorpseLootEntry(Container, EntryIndex, TargetSlotIndex)` or wrapper |
| Ground item | Context interact on focused item | Request pickup | `Server_RequestPickupGroundItem(GroundItem)` or wrapper |

## Request API Expectations

Names are provisional unless already implemented.

Existing authoritative paths:

- `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)`
- `AWTBRCharacter::Server_RequestPickupGroundItem(GroundItem)`
- `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(LootContainer, LootEntryIndex, TargetSlotIndex)`

Provisional future request paths:

- `RequestDropInventorySlot(SlotIndex)`
- `RequestMoveOrSwapInventorySlot(SourceSlotIndex, TargetSlotIndex)`
- `RequestEquipInventoryTrigger(InventorySlotIndex, TargetTriggerSlotIndex)`

Implementation expectations:

- UI-facing functions may be local wrappers, but mutation must happen only after server validation.
- All request functions must reject invalid indices, null assets, empty slots, incompatible target slots, wrong match phase, dead/invalid characters, invalid distance, and already-consumed actors/entries.
- Request wrappers should avoid client-side optimistic mutation. Visual pending state is allowed if it is clearly reversible and reconciled from replicated state.
- Server rejection should provide enough signal for UI feedback, either through existing replication, logs, or future lightweight result notifications.

## Replication / Refresh Assumptions

- Inventory UI refreshes from replicated `UWTBRInventoryComponent` slot state.
- Corpse/loot UI refreshes from replicated corpse-container entries and `OnCorpseLootEntriesChanged`.
- Ground item prompts refresh from valid replicated ground-item actors and their read-only item/quantity state.
- Trigger slot UI refreshes from replicated trigger-slot state.
- UI must treat local selection, hover, drag payload, and radial selection as presentation-only state.
- After every server-approved mutation or rejection, UI should reconcile with replicated state rather than trusting a client-side prediction.

## Rejection Behavior

On reject:

- Leave inventory slots unchanged.
- Leave corpse-container entries unchanged or rolled back.
- Leave ground item actor valid and unconsumed.
- Leave dropped trigger actor valid and unconsumed.
- Close no UI automatically unless the user explicitly requested close.
- Show a compact failure cue when possible.

Recommended rejection categories:

- Invalid slot.
- Empty slot.
- Incompatible target slot.
- Inventory full.
- Invalid item data.
- Not in match.
- Out of range.
- Already consumed.
- Server rejected / unknown.

## Edge Cases

- Double-click / rapid repeat requests must not duplicate consume/drop/spawn.
- Dragging outside UI while over world widgets still counts as Drop only if the source was a valid Bag slot and the release target is outside Bag/Loot surfaces.
- Dragging from Loot UI into Bag should not directly transfer; it should select target slot and request server-authoritative loot pickup/swap.
- Quick Item radial must not expose Drop.
- Consuming at full HP or unsupported effects must not consume the item.
- Dropping from an empty slot is a no-op reject.
- Moving from a slot to itself is a no-op and should not call the server unless implementation chooses to validate as no-op.
- Corpse-container entries that are consumed by another player must refresh before the player can confirm stale selection.
- Late-join clients must rely on replicated container state and not local cached state.
- If UI is open while the player dies, leaves match, or moves out of range, requests should reject and UI should refresh/close according to later UX policy.

## Future UI Automation Tests

Required future tests:

- `WTBR.Inventory.Use.ConsumableFromBag.ConsumesOnlyOnSuccess`
- `WTBR.Inventory.Drop.RMB.DropsFullSlot`
- `WTBR.Inventory.Drop.DragOutside.DropsFullSlot`
- `WTBR.Inventory.Drop.DoesNotMutateOnReject`
- `WTBR.Inventory.MoveSwap.DragWithinBag.SwapsSlots`
- `WTBR.Inventory.QuickItem.UseSelectedConsumable`
- `WTBR.Inventory.QuickItem.CancelDoesNotConsume`

Additional recommended tests:

- `WTBR.Inventory.Drop.RMBAndDragOutsideShareRequestPath`
- `WTBR.Inventory.MoveSwap.RejectInvalidSlotDoesNotMutate`
- `WTBR.Inventory.QuickItem.DoesNotExposeDrop`
- `WTBR.Loot.Container.PickupRequestDoesNotMutateClientSide`
- `WTBR.Loot.Container.StaleEntryRejectsAndRefreshes`
- `WTBR.GroundItem.Prompt.PickupRequestOnly`
- `WTBR.UI.Authority.NoClientDirectInventoryMutation`

## Implementation Notes For Later

- Start with docs/spec review before WBP/UMG work.
- Any UI implementation should use existing read-only model helpers where possible.
- Prefer adding C++ request wrappers only when they clarify UI boundaries or reduce Blueprint surface risk.
- Keep WBP/UMG and binary asset changes human-approved and reviewable.
- Keep gameplay server-authoritative even when adding local hover, selection, drag payload, or pending visual states.
