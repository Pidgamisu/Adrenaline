"""
Puts the three mantle ledges back into /Game/Maps/TestCourse.

The course was built with ledges at 60 / 110 / 165 cm on the Floor_Mantle stretch, spanning
the MantleMinHeight 30 .. MantleMaxHeight 170 window, and docs/test-course.md still describes
them - but a dump of the map on 2026-11-01 found them gone. Floor_Mantle is 1900 cm of empty
floor with Panel_1 standing in the middle of it. This adds them again.

Run it from the editor: open /Game/Maps/TestCourse, then Tools > Execute Python Script...,
pick this file. Then look at the course and press Ctrl+S yourself if you like it.

It deliberately does NOT save the map. A Python script that writes a .umap is a bigger hammer
than one Ctrl+S, and if the numbers below are wrong you want to close without saving rather
than undo a commit. Same reasoning as setup_tuning_blueprints.py.

Safe to run twice: a ledge that is already there is moved and resized rather than duplicated,
so you can edit the table below, run it again, and watch them move.
"""

import unreal

MAP = "/Game/Maps/TestCourse"
CUBE = "/Engine/BasicShapes/Cube.Cube"
MATERIAL = "/Game/Maps/Materials/M_Course_Ledge.M_Course_Ledge"  # the green one, as RocketLedge uses

# name, X start, X end, top Z. They sit on Floor_Mantle, whose top face is Z = 0, so the
# bottom of each ledge is 0 and "top" is also its height.
#
# 300 cm deep along X: more than enough for the landing capsule (34 radius) to stand on top,
# which keeps these a test of the ordinary mantle rather than of the thin-ledge fallback added
# on 2026-11-01. Full 800 cm across Y, like the floor, so you cannot run around one.
#
# The gaps avoid the breakable panels, which are at X 4767.5..4792.5 and 5466.4..5491.4:
# 3700..3850 of run-up, then the three ledges, with 217 cm clear before Panel_1 and 166 cm
# after the last ledge before the stacked pair.
LEDGES = [
    ("Ledge_60", 3850.0, 4150.0, 60.0),
    ("Ledge_110", 4250.0, 4550.0, 110.0),
    ("Ledge_165", 5000.0, 5300.0, 165.0),
]

# Floor_Mantle spans Y -400..400 and the ledges match it.
Y_MIN = -400.0
Y_MAX = 400.0

CUBE_SIZE = 100.0  # the engine cube is 100 cm on a side, pivot in the middle


def log(message):
    unreal.log("[Vaultborne ledges] " + message)


def fail(message):
    unreal.log_error("[Vaultborne ledges] " + message)


def find_by_label(actors, label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if world is None:
        fail("no editor world. Open the map first.")
        return

    if MAP not in world.get_path_name():
        fail("the open map is %s, not %s. Open the test course and run this again."
             % (world.get_path_name(), MAP))
        return

    mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
    if mesh is None:
        fail("could not load %s." % CUBE)
        return
    if material is None:
        log("could not load %s, leaving the ledges grey." % MATERIAL)

    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = actor_sub.get_all_level_actors()

    # Put them in whatever outliner folder the floor they stand on lives in.
    folder = ""
    floor = find_by_label(existing, "Floor_Mantle")
    if floor is not None:
        folder = str(floor.get_folder_path())

    # Warn rather than refuse: the panels have been moved by hand before now, and if one has
    # been moved into a ledge you want to be told, not to have the script quietly decide.
    panels = [a for a in existing if "Breakable" in a.get_class().get_name()]

    for name, x0, x1, top in LEDGES:
        centre = unreal.Vector((x0 + x1) * 0.5, (Y_MIN + Y_MAX) * 0.5, top * 0.5)
        scale = unreal.Vector((x1 - x0) / CUBE_SIZE, (Y_MAX - Y_MIN) / CUBE_SIZE, top / CUBE_SIZE)

        actor = find_by_label(existing, name)
        if actor is None:
            actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, centre)
            if actor is None:
                fail("could not spawn %s." % name)
                continue
            actor.set_actor_label(name)
            if folder:
                actor.set_folder_path(folder)
            log("added %s." % name)
        else:
            actor.set_actor_location(centre, False, False)
            log("%s was already there; moved it." % name)

        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_static_mesh(mesh)
        if material is not None:
            component.set_material(0, material)
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        actor.set_actor_scale3d(scale)

        for p in panels:
            origin, extent = p.get_actor_bounds(False)
            if origin.x - extent.x < x1 and origin.x + extent.x > x0:
                log("careful: %s overlaps %s along X. Move one of them."
                    % (name, p.get_actor_label()))

        origin, extent = actor.get_actor_bounds(False)
        log("%s is now X %.1f..%.1f, Y %.1f..%.1f, Z %.1f..%.1f"
            % (name, origin.x - extent.x, origin.x + extent.x,
               origin.y - extent.y, origin.y + extent.y,
               origin.z - extent.z, origin.z + extent.z))

    log("done. Nothing is saved yet - look at the course, then Ctrl+S if you want to keep it.")


main()
