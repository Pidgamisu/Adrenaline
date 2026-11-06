# The look: unlit pink

Started 2026-11-02, from a reference shot of a black world with see-through faces, colour around
every edge, and a grid, plus CS surf maps for the overall vibe. We are doing it in pink.

## Three passes so far

**Pass one** (2026-11-02): lights off, unlit emissive colour, a wireframe overlay. Played, and
the verdict was *flat* - correct, because nothing in it replaced the depth cues that turning the
lights off had thrown away.

**Pass two** (2026-11-02): a world grid, a Fresnel rim, additive see-through panes. Not played.
The rim was the wrong idea; see below.

**Pass three** (2026-11-02): the **edge band**, which is what the reference was showing all along
- each face see-through in the middle with solid colour around its rim. **Played the same day and
signed off, at speed** - see *Played and signed off* at the end.

## What the look is made of

1. **No lighting.** Not "dim lighting" - none. The level's lights are dimmed to zero and every
   material is Unlit, driving **Emissive Color** instead of Base Colour.
2. **The edge band.** Each face is solid colour around its rim and hollow in the middle. This is
   the look. Everything else is in service of it.
3. **See-through faces.** `M_VB_Pane` is additive and two-sided, so you see the far side of a box
   through the near side, and overlapping geometry accumulates.
4. **The world grid**, faint, across the hollow middle. Replaces the shadows as a distance and
   speed cue: lines converging into the distance say how far away a ledge is, lines streaming
   past the screen edge say how fast you are going.
5. **One hue, varied by brightness.** Pink is the architecture.
6. **Black background and bloom.** No sky, no fog.

## How the edge band works, and what it costs

**`BorderWidth` is in centimetres**, and 8 means 8 on every piece, whatever its size.

It did not start that way. The first version measured the band in UVs - a fraction of each face -
which looked fine on one piece and wrong across a level, because the pieces are boxes scaled to
whatever size the catalog asks for. The same `0.06` came out as a 6 cm frame on a 1 m crate and a
120 cm slab on a 20 m floor.

The band is now measured from the piece's **world bounding box**. `ObjectPositionWS` and
`ObjectBounds` give the centre and the half extents - `ObjectBounds` is
`FBoxSphereBounds::BoxExtent`, so half extents, checked in
`PrimitiveUniformShaderParametersBuilder.h`. Subtracting the pixel's distance from the centre
leaves its distance in centimetres to each of the three pairs of box faces, and the nearest of
those is how far it is from an edge. One of the three is the face you are standing on, where the
distance is zero everywhere and would paint the whole face; `VertexNormalWS` says which, and that
axis is pushed out of contention.

**It measures to the bounding box, not to the mesh.** For a box at any multiple of 90 degrees
those are the same thing, and placement only rotates in quarter turns
(`AGetawayPlacementPawn::OnRotate`), so every normally placed piece is exact. Two cases are not:
a piece surface-mounted on a slope sits at an angle inside a larger box, so its band creeps
inwards from the real edge; and the ramp is a wedge, not a box.

**The ramp therefore keeps the old UV band**, via `BandMode = 1` on `MI_VB_Ramp`. That band is
proportional rather than constant, but it follows the mesh's own faces, which a bounding box
cannot do for a wedge. It works at all because every piece has per-face 0-to-1 UVs - the pieces
use `/Engine/BasicShapes/Cube` and `AGetawayRamp::BuildWedge` lays its own out the same way. Both
bands are built and crossfaded by `BandMode` rather than switched, so picking between them is one
number on an instance, with no static switch and no second master.

**A Fresnel rim was tried in pass two and dropped.** On a flat box face the normal is constant,
so Fresnel is very nearly constant across the whole face. It dims and brightens whole faces as
you turn, which is not an edge. Worth not re-inventing.

## One trap in building materials from Python

**An unnamed input pin is addressed with an empty string, not with `"Input"`.** `Frac`, `Abs`,
`OneMinus`, `Saturate` and `ComponentMask` all declare a C++ member called `Input`, but that is
the variable name and not the pin name - those pins are drawn blank in the material editor and
`UMaterialExpression::GetInputName` returns nothing for them. `MaterialEditingLibrary`'s
`GetExpressionInputByName` only falls back to `GetInput(0)` when the name it is handed is None,
so `"Input"` matches nothing, `ConnectMaterialExpressions` returns false, and the connection
silently does not happen.

