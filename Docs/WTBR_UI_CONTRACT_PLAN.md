# WTBR UI Contract Plan - Inventory / Loot / Quick Item / Drop

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Baseline: `d5e27db` - Docs: Add in-match HUD visual direction

## Purpose And Status

This document defines the planned C++ communication contract between future player-facing UI widgets and authoritative gameplay systems for inventory, loot, quick item, drop, move/swap, trigger cards, and ground-item pickup presentation.

Status:

- Docs-only planning pass.
- No C++ changes are implemented here.
- No automation tests are implemented here.
- No WBP/UMG/Blueprint/.uasset/.umap/binary assets are approved or implemented here.
- Player-facing UI remains a later, human-approved implementation pass.

## Non-Goals

- No widget implementation.
- No Blueprint, WBP, UMG, `.uasset`, `.umap`, or binary asset edits.
- No final API lock for functions that are not implemented yet.
- No gameplay balance changes.
- No context-interact priority changes.
- No removal of the legacy dropped-trigger path.
- No client-side mutation shortcuts.

## Hard Rules

- Gameplay remains server-authoritative.
- Client UI must not directly mutate inventory, trigger slots, loot entries, ground items, or dropped triggers.
- UI sends requests only.
- Server validates requests and mutates authoritative state only on success.
- UI refreshes from replicated state or read-only viewmodel snapshots derived from replicated state.
- Rejected requests must not consume items, remove loot entries, destroy ground items, spawn drops, or alter trigger slots.
- Do not hardcode physical key F in gameplay logic; input must remain action/remapping based.
- Do not remove or weaken the legacy dropped-trigger path.
- Do not use `git add .`; stage only reviewed files in a later approved staging pass.

## UI Surfaces That Need Contracts

### HUD HP / VAEL

Purpose: show current survivability and energy state without driving gameplay mutation.

UI reads:

- Current HP.
- Max HP.
- Current VAEL.
- Max VAEL.
- Derived VAEL warning state: Normal, Warning, Critical.
- Optional server-replicated damage/restore feedback state when available.

UI requests:

- None for direct HP or VAEL mutation.
- Consumable use must go through inventory use requests, not direct HP mutation.

### Main/Sub Trigger Cards

Purpose: show equipped Main and Sub Trigger identity, input side, cooldown/readiness, and active/warning states.

UI reads:

- Active Main Trigger slot state.
- Active Sub Trigger slot state.
- Trigger display name/icon/type data.
- Trigger availability, cooldown, ammo/charge, and invalid/empty states when exposed.
- Main identity uses cyan/blue language; Sub identity uses orange/red language.

UI requests:

- Trigger activation should remain bound to gameplay input actions, not direct UI mutation.
- Inventory-to-trigger equip/swap may use a future request wrapper such as `RequestEquipTriggerFromInventory(SlotIndex, TargetTriggerSlot)`.

### Quick Item Dialog

Purpose: select and use a consumable quickly without exposing accidental drop actions.

UI reads:

- Inventory consumable slots from replicated inventory state.
- Item display data, quantity, availability, and disabled reason when known.
- Local radial selection state as presentation-only data.

UI requests:

- Existing authoritative path: `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)`.
- Future UI-facing wrapper may be named `RequestUseInventorySlot(SlotIndex)` and route to the authoritative server path.
- Cancel/close is presentation-only and must not call mutation APIs.

### Inventory / Bag

Purpose: show owned BR inventory slots and request use, equip/swap, move/swap, and full-slot/full-stack drop.

UI reads:

- `UWTBRInventoryComponent` replicated slot state.
- `FWTBRInventorySlot` item identity and quantity.
- Item data asset display fields.
- Empty slot state.
- Local hover, selection, and drag payload state as presentation-only data.

UI requests:

- Existing authoritative path: `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)`.
- Future wrapper: `RequestUseInventorySlot(SlotIndex)`.
- Future wrapper: `RequestDropInventorySlot(SlotIndex)`.
- Future wrapper: `RequestMoveInventorySlot(FromSlotIndex, ToSlotIndex)`.
- Future wrapper: `RequestEquipTriggerFromInventory(SlotIndex, TargetTriggerSlot)`.

### Loot / Corpse Container

Purpose: display corpse/container entries and request pickup or slot replacement without local transfer.

UI reads:

- Replicated corpse-container entries.
- Read-only loot entry view data.
- Container validity, distance/availability state, and local selected entry/target slot.
- Inventory/trigger target slot state for compatibility display.

