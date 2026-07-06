# WBP_HUD_Generated — Phase 4A Binding Audit + Binding Plan

Project: WTBR / Vaelborne: Dominion
Engine: Unreal Engine 5.1.1 C++
Pass: Phase 4A — docs/planning only. No C++, no WBP, no compile, no staging/commit/push.

## Status

- Docs-only planning pass.
- No C++ classes added or edited in this pass.
- No WBP/UMG/Blueprint/.uasset/.umap/binary assets touched.
- No Unreal compile performed.

---

## 1. Current Generated HUD Status

- JSON path: `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_Generated.json`
- `widgetName`: `WBP_HUD_Generated`
- `resolution`: `1920 x 1080`
- `root`: `RootCanvas` (`CanvasPanel`)
- Last WebPreview validation result (Phase 3.3 correction pass): **Widgets = 70, Warnings = 0, Skipped = 0, Unsupported = 0, Supported = 71** — "Validation passed."
- The current layout is accepted as a usable visual mockup for binding/runtime testing. No further visual polish is in scope for this pass.

---

## 2. Binding Candidate List

### Text

- `Txt_AliveValue`
- `Txt_KillValue`
- `Txt_KillFeedLine1`
- `Txt_KillFeedLine2`
- `Txt_KillFeedLine3`
- `Txt_ZoneValue`
- `Txt_ZoneTimer`
- `Txt_PhaseValue`
- `Txt_HPValue`
- `Txt_VaelValue`
- `Txt_QuickItemName`
- `Txt_QuickItemCount`
- `Txt_QuickItemKey`
- `Txt_MainTriggerName`
- `Txt_MainTriggerCost`
- `Txt_MainTriggerSlotIndicator`
- `Txt_SubTriggerName`
- `Txt_SubTriggerCost`
- `Txt_SubTriggerSlotIndicator`
- `Txt_PickupPrompt`
- `Txt_DamageFeedback`

### Progress

- `PB_HP`
- `PB_Vael`
- `PB_MainTriggerCooldown`
- `PB_SubTriggerCooldown`

### Image

- `Img_MinimapPlaceholder`

Not included in the Phase 4 scope but present in the JSON as additional, lower-priority binding candidates for a later pass: `Txt_AliveLabel`, `Txt_KillLabel`, `Txt_KillFeedTitle`, `Txt_MinimapTitle`, `Txt_ZoneLabel`, `Txt_PhaseLabel`, `Txt_HPLabel`, `Txt_VaelLabel`, `Txt_QuickItemLabel`, `Txt_CancelPrompt`, `Txt_MainTriggerLabel`, `Txt_MainTriggerKey`, `Txt_SubTriggerLabel`, `Txt_SubTriggerKey`, `Txt_HitMarker`.

---

## 3. Minimal Phase 4 Binding Scope

Start with only these 14 widgets:

- `PB_HP`
- `Txt_HPValue`
- `PB_Vael`
- `Txt_VaelValue`
- `Txt_MainTriggerName`
- `Txt_MainTriggerCost`
- `Txt_MainTriggerSlotIndicator`
- `Txt_SubTriggerName`
- `Txt_SubTriggerCost`
- `Txt_SubTriggerSlotIndicator`
- `Txt_ZoneValue`
- `Txt_PhaseValue`
- `Txt_ZoneTimer`
- `Txt_PickupPrompt`

Do not bind everything at once. Kill feed, minimap, quick item, cancel prompt, hit/damage feedback, and key-glyph labels are deferred to a later pass.

---

## 4. Runtime Data Source Audit

This audit is based on direct inspection of the current C++ source (not guessed). A HUD ViewModel layer already exists and is partially wired — see the important note at the end of this section.

