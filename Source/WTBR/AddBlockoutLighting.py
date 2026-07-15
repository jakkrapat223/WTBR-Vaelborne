"""Add the daytime lighting actors required by the 15-player blockout map."""

import unreal

MAP_PATH = "/Game/Maps/Map_15P_Blockout"


def find_existing_actor(class_name):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() == class_name:
            return actor
    return None


def actor_name(actor):
    label = actor.get_actor_label()
    return label if label else actor.get_name()


def configure_sun(actor):
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    actor.set_actor_rotation(unreal.Rotator(-50.0, -35.0, 0.0), False)


def configure_sky_light(actor):
    component = actor.get_component_by_class(unreal.SkyLightComponent)
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("real_time_capture", True)


def configure_post_process(actor):
    actor.set_editor_property("unbound", True)
    settings = actor.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 1.0)
    settings.set_editor_property("auto_exposure_max_brightness", 1.0)
    actor.set_editor_property("settings", settings)


def add_or_skip(actor_subsystem, actor_class, class_name, label, configure, added, skipped):
    existing_actor = find_existing_actor(class_name)
    if existing_actor:
        skipped.append("{} ({})".format(actor_name(existing_actor), class_name))
        return

    actor = actor_subsystem.spawn_actor_from_class(actor_class, unreal.Vector(0.0, 0.0, 0.0))
    if not actor:
        raise RuntimeError("Could not spawn {}".format(actor_class.__name__))

    actor.set_actor_label(label)
    if configure:
        configure(actor)
    added.append("{} ({})".format(label, class_name))


if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
    raise RuntimeError("Could not load {}".format(MAP_PATH))

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
added_actors = []
skipped_actors = []

add_or_skip(actor_subsystem, unreal.DirectionalLight, "DirectionalLight", "Sun_15P", configure_sun,
            added_actors, skipped_actors)
add_or_skip(actor_subsystem, unreal.SkyLight, "SkyLight", "SkyLight_15P", configure_sky_light,
            added_actors, skipped_actors)
add_or_skip(actor_subsystem, unreal.SkyAtmosphere, "SkyAtmosphere", "SkyAtmosphere_15P", None,
            added_actors, skipped_actors)
add_or_skip(actor_subsystem, unreal.ExponentialHeightFog, "ExponentialHeightFog", "Fog_15P", None,
            added_actors, skipped_actors)
add_or_skip(actor_subsystem, unreal.PostProcessVolume, "PostProcessVolume", "PostProcess_15P", configure_post_process,
            added_actors, skipped_actors)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save {}".format(MAP_PATH))

unreal.log("Map_15P_Blockout lighting saved. Added: {}. Skipped: {}.".format(
    ", ".join(added_actors) if added_actors else "none",
    ", ".join(skipped_actors) if skipped_actors else "none"))
