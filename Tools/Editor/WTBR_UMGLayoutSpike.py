"""
WTBR T1 UMG Layout Tooling Spike
Run INSIDE Unreal Editor only — do NOT run as standalone Python.

How to run:
  1. Enable PythonScriptPlugin + EditorScriptingUtilities in WTBR.uproject (see WTBR_T1_UMG_LAYOUT_TOOLING_SPIKE.md Section 10)
  2. Open WTBR.uproject in Unreal Editor
  3. Window -> Developer Tools -> Output Log -> Python tab
     OR: python console (py:) in Output Log
  4. Type: exec(open('Tools/Editor/WTBR_UMGLayoutSpike.py').read())
     OR launch with: -ExecutePythonScript="Tools/Editor/WTBR_UMGLayoutSpike.py"

GUARDRAILS (enforced by this script):
  - ONLY creates assets under /Game/UI/Sandbox/
  - Does NOT touch Content/UI/HUD/ production WBPs
  - Does NOT create fake .uasset files
  - Does NOT add Event Graph logic
  - Does NOT import textures/sounds/VFX
"""

import unreal

SANDBOX_FOLDER = "/Game/UI/Sandbox"
ASSET_NAME = "WBP_WTBRToolingSpike_Test"
ASSET_FULL_PATH = f"{SANDBOX_FOLDER}/{ASSET_NAME}"

PRODUCTION_PATHS = [
    "/Game/UI/HUD/WBP_WTBRBagLootLayer",
    "/Game/UI/HUD/WBP_WTBRBagPanel",
    "/Game/UI/HUD/WBP_WTBRBagSlot",
    "/Game/UI/HUD/WBP_WTBRCorpseLootPanel",
    "/Game/UI/HUD/WBP_WTBRCorpseLootEntry",
]


def _guard_production_untouched():
    """Abort if any production asset was modified during this run."""
    for path in PRODUCTION_PATHS:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
            for pkg in dirty:
                if path in pkg.get_name():
                    raise RuntimeError(
                        f"GUARDRAIL VIOLATION: production WBP is dirty: {path}"
                    )
    unreal.log("GUARDRAIL: production WBPs untouched OK")


def _ensure_sandbox_folder():
    if not unreal.EditorAssetLibrary.does_directory_exist(SANDBOX_FOLDER):
        unreal.EditorAssetLibrary.make_directory(SANDBOX_FOLDER)
        unreal.log(f"Created folder: {SANDBOX_FOLDER}")
    else:
        unreal.log(f"Folder already exists: {SANDBOX_FOLDER}")


def _create_sandbox_wbp():
    """Create the sandbox Widget Blueprint asset."""
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_FULL_PATH):
        unreal.log_warning(
            f"Asset already exists: {ASSET_FULL_PATH} — delete it first if you want to recreate."
        )
        return unreal.EditorAssetLibrary.load_asset(ASSET_FULL_PATH)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("ParentClass", unreal.UserWidget)

    widget_bp = asset_tools.create_asset(
        ASSET_NAME,
        SANDBOX_FOLDER,
        unreal.WidgetBlueprint,
        factory,
    )

    if widget_bp is None:
        raise RuntimeError(f"Failed to create Widget Blueprint at {ASSET_FULL_PATH}")

    unreal.log(f"Created Widget Blueprint: {widget_bp.get_path_name()}")
    return widget_bp


