# WTBR D3 UMG Bag + Corpse Loot Asset Approval Gate

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1  
Baseline: `bf16cab` — C4: Polish HUD empty trigger fallback text  
Pass: D3 — Docs-only approval gate  
Precedes: D4 — Future WBP/UMG implementation (pending human approval)

---

## 1. Purpose

D3 is a docs-only approval gate that sits between the D2 conversion plan and any future WBP/UMG implementation pass. No assets are created, edited, staged, or committed in this pass.

D3 answers:

- Which WBP/UMG assets are proposed for future creation?
- Which files may be created later (pending approval)?
- Which existing files may be edited later (pending approval)?
- Which files remain forbidden unless separately approved?
- What is the safe first implementation slice?
- What must the human approve before any WBP/UMG work begins?

This document does not grant permission to implement anything. Every item is Pending Human Approval until a human explicitly signs off on each line in the checklist in Section 8.

---

## 2. Proposed Future Widget Assets

The assets listed below are proposed for future creation only. They do not exist yet. No `.uasset`, WBP, or UMG file has been created in this pass.

### 2.1 Path Convention Note — Requires Human Confirmation

> **⚠ TENTATIVE — Requires human confirmation before any asset is created.**
>
> The repo currently has no established WBP/UMG directory. `Content/UI/` exists but contains only icons. The proposed base path `Content/UI/HUD/` is a planning placeholder.
>
> Additionally, D2 (`WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md`) established a widget naming convention using the `WBP_WTBR` prefix (e.g., `WBP_WTBRBagPanel`). The simplified names below (`WBP_BagPanel`, etc.) are the prompt-suggested short forms. Human must confirm which prefix and folder convention to use before any asset is created.
>
> Candidates requiring human decision:
> - Path: `Content/UI/HUD/` vs another path
> - Prefix: `WBP_WTBR*` (D2 convention) vs `WBP_*` (simplified)

### 2.2 Proposed Asset Table

#### Asset: WBP\_InMatchBagLootLayer  
*(D2 equivalent: `WBP_WTBRBagLootLayer`)*

| Field | Value |
|---|---|
| Proposed path | `Content/UI/HUD/WBP_InMatchBagLootLayer.uasset` ⚠ tentative |
| D2 name | `WBP_WTBRBagLootLayer` |
| Purpose | Container layer that groups Bag panel and Corpse Loot panel for in-match contextual overlay. Handles show/hide transitions and safe viewport margins. |
| Parent / owner | Future HUD root or separate contextual overlay root — pending human approval on ownership |
| Child widgets | `WBP_BagPanel`, `WBP_CorpseLootPanel` |
| Data source | HUD ViewModel / read-only snapshot: `bIsBagVisible`, `bHasLootContext`, `bIsBagLayerActive` |
| Request-only actions | None — visibility shell only |
| Visibility rules | Hidden by default. Visible only when bag layer is actively opened by the player. |
| May be created in D4-A | Yes — skeleton/shell only |
| May be edited later | Yes — visual and binding integration in D4-B and later |
| Approval status | **Pending Human Approval** |

---

#### Asset: WBP\_BagPanel  
*(D2 equivalent: `WBP_WTBRBagPanel`)*

| Field | Value |
|---|---|
| Proposed path | `Content/UI/HUD/WBP_BagPanel.uasset` ⚠ tentative |
| D2 name | `WBP_WTBRBagPanel` |
| Purpose | Right-side full-height in-match bag view. Contains trigger cards, reserve rows, bag grid, header, capacity row, and footer. |
| Parent / owner | `WBP_InMatchBagLootLayer` |
| Child widgets | `WBP_BagGrid`, `WBP_BagFooter` (children defined in D2; full subwidget list pending human approval) |
| Data source | HUD ViewModel snapshot: `bIsBagVisible`, `CurrentVolume`, `MaxVolume`, `bShowBagFullWarning`, `bCanTransferFocusedLootToBag`, trigger card fields, reserve row entries |
| Request-only actions | None at panel level — requests dispatched from child slot entries |
| Visibility rules | Visible only when `bIsBagLayerActive`. Hidden during normal combat. |
| May be created in D4-A | Yes — skeleton/layout shell only |
| May be edited later | Yes |
| Approval status | **Pending Human Approval** |