| HUD Data Need | Source Class / Function | Replicated? | Status |
|---|---|---|---|
| HP current | `UWTBRHealthComponent::GetCurrentHP()` (`Components/WTBRHealthComponent.h`) — backing field `CurrentHP`, `ReplicatedUsing=OnRep_CurrentHP` | Yes | **Ready** |
| HP max | `UWTBRHealthComponent::GetMaxHP()` (reads `UWTBRCoreStatsDataAsset`) | N/A (DataAsset) | **Ready** |
| Vael current | `UWTBRVaelComponent::GetCurrentVael()` (`Components/WTBRVaelComponent.h`) — backing field `CurrentVael`, `ReplicatedUsing=OnRep_CurrentVael` | Yes | **Ready** |
| Vael max | `UWTBRVaelComponent::GetMaxVael()` (reads `UWTBRCoreStatsDataAsset`) | N/A (DataAsset) | **Ready** |
| Active Main trigger name | `UWTBRTriggerSetComponent::GetActiveMainDataAsset()->DisplayName` (`Trigger/WTBRTriggerSetComponent.h`, `Trigger/WTBRTriggerDataAsset.h`) | `ActiveMainIndex`/`TriggerSlots` replicated | **Ready** |
| Active Main trigger cost | `UWTBRTriggerDataAsset::VaelCostPerUse` on the active Main DataAsset | Same as above | **Ready** (raw float; needs "Cost: N" text formatting) |
| Active Main trigger cooldown % | — | — | **UNKNOWN / needs building.** No unified cooldown-remaining getter exists anywhere in the Trigger hierarchy. Individual trigger subclasses (`WTBRSoluxTrigger.cpp`, `WTBRFulgrixTrigger.cpp`, `WTBRAcervynTrigger.cpp`, `WTBRFulgrisTrigger.cpp`, `WTBRPiercexTrigger.cpp`, `WTBRSerpveilTrigger.cpp`, `WTBRTelornTrigger.cpp`, `WTBRVenyxTrigger.cpp`) each track their own fire-cooldown timer internally, but none expose a read-only "0.0–1.0 remaining" value. |
| Active Main slot index (1-4) | `UWTBRTriggerSetComponent::GetActiveMainIndex()` returns **absolute** index 0-3 for Main. Slot-indicator text needs `AbsoluteIndex + 1` mapping (see §6). | Yes | **Ready**, needs mapping in caller |
| Active Sub trigger name/cost/slot | Same as Main via `GetActiveSubDataAsset()` / `GetActiveSubIndex()` (absolute 4-7, map to 1-4 via `AbsoluteIndex - 4 + 1`) | Yes | **Ready** for name/cost; **UNKNOWN** for cooldown % (same gap as Main) |
| Current zone number | — | — | **UNKNOWN / not built.** No safe-zone / shrinking-zone manager class found anywhere in `Source/WTBR`. Matches project memory: "Shrinking zone: phase-based HP drain, heal reduction, UI values, phase values — all placeholder." `AWTBRKaldrixZone` exists but is the Kaldrix Black Trigger explosion zone, unrelated to BR safe-zone shrink. |
| Current (shrink) phase number | — | — | **UNKNOWN / not built.** Do not confuse with `EWTBRMatchPhase` (Lobby/PreMatch/Countdown/InMatch/PostMatch) on `AWTBRGameState`, which IS implemented but represents match lifecycle, not the zone-shrink phase (e.g. "Phase 3" of a shrinking ring). |
| Zone timer | `FWTBRHUDMatchSnapshot.ZoneTimeRemaining` field exists in `UI/WTBRHUDViewTypes.h` with a `bHasZoneTimer` guard flag, but `UWTBRHUDViewModelComponent::BuildMatchSnapshot()` (`Components/WTBRHUDViewModelComponent.cpp`) currently only populates `MatchPhaseText` — it never sets `ZoneTimeRemaining` or `bHasZoneTimer`. | N/A | **UNKNOWN / stubbed field, not populated.** Same root cause as zone number above (no zone manager yet). |
| Pickup prompt text | `UWTBRInteractionComponent::GetFocusedInteractionPromptText()` (`Components/WTBRInteractionComponent.h`) → `FText`. Already wired into `FWTBRHUDPickupPromptSnapshot` (`bIsVisible`, `PromptText`, `Binding`) via `UWTBRHUDViewModelComponent::BuildPickupPromptSnapshot()`. | Read-only, focus-trace based (not replicated, local per-client) | **Ready** |
| Quick item name | `FWTBRInventorySlot::ItemData` (`Inventory/WTBRInventorySlot.h`) → `UWTBRItemDataAsset` display fields. **Not yet exposed** on `FWTBRHUDQuickItemSnapshot`, which currently only carries `bHasItem`, `Icon`, `Count`, `State`, `Binding` — no name/text field. | `InventorySlots` replicated on `UWTBRInventoryComponent` | **Partial** — data exists, snapshot struct needs a `Name` (FText) field added |
| Quick item count | `FWTBRHUDQuickItemSnapshot.Count` (int32), already built by `BuildQuickItemSnapshot()` | Derived from replicated inventory | **Ready** (needs "xN" text formatting) |
| Quick item key (binding label) | `FWTBRHUDQuickItemSnapshot.Binding` (`FWTBRHUDBindingDisplay`), resolved via `UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName()` (`UI/WTBRInputBindingDisplayLibrary.h`) | N/A (input mapping, not replicated) | **Ready** |

