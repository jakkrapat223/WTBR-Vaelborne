# WBP_BagPanel / WBP_CorpseLootPanel / WBP_BagLootLayer — Phase 3D / 4A-BagLoot Binding Audit + Plan

Project: WTBR / Vaelborne: Dominion
Engine: Unreal Engine 5.1.1 C++
Pass: Docs/planning only. No C++, no WBP, no compile, no staging/commit/push.

## Status

- Docs-only audit + plan.
- No C++ classes added or edited in this pass.
- No WBP/UMG/Blueprint/.uasset/.umap/binary assets touched.
- No Unreal compile performed.
- Human is unavailable for WBP parent-class assignment right now — this pass does not
  attempt or wait on that step (see `WBP_HUD_BindingPlan.md` for the still-pending HUD
  parent-class assignment, which this pass does not touch).

---

## Current Generated JSON Status

| File | widgetName | Widgets | Last validation |
|---|---|---|---|
| `WBP_BagPanel_Generated.json` | `WBP_BagPanel_Generated` | 62 | Warnings 0, Skipped 0, Unsupported 0 |
| `WBP_CorpseLootPanel_Generated.json` | `WBP_CorpseLootPanel_Generated` | 44 | Warnings 0, Skipped 0, Unsupported 0 |
| `WBP_BagLootLayer_Generated.json` | `WBP_BagLootLayer_Generated` | 107 | Warnings 0, Skipped 0, Unsupported 0 |

All three are accepted as usable visual mockups; binding is intentionally deferred
(see `Plugins/UMGJsonGenerator/README.md` Phase 3B/3C section).

---

## 1. Runtime Data Source Audit

Based on direct inspection of current C++ source (not guessed).

### Player inventory / bag slots

- **`UWTBRInventoryComponent`** (`Source/WTBR/Inventory/WTBRInventoryComponent.h/.cpp`)
  - `GetInventorySlots() : const TArray<FWTBRInventorySlot>&` — replicated
    (`ReplicatedUsing=OnRep_InventorySlots`), read-only accessor. **Ready.**
  - `InventorySlotCount` (int32, `EditDefaultsOnly`, default 14) — this is the
    **max slot count**, i.e. `Txt_BagCapacity`'s "max" half (`14` in `"12 / 14"`).
    The "current" half (`12`) is simply `GetInventorySlots().Num()` filtered by
    `!IsEmpty()`, or just `InventorySlots.Num()` if the array is always sized to
    `InventorySlotCount` (confirm via `EnsureSlotsSized()` before relying on this).
    **Ready**, needs a small counting helper (count non-empty slots), not a new
    data source.
  - `OnInventoryChanged` (dynamic multicast delegate) — fires after server mutation
    and on client replication. **Ready** for refresh-on-change wiring, same pattern
    already used by `UWTBRHUDViewModelComponent`.
  - `CanAddItem(ItemData, Quantity)`, `TryAddItem(...)`, `ConsumeItemAtSlot(...)` —
    server-authoritative mutation methods. **UI must never call these directly.**

- **`FWTBRInventorySlot`** (`Source/WTBR/Inventory/WTBRInventorySlot.h`)
  - `ItemData : TSoftObjectPtr<UWTBRItemDataAsset>` — item identity. **Ready**
    (same pattern as `UWTBRHUDViewModelComponent::BuildQuickItemSnapshot()`, which
    already does a non-blocking `.Get()` for display and a `.LoadSynchronous()`
    only for the actual use-item request).
  - `Quantity : int32` — stack count. **Ready.**
  - `IsEmpty()` / `IsValid()` — helper predicates. **Ready.**

### Item display name / icon / type

- **`UWTBRItemDataAsset`** (`Source/WTBR/Inventory/WTBRItemDataAsset.h`)
  - `DisplayName` (FText), `FunctionalName` (FText), `FunctionalDescription` (FText),
    `HUDIcon` (`TSoftObjectPtr<UTexture2D>`), `ItemType` (`EWTBRItemType`:
    None/Consumable/VaelItem/Tuning/Option/UpgradeMaterial), `MaxStackSize`,
    `EffectMagnitude`, `UseTimeSeconds`. **Ready** for `Txt_BagSlotNNName` /
    `Img_BagSlotNNIcon`.
  - **No `Rarity` field exists anywhere on this DataAsset** (confirmed by reading the
    full file). The Common/Uncommon/Rare/Epic/Legendary rarity system shown in the
    `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.html` mockup **has no backing data
    source** — it was a visual-reference-only concept, not implemented gameplay data.
    This mockup pass correctly did not add rarity text/color to the JSON.

### Max bag capacity / bag weight

- **Max slot count**: `UWTBRInventoryComponent::InventorySlotCount` — **Ready** (see above).
- **Bag weight/volume**: **UNKNOWN / NOT BUILT.** A repo-wide search of
  `Source/WTBR/Inventory/*.h` and `*.cpp` for `Weight` or `Volume` returns zero
  matches. There is no weight/volume concept anywhere in the inventory system.
  `Txt_BagWeight` (sample text `"48 / 60"`) and the `--fill` volume bar in the HTML
  mockup are **visual-reference-only** with no backing gameplay data. If a real
  weight/volume system is wanted, it needs new C++ design work (a new replicated
  field + accumulation logic) — out of scope for a thin binding pass.

### Quick item slot

- Already fully covered by the existing HUD ViewModel from Phase 4B:
  `UWTBRHUDViewModelComponent::BuildQuickItemSnapshot()` /
  `FWTBRHUDQuickItemSnapshot` (`bHasItem`, `ItemName`, `Icon`, `Count`, `State`,
  `Binding`) and `ResolveQuickItemSlotIndex()`. **Ready** — the Bag/Loot pass should
  reuse this, not rebuild it.

