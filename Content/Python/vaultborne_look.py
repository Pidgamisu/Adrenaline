"""
Vaultborne's look: a black world, see-through faces, pink colour around every edge.

Builds the master materials and the palette of instances the pieces wear:

  /Game/Materials/M_VB_Solid   unlit, opaque     - things you land on. Edge band on a filled face
  /Game/Materials/M_VB_Pane    unlit, additive   - things you see through. Edge band, hollow face
  /Game/Materials/M_VB_Wire    unlit, wireframe  - the triangle edges, worn as an Overlay Material
  /Game/Materials/MI_VB_*      one instance per colour in the palette below

Run it from the editor: Tools > Execute Python Script..., pick this file.
Then run dress_pieces.py, and dress_level.py on the open map.

## The edge band

Each face of a piece is see-through in the middle and solid colour around its rim. That band is
the whole look, and everything else here is in service of it.

**`BorderWidth` is in centimetres**, and 8 means 8 on every piece. It is measured from the
piece's world bounding box rather than from its UVs, which is the whole point: the pieces are
boxes scaled to whatever size the catalog asks for, so a band defined as a fraction of a face
came out as a 6 cm frame on a crate and a 120 cm slab on a floor. See build_world_band.

The exception is the ramp. The band is measured against a bounding box, which is exactly the
mesh for a box at any multiple of 90 degrees - every piece placed normally, since placement
rotates in quarter turns - but a wedge is not a box, so its sloped face would only band along two
of its four edges. `MI_VB_Ramp` therefore sets `BandMode` to 1, which switches it to the older
UV-based band: proportional rather than constant, but it follows the mesh's own faces. Every
piece has per-face 0-to-1 UVs, so that path works anywhere if you want it.

Inside the band, the face is nearly transparent, with a faint world-position grid across it. The
grid is set from `CellSize` in centimetres and holds its real-world size however a piece is
scaled, so it doubles as a measuring stick: lines converging into the distance say how far away a
ledge is, lines streaming past the screen edge say how fast you are moving. `GridStrength` at 0
turns it off and leaves the band on its own.

A Fresnel rim was tried in the pass before this one and dropped. On a flat box face the normal is
constant, so Fresnel is very nearly constant across the whole face - it dims and brightens whole
faces as you turn, which is not an edge and not what the reference shows.

## Why additive

M_VB_Pane never darkens what is behind it, only adds. Two things follow. Overlapping geometry
accumulates, so a stack of panes is visibly brighter than one, which is depth you read at a
glance. And additive blending is order-independent, so unlike ordinary translucency there is no
per-object sorting to get wrong - in a level made of dozens of overlapping see-through faces,
ordinary translucency pops and flickers as you run.

The cost is that an additive surface can never occlude. Anything on M_VB_Pane is see-through
forever, so panes are for walls, breakables and glow, never for a floor you are judging a landing
on. M_VB_Solid is opaque for that reason, and it still gets the band, so the two read as one
family.

## Tuning

Every number is a parameter, so it can also be changed live in the Details panel of any MI_VB_
instance. The ones worth reaching for first:

  EdgeColor      the colour around the edges. The one you will touch most
  BorderWidth    band thickness in centimetres, the same on every piece. 8 is a tidy frame
  BandMode       0 measures the band in centimetres, 1 measures it as a fraction of the face
  BorderWidthUV  the fraction BandMode 1 uses. Only the ramp is on that path by default
  FaceOpacity    panes only: how much of the hollow middle you can see. 0 is fully see-through
  GridStrength   0 removes the interior grid entirely
  CellSize       grid cell in cm. 100 reads as one metre
  FaceGlow       how lit the middle of a face is
  Brightness     multiplies the lot. Above 1 is what blooms

Safe to run twice, and running it twice is the point: the masters are rebuilt from scratch and
the instances rewritten, so editing a number here and running again is how the look is tuned. A
node added to a master by hand will NOT survive - tune through the parameters instead.
"""

import unreal

FOLDER = "/Game/Materials"

SOLID = "M_VB_Solid"
PANE = "M_VB_Pane"
WIRE = "M_VB_Wire"

# Superseded. Removed if they are lying around from an earlier run.
RETIRED = ["M_VB_Surface", "M_VB_Glow"]