### Important note: a HUD ViewModel already exists

Direct inspection found `UWTBRHUDViewModelComponent` (`Source/WTBR/Components/WTBRHUDViewModelComponent.h/.cpp`) — this is exactly the `UWTBRHUDViewModel` candidate proposed in `Docs/WTBR_UI_CONTRACT_PLAN.md` §"Suggested Future C++ Classes", except it is **already implemented**, not just planned. It exposes:

- `GetHUDSnapshot() : FWTBRHUDSnapshot` (BlueprintPure, read-only) — a nested struct of `Match`, `Vitals`, `MainTrigger`, `SubTrigger`, `QuickItem`, `PickupPrompt`, `CancelPrompt` sub-snapshots (`UI/WTBRHUDViewTypes.h`).
- `OnHUDSnapshotChanged` (BlueprintAssignable delegate) — fires on HP/Vael/inventory/active-trigger/match-phase change.
- `RequestUseDisplayedQuickItem()`, `RequestPickupFocusedTarget()`, `RequestCancelCurrentUIOrAction()` — request-only Blueprint-callable wrappers, matching the UI Contract Plan's request-only rule.

However, the snapshot structs (`FWTBRHUDTriggerCardSnapshot`, `FWTBRHUDQuickItemSnapshot`, `FWTBRHUDMatchSnapshot`) do **not yet** carry every field this HUD JSON needs (no cost text, no cooldown percent, no quick-item name, no zone/phase number). This is the main gap between "what exists" and "what Phase 4B needs to add."

---

## 5. Suggested Update Functions

Proposed functions the WBP (or its future C++ parent class) should expose, to be called from a HUD refresh tick or from `OnHUDSnapshotChanged`:

```
RefreshVitals(CurrentHP, MaxHP, CurrentVael, MaxVael)
RefreshMainTrigger(Name, CostText, SlotIndex, CooldownPercent)
RefreshSubTrigger(Name, CostText, SlotIndex, CooldownPercent)
RefreshZone(ZoneNumber, PhaseNumber, TimeRemainingText)
RefreshPickupPrompt(bVisible, PromptText)
RefreshQuickItem(ItemName, CountText, KeyText)
RefreshDamageFeedback(DamageText)
```

Notes:

