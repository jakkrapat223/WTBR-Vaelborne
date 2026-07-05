"""
WTBR T3 UMG Template Clone Property Edit Spike
Run INSIDE Unreal Editor only — do NOT run as standalone Python.

Purpose:
  Test whether Unreal Python can edit WBP properties on a cloned sandbox asset
  after the T2 template clone workflow. Covers metadata tags, Blueprint-level
  property access, widget object search via find_objects, and set_editor_property
  on accessible widget sub-objects.

Source (read-only):
  /Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike   (T2 sandbox clone)

Target (new sandbox clone for T3):
  /Game/UI/Sandbox/WBP_WTBR_T3_PropertyEditSpike

How to run:
  Open WTBR.uproject in Unreal Editor
  Output Log -> Python:
    exec(open('Tools/Editor/WTBR_T3_PropertyEditSpike.py').read())
  OR launch with:
    -ExecutePythonScript="Tools/Editor/WTBR_T3_PropertyEditSpike.py"

GUARDRAILS enforced by this script:
  - Source is sandbox only (NOT a production WBP)
  - Target is sandbox only (under /Game/UI/Sandbox/)
  - Production WBPs under /Game/UI/HUD/ are never loaded, touched, or saved
  - Blocks if any production WBP is found dirty before or after the run
  - Each test phase is logged individually — failures do not abort all phases
  - Only the T3 clone is saved at the end
"""

import unreal

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

SOURCE_PATH = "/Game/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike"  # T2 sandbox clone — read-only
TARGET_FOLDER = "/Game/UI/Sandbox"
TARGET_NAME = "WBP_WTBR_T3_PropertyEditSpike"
TARGET_PATH = f"{TARGET_FOLDER}/{TARGET_NAME}"

PRODUCTION_PATHS = [
    "/Game/UI/HUD/WBP_WTBRBagLootLayer",
    "/Game/UI/HUD/WBP_WTBRBagPanel",
    "/Game/UI/HUD/WBP_WTBRBagSlot",
    "/Game/UI/HUD/WBP_WTBRCorpseLootPanel",
    "/Game/UI/HUD/WBP_WTBRCorpseLootEntry",
]

# Metadata tag used in Test C
T3_METADATA_TAG = "WTBR_T3_Spike"
T3_METADATA_VALUE = "property_edit_spike_2026-07-05"

# Placeholder text written to first accessible TextBlock in Test F
T3_PLACEHOLDER_TEXT = "[T3_Placeholder]"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _log_section(label):
    unreal.log(f"--- {label} ---")


def _guard_production_untouched(label=""):
    """Abort if any production WBP package is dirty."""
    try:
        dirty_pkgs = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        for pkg in dirty_pkgs:
            pkg_name = pkg.get_name()
            for prod in PRODUCTION_PATHS:
                if prod in pkg_name:
                    raise RuntimeError(
                        f"GUARDRAIL VIOLATION [{label}]: production WBP dirty: {pkg_name}"
                    )
    except RuntimeError:
        raise
    except Exception as e:
        unreal.log_warning(f"GUARDRAIL check failed to run ({label}): {e}")
        return
    unreal.log(f"GUARDRAIL [{label}]: all production WBPs untouched OK")


def _ensure_sandbox_folder():
    if not unreal.EditorAssetLibrary.does_directory_exist(TARGET_FOLDER):
        unreal.EditorAssetLibrary.make_directory(TARGET_FOLDER)
        unreal.log(f"Created folder: {TARGET_FOLDER}")
    else:
        unreal.log(f"Folder exists: {TARGET_FOLDER}")


# ---------------------------------------------------------------------------
# Phase A: Clone T2 sandbox clone → T3 sandbox clone
# ---------------------------------------------------------------------------