---

#### Asset: WBP\_BagSlotEntry  
*(D2 equivalent: `WBP_WTBRBagSlot`)*

| Field | Value |
|---|---|
| Proposed path | `Content/UI/HUD/WBP_BagSlotEntry.uasset` ⚠ tentative |
| D2 name | `WBP_WTBRBagSlot` |
| Purpose | Individual inventory slot cell within the 16-cell bag grid. Displays item icon, stack count, locked badge, optional tooltip anchor, and state visuals. |
| Parent / owner | `WBP_BagPanel` → `WBP_BagGrid` |
| Child widgets | None — leaf widget |
| Data source | Per-slot snapshot: `SlotIndex`, `bHasItem`, `ItemDisplayName`, `ItemIcon`, `StackCount`, `Rarity`, `bIsLocked`, `bIsSelected`, `bIsHovered`, `bIsDragTarget`, `bIsCannotTransfer`, `bIsReservedVisualCell` |
| Request-only actions | Request select slot. Request move/swap slot (D4-C or later). Request drop slot (D4-C or later). |
| Visibility rules | Always visible within grid. State visuals (locked, reserved, drag-target, cannot-transfer) driven by read-only snapshot flags. |
| May be created in D4-A | Yes — skeleton/layout shell only. No drag/drop, no action wiring. |
| May be edited later | Yes |
| Approval status | **Pending Human Approval** |

---

#### Asset: WBP\_CorpseLootPanel  
*(D2 equivalent: `WBP_WTBRCorpseLootPanel`)*

| Field | Value |
|---|---|
| Proposed path | `Content/UI/HUD/WBP_CorpseLootPanel.uasset` ⚠ tentative |
| D2 name | `WBP_WTBRCorpseLootPanel` |
| Purpose | Left-adjacent corpse loot context panel. Appears when a loot context (corpse or container) is active and the bag layer is open. Contains header, scrollable loot entry list, and footer hint row. |
| Parent / owner | `WBP_InMatchBagLootLayer` |
| Child widgets | `WBP_CorpseLootEntry` (per list entry) |
| Data source | `bIsCorpseLootVisible`, `CorpseLootTitle`, `CorpseLootItemCount`, `CorpseLootEntries[]`, `CorpseLootFooterHintText` |
| Request-only actions | Request close loot context |
| Visibility rules | Hidden by default. Visible only when `bIsBagLayerActive && bHasLootContext`. Must not occupy full screen. |
| May be created in D4-A | Yes — skeleton/layout shell only |
| May be edited later | Yes |
| Approval status | **Pending Human Approval** |

---

#### Asset: WBP\_CorpseLootEntry  
*(D2 equivalent: `WBP_WTBRCorpseLootEntry`)*

| Field | Value |
|---|---|
| Proposed path | `Content/UI/HUD/WBP_CorpseLootEntry.uasset` ⚠ tentative |
| D2 name | `WBP_WTBRCorpseLootEntry` |
| Purpose | Individual row entry within the Corpse Loot scroll list. Shows item display name, type label, rarity, quantity, icon, and transfer availability state. |
| Parent / owner | `WBP_CorpseLootPanel` |
| Child widgets | None — leaf widget |
| Data source | Per-entry snapshot: `EntryIndex`, `DisplayName`, `EntryTypeLabel`, `Rarity`, `Quantity`, `Icon`, `bIsSelected`, `bIsHovered`, `bCanTransfer` |
| Request-only actions | Request select entry. Request take loot item. Request transfer loot item to bag (D4-C or later). |
| Visibility rules | Visible within loot scroll list. `BlockedTransfer` state shown when `!bCanTransfer`. |
| May be created in D4-A | Yes — skeleton/layout shell only. No transfer wiring. |
| May be edited later | Yes |
| Approval status | **Pending Human Approval** |

---

### 2.3 Additional Proposed Widgets from D2 Hierarchy

The following widgets are named in D2's hierarchy plan. They are proposed future assets but are NOT listed in the 5-asset core set above. Their creation is lower priority and requires separate human approval.