# Edge colours. The band carries the hue; the face behind it stays dark, which is what lets you
# see through it.
EDGE_PINK = (1.00, 0.35, 0.68)
EDGE_WHITE = (1.00, 0.88, 0.95)
EDGE_CYAN = (0.40, 1.00, 0.95)
EDGE_LIME = (0.80, 1.00, 0.35)

# name, master, face colour, edge colour, brightness, parameter overrides
#
# The rule: pink is architecture, cyan means it helps you, lime means it kills you. Solid is for
# anything you have to judge a landing on; everything else can be a pane.
PALETTE = [
    ("MI_VB_Floor",  SOLID, (0.14, 0.01, 0.07), EDGE_PINK,  1.0, {}),
    ("MI_VB_Ledge",  SOLID, (0.22, 0.02, 0.11), EDGE_PINK,  1.1, {"BorderWidth": 10.0}),
    # The wedge is not a box, so it measures its band in UVs. See build_border.
    ("MI_VB_Ramp",   SOLID, (0.18, 0.01, 0.09), EDGE_PINK,  1.0, {"BandMode": 1.0}),
    ("MI_VB_Bridge", SOLID, (0.14, 0.01, 0.07), EDGE_PINK,  1.0, {"BorderWidth": 10.0}),
    ("MI_VB_Dark",   SOLID, (0.04, 0.00, 0.02), (0.45, 0.12, 0.28), 1.0, {"BorderWidth": 5.0}),

    ("MI_VB_Wall",   PANE,  (0.30, 0.02, 0.14), EDGE_PINK,  1.0, {}),
    ("MI_VB_Panel",  PANE,  (0.45, 0.03, 0.20), EDGE_WHITE, 1.3, {"FaceOpacity": 0.10}),
    ("MI_VB_Crate",  PANE,  (0.50, 0.10, 0.28), EDGE_WHITE, 1.2, {}),
    ("MI_VB_Vortex", PANE,  (0.05, 0.30, 0.28), EDGE_CYAN,  1.6, {"FaceOpacity": 0.04}),
    ("MI_VB_Pickup", PANE,  (0.05, 0.35, 0.32), EDGE_CYAN,  1.8, {"BorderWidth": 5.0}),
    ("MI_VB_Danger", PANE,  (0.25, 0.40, 0.04), EDGE_LIME,  1.8, {"FaceOpacity": 0.30,
                                                                 "CellSize": 40.0,
                                                                 "BorderWidth": 4.0}),
    ("MI_VB_Player", PANE,  (0.40, 0.22, 0.34), EDGE_WHITE, 1.4, {"BorderWidth": 4.0}),

    # The placement ghost. A cursor rather than architecture, which is why it is allowed cyan under
    # the colour rule, and why it carries no interior grid - the world grid belongs to the level,
    # not to a box following your crosshair. AGetawayPlacementPawn makes two dynamic copies of this
    # (one for boxes, one with BandMode 1 for the ramp's wedge) and swaps Color and EdgeColor to red
    # where the piece cannot go, so these values are only what it starts from.
    ("MI_VB_Ghost",  PANE,  (0.09, 0.19, 0.21), EDGE_CYAN,  1.5, {"FaceOpacity": 0.04,
                                                                  "BorderWidth": 6.0,
                                                                  "GridStrength": 0.0}),

    ("MI_VB_Wire",   WIRE,  (1.00, 0.70, 0.86), None,       1.5, {}),
]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary

_problems = []


def log(message):
    unreal.log("[Vaultborne look] " + message)


def fail(message):
    _problems.append(message)
    unreal.log_error("[Vaultborne look] " + message)


def node(material, expression_class, x, y):
    return edit.create_material_expression(material, expression_class, x, y)