def _phase_a_clone():
    _log_section("Phase A: Clone sandbox WBP")
    unreal.log(f"Source (read-only): {SOURCE_PATH}")
    unreal.log(f"Target (new T3 clone): {TARGET_PATH}")

    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_PATH):
        raise RuntimeError(
            f"BLOCKED — Source not found: {SOURCE_PATH}\n"
            "Ensure T2 spike was run first and WBP_WTBRBagSlot_CloneSpike exists."
        )
    unreal.log(f"Source confirmed: {SOURCE_PATH}")

    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_PATH):
        unreal.log(f"Target already exists: {TARGET_PATH} — loading existing asset (skip clone)")
        return unreal.EditorAssetLibrary.load_asset(TARGET_PATH)

    unreal.log(f"Target clear — cloning: {SOURCE_PATH} -> {TARGET_PATH}")
    ok = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_PATH, TARGET_PATH)
    if not ok:
        # Fallback: AssetTools
        try:
            src_obj = unreal.EditorAssetLibrary.load_asset(SOURCE_PATH)
            asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
            clone = asset_tools.duplicate_asset(TARGET_NAME, TARGET_FOLDER, src_obj)
            ok = clone is not None
            if ok:
                unreal.log(f"Cloned via AssetTools fallback: {clone.get_path_name()}")
        except Exception as e2:
            unreal.log_warning(f"AssetTools fallback failed: {e2}")

    if not ok:
        raise RuntimeError("BLOCKED — Clone failed via all paths.")

    unreal.log(f"Phase A: PASS — clone created at {TARGET_PATH}")
    loaded = unreal.EditorAssetLibrary.load_asset(TARGET_PATH)
    if loaded is None:
        raise RuntimeError(f"BLOCKED — Load returned None immediately after clone: {TARGET_PATH}")
    return loaded


# ---------------------------------------------------------------------------
# Phase B: Introspection — list accessible Python members
# ---------------------------------------------------------------------------

def _phase_b_introspect(loaded_bp):
    _log_section("Phase B: Introspection")
    results = {"status": "PARTIAL", "notes": []}

    try:
        pkg_name = loaded_bp.get_path_name()
        obj_name = loaded_bp.get_name()
        class_name = loaded_bp.get_class().get_name()
        unreal.log(f"  get_path_name(): {pkg_name}")
        unreal.log(f"  get_name(): {obj_name}")
        unreal.log(f"  get_class().get_name(): {class_name}")
        results["notes"].append(f"path={pkg_name}, name={obj_name}, class={class_name}")
        results["status"] = "PASS"
    except Exception as e:
        unreal.log_warning(f"  Basic identity access failed: {e}")
        results["notes"].append(f"identity access failed: {e}")

    try:
        members = [m for m in dir(loaded_bp) if not m.startswith("_")]
        unreal.log(f"  dir() member count: {len(members)}")
        unreal.log(f"  dir() sample (first 20): {members[:20]}")
        results["notes"].append(f"dir() returned {len(members)} members")
    except Exception as e:
        unreal.log_warning(f"  dir() failed: {e}")

    return results


# ---------------------------------------------------------------------------
# Phase C: Metadata tag edit
# ---------------------------------------------------------------------------

def _phase_c_metadata(loaded_bp):
    _log_section("Phase C: Metadata tag edit")
    results = {"status": "FAIL", "notes": []}

    try:
        unreal.EditorAssetLibrary.set_metadata_tag(loaded_bp, unreal.Name(T3_METADATA_TAG), T3_METADATA_VALUE)
        unreal.log(f"  set_metadata_tag('{T3_METADATA_TAG}', '{T3_METADATA_VALUE}') — OK")
        results["notes"].append("set_metadata_tag: OK")
    except Exception as e:
        unreal.log_warning(f"  set_metadata_tag failed: {e}")
        results["notes"].append(f"set_metadata_tag failed: {e}")
        return results

    try:
        read_back = unreal.EditorAssetLibrary.get_metadata_tag(loaded_bp, unreal.Name(T3_METADATA_TAG))
        unreal.log(f"  get_metadata_tag('{T3_METADATA_TAG}') = '{read_back}'")
        if read_back == T3_METADATA_VALUE:
            unreal.log("  Metadata round-trip: PASS")
            results["status"] = "PASS"
            results["notes"].append(f"get_metadata_tag: '{read_back}' — match OK")
        else:
            unreal.log_warning(f"  Metadata mismatch: expected '{T3_METADATA_VALUE}', got '{read_back}'")
            results["status"] = "PARTIAL"
            results["notes"].append(f"metadata mismatch: got '{read_back}'")
    except Exception as e:
        unreal.log_warning(f"  get_metadata_tag failed: {e}")
        results["notes"].append(f"get_metadata_tag failed: {e}")

    return results


# ---------------------------------------------------------------------------
# Phase D: Blueprint-level editor property access
# ---------------------------------------------------------------------------

