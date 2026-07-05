# WTBR D2 UMG Bag + Corpse Loot Conversion Plan

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1  
Baseline: `e331c12` - D1: Add in-match bag and corpse loot prototype

## Purpose

This document is a docs-only plan for converting the approved D1 in-match Bag + Corpse Loot HTML/CSS prototype into future UMG widgets.

This pass does not approve or implement:

- WBP / UMG assets
- Blueprint logic
- `.uasset`, `.umap`, or other binary assets
- `Source/` C++ changes
- gameplay mutation from UI
- Web Browser Widget production UI

The D1 HTML/CSS remains a visual reference only.

Traceability: every widget state in this plan should map back to a documented D1 visual state or CSS state class where possible, such as `bag-slot--warn` -> `BagFullWarning` and `bag-slot--cannot-transfer` -> `CannotTransfer`. D1 remains the visual lock; D2 only names the future UMG equivalents.

## Core Rules

- Gameplay remains server-authoritative.
- Client UI must not mutate inventory, slot assignment, corpse loot entries, dropped triggers, or ground items directly.
- UI sends requests only.
- Server validates and mutates.
- UI reads from replicated state or read-only ViewModel snapshot data.
- Future UMG must not hardcode physical keys such as `F`, `X`, `Q`, `E`, `4`, `LMB`, or `RMB`.

## Widget Hierarchy

### Root

Future root widget:

- `WBP_WTBRInMatchBagLootRoot`

Suggested role:

- Owns the in-match contextual overlay layer for Bag and Corpse Loot only.
- Anchored to the gameplay viewport, not a full-screen inventory replacement.
- Groups the right-side Bag panel and left-adjacent Corpse Loot panel.
- Handles show/hide transitions and safe margins only.

Suggested child structure:

- `CanvasPanel`
- `Overlay`
- `WBP_WTBRBagLootLayer`

### Bag + Loot Layer

Future container:

- `WBP_WTBRBagLootLayer`

Suggested children:

- `WBP_WTBRCorpseLootPanel`
- `WBP_WTBRBagPanel`

Layout intent:

- `WBP_WTBRBagPanel` stays as the right-side full-height in-match layer.
- `WBP_WTBRCorpseLootPanel` appears to the left of Bag only when corpse/loot context exists.
- Empty left side of the screen remains gameplay space / dim overlay space.

### Bag Panel

Future widget:

- `WBP_WTBRBagPanel`

Suggested child structure:

- `Border` / `Image` / `Overlay` root for glass frame
- `WBP_WTBRBagHeader`
- `WBP_WTBRBagCapacityRow`
- `WBP_WTBRActiveTriggerCard_Main`
- `WBP_WTBRActiveTriggerCard_Sub`
- `WBP_WTBRReserveRow_Main`
- `WBP_WTBRReserveRow_Sub`
- divider / spacer
- `WBP_WTBRBagGrid`
- `WBP_WTBRBagFooter`

### Bag Grid

Future widget:

- `WBP_WTBRBagGrid`

Suggested structure:

- `UniformGridPanel`
- 16 visual cells total

Visual cell intent:

- 14 active inventory slots
- 2 reserved visual cells

Suggested cell widget:

- `WBP_WTBRBagSlot`

### Bag Slot

Future widget:

- `WBP_WTBRBagSlot`

Suggested internal structure:

- `SizeBox` with stable square ratio
- `Overlay`
- item icon image / text token
- stack count badge
- optional locked badge
- optional tooltip anchor
- state frame / glow / drag target visuals

Supported visual states:

- filled
- empty
- reserved
- locked
- hover
- selected
- drag-target
- cannot-transfer
- bag-full warning as alternate state only, not default layout

### Active Trigger Cards

Future widgets:

- `WBP_WTBRActiveTriggerCard_Main`
- `WBP_WTBRActiveTriggerCard_Sub`

Reusable alternative:

- `WBP_WTBRActiveTriggerCard` with a Main/Sub style parameter