This left every master black on 2026-11-02, and it is quiet: the named pins (`A`, `B`, `Alpha`,
`Min`, `Max`, `Value`, `ExponentIn`) were all fine, so only the single-input nodes were dropped.
A material with a dangling input either fails to compile or compiles to nonsense, and because an
unlit game has no lighting to fall back on, the result is a completely black world rather than an
obviously wrong-coloured one.

`adrenaline_look.py`'s `wire()` now retries unnamed when a pin name does not match, and the
script refuses to tell you to run the other two if anything failed.

## Why additive, and what it costs

`M_VB_Pane` never darkens what is behind it, only adds. Overlapping geometry accumulates, so a
stack of faces is visibly brighter than one - depth you read at a glance. And additive blending
is order-independent, so unlike ordinary translucency there is no per-object sorting to get
wrong; in a level made of dozens of overlapping see-through faces, ordinary translucency pops and
flickers as you run.

**An additive surface can never occlude.** Anything on `M_VB_Pane` is see-through forever, so
panes are for walls, breakables and glow, never for a floor you are judging a landing on.
`M_VB_Solid` is opaque for that reason and still gets the band, so the two read as one family.
That split is a gameplay decision as much as a visual one.

## Where each part lives

| Part | Where | How you change it |
|---|---|---|
| The masters and every tuning number's default | `Content/Python/adrenaline_look.py` | Edit a number, run the script again |
| Any one colour, live | the `MI_VB_*` instance's Details panel | Change it in the editor; re-running the script overwrites it |
| Which piece wears which colour | the `PIECES` table in `Content/Python/dress_pieces.py` | Change a name, run that script again |
| The course that is already built | `Content/Python/dress_level.py` | Run it on the open map, then repaint actors by hand |
| Exposure, bloom, AO, Lumen | `[/Script/Engine.RendererSettings]` in `Config/DefaultEngine.ini` | Edit the ini, restart the editor |

Three masters, because blend mode is a property of a material and not of an instance:

- `M_VB_Solid` - unlit, opaque. Floors, ledges, ramps, bridges, bases. Things you land on.
- `M_VB_Pane` - unlit, additive, two-sided. Walls, breakables, pickups, spikes, the vortex.
- `M_VB_Wire` - unlit, translucent, **Wireframe** ticked. **Off by default since pass three**:
  the band already outlines every face, so the overlay only adds a diagonal across each one -
  the "tilted lines" - and a second line on top of the band. `WIRE_OVERLAY` in `dress_pieces.py`
  and `WIRE` in `dress_level.py` turn it back on; both must agree.

  Turning it off only takes effect when those two scripts are re-run, because the overlay is
  written onto the Blueprints and onto the level's components, not read from the material at
  draw time. `dress_level.py` therefore writes the overlay on **every** component it looks at,
  including ones whose colour it leaves alone - otherwise a stale overlay could never be
  cleared, which is what happened on 2026-11-02.

Parameters worth reaching for first: `EdgeColor`, `BorderWidth` (band thickness **in cm**, the
same on every piece), `FaceOpacity` (panes only - 0 is fully see-through), `GridStrength` (0
removes the interior grid), `CellSize` (grid cell in cm), `FaceGlow`, `Brightness`. `BandMode`
and `BorderWidthUV` only matter for the ramp.

`dress_pieces.py` covers the eleven piece Blueprints, which is everything spawned during the
placement phase. It does not cover TestCourse itself, which was built by hand before those
Blueprints existed - that is what `dress_level.py` is for. It dims the lighting and paints every
static mesh standing in the open map, guessing walls from floors by shape, and it saves nothing:
Ctrl+S keeps the result, closing the map without saving throws it away.

## The colour rule

**Pink is architecture. An off-hue only ever means something you touch.**

- `MI_VB_Pickup` (cyan) - this helps you. C4 charges, the jump pad's piston.
- `MI_VB_Danger` (lime) - this kills you. Spikes, on a 40 cm grid rather than 100 and with the
  fattest band in the game, so the one thing with a finer weave is the one thing that ends a run.

At 800 cm/s there is no time to read a shape, only a colour. A third accent hue costs more than
it sounds like it does.

## Why unlit suits this game

- Greybox becomes the finished look, so the art pass stops being a blocker.
- Unlit with Lumen off is very cheap, which a movement game wants.
- It fixed *Next steps* item 3 on 2026-11-02: the placement ghost was drawn with debug lines, which
  do not exist in a Shipping build. See *The placement ghost* below.

