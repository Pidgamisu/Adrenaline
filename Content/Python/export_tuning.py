"""
Read-only. Prints what the assets actually hold, so it can be compared against the scripts and the
README - the values in a .uasset are invisible to everything except the editor, and that is how a
hand-set value quietly drifts from what the docs claim.

  1. Every MI_VB_* material instance's parameters, with CHANGED marked against adrenaline_look.py's
     PALETTE.
  2. Every piece Blueprint's material *properties* (SurfaceMaterial, ScrapMaterial, PointMaterial,
     BarMaterial, BladeMaterial, ArmMaterial), with CHANGED marked against dress_pieces.py's PIECES.
  3. Every property on BP_Character, its movement component and BP_GetawayGameMode that differs from
     the C++ class default - which is the list of everything tuned in the editor - plus the catalog.

Two ways to run it. In the editor: Tools > Execute Python Script..., then read the Output Log.
Headless, with the editor closed, which is usually easier:

  "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
    "<path>\\Getaway.uproject" -run=pythonscript -Script="<path>\\Content\\Python\\export_tuning.py" ^
    -unattended -nopause -nosplash -abslog="<somewhere>\\export.log"

Changes nothing, saves nothing, compiles nothing.

**It does not report component materials.** An earlier version did, and it was wrong: a cold-loaded
class default object does not show the material overrides a Blueprint puts on its components, so it
reported Floor, Wall, Ledge, Crate and Panel as wearing the raw master materials when they are not.
The material *properties* below are plain UPROPERTYs on the CDO and do read correctly. To check a
component's material, open the Blueprint and look at Components > Mesh > Materials > Element 0.

Added 2026-11-05.
"""

import unreal

MATERIALS = "/Game/Materials"
PIECES_FOLDER = "/Game/Blueprints/Pieces"
CHARACTER_PATH = "/Game/Blueprints/BP_Character"
GAMEMODE_PATH = "/Game/Blueprints/BP_GetawayGameMode"

assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary

EDGE_PINK = (1.00, 0.35, 0.68)
EDGE_WHITE = (1.00, 0.88, 0.95)
EDGE_CYAN = (0.40, 1.00, 0.95)
EDGE_LIME = (0.80, 1.00, 0.35)

# A snapshot of adrenaline_look.py's PALETTE. Keep it in step with that file, or the CHANGED marks
# here will cry wolf.
#   name: (master, color, edge, brightness, {scalar overrides})
EXPECTED_PALETTE = {
    "MI_VB_Floor":  ("M_VB_Solid", (0.14, 0.01, 0.07), EDGE_PINK, 1.0, {}),
    "MI_VB_Ledge":  ("M_VB_Solid", (0.22, 0.02, 0.11), EDGE_PINK, 1.1, {"BorderWidth": 10.0}),
    "MI_VB_Ramp":   ("M_VB_Solid", (0.18, 0.01, 0.09), EDGE_PINK, 1.0, {"BandMode": 1.0}),
    "MI_VB_Bridge": ("M_VB_Solid", (0.14, 0.01, 0.07), EDGE_PINK, 1.0, {"BorderWidth": 10.0}),
    "MI_VB_Dark":   ("M_VB_Solid", (0.04, 0.00, 0.02), (0.45, 0.12, 0.28), 1.0, {"BorderWidth": 5.0}),
    "MI_VB_Wall":   ("M_VB_Pane",  (0.30, 0.02, 0.14), EDGE_PINK, 1.0, {}),
    "MI_VB_Panel":  ("M_VB_Pane",  (0.45, 0.03, 0.20), EDGE_WHITE, 1.3, {"FaceOpacity": 0.10}),
    "MI_VB_Crate":  ("M_VB_Pane",  (0.50, 0.10, 0.28), EDGE_WHITE, 1.2, {"BorderWidth": 8.0}),
    "MI_VB_Vortex": ("M_VB_Pane",  (0.05, 0.30, 0.28), EDGE_CYAN, 1.6, {"FaceOpacity": 0.04}),
    "MI_VB_Pickup": ("M_VB_Pane",  (0.05, 0.35, 0.32), EDGE_CYAN, 1.8, {"BorderWidth": 5.0}),
    "MI_VB_Danger": ("M_VB_Pane",  (0.054, 0.006, 0.400), EDGE_LIME, 1.8, {"FaceOpacity": 0.30,
                                                                           "CellSize": 40.0,
                                                                           "BorderWidth": 4.0}),
    "MI_VB_Player": ("M_VB_Pane",  (0.40, 0.22, 0.34), EDGE_WHITE, 1.4, {"BorderWidth": 4.0}),
    "MI_VB_Ghost":  ("M_VB_Pane",  (0.09, 0.19, 0.21), EDGE_CYAN, 1.5, {"FaceOpacity": 0.04,
                                                                        "BorderWidth": 6.0,
                                                                        "GridStrength": 0.0}),
    "MI_VB_Wire":   ("M_VB_Wire",  (1.00, 0.70, 0.86), None, 1.5, {}),
}

