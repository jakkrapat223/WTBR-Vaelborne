"""Bind generated Sniper VFX to gameplay Data Assets, then log static audits."""
import unreal

MANIFEST = r"E:\World Trigger\WTBR-Vaelborne\Source\WTBR\VFXJson\SniperVFX.bind.json"

world = unreal.EditorLevelLibrary.get_editor_world()
commands = [
    "WTBR.Niagara.ImportBatch " + unreal.Paths.project_dir() + "Source/WTBR/VFXJson/Surfaces/SniperSurfaceImpacts.batch.json",
    "WTBR.Niagara.AutoBindSniper " + MANIFEST,
    "WTBR.Niagara.AutoBindProjectileBlueprints "
        + unreal.Paths.project_dir() + "Source/WTBR/VFXJson/SniperProjectileVFX.bind.json",
    "WTBR.Niagara.AuditSystem /Game/VFX/Sniper/NS_Telorn_Impact",
    "WTBR.Niagara.AuditSystem /Game/VFX/Sniper/NS_Piercex_Impact",
    "WTBR.Niagara.AuditSystem /Game/VFX/Sniper/NS_Fulgris_Impact",
]

for command in commands:
    unreal.log("WTBR VFX bind/audit command: {}".format(command))
    unreal.SystemLibrary.execute_console_command(world, command)