- `RefreshVitals` and `RefreshPickupPrompt` map almost directly onto the existing `FWTBRHUDSnapshot.Vitals` and `.PickupPrompt` structs — thin passthrough.
- `RefreshMainTrigger` / `RefreshSubTrigger` need `CooldownPercent` and cost-text fields that do not exist on `FWTBRHUDTriggerCardSnapshot` yet — this is new C++ work for Phase 4B, not just WBP wiring.
- `RefreshZone` has **no backing data source at all** yet (see §4) — this function can be stubbed/no-op or hidden until the zone/shrink system is built. Do not fabricate zone/phase values.
- `RefreshQuickItem` needs an `ItemName` field added to `FWTBRHUDQuickItemSnapshot`.
- `RefreshDamageFeedback` has no existing snapshot at all; it is out of the minimal Phase 4 scope (see §3) and can be deferred entirely.

---

## 6. Slot Indicator Rule

```
Slot 1 = ◆ ◇ ◇ ◇
Slot 2 = ◇ ◆ ◇ ◇
Slot 3 = ◇ ◇ ◆ ◇
Slot 4 = ◇ ◇ ◇ ◆
```

This must be runtime-driven from `UWTBRTriggerSetComponent::GetActiveMainIndex()` / `GetActiveSubIndex()`, not hardcoded final data. Mapping:

- Main: `SlotNumber = GetActiveMainIndex() + 1` (absolute 0-3 → slot 1-4).
- Sub: `SlotNumber = GetActiveSubIndex() - 4 + 1` (absolute 4-7 → slot 1-4).
- `FWTBRHUDTriggerCardSnapshot.ActiveSlotIndex` already stores the resolved slot index per card (need to confirm during Phase 4B whether it stores the absolute 0-7 index or an already-normalized 0-3 value — verify against `BuildTriggerCardSnapshot()` before wiring).

---

## 7. Recommended Implementation Path

| Option | Description | Trade-off |
|---|---|---|
| **A — Manual WBP Graph functions** | Human opens `WBP_HUD_Generated` and writes Blueprint graph functions matching §5 that call `UWTBRHUDViewModelComponent::GetHUDSnapshot()` (or the individual component getters) and push values into the named widgets. | Fastest to a first runtime test. No new C++. Least reusable; logic lives only in this one WBP. |
| **B — C++ `UUserWidget` parent class with `BindWidget`** | Add a C++ parent class (e.g. `UWTBRHUDWidget : public UUserWidget`) with `UPROPERTY(meta=(BindWidget)) UTextBlock* Txt_HPValue;` etc. matching the JSON widget names, plus native `Tick`/delegate-driven refresh calling into `UWTBRHUDViewModelComponent`. Assign as the WBP's parent class. | More stable long-term; refresh logic is testable/reusable in C++; but requires editing `WTBR_HUD_Generated`'s parent class assignment (a WBP metadata change) and a small C++ addition. |
| **C — Plugin auto-parentClass support** | Extend `UMGJsonGenerator` to let the JSON specify a parent class for the generated WBP automatically. | Convenience only; not needed for the first binding test; plugin source change is out of scope unless separately approved. |

**Recommendation for Phase 4B: Option B**, for the 14 widgets in the minimal scope (§3), because:

- `UWTBRHUDViewModelComponent` already exists and already does most of the read-only assembly work — Option B lets a thin C++ widget class consume it directly and keeps mutation-free UI logic centralized and testable, consistent with `Docs/WTBR_UI_CONTRACT_PLAN.md`'s "prefer read-only structs and const accessors" guidance.
- Option A is acceptable as a *very first* smoke test (e.g. binding just `Txt_HPValue`/`PB_HP` manually in the WBP graph to prove the import → runtime pipeline works end-to-end) before committing to Option B for the full minimal scope.
- Option C is explicitly deferred per the task.

---

## 8. Manual Unreal Steps For Human