# A snapshot of the material *properties* in dress_pieces.py's PIECES.
EXPECTED_PROPERTIES = {
    "BP_Piece_Panel":  {"scrap_material": "MI_VB_Panel"},
    "BP_Piece_Crate":  {"scrap_material": "MI_VB_Crate"},
    "BP_Piece_Ramp":   {"surface_material": "MI_VB_Ramp"},
    "BP_Piece_Vortex": {"bar_material": "MI_VB_Vortex"},
    "BP_Piece_Windmill": {"blade_material": "MI_VB_Ledge", "arm_material": "MI_VB_Dark"},
    "BP_Piece_Spikes": {},  # hand-dressed on purpose; the dress script leaves it alone
}

PROPERTIES = ["surface_material", "scrap_material", "point_material", "bar_material",
              "blade_material", "arm_material"]

# Property names that hold object references or engine bookkeeping rather than tuning.
SKIP = ("component", "class", "template", "blueprint", "guid", "uber", "graph")


def log(message=""):
    unreal.log("[export] " + message)


def short(obj):
    if obj is None:
        return "None"
    try:
        return obj.get_name()
    except Exception:
        return str(obj)


def close(a, b, tolerance=0.005):
    try:
        return abs(float(a) - float(b)) <= tolerance
    except Exception:
        return False


def defaults_of(blueprint, asset_name, folder):
    """The class default object of a Blueprint, or None."""
    cls = None
    try:
        cls = blueprint.generated_class()
    except Exception:
        pass
    if cls is None:
        try:
            cls = unreal.load_object(None, "%s/%s.%s_C" % (folder, asset_name, asset_name))
        except Exception:
            return None
    if cls is None:
        return None
    return unreal.get_default_object(cls)


def report_materials():
    log()
    log("---------- 1. the material instances ----------")
    for name in sorted(EXPECTED_PALETTE.keys()):
        path = "%s/%s" % (MATERIALS, name)
        if not assets.does_asset_exist(path):
            log("%s: MISSING from %s" % (name, MATERIALS))
            continue
        instance = assets.load_asset(path)
        expected_master, exp_rgb, exp_edge, exp_bright, exp_overrides = EXPECTED_PALETTE[name]

        try:
            parent = instance.get_editor_property("parent")
        except Exception:
            parent = None

        bits = []
        changed = []
        if short(parent) != expected_master:
            changed.append("parent %s -> %s" % (expected_master, short(parent)))

        vectors = {}
        try:
            for param in edit.get_vector_parameter_names(instance):
                value = edit.get_material_instance_vector_parameter_value(instance, param)
                vectors[str(param)] = (value.r, value.g, value.b)
        except Exception as error:
            log("%s: could not read vector parameters (%s)" % (name, error))

        scalars = {}
        try:
            for param in edit.get_scalar_parameter_names(instance):
                scalars[str(param)] = edit.get_material_instance_scalar_parameter_value(instance, param)
        except Exception as error:
            log("%s: could not read scalar parameters (%s)" % (name, error))

        colour = vectors.get("Color")
        if colour:
            bits.append("Color (%.3f, %.3f, %.3f)" % colour)
            if not all(close(colour[i], exp_rgb[i]) for i in range(3)):
                changed.append("Color (%.3f, %.3f, %.3f) -> (%.3f, %.3f, %.3f)"
                               % (exp_rgb + colour))
        edge = vectors.get("EdgeColor")
        if edge:
            bits.append("EdgeColor (%.3f, %.3f, %.3f)" % edge)
            if exp_edge and not all(close(edge[i], exp_edge[i]) for i in range(3)):
                changed.append("EdgeColor (%.3f, %.3f, %.3f) -> (%.3f, %.3f, %.3f)"
                               % (exp_edge + edge))

        for param in sorted(scalars.keys()):
            value = scalars[param]
            bits.append("%s %.3f" % (param, value))
            if param == "Brightness" and not close(value, exp_bright):
                changed.append("Brightness %.2f -> %.3f" % (exp_bright, value))
            elif param in exp_overrides and not close(value, exp_overrides[param]):
                changed.append("%s %.2f -> %.3f" % (param, exp_overrides[param], value))

        log("%-14s parent %-12s %s" % (name, short(parent), "  ".join(bits)))
        for line in changed:
            log("%-14s   CHANGED: %s" % ("", line))


