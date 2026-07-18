"""Create the physical-material assets referenced by the VFX surface routing.

Assignments to individual environment meshes remain deliberately separate:
the owning level/material author chooses whether a surface is Metal, Stone,
Flesh, or Shield instead of a batch tool guessing gameplay semantics.
"""
import unreal

ROOT = "/Game/Physics/VFX"
SURFACES = [
    ("PM_VFX_Metal", "SURFACE_TYPE1"),
    ("PM_VFX_Stone", "SURFACE_TYPE2"),
    ("PM_VFX_Flesh", "SURFACE_TYPE3"),
    ("PM_VFX_Shield", "SURFACE_TYPE4"),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, surface_name in SURFACES:
    path = ROOT + "/" + name
    asset = unreal.EditorAssetLibrary.load_asset(path) \
        if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        asset = asset_tools.create_asset(
            name, ROOT, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
    if not asset:
        unreal.log_error("Could not create physical material: " + path)
        continue
    surface_type = getattr(unreal.PhysicalSurface, surface_name)
    asset.set_editor_property("surface_type", surface_type)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log("VFX physical material ready: {} -> {}".format(path, surface_type))