1. Import the latest `WBP_HUD_Generated.json` via **Tools → Import UMG JSON...** (or re-import if already present) to make sure the Phase 3.3 layout/widget names are current in the editor.
2. Open `WBP_HUD_Generated` in UMG Designer.
3. Confirm all 14 minimal-scope widget names from §3 exist exactly as named (case-sensitive) in the widget tree.
4. Decide Option A (manual Graph) vs Option B (C++ parent class) per §7 before writing any binding logic.
5. **Do not** use "Save All".
6. **Do not** save the map or any external actors.
7. Save only the specific WBP asset if it is manually changed during this pass, and only after human review.

---

## 9. Test Checklist

- [ ] HP text (`Txt_HPValue`) and bar (`PB_HP`) update when HP changes.
- [ ] Vael text (`Txt_VaelValue`) and bar (`PB_Vael`) update when Vael changes.
- [ ] Main trigger name (`Txt_MainTriggerName`), cost (`Txt_MainTriggerCost`), and slot indicator (`Txt_MainTriggerSlotIndicator`) update on slot switch.
- [ ] Sub trigger name (`Txt_SubTriggerName`), cost (`Txt_SubTriggerCost`), and slot indicator (`Txt_SubTriggerSlotIndicator`) update on slot switch.
- [ ] Zone number (`Txt_ZoneValue`), phase (`Txt_PhaseValue`), and timer (`Txt_ZoneTimer`) update — **expected to fail/no-op until the zone/shrink system exists (§4)**; do not treat this as a Phase 4B blocker for the other 11 widgets.
- [ ] Pickup prompt text/visibility (`Txt_PickupPrompt`) updates when focusing/unfocusing an interactable.
- [ ] No client-side mutation: confirm none of the above bindings call any `Server_Request*` mutation function directly from a UI tick/refresh path — only read-only getters/snapshots.
- [ ] PIE listen server test (2 clients if possible) to confirm HP/Vael/trigger-slot changes replicate correctly to the HUD on a non-host client.

---

## Phase 4B — Native Widget Parent Class

Status: **Implemented.** `UWTBRGeneratedHUDWidget` exists and builds cleanly (0 errors, 0
warnings, Development Editor Win64). No WBP was touched; no map/external actors were
touched; nothing was staged/committed/pushed.

### What was added

- `Source/WTBR/UI/WTBRGeneratedHUDWidget.h` / `.cpp` — thin `UUserWidget` subclass.
  All 24 widgets from the minimal + optional-later scope are bound via
  `UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))` (not required
  `BindWidget`), so the WBP still compiles even if a widget is renamed or missing
  during iteration.
- `RefreshFromViewModel()` — resolves `AWTBRCharacter::GetHUDViewModelComponent()`
  from `GetOwningPlayerPawn()` and calls `ApplySnapshot()` with the current
  `FWTBRHUDSnapshot`.
- `ApplySnapshot(const FWTBRHUDSnapshot&)` — the only place that writes to widgets;
  never calls a `Server_Request*` or other mutating function.
- `NativeConstruct()` binds to `UWTBRHUDViewModelComponent::OnHUDSnapshotChanged`
  (`AddUniqueDynamic`) and calls `RefreshFromViewModel()` once immediately.
  `NativeDestruct()` unbinds (`RemoveDynamic`) before calling `Super::NativeDestruct()`.
- Safe helpers: `SetTextSafe`, `SetPercentSafe` (clamped 0-1), `FormatCurrentMax`,
  `FormatTriggerCostText`, `FormatSlotIndicatorFromAbsoluteIndex`.
- `SetPickupPromptVisible(bool)` and `SetDamageFeedbackText(const FText&)` exposed as
  `BlueprintCallable` manual hooks, per the task's request.

### Small additive ViewModel extension (closing 2 of the audit's gaps)

Two of the Phase 4A "partial" gaps had underlying data available but no snapshot
field to carry it — both were closed with small, additive, default-safe struct
field additions (no existing field removed or renamed, no function signature
changed):