### Ground item pickup

- **`AWTBRGroundItemActor`** (`Source/WTBR/Inventory/WTBRGroundItemActor.h/.cpp`)
  - `GetItemData() : TSoftObjectPtr<UWTBRItemDataAsset>`, `GetQuantity() : int32`,
    `IsConsumed() : bool` — all replicated fields with read-only getters. **Ready.**
  - `GetInteractionPromptText() : FText` — designer-set per-instance/DA prompt.
    **Ready** but not directly used by the Bag/Loot panels themselves (this is the
    focus-prompt shown before pickup, analogous to `Txt_PickupPrompt` on the main
    HUD, not a Bag/Loot panel widget).
  - `TryMarkConsumed()` / `ClearConsumedForFailedPickup()` — server-authoritative
    claim/rollback. **UI must never call these directly.**

### Corpse loot container / Trigger Cache

- **`AWTBRCorpseLootContainerActor`** (`Source/WTBR/Interaction/WTBRCorpseLootContainerActor.h/.cpp`)
  - `LootEntries : TArray<FWTBRCorpseLootEntry>` — **replicated**
    (`ReplicatedUsing=OnRep_LootEntries`).
  - **`FWTBRCorpseLootEntry`** fields: `TriggerDataAsset` (soft ptr),
    `SourceSlotIndex` (int32), `CachedCategory` (`ETriggerCategory`), `bConsumed` (bool).
  - `OnCorpseLootEntriesChanged` (dynamic multicast delegate) — fires on entry
    change. **Ready** for refresh-on-change wiring.
  - `GetLootEntriesForUIReadOnly(TArray<FWTBRCorpseLootEntry>& Out)` — bulk
    read-only snapshot. **Ready.**
  - `BuildLootEntryViewModel(LootEntryIndex) : FWTBRCorpseLootEntryViewModel` —
    per-entry view (`StableEntryIndex`, `TriggerDataAsset`, `CachedCategory`,
    `SourceSlotIndex`, `bIsAvailable`). **Ready** and preferred over raw
    `LootEntries` access since it already derives `bIsAvailable` correctly.
  - `GetInteractionPromptText() : FText` — returns
    `NSLOCTEXT("WTBR", "CorpseLootContainerPrompt_OpenTriggerCache", "Open Trigger Cache")`
    (confirmed exact string in source). This is the **exact real string** already
    used for `Txt_CorpseLootPrompt` in the Phase 3C mockup — good sign the mockup's
    wording matches production intent.
  - `HasAvailableLootEntries()`, `AreAllEntriesConsumed()`,
    `CanBeInteractedWithBy(Actor)` — read-only query helpers. **Ready.**
  - `BuildTargetSlotOptions(...)` / `BuildTargetSlotOptionsForEntry(...)` →
    `FWTBRTargetSlotOptionViewModel` (`SlotIndex`, `SlotLabel` e.g. `"Main 1"`–`"Sub 4"`,
    `bIsEmpty`, `EquippedDataAsset`, `bIsCompatible`, `IncompatibleReason` e.g.
    `"Main slot only"`) — relevant for a future target-slot picker UI, not needed
    for the current 8-row loot list mockup. **Ready but out of current scope.**

**Important gap — the "Type" column has no single clean source.** The JSON mockup's
sample `Txt_LootEntryNNType` text (`"Main Slot"` / `"Sub Slot"`) does not map cleanly
to any one existing field:
  - `FWTBRCorpseLootEntry.CachedCategory` (`ETriggerCategory`: Melee/Gunner/
    SniperBullet/Defense/Movement/Trap/BlackTrigger) is available **without loading
    the DataAsset**, but does not produce "Main Slot"/"Sub Slot" — it produces a
    weapon-style category instead.
  - The DataAsset's `SlotConstraint` (`ETriggerSlotConstraint`: MainOnly="Main Slot
    Only"/SubOnly="Sub Slot Only"/Any="Any Slot") **would** produce text matching the
    mockup's intent, but requires loading `TriggerDataAsset` (a `TSoftObjectPtr`),
    which is a resident-only `.Get()` (non-blocking) or `.LoadSynchronous()`
    (blocking) call — same tradeoff already handled for Quick Item.
  - **Recommendation**: bind `Txt_LootEntryNNType` from the loaded DataAsset's
    `SlotConstraint` display text (matches the mockup and the codebase's own
    `"Main slot only"` / `"Sub slot only"` wording used elsewhere), falling back to
    `CachedCategory`'s enum display name if the DataAsset isn't resident yet — mirror
    the non-blocking-display / blocking-on-request-only pattern already established.

### Corpse box / death drop / dropped trigger (context)

- `AWTBRDroppedTriggerActor` (`Source/WTBR/Interaction/WTBRDroppedTriggerActor.h/.cpp`)
  exists as the **legacy** drop path (CVar-gated, still preserved per project rules)
  and is priority-2 in the context-interact dispatch order (corpse/container →
  dropped trigger → BR ground item → generic interactable). It is not directly
  relevant to the Bag/Corpse-Loot **panel** widgets in this JSON (those model the
  corpse **container** path, priority-1), but is the reason
  `UWTBRInteractionComponent::RequestContextInteract()` exists as a shared dispatcher.
  Not a data source for any bound widget in this pass.

### Interaction / prompt

