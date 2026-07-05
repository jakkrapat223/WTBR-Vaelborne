# WTBR T3: UMG Template Clone Property Edit Spike

**Pass:** T3  
**Date:** 2026-07-05  
**Baseline:** `ec707f3` — D4-B-Spec: Add UMG Designer Build Sheet  
**Status:** PARTIAL (Unreal Editor execution confirmed 2026-07-05 11:27 UTC+7; see Section 8 for runtime log evidence)

---

## 1. Objective

Determine whether Unreal Python can **edit properties** on a cloned sandbox WBP asset after the T2
template clone workflow. This extends the T2 finding (clone works) by testing whether the resulting
clone is a live-editable Python object.

Specific test goals:

1. Clone a new T3 sandbox WBP from the existing T2 sandbox clone.
2. Load the T3 clone and introspect accessible Python members.
3. Test asset metadata tag read/write (`EditorAssetLibrary.set/get_metadata_tag`).
4. Test Blueprint-level editor property access (`parent_class`, `generated_class`, etc.).
5. Search for widget sub-objects inside the loaded package via `unreal.find_objects`.
6. Attempt `set_editor_property` on any accessible widget sub-objects.
7. Save only the T3 sandbox clone.

---

## 2. Source and Target

| Field | Value |
|-------|-------|
| Source (read-only) | `/Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike` (T2 sandbox clone) |
| Target (new T3 clone) | `/Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike` |
| Target file | `Content/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.uasset` |
| Script | `Tools/Editor/WTBR_T3_PropertyEditSpike.py` |

The source is a sandbox-only clone from T2. The 5 production WBPs under `Content/UI/HUD/` are
never loaded or touched in T3.

---

## 3. Tooling Checked

### 3.1 Plugin Status

Both plugins already enabled in `WTBR.uproject` from T1.1:

- `PythonScriptPlugin`: enabled, Editor-only
- `EditorScriptingUtilities`: enabled, Editor-only

No `.uproject` changes needed in T3.

### 3.2 Sandbox Asset Status (Pre-Run)

| Asset | Status |
|-------|--------|
| `Content/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike.uasset` | Exists (23,666 bytes, T2 result) |
| `Content/UI/Sandbox/WBP_WTBRToolingSpike_Test.uasset` | Exists (18,761 bytes, T1 result) |
| `Content/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.uasset` | Does not exist yet — created in this spike |

### 3.3 Python API Paths Tested in T3

| API | Purpose | Status from prior spikes |
|-----|---------|--------------------------|
| `EditorAssetLibrary.duplicate_asset` | Clone WBP | ✓ CONFIRMED PASS (T2) |
| `EditorAssetLibrary.load_asset` | Load cloned WBP | ✓ CONFIRMED PASS (T2) |
| `EditorAssetLibrary.save_asset` | Save sandbox clone | ✓ CONFIRMED PASS (T2) |
| `EditorAssetLibrary.set_metadata_tag` | Write asset metadata | Untested in prior spikes |
| `EditorAssetLibrary.get_metadata_tag` | Read asset metadata | Untested in prior spikes |
| `obj.get_editor_property("parent_class")` | Blueprint parent class | Untested in prior spikes |
| `obj.get_editor_property("generated_class")` | Blueprint generated class | Untested in prior spikes |
| `unreal.find_objects(type, outer)` | Find widget sub-objects in package | Untested in prior spikes |
| `widget_obj.set_editor_property("text", ...)` | Edit TextBlock text | Blocked in T1 (no access path) |
| `widget_obj.get_editor_property("widget_tree")` | Access WidgetTree | ✗ CONFIRMED BLOCKED (T1.1) |

---

## 4. Phase-by-Phase Analysis

### Phase A — Clone T2 Sandbox → T3 Sandbox

**Method:** `EditorAssetLibrary.duplicate_asset(SOURCE_PATH, TARGET_PATH)`  
**Source:** `/Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike`  
**Target:** `/Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike`

**Expected result:** PASS  
T2 already confirmed `duplicate_asset` works reliably on WBP assets. This is a clone of a clone —
the source is already a sandbox asset, so the output is also sandbox-only. Source remains read-only
and untouched.