## The placement ghost

`MI_VB_Ghost`, added 2026-11-02, is what let the placement ghost stop being debug lines. The point
worth recording is that **it needed no new look at all**: `M_VB_Pane` is additive, two-sided and
hollow-faced with a band round every edge, which is already a hologram. The ghost is one more row
in the palette, not a new master.

- **It carries no interior grid** (`GridStrength` 0). The world grid is a property of the level and
  a distance cue; a box following your crosshair that also has a grid on it reads as another piece
  of architecture, which is the one thing a cursor must not do.
- **Cyan where the piece may go, red where it may not**, which is what the debug lines did, and a
  deliberate exception to the colour rule below - the ghost is a cursor, not architecture.
  **Settled 2026-11-02.** `AGetawayPlacementPawn` makes dynamic copies and writes `Color` and
  `EdgeColor` per frame, so both states come off this one asset.
- **The face is only tinted, at `GhostFaceScale` of the band's colour.** Additive blending means a
  bright ghost face washes out whatever you are trying to line the piece up against, which is the
  one thing you are looking at while placing.
- **Two dynamic copies, because a ramp's ghost is a wedge.** The boxes keep `BandMode` 0 and the
  wedge takes `BandMode` 1, for the same reason `MI_VB_Ramp` does: a bounding box cannot find a
  sloping face. See *How the edge band works* above.

The ghost geometry itself, and what is still unproved about it, is in *The ghost is real geometry*
in `docs/placement.md`.

## Known gaps and traps

**The band stretches with the piece**, as above. The most likely thing to look wrong first.

**The character is undressed.** `BP_Character` still wears a lit material, so in a two-player game
the other player renders as a black silhouette. `MI_VB_Player` exists for it; it has to be put on
by hand, because the script does not know that Blueprint's components.

**Thin bright lines on black are the worst case for temporal anti-aliasing.** If the band or the
grid shimmers or crawls while you run, that is TSR, not the material. `r.AntiAliasingMethod` is
the dial; left alone on purpose, because changing it affects everything.

**A world-position grid has no idea how far away it is.** Far-off cells pack into fewer and fewer
pixels and will alias. `GridStrength` at 0 is the blunt fix; a distance fade is the real one.

## Played and signed off (2026-11-02)

**Pass three was played the day it was built, and it works - including at speed**, which was the
whole question. The verdict was "the game looks good even when going fast". That is the thing pass
one failed at: the look read as *flat* because turning the lights off threw away the depth cues,
and the edge band was the answer. It is.

Answered by playing, so treat the list that used to be here as history:

- **See-through faces are readable enough to play against.** The band carries the shape. Running
  into something you can see straight through was the headline risk and did not happen.
- **`BorderWidth` 8 cm is a frame, not a hairline**, at 800 cm/s.
- **The thin-bright-lines-on-black worry did not materialise.** No shimmer or crawl reported while
  running, so TSR is not fighting the band and `r.AntiAliasingMethod` stays alone.
- **Bloom does not blow out** where panes overlap, and **the interior grid helps** rather than
  adding noise.

Two close-up details were never specifically looked at, and are the only things left on this list:

- Whether pieces surface-mounted on a slope show the band creeping in from their real edge. It is a
  consequence of measuring to the bounding box, so it is real; the question is only whether it is
  visible enough to care about.
- Whether the ramp, on the UV band rather than the world one, looks out of family close up.

Both are the kind of thing you notice standing still, not at 800 cm/s, which is presumably why
neither came up.

## Next levers, if it still is not there

1. **Fog.** Surf maps lean on it heavily. A very dark pink `ExponentialHeightFog` would fade
   distance and stop far geometry piling into a bright mush - a problem additive makes worse.
   `dress_level.py` currently hides fog; it would need to stop.
2. **A backdrop instead of pure black.** A near-black gradient with a horizon gives a sense of up
   and down that an empty void does not. The reference is pure black; surf maps never are.
3. **A real-world-width band**, deriving face size in the material.
4. **Scrolling or pulsing emissive** on panes, which is very surf and nearly free.

## The explosion (2026-11-02)

`AGetawayBlast`: an additive flash that swells and fades, plus a handful of glowing cubes thrown
out of it. *Next steps* 5, and the last thing keeping the packaging config on Development.

### Why it is not Niagara

CLAUDE.md asked for a Niagara explosion. It is geometry instead, decided 2026-11-02:

