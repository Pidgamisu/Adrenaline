# Tuning movement in the editor

Added 2026-10-26. How to change a feel value and keep it, without touching C++.

## The three Blueprints

| Asset | Parent | What it is for |
|---|---|---|
| `/Game/Blueprints/BP_Character` | `AGetawayCharacter` | The pawn that actually spawns. Tune movement here. |
| `/Game/Blueprints/BP_PlacementCamera` | `AGetawayPlacementPawn` | The placement fly camera. Added 2026-10-31 so its fly speed and pad look values could be tuned in the editor at all. |
| `/Game/Blueprints/BP_GetawayGameMode` | `AGetawayGameMode` | Changes nothing except `Default Pawn Class = BP_Character` and `Placement Pawn Class = BP_PlacementCamera`. |

All three are created by `Content/Python/setup_tuning_blueprints.py`
(*Tools > Execute Python Script...*). It is safe to re-run: it reuses assets that already exist, so
tuning is never overwritten, and it only ever writes the game mode's two pawn class fields.

`PlacementPawnClass` is `EditDefaultsOnly` on the game mode and is read by
`AGetawayGameMode::StartPlacementFor` when it spawns the camera, with the C++ class as a fallback,
so an unset or broken value falls back to the old behaviour rather than failing to spawn.

## Why the game mode needs a Blueprint too

`AGetawayGameMode::StartRunFor` spawns the runner from `DefaultPawnClass`, so pointing that at a
Blueprint is all it takes - no C++ change. But *setting* it is the problem. Project Settings and
World Settings both show a **Default Pawn Class** field under the selected game mode, and both grey
it out when the game mode is a C++ class. The engine is explicit about it, in
`Engine/Source/Editor/UnrealEd/Public/GameModeInfoCustomizer.h`:

> Only allow editing GameMode BP, not native class!

So the only way to set a pawn class from the editor is for the game mode itself to be a Blueprint.
That is the whole reason `BP_GetawayGameMode` exists. The alternative - loading the Blueprint by
asset path in the C++ constructor - was turned down: it ties the portable plugin to this project's
content and cannot be changed without a rebuild.

`BP_GetawayGameMode` is also the place to edit `HandSize`, `PlacementTime`, `FinishGraceTime` and
the `Catalog`, which are all `EditDefaultsOnly` and were unreachable for the same reason.

## Where the game mode is named

Two places, and **both** have to point at `BP_GetawayGameMode` or the C++ pawn comes back:

1. **Project Settings > Project > Maps & Modes > Default Modes > Default GameMode.** Writes
   `GlobalDefaultGameMode` in `Config/DefaultEngine.ini`, as the `BP_GetawayGameMode_C` class path.
   This is what a map with no override uses.
2. **TestCourse > Window > World Settings > Game Mode > GameMode Override.** Stored in the `.umap`.
   This wins over the project setting for that map, which is why setting only one of them looks
   like nothing happened.

## Which game mode is actually live

The reliable check is the log, not the Outliner. Press Play, stop, then look in the newest
`Saved/Logs/Getaway.log` for these two lines:

    LogLoad: Game class is 'BP_GetawayGameMode_C'
    Getaway: PlayerController_0 is running as BP_Character_C_1

The second line is `AGetawayGameMode::StartRunFor` naming the pawn it just spawned, which answers
the question directly and is cheaper than hunting through the Outliner.

If it says `GetawayGameMode` instead, the map override is still the C++ class and the C++ pawn is
spawning. The pawn in the Outliner is named after whichever won: `BP_Character_C_0` or
`GetawayCharacter_0`.

## Tuning, and the two ways to lose your work

Open `BP_Character`, select **CharacterMovement** in the *Components* panel, and edit the
**Getaway** categories in *Details*. Compile and save.

- **Editing the pawn during Play does not persist.** The Details panel for a possessed pawn in PIE
  is editing that one throwaway instance. Stop, open the Blueprint asset, change it there.
- **An overridden value stops tracking C++.** Once `BP_Character` holds its own `RunSpeed`, later
  changes to the C++ default no longer reach it. The yellow *Reset to Default* arrow beside the
  field drops the override and puts the C++ value back. Worth checking that arrow first when a
  tuning change in code appears to do nothing.