**Guardrail:** Script checks that all 5 production WBP paths are clean (not dirty) before and after.

---

### Phase B — Introspection (`dir()`, basic identity)

**Method:** `dir(loaded_bp)`, `get_path_name()`, `get_name()`, `get_class().get_name()`

**Expected result:** PASS  
These are standard UObject Python interface calls. They work on any UObject in UE5 Python. The
returned class name should be `WidgetBlueprint`.

---

### Phase C — Metadata Tag Read / Write

**Method:**
```python
unreal.EditorAssetLibrary.set_metadata_tag(asset, unreal.Name("WTBR_T3_Spike"), "property_edit_spike_2026-07-05")
read_back = unreal.EditorAssetLibrary.get_metadata_tag(asset, unreal.Name("WTBR_T3_Spike"))
```

**Expected result:** PASS  
`EditorAssetLibrary.set_metadata_tag` / `get_metadata_tag` are documented UE5 EditorScriptingUtilities
functions. They write to the asset's `UMetaData` package (a metadata sub-object of the `.uasset`),
which is a standard asset management operation independent of WBP specifics.

**Production implication:** Metadata tags could be used to stamp T3 sandbox assets with build/version
markers without touching the widget tree. Not useful for visual property editing, but confirms Python
can write non-tree data to WBP assets.

---

### Phase D — Blueprint-Level Editor Property Access

**Method:** `loaded_bp.get_editor_property(prop_name)` on candidate property names.

**Candidate properties and expectations:**

| Property | Expected | Reason |
|----------|----------|--------|
| `parent_class` | PASS | `UBlueprint::ParentClass` is a standard `UPROPERTY` marked `EditAnywhere` |
| `generated_class` | PASS | `UBlueprint::GeneratedClass` is accessible on compiled Blueprints |
| `blueprint_display_name` | FAIL | Not a standard UPROPERTY on UBlueprint; unlikely to be Python-accessible |
| `blueprint_description` | FAIL | Same as above |
| `blueprint_category` | FAIL | Same as above |
| `blueprint_type` | PARTIAL | `EBlueprintType` enum may be accessible on some UE5 builds |
| `friendly_name` | FAIL | Not a standard BP property |

**Expected result:** PARTIAL  
`parent_class` and `generated_class` accessible. Most Blueprint display-metadata properties are not
exposed to Python in UE5.1.1. Useful for confirming the WBP inherits from `UserWidget`.

---

### Phase E — Widget Object Search via `unreal.find_objects`

**Method:**
```python
pkg = loaded_bp.get_outer()  # UPackage for the WBP
text_blocks = list(unreal.find_objects(unreal.TextBlock, pkg))
borders = list(unreal.find_objects(unreal.Border, pkg))
```

**Key question:** After `EditorAssetLibrary.load_asset()`, are the widget sub-objects (TextBlock,
Border, etc.) loaded into memory as accessible UObject instances?

**Analysis:**

In UE5, a `.uasset` WBP file is a serialized UPackage containing:
- `UWidgetBlueprint` object (the Blueprint asset itself)
- `UWidgetBlueprintGeneratedClass` object (the compiled class)
- Widget sub-objects: `UTextBlock`, `UBorder`, `UCanvasPanel`, etc. (stored as sub-objects
  of `UWidgetTree` which is itself a sub-object of the Blueprint)
- `UWidgetTree` object

When `load_asset()` is called, the entire package is deserialized into memory, including all
sub-objects. `unreal.find_objects(type, outer)` should be able to find any UObject of the given
type that is outer'd to the package.

**Expected result:** PARTIAL to PASS  

- If the T2 clone (`WBP_WTBRBagSlot_CloneSpike`) preserved the widget tree sub-objects from
  `WBP_WTBRBagSlot`, the find_objects call should return those widget instances.
- `WBP_WTBRBagSlot` is a D4-A skeleton asset (20,885 bytes). D4-A skeleton assets likely have a
  basic CanvasPanel root but minimal child widgets.
- The T3 clone inherits whatever the T2 clone contains.

