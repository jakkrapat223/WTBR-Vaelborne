"""Installs the curated melee library and creates a first Lacern Slash variant."""

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "WTBR.Niagara.InstallMeleeTemplates")
unreal.SystemLibrary.execute_console_command(
    world,
    "WTBR.Niagara.GenerateLacernSlash /Game/VFX/Generated/Lacern/NS_Lacern_Slash_VaelCyan_01",
)
unreal.SystemLibrary.execute_console_command(
    world,
    "WTBR.Niagara.AuditSystem /Game/VFX/Generated/Lacern/NS_Lacern_Slash_VaelCyan_01",
)