The C++ defaults are unchanged and stay the source of truth; the tuned table is in
`Plugins/GetawayMovement/README.md`. When a value settles, it belongs in the C++ default and in
that table, not only in the Blueprint - a Blueprint-only value is invisible to anyone reading code.

## Dressing a piece: materials

Added 2026-11-02. The plugin ships no content, so every piece wears an engine default: the cube's
`BasicShapeMaterial`, except breakables, which are given `WorldGridMaterial` in
`AGetawayBreakable`'s constructor so they read as breakable at a glance. Logic stays in C++ and the
look is hooked up in a Blueprint subclass, the same rule as any third-party art.

Three things to know before changing any of it.

**One class can only have one look.** The catalog's `floor`, `wall` and `ledge` entries all point at
`AGetawayPiece` (see `AGetawayGameMode`'s constructor), so a material set on that class dresses all
three. Different looks need a Blueprint child each - `BP_Piece_Floor`, `BP_Piece_Wall`,
`BP_Piece_Ledge` - with the entry's **Actor Class** in `BP_GetawayGameMode`'s **Catalog** repointed
at it. Leave **Size**, **Directional** and **Surface Mounted** alone; those came from the lab.

**Most parts are reachable from the Components panel.** Everything a piece shows is a default
subobject created in its constructor - `Mesh`, `Base`, `Piston`, `Pad`, `Charge`, `Cap`, and even
the pooled `Spike_0_0…` and `Bar0…` - so a Blueprint child can select any of them and set
**Materials > Element 0** in *Details*.

**Three slots exist because the Details panel cannot reach those parts** (added 2026-11-02), beside
one older one:

| Property | Class | Why it has to be a property |
|---|---|---|
| `SurfaceMaterial` | `AGetawayRamp` | The wedge is a `UProceduralMeshComponent`. A procedural mesh has no material slot until its section exists, and the section is only built by `BuildWedge` on spawn, so there is nothing to set at Blueprint-edit time. Applied after `CreateMeshSection_LinearColor`, which is the only order that works. |
| `PointMaterial` | `AGetawaySpikes` | Covers the whole pooled bed of cones in one field instead of 16 components. Applied in `LayOutPoints`. |
| `BarMaterial` | `AGetawayVortex` | Same for the cage's bars. Applied in `LayOutColumn`. A translucent or glowing material is the one that reads right - you have to be able to see the course through the cage. |
| `BladeMaterial`, `ArmMaterial` | `AGetawayWindmill` | The paddle wheel pools eight plates and eight spokes, so the Components panel cannot reach them one by one; these two cover the lot. Applied in `LayOutWheel`. The hub is a named component and is dressed there as usual. |

**The script wins over the Details panel, for every piece named in its table.** That is what it is
for - it writes the materials in `PIECES` every run - but it means a look set by hand in
`BP_Piece_*` is gone the next time it runs. It took the spikes that way on 2026-11-03. To keep a
hand-picked look, empty that piece's two dictionaries in `PIECES` (the spikes are `{}, {}` now) and
leave the row itself in place: the row is also what points the catalog entry at the Blueprint, so
deleting it hands that piece back to the raw C++ class.

**Five rows are empty now, not one** (2026-11-05). The panel, the jump pad, the C4 pickup and the
vortex joined the spikes, because they were dressed by hand in the editor that afternoon
(commit 800940a) and a script run put the table's colours back over them the same evening. There is
no way to protect them by *recording* the hand-set value in the table instead: nothing can read a
component material back off a Blueprint's class default object, which is the same limitation that
stops `export_tuning.py` reporting them. Emptying the row is the only lever. The cost is that a
fresh clone gets those five undressed, and that is accepted.

### A script run that resets a colour with nothing saved and nothing to revert (2026-11-05)

Worth knowing because the symptom and the cause disagree. Running `dress_pieces.py` put the panel,
pad, pickup and vortex back to their old colours **in the editor**, while `git status` showed none
of those four `.uasset` files as modified and the log said *"All files are already saved"* for every
one of them.

Both are true at once. `dress()` writes the materials onto the class default object, then calls
`compile_blueprint`, then `save_loaded_asset`. The CDO write **does not dirty the package** - the
same non-persisting write recorded further down this file - so there is nothing to save and the
`.uasset` keeps its hand-set colours. But the **compile** propagates the CDO into the running
editor session, so what you are looking at is the table's colours even though the asset on disk
still holds yours.

So the asset is the source of truth and it was never harmed: **closing the editor without saving
and reopening restores the look.** Nothing to revert, nothing lost. What makes it not happen again
is the five empty rows above, not the reopen.

The trap in it is the other direction: if something else dirties one of those Blueprints while the
wrong colours are live in the session, a **Save All** would write them to disk for real. That has
not happened, and the empty rows mean it now cannot arise from a script run.

**The script also rebuilds `BP_GetawayGameMode`'s whole Catalog from the C++ class defaults** every
run, which is how a piece added in C++ reaches the party box at all (see *A new piece did not reach
the game by itself* in `docs/placement.md`). So a piece's id, size and flags are C++ values only -
editing them in the Blueprint's Catalog array does not last.
| `ScrapMaterial` | `AGetawayBreakable` | Older. The scraps are `NewObject`ed when the panel bursts, so they exist on no Blueprint. Falls back to the mesh's own material when empty. |

All four are set unconditionally, so clearing one in the Details panel puts the engine default back
rather than leaving the last material stuck on.

**`Content/Python/dress_pieces.py` does all twelve at once**, added 2026-11-02. It makes one
Blueprint per catalog entry under `/Game/Blueprints/Pieces`, puts a material on each, and rewires
`BP_GetawayGameMode`'s **Catalog > Actor Class** to point at them. The colours live in a `PIECES`
table at the top of the script - they are a first guess, not a decision, and changing them there
or in the Blueprint's Details panel both stick. Safe to run twice: existing Blueprints are reused,
though the materials named in the table are rewritten every run, so a colour picked by hand needs
clearing out of the table to survive.

**`Content/Python/flag_special_pieces.py` is the narrow version of that**, added 2026-11-06 with
the one-normal-one-special hand. It copies a single field - `bSpecial` on each piece - out of
`AGetawayGameMode`'s class defaults onto `BP_GetawayGameMode`'s `Catalog` rows, matching on Id and
writing nothing else: no Actor Class, no sizes, no materials, and no piece Blueprint recompiled. It
exists because the Blueprint stores the whole `Catalog` array and stopped tracking C++ long ago, so
a field *added* to `FGetawayPieceDef` reads false on all fourteen rows there until something writes
it - and `dress_pieces.py`, which would also fix it, rebuilds the array wholesale and recompiles
nine Blueprints to do so. Reach for that one only when a piece is being **added**; this one when a
flag on an existing piece changed. It forces the save rather than trusting the dirty flag, for the
reason in *Known bug* below, and it refuses to write at all if C++ reports no special pieces, since
that means the build is stale and the result would be fourteen normal rows.

It ran clean first time and the hand it produced played the same day, so its one real unknown - what
UE's Python layer calls a `bSpecial` property, since the engine's own scripts pass the raw name while
the generated stubs strip the leading b - never had to be settled by hand. The script tries each
candidate against a real struct and logs the one that answered, which is the pattern to copy for the
next flag added to a piece.

Two things `dress_pieces.py` does that are worth knowing. It matches catalog entries on **Id**, not position,
because adding a piece to the C++ catalog later would shift every index below it and silently
dress the wrong piece. And it **reads every material back** after compiling, because writing to a
component on a Blueprint's class default object is the part most likely to go quietly nowhere; a
material that did not take is reported with the panel to set it in by hand.

**Known bug: a property write can pass that read-back and still not persist** (found 2026-11-05,
deliberately not chased). `BP_Piece_Ramp`'s `SurfaceMaterial` reads as `M_VB_Solid` on disk, not the
`MI_VB_Ramp` the table names - confirmed three ways: a byte grep of the `.uasset`, and two cold
headless reads via `Content/Python/export_tuning.py`. Yet the script wrote it twice that day and the
read-back passed both times. So the value is on the class default object after the compile and the
save, and is not in the saved asset. **The read-back does not catch this class of failure**, which
is the one thing it exists to catch.

Two consequences while it stands:

- **A placed ramp wears the master material.** Wrong colour, and - worse - `BandMode` 0 instead of
  the `BandMode` 1 that `MI_VB_Ramp` sets. That is the UV band a wedge needs, so the sloped face you
  run up bands along two edges instead of four. On a look where the band carries the shape, that
  reads as the ramp having changed colour.
- **Every piece the script dresses is suspect.** The only ones proved right are those
  `export_tuning.py` reads as *properties*; its component reads are unreliable for a different
  reason, documented at the top of that script.

The fix is by hand rather than in the script: `BP_Piece_Ramp` > **Class Defaults** > **Surface
Material** > `MI_VB_Ramp`, then **Compile** and **Save**. It survives a script run, because the
script's write of that same value is precisely the one that does not stick.

**Scale stretches UVs.** Pieces are the 100 cm engine cube scaled to `Size`, so the catalog's
400 x 50 x 400 wall is scale (4, 0.5, 4) and any ordinary UV-mapped texture smears with it. A
world-aligned or triplanar material is the way out, and the one the first pack happens to use:
`/Game/ThirdParty/GlowGridStarter/Materials/M_GG_Grid` is built on the engine's
`WorldAlignedTexture` function, so its grid holds its size whatever a piece is scaled to. Packs
live under `Content/ThirdParty/<PackName>/` and are never edited in place; `CREDITS.md` says what
is still unanswered about this one, and `Content/Python/move_pack_to_thirdparty.py` is what
moves a pack there.

## Not verified yet

- **Single player: played 2026-10-26 and fine.** `BP_GetawayGameMode_C` spawning `BP_Character_C`
  was confirmed in the log, and the run played normally.
- **The two-player test has not been re-run** since the pawn became a Blueprint. Steps are in
  `docs/multiplayer.md`. It adds no logic and the host path is unchanged, so nothing is expected to
  move, but it is the one gap left.
- **Changing a value in `BP_Character` and feeling it has not been done.** The wiring is proved; the
  round trip through the Details panel is not.
- **No material has been seen on a piece in play.** The three slots above compile and are
  applied in the right place by reading, but nothing has been dressed and run yet: the ramp's
  procedural-mesh slot is the one most worth a look, since it is the only one whose timing could be
  wrong. Nor has a Blueprint child of any piece been made, so *one class, one look* - floor, wall
  and ledge sharing `AGetawayPiece` - is read off the catalog rather than played.
- **`BP_PlacementCamera` exists and the script's write took: run 2026-10-31 and reported clean.**
  `placement_pawn_class` does accept a Blueprint class through `set_editor_property`, the same as
  `default_pawn_class`. What has *not* been done is changing a value on `BP_PlacementCamera` and
  feeling it in the camera - the same gap as `BP_Character` above.
- **`Content/Python/move_pack_to_thirdparty.py`: first run failed, 2026-11-02, and two things
  came out of it.** The script itself works - it loaded all 62 assets and started the rename -
  but `rename_directory` puts up a **modal dialog** when the folder holds maps: *"Source code,
  config INI, and text files may need Find/Replace for: StarterRoom, Overview ... Continue with
  rename?"*. That was answered Cancel, so `RenameDirectory failed: Could not rename the assets`
  and nothing moved. Click **OK**, or delete the pack's demo maps first and the dialog never
  appears. The script now names the maps before it starts so the dialog is expected.
  The second thing: in UE 5.8 the Content Browser folder menu entry is **Update Redirector
  References**, under *Bulk Operations* just below *Delete*. It was called *Fix Up Redirectors in
  Folder* in earlier versions, which is what sent me looking for a menu item that no longer
  exists by that name (`AssetFolderContextMenu.cpp` still uses `FixUpRedirectorsInFolder` as the
  internal id). It is always in the menu, whether or not any redirectors exist.
  The script still stops short of doing the fixup itself: `IAssetTools::FixupReferencers` is not
  a `UFUNCTION` in 5.8, so it is not reachable from Python.
  **A successful move has still not been seen.**
- **`Content/Python/dress_pieces.py` has not been run.** Written 2026-11-02. Its Blueprint
  creation, CDO writes, `compile_blueprint` and read-back all copy `setup_tuning_blueprints.py`,
  which is proven, and `set_material` / `get_material` are `BlueprintCallable` on
  `UPrimitiveComponent` in 5.8 (checked in `PrimitiveComponent.h`; `OverrideMaterials` is
  deliberately not written directly, as the header warns against it). What is genuinely untested
  is whether a material set on an **inherited native component** of a Blueprint's class default
  object survives a compile and save. That is why the script verifies instead of assuming. If it
  reports those by name, they are three clicks each in the Components panel and the script's
  other work still stands.