def _phase_d_bp_properties(loaded_bp):
    _log_section("Phase D: Blueprint-level editor property access")
    results = {"status": "PARTIAL", "notes": [], "pass_count": 0, "fail_count": 0}

    candidate_props = [
        "parent_class",
        "generated_class",
        "blueprint_display_name",
        "blueprint_description",
        "blueprint_category",
        "blueprint_type",
        "use_native_event_for_all_bounds",
        "ticket_mark",
        "friendly_name",
        "palette_category",
    ]

    for prop in candidate_props:
        try:
            val = loaded_bp.get_editor_property(prop)
            unreal.log(f"  get_editor_property('{prop}') = {val!r}")
            results["notes"].append(f"'{prop}' = {val!r}")
            results["pass_count"] += 1
        except Exception as e:
            unreal.log(f"  get_editor_property('{prop}') — not accessible: {e}")
            results["fail_count"] += 1

    unreal.log(f"  Phase D summary: {results['pass_count']} accessible, {results['fail_count']} failed")
    if results["pass_count"] > 0:
        results["status"] = "PASS" if results["fail_count"] == 0 else "PARTIAL"

    return results


# ---------------------------------------------------------------------------
# Phase E: Widget object search via unreal.find_objects
# ---------------------------------------------------------------------------

def _phase_e_find_widget_objects(loaded_bp):
    _log_section("Phase E: Widget object search via find_objects")
    results = {"status": "FAIL", "notes": [], "found_objects": []}

    try:
        pkg = loaded_bp.get_outer()
        unreal.log(f"  Package outer: {pkg.get_name() if pkg else 'None'}")

        # Search for widget sub-objects within this package
        widget_types = {
            "TextBlock": unreal.TextBlock,
            "Border": unreal.Border,
            "VerticalBox": unreal.VerticalBox,
            "HorizontalBox": unreal.HorizontalBox,
            "CanvasPanel": unreal.CanvasPanel,
            "Overlay": unreal.Overlay,
            "SizeBox": unreal.SizeBox,
            "ScrollBox": unreal.ScrollBox,
            "ProgressBar": unreal.ProgressBar,
            "Image": unreal.Image,
        }

        total_found = 0
        for type_name, ue_type in widget_types.items():
            try:
                found = list(unreal.find_objects(ue_type, pkg))
                if found:
                    unreal.log(f"  find_objects({type_name}): {len(found)} found")
                    for obj in found:
                        obj_path = obj.get_path_name() if obj else "None"
                        unreal.log(f"    {obj_path}")
                        results["found_objects"].append((type_name, obj))
                    total_found += len(found)
                else:
                    unreal.log(f"  find_objects({type_name}): 0 found")
            except Exception as e:
                unreal.log(f"  find_objects({type_name}): error — {e}")
                results["notes"].append(f"find_objects({type_name}) error: {e}")

        unreal.log(f"  Phase E total objects found: {total_found}")
        results["notes"].append(f"total found: {total_found}")

        if total_found > 0:
            results["status"] = "PASS"
        else:
            results["status"] = "PARTIAL"
            results["notes"].append("No widget sub-objects found via find_objects")

    except Exception as e:
        unreal.log_warning(f"  Phase E failed: {e}")
        results["notes"].append(f"phase failed: {e}")

    return results


# ---------------------------------------------------------------------------
# Phase F: Property set_editor_property on accessible widget objects
# ---------------------------------------------------------------------------

def _phase_f_set_properties(found_objects):
    _log_section("Phase F: set_editor_property on accessible widget objects")
    results = {"status": "FAIL", "notes": [], "pass_count": 0, "fail_count": 0}

    if not found_objects:
        unreal.log("  No widget objects to test — Phase F skipped (no objects from Phase E)")
        results["status"] = "SKIP"
        results["notes"].append("skipped: no objects from Phase E")
        return results

    # Per-type property test list: (type_name, property_name, test_value)
    type_test_map = {
        "TextBlock": [
            ("text", "T3_Placeholder"),
        ],
        "Border": [
            ("background_color", unreal.LinearColor(r=0.008, g=0.031, b=0.055, a=0.74)),
        ],
        "CanvasPanel": [],
        "Overlay": [],
        "VerticalBox": [],
        "HorizontalBox": [],
        "SizeBox": [
            ("width_override", 356.0),
        ],
        "ProgressBar": [
            ("percent", 0.75),
        ],
    }

    tested_types = set()

    for type_name, obj in found_objects:
        if type_name in tested_types:
            continue  # test each type once to avoid redundant writes
        tested_types.add(type_name)

        tests = type_test_map.get(type_name, [])
        if not tests:
            unreal.log(f"  {type_name}: no test properties defined — skip")
            continue

        for prop_name, test_value in tests:
            # First read current value
            try:
                old_val = obj.get_editor_property(prop_name)
                unreal.log(f"  {type_name}.get_editor_property('{prop_name}') = {old_val!r}")
                results["notes"].append(f"{type_name}.{prop_name} current = {old_val!r}")
            except Exception as e:
                unreal.log(f"  {type_name}.get_editor_property('{prop_name}') — not readable: {e}")
                results["notes"].append(f"{type_name}.{prop_name} get failed: {e}")
                results["fail_count"] += 1
                continue

            # Then try to write
            try:
                obj.set_editor_property(prop_name, test_value)
                unreal.log(f"  {type_name}.set_editor_property('{prop_name}', {test_value!r}) — OK")
                results["notes"].append(f"{type_name}.{prop_name} set OK")
                results["pass_count"] += 1
            except Exception as e:
                unreal.log_warning(f"  {type_name}.set_editor_property('{prop_name}') failed: {e}")
                results["notes"].append(f"{type_name}.{prop_name} set failed: {e}")
                results["fail_count"] += 1

    unreal.log(f"  Phase F summary: {results['pass_count']} set OK, {results['fail_count']} failed")
    if results["pass_count"] > 0:
        results["status"] = "PASS" if results["fail_count"] == 0 else "PARTIAL"

    return results