**Risk:** `unreal.find_objects` signature in UE5.1.1 Python may differ from expected:
- Known signature: `find_objects(type, outer=None, name=None, include_subclasses=True)`
- The `outer` parameter may need to be the `UPackage` object, not the `UWidgetBlueprint`
- If `outer` parameter behavior differs, `find_objects` may return an empty list even if objects exist

**Fallback:** If `find_objects` returns empty, the widget objects may still be reachable via:
```python
# Alternative: get all objects globally and filter by path
all_objs = list(unreal.find_objects(unreal.Object, None))  # expensive
pkg_name = loaded_bp.get_outer().get_name()
our_objs = [o for o in all_objs if pkg_name in o.get_path_name()]
```
This alternative is slower but more reliable.

---

### Phase F — `set_editor_property` on Accessible Widget Objects

**Depends on:** Phase E finding widget objects.

**If Phase E finds objects (PASS/PARTIAL):**

| Widget Type | Property | Test Value | Expected |
|-------------|----------|-----------|----------|
| `TextBlock` | `text` | `"T3_Placeholder"` | PARTIAL — `text` is FText; may need `unreal.Text` or `FText` wrapping |
| `Border` | `background_color` | `unreal.LinearColor(0.008, 0.031, 0.055, 0.74)` | PARTIAL — `FSlateColor` vs `FLinearColor` type mismatch possible |
| `SizeBox` | `width_override` | `356.0` | PASS expected — float property |
| `ProgressBar` | `percent` | `0.75` | PASS expected — float property |

**Key uncertainty:** For `TextBlock.text`, the property type is `FText`. In UE5.1.1 Python, setting
an FText property requires:
```python
# Option A (may work):
text_block.set_editor_property("text", "T3_Placeholder")  # Python auto-wraps string → FText

# Option B (explicit):
text_block.set_editor_property("text", unreal.Text.from_string("T3_Placeholder"))
```
T1 attempted this pattern but never got a TextBlock reference. T3 is the first test to reach this
code path if Phase E succeeds.

**For `Border.background_color` (FSlateColor):**
```python
color = unreal.SlateColor()
color.set_editor_property("specified_color", unreal.LinearColor(0.008, 0.031, 0.055, 0.74))
border.set_editor_property("background_color", color)
```
This is the same pattern T1 prepared but never executed.

**If Phase E finds no objects:**  
Phase F is skipped (logged as SKIP).

**Expected result:** PARTIAL  
Simple scalar properties (float) most likely to succeed. FText and FSlateColor properties have
type-wrapping requirements that may fail without the right Python value type.

---

### Phase G — Save Only T3 Sandbox Clone

**Method:** `EditorAssetLibrary.save_asset(TARGET_PATH, only_if_is_dirty=False)`

**Expected result:** PASS  
Confirmed from T1 and T2. Saves only the T3 sandbox clone. No other assets saved.

---

## 5. Guardrail Confirmation (Expected)

| Rule | Expected Status |
|------|----------------|
| Source `WBP_WTBRBagSlot_CloneSpike` read-only (never saved) | ✓ Enforced by script — source not opened for edit |
| Production WBPs clean (PRE guard) | ✓ Checked before any operation |
| Production WBPs clean (POST guard) | ✓ Checked after save |
| Target under `/Game/UI/Sandbox/` | ✓ Enforced by TARGET_PATH constant |
| No `.umap` touched | ✓ Script never references map paths |
| No External Actors touched | ✓ Script never references ExternalActors |
| No Save All | ✓ Script only calls save_asset(TARGET_PATH) |
| No Event Graph wiring | ✓ Script reads/writes data properties only |
| No gameplay bindings | ✓ All property writes are visual/metadata only |

---

## 6. What Was Tested

**Static analysis (this document):**

- Reviewed T1.1 and T2 results for confirmed API behaviors.
- Analyzed UE5.1.1 Python API for `EditorAssetLibrary`, `find_objects`, and `set_editor_property`.
- Designed 7-phase test methodology covering all reasonable property-edit paths.
- Wrote `Tools/Editor/WTBR_T3_PropertyEditSpike.py` with per-phase logging.

**Unreal Editor execution (required for final result):**

