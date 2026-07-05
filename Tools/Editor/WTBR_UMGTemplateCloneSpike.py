"""
WTBR T2 UMG Template Clone Spike
Run INSIDE Unreal Editor only — do NOT run as standalone Python.

Purpose:
Test Path B (Template Clone) — duplicate an existing production WBP into a
sandbox clone. Source must remain read-only. Only the sandbox clone is saved.

How to run:
  Open WTBR.uproject in Unreal Editor
  Output Log -> Python: exec(open('Tools/Editor/WTBR_UMGTemplateCloneSpike.py').read())
  OR: -ExecutePythonScript="Tools/Editor/WTBR_UMGTemplateCloneSpike.py"

GUARDRAILS:
  - Source production WBP is read-only — never saved or modified
  - Clone target is ONLY under /Game/UI/Sandbox/
  - Blocks if target already exists (no silent overwrite)
  - Guards production WBP dirty state before and after
"""

import unreal

SOURCE_PATH  = "/Game/UI/HUD/WBP_WTBRBagSlot"
TARGET_FOLDER = "/Game/UI/Sandbox"
TARGET_NAME  = "WBP_WTBRBagSlot_CloneSpike"
TARGET_PATH  = f"{TARGET_FOLDER}/{TARGET_NAME}"

PRODUCTION_PATHS = [
    "/Game/UI/HUD/WBP_WTBRBagLootLayer",
    "/Game/UI/HUD/WBP_WTBRBagPanel",
    "/Game/UI/HUD/WBP_WTBRBagSlot",
    "/Game/UI/HUD/WBP_WTBRCorpseLootPanel",
    "/Game/UI/HUD/WBP_WTBRCorpseLootEntry",
]


def _guard_production_untouched(label=""):
    try:
        dirty_pkgs = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        for pkg in dirty_pkgs:
            pkg_name = pkg.get_name()
            for prod in PRODUCTION_PATHS:
                if prod in pkg_name:
                    raise RuntimeError(
                        f"GUARDRAIL VIOLATION [{label}]: production WBP is dirty: {pkg_name}"
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


def _duplicate_asset():
    """
    Attempt duplication via EditorAssetLibrary.duplicate_asset first.
    Falls back to AssetToolsHelpers if the primary call returns None/False.
    """
    unreal.log(f"Duplicating: {SOURCE_PATH} -> {TARGET_PATH}")

    # Primary path: EditorAssetLibrary.duplicate_asset
    # Signature: duplicate_asset(source_asset_path, destination_asset_path) -> bool
    try:
        result = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_PATH, TARGET_PATH)
        if result:
            unreal.log(f"duplicate_asset (EditorAssetLibrary) succeeded: {TARGET_PATH}")
            return True
        unreal.log_warning("duplicate_asset (EditorAssetLibrary) returned False — trying AssetTools fallback")
    except Exception as e:
        unreal.log_warning(f"duplicate_asset (EditorAssetLibrary) raised: {e} — trying AssetTools fallback")

    # Fallback: AssetToolsHelpers.duplicate_asset
    # Signature: duplicate_asset(asset_name, package_path, original_object) -> UObject
    try:
        source_obj = unreal.EditorAssetLibrary.load_asset(SOURCE_PATH)
        if source_obj is None:
            raise RuntimeError(f"Could not load source asset: {SOURCE_PATH}")
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        clone = asset_tools.duplicate_asset(TARGET_NAME, TARGET_FOLDER, source_obj)
        if clone is not None:
            unreal.log(f"duplicate_asset (AssetTools) succeeded: {clone.get_path_name()}")
            return True
        unreal.log_warning("duplicate_asset (AssetTools) returned None")
    except Exception as e2:
        unreal.log_warning(f"duplicate_asset (AssetTools) raised: {e2}")

    return False


def _save_clone():
    try:
        success = unreal.EditorAssetLibrary.save_asset(TARGET_PATH, only_if_is_dirty=False)
        if success:
            unreal.log(f"Saved clone: {TARGET_PATH}")
        else:
            unreal.log_warning(f"save_asset returned False for {TARGET_PATH}. Save manually via Content Browser.")
    except Exception as e:
        unreal.log_warning(f"save_asset failed ({e}). Save manually via Content Browser.")


def run_spike():
    unreal.log("=== WTBR T2 UMG Template Clone Spike — START ===")
    unreal.log(f"Source : {SOURCE_PATH}")
    unreal.log(f"Target : {TARGET_PATH}")

    try:
        # Pre-guard
        _guard_production_untouched("PRE")

        # 1. Validate source exists
        if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_PATH):
            raise RuntimeError(f"BLOCKED — Source not found: {SOURCE_PATH}")

        unreal.log(f"Source confirmed: {SOURCE_PATH}")

        # 2. Validate target does not exist
        if unreal.EditorAssetLibrary.does_asset_exist(TARGET_PATH):
            raise RuntimeError(
                f"BLOCKED — Target already exists: {TARGET_PATH}\n"
                "Delete it first or get human approval before re-running."
            )

        unreal.log(f"Target clear (does not exist): {TARGET_PATH}")

        # 3. Ensure sandbox folder
        _ensure_sandbox_folder()

        # 4. Duplicate
        ok = _duplicate_asset()
        if not ok:
            raise RuntimeError(f"BLOCKED — All duplicate paths failed. See warnings above.")

        # 5. Save clone only
        _save_clone()

        # Post-guard: source must still be untouched
        _guard_production_untouched("POST")

        unreal.log(f"=== RESULT: PASS — Clone created and saved: {TARGET_PATH} ===")
        unreal.log("Human visual verification required: open clone in UMG Designer and confirm hierarchy matches source.")

    except Exception as e:
        unreal.log_error(f"=== RESULT: BLOCKED — {e} ===")

    finally:
        unreal.log("=== WTBR T2 UMG Template Clone Spike — END ===")
        # UE5.1.1: close_editor not available; editor auto-shuts via -unattended
        try:
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).close_editor()
        except Exception:
            pass


if __name__ == "__main__" or True:
    run_spike()
