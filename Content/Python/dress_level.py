"""
Puts the unlit look on the level that is open: turns the lighting off and repaints every static
mesh already standing in the map.

Run it from the editor: Tools > Execute Python Script..., pick this file. Run vaultborne_look.py
first, or there will be no materials to hand out.

Why it is needed at all. dress_pieces.py dresses the eleven piece *Blueprints*, which covers every
piece spawned during the placement phase. It does not cover the course itself - TestCourse was
built by hand before those Blueprints existed, so its geometry is plain AGetawayPiece actors and
engine defaults, all of them lit. Lit plus no lights equals black, so without this the first Play
is a black void with a few pink pieces floating in it.

**Nothing here is saved.** The script changes the level in memory and stops. Look around, and
then either Ctrl+S to keep it or close the map without saving to throw it all away. That is the
undo, and it is the reason the script does not delete anything.

Walls versus floors are guessed from the shape of each actor: taller than it is wide reads as a
wall, anything else as a floor. It is a first coat, not a decision - repaint individual actors in
the Details panel afterwards, and this script will leave them alone next run, because it skips
anything already wearing an MI_VB_ material.
"""

import unreal

MATERIALS = "/Game/Materials"

FLOOR = "MI_VB_Floor"
WALL = "MI_VB_Wall"

# The wireframe triangle edges, as an Overlay Material. None since the edge band arrived, for the
# same reason as in dress_pieces.py: the band already outlines every face.
WIRE = None

# The actors that make the world black when they stop working. Lights are dimmed to zero rather
# than deleted; the rest are hidden.
LIGHTING = (
    unreal.SkyAtmosphere,
    unreal.ExponentialHeightFog,
    unreal.VolumetricCloud,
)

assets = unreal.EditorAssetLibrary

_problems = []


def log(message):
    unreal.log("[Vaultborne level] " + message)


def fail(message):
    _problems.append(message)
    unreal.log_error("[Vaultborne level] " + message)


def material(name):
    path = "%s/%s" % (MATERIALS, name)
    if not assets.does_asset_exist(path):
        fail("%s is missing. Run vaultborne_look.py first." % path)
        return None
    return assets.load_asset(path)


def douse_lighting(actors):
    """Lights to zero intensity, sky and fog hidden. Reversible by not saving."""
    touched = 0
    for actor in actors:
        if isinstance(actor, unreal.Light):
            # Intensity rather than hiding: it reads the same in the viewport and in Play, where
            # hiding an actor only affects Play and leaves the editor looking different.
            # get_components_by_class, plural, because the singular GetComponentByClass is native
            # only in UE 5.8 and never reaches Python.
            for component in actor.get_components_by_class(unreal.LightComponent):
                component.set_intensity(0.0)
                touched += 1
                log("dimmed %s to zero." % actor.get_actor_label())
            continue

        if isinstance(actor, LIGHTING) or "sky" in actor.get_actor_label().lower():
            actor.set_actor_hidden_in_game(True)
            try:
                actor.set_is_temporarily_hidden_in_editor(True)
            except Exception:
                # Editor-only and not always exposed. Play will still be black; the viewport may
                # keep showing a sky until the map is reopened.
                pass
            touched += 1
            log("hid %s." % actor.get_actor_label())

    if touched == 0:
        log("no lights, sky or fog found in this level - it may already be dark.")
    return touched


def looks_like_a_wall(actor):
    """Taller than it is wide. Crude, but it separates uprights from platforms in one pass."""
    try:
        _, extent = actor.get_actor_bounds(False)
    except Exception:
        return False
    return extent.z > min(extent.x, extent.y) * 1.5


def repaint(actors, floor, wall, wire):
    """One coat of paint on every static mesh standing in the level."""
    painted = 0
    skipped = 0
    for actor in actors:
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        if not components:
            continue

        chosen = wall if looks_like_a_wall(actor) else floor

        for component in components:
            count = component.get_num_materials()
            if count == 0:
                continue

            # The overlay is written on every component, including ones whose colour is left
            # alone below. It has to be: WIRE going to None is how the wireframe is turned off,
            # and skipping an already-dressed component would leave a stale overlay stuck on it
            # forever with no way to clear it. That is exactly what happened on 2026-11-02 -
            # the wireframe was switched off in both scripts and the level kept drawing it.
            try:
                component.set_editor_property("overlay_material", wire)
            except Exception as error:
                fail("%s would not take an overlay material (%s)." % (actor.get_actor_label(),
                                                                      error))

            # The colour, though, is left alone if it was chosen on purpose - by an earlier run,
            # or by hand in the Details panel afterwards.
            current = component.get_material(0)
            if current is not None and current.get_name().startswith("MI_VB_"):
                skipped += 1
                continue

            for slot in range(count):
                component.set_material(slot, chosen)
            painted += 1

    # "painted 0, left 17 alone" is the normal result of a second run, not a failure: the level
    # was dressed the first time and the actors still point at the same MI_VB_ assets, so they
    # pick up any change to the masters on their own. Nothing needs repainting to see new colours.
    log("painted %d mesh component(s), left %d colour(s) alone that were already dressed, and "
        "set the overlay on all %d." % (painted, skipped, painted + skipped))
    if painted == 0 and skipped > 0:
        log("Everything here already had its colour from an earlier run, which is why nothing "
            "was repainted. Those actors follow the masters, so they are already showing the "
            "current look.")
    return painted


def run():
    floor = material(FLOOR)
    wall = material(WALL)
    wire = material(WIRE) if WIRE else None
    if floor is None or wall is None:
        fail("Stopping: the palette is not there yet.")
        return

    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()
    log("looking at %d actors in the open level." % len(actors))

    douse_lighting(actors)
    repaint(actors, floor, wall, wire)

    if _problems:
        fail("Finished with %d problem(s), listed above." % len(_problems))
    log("Nothing has been saved. Press Play to look at it. Ctrl+S keeps it; closing the map "
        "without saving throws it away.")


run()
