# WTBR T1: UMG Layout Tooling Spike

**Pass:** T1 / T1.1  
**Date:** 2026-07-05  
**Baseline:** 73bc151 (D4-A: Add bag loot WBP skeleton assets)  
**Status:** PARTIAL (confirmed — WBP created, WidgetTree not populated via Python)  

---

## 1. Objective

Determine whether UE5.1.1 supports a safe editor automation workflow that can:
- Convert a JSON layout spec (HTML/CSS-inspired) into a UMG Widget Blueprint asset
- Create or modify WBP layout through Unreal Editor-supported tooling (Unreal Python, Editor Utility Widget, or C++ editor tooling)
- Do so without manually writing or faking binary `.uasset` files

This is a **tooling spike**, not a production UI implementation.  
Sandbox target: `Content/UI/Sandbox/WBP_WTBRToolingSpike_Test.uasset`  
Production D4-A WBPs are untouched.

---

## 2. Tooling Checked

### 2.1 Python Script Plugin (`PythonScriptPlugin`)

**Status:** Mounts as Engine plugin but NOT explicitly enabled in `.uproject`

Evidence from logs (B7C_Server.log, B7D_Client1.log, C1_UIContract_AutoTest.log, etc.):
```
LogPluginManager: Mounting Engine plugin PythonScriptPlugin
FPackageName: Mount point added: '../../Plugins/Experimental/PythonScriptPlugin/Content/' mounted to '/PythonScriptPlugin/'
```

The `.uproject` at root contains only:
```json
{
  "Plugins": [
    {
      "Name": "ModelingToolsEditorMode",
      "Enabled": true,
      "TargetAllowList": ["Editor"]
    }
  ]
}
```

`PythonScriptPlugin` and `EditorScriptingUtilities` are NOT listed.

**Implication:** The Engine-level Python plugin loads by default in UE5 Editor builds, but the `unreal` Python module for editor automation (asset creation, folder creation) also requires `EditorScriptingUtilities` to be enabled for `unreal.EditorAssetLibrary` and `unreal.AssetToolsHelpers` APIs to be available.

**Required action before experiment can run:**
1. Add `PythonScriptPlugin` explicitly to `.uproject` Plugins array with `"Enabled": true` and `"TargetAllowList": ["Editor"]`
2. Add `EditorScriptingUtilities` to `.uproject` Plugins array with `"Enabled": true` and `"TargetAllowList": ["Editor"]`

### 2.2 Editor Scripting Utilities (`EditorScriptingUtilities`)

**Status:** NOT enabled in `.uproject`

This plugin provides:
- `unreal.EditorAssetLibrary` — folder creation, asset loading/saving
- `unreal.AssetToolsHelpers.get_asset_tools()` — asset creation via factory
- `unreal.WidgetBlueprintFactory` — creates WidgetBlueprint assets
- `unreal.EditorLoadingAndSavingUtils` — saving assets programmatically

Without this plugin, the Python script at `Tools/Editor/WTBR_UMGLayoutSpike.py` will fail with import or API-not-found errors.

### 2.3 Editor Utility Widget / Editor Utility Blueprint

**Status:** Not explored in this spike (would require creating a Blueprint asset manually first — defeats the automation purpose)

Could be a fallback for simpler workflows (UI buttons that call Python/scripts), but for automated WBP creation the Python + EditorScriptingUtilities path is more suitable.

### 2.4 C++ Editor Plugin

**Status:** Not explored. Reserved as future work only.

C++ editor tooling (`IAssetTools`, `FWidgetBlueprintFactory`, `WidgetTree` manipulation via `UWidgetTree::ConstructWidget`) is the most powerful path and is how Unreal internally handles WBP creation. However:
- Requires a new editor-only C++ module
- High complexity for a spike
- Should be future work only, reported here as a path if Python proves insufficient

### 2.5 JSON Layout Spec

**Status:** Viable as a build sheet / specification artifact regardless of Python outcome.

