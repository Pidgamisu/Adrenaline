"""
Gives every placeable piece its own Blueprint and its own material, and points the party box
at them.

  /Game/Blueprints/Pieces/BP_Piece_Floor, BP_Piece_Wall, ... one per catalog entry
  BP_GetawayGameMode's Catalog > Actor Class rewired to those Blueprints

Run it from the editor: Tools > Execute Python Script..., pick this file.

Why it is needed at all: the catalog's floor, wall and ledge entries all point at the same
C++ class, AGetawayPiece, so a material set on that class dresses all three. A Blueprint
child each is the only way to tell them apart. See *Dressing a piece: materials* in
docs/blueprint-tuning.md.

Safe to run twice. Blueprints that already exist are reused, not replaced, so a material you
have changed by hand survives - EXCEPT the ones named in PIECES below, which are written every
time. To keep a hand-picked material, clear it out of the table here, but leave the row: the
row is also what points the catalog entry at the Blueprint. The spikes are kept that way.

**Four pieces are hand-dressed, and this script must not touch them.** The panel, the jump pad,
the C4 pickup and the vortex were dressed in the editor on 2026-11-05 (commit 800940a), and
their rows are `{}, {}` for the same reason the spikes' row is. Their colours live in the
.uasset and nowhere else, so emptying the row is the only thing that protects them - the table
cannot hold the right value because nothing can read it back out (see export_tuning.py, which
deliberately does not report component materials: a cold-loaded class default object does not
show them). A fresh clone therefore gets those four undressed, which is the accepted cost.

**Why a run of this script appeared to reset them, on 2026-11-05.** The symptom was that those
four went back to their old colours in the editor, with nothing saved and nothing to revert.
`dress()` writes to the class default object, then compiles the Blueprint, then saves it. The
write does not dirty the package - that is the known bug recorded in docs/blueprint-tuning.md -
so the log says "All files are already saved" and the .uasset keeps its hand-set colours. But
the *compile* propagates the CDO into the running session, so the editor shows the table's
colours until it is closed and reopened. Nothing is lost; reopening is the whole fix. The rows
being empty is what stops it happening again.

It also rebuilds BP_GetawayGameMode's Catalog from AGetawayGameMode's class defaults each run,
which is the only way a piece added to the C++ catalog ever reaches the party box - a Blueprint
that has been written once stops tracking C++. Everything about a piece except its Actor Class
therefore lives in C++; editing size or flags in the Blueprint's Catalog array will not last.

**Which colour goes where is decided here; what each colour IS lives in adrenaline_look.py.**
Change the table below to move a colour onto a different piece, change adrenaline_look.py to
change the colour itself. Either way, re-run both. Logic stays in C++, the look lives in the
Blueprint; the script only puts the first coat on.

Needs BP_GetawayGameMode to exist. If it does not, run setup_tuning_blueprints.py first.
Needs adrenaline_look.py to have been run, or /Game/Materials will be empty.
"""

import unreal

PIECES_FOLDER = "/Game/Blueprints/Pieces"
GAMEMODE_PATH = "/Game/Blueprints/BP_GetawayGameMode"
MATERIALS = "/Game/Materials"

# The wireframe triangle edges, worn by every component as an Overlay Material.
#
# Off by default since the edge band arrived. The band already draws a line around every face, so
# the overlay's contribution is the diagonal across each one plus a second line on top of the
# band - noise, mostly. Put "MI_VB_Wire" back here to see it; the material is still built.
WIRE_OVERLAY = None