| D2 Widget Name | Role | Status |
|---|---|---|
| `WBP_WTBRInMatchBagLootRoot` | Top-level overlay root; owns bag layer | Pending Human Approval |
| `WBP_WTBRBagHeader` | Bag header row | Pending Human Approval |
| `WBP_WTBRBagCapacityRow` | Volume capacity display row | Pending Human Approval |
| `WBP_WTBRActiveTriggerCard_Main` | Active Main trigger display card | Pending Human Approval |
| `WBP_WTBRActiveTriggerCard_Sub` | Active Sub trigger display card | Pending Human Approval |
| `WBP_WTBRReserveRow_Main` | Main reserve slot row | Pending Human Approval |
| `WBP_WTBRReserveRow_Sub` | Sub reserve slot row | Pending Human Approval |
| `WBP_WTBRReserveSlot` | Individual reserve slot entry | Pending Human Approval |
| `WBP_WTBRBagGrid` | 16-cell UniformGridPanel for inventory | Pending Human Approval |
| `WBP_WTBRBagFooter` | Volume footer with bag-full warning row | Pending Human Approval |

All names above use D2 convention (`WBP_WTBR`). Human must confirm naming and path before any are created.

---

## 3. Explicit Non-Approved Assets

The following assets are **forbidden** unless separately and explicitly approved by the human.

### 3.1 Existing HUD WBP Assets

Any existing WBP/UMG assets in the project that are not listed in Section 2 of this document.

- Specifically: any existing HUD root widget, HUD overlay widgets, or existing WBP files not part of the proposed Bag + Corpse Loot layer.
- These may not be modified, extended, or reparented until separately approved.

### 3.2 Maps

- Any `.umap` file in the project.
- `ThirdPersonMap` and all derived map files.
- The in-progress `ThirdPersonMap` external actor entry visible in git status (`Content/__ExternalActors__/ThirdPerson/Maps/ThirdPersonMap/B/KZ/`) — this is untracked and must not be touched, staged, or committed.

### 3.3 Binary Assets

- Any `.uasset` not listed in Section 2 of this document.
- Any mesh, skeletal mesh, material, material instance, physics asset, animation, or animation Blueprint.
- Any Data Asset, Data Table, or Curve Asset not part of an explicitly approved pass.

### 3.4 Visual / Audio / VFX Assets

- Icons, textures, sprites, render targets, or image assets of any kind.
- Sound, SoundCue, MetaSound, or sound wave assets.
- Niagara system or Niagara emitter assets.
- Any VFX asset.
- Media assets of any kind.

### 3.5 Unrelated UI Assets

- Any UI widget not related to in-match Bag + Corpse Loot context.
- Main menu, lobby, scoreboard, settings, or any other screen-level UI assets.
- Match summary, kill feed, or notification widget assets.

---

## 4. Implementation Slice Recommendation

The following slices describe the recommended order of future implementation after human approval.

### Slice D4-A — Skeleton and Layout Shell Only (Recommended First Slice)

- Create skeleton WBP assets: layout containers, placeholder text, and safe margins only.
- No gameplay mutation of any kind.
- No drag/drop behavior.
- No asset import (no icons, no textures, no sounds).
- No final art or placeholder textures.
- Bind only read-only placeholder fields where safe and explicitly approved.
- Implement visibility shell only: widget visible/hidden toggle based on approved read-only flags.
- Keep all request actions stubbed (empty or routed through existing approved wrappers only).
- Do not modify any gameplay system, C++ class, or server-side logic.
- Do not introduce any Blueprint-side game logic beyond approved read-only display.

### Slice D4-B — Read-Only Binding Integration

- Wire ViewModel / snapshot fields to widget display properties.
- Volume, slot count, entry count, header labels.
- Rarity and state display driven by read-only snapshot flags.
- No request wiring yet.

### Slice D4-C — Request-Only Button/Action Wiring

- Wire approved request actions only: select, pickup request, cancel, close loot context.
- All requests routed through existing approved wrappers (see Section 6).
- No client-side inventory mutation.

### Slice D4-D — Drag/Drop Visual States Only

- Show drag target, drag source, cannot-transfer visual states.
- UI may highlight potential targets locally for UX feedback.
- No authoritative item movement client-side.
- Server result remains the only source of truth.