- **Niagara cannot be scripted.** The only Blueprint or Python exposed API in the `NiagaraEditor`
  module is script-upgrade plumbing (`UpgradeNiagaraScriptResults.h`); there is nothing for
  building an emitter stack. A system can only be authored by hand in the editor, so it could
  never be built, seen or verified from here - only described as click steps and iterated on
  blind.
- **Niagara's stock sprite material is lit and ordinary translucent**, so a default system would
  have looked like stock Unreal in a game with no lights, and would have needed its own unlit
  additive sprite material before it looked like anything.
- **Geometry already is the look.** This is the same move the placement ghost made the same day:
  `DrawDebugSphere` out, real components in. It costs no asset, it is in every build, and every
  number is a `UPROPERTY`.

**The Niagara slot is untouched and still wins.** `ExplosionEffect` on `BP_C4` overrides this
entirely, so a real system can replace it later without deleting anything.

### What it is made of

Both parts are dynamic instances of **`M_VB_Pane`** - unlit, additive, two-sided. Additive is
what makes a blast safe to draw: it never occludes, so it cannot hide the floor you are about to
land on, and it needs no sorting, so overlapping shards simply add up. `GridStrength` is set to 0
on both, because the world grid belongs to the level, not to a flash going off inside it - the
same thing `MI_VB_Ghost` does.

**The flash** is `/Engine/BasicShapes/Sphere`, swelling from `FlashStartRadius` to `FlashRadius`
on an ease-out while `Brightness` falls off squared. It is sized by the charge, not by the class:
`AGetawayC4` calls `SizeToBlast(BlastRadius)` and the flash fills `FlashRadiusFraction` (0.55) of
the real blast - under 1 deliberately, because the edge of a light is the last thing you see and
the blast itself has no edge.

**`FlashFaceOpacity` is 0.55, against 0.04 everywhere else in the palette, and that is not a
mistake.** The edge band is what carries shape on a box, but a sphere only touches its own
bounding box at six points, so there is almost nothing for a band measured off that box to grip.
On a flash the *face* is the effect. If a sphere ever needs a band, `BandMode` 1 gives it the UV
one, the same way the ramp's wedge works.

**The shards** are 16 cubes stepped by hand - a plain ballistic arc with a tumble, no physics,
because they are cosmetic and must not touch anything. They are cubes on purpose: the band is
exact for a box at any quarter turn, and `ShardBorderWidth` 10 on a 14 cm cube covers the whole
thing, so a shard reads as a spark rather than as a tiny hollow crate. `ShardUpwardBias` tilts
the spray up so half of them do not vanish into the floor on frame one.

They are separate components rather than one instanced mesh, and that is forced: an
`UInstancedStaticMeshComponent` reports the bounds of the whole component to the material, not of
each instance, so `ObjectBounds` would be the bounding box of the entire spray and the band maths
would collapse. One component per shard is what keeps each one's band correct.

### Cyan, not pink

The blast is cyan, which follows the colour rule rather than breaking it: pink is architecture,
and cyan is the colour of things that help you. C4 is a rocket jump and a way through a wall, and
`MI_VB_Pickup` is already cyan for the same reason. `FaceColour` and `EdgeColour` are both
editable if it reads wrong against the pink.

### One trap worth not repeating

**`/Engine/BasicShapes/Sphere` was not referenced anywhere in this project**, so a runtime
`LoadObject` by path would have returned null in a packaged build - the flash would have been
missing from the exact kind of build this class exists to fix, and only there. Both meshes are
now `ConstructorHelpers::FObjectFinder` hard references, which is what gets them cooked. Same
reason `AGetawayC4` finds its own cube that way.

### What was verified, and how

By summoning one in the real game - `-ExecCmds="summon GetawayBlast"` on a `?Play=1` session -
and reading a log probe:

    BLASTPROBE built: flash=yes shards=16 radius=330 mat=ok
    BLASTPROBE done after 0.75s

So the sphere and cube meshes resolve, all 16 shards build, `/Game/Materials/M_VB_Pane` loads
(no fall back to the lit default material), and the actor tidies itself up on time. The probe was
removed afterwards; `summon GetawayBlast` is still the quickest way to see one without throwing a
charge.

Both build targets compile and link with no errors and no warnings.

### Still unverified

- **How it looks.** Nobody has seen it. Whether the flash reads as a bang, whether 16 shards at
  14 cm is too few or too many, and whether cyan sits right against the pink are all feel
  questions, and every one of them is a field in the Details panel of `BP_C4`'s blast class.
