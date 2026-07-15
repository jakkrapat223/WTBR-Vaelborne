"""Create the editor-ready 1 x 1 km 15-player combat blockout level.

Run with UnrealEditor-Cmd -ExecutePythonScript=<this file>. It only creates a
new level; it intentionally refuses to overwrite an existing map asset.
"""

import unreal

MAP_PATH = "/Game/Maps/Map_15P_Blockout"

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError("Refusing to overwrite existing map: {}".format(MAP_PATH))

if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
    raise RuntimeError("Could not create level: {}".format(MAP_PATH))

world = unreal.EditorLevelLibrary.get_editor_world()
world_settings = world.get_world_settings()
game_mode_class = unreal.load_class(None, "/Script/WTBR.WTBRTeamThree15PGameMode")
generator_class = unreal.load_class(None, "/Script/WTBR.WTBRCombatBlockoutGenerator")
nav_volume_class = unreal.load_class(None, "/Script/NavigationSystem.NavMeshBoundsVolume")

if not game_mode_class or not generator_class or not nav_volume_class:
    raise RuntimeError("Could not load the 15P GameMode, blockout generator, or NavMeshBoundsVolume class")

world_settings.set_editor_property("default_game_mode", game_mode_class)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

generator = actor_subsystem.spawn_actor_from_class(generator_class, unreal.Vector(0.0, 0.0, 0.0))
generator.set_actor_label("Blockout_1x1km_15P")

nav_bounds = actor_subsystem.spawn_actor_from_class(nav_volume_class, unreal.Vector(0.0, 0.0, 2500.0))
nav_bounds.set_actor_label("NavMesh_1x1km")
# A default volume brush is 200 units wide; scale it to cover the 100,000-unit
# square plus enough vertical room for the tower platforms.
nav_bounds.set_actor_scale3d(unreal.Vector(500.0, 500.0, 30.0))

player_start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, -1500.0, 200.0))
player_start.set_actor_label("PlayerStart_15P")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Map was created but could not be saved: {}".format(MAP_PATH))

unreal.log("Created {} with TeamThree15P GameMode, blockout generator, NavMesh bounds, and PlayerStart.".format(MAP_PATH))