UI requests:

- Existing authoritative path: `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(LootContainer, LootEntryIndex, TargetSlotIndex)`.
- Future wrapper may be named `RequestLootContainerSlot(ContainerId, SlotIndex)` if the implementation needs a UI-stable container handle.
- Future close path may be named `RequestCloseLootContainer(ContainerId)` if closing must notify gameplay; otherwise close remains local presentation only.

### BR Ground Item Pickup Prompt

Purpose: show focused BR ground item prompt while preserving context-interact routing and server-authoritative pickup.

UI reads:

- Focused ground item actor read-only display data.
- Item name/type/quantity.
- Interact prompt text from replicated or read-only focus state.

UI requests:

- Preferred pickup path remains context interact.
- Existing authoritative path: `AWTBRCharacter::Server_RequestPickupGroundItem(GroundItem)`.
- UI must not destroy, hide, or consume the ground item locally.

### Drop / Move / Swap Flow

Purpose: keep drag/drop UX deterministic while routing all mutation through server validation.

UI reads:

- Source slot, target slot, and drag payload as presentation-only state.
- Replicated inventory slots for final truth.

UI requests:

- RMB Drop and Drag Outside Drop should share `RequestDropInventorySlot(SlotIndex)`.
- Drag within Bag should use `RequestMoveInventorySlot(FromSlotIndex, ToSlotIndex)`.
- MVP drop always drops the full slot/full stack.
- No drop amount dialog in normal BR gameplay.

## Read-Only Data Exposed To UI

UI-facing data should be read-only snapshots or const accessors derived from replicated state:

- Inventory slot count.
- Inventory slot item data pointer or stable item id.
- Inventory slot quantity.
- Item display name, icon reference, item type, stack limit, and usability flags.
- Equipped Main/Sub Trigger display state.
- Corpse/container entry count.
- Corpse/container entry item/trigger identity, quantity, and target slot compatibility hint.
- Ground item display identity and quantity.
- HP / VAEL current and max values.
- Derived warning states such as Low VAEL Warning or Critical.
- Context prompt text and focused target type.

Read-only UI data must not expose mutable references that let UI directly edit authoritative arrays, actors, entries, or slot structs.

## Request-Only Functions UI May Call

Names are provisional unless they already exist in project code.

Existing authoritative paths:

- `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)`
- `AWTBRCharacter::Server_RequestPickupGroundItem(GroundItem)`
- `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(LootContainer, LootEntryIndex, TargetSlotIndex)`
- `AWTBRCharacter::Server_RequestPickupDroppedTrigger(DroppedTrigger, TargetSlotIndex)`
- `UWTBRInteractionComponent::RequestContextInteract()`
- `UWTBRInteractionComponent::RequestCorpseLootInteract()`

Suggested future UI-facing wrappers:

- `RequestUseInventorySlot(SlotIndex)`
- `RequestDropInventorySlot(SlotIndex)`
- `RequestMoveInventorySlot(FromSlotIndex, ToSlotIndex)`
- `RequestEquipTriggerFromInventory(SlotIndex, TargetTriggerSlot)`
- `RequestLootContainerSlot(ContainerId, SlotIndex)`
- `RequestCloseLootContainer(ContainerId)`

Wrapper expectations:

- Wrappers may live on character, controller, UI bridge component, or viewmodel object, but they must only dispatch requests.
- Wrappers must not mutate replicated inventory, trigger slots, loot entries, or ground item actors on the client.
- Wrappers should resolve UI-stable ids to authoritative actors/indices server-side where possible.
- Wrappers should preserve existing server validation and rollback behavior.

## Rejection / Failure Behavior

On rejection:

- Inventory remains unchanged.
- Trigger slots remain unchanged.
- Corpse/container entries remain unchanged or roll back to the previous authoritative state.
- Ground items remain valid and visible from replicated state.
- Dropped triggers remain valid and visible from replicated state.
- No dropped item actor is spawned unless the server accepts the drop.
- UI clears pending visual state after replicated state or result feedback confirms the reject.

Recommended failure categories:

- Invalid slot.
- Empty slot.
- Invalid item data.
- Incompatible target slot.
- Inventory full.
- Container invalid.
- Loot entry invalid or already consumed.
- Ground item invalid or already consumed.
- Dropped trigger invalid or already consumed.
- Out of range.
- Wrong match phase.
- Character dead or invalid.
- Server rejected / unknown.

## Replication / Refresh Assumptions