# ---------------------------------------------------------------------------
# Phase G: Save only T3 sandbox clone
# ---------------------------------------------------------------------------

def _phase_g_save():
    _log_section("Phase G: Save T3 sandbox clone only")
    try:
        ok = unreal.EditorAssetLibrary.save_asset(TARGET_PATH, only_if_is_dirty=False)
        if ok:
            unreal.log(f"  Saved: {TARGET_PATH}")
            return True
        unreal.log_warning(f"  save_asset returned False for {TARGET_PATH}")
        return False
    except Exception as e:
        unreal.log_warning(f"  save_asset failed: {e}")
        return False


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def run_spike():
    unreal.log("=== WTBR T3 Template Clone Property Edit Spike — START ===")
    unreal.log(f"Source (read-only): {SOURCE_PATH}")
    unreal.log(f"Target (sandbox):   {TARGET_PATH}")

    phase_results = {}

    try:
        _guard_production_untouched("PRE")
        _ensure_sandbox_folder()

        # Phase A — clone
        loaded_bp = _phase_a_clone()
        phase_results["A_clone"] = "PASS"

        # Phase B — introspect
        phase_results["B_introspect"] = _phase_b_introspect(loaded_bp)

        # Phase C — metadata tags
        phase_results["C_metadata"] = _phase_c_metadata(loaded_bp)

        # Phase D — blueprint-level props
        phase_results["D_bp_props"] = _phase_d_bp_properties(loaded_bp)

        # Phase E — find_objects
        phase_e = _phase_e_find_widget_objects(loaded_bp)
        phase_results["E_find_objects"] = phase_e

        # Phase F — set_editor_property on found objects
        found_objects = phase_e.get("found_objects", [])
        phase_results["F_set_props"] = _phase_f_set_properties(found_objects)

        # Phase G — save
        saved = _phase_g_save()
        phase_results["G_save"] = "PASS" if saved else "PARTIAL"

        _guard_production_untouched("POST")

        # Summary
        unreal.log("--- T3 Phase Summary ---")
        for phase, result in phase_results.items():
            if isinstance(result, dict):
                status = result.get("status", "UNKNOWN")
                notes = result.get("notes", [])
                unreal.log(f"  {phase}: {status}")
                for note in notes[:3]:  # limit to 3 notes per phase in summary
                    unreal.log(f"    • {note}")
            else:
                unreal.log(f"  {phase}: {result}")

        # Determine overall result
        statuses = []
        for result in phase_results.values():
            if isinstance(result, dict):
                statuses.append(result.get("status", "FAIL"))
            else:
                statuses.append(str(result))

        if all(s == "PASS" for s in statuses if s != "SKIP"):
            overall = "PASS"
        elif "FAIL" in statuses and not any(s in ("PASS", "PARTIAL") for s in statuses):
            overall = "FAIL"
        else:
            overall = "PARTIAL"

        unreal.log(f"=== RESULT: {overall} — T3 Template Clone Property Edit Spike complete ===")
        unreal.log(f"Sandbox clone saved at: {TARGET_PATH}")
        unreal.log("Human review required before staging or committing any files.")

    except Exception as e:
        unreal.log_error(f"=== RESULT: BLOCKED — {e} ===")

    finally:
        unreal.log("=== WTBR T3 Template Clone Property Edit Spike — END ===")
        try:
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).close_editor()
        except Exception:
            pass  # UE5.1.1: close_editor not available; auto-shuts via -unattended


if __name__ == "__main__" or True:
    run_spike()