# One row per catalog entry, keyed on the Id in AGetawayGameMode's constructor.
#
#   catalog id, Blueprint name, C++ parent, {component: material}, {property: material}
#
# Components are the parts the Details panel can reach (Mesh, Base, Piston, Pad, Charge, Cap).
# Properties are the slots added 2026-11-02 for the parts it cannot: a procedural mesh with no
# section yet, and the pooled spikes and vortex bars.
#
# The palette is pink for architecture, cyan for anything that helps you, lime for anything that
# kills you. The colours themselves live in adrenaline_look.py, not here.
PIECES = [
    ("floor",    "BP_Piece_Floor",    "GetawayPiece",             {"mesh": "MI_VB_Floor"},    {}),
    ("wall",     "BP_Piece_Wall",     "GetawayPiece",             {"mesh": "MI_VB_Wall"},     {}),
    ("ledge",    "BP_Piece_Ledge",    "GetawayPiece",             {"mesh": "MI_VB_Ledge"},    {}),
    # Hand-dressed in the editor on 2026-11-05 (commit 800940a), so this script leaves it alone -
    # the same deal as the spikes below. See *Four pieces are hand-dressed* in this docstring.
    # Was: {"mesh": "MI_VB_Panel"}, {"scrap_material": "MI_VB_Panel"}.
    ("break",    "BP_Piece_Panel",    "GetawayBreakable",         {}, {}),
    ("crate",    "BP_Piece_Crate",    "GetawayBreakable",         {"mesh": "MI_VB_Crate"},
                                                                  {"scrap_material": "MI_VB_Crate"}),
    ("ramp",     "BP_Piece_Ramp",     "GetawayRamp",              {},
                                                                  {"surface_material": "MI_VB_Ramp"}),
    ("bridge",   "BP_Piece_Bridge",   "GetawayCollapsingBridge",  {"mesh": "MI_VB_Bridge"},   {}),
    # Hand-dressed 2026-11-05, left alone. Was: {"pad": "MI_VB_Dark", "charge": "MI_VB_Pickup"}, {}.
    ("c4",       "BP_Piece_C4Pickup", "GetawayC4Pickup",          {}, {}),
    # Dressed by hand, so this script leaves it alone: on 2026-11-03 a run of this script put the
    # table back over a look that had been set in the editor, and the row is empty so that cannot
    # happen again. The row itself has to stay, or the catalog would point the spikes at the raw
    # C++ class instead of this Blueprint. Was: {"base": "MI_VB_Dark"}, {"point_material":
    # "MI_VB_Danger"} - put those back here to hand the spikes to the script again.
    ("spikes",   "BP_Piece_Spikes",   "GetawaySpikes",            {}, {}),
    # Hand-dressed 2026-11-05, left alone. Was: {"base": "MI_VB_Dark", "piston": "MI_VB_Pickup"}, {}.
    ("jumppad",  "BP_Piece_JumpPad",  "GetawayJumpPad",           {}, {}),
    # Hand-dressed 2026-11-05, left alone. Was: {"base": "MI_VB_Dark", "cap": "MI_VB_Dark"},
    # {"bar_material": "MI_VB_Vortex"}.
    ("vortex",   "BP_Piece_Vortex",   "GetawayVortex",            {}, {}),
    # The plates are architecture you stand on, so pink, and the brighter of the two floor pinks
    # because they are moving and have to be read at a glance. The hub and the spokes are the dark
    # frame they hang off, which is also the one thing that visibly turns.
    ("windmill", "BP_Piece_Windmill", "GetawayWindmill",          {"hub": "MI_VB_Dark"},
                                                                  {"blade_material": "MI_VB_Ledge",
                                                                   "arm_material": "MI_VB_Dark"}),
    # Cyan, because the ring is purely something that helps you - the same material the C4
    # pickup's charge wears, which is the brightest thing in the palette. The rim is the only
    # part there is, and it is pooled, so it goes in by property rather than by component.
    ("speedring", "BP_Piece_SpeedRing", "GetawaySpeedRing",        {},
                                                                  {"ring_material": "MI_VB_Pickup"}),
    # The hook pad: the dark plinth the C4 pickup and the jump pad stand on, with the hook itself in
    # the brightest cyan there is, because it is the thing you are meant to run at. The hook's bend
    # is pooled, so it goes in by property like the vortex's bars.
    ("grapple",  "BP_Piece_GrapplePickup", "GetawayGrapplePickup", {"pad": "MI_VB_Dark"},
                                                                  {"hook_material": "MI_VB_Pickup"}),
]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

_material_cache = {}
_problems = []


def log(message):
    unreal.log("[Adrenaline dress] " + message)


def fail(message):
    _problems.append(message)
    unreal.log_error("[Adrenaline dress] " + message)


def native_class(name):
    """The plugin's C++ class, or None if the module did not load."""
    cls = getattr(unreal, name, None)
    if cls is None:
        fail("unreal.%s does not exist. Is the GetawayMovement plugin compiled and enabled?" % name)
    return cls