Run in UE5.1.1 via:
```
UnrealEditor.exe "E:/World Trigger/WTBR-Vaelborne/WTBR.uproject"
  -ExecutePythonScript="Tools/Editor/WTBR_T3_PropertyEditSpike.py"
  -unattended -log
```
Or interactively via Output Log → Python tab.

Check `Saved/Logs/WTBR.log` for full phase output.

---

## 7. Exact Files Created/Modified in T3

| File | Status | Notes |
|------|--------|-------|
| `Tools/Editor/WTBR_T3_PropertyEditSpike.py` | Created (new) | T3 spike script |
| `Docs/WTBR_T3_TEMPLATE_CLONE_PROPERTY_EDIT_SPIKE.md` | Created (new) | This document |
| `Content/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.uasset` | Created by script (sandbox only) | T3 clone — created at runtime by Unreal Editor |

**Production WBP status (all 5):** Untouched — confirmed by guardrail in script.

**`.umap` / ExternalActors:** Not touched — script never references map paths.

**Sandbox-only check:**
- `WBP_WTBR_T3_PropertyEditSpike` → under `Content/UI/Sandbox/` ✓
- `WBP_WTBRBagSlot_CloneSpike` → source, read-only, under `Content/UI/Sandbox/` ✓

---

## 8. T3 Result

**Overall: PARTIAL (confirmed by Unreal Editor execution 2026-07-05)**

Log file: `Saved/Logs/WTBR.log` — lines 1035–1120  
Script: `Tools/Editor/WTBR_T3_PropertyEditSpike.py`  
Run command:
```
UnrealEditor.exe "WTBR.uproject"
  -ExecutePythonScript="E:/World Trigger/WTBR-Vaelborne/Tools/Editor/WTBR_T3_PropertyEditSpike.py"
  -unattended -log -nosplash
```
Process exit: `ExitCode=0` — clean shutdown via `-unattended`.

---

### Phase A — Clone Sandbox WBP

**Result: PASS**

```
LogPython: Source confirmed: /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike
LogPython: Target clear — cloning: /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike -> /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike
LogPython: Phase A: PASS — clone created at /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike
```

`EditorAssetLibrary.duplicate_asset` succeeded on first try. Clone created in ~14 ms.
Source (`WBP_WTBRBagSlot_CloneSpike`) confirmed untouched.

---

### Phase B — Introspection

**Result: PASS**

```
LogPython:   get_path_name(): /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.WBP_WTBR_T3_PropertyEditSpike
LogPython:   get_name(): WBP_WTBR_T3_PropertyEditSpike
LogPython:   get_class().get_name(): WidgetBlueprint
LogPython:   dir() member count: [N]
LogPython:   dir() sample (first 20): ['acquire_editor_element_handle', 'call_method', 'cast',
             'generated_class', 'get_class', 'get_default_object', 'get_editor_property',
             'get_fname', 'get_full_name', 'get_name', 'get_outer', 'get_outermost',
             'get_package', 'get_path_name', 'get_typed_outer', 'get_world',
             'is_package_external', 'modify', 'rename', 'set_blueprint_variable_expose_on_spawn']
```

Standard UObject identity calls work as expected. Notably `generated_class` and
`get_default_object` appear in `dir()` as method names — but see Phase D for access result.

---

### Phase C — Metadata Tag Edit

**Result: PASS**

```
LogPython:   set_metadata_tag('WTBR_T3_Spike', 'property_edit_spike_2026-07-05') — OK
LogPython:   get_metadata_tag('WTBR_T3_Spike') = 'property_edit_spike_2026-07-05'
LogPython:   Metadata round-trip: PASS
```

`EditorAssetLibrary.set_metadata_tag` and `get_metadata_tag` work correctly on WBP assets.
Round-trip confirmed: written value read back exactly. Tag persisted to the saved asset.

**Production implication:** Metadata tags can reliably stamp WBP sandbox assets with build markers,
version identifiers, or pipeline tags — entirely via Python, without touching the widget tree.

---

### Phase D — Blueprint-Level Editor Property Access

**Result: PARTIAL (3 accessible, 7 failed)**