Content:

- trigger icon
- trigger display name
- trigger sublabel / slot label
- active slot chip

### Reserve Rows

Future widgets:

- `WBP_WTBRReserveRow_Main`
- `WBP_WTBRReserveRow_Sub`

Suggested cell widget:

- `WBP_WTBRReserveSlot`

States:

- filled reserve
- reserved/disabled

### Bag Footer

Future widget:

- `WBP_WTBRBagFooter`

Content:

- volume label
- current / max volume
- fill bar
- optional bag-full warning row

Rule:

- bag-full warning row hidden by default
- visible only in explicit bag-full scenario

### Corpse Loot Panel

Future widget:

- `WBP_WTBRCorpseLootPanel`

Suggested structure:

- glass frame root
- header row
- scroll list
- footer hint row

Suggested list entry widget:

- `WBP_WTBRCorpseLootEntry`

States:

- normal
- hover
- selected

## Binding Fields

The future UMG should bind to read-only snapshot/view data. No widget should read authoritative gameplay state and then mutate it directly.

### Bag Panel Fields

- `bIsBagVisible`
- `CurrentSlotCount`
- `MaxSlotCount`
- `CurrentVolume`
- `MaxVolume`
- `bShowBagFullWarning`
- `bCanTransferFocusedLootToBag`

Note: `bShowBagFullWarning` and `bCanTransferFocusedLootToBag` are read-only display hints derived from server-validated / replicated state. The UI must NOT compute transfer eligibility client-side, because a local guess can drift from authoritative truth and mislead the player.

### Active Trigger Card Fields

- `MainTriggerName`
- `MainTriggerIcon`
- `MainTriggerSlotLabel`
- `SubTriggerName`
- `SubTriggerIcon`
- `SubTriggerSlotLabel`

### Reserve Row Fields

- `MainReserveEntries`
- `SubReserveEntries`

Per reserve entry:

- `SlotLabel`
- `DisplayName`
- `bIsFilled`
- `bIsReserved`
- `bIsDisabled`

### Bag Grid Fields

- `InventorySlots`
- `ReservedVisualCellCount`

Per bag slot:

- `SlotIndex`
- `bHasItem`
- `ItemDisplayName`
- `ItemIcon`
- `StackCount`
- `Rarity`
- `bIsLocked`
- `bIsSelected`
- `bIsHovered`
- `bIsDragTarget`
- `bIsCannotTransfer`
- `bIsReservedVisualCell`

### Corpse Loot Panel Fields

- `bIsCorpseLootVisible`
- `CorpseLootTitle`
- `CorpseLootItemCount`
- `CorpseLootEntries`
- `CorpseLootFooterHintText`

Per loot entry:

- `EntryIndex`
- `DisplayName`
- `EntryTypeLabel`
- `Rarity`
- `Quantity`
- `Icon`
- `bIsSelected`
- `bIsHovered`
- `bCanTransfer`

## Visibility Rules

- Bag layer hidden by default when gameplay is in normal combat state and bag is not opened.
- Bag panel visible only when the in-match bag layer is active.
- Corpse Loot panel hidden by default.
- Corpse Loot panel visible only when `bIsBagLayerActive && bHasLootContext`.
- Corpse Loot panel should not consume the full screen.
- Bag Full warning row hidden by default.
- Cannot-transfer state hidden by default and shown only for a blocked transfer scenario.
- Reserved cells remain visible only inside the Bag panel grid and reserve rows.

## Drag / Drop Visual States

Drag/drop in this pass is presentation planning only. Final implementation must still route through request-only authoritative paths.

### Bag Slot Drag States

- `NormalFilled`
- `NormalEmpty`
- `Hover`
- `Selected`
- `Locked`
- `Reserved`
- `DragTarget`
- `CannotTransfer`
- `BagFullWarning`

### Corpse Loot Entry States

- `Normal`
- `Hover`
- `Selected`
- `BlockedTransfer`

### Drag / Drop Rules