def material(name):
    """A material instance from the pack, loaded once."""
    if name in _material_cache:
        return _material_cache[name]
    path = "%s/%s" % (MATERIALS, name)
    loaded = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if loaded is None:
        fail("%s is not there. Has the pack moved? It should be under %s." % (path, MATERIALS))
    _material_cache[name] = loaded
    return loaded


def make_or_load(asset_name, parent):
    """Create a Blueprint child of `parent`, or return the one already there."""
    path = "%s/%s" % (PIECES_FOLDER, asset_name)
    if assets.does_asset_exist(path):
        log("%s already exists, reusing it." % asset_name)
        return assets.load_asset(path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    created = tools.create_asset(asset_name, PIECES_FOLDER, unreal.Blueprint, factory)
    if created is None:
        fail("could not create %s." % asset_name)
        return None
    log("created %s." % asset_name)
    return created


def generated_class(blueprint, asset_name, folder):
    """The runnable `_C` class of a Blueprint. Tries the cheap routes, then the asset path."""
    try:
        cls = blueprint.generated_class()
        if cls:
            return cls
    except Exception:
        pass
    try:
        cls = blueprint.get_editor_property("generated_class")
        if cls:
            return cls
    except Exception:
        pass
    return unreal.load_object(None, "%s/%s.%s_C" % (folder, asset_name, asset_name))


def dress(blueprint, asset_name, components, properties):
    """Put the materials on one Blueprint's defaults. Returns True if every one read back."""
    cls = generated_class(blueprint, asset_name, PIECES_FOLDER)
    if cls is None:
        fail("could not find %s_C, so it has not been dressed." % asset_name)
        return False

    # Blueprint defaults live on the class default object, the same place
    # setup_tuning_blueprints.py writes the game mode's pawn classes.
    defaults = unreal.get_default_object(cls)
    ok = True
    overlay = material(WIRE_OVERLAY) if WIRE_OVERLAY else None

    for component_name, material_name in sorted(components.items()):
        mat = material(material_name)
        if mat is None:
            ok = False
            continue
        try:
            component = defaults.get_editor_property(component_name)
        except Exception as error:
            fail("%s has no %s component (%s)." % (asset_name, component_name, error))
            ok = False
            continue
        if component is None:
            fail("%s's %s component is empty." % (asset_name, component_name))
            ok = False
            continue
        # set_material rather than writing OverrideMaterials: the engine header warns the array
        # must not be set directly.
        component.set_material(0, mat)

        # The edge lines: a second draw of the same mesh with a translucent wireframe material,
        # which is exactly what UMeshComponent::OverlayMaterial is for. Written unconditionally,
        # so clearing WIRE_OVERLAY takes the lines off rather than leaving the last one stuck on.
        try:
            component.set_editor_property("overlay_material", overlay)
        except Exception as error:
            fail("%s's %s would not take an overlay material (%s). The piece keeps its colour "
                 "and loses its edge lines." % (asset_name, component_name, error))
            ok = False

    for property_name, material_name in sorted(properties.items()):
        mat = material(material_name)
        if mat is None:
            ok = False
            continue
        try:
            defaults.set_editor_property(property_name, mat)
        except Exception as error:
            fail("%s has no %s property (%s). Is the C++ built?"
                 % (asset_name, property_name, error))
            ok = False

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assets.save_loaded_asset(blueprint)

    # Read back rather than trust the writes. Component materials on a Blueprint's defaults are
    # the part most likely to go quietly nowhere, so they are checked, not assumed.
    written = unreal.get_default_object(cls)
    for component_name, material_name in sorted(components.items()):
        component = written.get_editor_property(component_name)
        got = component.get_material(0) if component else None
        if got is None or got.get_name() != material_name:
            fail("%s's %s did not take (it reads as %s). Set Materials > Element 0 on it by "
                 "hand in the Components panel." % (asset_name, component_name,
                                                    got.get_name() if got else "nothing"))
            ok = False
    for property_name, material_name in sorted(properties.items()):
        got = written.get_editor_property(property_name)
        if got is None or got.get_name() != material_name:
            fail("%s's %s did not take (it reads as %s). Set it by hand in Class Defaults."
                 % (asset_name, property_name, got.get_name() if got else "nothing"))
            ok = False

    return ok


def rewire_catalog(built):
    """Point each catalog entry's Actor Class at the Blueprint made for it."""
    if not assets.does_asset_exist(GAMEMODE_PATH):
        fail("%s is not there. Run setup_tuning_blueprints.py first, then this again."
             % GAMEMODE_PATH)
        return

    gamemode_bp = assets.load_asset(GAMEMODE_PATH)
    gamemode_class = generated_class(gamemode_bp, "BP_GetawayGameMode", "/Game/Blueprints")
    if gamemode_class is None:
        fail("could not find BP_GetawayGameMode_C. Set Catalog > Actor Class by hand instead.")
        return

    defaults = unreal.get_default_object(gamemode_class)

    # **Read the catalog from C++, not from the Blueprint.** The moment this script writes the
    # array, BP_GetawayGameMode holds its own copy and stops tracking C++, so a piece added to
    # AGetawayGameMode later is simply not in it and is never dealt - which is exactly what
    # happened to the windmill on 2026-11-03. Reseeding from the class defaults every run means a
    # new piece reaches the party box as soon as the C++ is built, with nothing to click.
    #
    # Everything that decides what a piece IS - its id, size, and whether it is directional,
    # surface mounted or scaled - therefore lives in C++ and only in C++. The one thing this
    # script changes is Actor Class.
    native_gamemode = native_class("GetawayGameMode")
    catalog = (unreal.get_default_object(native_gamemode).get_editor_property("catalog")
               if native_gamemode is not None else None)
    if not catalog:
        fail("AGetawayGameMode's own Catalog came back empty, so there is nothing to rewire. "
             "Is the C++ built?")
        return

    # Matched on Id, not on position: an entry added to the C++ catalog later would shift every
    # index below it, and silently dressing the wrong piece is worse than dressing none.
    entries = []
    changed = 0
    seen = set()
    for source in catalog:
        # A copy. These structs belong to the C++ class defaults and must not be edited in place.
        entry = source.copy()
        entry_id = str(entry.get_editor_property("id"))
        seen.add(entry_id)
        if entry_id in built:
            entry.set_editor_property("actor_class", built[entry_id])
            changed += 1
        else:
            log("catalog entry '%s' has no Blueprint in this script; left as it is." % entry_id)
        entries.append(entry)

    for missing in sorted(set(built) - seen):
        fail("AGetawayGameMode's C++ Catalog has no '%s' entry, so that piece cannot be dealt. "
             "Add it to the catalog in AGetawayGameMode, build, and run this again - or take the "
             "row out of PIECES in this script." % missing)

    defaults.set_editor_property("catalog", entries)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    assets.save_loaded_asset(gamemode_bp)

    written = unreal.get_default_object(gamemode_class).get_editor_property("catalog")
    confirmed = 0
    for entry in written:
        entry_id = str(entry.get_editor_property("id"))
        if entry_id not in built:
            continue
        actor_class = entry.get_editor_property("actor_class")
        if actor_class == built[entry_id]:
            confirmed += 1
        else:
            fail("catalog entry '%s' still reads as %s. Set its Actor Class by hand."
                 % (entry_id, actor_class))
    log("Catalog: %d of %d entries now point at a Blueprint." % (confirmed, changed))


def run():
    if not assets.does_directory_exist(MATERIALS):
        fail("%s does not exist. Run adrenaline_look.py first - it builds the materials this "
             "script hands out - then run this again." % MATERIALS)
        return

    assets.make_directory(PIECES_FOLDER)

    built = {}
    for entry_id, asset_name, parent_name, components, properties in PIECES:
        parent = native_class(parent_name)
        if parent is None:
            continue
        blueprint = make_or_load(asset_name, parent)
        if blueprint is None:
            continue
        if dress(blueprint, asset_name, components, properties):
            log("dressed %s." % asset_name)
        cls = generated_class(blueprint, asset_name, PIECES_FOLDER)
        if cls is not None:
            built[entry_id] = cls

    rewire_catalog(built)

    if _problems:
        fail("Finished with %d problem(s), listed above. Everything else went in."
             % len(_problems))
    else:
        log("Done. %d pieces dressed and wired into the catalog." % len(built))
    log("Press Play and place a few pieces. To change a colour: edit the PIECES table in this "
        "script and run it again, or set it in the Blueprint's Details panel.")


run()
