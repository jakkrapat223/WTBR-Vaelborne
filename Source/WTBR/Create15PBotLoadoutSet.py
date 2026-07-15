"""Create the approved Gunner / Sniper / Melee + Aegorn bot loadouts for 15P."""

import unreal

ASSET_PATH = "/Game/Data/DA_BotLoadoutSet_15P_Blockout"

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
else:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WTBRBotLoadoutSetDataAsset)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_BotLoadoutSet_15P_Blockout",
        "/Game/Data",
        unreal.WTBRBotLoadoutSetDataAsset,
        factory)
    if not asset:
        raise RuntimeError("Could not create {}".format(ASSET_PATH))

if not asset:
    raise RuntimeError("Could not load {}".format(ASSET_PATH))

aegorn = unreal.load_asset("/Game/Data/Triggers/DA_Aegorn")
if not aegorn:
    raise RuntimeError("Could not load DA_Aegorn")
entries = []
for role, main in [
    (unreal.WTBRBotCombatRole.GUNNER, "/Game/Data/Triggers/DA_Fulgrix.DA_Fulgrix"),
    (unreal.WTBRBotCombatRole.SNIPER, "/Game/Data/Triggers/DA_Fulgris.DA_Fulgris"),
    (unreal.WTBRBotCombatRole.MELEE, "/Game/Data/Triggers/DA_Feryx.DA_Feryx"),
]:
    entry = unreal.WTBRApprovedBotLoadout()
    entry.set_editor_property("role", role)
    main_asset = unreal.load_asset(main.rsplit(".", 1)[0])
    if not main_asset:
        raise RuntimeError("Could not load {}".format(main))
    entry.set_editor_property("main_trigger", main_asset)
    entry.set_editor_property("aegorn_sub_trigger", aegorn)
    entries.append(entry)

asset.set_editor_property("approved_loadouts", entries)
if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
    raise RuntimeError("Could not save {}".format(ASSET_PATH))
unreal.log("Saved {} with Fulgrix, Fulgris, Feryx, and locked Aegorn.".format(ASSET_PATH))
