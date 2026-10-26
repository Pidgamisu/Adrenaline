"""
Creates the two Blueprints that let movement be tuned in the editor and saved:

  /Game/Blueprints/BP_Character        - child of AGetawayCharacter, the pawn you tune
  /Game/Blueprints/BP_GetawayGameMode  - child of AGetawayGameMode, spawns BP_Character

Run it from the editor: Tools > Execute Python Script..., pick this file.

Safe to run twice: existing assets are reused, not replaced, so tuning you have already
done to BP_Character survives. It only ever writes BP_GetawayGameMode's Default Pawn Class.

It deliberately does NOT touch Project Settings or any map - pointing the project and
TestCourse at BP_GetawayGameMode is done by hand, because a Python script that saves a
.umap is a bigger hammer than the three clicks it replaces. See docs/blueprint-tuning.md.
"""

import unreal

FOLDER = "/Game/Blueprints"
CHARACTER_BP = "BP_Character"
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
    gamemode_parent = native_class("GetawayGameMode")
    if character_parent is None or gamemode_parent is None:
        return

    character_bp = make_or_load(CHARACTER_BP, character_parent)
    gamemode_bp = make_or_load(GAMEMODE_BP, gamemode_parent)
    if character_bp is None or gamemode_bp is None:
        return

    character_class = generated_class(character_bp, CHARACTER_BP)
    if character_class is None:
        fail("could not find %s_C. Set Default Pawn Class by hand instead." % CHARACTER_BP)
        return

    # Blueprint defaults live on the class default object, so setting it there and saving the
    # asset is what makes the choice stick.
    gamemode_class = generated_class(gamemode_bp, GAMEMODE_BP)
    defaults = unreal.get_default_object(gamemode_class)
    defaults.set_editor_property("default_pawn_class", character_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(gamemode_bp)
    assets.save_loaded_asset(gamemode_bp)

    # Read it back rather than trust the write, so a silent failure is not reported as success.
    written = unreal.get_default_object(gamemode_class).get_editor_property("default_pawn_class")
    if written and CHARACTER_BP in str(written):
        log("%s Default Pawn Class is now %s. Done." % (GAMEMODE_BP, written.get_name()))
        log("Next, by hand: Project Settings > Maps & Modes > Default GameMode, and "
            "TestCourse > World Settings > GameMode Override. Both to %s." % GAMEMODE_BP)
    else:
        fail("Default Pawn Class did not take (it reads as %r). Set it by hand in %s."
             % (written, GAMEMODE_BP))


run()
