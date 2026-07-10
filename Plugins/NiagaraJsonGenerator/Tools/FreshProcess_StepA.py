"""Fresh-process regression test — Step A (run in its own UnrealEditor-Cmd process).

Creates /Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test from the template
and plants a sentinel User Parameter (SentinelMarker) that Step B's JSON never
mentions. Step A's process then exits completely — nothing about this asset
survives in memory once it does. See Tools/RunFreshProcessRegressionTest.ps1.
"""
import unreal

OUTPUT = "/Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test"

_plugins_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir())
_examples = _plugins_dir + "NiagaraJsonGenerator/Examples/"


def _cmd(command):
    unreal.SystemLibrary.execute_console_command(None, command)


unreal.log("=== FreshProcess regression Step A: begin ===")
_cmd("WTBR.Niagara.ImportJson " + _examples + "NS_FreshProcess_A_CreateWithSentinel.json")
_cmd("WTBR.Niagara.DumpUserParams " + OUTPUT)
unreal.log("=== FreshProcess regression Step A: end ===")
