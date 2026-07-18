"""Headless smoke test for the Niagara Preview PNG console command."""

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(
    world,
    "WTBR.Niagara.ExportPreviewPng /Game/VFX/Sniper/NS_Telorn_Impact",
)
