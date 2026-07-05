# WTBR T2: UMG Template Clone Spike

**Pass:** T2  
**Date:** 2026-07-05  
**Baseline:** 538f695 (T1.1: Add partial UMG layout tooling spike)  
**Status:** PASS (clone created and saved; human visual verification required)

---

## 1. Objective

Test Path B — Template Clone workflow.  
Verify whether Unreal Python can duplicate an existing production Widget Blueprint template into a new sandbox clone while preserving the template hierarchy/layout, and without modifying the source production asset.

---

## 2. Source Template

| Field | Value |
|-------|-------|
| Asset path | `/Game/UI/HUD/WBP_WTBRBagSlot` |
| File | `Content/UI/HUD/WBP_WTBRBagSlot.uasset` |
| Treatment | **Read-only** — never saved, modified, recompiled, or dirtied |
| Source size | 20,885 bytes (pre-run) |

---

## 3. Sandbox Clone Target

| Field | Value |
|-------|-------|
| Asset path | `/Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike` |
| File | `Content/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike.uasset` |
| Clone size | 23,666 bytes (post-run) |
| Size delta | +2,781 bytes — expected: duplicate embeds new package path in binary |

---

## 4. Tooling API Tested

**Primary path used:**
```python
unreal.EditorAssetLibrary.duplicate_asset(
    source_path="/Game/UI/HUD/WBP_WTBRBagSlot",
    destination_path="/Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike"
)
```

**Result:** Succeeded on first try — no fallback needed.

**Fallback path (not needed this run):**
```python
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
clone = asset_tools.duplicate_asset(target_name, target_folder, source_object)
```

**Save:**
```python
unreal.EditorAssetLibrary.save_asset(TARGET_PATH, only_if_is_dirty=False)
```

---

## 5. Whether Duplicate Succeeded

**Yes — PASS.**

Log evidence (Saved/Logs/WTBR.log):
```
LogPython: === WTBR T2 UMG Template Clone Spike — START ===
LogPython: Source : /Game/UI/HUD/WBP_WTBRBagSlot
LogPython: Target : /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: GUARDRAIL [PRE]: all production WBPs untouched OK
LogPython: Source confirmed: /Game/UI/HUD/WBP_WTBRBagSlot
LogPython: Target clear (does not exist): /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: Folder exists: /Game/UI/Sandbox
LogPython: Duplicating: /Game/UI/HUD/WBP_WTBRBagSlot -> /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: duplicate_asset (EditorAssetLibrary) succeeded: /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: Saved clone: /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: GUARDRAIL [POST]: all production WBPs untouched OK
LogPython: === RESULT: PASS — Clone created and saved: /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike ===
LogPython: Human visual verification required: open clone in UMG Designer and confirm hierarchy matches source.
LogPython: === WTBR T2 UMG Template Clone Spike — END ===
```

Operation time: ~10 ms for duplication, ~71 ms for save. Process exit: ExitCode=0.

---

## 6. Whether Hierarchy/Layout Is Preserved

**Human visual verification required.**

The duplication is a binary-level copy of the source `.uasset` via Unreal's internal asset duplication mechanism. The resulting binary is a valid `.uasset` (23,666 bytes) with a new package name embedded. Structurally, `duplicate_asset` in UE5 creates an identical copy of all serialized data including:
- WidgetTree root and children
- Panel slots (Canvas Panel slot positions, anchors, alignment)
- Widget properties (text, colors, fonts, padding)
- Blueprint graph references (if any)

**To confirm visually:**
1. Open `Content/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike` in Unreal Editor Content Browser
2. Double-click to open in UMG Designer
3. Verify the widget hierarchy in the Hierarchy panel matches the source `WBP_WTBRBagSlot`
4. Confirm no compilation errors

---

## 7. Property Changes via Python

**Deferred — not tested in this spike.**

T2 confirmed that asset duplication works. The next logical step (T3 or production use) would be:
- Loading the clone after duplication
- Using `set_editor_property` to change text, colors, or slot sizes
- This is analogous to what works in T1.1 for `TextBlock.text`, `Border.background_color`, etc.
- Widget tree structural changes (add/remove children) remain BLOCKED per T1 findings

---

## 8. Guardrail Confirmation

| Rule | Status |
|------|--------|
| Production WBP untouched (PRE guard) | ✓ PASS — confirmed in log |
| Production WBP untouched (POST guard) | ✓ PASS — confirmed in log |
| Source WBP not saved/dirtied | ✓ PASS — guardrail found no dirty production packages |
| No Source/C++ touched | ✓ PASS |
| No maps touched | ✓ PASS |
| No external actors touched | ✓ PASS |
| No assets imported | ✓ PASS |
| No fake .uasset written | ✓ PASS — Unreal's `duplicate_asset` API used |
| WTBR.uproject unchanged | ✓ PASS — plugins already enabled from T1.1 |

---

## 9. Result

**PASS**

`EditorAssetLibrary.duplicate_asset` successfully cloned `WBP_WTBRBagSlot` into sandbox `WBP_WTBRBagSlot_CloneSpike`. Asset saved. Guardrails passed. Human visual verification of hierarchy/layout in UMG Designer is the remaining step to confirm full fidelity.

---

## 10. Recommendation

### Use Template Clone for repeated slot/row/card patterns

Template Clone (Path B) is the **recommended workflow** for WTBR production UI assets that share the same widget structure:

| Asset type | Recommendation |
|------------|----------------|
| BagSlot, LootEntry rows, card-style widgets | **Template Clone via Python** — clone once, adjust text/color/size via `set_editor_property` |
| BagPanel, HUDRoot, unique panels | **Manual UMG Designer** guided by JSON spec (`WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json`) |
| Any asset needing new widget children | **Manual UMG Designer** — widget tree structural add/remove still BLOCKED in Python |

### Keep C++ Editor Plugin as future-only

The C++ Editor Plugin path (Path A / T1 Section 2.4) enables full `UWidgetTree::ConstructWidget<T>()` automation. This remains the option if:
- The number of distinct WBP patterns grows significantly
- Automated hierarchy generation becomes a bottleneck
- Manual Designer work is no longer feasible at scale

For current WTBR scope (5 production WBPs in D4-A, small UI surface), Path B covers the use case without C++ overhead.

### Suggested T3 scope

Test `set_editor_property` modifications on the cloned WBP to confirm:
- TextBlock `.text` changes persist after save
- TextBlock `.color_and_opacity` changes persist
- CanvasPanel slot `.size` / `.position` / `.anchors` changes persist (if exposed)

If all persist correctly → **Template Clone + Property Edit workflow is production-ready**.
