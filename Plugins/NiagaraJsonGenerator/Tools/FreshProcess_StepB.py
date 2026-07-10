"""Fresh-process regression test — Step B (run in its OWN separate UnrealEditor-Cmd
process, launched after Step A's process has fully exited).

This is the process that reproduces the original bug: it has never seen
/Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test before, so its own Asset
Registry has not scanned that package yet even though Step A wrote it to disk.
The importer must still detect it via FPackageName::DoesPackageExist and take
the update-in-place path — logging "Updated", never "Created"/"Duplicated" —
and must leave the SentinelMarker param (not present in this JSON) untouched.
See Tools/RunFreshProcessRegressionTest.ps1.
"""
import unreal

OUTPUT = "/Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test"

_plugins_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir())
_examples = _plugins_dir + "NiagaraJsonGenerator/Examples/"


def _cmd(command):
    unreal.SystemLibrary.execute_console_command(None, command)


unreal.log("=== FreshProcess regression Step B: begin ===")
_cmd("WTBR.Niagara.ImportJson " + _examples + "NS_FreshProcess_B_UpdateOnly.json")
_cmd("WTBR.Niagara.DumpUserParams " + OUTPUT)
unreal.log("=== FreshProcess regression Step B: end ===")