def wire(source, target, input_name="", output_name=""):
    """
    Plug one expression into another's input.

    **An unnamed input is addressed with an empty string, not with "Input".** The C++ member on
    Frac, Abs, OneMinus, Saturate and ComponentMask is called `Input`, but that is the variable
    name, not the pin name - those pins are drawn blank in the material editor and
    UMaterialExpression::GetInputName gives nothing back for them. MaterialEditingLibrary's
    GetExpressionInputByName only falls through to GetInput(0) when the name it is handed is
    None, so passing "Input" matches nothing and the connection silently does not happen. That
    is what left every master black on 2026-11-02; the named pins (A, B, Alpha, Min, Max, Value,
    ExponentIn) were all fine.

    The retry below means a wrong pin name degrades to the first input instead of to nothing.
    """
    if edit.connect_material_expressions(source, output_name, target, input_name):
        return
    if input_name and edit.connect_material_expressions(source, output_name, target, ""):
        log("'%s' is not a pin on %s; used its first input instead."
            % (input_name, target.get_class().get_name()))
        return
    fail("could not connect into '%s' on %s."
         % (input_name or "the first input", target.get_class().get_name()))


def scalar(material, name, value, x, y):
    expression = node(material, unreal.MaterialExpressionScalarParameter, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", value)
    return expression


def vector(material, name, rgb, x, y):
    expression = node(material, unreal.MaterialExpressionVectorParameter, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value",
                                   unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return expression


def constant(material, value, x, y):
    expression = node(material, unreal.MaterialExpressionConstant, x, y)
    expression.set_editor_property("r", value)
    return expression


def channel(material, source, which, x, y):
    """One component of a vector, as a scalar."""
    mask = node(material, unreal.MaterialExpressionComponentMask, x, y)
    for name in ("r", "g", "b", "a"):
        mask.set_editor_property(name, name == which)
    wire(source, mask)
    return mask


def band_from_distance(material, to_edge, width, softness, x, y):
    """Shared tail: a distance to the nearest edge becomes a band, softened so it does not alias."""
    inner = node(material, unreal.MaterialExpressionSubtract, x, y + 200)
    wire(width, inner, "A")
    wire(softness, inner, "B")

    beyond = node(material, unreal.MaterialExpressionSmoothStep, x + 180, y)
    wire(inner, beyond, "Min")
    wire(width, beyond, "Max")
    wire(to_edge, beyond, "Value")

    band = node(material, unreal.MaterialExpressionOneMinus, x + 330, y)
    wire(beyond, band)
    return band


def build_world_band(material):
    """
    The edge band, measured in centimetres. Returns a scalar: 1 inside the band, 0 in the middle.

    This is the default, and the reason it exists: a band defined as a fraction of a face is a
    different thickness on every piece, because the pieces are boxes scaled to whatever size the
    catalog asks for. A 1 m crate and a 20 m floor would get a 6 cm frame and a 120 cm slab from
    the same number. BorderWidth here is centimetres, and 8 means 8 on everything.

    How it works. ObjectPositionWS and ObjectBounds give the centre and the half extents of the
    piece's world-space bounding box - ObjectBounds is FBoxSphereBounds::BoxExtent, so it is half
    extents, checked in PrimitiveUniformShaderParametersBuilder.h. Subtracting the pixel's
    distance from the centre leaves the distance, in centimetres, to each of the three pairs of
    box faces.

    One of those three is the face we are standing on, where the distance is zero everywhere - it
    would swamp the minimum and paint the whole face. VertexNormalWS says which one, so that axis
    is pushed out of contention by adding a number no piece is large enough to beat.

    **It measures to the bounding box, not to the mesh.** For a box at any multiple of 90 degrees
    those are the same thing, which covers every piece placed normally; placement rotates in
    quarter turns (AGetawayPlacementPawn::OnRotate). A piece surface-mounted on a slope sits at an
    angle inside a larger box, so its band creeps inwards from the real edge. And the ramp is a
    wedge, not a box, so its sloped face only bands along two of its four edges - which is what
    BandMode and the UV band below are for.
    """
    width = scalar(material, "BorderWidth", 8.0, -2000, 780)
    softness = scalar(material, "BorderSoftness", 1.0, -2000, 860)

    position = node(material, unreal.MaterialExpressionWorldPosition, -2300, 300)
    centre = node(material, unreal.MaterialExpressionObjectPositionWS, -2300, 380)

    offset = node(material, unreal.MaterialExpressionSubtract, -2120, 330)
    wire(position, offset, "A")
    wire(centre, offset, "B")

    reach = node(material, unreal.MaterialExpressionAbs, -1980, 330)
    wire(offset, reach)

    extents = node(material, unreal.MaterialExpressionObjectBounds, -2300, 470)

    to_faces = node(material, unreal.MaterialExpressionSubtract, -1840, 400)
    wire(extents, to_faces, "A")
    wire(reach, to_faces, "B")

    normal = node(material, unreal.MaterialExpressionVertexNormalWS, -2300, 600)
    facing = node(material, unreal.MaterialExpressionAbs, -2120, 600)
    wire(normal, facing)

    # Larger than any piece will ever be, so the axis we are facing never wins the minimum.
    out_of_reach = constant(material, 1000000.0, -2120, 680)
    pushed = node(material, unreal.MaterialExpressionMultiply, -1980, 630)
    wire(facing, pushed, "A")
    wire(out_of_reach, pushed, "B")

    edges = node(material, unreal.MaterialExpressionAdd, -1700, 500)
    wire(to_faces, edges, "A")
    wire(pushed, edges, "B")

    along_x = channel(material, edges, "r", -1550, 420)
    along_y = channel(material, edges, "g", -1550, 500)
    along_z = channel(material, edges, "b", -1550, 580)

    closest_pair = node(material, unreal.MaterialExpressionMin, -1400, 460)
    wire(along_x, closest_pair, "A")
    wire(along_y, closest_pair, "B")

    to_edge = node(material, unreal.MaterialExpressionMin, -1260, 500)
    wire(closest_pair, to_edge, "A")
    wire(along_z, to_edge, "B")

    return band_from_distance(material, to_edge, width, softness, -1100, 500)


def build_uv_band(material):
    """
    The old proportional band, kept for the ramp. Returns a scalar, as above.

    How far is this pixel from the nearest edge of its face? In UV terms that is
    min(u, 1-u, v, 1-v), 0 on the edge and 0.5 dead centre. It is a fraction of the face rather
    than a distance, so it stretches with the piece - but it follows the mesh's own faces rather
    than a bounding box, which is why the wedge still wants it. Every piece has per-face 0-to-1
    UVs: the pieces use /Engine/BasicShapes/Cube, and AGetawayRamp::BuildWedge lays its own out
    the same way.
    """
    width = scalar(material, "BorderWidthUV", 0.06, -2000, 1180)
    softness = scalar(material, "BorderSoftnessUV", 0.01, -2000, 1260)

    uv = node(material, unreal.MaterialExpressionTextureCoordinate, -2000, 1000)
    flipped = node(material, unreal.MaterialExpressionOneMinus, -1800, 1050)
    wire(uv, flipped)

    nearest = node(material, unreal.MaterialExpressionMin, -1650, 1000)
    wire(uv, nearest, "A")
    wire(flipped, nearest, "B")

    along_u = channel(material, nearest, "r", -1500, 950)
    along_v = channel(material, nearest, "g", -1500, 1070)

    to_edge = node(material, unreal.MaterialExpressionMin, -1350, 1000)
    wire(along_u, to_edge, "A")
    wire(along_v, to_edge, "B")

    return band_from_distance(material, to_edge, width, softness, -1100, 1000)


def build_border(material):
    """
    The band the material actually uses: centimetres by default, the proportional one at
    BandMode 1. Both are built and crossfaded rather than switched, so one number on an instance
    picks between them with no static switch and no second master.
    """
    in_centimetres = build_world_band(material)
    proportional = build_uv_band(material)

    mode = scalar(material, "BandMode", 0.0, -800, 820)
    chosen = node(material, unreal.MaterialExpressionLinearInterpolate, -620, 700)
    wire(in_centimetres, chosen, "A")
    wire(proportional, chosen, "B")
    wire(mode, chosen, "Alpha")
    return chosen


def build_grid(material):
    """
    A faint world-aligned grid across the hollow middle. Returns a scalar, 0 on the face and
    GridStrength on a line.

    Built on world position rather than UVs on purpose. The pieces are box meshes scaled to
    whatever size the catalog asks for, so UVs stretch and a UV grid would stretch with them -
    a tall wall and a short one would show different-sized cells and the grid would stop being
    a measuring stick. World position does not stretch. (The band above accepts that trade the
    other way round, because a band has to follow the face's own edges.)

    The triplanar weighting is the (1 - |normal|) term: a floor faces +Z, so its Z lines are
    weighted out and its X and Y lines kept. A 45-degree ramp keeps a dimmer version of all three.
    """
    cell = scalar(material, "CellSize", 100.0, -2000, -500)
    line_width = scalar(material, "LineWidth", 0.03, -2000, -400)
    strength = scalar(material, "GridStrength", 0.35, -2000, -250)
    half = constant(material, 0.5, -2000, -320)

    position = node(material, unreal.MaterialExpressionWorldPosition, -2000, -700)

    cells = node(material, unreal.MaterialExpressionDivide, -1750, -650)
    wire(position, cells, "A")
    wire(cell, cells, "B")

    repeated = node(material, unreal.MaterialExpressionFrac, -1600, -650)
    wire(cells, repeated)

    centred = node(material, unreal.MaterialExpressionSubtract, -1450, -650)
    wire(repeated, centred, "A")
    wire(half, centred, "B")

    # 0 at the middle of a cell, 0.5 at its edge, per axis.
    distance = node(material, unreal.MaterialExpressionAbs, -1300, -650)
    wire(centred, distance)

    inner = node(material, unreal.MaterialExpressionSubtract, -1450, -380)
    wire(half, inner, "A")
    wire(line_width, inner, "B")

    lines = node(material, unreal.MaterialExpressionSmoothStep, -1150, -650)
    wire(inner, lines, "Min")
    wire(half, lines, "Max")
    wire(distance, lines, "Value")

    normal = node(material, unreal.MaterialExpressionVertexNormalWS, -1450, -150)
    facing = node(material, unreal.MaterialExpressionAbs, -1300, -150)
    wire(normal, facing)
    across = node(material, unreal.MaterialExpressionOneMinus, -1150, -150)
    wire(facing, across)

    weighted = node(material, unreal.MaterialExpressionMultiply, -950, -400)
    wire(lines, weighted, "A")
    wire(across, weighted, "B")

    x_lines = channel(material, weighted, "r", -800, -550)
    y_lines = channel(material, weighted, "g", -800, -430)
    z_lines = channel(material, weighted, "b", -800, -310)

    strongest = node(material, unreal.MaterialExpressionMax, -650, -490)
    wire(x_lines, strongest, "A")
    wire(y_lines, strongest, "B")

    grid = node(material, unreal.MaterialExpressionMax, -520, -420)
    wire(strongest, grid, "A")
    wire(z_lines, grid, "B")

    faint = node(material, unreal.MaterialExpressionMultiply, -380, -420)
    wire(grid, faint, "A")
    wire(strength, faint, "B")
    return faint


def build_lit_material(name, blend_mode, two_sided, face_glow, with_opacity, face_opacity):
    """
    Emissive lerps from the face colour to the edge colour wherever there is ink - ink being the
    band and the grid together. Opacity, on the masters that have one, is FaceOpacity plus that
    same ink, so a face is see-through in the middle and solid around its rim.
    """
    material = fresh_material(name)
    if material is None:
        return None

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", blend_mode)
    material.set_editor_property("two_sided", two_sided)

    band = build_border(material)
    grid = build_grid(material)

    ink = node(material, unreal.MaterialExpressionMax, -820, 0)
    wire(band, ink, "A")
    wire(grid, ink, "B")

    inked = node(material, unreal.MaterialExpressionSaturate, -700, 0)
    wire(ink, inked)

    face_colour = vector(material, "Color", (0.30, 0.02, 0.14), -700, -950)
    edge_colour = vector(material, "EdgeColor", EDGE_PINK, -700, -830)
    glow = scalar(material, "FaceGlow", face_glow, -700, -710)
    brightness = scalar(material, "Brightness", 1.0, -700, -630)

    middle = node(material, unreal.MaterialExpressionMultiply, -480, -900)
    wire(face_colour, middle, "A")
    wire(glow, middle, "B")

    blended = node(material, unreal.MaterialExpressionLinearInterpolate, -300, -820)
    wire(middle, blended, "A")
    wire(edge_colour, blended, "B")
    wire(inked, blended, "Alpha")

    emissive = node(material, unreal.MaterialExpressionMultiply, -140, -820)
    wire(blended, emissive, "A")
    wire(brightness, emissive, "B")
    edit.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if with_opacity:
        base = scalar(material, "FaceOpacity", face_opacity, -480, 150)
        solid_enough = node(material, unreal.MaterialExpressionAdd, -300, 80)
        wire(base, solid_enough, "A")
        wire(inked, solid_enough, "B")
        opacity = node(material, unreal.MaterialExpressionSaturate, -140, 80)
        wire(solid_enough, opacity)
        edit.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    finish(material, name)
    return material


def build_wire_material(name):
    """The triangle edges. Flat colour; a band would fight the lines it is drawn over."""
    material = fresh_material(name)
    if material is None:
        return None

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("wireframe", True)

    colour = vector(material, "Color", (1.0, 0.7, 0.86), -600, -100)
    brightness = scalar(material, "Brightness", 1.5, -600, 100)
    emissive = node(material, unreal.MaterialExpressionMultiply, -350, 0)
    wire(colour, emissive, "A")
    wire(brightness, emissive, "B")
    edit.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    finish(material, name)
    return material


def fresh_material(name):
    """
    The material, with an empty graph. Reused rather than deleted and remade, so the instances
    parented to it keep their link and nothing has to be re-pointed.
    """
    path = "%s/%s" % (FOLDER, name)
    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
        edit.delete_all_material_expressions(material)
        log("rebuilding %s." % name)
        return material

    material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        fail("could not create %s." % name)
    else:
        log("built %s." % name)
    return material


def finish(material, name):
    edit.layout_material_expressions(material)
    errors = edit.recompile_material(material)
    if errors:
        for line in errors:
            fail("%s: %s" % (name, line))
    assets.save_loaded_asset(material)


def build_instance(name, master, rgb, edge_rgb, brightness, overrides):
    """A colour from the palette. Created once; its parameters are rewritten every run."""
    if master is None:
        fail("%s has no master material, so it was skipped." % name)
        return False

    path = "%s/%s" % (FOLDER, name)
    if assets.does_asset_exist(path):
        instance = assets.load_asset(path)
    else:
        instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        if instance is None:
            fail("could not create %s." % name)
            return False

    edit.set_material_instance_parent(instance, master)
    edit.set_material_instance_vector_parameter_value(
        instance, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    if edge_rgb is not None:
        edit.set_material_instance_vector_parameter_value(
            instance, "EdgeColor",
            unreal.LinearColor(edge_rgb[0], edge_rgb[1], edge_rgb[2], 1.0))
    edit.set_material_instance_scalar_parameter_value(instance, "Brightness", brightness)
    for parameter, value in sorted(overrides.items()):
        edit.set_material_instance_scalar_parameter_value(instance, parameter, value)

    assets.save_loaded_asset(instance)
    return True


def retire_old_masters():
    for name in RETIRED:
        path = "%s/%s" % (FOLDER, name)
        if not assets.does_asset_exist(path):
            continue
        if assets.delete_asset(path):
            log("removed %s, which this version replaces." % name)
        else:
            log("%s is still referenced by something, so it was left alone. Harmless." % name)


def run():
    assets.make_directory(FOLDER)

    masters = {
        SOLID: build_lit_material(SOLID, unreal.BlendMode.BLEND_OPAQUE, False, 1.0, False, 0.0),
        PANE: build_lit_material(PANE, unreal.BlendMode.BLEND_ADDITIVE, True, 0.35, True, 0.06),
        WIRE: build_wire_material(WIRE),
    }

    made = 0
    for name, master_name, rgb, edge_rgb, brightness, overrides in PALETTE:
        if build_instance(name, masters.get(master_name), rgb, edge_rgb, brightness, overrides):
            made += 1

    retire_old_masters()

    if _problems:
        fail("STOP. %d problem(s), listed above. A master that fails to connect or compile "
             "renders black, and every colour parented to it renders black with it, so the game "
             "will look unlit rather than wrong-coloured. Do not run the other two scripts - "
             "fix this first." % len(_problems))
        return

    log("Done. 3 masters and %d colours in %s, no compile errors." % (made, FOLDER))
    log("Next: run dress_pieces.py, then dress_level.py on the open map.")


run()