- Inventory UI refreshes from replicated `UWTBRInventoryComponent` slot state.
- Loot UI refreshes from replicated corpse-container entry state.
- Ground item prompt refreshes from focused, valid ground item actor state.
- Trigger cards refresh from replicated trigger-slot state.
- HP / VAEL HUD refreshes from replicated character/attribute state.
- Local hover, selection, drag payload, radial selection, and panel open/close are presentation state only.
- Optimistic visuals are allowed only if they are reversible and reconciled from replicated state.
- If a replicated actor disappears after a successful server action, UI treats disappearance as the authoritative refresh.

## Suggested Future C++ Classes / Interfaces / Viewmodels

These are planning candidates, not implementation approval:

- `UWTBRInventoryUIViewModel`: read-only inventory slot snapshot builder plus UI request dispatch helpers.
- `UWTBRLootContainerUIViewModel`: read-only corpse/container entries, target compatibility hints, and pickup request helpers.
- `UWTBRHUDViewModel`: HP, VAEL, Main/Sub Trigger card, prompt, and warning-state snapshots.
- `UWTBRQuickItemViewModel`: consumable-only filtered slot snapshot and selected consumable request dispatch.
- `UWTBRUIRequestRouterComponent`: optional owner-side bridge that exposes Blueprint/UI-safe request functions while delegating to existing authoritative character RPCs.
- `FWTBRInventorySlotView`: immutable UI snapshot for one inventory slot.
- `FWTBRLootEntryView`: immutable UI snapshot for one loot/container entry.
- `FWTBRTriggerCardView`: immutable UI snapshot for one Main/Sub Trigger card.
- `EWTBRUIRequestRejectReason`: optional lightweight rejection enum for future user feedback.
- `FWTBRUIRequestResult`: optional result payload for local feedback; must not be used as the source of truth over replication.

Implementation preference:

- Prefer read-only structs and const accessors over exposing mutable gameplay state.
- Prefer routing through existing authoritative functions where behavior already exists.
- Add new request APIs only when they are needed for UI flows not currently covered, such as drop and move/swap.
- Keep dev/test seams behind automation-only guards if deterministic UI contract automation needs them later.

## Suggested Future Automation Tests

Contract-level tests:

- `WTBR.UI.Contract.Inventory.ReadOnlySnapshotDoesNotMutate`
- `WTBR.UI.Contract.RequestUseInventorySlot.RoutesToServerUse`
- `WTBR.UI.Contract.RequestDropInventorySlot.RoutesToServerDrop`
- `WTBR.UI.Contract.RequestMoveInventorySlot.RoutesToServerMoveSwap`
- `WTBR.UI.Contract.RequestEquipTriggerFromInventory.RoutesToServerValidation`
- `WTBR.UI.Contract.LootContainer.RequestDoesNotMutateClientSide`
- `WTBR.UI.Contract.GroundItem.PromptDoesNotConsumeLocally`
- `WTBR.UI.Contract.QuickItem.CancelDoesNotRequestMutation`

Flow tests:

- `WTBR.Inventory.Use.ConsumableFromBag.ConsumesOnlyOnSuccess`
- `WTBR.Inventory.Drop.RMB.DropsFullSlot`
- `WTBR.Inventory.Drop.DragOutside.DropsFullSlot`
- `WTBR.Inventory.Drop.DoesNotMutateOnReject`
- `WTBR.Inventory.Drop.RMBAndDragOutsideShareRequestPath`
- `WTBR.Inventory.MoveSwap.DragWithinBag.SwapsSlots`
- `WTBR.Inventory.MoveSwap.RejectInvalidSlotDoesNotMutate`
- `WTBR.Inventory.QuickItem.UseSelectedConsumable`
- `WTBR.Inventory.QuickItem.CancelDoesNotConsume`
- `WTBR.Inventory.QuickItem.DoesNotExposeDrop`
- `WTBR.Loot.Container.PickupRequestDoesNotMutateClientSide`
- `WTBR.Loot.Container.StaleEntryRejectsAndRefreshes`
- `WTBR.GroundItem.Prompt.PickupRequestOnly`
- `WTBR.UI.Authority.NoClientDirectInventoryMutation`

## Explicit Implementation Boundary

This pass does not approve or implement WBP, UMG, Blueprint, `.uasset`, `.umap`, or binary asset work.

The next implementation pass must be separately approved by a human and should start from this contract plus `Docs/WTBR_UI_INVENTORY_LOOT_SPEC.md`.
