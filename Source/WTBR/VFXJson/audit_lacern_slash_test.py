"""Headless Lacern slash VFX audit: dumps params and static Niagara cost."""

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

for command in (
    "WTBR.Niagara.DumpUserParams /Game/VFX/Templates/NS_Base_SlashTrail",
    "WTBR.Niagara.DumpUserParams /Game/VFX/Generated/Lacern/NS_Lacern_Slash_VaelCyan_01",
    "WTBR.Niagara.AuditSystem /Game/VFX/Generated/Lacern/NS_Lacern_Slash_VaelCyan_01",
):
    unreal.log("WTBR Lacern VFX audit command: {}".format(command))
    unreal.SystemLibrary.execute_console_command(world, command)