def _attempt_widget_tree(widget_bp):
    """
    Attempt to add a minimal widget tree.
    UE5.1.1 LIMITATION: WidgetTree structural editing via Python is not fully
    supported. This function attempts the known path and logs the result.
    If it fails, the WBP asset is still created (empty root) and the user
    should populate it manually in UMG Designer using the JSON spec as reference.
    """
    unreal.log("Attempting WidgetTree population (UE5.1.1 limitation may apply)...")

    try:
        widget_tree = widget_bp.get_editor_property("widget_tree")
        if widget_tree is None:
            unreal.log_warning(
                "widget_tree property is None — UE5.1.1 does not expose WidgetTree "
                "via Python on this build. WBP created without hierarchy. "
                "Use UMG Designer manually with WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json as reference."
            )
            return False

        root = widget_tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
        if root is None:
            unreal.log_warning(
                "WidgetTree.construct_widget returned None for CanvasPanel. "
                "Widget hierarchy not populated. Use UMG Designer manually."
            )
            return False

        widget_tree.set_editor_property("root_widget", root)
        unreal.log("Root CanvasPanel created: RootCanvas")

        border = widget_tree.construct_widget(unreal.Border, "Panel_Test")
        if border is not None:
            canvas_slot = root.add_child_to_canvas(border)
            if canvas_slot:
                canvas_slot.set_editor_property(
                    "anchors",
                    unreal.Anchors(minimum=unreal.Vector2D(0.5, 0.5), maximum=unreal.Vector2D(0.5, 0.5)),
                )
                canvas_slot.set_editor_property("alignment", unreal.Vector2D(0.5, 0.5))
                canvas_slot.set_editor_property("size", unreal.Vector2D(500.0, 300.0))
            unreal.log("Border created: Panel_Test")

            text_block = widget_tree.construct_widget(unreal.TextBlock, "Text_Title")
            if text_block is not None:
                text_block.set_editor_property("text", "UMG Tooling Spike")
                brush = unreal.SlateFontInfo()
                brush.set_editor_property("size", 24)
                text_block.set_editor_property("font", brush)
                color_style = unreal.SlateColor()
                color_style.set_editor_property(
                    "specified_color",
                    unreal.LinearColor(r=0.0, g=0.85, b=0.85, a=1.0),
                )
                text_block.set_editor_property("color_and_opacity", color_style)
                border.set_content(text_block)
                unreal.log("TextBlock created: Text_Title")

        return True

    except Exception as e:
        unreal.log_warning(
            f"WidgetTree population failed: {e}\n"
            "This is expected in UE5.1.1 — the WBP asset was still created. "
            "Populate the widget hierarchy manually in UMG Designer."
        )
        return False


def _save_asset(widget_bp):
    path = widget_bp.get_path_name()
    try:
        success = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        if success:
            unreal.log(f"Saved: {path}")
        else:
            unreal.log_warning(f"save_asset returned False for {path}. Save manually via Content Browser.")
    except Exception as e:
        unreal.log_warning(f"save_asset failed ({e}). Trying save_packages fallback...")
        try:
            unreal.EditorLoadingAndSavingUtils.save_packages(
                [widget_bp.get_outer()], only_if_is_dirty=False
            )
            unreal.log(f"Saved via save_packages: {path}")
        except Exception as e2:
            unreal.log_warning(f"save_packages also failed ({e2}). Save manually via Content Browser.")


def run_spike():
    unreal.log("=== WTBR T1 UMG Layout Tooling Spike — START ===")
    unreal.log(f"Target: {ASSET_FULL_PATH}")

    try:
        _guard_production_untouched()
        _ensure_sandbox_folder()
        widget_bp = _create_sandbox_wbp()
        tree_ok = _attempt_widget_tree(widget_bp)
        _save_asset(widget_bp)
        _guard_production_untouched()

        if tree_ok:
            unreal.log("=== RESULT: PASS — WBP created with widget tree ===")
        else:
            unreal.log(
                "=== RESULT: PARTIAL — WBP created (empty root), widget tree not populated. "
                "Use UMG Designer with Docs/WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json as reference. ==="
            )

        unreal.log(f"Asset at: {ASSET_FULL_PATH}")

    except Exception as e:
        unreal.log_error(f"=== RESULT: BLOCKED — Script raised exception: {e} ===")

    finally:
        unreal.log("=== WTBR T1 UMG Layout Tooling Spike — END ===")
        # Request editor close for headless / -ExecutePythonScript runs
        try:
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).close_editor()
        except Exception as e:
            unreal.log(f"Auto-close not available ({e}) — close editor manually if running interactively.")


if __name__ == "__main__" or True:
    run_spike()