The JSON spec (`Docs/WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json`) serves as:
- A human-readable layout specification
- A source of truth for manual UMG Designer work
- A potential input file for a future Python script to parse

### 2.6 Existing Tooling in Repo

Searched with:
```
rg -n "EditorUtility|unreal\.py|PythonScript|ExecutePythonScript|AssetTools|EditorAssetLibrary|WidgetBlueprint|WidgetTree|WidgetBlueprintFactory|UWidgetBlueprint" . -S
```

**Result:** No existing Python editor scripts, no Editor Utility Blueprints/Widgets, no existing automation scripts for asset creation in the repo. All matches were from `.log` files (Engine startup messages), not source code.

---

## 3. UE5.1.1 Python: Widget Blueprint Creation

### Can Python create a Widget Blueprint asset?

**Yes — with prerequisites met.**

The path is:
```python
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("ParentClass", unreal.UserWidget)
widget_bp = asset_tools.create_asset(
    "WBP_WTBRToolingSpike_Test",
    "/Game/UI/Sandbox",
    unreal.WidgetBlueprint,
    factory
)
unreal.EditorAssetLibrary.save_asset(widget_bp.get_path_name())
```

This creates a valid, compilable `.uasset` file **through Unreal's own asset system** — no binary file is written manually. This is the correct and safe path.

