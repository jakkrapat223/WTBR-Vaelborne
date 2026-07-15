"""Create the map-specific random-spawn Data Asset without overwriting it."""

import unreal

ASSET_PATH = "/Game/Data/DA_RandomSpawnConfig_15P_Blockout"

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
else:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WTBRRandomSpawnConfigDataAsset)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_RandomSpawnConfig_15P_Blockout",
        "/Game/Data",
        unreal.WTBRRandomSpawnConfigDataAsset,
        factory)
    if not asset:
        raise RuntimeError("Could not create {}".format(ASSET_PATH))

if not asset:
    raise RuntimeError("Could not load {}".format(ASSET_PATH))

anchors = [
    (-25000, -40000), (0, -40000), (25000, -40000),
    (-40000, -25000), (-25000, -25000), (0, -25000), (25000, -25000), (40000, -25000),
    (-40000, 0), (-25000, 0), (0, 0), (25000, 0), (40000, 0),
    (-25000, 25000), (0, 25000), (25000, 25000),
    (-25000, 40000), (0, 40000), (25000, 40000),
]
asset.set_editor_property("spawn_area_center", unreal.Vector(0.0, 0.0, 0.0))
asset.set_editor_property("spawn_area_radius", 46000.0)
asset.set_editor_property("min_spawn_distance", 6000.0)
asset.set_editor_property("safe_spawn_anchors", [unreal.Vector(x, y, 0.0) for x, y in anchors])
if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
    raise RuntimeError("Could not save {}".format(ASSET_PATH))
unreal.log("Saved {} with {} road-safe anchors.".format(ASSET_PATH, len(anchors)))