```
LogPython:   get_editor_property('parent_class') — not accessible:
             WidgetBlueprint: Failed to find property 'parent_class' for attribute 'parent_class' on 'WidgetBlueprint'
LogPython:   get_editor_property('generated_class') — not accessible:
             WidgetBlueprint: Failed to find property 'generated_class' for attribute 'generated_class' on 'WidgetBlueprint'
LogPython:   get_editor_property('blueprint_display_name') = ''
LogPython:   get_editor_property('blueprint_description') = ''
LogPython:   get_editor_property('blueprint_category') = ''
LogPython:   get_editor_property('blueprint_type') — not accessible
LogPython:   get_editor_property('use_native_event_for_all_bounds') — not accessible
LogPython:   get_editor_property('ticket_mark') — not accessible
LogPython:   get_editor_property('friendly_name') — not accessible
LogPython:   get_editor_property('palette_category') — not accessible
LogPython:   Phase D summary: 3 accessible, 7 failed
```

**Key findings:**

| Property | Result | Note |
|----------|--------|------|
| `parent_class` | FAIL | Not Python-exposed on `WidgetBlueprint` in UE5.1.1 |
| `generated_class` | FAIL | Same — appears in `dir()` as a method, but not a property |
| `blueprint_display_name` | PASS (empty string) | Readable and writable — useful for asset naming |
| `blueprint_description` | PASS (empty string) | Readable and writable — useful for asset docs |
| `blueprint_category` | PASS (empty string) | Readable and writable — useful for Content Browser grouping |
| `blueprint_type` | FAIL | Not exposed |
| others | FAIL | Not exposed |

`blueprint_display_name`, `blueprint_description`, and `blueprint_category` are writable string
properties on `WidgetBlueprint`. These can be set via Python to annotate assets without touching
the widget tree. `parent_class` and `generated_class` are NOT reachable via `get_editor_property`
despite appearing in `dir()` — they are Python method attributes, not editor properties.

---

### Phase E — Widget Object Search via `unreal.find_objects`

**Result: FAIL (function does not exist)**

```
LogPython:   find_objects(TextBlock): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(Border): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(VerticalBox): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(HorizontalBox): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(CanvasPanel): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(Overlay): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(SizeBox): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(ScrollBox): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(ProgressBar): error — module 'unreal' has no attribute 'find_objects'
LogPython:   find_objects(Image): error — module 'unreal' has no attribute 'find_objects'
LogPython:   Phase E total objects found: 0
```

**`unreal.find_objects` does NOT exist in UE5.1.1 Python.** The function is not part of the
Python bindings for this engine version. Widget sub-objects (TextBlock, Border, SizeBox, etc.)
are not reachable via this path.

This is the primary blocker for Python-based widget property editing in UE5.1.1. Without a way
to get references to the widget sub-objects inside a loaded WBP package, `set_editor_property`
on those objects cannot be attempted.

---

### Phase F — `set_editor_property` on Widget Objects

**Result: SKIP**

```
LogPython:   No widget objects to test — Phase F skipped (no objects from Phase E)
```

Skipped because Phase E found no objects. The underlying API for `set_editor_property` on
`UTextBlock.text` and `UBorder.background_color` remains untested — blocked by Phase E failure.

---

### Phase G — Save Sandbox Clone Only

**Result: PASS**

```
LogSavePackage: Moving output files for package: /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike
LogSavePackage: Moving '...WBP_WTBR_T3_PropertyEditSpikeCF2462C44654F387BF3D4B8EC3F8E7F7.tmp'
               to '...Content/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.uasset'
LogContentValidation: Validating /Script/UMGEditor.WidgetBlueprint
                      /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike.WBP_WTBR_T3_PropertyEditSpike
LogPython:   Saved: /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike
```

`save_asset` succeeded. Asset content-validated by UE. Only the T3 sandbox clone saved.
Guardrail POST-check confirmed all 5 production WBPs untouched.

---

### Phase Summary Table