- `FWTBRHUDTriggerCardSnapshot` gained `bIsCostKnownForHUD`, `bHasVaelCost`,
  `EffectiveVaelCost` (`Source/WTBR/UI/WTBRHUDViewTypes.h`), populated in
  `UWTBRHUDViewModelComponent::BuildTriggerCardSnapshot()` from the character's
  existing `GetActiveMainTriggerVaelAffordabilityForHUD()` /
  `GetActiveSubTriggerVaelAffordabilityForHUD()` (already-existing, already-used
  getters — no new gameplay-facing API added).
- `FWTBRHUDQuickItemSnapshot` gained `ItemName` (FText), populated in
  `BuildQuickItemSnapshot()` from the resolved `UWTBRItemDataAsset::DisplayName`.

Both changes were made because the task explicitly asked to bind `Txt_MainTriggerCost`
/ `Txt_SubTriggerCost` / `Txt_QuickItemName` from the snapshot, and the data was one
struct field away from being real instead of a placeholder.

### Build.cs change

`Source/WTBR/WTBR.Build.cs` — added `"UMG"` to `PublicDependencyModuleNames`. Required
because `UUserWidget`, `UTextBlock`, and `UProgressBar` live in the UMG module, which
the `WTBR` module did not previously depend on.

### Widgets bound and their data source

| Widget | Source | Status |
|---|---|---|
| `PB_HP`, `Txt_HPValue` | `Snapshot.Vitals.HP` / `.MaxHP` | Live |
| `PB_Vael`, `Txt_VaelValue` | `Snapshot.Vitals.Vael` / `.MaxVael` | Live |
| `Txt_MainTriggerName`, `Txt_SubTriggerName` | `Snapshot.{Main,Sub}Trigger.TriggerName` | Live |
| `Txt_MainTriggerCost`, `Txt_SubTriggerCost` | `Snapshot.{Main,Sub}Trigger.{bIsCostKnownForHUD,bHasVaelCost,EffectiveVaelCost}` (new fields, §above) | Live |
| `Txt_MainTriggerSlotIndicator`, `Txt_SubTriggerSlotIndicator` | `Snapshot.{Main,Sub}Trigger.ActiveSlotIndex` mapped per §6 | Live |
| `PB_MainTriggerCooldown`, `PB_SubTriggerCooldown` | — | **Placeholder 0.0** — no unified cooldown-remaining getter exists (§4 gap, unchanged) |
| `Txt_ZoneValue`, `Txt_PhaseValue` | — | **Placeholder "-"** — no shrink-zone system exists (§4 gap, unchanged) |
| `Txt_ZoneTimer` | `Snapshot.Match.ZoneTimeRemaining` guarded by `bHasZoneTimer` | **Placeholder "-"** in practice — `BuildMatchSnapshot()` never sets `bHasZoneTimer` true yet |
| `Txt_PickupPrompt` (+ visibility) | `Snapshot.PickupPrompt.{bIsVisible,PromptText}` | Live |
| `Txt_QuickItemName` | `Snapshot.QuickItem.{bHasItem,ItemName}` (new field, §above) with `"Quick Item"` fallback | Live |
| `Txt_QuickItemCount` | `Snapshot.QuickItem.{bHasItem,Count}` formatted `"xN"` | Live |
| `Txt_QuickItemKey` | `Snapshot.QuickItem.Binding` with `"[Input]"` fallback | Live |
| `Txt_AliveValue`, `Txt_KillValue`, `Txt_KillFeedLine1-3`, `Txt_DamageFeedback` | — | **Bound but not populated** — no snapshot fields exist for these yet; deferred per task scope ("Optional later") |

### First smoke test steps (manual, human)

1. Open the project in Unreal Editor (the new C++ has already been compiled by this
   pass — no live-coding recompile should be needed, but hot-reload if prompted).
2. Import the latest `WBP_HUD_Generated.json` if not already current (Tools → Import
   UMG JSON...).
