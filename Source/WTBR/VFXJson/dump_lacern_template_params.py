"""Logs exposed Niagara parameters for the existing Lacern VFX family."""

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for asset_path in (
    "/Game/VFX/Lacern/NS_Lacern_Slash",
    "/Game/VFX/Lacern/NS_Lacern_Extend",
    "/Game/VFX/Lacern/NS_Lacern_Hit_Impact_01",
):
    unreal.SystemLibrary.execute_console_command(
        world, "WTBR.Niagara.DumpUserParams " + asset_path
    )