- **`UWTBRInteractionComponent`** (`Source/WTBR/Components/WTBRInteractionComponent.h`)
  - `GetFocusedCorpseLootContainer() : AWTBRCorpseLootContainerActor*` — read-only
    focus trace result. **Ready.**
  - `GetFocusedInteractionPromptText() : FText`, `HasValidFocusedInteractable() : bool`
    — already wired into the main HUD's `FWTBRHUDPickupPromptSnapshot`. **Ready**,
    reusable for a pre-open "you are looking at a Trigger Cache" state, distinct from
    the panel's own internal prompt once opened.
  - `OnCorpseLootInteractRequested` (delegate) + `RequestCorpseLootInteract()` +
    `RequestContextInteract()` — client-local request dispatch, no mutation.
    **Ready.**

### Request functions (existing vs. not yet implemented)

Existing, confirmed in `Source/WTBR/WTBRCharacter.h`:

- `Server_RequestUseInventoryItem(int32 SlotIndex)` — **exists.**
- `Server_RequestPickupGroundItem(AWTBRGroundItemActor* GroundItem)` — **exists.**
- `Server_RequestPickupCorpseLootEntry(AWTBRCorpseLootContainerActor* LootContainer, int32 LootEntryIndex, int32 TargetSlotIndex)` — **exists.**
- `Server_RequestPickupDroppedTrigger(AWTBRDroppedTriggerActor* DroppedTrigger, int32 TargetSlotIndex)` — **exists.**

Also existing on `UWTBRHUDViewModelComponent`:

- `RequestUseDisplayedQuickItem()`, `RequestPickupFocusedTarget()`,
  `RequestCancelCurrentUIOrAction()` — **exist**, already used by the HUD.

**Confirmed NOT YET implemented** (grepped `WTBRCharacter.h` for `RequestDrop`,
`RequestMove`, `RequestEquip` — zero matches beyond the existing pickup/use RPCs
above):

- `RequestDropInventorySlot(SlotIndex)` — provisional name only, per
  `Docs/WTBR_UI_CONTRACT_PLAN.md` / `Docs/WTBR_UI_INVENTORY_LOOT_SPEC.md`.
- `RequestMoveOrSwapInventorySlot(FromSlotIndex, ToSlotIndex)` — provisional name only.
- `RequestEquipInventoryTrigger(InventorySlotIndex, TargetTriggerSlotIndex)` — provisional name only.
- A generic "take all loot" request — **not documented anywhere**, not even as a
  provisional name. Would need explicit human design approval before naming/adding.

### Key labels / input mapping

- `UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(MappingContext, InputAction)`
  → `FWTBRHUDBindingDisplay` (`bIsBound`, `DisplayName`, `GlyphToken`) — **exists**,
  already used by the HUD ViewModel for Cancel/QuickItem/Main/Sub bindings. **Ready**
  to reuse for any future Bag-open/close or Loot-close binding display, once a
  human approves which input action represents those.

---

## 2. Binding Candidates By File

### A) `WBP_BagPanel_Generated.json`

**Text widgets:**
`Txt_BagTitle`, `Txt_BagCapacity`, `Txt_BagWeight`, `Txt_BagHint`, `Txt_BagWarning`,
`Txt_BagSlot01Name`…`Txt_BagSlot14Name` (14), `Txt_BagSlot01Count`…`Txt_BagSlot14Count` (14)
— 33 text widgets total.

**Image widgets:**
`Img_BagSlot01Icon`…`Img_BagSlot14Icon` (14 total).

**Progress/state widgets:** None in this file (no ProgressBar was used for Bag).

**Decorative (not intended for binding):**
`Border_BagPanel` (panel background), `Border_BagSlot01`…`Border_BagSlot14` (14 slot
backgrounds) — these could later gain a *state* concept (filled/empty/locked/hover/
selected/drag-target per `Docs/WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md`), but that is
explicitly out of the minimal first-pass scope below.

### B) `WBP_CorpseLootPanel_Generated.json`

**Text widgets:**
`Txt_CorpseLootTitle`, `Txt_CorpseLootSubtitle`, `Txt_CorpseLootPrompt`,
`Txt_LootEntry01Name`…`Txt_LootEntry08Name` (8), `Txt_LootEntry01Type`…`Txt_LootEntry08Type` (8),
`Txt_LootEntry01Action`…`Txt_LootEntry08Action` (8) — 27 text widgets total.

**Image widgets:**
`Img_LootEntry01Icon`…`Img_LootEntry08Icon` (8 total).

**Progress/state widgets:** None in this file.

**Decorative (not intended for binding):**
`Border_CorpseLootPanel` (panel background), `Border_LootEntry01`…`Border_LootEntry08`
(8 row backgrounds) — same future-state-concept note as Bag slots (normal/hover/
selected/blocked-transfer per D2 plan), out of first-pass scope.

### C) `WBP_BagLootLayer_Generated.json`

Union of A + B's widgets (same names, since it's a literal duplication per the
Phase 3C note that nested WBP references aren't supported), plus `Txt_ContextHint`
(decorative/informational text, not tied to any specific data source yet — a
generic placeholder for whichever prompt is most relevant when both panels are
visible together; likely candidate: the same corpse-loot-container
`GetInteractionPromptText()` used for `Txt_CorpseLootPrompt`, but not decided here).

Total binding candidates in file C: 33 (Bag text) + 14 (Bag icons) + 27 (Loot text) +
8 (Loot icons) + 1 (`Txt_ContextHint`, undecided source) = 83.

---

## 3. Minimal First Binding Scope

**Bag first pass:**

```
Txt_BagCapacity
Txt_BagWeight
Txt_BagWarning
Txt_BagSlot01Name .. Txt_BagSlot14Name
Txt_BagSlot01Count .. Txt_BagSlot14Count
Img_BagSlot01Icon .. Img_BagSlot14Icon
```

**Corpse loot first pass:**

