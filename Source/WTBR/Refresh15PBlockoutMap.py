"""Regenerate the placed blockout actor and bake navigation for the 15P map."""

import unreal

MAP_PATH = "/Game/Maps/Map_15P_Blockout"

if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
    raise RuntimeError("Could not load {}".format(MAP_PATH))

generator = None
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name() == "WTBRCombatBlockoutGenerator":
        generator = actor
        break

if not generator:
    raise RuntimeError("Blockout generator is missing from {}".format(MAP_PATH))

generator.rebuild_blockout()
unreal.SystemLibrary.execute_console_command(
    unreal.EditorLevelLibrary.get_editor_world(),
    "RebuildNavigation")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save {}".format(MAP_PATH))

unreal.log("Refreshed blockout geometry and navigation for {}".format(MAP_PATH))