**Prerequisites:**
- `EditorScriptingUtilities` enabled in `.uproject`
- Python script run **inside Unreal Editor** (via Unreal's Python console, `-ExecutePythonScript` launch arg, or a startup script)
- NOT run from a standalone Python process outside the editor

### Can Python populate the Widget Tree (add children in Designer hierarchy)?

**Limited in UE5.1.1.**

The Widget Tree (`UWidgetTree`) in UE5.1.1 is not fully exposed to Python for structural manipulation. The `WidgetTree` property on a `WidgetBlueprint` exists in C++ (`UWidgetBlueprint::WidgetTree`) but:

1. **Direct child-add via Python is not reliably supported** in UE5.1.1:
   - `widget_bp.widget_tree` may not be accessible as a Python property in UE5.1 (depends on exact build and Python bindings)
   - `WidgetTree.construct_widget` equivalent in Python is `widget_tree.construct_widget(widget_class, name)` — this exists in UE5 Python but its behavior with Designer hierarchy (slots, canvas panel offsets, anchors) is not guaranteed to produce correct Designer-visible layout

2. **What typically works:**
   - Creating the WBP asset itself ✓
   - Setting the parent class ✓
   - Setting UWidget properties on already-existing widgets via `set_editor_property` ✓

3. **What does NOT work cleanly:**
   - Adding new widget children to `CanvasPanel`, `VerticalBox`, etc. through Python in UE5.1 (the slot geometry, anchors, and designer offsets are not reliably set)
   - WidgetTree structural modifications often require Editor refresh or do not persist correctly to the designer hierarchy visible in the UMG Designer tab

4. **Workaround path (not pursued in this spike):**
   - C++ editor plugin: use `UWidgetTree::ConstructWidget<T>()` and `UCanvasPanel::AddSlot()` — fully supported
   - Manual UMG Designer: build hierarchy by hand, guided by the JSON spec

---

## 4. Editor Utility Widget as Alternative

An Editor Utility Widget (EUW) is a Blueprint-based editor tool (runs in editor, not in game). It can:
- Call `AssetTools.CreateAsset` to create WBPs
- Trigger Python scripts
- Provide buttons/inputs for iterative manual creation

**Verdict:** More appropriate for interactive/iterative workflows, not for pure automation. For this spike, the Python script path is more suitable as a first research target.

---

## 5. C++ Editor Plugin (Future Work Only)

If Python's Widget Tree manipulation limitations block the tooling goal, a C++ editor-only plugin is the next path:
- Create an editor module (LoadingPhase = `PostEngineInit`)
- Use `IAssetTools::CreateAsset<UWidgetBlueprint>` with `UWidgetBlueprintFactory`
- Use `UWidgetTree::ConstructWidget<UCanvasPanel>` + `UWidgetTree::ConstructWidget<UBorder>` etc.
- Set slot properties via `UCanvasPanelSlot`

This is the most powerful path but highest complexity. Report as future work.

---

## 6. What Was Tested

**Research (static analysis):**
- Read `.uproject` — confirmed plugin configuration before changes
- Searched all source files for existing editor automation
- Inspected UE startup logs (B7C/B7D/C series) to confirm PythonScriptPlugin loading behavior
- Reviewed `Content/UI/HUD/` — confirmed 5 D4-A production WBPs present and untouched
- Confirmed `Content/UI/Sandbox/` did not exist before test run

**T1.1 — Actual Unreal Editor execution (2026-07-05):**

WTBR.uproject updated to enable `PythonScriptPlugin` and `EditorScriptingUtilities`. Editor launched via:

```
UnrealEditor.exe "WTBR.uproject"
  -ExecutePythonScript="Tools/Editor/WTBR_UMGLayoutSpike.py"
  -unattended -log
```

**Log evidence (Saved/Logs/WTBR.log):**
```
LogPython: Using Python 3.9.7
LogPluginManager: Mounting Engine plugin EditorScriptingUtilities
LogPython: === WTBR T1 UMG Layout Tooling Spike — START ===
LogPython: GUARDRAIL: production WBPs untouched OK
LogPython: Created folder: /Game/UI/Sandbox
LogPython: Created Widget Blueprint: /Game/UI/Sandbox/WBP_WTBRToolingSpike_Test.WBP_WTBRToolingSpike_Test
LogPython: Attempting WidgetTree population (UE5.1.1 limitation may apply)...
LogPython: Warning: WidgetTree population failed:
  WidgetBlueprint: Failed to find property 'widget_tree' for attribute 'widget_tree' on 'WidgetBlueprint'
LogSavePackage: Moving output files for package: /Game/UI/Sandbox/WBP_WTBRToolingSpike_Test
LogPython: Saved: /Game/UI/Sandbox/WBP_WTBRToolingSpike_Test.WBP_WTBRToolingSpike_Test
LogPython: GUARDRAIL: production WBPs untouched OK
LogPython: === RESULT: PARTIAL — WBP created (empty root), widget tree not populated. ===
LogPython: === WTBR T1 UMG Layout Tooling Spike — END ===
LogContentValidation: Validating /Script/UMGEditor.WidgetBlueprint /Game/UI/Sandbox/WBP_WTBRToolingSpike_Test
LogModuleManager: Shutting down and abandoning module EditorScriptingUtilities
```

Process exit: `ExitCode=0` — clean exit via `-unattended` auto-shutdown after script completion.

**Note on `close_editor()`:** `UnrealEditorSubsystem` in UE5.1.1 does not have `close_editor()` method. The editor closed due to `-unattended` + `-ExecutePythonScript` triggering automatic shutdown after the Python script finishes. Future runs don't need explicit close_editor() call.

---

## 7. Exact Files Created/Modified

**Created:**
- `Docs/WTBR_T1_UMG_LAYOUT_TOOLING_SPIKE.md` — this file
- `Docs/WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json` — JSON layout spec prototype
- `Tools/Editor/WTBR_UMGLayoutSpike.py` — Unreal Python editor script
- `Content/UI/Sandbox/WBP_WTBRToolingSpike_Test.uasset` — sandbox WBP created by Unreal Editor Python (18,761 bytes, valid asset, content-validated by UE)

**Modified:**
- `WTBR.uproject` — added `PythonScriptPlugin` + `EditorScriptingUtilities` to Plugins array

**Untouched (confirmed):**
- All 5 `Content/UI/HUD/` production WBPs (timestamps unchanged: Jul 5 14:20, pre-script)
- No `.umap` files
- No `Content/__ExternalActors__`
- No `Source/C++`
- No textures/sounds/VFX

---

## 8. Result

**PARTIAL (confirmed by Unreal Editor execution 2026-07-05)**

| Component | Status | Notes |
|-----------|--------|-------|
| Plugin research | ✓ DONE | Both plugins enabled in `.uproject`, confirmed mounting in editor |
| JSON layout spec | ✓ DONE | `Docs/WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json` created |
| Python script | ✓ DONE | `Tools/Editor/WTBR_UMGLayoutSpike.py` ran successfully inside UE5.1.1 |
| WBP asset creation via Python | ✓ PASS | `Content/UI/Sandbox/WBP_WTBRToolingSpike_Test.uasset` created, 18,761 bytes, content-validated |
| Widget Tree population via Python | ✗ BLOCKED | UE5.1.1: `widget_tree` property NOT exposed on `WidgetBlueprint` in Python — confirmed |
| Production WBPs untouched | ✓ PASS | Guardrail confirmed x2 (before + after) |
| Auto-close | ⚠ NOTE | `close_editor()` not available in UE5.1.1; editor auto-shuts via `-unattended` |

---

## 9. Recommendation

**Use Python for asset creation; use UMG Designer for widget hierarchy (guided by JSON spec).**

Specifically:

1. **Immediate next step (human action required):**
   - Add `PythonScriptPlugin` and `EditorScriptingUtilities` to `.uproject` Plugins array (see exact JSON snippet in Section 10)
   - Open Unreal Editor → Window → Developer Tools → Python Console (or Output Log → Python tab)
   - Run: `import unreal; exec(open('Tools/Editor/WTBR_UMGLayoutSpike.py').read())`
   - Verify sandbox WBP is created under Content → UI → Sandbox in the Content Browser

2. **If WBP asset creation succeeds but Widget Tree population fails:**
   - Declare PARTIAL: Python tooling can create WBP assets reliably
   - Use the JSON spec as a build sheet; human implements widget hierarchy in UMG Designer
   - This is still a major workflow improvement (no manual "File → New Widget Blueprint")

3. **If Widget Tree population also succeeds (best case):**
   - Declare PASS: full automation path viable
   - Future work: extend JSON spec to include bindings, style properties
   - Future work: C++ editor plugin for more complex layouts

4. **If Python tooling fails entirely (plugins not available or UE build doesn't support it):**
   - Declare BLOCKED: return to manual UMG Designer work
   - JSON spec remains useful as a design reference

---

## 10. Plugin Prerequisites (Human Action)

To enable the required plugins, add to `WTBR.uproject` in the `"Plugins"` array:

```json
{
  "Name": "PythonScriptPlugin",
  "Enabled": true,
  "TargetAllowList": ["Editor"]
},
{
  "Name": "EditorScriptingUtilities",
  "Enabled": true,
  "TargetAllowList": ["Editor"]
}
```

**Important:** After adding these, Unreal Editor will prompt to rebuild modules. Allow it.

Alternatively, enable via Unreal Editor:
- Edit → Plugins → search "Python Script Plugin" → enable
- Edit → Plugins → search "Editor Scripting Utilities" → enable
- Restart editor when prompted

---

## 11. Guardrail Notes

| Rule | Status |
|------|--------|
| No production WBP touched | ✓ Confirmed — `Content/UI/HUD/` WBPs not touched |
| No maps touched | ✓ Confirmed |
| No external actors touched | ✓ Confirmed |
| No Source/C++ touched | ✓ Confirmed |
| No fake `.uasset` generated | ✓ Confirmed — no `.uasset` written manually |
| No gameplay logic added | ✓ Confirmed |
| No production UI bindings added | ✓ Confirmed |
| Not staged/committed | ✓ Confirmed |

---

## 12. Path to `-ExecutePythonScript` Launch

If interactive Python console is not preferred, the script can also be run via:

```
UnrealEditor.exe "E:/World Trigger/WTBR-Vaelborne/WTBR.uproject" -ExecutePythonScript="Tools/Editor/WTBR_UMGLayoutSpike.py" -unattended -nullrhi
```

Note: `-nullrhi` means no rendering — Widget Blueprints can typically be created without rendering, but some editor subsystems may not initialize correctly in headless mode. Interactive editor run is more reliable for widget asset creation.