### Slice D4-E — Server-Authoritative Transfer Integration

- Full request/response cycle for loot transfer, bag drop, slot move/swap.
- UI dispatches request, waits for replicated state refresh.
- Optimistic animation (if desired) must reconcile to server-approved replicated state.

### Slice D4-F — Visual Polish

- Final art, icons, color tuning, animation polish.
- Only after human playtest approval of D4-E behavior.

---

## 5. Data Binding Approval

Future widgets may read only from:

- Existing HUD ViewModel / read-only snapshot data structures.
- Replicated/read-only state exposed through the approved ViewModel component.
- Approved C++ UI contract fields as defined in the existing HUD contract and D2.

### Approved Read-Only Binding Sources

Bag layer scope:

- `bIsBagVisible`, `bIsBagLayerActive`, `bHasLootContext`
- `CurrentSlotCount`, `MaxSlotCount`, `CurrentVolume`, `MaxVolume`
- `bShowBagFullWarning`, `bCanTransferFocusedLootToBag`
- Per-slot snapshot array: `InventorySlots[]` (all fields from D2 section: Bag Grid Fields)
- Trigger card fields: `MainTriggerName`, `MainTriggerIcon`, `MainTriggerSlotLabel`, `SubTriggerName`, `SubTriggerIcon`, `SubTriggerSlotLabel`
- Reserve row entries: `MainReserveEntries[]`, `SubReserveEntries[]`

Corpse loot scope:

- `bIsCorpseLootVisible`, `CorpseLootTitle`, `CorpseLootItemCount`, `CorpseLootFooterHintText`
- Per-entry snapshot array: `CorpseLootEntries[]` (all fields from D2 section: Corpse Loot Panel Fields)

### Forbidden Direct Mutations from UI

UI must not directly mutate any of the following:

- Inventory slots or slot assignments
- Bag slot data or slot entry counts
- Loot entries or loot container contents
- Corpse container state
- Ground items or dropped triggers
- Vael (Trion energy resource)
- HP or damage state
- Cooldown timers or cooldown state
- Trigger state or trigger slot assignments
- Any gameplay state or authoritative actor state

---

## 6. Request-Only Approval

Future UI actions must route only through approved request-only wrappers. The UI dispatches a request. The server validates the request and mutates authoritative state only on success. The UI then refreshes from replicated/read-only state.

### Approved Request Actions (Future D4-C+)

| Request | Description |
|---|---|
| Request select bag slot | Select a slot for focus; no inventory mutation |
| Request select corpse loot entry | Select a loot entry for focus; no loot mutation |
| Request take loot item | Dispatch request to take focused loot entry; server validates |
| Request transfer loot item to bag | Dispatch request to transfer loot to proposed bag slot; server validates and may reject or reroute |
| Request drop full slot | Dispatch request to drop bag slot content; server validates |
| Request move / swap slot | Dispatch request to move or swap between two bag slots; server validates |
| Request use item | Dispatch request to use an item from bag; server validates |
| Request close loot context | Dispatch request to close the active loot/bag overlay |

### Existing Approved Request Paths (Must Not Be Bypassed)

- `UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()`
- `UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()`
- `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(...)`
- Existing interaction and context pickup request chains

D3 does not approve modifications to these paths. Future UMG must use them as-is or request a separate approval pass before any change.

### Golden Rule

> **Client UI never decides success. Server validates and mutates. UI refreshes from authoritative replicated / read-only state.**

---

## 7. Input / Key Label Rule

### No Hardcoded Physical Keys

Future WBP/UMG widgets must not hardcode any physical key name or glyph in their designer text, bindings, or Blueprint graph.

Physical key strings such as `F`, `X`, `Q`, `E`, `LMB`, `RMB`, `4` may appear only as mockup examples in documentation. They must not appear as final UI implementation strings.

### Resolution Rule

All visible input labels and glyphs must resolve from Input Mapping / Enhanced Input at runtime:

- Use the active mapping context and input action to resolve the display label.
- Use the existing resolution strategy from the HUD contract work:  
  `UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(...)`
