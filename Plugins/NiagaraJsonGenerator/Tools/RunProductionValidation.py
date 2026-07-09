"""Production template validation for NiagaraJsonGenerator (spike follow-up).

Run headless:
  UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript
      -script="<this file>" -stdout -unattended -nosplash -nullrhi

Or in-editor from the Output Log's Python bar:  py "<this file>"

Requires the human-authored template /Game/VFX/Templates/NS_Template_Burst
(see plugin README, "One-Time Template Setup"). Aborts cleanly if missing.
"""
import unreal

TEMPLATE = "/Game/VFX/Templates/NS_Template_Burst"
OUTPUT = "/Game/VFX/Generated/NS_Test_Burst_FromJson"

_plugins_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir())
_examples = _plugins_dir + "NiagaraJsonGenerator/Examples/"


def _cmd(command):
    unreal.SystemLibrary.execute_console_command(None, command)


unreal.log("=== NiagaraJson production validation: begin ===")

if not unreal.EditorAssetLibrary.does_asset_exist(TEMPLATE):
    unreal.log_error(
        "VALIDATION ABORT: template not found: " + TEMPLATE
        + " - create it first (README: One-Time Template Setup), then re-run.")
else:
    if unreal.EditorAssetLibrary.does_asset_exist(OUTPUT):
        unreal.log("Deleting stale output asset for a clean round-1 run: " + OUTPUT)
        unreal.EditorAssetLibrary.delete_asset(OUTPUT)

    unreal.log("--- ROUND 1: create from template ---")
    _cmd("WTBR.Niagara.ImportJson " + _examples + "NS_Test_Burst_FromJson.json")
    _cmd("WTBR.Niagara.DumpUserParams " + OUTPUT)

    unreal.log("--- ROUND 2: update in place with changed values ---")
    _cmd("WTBR.Niagara.ImportJson " + _examples + "NS_Test_Burst_FromJson_Round2.json")
    _cmd("WTBR.Niagara.DumpUserParams " + OUTPUT)

    unreal.log("--- NEGATIVE: unknown param must warn+skip, not be created ---")
    _cmd("WTBR.Niagara.ImportJson " + _examples + "Invalid/NS_Invalid_MissingParam.json")
    _cmd("WTBR.Niagara.DumpUserParams " + OUTPUT)

    unreal.log("=== NiagaraJson production validation: end ===")