3. Open `WBP_HUD_Generated` in UMG Designer.
4. **Class Settings → Parent Class → `WTBRGeneratedHUDWidget`** (search by the class
   name; it should appear since the module compiled successfully).
5. Compile the WBP (Designer toolbar → Compile). Expect 0 errors — `BindWidgetOptional`
   means missing/renamed widgets produce no compile error, only a silent unbound
   pointer at runtime.
6. **Do not use "Save All".**
7. **Do not save the map or any external actors.**
8. Save only `WBP_HUD_Generated` itself, and only after a human reviews the change.
9. PIE test (listen server, 2 clients if possible): confirm HP/Vael text+bars, trigger
   name/cost/slot-indicator, and pickup prompt update live; confirm zone/phase/cooldown
   widgets show their placeholder ("-" / empty bar) without erroring.

### Known missing data sources (unchanged from Phase 4A, still real gaps)

- Trigger cooldown percent — no unified per-trigger cooldown-remaining getter
  anywhere in the Trigger hierarchy. Building this needs a small design decision
  (e.g. a virtual on `UWTBRTriggerBase`) that is out of scope for a "thin widget" pass.
- Zone number / shrink-phase number / zone timer — no safe-zone/shrink-zone manager
  exists at all yet (confirmed both in Phase 4A and by re-reading
  `BuildMatchSnapshot()`, which only ever sets `MatchPhaseText`).
- Kill feed lines, Alive/Kill counts, damage feedback — no snapshot fields exist;
  `Txt_AliveValue`/`Txt_KillValue`/`Txt_KillFeedLine1-3`/`Txt_DamageFeedback` are bound
  in the widget class for future use but intentionally left unpopulated by
  `ApplySnapshot()` in this pass.

These are documented as TODOs directly in `WTBRGeneratedHUDWidget.cpp` next to the
relevant `ApplySnapshot()` lines.

---

## Files Inspected For This Audit

- `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_Generated.json`
- `Docs/WTBR_UI_CONTRACT_PLAN.md`
- `Docs/WTBR_UI_INVENTORY_LOOT_SPEC.md`
- `Docs/WTBR_UMG_HUD_WIDGET_BREAKDOWN.md`
- `Source/WTBR/Components/WTBRHealthComponent.h`
- `Source/WTBR/Components/WTBRVaelComponent.h`
- `Source/WTBR/Components/WTBRInteractionComponent.h`
- `Source/WTBR/Components/WTBRHUDViewModelComponent.h` / `.cpp`
- `Source/WTBR/Trigger/WTBRTriggerSetComponent.h`
- `Source/WTBR/Trigger/WTBRTriggerDataAsset.h`
- `Source/WTBR/Inventory/WTBRInventoryComponent.h`
- `Source/WTBR/Inventory/WTBRInventorySlot.h`
- `Source/WTBR/UI/WTBRHUDViewTypes.h`
- `Source/WTBR/UI/WTBRInputBindingDisplayLibrary.h`
- `Source/WTBR/WTBRGameState.h`

## Task 2 Confirmation (Phase 4A)

No C++ classes were added. No WBP assets were touched. Unreal was not compiled. This document was planning-only for Phase 4A.

## Phase 4B Confirmation

C++ was added in this pass (see "Phase 4B — Native Widget Parent Class" above):
`Source/WTBR/UI/WTBRGeneratedHUDWidget.h/.cpp` (new), plus small additive edits to
`Source/WTBR/UI/WTBRHUDViewTypes.h`, `Source/WTBR/Components/WTBRHUDViewModelComponent.cpp`,
and `Source/WTBR/WTBR.Build.cs`. No `.uasset`/`.umap`/binary assets were touched. No
WBP was opened, edited, or saved by this pass — assigning `WBP_HUD_Generated`'s parent
class is an explicit manual human step (see smoke test steps above). Nothing was
staged, committed, or pushed. Development Editor Win64 build: **0 errors, 0 warnings.**
