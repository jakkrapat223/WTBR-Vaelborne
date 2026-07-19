"""Create the non-destructive Voltis P01 Niagara prototype family."""

import os
import unreal

root = unreal.Paths.project_dir()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
spec_dir = os.path.join(root, "Source", "WTBR", "VFXJson", "Voltis")
specs = (
    "NS_Voltis_TapPrism_P02.json",
    "NS_Voltis_HoldPrism_P02.json",
    "NS_Voltis_TrapPrism_P02.json",
    "NS_Voltis_TrapTriggerPrism_P02.json",
)

for spec in specs:
    unreal.SystemLibrary.execute_console_command(
        world, "WTBR.Niagara.ImportJson " + os.path.join(spec_dir, spec))