- UI may highlight a potential drop target locally.
- UI may show a blocked transfer state locally.
- UI must not move items locally as authoritative truth.
- Final item movement remains a server-approved result.
- If future UX wants optimistic animation, it must still reconcile to replicated/server-approved state.

## Request-Only Actions

The UI should expose only request-style interactions.

### Bag / Loot Requests

- select bag slot
- select corpse loot entry
- request pickup focused corpse loot entry to a proposed target slot. The proposed slot is only a client request hint. The server validates the request and may reject or reroute the result. The UI must treat the returned replicated state / ViewModel snapshot as the source of truth.
- request pickup via existing focused interaction flow when applicable
- request close current loot/bag mode

### Important Rule

- UI must never directly remove an item from corpse loot.
- UI must never directly add an item to inventory.
- UI must never directly swap trigger slots.
- UI must never directly consume ground items.
- UI must only dispatch a request, then wait for authoritative replicated/UI snapshot refresh.

### Existing Related Request Paths To Respect

Known existing request-side references in the current codebase include:

- `UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()`
- `UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()`
- `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(...)`
- existing interaction/context pickup request chains

D2 does not approve changing those paths yet. It only maps future UMG onto that request-only model.

## Input / Key Display Rules

All visible input labels must come from Input Mapping / Enhanced Input, not hardcoded physical keys.

### Rules

- Use the current mapping context and input action to resolve display labels.
- Prefer the same display-name resolution strategy already used by the HUD contract work.
- Do not hardcode `F`, `X`, `Q`, `E`, `LMB`, `RMB`, or `4`.
- If an action is unbound, show a semantic fallback or hidden binding state.
- Do not guess a physical key.

### Likely Input Displays For Future UMG

- interact / pickup prompt binding
- cancel binding
- bag open / close binding if future design exposes it
- quick item binding if bag layer links to quick item affordances later

### Recommended Resolution Path

- `UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(...)`

### UMG Binding Strategy

- Widget receives a resolved display name or a small binding-display struct from a ViewModel layer.
- Widget does not hardcode platform-specific labels.
- Widget does not inspect raw physical key assumptions in designer text.

## Human Approval Checklist Before Future WBP / UMG Implementation

- Human approves exact WBP/UMG assets to create or edit.
- Human approves whether the bag layer belongs inside an existing HUD root or a separate contextual root.
- Human approves final widget names and folder layout.
- Human approves the authoritative request path for corpse loot transfer.
- Human approves whether drag/drop is mouse-first, gamepad-first, or both.
- Human approves which slot states are visible in shipping by default.
- Human approves input-display presentation style.
- Human approves the visual fallback when an input action is unbound.
- Human confirms no Web Browser Widget is used for production runtime.
- Human confirms no new gameplay mutation logic is introduced in UI.

## D2 Acceptance Criteria

- A docs-only Markdown conversion plan exists at `Docs/WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md`.
- The plan defines a widget hierarchy for Bag + Corpse Loot.
- The plan defines read-only binding fields.
- The plan defines visibility rules.
- The plan defines drag/drop visual states.
- The plan defines request-only actions.
- The plan defines Input Mapping / Enhanced Input key-display rules.
- The plan includes a human approval checklist before future WBP/UMG work.
- The plan includes a D2 acceptance criteria section.
- The plan includes a git safety checklist.
- No `.uasset`, `.umap`, WBP, UMG, binary assets, or `Source/` C++ files are edited in this pass.

## Git Safety Checklist

- Review `git status` before editing.
- Review recent commits before editing.
- Edit only approved files for the pass.
- Do not use `git add .`
- Do not use `git add ..`
- Do not use `git add -A`
- Stage only reviewed files
- Do not stage logs, local settings, temp artifacts, or unrelated assets
- Do not commit until human review approves the doc

## Non-Goals For D2

- No WBP creation
- No UMG asset creation
- No Blueprint implementation
- No C++ implementation
- No server RPC changes
- No test changes
- No staging, commit, or push