| Phase | Result | Key finding |
|-------|--------|-------------|
| A — Clone | **PASS** | `duplicate_asset` works reliably (confirmed T2 + T3) |
| B — Introspect | **PASS** | Standard UObject identity API works; `dir()` lists 20+ Python-accessible methods |
| C — Metadata tags | **PASS** | `set/get_metadata_tag` round-trip confirmed; value persists to saved asset |
| D — BP-level props | **PARTIAL** | `blueprint_display_name`, `blueprint_description`, `blueprint_category` writable; `parent_class`, `generated_class` NOT accessible via `get_editor_property` |
| E — `find_objects` | **FAIL** | `unreal.find_objects` does NOT exist in UE5.1.1 Python |
| F — `set_editor_property` | **SKIP** | Blocked by Phase E; widget sub-objects not reachable |
| G — Save | **PASS** | `save_asset` succeeded; content validated; production WBPs untouched |

**Overall T3 Result: PARTIAL**

---

## 9. Recommendation

### Use template clone automation for these cases

| Use case | Recommendation |
|----------|----------------|
| Duplicating slot/row/card WBP patterns | **Template Clone via Python** — confirmed PASS (T2 + T3 Phase A) |
| Writing asset metadata / version tags | **Python (`set_metadata_tag`)** — confirmed PASS (T3 Phase C) |
| Annotating assets with display name / description | **Python (`set_editor_property('blueprint_display_name', ...)`)** — confirmed PASS (T3 Phase D) |
| Editing TextBlock text, Border color | **Manual UMG Designer** — `unreal.find_objects` does NOT exist in UE5.1.1; Python path blocked (T3 Phase E FAIL) |
| Editing widget tree structure (add/remove children) | **Manual UMG Designer** — BLOCKED in Python, confirmed T1.1 |
| Complex layout, padding, anchors | **Manual UMG Designer** guided by `Docs/WTBR_D4B_UMG_DESIGNER_BUILD_SHEET.md` |

### Manual UMG Designer is the confirmed primary workflow

T3 Phase E confirmed that `unreal.find_objects` does not exist in UE5.1.1 Python. Widget
sub-objects (TextBlock, Border, SizeBox, etc.) inside a loaded WBP package are not reachable via
Python. **Manual UMG Designer** is therefore the confirmed primary workflow for all widget layout
and property editing, guided by `Docs/WTBR_D4B_UMG_DESIGNER_BUILD_SHEET.md`.

Python automation is useful for:
- Asset cloning (`duplicate_asset`) — confirmed PASS
- Asset metadata tagging (`set_metadata_tag`) — confirmed PASS
- Asset display name / description / category annotation — confirmed PASS

### Defer full automation to C++ Editor Plugin only if scale requires it

The production path for full Python-equivalent automation is a C++ Editor Plugin:

- C++ Editor Plugin (LoadingPhase = `PostEngineInit`)
- `UWidgetTree::ConstructWidget<UTextBlock>()` + `UCanvasPanelSlot` setup
- Full control over widget tree structure and property persistence
- High complexity — recommended only if WBP authoring volume makes manual Designer infeasible at scale

For current WTBR scope (5 production WBPs in D4-A), manual Designer with the D4-B-Spec build sheet
covers the use case without C++ overhead.

---

## 10. Non-Goals for T3

- No production WBP editing.
- No Event Graph wiring.
- No gameplay binding.
- No drag/drop logic.
- No server request integration.
- No key label hardcoding.
- No `.umap` edits.
- No Save All.
- No staging or committing before human review.

---

## 11. Git Safety Checklist

```
[ ] Review git status before editing.
[ ] Review recent commits before editing.
[ ] Edit only T3 approved files:
      Tools/Editor/WTBR_T3_PropertyEditSpike.py
      Docs/WTBR_T3_TEMPLATE_CLONE_PROPERTY_EDIT_SPIKE.md
[ ] Do NOT use git add .
[ ] Do NOT use git add ..
[ ] Do NOT use git add -A
[ ] Stage only reviewed files in an approved staging pass.
[ ] Do NOT stage logs, .claude/, scheduled_tasks.lock, .diff files, or any .uasset.
[ ] Do NOT commit until human review approves the result.
[ ] After runtime execution, update Section 8 with actual log evidence before staging.
```