- If an action is unbound, show a semantic fallback label or a hidden binding state. Do not guess or hardcode a physical key.

### Remappability Requirement

Future implementation must support:

- Remappable keyboard and mouse bindings.
- Future gamepad display names.
- Platform-agnostic display label strategy.

A widget that displays a hardcoded `"F"` key label will fail this rule even if `F` happens to be the current default. The label must be resolved dynamically.

---

## 8. Human Approval Checklist

The following checklist must be explicitly approved by the human before D4 implementation begins. No WBP/UMG work may start until every applicable item is checked.

```markdown
# Human Approval Checklist

Before D4 implementation starts, human must explicitly approve:

[ ] Exact WBP assets allowed to create (from Section 2 of this document)
[ ] Exact existing WBP assets allowed to edit (none currently — confirm)
[ ] Exact folder/path convention for new UI assets (Content/UI/HUD/ is tentative — confirm)
[ ] Widget naming prefix: WBP_WTBR* (D2 convention) vs WBP_* (simplified) — confirm one
[ ] Whether WBP_WTBRInMatchBagLootRoot is the top-level owner or if bag layer plugs into an existing HUD root
[ ] Whether placeholder icons are allowed in D4-A (skeleton pass)
[ ] Whether any new textures/icons may be imported in any D4 slice
[ ] Whether drag/drop behavior may be implemented in WBP (D4-D or later)
[ ] Whether Blueprint graph logic is allowed or C++ only for binding and request dispatch
[ ] Whether existing HUD root/layer assets may be modified (currently: forbidden)
[ ] Whether any .uasset files may be staged
[ ] Whether any .umap files may be touched
[ ] Whether any binary assets may be touched
[ ] Whether D4-A skeleton-only implementation is approved to begin
[ ] Confirm authoritative request path for loot transfer (use existing or new RPC)
[ ] Confirm drag/drop is mouse-first, gamepad-first, or both
[ ] Confirm input-display presentation style for unbound actions
[ ] Confirm no Web Browser Widget is used for production runtime UI
[ ] Confirm no new gameplay mutation logic in any UI Blueprint
```

---

## 9. D3 Acceptance Criteria

```markdown
# D3 Acceptance Criteria

[ ] Docs-only file created at Docs/WTBR_D3_UMG_BAG_LOOT_ASSET_APPROVAL_GATE.md
[ ] Proposed future WBP/UMG asset list documented (Section 2)
[ ] Asset purpose and ownership documented for each proposed widget
[ ] Path convention note and naming discrepancy (WBP_WTBR vs WBP_) documented as tentative
[ ] Additional D2 hierarchy widgets listed as lower-priority proposed assets
[ ] Explicit non-approved asset list documented (Section 3)
[ ] Safe first implementation slice (D4-A) documented (Section 4)
[ ] Future slice progression D4-A through D4-F documented
[ ] Read-only binding rule documented (Section 5)
[ ] Forbidden mutation list documented (Section 5)
[ ] Request-only action rule documented (Section 6)
[ ] Existing approved request paths documented (Section 6)
[ ] Input/remappable key rule documented (Section 7)
[ ] Resolution function reference documented (Section 7)
[ ] Human approval checklist documented (Section 8)
[ ] No Source/C++ changes in this pass
[ ] No WBP/UMG changes in this pass
[ ] No .uasset, .umap, or binary asset changes in this pass
[ ] No files staged or committed before human review
```

---

## 10. Non-Goals for D3

- No WBP/UMG asset creation.
- No Blueprint implementation.
- No C++ implementation or modification.
- No server RPC changes.
- No test changes.
- No staging, commit, or push.
- No import of icons, textures, sounds, VFX, or media assets.
- No modification of any `.umap` or binary asset.
- No locking of final widget names or paths — all names remain tentative until human approval.

---

## 11. Git Safety Checklist

- Review `git status` before any future action.
- Review recent commits before any future action.
- Edit only approved files for the pass.
- Do not use `git add .`
- Do not use `git add ..`
- Do not use `git add -A`
- Stage only reviewed files in a later approved staging pass.
- Do not stage logs, local settings, temp artifacts, or unrelated assets.
- Do not commit until human review approves this document.