- **How it looks through a wall.** Additive never occludes, so a blast behind a panel will show
  through it. That is deliberate and might still be wrong.
- **Whether it is enough to move packaging to Shipping.** The guards are right by inspection and
  nothing in the effect is debug draw, but nothing has been packaged yet at all.

## The logo, the icon and the splash (2026-11-06)

A logo arrived: a 2000x2000 square, a pink field above a white diagonal and a black field below
it, with a tall outlined **A** straddling the line. It is the look in one picture - pink against
black, shape carried by a line rather than by a fill - so it needed no restyling to sit next to
everything else. The master is in the repository at `Branding/adrenaline-logo.png`, which is a new
top-level folder for source art, the way `Audio/` is for source sound. Nothing in `Content/` is
generated from it, so there is no asset to re-import and nothing to do in the editor.

It is applied in two places, and both are picked up by **filename alone**: no project setting, no
Details panel, nothing to click.

| File | What it is |
|---|---|
| `Build/Windows/Application.ico` | The packaged `Getaway.exe`'s icon. UnrealBuildTool compiles it into the executable's resources, and the staging step copies it beside the exe, so it is what Explorer, the taskbar, the Alt-Tab list and anyone downloading the build from itch sees. |
| `Content/Splash/Splash.bmp` | The window the packaged game shows while it loads. |
| `Content/Splash/EdSplash.bmp` | The same for the editor. |

`Tools/make_brand.py` builds all three out of the master. It only needs running again if the logo
changes. It needs Pillow, which Unreal's bundled Python does not ship; its docstring has the one
`pip install --target Tools/pylibs` line that fixes that, and `Tools/pylibs/` is in `.gitignore`.

### The splash

720x370, matching the engine's own. Black field, the mark on the left at 196 px, **A D R E N A L
I N E** to the right in Roboto Bold with the letters spaced out by hand - the same face and the
same trick as the menu's title, so the splash and the front end read as one thing - a pink rule
under the word, and `FIRST PERSON MOVEMENT PARTY GAME` under that in pink.

The mark gets a **one-pixel pink hairline round it**. Without it the logo's lower half, which is
black, merges into the black splash and the square loses its bottom and right edges.

The game's and the editor's splash are the same picture laid out differently. The editor draws its
own text over the bottom of its splash - the game name, the engine version, the copyright line and
"Loading..." - in rects running from `bmHeight - 60` downwards, so `EdSplash.bmp` centres its
content in the top 306 px and leaves the bottom 64 clear. The game's splash is *custom*, which is
what stops the engine writing the project name over it, so that one uses the full height.

### Two engine details that decide the file formats

Both read out of UE 5.8's source rather than guessed, and both are the kind of thing that fails
silently:

- **BMP, not PNG.** `GetSplashFilename` in `GenericPlatformSplash.cpp` looks for `Splash.png`
  first and only falls back to `.bmp` - but `WinPlatform.Automation.cs` stages exactly one file,
  `Content/Splash/Splash.bmp`. A PNG would look right every time you tested it in the editor and
  be **missing from every package**.
- **Uncompressed icon entries.** Pillow stores 256 px icon entries as embedded PNG by default, and
  the resource compiler UBT runs over the icon rejects those. The script passes
  `bitmap_format="bmp"` so every entry is a plain DIB.

### Why each icon size is drawn separately

The A is thin outlines, and a straight downsample of them turns to grey mush below about 48 px.
So the icon holds **seven separate drawings** (256, 128, 64, 48, 32, 24, 16), and the small ones
have their white strokes fattened first - by 5, 3, 2 and 1 pixels on a 256 px working copy at 16,
24, 32 and 48. Judged by eye against a 4x blow-up of every size. At 16 px the A is a chunky mark
rather than an outline, which is the best that can be done with a line that fine; from 24 up it
reads properly.

### Still unverified

- **The icon on a packaged build.** Nothing has been packaged since it was added, so UBT's
  resource step and the staged copy are both right by inspection only. The `.ico` itself was read
  back and holds all seven sizes as plain DIBs.
- **Both splashes on screen.** The editor's shows on the next editor start; the game's needs a
  package. Neither has been looked at.
- **The logo anywhere in the game itself.** The menu still spells ADRENALINE out as text. Putting
  the mark on the front end would mean a texture asset and a change to
  `UGetawayMenuWidget::NativeOnInitialized`, and was deliberately left out of this pass.