```
Txt_CorpseLootTitle
Txt_CorpseLootSubtitle
Txt_CorpseLootPrompt
Txt_LootEntry01Name .. Txt_LootEntry08Name
Txt_LootEntry01Type .. Txt_LootEntry08Type
Txt_LootEntry01Action .. Txt_LootEntry08Action
Img_LootEntry01Icon .. Img_LootEntry08Icon
```

Note on `Txt_BagWeight`: included in the requested first-pass scope, but per §1 there
is **no backing data source at all**. First-pass binding for this widget should
either (a) show a fixed/hidden placeholder until a weight system is designed, or
(b) be explicitly descoped from the first pass with human sign-off. This plan does
not decide that trade-off — it only flags it clearly (same treatment as the HUD's
zone/phase/cooldown placeholders in `WBP_HUD_BindingPlan.md`).

Note on `Txt_BagTitle`, `Txt_BagHint`, `Txt_LootEntry0XAction`: intentionally **not**
in the minimal scope (title/hint are static, and Action text's behavior depends on
which request-only function gets wired — see §4 — which is a bigger decision than a
first pass should make).

---

## 4. Request-Only UI Actions (future, not implemented here)

| Function | Status | Notes |
|---|---|---|
| `RequestUseDisplayedQuickItem()` | **Exists** on `UWTBRHUDViewModelComponent` | Reuse as-is. |
| `RequestUseBagItem(SlotIndex)` | Not yet named/built | Should route to the **existing** `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)` — this is a thin wrapper, not new gameplay logic. |
| `RequestDropBagItem(SlotIndex)` | Not yet built (matches doc's `RequestDropInventorySlot`) | No existing server RPC to route to — needs new C++ (server validates slot, removes item, spawns ground item). Out of scope for this pass. |
| `RequestMoveBagItem(FromSlot, ToSlot)` | Not yet built (matches doc's `RequestMoveOrSwapInventorySlot`) | No existing server RPC. Needs new C++. Out of scope. |
| `RequestEquipTriggerFromBag(SlotIndex, TargetTriggerSlot)` | Not yet built (matches doc's `RequestEquipInventoryTrigger`) | No existing server RPC. Needs new C++. Out of scope. |
| `RequestTakeLootItem(LootIndex)` | Not yet named/built | Should route to the **existing** `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(LootContainer, LootEntryIndex, TargetSlotIndex)` — needs a `TargetSlotIndex` resolution strategy (auto-pick first compatible empty slot via `BuildTargetSlotOptionsForEntry`, or prompt the player — human decision). |
| `RequestTakeAllLoot()` | **Not documented anywhere**, no existing RPC | Needs explicit human design approval (server-side "take all" semantics, partial-failure behavior) before naming or building. |
| `RequestCloseLootPanel()` | Not yet built | Presentation-only (no server RPC needed) unless closing must also send a "stop interacting" signal — likely a pure client-local UI-state toggle, same category as the Cancel prompt's local-only close behavior. |

**Hard rule (unchanged from `WBP_HUD_BindingPlan.md` and both UI contract docs):** the
UI must never directly mutate inventory, loot container entries, corpse box/dropped
trigger actors, ground item actors, Vael, HP, cooldown, or trigger slot state. All of
the above are **request dispatchers only** — they hand off to server RPCs and wait
for replicated state / view-model refresh to reflect the result.

---

## 5. Missing / Unclear Data Source Gaps (explicit)

| Question | Answer |
|---|---|
| Does bag capacity (max slot count) exist? | **Yes** — `UWTBRInventoryComponent::InventorySlotCount`. |
| Does a "current slot count" exist as a direct field? | **No direct field**, but trivially derivable by counting non-empty entries in `GetInventorySlots()`. |
| Does bag weight/volume exist? | **No.** Confirmed via full-file grep of the Inventory module — zero matches for `Weight`/`Volume`. Not built. |
| Does item icon exist? | **Yes** — `UWTBRItemDataAsset::HUDIcon` (items) and `UWTBRTriggerDataAsset::HUDIcon` (triggers), same field name on both DataAsset types. |
| Does a corpse loot container snapshot exist? | **Yes** — `AWTBRCorpseLootContainerActor::LootEntries` (replicated) + `GetLootEntriesForUIReadOnly()` + `BuildLootEntryViewModel()`. No dedicated standalone "ViewModel component" wraps it yet (unlike the HUD's `UWTBRHUDViewModelComponent`) — the read-only helpers live directly on the container actor. |
| Are corpse loot item rows replicated/read-only? | **Yes**, replicated via `OnRep_LootEntries`, and every UI-facing accessor is explicitly read-only per its own doc comments ("None of these mutate container or trigger-set state"). |
| Do request functions already exist? | **Partially.** Use/pickup-corpse-loot/pickup-ground-item/pickup-dropped-trigger RPCs exist. Drop/Move/Equip-from-bag RPCs do **not** exist yet (provisional names only in docs). A generic "take all loot" concept does not exist anywhere, even as a provisional name. |
| Do key labels come from Input Mapping? | **Yes, the resolution mechanism exists** (`UWTBRInputBindingDisplayLibrary`), but no specific Bag-open/Bag-close/Loot-close input action has been identified or approved yet — that decision is still pending human input-design work, same as noted in the D2 conversion plan's "Human Approval Checklist." |
| Does a Bag/Loot-specific ViewModel component exist (like the HUD's)? | **No.** See Option C below. |

---

## 6. Recommended Implementation Path

| Option | Description | Trade-off |
|---|---|---|
| **A — Manual Blueprint Graph binding** | Human opens each WBP and writes Blueprint graph logic calling `UWTBRInventoryComponent::GetInventorySlots()` / `AWTBRCorpseLootContainerActor::GetLootEntriesForUIReadOnly()` directly. | Fastest first look; but the "Type" column ambiguity (§1) and quick-item-style non-blocking asset loading pattern get re-solved ad hoc in Blueprint instead of once in C++ — fragile and hard to keep consistent with the HUD's existing pattern. |
| **B — C++ `UUserWidget` parent class for Bag/Loot** | Same shape as `UWTBRGeneratedHUDWidget` from Phase 4B: `BindWidgetOptional` properties matching the JSON names, refreshed from a snapshot. | Only clean if a snapshot struct already exists to refresh from — it partially does (container/inventory read-only accessors exist) but there is **no single assembled snapshot** analogous to `FWTBRHUDSnapshot` yet; a widget class would end up calling 3-4 different components/actors directly, which is workable but duplicates the "resolve non-blocking icon/name, handle DataAsset-not-resident" logic already written once for Quick Item. |
| **C — Add a Bag/Loot ViewModel component** | A new `UWTBRBagLootViewModelComponent` (or extend `UWTBRHUDViewModelComponent`) that assembles a `FWTBRBagSnapshot` (per-slot name/count/icon + capacity) and `FWTBRCorpseLootSnapshot` (title/subtitle/prompt + per-entry name/type/action-eligibility) from the existing read-only accessors in §1, mirroring `BuildQuickItemSnapshot()`'s non-blocking-load pattern. | More upfront C++ work, but centralizes the "Type" ambiguity resolution and asset-loading tradeoffs in exactly one place, reusable by both a future Blueprint binding AND a future native widget — same reasoning that made Option B the right call for the HUD once its ViewModel already existed. |

**Recommendation for Phase 4B-BagLoot: Option C first, then Option B.**

Unlike the HUD (where `UWTBRHUDViewModelComponent` + `FWTBRHUDSnapshot` already
existed before Phase 4B started, making Option B a natural fit), Bag/Corpse Loot has
**no assembled snapshot yet** — only scattered read-only accessors across
`UWTBRInventoryComponent`, `AWTBRCorpseLootContainerActor`, and `UWTBRItemDataAsset`/
`UWTBRTriggerDataAsset`. Building a small Bag/Loot ViewModel component first (Option C)
that assembles those into two snapshot structs is the same shape of work that already
proved out for the HUD, and it resolves the "Type" column and asset-loading questions
in one reviewable place instead of scattering them across Blueprint graphs or a widget
class's `ApplySnapshot()`. Once that snapshot exists, a thin `UUserWidget` parent class
(Option B) can consume it exactly like `UWTBRGeneratedHUDWidget` does today.

---

## 7. Manual Unreal Import/Review Steps

1. Open the project in Unreal Editor with the plugin enabled.
2. **Tools → Import UMG JSON...** and import, in this order:
   - `WBP_BagPanel_Generated.json`
   - `WBP_CorpseLootPanel_Generated.json`
   - `WBP_BagLootLayer_Generated.json`
3. If prompted for an existing asset, use **Yes / Overwrite** only for these
   generated assets under `/Game/UI/Generated/` — never overwrite anything outside
   that folder.
4. Confirm each generated WBP opens in UMG Designer with 0 warnings / 0 skipped in
   the import summary and that all widget names from §2 exist.
5. **Do not use "Save All."**
6. **Do not save the map or any external actors.**
7. Save only an approved generated WBP asset if it was manually reviewed and changed
   — and only after human review, same rule as `WBP_HUD_BindingPlan.md`.

---

## 8. Test Checklist

- [ ] Bag panel opens/closes (presentation-only local UI state; confirm no request
      is sent merely by opening/closing).
- [ ] `Txt_BagCapacity` updates when slot count changes (derived count, see §1).
- [ ] `Txt_BagWeight` — confirm it shows its placeholder/hidden state cleanly rather
      than erroring, since no data source exists yet (§5).
- [ ] Slot names/counts (`Txt_BagSlotNNName`/`Count`) update as items are added/removed.
- [ ] Empty slots clear their name/count text and icon rather than showing stale data.
- [ ] `Txt_CorpseLootTitle`/`Txt_CorpseLootSubtitle` update when a different corpse
      container is focused/opened.
- [ ] Loot entries (`Txt_LootEntryNNName`/`Type`/`Action`, `Img_LootEntryNNIcon`)
      update to match `LootEntries` / `BuildLootEntryViewModel()` output, including
      the "Type" ambiguity resolution chosen in a future pass (§1).
- [ ] Any request/action control does not mutate inventory, loot entries, ground
      items, or trigger slots client-side — confirm by inspecting the call path,
      not just the visual result.
- [ ] Server validates the loot transfer (`Server_RequestPickupCorpseLootEntry`)
      and rejects invalid slot/incompatible/already-consumed cases without any
      client-side pre-mutation.
- [ ] Corpse box/container updates or disappears appropriately after all entries are
      consumed (`AreAllEntriesConsumed()`), confirmed via replicated state, not a
      local guess.
- [ ] Quick item display (already-existing HUD ViewModel) remains consistent with
      Bag contents after a Bag-panel-driven use/pickup (both read the same
      `UWTBRInventoryComponent`, so a refresh of one should not desync from the other).

---

## Phase 4B-BagLoot — ViewModel Snapshot Foundation

Status: **Implemented.** A read-only Bag/Loot ViewModel component now exists,
mirroring `UWTBRHUDViewModelComponent`'s pattern, and builds cleanly (0 errors,
0 warnings, Development Editor Win64). No WBP was touched; no `.uasset`/`.umap`
edits; nothing staged/committed/pushed.

### Files created

- `Source/WTBR/UI/WTBRBagLootViewTypes.h` — snapshot structs:
  `FWTBRBagSlotSnapshot`, `FWTBRBagPanelSnapshot`, `FWTBRLootEntrySnapshot`,
  `FWTBRCorpseLootPanelSnapshot`, `FWTBRBagLootSnapshot` (all `BlueprintType`,
  matching the style of `UI/WTBRHUDViewTypes.h`).
- `Source/WTBR/Components/WTBRBagLootViewModelComponent.h` / `.cpp` —
  `UWTBRBagLootViewModelComponent : public UActorComponent`.

### Snapshot structs — data sources used

- `FWTBRBagSlotSnapshot` ← `FWTBRInventorySlot` (`ItemData`, `Quantity`,
  `IsEmpty()`) + `UWTBRItemDataAsset` (`DisplayName`, `HUDIcon`, `ItemType` →
  enum display text via `StaticEnum<EWTBRItemType>()`). Non-blocking `.Get()`
  only — never forces a synchronous load during display refresh (same rule as
  `UWTBRHUDViewModelComponent::BuildQuickItemSnapshot()`).
- `FWTBRBagPanelSnapshot` ← `UWTBRInventoryComponent::GetInventorySlots()` /
  `InventorySlotCount`. `CurrentSlotsUsed` is derived by counting non-empty slots
  (no direct field for this exists, as noted in the original audit).
  `WeightText` is **always** `FormatUnknownWeightText()` → `"-"` — no
  weight/volume system exists anywhere in the module (re-confirmed by full-module
  grep before writing this code; not fabricated).
- `FWTBRLootEntrySnapshot` ← `AWTBRCorpseLootContainerActor::BuildLootEntryViewModel()`
  (`FWTBRCorpseLootEntryViewModel`: `StableEntryIndex`, `TriggerDataAsset`,
  `CachedCategory`, `bIsAvailable`) + the resolved `UWTBRTriggerDataAsset`
  (`DisplayName`, `HUDIcon`, `SlotConstraint`) when resident. **Type column
  resolved**: uses `SlotConstraint`'s enum display text (e.g. "Main Slot Only")
  when the DataAsset is loaded, falling back to `CachedCategory`'s enum display
  text (e.g. "Melee") when it is not — implements the tradeoff documented in §1
  of this plan rather than leaving it unresolved.
- `FWTBRCorpseLootPanelSnapshot` ← `AWTBRCorpseLootContainerActor::GetInteractionPromptText()`
  (confirmed exact string `"Open Trigger Cache"`) for `PromptText`; `TitleText`
  is a fixed `"TRIGGER CACHE"` string reusing the container's own established
  terminology (no dynamic per-container title field exists); `SubtitleText` is
  derived from `GetLootEntryCount()`.
- `FWTBRBagLootSnapshot.bBagVisible` — **left `false` always**, per its own TODO;
  no open/close state source exists anywhere (not derived, not faked).
  `bCorpseLootVisible` — derived only from `CorpseLoot.bHasFocusedContainer`
  (a reasonable read-only derivation, not invented state).

### Delegates added / reused

- `FWTBRBagLootSnapshotChanged OnBagLootSnapshotChanged` (new, on the ViewModel
  component) — broadcasts after every `RefreshBagLootSnapshot()` call.
- Reused (not modified): `UWTBRInventoryComponent::OnInventoryChanged`,
  `AWTBRCorpseLootContainerActor::OnCorpseLootEntriesChanged`.

### Focus-tracking limitation (documented, not silently ignored)

`UWTBRInteractionComponent` has no "focused interactable changed" delegate today
(re-confirmed before writing this code). The component works around this without
ticking: `RefreshBagLootSnapshot()` re-resolves the currently focused corpse loot
container every time it runs (triggered by `OnInventoryChanged`, or the
currently-bound container's own `OnCorpseLootEntriesChanged`) and rebinds its
entries-changed delegate if the focused container has changed
(`RebindFocusedCorpseLootContainerDelegate()`). This is self-healing but not
fully reactive: if focus changes to a different container and nothing else
triggers a refresh in between, the snapshot will not update until
`RefreshBagLootSnapshot()` is called again. A future widget/controller should
call it manually on relevant input events (e.g. when a pickup/interact prompt
becomes visible) until a real focus-changed delegate exists. Documented as a
TODO directly in the header/cpp, not silently worked around.

### Request-only wrappers exposed

- `RequestUseBagItem(int32 SlotIndex)` → wraps the **existing**
  `AWTBRCharacter::Server_RequestUseInventoryItem(SlotIndex)`.
- `RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex)` → wraps
  the **existing** `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(FocusedContainer,
  LootIndex, TargetSlotIndex)`, resolving `FocusedContainer` fresh at call time
  (never trusts a cached value for a mutating call). Both check
  `IsLocallyControlled()` before dispatching, matching the existing HUD ViewModel
  pattern, and return `false` without any server call if preconditions aren't met.

### Request-only wrappers intentionally NOT added

- `RequestPickupFocusedGroundItem()` / `RequestPickupFocusedDroppedTrigger()` —
  **not added.** `UWTBRInteractionComponent::GetFocusedGroundItem()` and
  `GetFocusedDroppedTrigger()` are `private` — there is no public accessor to
  isolate "the focused ground item" or "the focused dropped trigger" specifically,
  only the unified priority dispatcher `RequestContextInteract()`, which is
  already exposed as `UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()`.
  Adding a second wrapper around the same call here would be redundant — UI
  should call the existing HUD ViewModel function for this.
- `RequestTakeAllLoot()`, `RequestMoveBagItem(FromSlot, ToSlot)`,
  `RequestDropBagItem(SlotIndex)`, `RequestEquipTriggerFromBag(SlotIndex, TargetSlot)`
  — **not added.** Re-confirmed via grep of `WTBRCharacter.h` immediately before
  writing this code: no server-authoritative RPC exists for any of these.
  Documented as comments in the header (not stubbed functions) pointing back to
  this plan's §4 for the provisional names already tracked in the UI contract docs.

### Next step

A thin native `UUserWidget` parent class for the Bag/Corpse-Loot WBPs (mirroring
`UWTBRGeneratedHUDWidget` from HUD Phase 4B), consuming
`UWTBRBagLootViewModelComponent::GetBagLootSnapshot()` via `BindWidgetOptional`
properties matching the JSON widget names from `WBP_BagLoot_BindingPlan.md` §2/§3.
Manual Unreal steps (parent-class assignment, WBP compile, save) remain deferred
— a human is still unavailable to do that step, same as the HUD's still-pending
parent-class assignment.

---

## Phase 4C-BagLoot — Native Widget Parent Class

Status: **Implemented.** `UWTBRBagLootWidget` exists and builds cleanly (0 errors,
0 warnings, Development Editor Win64). No WBP was touched; no `.uasset`/`.umap`
edits; nothing staged/committed/pushed.

A visual fidelity correction pass (Phase 3B.1/3C.1) was completed between Phase 4B
and this pass — the Bag/Corpse-Loot JSON layouts changed, but no binding widget
names changed, so this widget class binds against the same names documented in
§2/§3 above without modification. The large empty middle section of the Bag panel
(reserved for a future Active Trigger / Reserve Trigger management area) is
explicitly out of scope here — no widgets exist for it, and none were invented.

### Files created

- `Source/WTBR/UI/WTBRBagLootWidget.h` / `.cpp` — thin `UUserWidget` subclass,
  same shape as `UWTBRGeneratedHUDWidget` from HUD Phase 4B.

### Widget names bound (all `BindWidgetOptional`)

The **same parent class** is usable for all three generated WBPs — every property
is optional so a WBP missing some of these (e.g. `WBP_BagPanel_Generated` has no
`Txt_CorpseLoot*`/`Txt_LootEntry*` widgets, and vice versa) still compiles cleanly:

- Bag: `Txt_BagTitle`, `Txt_BagCapacity`, `Txt_BagWeight`, `Txt_BagHint`,
  `Txt_BagWarning`, `Txt_BagSlot01Name`..`Txt_BagSlot14Name`,
  `Txt_BagSlot01Count`..`Txt_BagSlot14Count`, `Img_BagSlot01Icon`..`Img_BagSlot14Icon`.
- Corpse Loot: `Txt_CorpseLootTitle`, `Txt_CorpseLootSubtitle`, `Txt_CorpseLootPrompt`,
  `Txt_LootEntry01Name`..`Txt_LootEntry08Name`, `Txt_LootEntry01Type`..`Txt_LootEntry08Type`,
  `Txt_LootEntry01Action`..`Txt_LootEntry08Action`, `Img_LootEntry01Icon`..`Img_LootEntry08Icon`.
- Composite: `Txt_ContextHint` (`WBP_BagLootLayer_Generated` only) — bound but not
  populated, since it has no dedicated snapshot field (same treatment as static
  labels like `Txt_BagTitle`/`Txt_BagHint`, which also have no snapshot field and
  are left as authored in the WBP).

### ViewModel connection behavior

- `NativeConstruct()` resolves `UWTBRBagLootViewModelComponent` via
  `Cast<AWTBRCharacter>(GetOwningPlayerPawn())->GetBagLootViewModelComponent()`
  (identical lookup style to `UWTBRGeneratedHUDWidget`), binds to
  `OnBagLootSnapshotChanged` (`AddUniqueDynamic`), then calls
  `RefreshFromViewModel()` once immediately.
- `NativeDestruct()` unbinds (`RemoveDynamic`) before calling
  `Super::NativeDestruct()`.
- `RefreshFromViewModel()` re-resolves and (re)binds if the cached weak pointer is
  stale (handles the case where the owning pawn wasn't valid yet at construct time),
  then calls `ApplySnapshot(ViewModel->GetBagLootSnapshot())`.
- `ApplyBagSnapshot()` / `ApplyCorpseLootSnapshot()` are the only places that write
  to widgets — never a `Server_Request*`/mutation call. Empty bag slots and empty
  loot rows clear their text to `FText::GetEmpty()` and hide their icon
  (`ESlateVisibility::Collapsed`) rather than showing stale/default WBP content.
  When no corpse loot container is focused, the whole Corpse Loot panel's
  title/subtitle/prompt are cleared to empty rather than showing whatever static
  text was authored in the WBP — "do not invent fake loot" applies at the panel
  level, not just per-row.
- Icon textures are resolved via `TSoftObjectPtr<UTexture2D>::Get()` (non-blocking)
  only — same rule as the ViewModel's own display-refresh code. A not-yet-resident
  icon renders as "no icon" for that frame rather than forcing a synchronous load.

### Request wrappers exposed

- `RequestUseBagItem(int32 SlotIndex)` → thin passthrough to
  `UWTBRBagLootViewModelComponent::RequestUseBagItem(SlotIndex)`.
- `RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex)` → thin
  passthrough to `UWTBRBagLootViewModelComponent::RequestPickupCorpseLootEntry(...)`.

Neither wrapper mutates anything itself — both simply forward to the ViewModel,
which forwards to the existing server RPCs (see Phase 4B-BagLoot above).
`RequestTakeAllLoot`, `RequestMoveBagItem`, `RequestDropBagItem`,
`RequestEquipTriggerFromBag` remain **not exposed** — no server-authoritative
request path exists for any of them (re-confirmed unchanged since Phase 4B-BagLoot).

### Runtime component integration status

**Resolved in this pass.** `UWTBRBagLootViewModelComponent` was not yet attached to
any actor before this pass — only a stray forward declaration
(`class UWTBRBagLootViewModelComponent;`) existed in `WTBRCharacter.h` from an
interrupted attempt in the previous session. That forward declaration was **kept**
(not reverted) because it is exactly what this pass needed to declare a public
member/getter for the new component without pulling in the full component header —
and the wiring was completed to mirror `HUDViewModelComponent`'s existing pattern
exactly:

- `Source/WTBR/WTBRCharacter.h`: added `TObjectPtr<UWTBRBagLootViewModelComponent> BagLootViewModelComponent;`
  (public, `VisibleAnywhere, BlueprintReadOnly, Category=Components` — same
  specifiers as `HUDViewModelComponent`) and
  `GetBagLootViewModelComponent() const { return BagLootViewModelComponent; }`
  (`BlueprintPure`, right next to `GetHUDViewModelComponent()`).
- `Source/WTBR/WTBRCharacter.cpp`: added
  `#include "Components/WTBRBagLootViewModelComponent.h"` and
  `BagLootViewModelComponent = CreateDefaultSubobject<UWTBRBagLootViewModelComponent>(TEXT("BagLootViewModelComponent"));`
  immediately after the existing `HUDViewModelComponent` line.

This was judged safe because it is a byte-for-byte mirror of an already-proven,
already-compiling pattern (`HUDViewModelComponent`) — same component category, same
non-ticking/non-replicated component shape, purely additive, no behavior change to
any existing component or code path. `AWTBRCharacter::GetBagLootViewModelComponent()`
now returns a valid, live component on every character instance, so
`UWTBRBagLootWidget::ResolveViewModel()` will find it at runtime without any further
manual/Blueprint step.

### Reserved middle Bag section

Intentionally deferred, per explicit instruction: the empty middle area of the Bag
panel (Active Trigger / Reserve Trigger / Trigger Management) is not implemented,
has no bound widgets, and this pass does not redesign or fill it.

### Manual Unreal steps (deferred)

1. Open `WBP_BagPanel_Generated` (or `WBP_BagLootLayer_Generated`) in UMG Designer.
2. **Class Settings → Parent Class → `WTBRBagLootWidget`**.
3. Compile the WBP (expect 0 errors — `BindWidgetOptional` means missing/renamed
   widgets never produce a compile error).
4. **Do not use "Save All."**
5. **Do not save the map or any external actors.**
6. Save only the specific WBP asset if manually changed, and only after human review.

---

## Files Inspected For This Audit

- `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagPanel_Generated.json`
- `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_CorpseLootPanel_Generated.json`
- `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagLootLayer_Generated.json`
- `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.html` / `.css`
- `Docs/WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md`
- `Docs/WTBR_UI_CONTRACT_PLAN.md`
- `Docs/WTBR_UI_INVENTORY_LOOT_SPEC.md`
- `Source/WTBR/Inventory/WTBRInventoryComponent.h/.cpp`
- `Source/WTBR/Inventory/WTBRInventorySlot.h`
- `Source/WTBR/Inventory/WTBRItemDataAsset.h`
- `Source/WTBR/Inventory/WTBRGroundItemActor.h`
- `Source/WTBR/Interaction/WTBRCorpseLootContainerActor.h/.cpp`
- `Source/WTBR/Interaction/WTBRDroppedTriggerActor.h` (context only)
- `Source/WTBR/Components/WTBRInteractionComponent.h`
- `Source/WTBR/Components/WTBRHUDViewModelComponent.h/.cpp` (for the Quick Item /
  asset-loading pattern this plan recommends reusing)
- `Source/WTBR/Trigger/WTBRTriggerDataAsset.h` (SlotConstraint / Category / HUDIcon)
- `Source/WTBR/WTBRCharacter.h` (existing Server_Request* RPC signatures)
- `Source/WTBR/UI/WTBRInputBindingDisplayLibrary.h`

## Task Confirmation (Original Audit Pass)

No C++ classes were added or edited. No WBP/UMG/Blueprint/`.uasset`/`.umap`/binary
assets were touched. Unreal was not compiled. That original audit pass was
planning-only.

## Phase 4B-BagLoot Confirmation

C++ was added in this later pass (see "Phase 4B-BagLoot — ViewModel Snapshot
Foundation" above): `Source/WTBR/UI/WTBRBagLootViewTypes.h` (new),
`Source/WTBR/Components/WTBRBagLootViewModelComponent.h/.cpp` (new). No existing
files were modified. No `.uasset`/`.umap`/binary assets were touched. No WBP was
opened, edited, or saved. Nothing staged, committed, or pushed. Development Editor
Win64 build: **0 errors, 0 warnings.**

## Phase 4C-BagLoot Confirmation

C++ was added and existing files were modified in this pass (see "Phase 4C-BagLoot
— Native Widget Parent Class" above): `Source/WTBR/UI/WTBRBagLootWidget.h/.cpp`
(new); `Source/WTBR/WTBRCharacter.h` and `Source/WTBR/WTBRCharacter.cpp` (modified,
to attach `UWTBRBagLootViewModelComponent` mirroring the existing
`HUDViewModelComponent` pattern). No `.uasset`/`.umap`/binary assets were touched.
No WBP was opened, edited, or saved — parent-class assignment remains a deferred
manual step. Nothing staged, committed, or pushed. Development Editor Win64 build:
**0 errors, 0 warnings.**
