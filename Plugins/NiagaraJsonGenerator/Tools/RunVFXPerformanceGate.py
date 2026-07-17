"""Emit static Niagara audit summaries for the VFX CI gate.

The PowerShell wrapper runs this in UnrealEditor-Cmd and evaluates the logged
counts against Source/WTBR/VFXJson/VFXPerformanceGate.json.
"""
import json
import os
import unreal

manifest = os.path.join(
    unreal.Paths.project_dir(), "Source", "WTBR", "VFXJson", "VFXPerformanceGate.json")
with open(manifest, "r", encoding="utf-8") as handle:
    spec = json.load(handle)

world = unreal.EditorLevelLibrary.get_editor_world()
unreal.log("=== WTBR VFX performance gate: begin ===")
for asset in spec["assets"]:
    unreal.SystemLibrary.execute_console_command(world, "WTBR.Niagara.AuditSystem " + asset)
unreal.log("=== WTBR VFX performance gate: end ===")
