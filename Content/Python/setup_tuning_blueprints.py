"""
Creates the Blueprints that let feel values be tuned in the editor and saved:

  /Game/Blueprints/BP_Character         - child of AGetawayCharacter, the pawn you tune
  /Game/Blueprints/BP_PlacementCamera   - child of AGetawayPlacementPawn, the fly camera
  /Game/Blueprints/BP_GetawayGameMode   - child of AGetawayGameMode, spawns both of the above

Run it from the editor: Tools > Execute Python Script..., pick this file.

Safe to run twice: existing assets are reused, not replaced, so tuning you have already
done survives. It only ever writes BP_GetawayGameMode's Default Pawn Class and Placement
Pawn Class.

It deliberately does NOT touch Project Settings or any map - pointing the project and
TestCourse at BP_GetawayGameMode is done by hand, because a Python script that saves a
.umap is a bigger hammer than the three clicks it replaces. See docs/blueprint-tuning.md.
"""

import unreal

FOLDER = "/Game/Blueprints"
CHARACTER_BP = "BP_Character"
PLACEMENT_BP = "BP_PlacementCamera"
GAMEMODE_BP = "BP_GetawayGameMode"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log("[Vaultborne setup] " + message)


def fail(message):
    unreal.log_error("[Vaultborne setup] " + message)


def native_class(name):
    """The plugin's C++ class, or None if the module did not load."""
    cls = getattr(unreal, name, None)
    if cls is None:
        fail("unreal.%s does not exist. Is the GetawayMovement plugin compiled and enabled?" % name)
    return cls


def make_or_load(asset_name, parent):
    """Create a Blueprint child of `parent`, or return the one already there."""
    path = "%s/%s" % (FOLDER, asset_name)
    if assets.does_asset_exist(path):
        log("%s already exists, leaving it alone." % asset_name)
        return assets.load_asset(path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    created = tools.create_asset(asset_name, FOLDER, unreal.Blueprint, factory)
    if created is None:
        fail("could not create %s." % asset_name)
        return None
    assets.save_loaded_asset(created)
    log("created %s." % asset_name)
    return created


def generated_class(blueprint, asset_name):
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
    return unreal.load_object(None, "%s/%s.%s_C" % (FOLDER, asset_name, asset_name))


def run():
    character_parent = native_class("GetawayCharacter")
    placement_parent = native_class("GetawayPlacementPawn")
    gamemode_parent = native_class("GetawayGameMode")
    if character_parent is None or placement_parent is None or gamemode_parent is None:
        return

    character_bp = make_or_load(CHARACTER_BP, character_parent)
    placement_bp = make_or_load(PLACEMENT_BP, placement_parent)
    gamemode_bp = make_or_load(GAMEMODE_BP, gamemode_parent)
    if character_bp is None or placement_bp is None or gamemode_bp is None:
        return

    # Which Blueprint class goes in which game mode field. Both are pawn classes the game mode
    # spawns: DefaultPawnClass for the runner, PlacementPawnClass for the fly camera.
    wiring = [
        ("default_pawn_class", character_bp, CHARACTER_BP),
        ("placement_pawn_class", placement_bp, PLACEMENT_BP),
    ]

    gamemode_class = generated_class(gamemode_bp, GAMEMODE_BP)
    if gamemode_class is None:
        fail("could not find %s_C. Set both pawn classes by hand instead." % GAMEMODE_BP)
        return

    # Blueprint defaults live on the class default object, so setting them there and saving the
    # asset is what makes the choice stick.
    defaults = unreal.get_default_object(gamemode_class)
    for prop, blueprint, asset_name in wiring:
        pawn_class = generated_class(blueprint, asset_name)
        if pawn_class is None:
            fail("could not find %s_C. Set %s by hand instead." % (asset_name, prop))
            return
        defaults.set_editor_property(prop, pawn_class)

    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    assets.save_loaded_asset(gamemode_bp)

    # Read them back rather than trust the writes, so a silent failure is not reported as success.
    written_defaults = unreal.get_default_object(gamemode_class)
    ok = True
    for prop, _blueprint, asset_name in wiring:
        written = written_defaults.get_editor_property(prop)
        if written and asset_name in str(written):
            log("%s %s is now %s." % (GAMEMODE_BP, prop, written.get_name()))
        else:
            ok = False
            fail("%s did not take (it reads as %r). Set it by hand in %s."
                 % (prop, written, GAMEMODE_BP))

    if not ok:
        return

    log("Done.")
    log("Next, by hand: Project Settings > Maps & Modes > Default GameMode, and "
        "TestCourse > World Settings > GameMode Override. Both to %s." % GAMEMODE_BP)


run()