def report_piece_properties():
    log()
    log("---------- 2. the piece Blueprints' material properties ----------")
    log("(component materials are NOT reported - see the note at the top of this file)")
    found = assets.list_assets(PIECES_FOLDER, recursive=False) \
        if assets.does_directory_exist(PIECES_FOLDER) else []
    for asset_name in sorted({p.split("/")[-1].split(".")[0] for p in found}):
        path = "%s/%s" % (PIECES_FOLDER, asset_name)
        cdo = defaults_of(assets.load_asset(path), asset_name, PIECES_FOLDER)
        if cdo is None:
            log("%s: could not reach its class defaults" % asset_name)
            continue
        expected = EXPECTED_PROPERTIES.get(asset_name, {})
        lines = []
        for property_name in PROPERTIES:
            try:
                value = short(cdo.get_editor_property(property_name))
            except Exception:
                continue
            mark = ""
            if property_name in expected and value != expected[property_name]:
                mark = "   CHANGED from %s" % expected[property_name]
            lines.append("    %-18s = %-16s%s" % (property_name, value, mark))
        if lines:
            log(asset_name)
            for line in lines:
                log(line)


def interesting(value):
    """Scalars, bools, strings and small structs. Not object references."""
    text = repr(value)
    if text.startswith("<") or "0x" in text:
        return False
    return len(text) < 120


def diff_against_native(cdo, native_cdo, label):
    log()
    log("%s - properties that differ from the C++ default:" % label)
    found = 0
    for name in sorted(dir(cdo)):
        if name.startswith("_") or any(word in name for word in SKIP):
            continue
        try:
            mine = cdo.get_editor_property(name)
        except Exception:
            continue
        if callable(mine) or not interesting(mine):
            continue
        try:
            theirs = native_cdo.get_editor_property(name)
        except Exception:
            continue
        if repr(mine) != repr(theirs):
            log("    %-34s %s   (C++ default %s)" % (name, repr(mine), repr(theirs)))
            found += 1
    if found == 0:
        log("    nothing - every value matches the C++ default")


def report_tuning():
    log()
    log("---------- 3. what has been tuned in the editor ----------")

    if assets.does_asset_exist(CHARACTER_PATH):
        cdo = defaults_of(assets.load_asset(CHARACTER_PATH), "BP_Character", "/Game/Blueprints")
        native = unreal.get_default_object(unreal.GetawayCharacter)
        if cdo and native:
            diff_against_native(cdo, native, "BP_Character")
            try:
                mine_move = cdo.get_editor_property("character_movement")
                their_move = native.get_editor_property("character_movement")
                if mine_move and their_move:
                    diff_against_native(mine_move, their_move, "BP_Character's movement component")
            except Exception as error:
                log("    could not reach the movement component (%s)" % error)
        else:
            log("BP_Character: could not reach its class defaults")

    if assets.does_asset_exist(GAMEMODE_PATH):
        cdo = defaults_of(assets.load_asset(GAMEMODE_PATH), "BP_GetawayGameMode", "/Game/Blueprints")
        native = unreal.get_default_object(unreal.GetawayGameMode)
        if cdo and native:
            diff_against_native(cdo, native, "BP_GetawayGameMode")
            try:
                catalog = cdo.get_editor_property("catalog")
                log()
                log("BP_GetawayGameMode's Catalog: %d entries (rebuilt from C++ by dress_pieces.py)"
                    % len(catalog))
                for entry in catalog:
                    size = entry.get_editor_property("size")
                    log("    %-9s %-22s %.0f x %.0f x %.0f" % (
                        str(entry.get_editor_property("id")),
                        short(entry.get_editor_property("actor_class")),
                        size.x, size.y, size.z))
            except Exception as error:
                log("    could not read the Catalog (%s)" % error)
        else:
            log("BP_GetawayGameMode: could not reach its class defaults")


log("======================== START ========================")
report_materials()
report_piece_properties()
report_tuning()
log()
log("======================== END - nothing was changed ========================")
