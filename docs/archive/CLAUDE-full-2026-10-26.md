# Project context for Claude

## The game

A 3D, first-person take on **Ultimate Chicken Horse**: each round, players draft a piece from a random box, place it in a shared level, then everyone races to the objective. The twist is **deep, skill-based movement** (Source/Apex/Titanfall style), so pieces should change *how* you can move (wallrun surfaces, slide ramps, breakable panels, C4 pickups, crumbling bridges, bounce pads), not only block the way. Scoring follows Ultimate Chicken Horse: no points if everyone finishes, so players try to make the level hard but still beatable for themselves.

Engine: **Unreal Engine 5 (C++)**. The movement is ported and signed off. **Multiplayer works**:
client-authoritative, two players play-tested 2026-10-17, see *Played and signed off* at the end.
Deaths are the main thing the round loop is still missing.

## What exists

- `Plugins/GetawayMovement/`: UE5 C++ plugin (port of the browser prototype). It was written without an Unreal install, then reviewed by hand. As of 2026-10-15 it **compiles clean against UE 5.8** with no changes, and the movement has been play-tested and signed off. See its `README.md` for setup, controls and the full tuning table.
  - `UGetawayMovementComponent` (subclass of UCharacterMovementComponent): Source friction/accelerate/air-accelerate in `CalcVelocity`; jump, double jump, slide, wallrun start and mantle start in `UpdateCharacterStateBeforeMovement`; custom modes WallRun and Mantle in `PhysCustom`.
  - `AGetawayCharacter`: Enhanced Input built in code (no assets), Source-style mouse look, camera effects, C4 throwing.
  - `AGetawayC4`, `AGetawayBreakable`, `AGetawayHUD`, `AGetawayGameMode`.
  - The round, added 2026-10-17: `AGetawayGameState` (phase and clocks, shared) and
    `AGetawayPlayerState` (your hand, your run, your score). See *The round loop* at the end.
- `Reference/getaway-lab.html`: the browser prototype (three.js). Open it in a browser to feel the target movement. It also has a 3D level editor and level share codes. This is the **reference for how movement should feel**. When the Unreal version feels different, compare against it.

## Decisions already made (don't undo without asking)

- **Movement feel:** Source-style ground friction and air strafing, lossless bunny hops, auto-hop while holding jump, jump buffering, mouse-wheel jump.
- **Double jump:** 1 air jump. It redirects your momentum toward the keys you're holding, without losing speed. It is not spent by a jump pressed just before landing (that press is kept for a bunny hop).
- **Slide:** crouch at speed. Entry boost on a 1 s cooldown, very low friction, speeds up on slopes.
- **Wallrun:**
  - Only when the wall is **beside** you (within about 40 degrees of straight left or right of your view), never in front.
  - Forgiving attach: 60 cm reach, snaps onto the wall, works at low speed and while falling fast.
  - Space kicks off.
  - In the Unreal version you can't re-grab the same wall until you land.
- **Mantle:** stays in. Keeps 75% of your run-up speed (it doesn't stop you) and does a small camera jiggle.
- **No kick/punch button.** Breaking things is done by hitting them at speed (slide or jump into them) or with C4.
- **C4:**
  - Carry up to two charges at a time, G to throw, F to detonate.
  - **C4 is a collectable resource** (decided 2026-10-15, replacing "charges return when they detonate"): a thrown
    charge is spent for good. You spawn with `StartingCharges` (0 by default) and restock by running through an
    `AGetawayC4Pickup`, which grants `ChargesGranted` (**1** since 2026-10-19, was 2, so a full load of
    `MaxCharges` 2 costs two pickups) and respawns after `RespawnDelay` (8 s) so a placed pickup
    stays useful for a whole round. `StartingCharges = MaxCharges` restores the old always-loaded behaviour.
  - **Inherits 100% of your velocity** when thrown.
  - Sticks to surfaces.
  - Rocket-jump style launch.
  - Bursts breakables.
  - Never damages walls or floors.
  - **A pickup is per player** (decided 2026-10-18, after playing): each player can collect the
    same `AGetawayC4Pickup` once per respawn, and each sees their own pad empty. The alternative -
    a shared pickup that the first player there takes from everyone - was considered and turned
    down: per-player is fairer in a race, and it falls straight out of client authority, since the
    charge count lives on the machine that owns the character. Making it shared means routing the
    grant through the server like `AGetawayBreakable::Break` and replicating `ChargesHeld` to its
    owner. Don't switch it without asking.
- **Destruction stays simple:** only panels and crates break, into loose scraps the player can shove around. A Finals-style fracture/collapse system was tried in the prototype and **scrapped**: it was too heavy and didn't feel like The Finals. If big destruction comes back later, use Chaos Destruction (Geometry Collections), not custom code.
- **Camera effects:** wallrun tilt, speed FOV, shake, landing kick and mantle jiggle are wanted, with a toggle to turn them off. Note when porting camera effects: roll signs invert between the lab and Unreal. three.js rolls about a camera Z axis pointing back at the viewer; Unreal rolls about forward X in the aviation sense (positive = right side down). Copying a roll formula across verbatim tilts it the wrong way, which is exactly what happened to the wallrun tilt (fixed 2026-10-15). A camera "shaking" bug was reported in the prototype and was likely frame-timing judder. Movement should look smooth at any refresh rate.

## Units and tuning

Unreal uses cm. The prototype showed Source units: **1 u = 2.5 cm** (40 u = 1 m, 320 u/s = 800 cm/s). The tuned defaults are in the plugin README table. Key values:

- RunSpeed 800
- GroundAccelerate 10, GroundFrictionSource 5, StopSpeed 250
- AirAccelerate 12, AirSpeedCap 75
- GravityScale 2.04
- JumpSpeed 750

## Next steps

1. ~~Build the project and fix compile errors in `Plugins/GetawayMovement`.~~ **Done 2026-10-15** - it built clean with no changes. See *Build setup on this machine* below.
2. Play-test against `Reference/getaway-lab.html`. Check mouse sensitivity first: `MouseInputScale` compensates for Unreal's 0.07 axis scaling, and that assumption is unverified.
3. ~~Decide the multiplayer approach.~~ **Client-authoritative movement, spiked 2026-10-17** and it holds up: the movement component did not have to change at all. See *The multiplayer spike* at the end. The server handles placements; deaths, finish and scoring are still single-player.
4. Build the round loop:
   - ~~Draft from a random "party box".~~ Per player as of 2026-10-17.
   - ~~Placement phase: fly camera, grid snapping, piece previews.~~
   - Short look-around before the run. **Still missing** - the run starts the moment you are possessed.
   - ~~Run.~~
   - ~~Ultimate Chicken Horse scoring.~~ **Done 2026-10-17, simplified 2026-10-19**, see *Scoring*
     under *The round loop*: a point for getting there, a point for getting there first, nothing
     at all if everybody cleared it. The solo-goal bonus and the leader scale-back this list
     originally asked for were both taken out on purpose. The **bonus for kills by your placed
     piece** was asked for here and **turned down on 2026-10-19**: kills are counted but pay
     nothing, because stopping somebody finishing is already worth two points under the all-finish
     rule. See *Scoring*.
5. Build the piece catalog: ~~wall, floor, ramp, breakable panel, crate, collapsing bridge, C4 pickup~~, ~~plus traps~~.
   All eight were in as of 2026-10-15 and the placement phase deals them - see *The placement
   phase* below. **Spikes, the first trap, landed 2026-10-19** and brought the death system with
   them; see *Spikes, and death* at the end. More traps are the obvious next pieces.

## Working style

The user is new to Unreal C++. Explain what to click in the editor when a step needs it. Keep the movement feel matching the prototype unless asked to change it.

## Build setup on this machine (as of 2026-10-15)

The handoff shipped only the plugin, so the host project was created around it:
`Getaway.uproject` (engine 5.8), `Source/Getaway` (a minimal primary game module, required because a
plugin can't be built on its own), and `Config/DefaultEngine.ini` (Enhanced Input default classes,
`GetawayGameMode` as the global default game mode, and `Template_Default` as the startup map).

Toolchain: Unreal Engine 5.8.3 at `C:\Program Files\Epic Games\UE_5.8`, Visual Studio Community 2022
17.14 with the *Game development with C++* workload, MSVC 14.44.35207 and Windows SDK 10.0.26100.
UE 5.8 wants MSVC 14.38 or newer and prefers 14.50, so it may log a note about the toolchain version.

### The plugin compiles clean

**The first build succeeded with no code changes and no warnings.** Despite being written for
UE 5.3-5.5 without an Unreal install, nothing in `Plugins/GetawayMovement` needed fixing for 5.8.
UnrealHeaderTool ran with `-WarningsAsErrors` and passed. All seven .cpp files were compiled
separately (the adaptive build excluded them from the unity file), so nothing was hidden by unity
batching.

Build it from a terminal with:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GetawayEditor Win64 Development -Project="C:\Users\natha\Documents\Unreal Projects\Getaway\Getaway.uproject" -WaitMutex
```

Or just double-click `Getaway.uproject` and let the editor rebuild when it asks.

### Verification status

The movement has been played and is signed off, `MouseInputScale` included - the 0.07 axis-scaling
assumption turned out to be about right and was never reported as wrong. Two bugs came out of that
first session and are fixed; see *Fixed after the first play-test* below. What is still unplayed is
tracked under the placement phase, further down.

## The test course (`/Game/Maps/TestCourse`)

Built 2026-10-15 and set as both the editor startup map and the default game map, so opening the
project and pressing Play drops you straight into it. It is a linear course along +X; every distance
comes from the plugin's own tuning values, so each obstacle is sized to *require* the mechanic it
tests rather than merely allow it.

| X (cm) | What | Why that size |
|---|---|---|
| -400..2300 | Runway | Long enough to reach RunSpeed 800 before anything asks for it |
| 1700 | Slide bar, gap 110..230 | Clears a crouched player (100 tall), stops a standing one (180) |
| 2300..3700 | Wallrun gap, walls 400 tall both sides | 1400 across - a double jump reaches only ~900, so the wall is the only way over |
| 3700..5600 | Mantle ledges at 60 / 110 / 165 | Spans the MantleMinHeight 30 .. MantleMaxHeight 170 window |
| 5600..6900 | Two breakable panels, then a 5-crate stack | Slide-smash threshold is 495; running flat out (800) is under SmashSpeed 900, so panels want a slide |
| 6900..9050 | Three jump platforms, offset sideways | Gaps of 450 with a 500 lateral shift - needs the double jump and air-strafing, not just a hop |
| 9050..9900 | Breakable blast wall, then a ledge at 500 | 500 is above a double jump (~280), so the only way up is a C4 rocket jump |

Colour coding follows the browser lab: cyan = wallrun surface, green = mantle ledge, amber = slide /
finish, orange = breakable, dark grey = ordinary solid.

A catch floor runs under the whole course at z=-300 so a miss never drops you into the void, and
7-step staircases (40 cm rise, under the 50 cm MaxStepHeight) climb out of each pit at its far end.
Falling used to cost a few seconds and skip that obstacle instead of ending the run, which also
meant someone who could not wallrun yet could still reach the breakables and the C4 section to test
them. **As of 2026-10-23 you die before you get there**: `FallDeathZ` is -100 and the catch floor is
at -300, so the floor and its staircases are now a backstop nothing reaches. See *Falling off kills
you* at the end - that is one number on the character if the forgiving version is wanted back.

**The C4 pickup is at X=6100** (`C4Pickup_1`, an `AGetawayC4Pickup`, added 2026-10-15). It sits on the
floor in the gap between the two breakable panels, straight in the running line, so a slide-smash run
through Panel_1 collects it on the way past. It grants 1 charge (2 until 2026-10-19) and comes back
8 s after being taken; one charge is still a rocket jump, so the ledge at 9050 is still clearable;
while depleted only the ground pad is left, with the floating brick and its light hidden.

It is there because **C4 is now a collectable resource**: you spawn with `StartingCharges = 0`, so the
rocket-jump ledge at 9050 is only reachable if you picked the charges up first. That makes the pickup
part of the course rather than decoration - but it also means the C4 section can't be tested at all if
the pickup fails to trigger. If that happens, set the character's `StartingCharges` to 2 in the
Details panel to carry on testing the blast itself.

The level was generated by script rather than by hand; the scripts are not kept in the project, so
edit the map in the editor from here on.

### Rebuilding or extending it

`Config/DefaultEngine.ini` points `GameDefaultMap` and `EditorStartupMap` at it, and the level's own
World Settings carry a `GetawayGameMode` override plus `KillZ = -2000`. `PythonScriptPlugin` and
`EditorScriptingUtilities` were enabled in `Getaway.uproject` to allow script-driven level editing;
nothing else depends on them, so they can be turned off if unwanted.

### Fixed after the first play-test (2026-10-15)

- **Wallrun camera tilted the wrong way.** The port copied the lab formula including its sign; see the
  roll-convention note under *Decisions already made*. The slide tilt shared the same root cause and
  was flipped too, though it was not reported.
- **Mantling almost never caught in mid-air.** The ledge was found by a single ray at feet + 35 cm, so
  the ledge top had to clear that before the wall was seen at all - on the 60 cm ledge that is a
  window of feet < 25 cm, roughly two frames of a jump. It now probes up the capsule and takes the
  lowest wall, restoring the full 30..170 window. The climb-clearance sweep was also widened from
  2 cm of slack to half capsule width, since it could report a blocking hit on the very wall being
  climbed.

Mantle still deliberately requires you to be airborne, matching the lab (`!P.onGround`): you jump
into a ledge rather than walking at it. That is a design decision, not a bug.

### Verification status

Both fixes above were play-tested and confirmed good, so the course's movement side is settled.

The C4 chain is confirmed too (2026-10-16): the pickup catches a player at speed and the blast
clears the 500 cm rocket ledge. The whole course is now completable.

## The placement phase (added 2026-10-15)

The first half of the Ultimate Chicken Horse round loop: fly around, drop a piece into the level,
then race it. `AGetawayGameMode` drives a phase (`EGetawayPhase::Placement` / `Run`) and swaps
which pawn each controller holds. It was built single-player; the phase now lives on
`AGetawayGameState` so clients can see it, see *The round loop*.

| Class | What it does |
|---|---|
| `AGetawayPlacementPawn` | The placement camera. Collisionless fly pawn, input built in code like the character |
| `AGetawayPiece` | A plain solid block - floors, walls, ledges. Just a scaled cube with collision |
| `AGetawayRamp` | A wedge that rises along its own +X. Mesh and convex collision built in code |
| `AGetawayCollapsingBridge` | A deck that holds for `CollapseDelay` after you stand on it, drops under physics, then comes back |
| `FGetawayPieceDef` | A catalog entry: id, display name, size in cm, actor class, whether to scale it, whether it is directional |

Controls during placement: **WASD + Space/C** fly, **Shift** faster, **mouse wheel** picks a piece
from your hand, **R** turns it 90°, **left click** places it, **Enter** says you are done building
(the run starts once everybody is, or when the clock runs out - it is not a personal start). During
the run, **Backspace** drops you out of it, back to the fly camera with everything you built still
standing - but it now forfeits your run rather than restarting the round, see *The round loop*.

### The build clock (added 2026-10-17)

Placement is timed. You get `PlacementTime` (**30 s**) to build, and the run starts by itself when
it runs out - or as soon as you have placed your whole hand, after a `LastPieceDelay` (1 s) beat so
you see the last piece land. Enter still starts the run early. Setting `PlacementTime` to 0 turns
the clock off and goes back to waiting for Enter, which is how the phase was first tested.

- **The clock is the only thing that ends placement for everybody.** An empty hand does not: dealing
  from an empty catalog would otherwise strand you in the fly camera. Each new round deals every
  player a fresh hand and starts a fresh 30 s.
- **The hand-spent run is deferred, not immediate.** `TakeFromHand` runs inside the placement pawn's
  own click handler, and `StartRun` destroys that pawn, so starting the run there and then would
  pull the object out from under the code still using it. The timer sidesteps that and gives the
  placement a visible beat at the same time.
- **All three routes into the run clear the timer** (Enter, the clock expiring, the hand emptying),
  so whichever fires first, the others cannot fire afterwards into a run that has already started.
- The HUD shows the countdown top-centre, where the run clock sits, so time is always read from one
  place. Amber at 10 s, red at 5 s, green while the hand-spent beat plays out.
- Since 2026-10-17 the **clock is shared and the hand-spent beat is yours alone**: one build clock
  covers everybody, but spending your own hand starts only your run. The clock lives on
  `AGetawayGameState` and the beat's timer on your `AGetawayPlayerState` - see *The round loop*.

### How placement positions a piece

Ported from the lab's `placeBox()`, so it behaves the way the browser editor taught you: the piece
sits **flush against the face you are aiming at**, and is snapped to a 50 cm grid (the lab's 0.5 m)
on the other two axes. Rotation is in 90° steps and starts aligned to the way you are facing, so a
wall always reads across your view. Aiming at nothing falls back to the ground plane. A piece that
would overlap existing geometry turns red and cannot be placed - and since 2026-10-23 so does one
that would touch nothing at all; see *Everything has to touch something* at the end. Aim at a
**sloped** face and the piece lies down on it rather than sitting square; see *Things lie down on
slopes* at the end.

### The catalog

Nine pieces. Eight are sized from the lab's `PTYPES` table (lab metres x 100, Y-up swapped to
Z-up): floor 400x400x50, wall 400x50x400, ledge 200x200x130, breakable panel 300x50x400, crate
100x100x100, ramp 600x400x200, collapsing bridge 1200x400x60, and the C4 pickup. The ninth is
**spikes** (400x400x50), added 2026-10-19, which has no lab equivalent and is the only
surface-mounted piece - see *Spikes, and death*. The party box deals `HandSize` (3) of them at
random without repeats.

Edit the catalog on a Blueprint child of `GetawayGameMode` - it is `EditDefaultsOnly`.

### Directional pieces (added 2026-10-15)

The ramp and the bridge care which way round they are - the lab calls these `DIRTYPES` and stores a
facing `d` on the piece. Here `FGetawayPieceDef::bDirectional` marks them, and the facing *is* the
piece's rotation: **a directional piece runs its length along its own +X and rises or falls that
way**, so `Size.X` is the length along the facing, not along world X. That is why the ramp is
600x400x200 rather than the lab's bare 400x600x200: placement lines a piece's +X up with the way
you are looking, so a ramp slopes away from you and a bridge drops away from you until you press R.

The ghost draws the arrow (and the HUD prints "facing north", the lab's `DIRNAME`) only for
directional pieces, plus the sloping edge the piece will actually have, since a ramp fills only half
of its ghost box. The blocked test still uses the whole box, so a ramp is called blocked by
something sitting in the empty air above its low end.

- **The ramp has no wedge asset.** The engine BasicShapes set has none and this plugin ships no
  content, so `AGetawayRamp` builds one: six vertices, five faces and a convex hull, filling a 100 cm
  cube so the catalog scales it like every other piece. The convex hull is what the capsule sweeps
  against (`bUseComplexAsSimpleCollision = false`), which matters because
  `UGetawayMovementComponent::CalcVelocity` reads the floor normal to push you along a slope while
  sliding - a triangle soup would give it edges to catch on. Verified by tracing onto the slope: one
  clean plane, normal (-0.316, 0, 0.949) at the catalog's 18.4°.
- **The bridge is plain physics.** Stand on it (a thin overlap probe over the deck, polled every
  0.1 s), it rattles for `CollapseDelay` (0.45 s), then it simulates physics with a downward kick at
  its far end so it tips nose-first instead of sinking flat. It keeps its collision all the way down,
  so you can ride it. Once it has gone it stays gone until the next round - `bRespawns` was on
  with a 6 s `RespawnDelay` until 2026-10-26 and is now off by default, see *A bridge stays down,
  and C4 gets swept up*. `WarnShake`, `TipSpeed` and `CleanupDrop` are the rest of the knobs. No
  hinge constraint and no debris - heavy destruction stays scrapped.

### Gotcha: Enhanced Input contexts outlive the pawn

Fixed 2026-10-15, after "I can't move after placing". A mapping context is registered on the
**local player subsystem**, not on the pawn that added it, so swapping pawns leaves the old context
in place. The character and the placement pawn both map W/A/S/D and Mouse2D at priority 0, and input
actions consume by default - so after Enter the dead placement pawn's actions still swallowed every
movement key and the character never saw them. Both pawns now call `ClearAllMappings()` before
`AddMappingContext()`. Any third pawn added later must do the same.

### The objective (added 2026-10-16)

`AGetawayFinish` ends the run. Reach it and the game mode stops the clock, shows your time and the
best of the session, holds the banner for `FinishLingerTime` (2.5 s) and then drops you back into
placement to build the next one. That closes the loop: place, run, finish, place again.

- **It never blocks you.** The trigger is QueryOnly and overlaps pawns only; both meshes have
  collision off. You can slide, wallrun or rocket-jump straight through it at full speed. An
  endpoint that stopped you dead on contact would be worse than none.
- **You keep control during the banner**, so overshooting still looks like running through a finish
  line rather than hitting a wall.
- **It fires once per player per run,** not once per run - the rest of the field is still coming,
  so since 2026-10-17 it stays lit and keeps spinning after the first player through it. The game
  mode is what knows who has already finished, so it does the de-duplicating and the actor just
  reports. `StartRun()` still arms every finish in the level and warns in the log if there are
  none, since the run would then have no way to end.
- **Only the server's overlap counts.** The actor exists on every machine; `Reach()` returns early
  off the server, because a client has no game mode to score against.
- **It is not in the party box.** Drafting a goal from the random hand would mean several goals in
  one level; the objective is a property of the level. Drag `GetawayFinish` in from the C++ Classes
  folder to place one, or move the existing one.
- `OnFinished` is a multicast delegate on the actor, fired on every machine. Scoring does not use
  it - that goes through the game mode - so it is free for cosmetics.

The run clock is `AGetawayGameState::GetRunTime()`, with `GetBestTime()` alongside it and each
player's own time on their `AGetawayPlayerState`. The HUD shows the clock top-centre during a run.

**In the test course the finish is at X=9300**, on the final floor between the breakable blast wall
and the rocket ledge. It was put there so finishing would not be gated behind the C4 rocket jump
while that was unproven. The jump *is* proven as of 2026-10-16, so moving the finish up onto
`RocketLedge_500cm` would now make the rocket jump the last challenge - a one-drag change in the
editor, left undone because it is a difficulty decision rather than a correctness one.

Play-tested 2026-10-16: reaching it ends the run and returns to placement as intended.

### What is deliberately not here yet

- **The rest of the round loop:** deaths. More than one player, scoring and per-player finish
  arrived on 2026-10-17 - see *The round loop*. The party box still deals from the whole catalog
  rather than a per-round deck.
- **The ghost is drawn with debug lines**, which are compiled out of Shipping builds. That keeps the
  plugin free of content, but it wants a real translucent material before release.

`bStartInPlacement` on the game mode defaults to **on**, so pressing Play now opens in the placement
camera rather than dropping you straight into the run. Press Enter to run immediately, or untick it
to restore the old behaviour for pure movement testing.

### Fixed after the placement play-test (2026-10-16)

- **The ghost did not show rotation.** Pressing R turned the piece but the ghost box looked
  identical, so you only saw which way a wall faced once it was down. `FGetawayPlacement::Size` is
  already the *world-space* extent (X and Y swapped on a quarter turn), and the ghost was drawn with
  that extent **and** the rotation, which swapped it straight back. It was drawn with
  `FQuat::Identity` from here until 2026-10-26, when the ghost and the blocked test both moved to
  the piece's own `LocalSize` turned by its own rotation - an oriented box, which is the version
  that can also show a piece lying over on a slope. The same double-rotation was in the blocked
  test, so a turned wall was being validated against a box lying the other way - also fixed.
- **Breakables did not come back between rounds.** `Break()` ended with `SetLifeSpan(ScrapLifetime)`,
  which deleted the whole actor, so a smashed panel was gone for the rest of the session and the
  course wore away run after run. The actor now survives: a timer clears only the scrap components,
  and `AGetawayBreakable::Restore()` makes the mesh solid and visible again. `StartPlacement()` calls
  it on every breakable and every collapsing bridge, so each round begins on an intact course while
  the pieces you placed stay put. Its mesh is also Movable now, since it hides and returns at
  runtime. Verified by script: break then restore returns broken/visible/collision to their starting
  values and the actor is still alive afterwards.

### Fixed: ramps could not be mantled (2026-10-16)

The mantle landing test sampled the surface height at a single probe point and sat the landing
capsule 2 cm above it. The capsule is R (40 cm) wide, so on a slope the ground keeps climbing across
its own radius - 13.3 cm at the ramp's 18.4 degrees - and the uphill side was buried in the ramp.
Both the standing and the crouching fit test therefore failed and the mantle was refused, from every
approach.

It now lifts the landing by the rise the slope makes over the capsule radius
(`R * tan(slope)`, taken from the landing surface normal). On level ground the normal points
straight up and the lift is zero, so flat mantles are untouched. The climb sweep and the final
standing position get the same allowance. Measured before and after: the landing capsule was blocked
by the ramp at a quarter, half and three-quarters along, and is clear at all three now.

Worth knowing: the wall probe was never the problem - it finds the ramp's back and side faces fine,
with exactly vertical normals. And one limit remains by design: **the top of a ramp is 200 cm and
`MantleMaxHeight` is 170**, so the last ~15% of a ramp's length, and its tall back face, are above
the mantle window. That is the lab's 1.7 m figure and it applies to every piece, not just ramps;
raising it would make 2 m objects mantleable everywhere but diverges from the prototype. **Considered
and declined 2026-10-16** - left at the lab's 170 to see how the ramp feels with the landing fix
first. Do not raise it without asking.

### Still unverified (placement and pieces)

The placement loop itself is confirmed working: place a piece, Enter to run, and the character moves
(that last part only after the Enhanced Input context fix above). Not yet played: the ramp, the
bridge, and how any of it feels. What *was*
checked without playing, by script against the running engine: the catalog deals eight pieces with
the right sizes and flags, the wedge builds with the same triangle winding the engine's own
`GenerateBoxMesh` uses (so it is not inside out), traces onto the slope land exactly where the maths
says with a constant normal, and the bridge's collapse and restore flip its state. **Not** checked:
how any of it feels, whether 0.45 s is the right hold, whether the tip impulse reads as a collapse
or as a shrug, and whether the debug-line ghost is legible for a wedge.

The build clock (2026-10-17) was **played and signed off the same day** - see *Played and signed
off* at the end of this file. It was written wondering whether 30 s is enough to fly out, pick a
spot and place three pieces, and whether the 1 s hand-spent beat reads as a hitch. Both turned out
fine; both are single values on the game mode if that changes.

One oddity to settle while play-testing: the **wall and breakable panel are rotated 90° from the
lab**. The lab's wall at facing 0 lies *across* your view (its thin axis is the facing axis); here
`Size` is 400x50x400, so the thin axis is Y and the wall runs *away* from you. The comment under
*How placement positions a piece* claims the lab's behaviour. The directional pieces follow the lab
(length along the facing), so if the wall should too, swap X and Y on the `wall` and `break` catalog
entries. It was left alone because a wall along your view is arguably the better wallrun piece -
that is a feel question, not a code one.

## The multiplayer spike (2026-10-17)

A throwaway-quality spike to settle the architecture decision in *Next steps* 3 before the round
loop gets rebuilt. The question it answers: **can this movement go online client-authoritative, and
does that leave the feel alone?** The answer, structurally, is yes - **`UGetawayMovementComponent`
was not touched, not one line**, and the whole of the custom movement state (wall normal, air jumps,
slide, mantle interpolation) stayed on the machine that owns the character. There is no
`FSavedMove_Character`, no prediction and no replay anywhere in the plugin.

It is **not play-tested**. Read *What the spike does not prove* before trusting it.

### How to run the two-player test in the editor

1. Double-click `Getaway.uproject`. Let it compile if it asks.
2. On the toolbar, click the **arrow next to the Play button**.
3. Under *Multiplayer Options* set **Number of Players** to **2** and **Net Mode** to
   **Play As Listen Server**. Leave *Run Under One Process* ticked.
4. Press **Play**. Two windows open: the first is the host, the second is the client.
5. Click into a window to give it the mouse. **Shift+F1** gives the mouse back so you can switch
   windows. Both players start in the placement camera, sharing one 30 s clock and each holding
   their own three pieces.
6. Fly, left click to place, **Enter** to start your run. Each window shows the *other* player as a
   grey cylinder with a nose block (you never see your own body), plus a line of on-screen text
   naming what they are doing - GROUND, AIR, SLIDE, WALLRUN, MANTLE - and their speed in u/s.
7. Since 2026-10-17 a scoreboard sits top-right with both scores and how each player's round is
   going, and the round is scored when the last runner is done. See *The round loop* at the end
   for what the numbers mean.

Two things worth knowing before reading the result as a bug:

- Both players spawn from the **same PlayerStart**, so they start on top of each other and the
  engine nudges them apart. Drag a second `PlayerStart` into TestCourse a few metres to the side if
  that is annoying.
- In PIE the on-screen state readout is drawn by `GEngine`, which is shared between the two windows,
  so each window may list the players from both. That is a PIE artifact, not the replication.

### What changed

| Where | What |
|---|---|
| `AGetawayCharacter` | `bReplicates`, **movement replication off**, an unreliable Server RPC at 25 Hz, proxy interpolation, a visible body |
| `UGetawayMovementComponent` | **nothing** |
| `AGetawayGameMode` | a placement pawn per controller, `StartPlacementFor` / `StartRunFor`, `HandleStartingNewPlayer`, pieces spawn with their scale in the transform |
| `AGetawayPlacementPawn` | replicates its hand to its owner, places through a Server RPC, ticks only where it is controlled |
| `AGetawayPiece`, `AGetawayRamp`, `AGetawayBreakable` | `bReplicates`, `bAlwaysRelevant` |
| `AGetawayHUD` | reads the hand off the pawn, since a client has no game mode |

#### The character: client authority in two flags and one RPC

The owning client sends `FGetawayNetMove` - location, velocity, yaw, pitch and a one-byte state -
25 times a second, unreliably. The server applies it to its own copy and republishes it; everyone
else interpolates toward it. `NetSendRate`, `ProxyInterpSpeed`, `ProxyExtrapolationLimit` and
`ProxySnapDistance` are all on the character in the Details panel.

The part that actually matters is two flags set on the movement component in the character's
constructor:

- `bIgnoreClientMovementErrorChecksAndCorrection = true` - **this is why the movement component did
  not need rewriting.** The engine's `ServerMove` path still runs, but the server is forbidden from
  correcting the client. Without it, every wallrun, slide and mantle would read as client error,
  because none of that state is in a saved move, and the server would yank the player back several
  times a second.
- `bServerAcceptClientAuthoritativePosition = true` - the server takes the client's position as
  truth rather than its own replay, so error cannot accumulate on the server's copy either.

Engine movement replication is then switched off (`SetReplicateMovement(false)`) so the server's
always-slightly-wrong simulation is not fighting `FGetawayNetMove` on everyone else's screen.

Three details that are load-bearing and easy to undo by accident:

- **A simulated proxy's movement component is switched off** (`DisableMovement()` plus the component
  tick). Left on, it applies gravity and its own collision on top of the interpolation and drags
  remote players through floors.
- **The role is checked, not just the controller.** `ROLE_AutonomousProxy` is set when the actor
  channel opens, a frame or two before the replicated `Controller` arrives. Asking only
  `IsLocallyControlled()` makes a client briefly treat its own character as someone else's and shut
  its own movement down. `SetProxySimulation` is reversible for the same reason.
- **The body is `bOwnerNoSee`**, so first person stays first person. It is an engine cylinder scaled
  to 80 x 180 cm plus a cube for a nose, squashed when the state byte says crouch or slide. Spike
  scaffolding: the real game wants a skeletal mesh, and this plugin ships no content.

#### Placement

`AGetawayGameMode::PlacePiece` is server-only and now spawns each piece **with its scale in the
spawn transform**. That is not cosmetic: an actor's spawn transform travels with the actor to the
clients, while a later `SetActorScale3D` does not, so a client would otherwise see every piece as a
100 cm cube. It also means `AGetawayRamp::OnConstruction` builds its wedge at the right scale first
time, on every machine.

Clicking no longer places anything. The client computes where it is aiming - only it can know that -
and calls `ServerPlacePiece(Slot, Centre, Steps)`. The server **looks the piece up in the hand it
dealt**, not in anything the client sent, and **re-runs the overlap test itself**; either check
failing logs a refusal and spawns nothing.

A client has no game mode at all (`GetGameMode` returns null there), which is why the hand is
replicated onto the placement pawn - `COND_OwnerOnly`, and the pawn itself is `bOnlyRelevantToOwner`
so nobody holds a copy of anyone else's fly camera. `AGetawayPlacementPawn::Tick` returns early when
it is not locally controlled, or the server would fly its copy of every client's camera around and
draw a second placement ghost into the host's viewport.

### What was verified, and how

Everything below was checked **by script**, not played. Two routes:

**1. Class defaults, through Python against the engine** - 17 checks, all passing: the three piece
classes replicate and are always relevant; the character replicates with movement replication off
and both client-authority flags set; the body mesh resolves and is owner-no-see; its pivot is
centred (origin 0, extent 50) so the 0.8 / 1.8 scaling gives an 80 x 180 cm body; the placement pawn
replicates without movement.

**2. A real networked session, headless** - a dedicated server plus two clients, three separate
processes talking over the loopback at 127.0.0.1:7777, run from the command line with `-nullrhi`.
From the logs of that run:

- Both clients joined and **each got its own placement pawn with its own hand**
  (`PlayerController_0 is building, with GetawayPlacementPawn_0`, then `_1` with `_1`). The old
  player-0 assumption would have had them fighting over one.
- The build clock expired and **both players swapped to their own character**
  (`PlayerController_0 is running as GetawayCharacter_2`, `PlayerController_1 ... _3`).
- **The server accepted client-authoritative updates for both characters**, and **each client
  received the other player's updates** (`first update for remote player ... at V(X=150.00,
  Y=80.30, Z=92.20)`) - so the RPC, the relay and the `COND_SimulatedOnly` replication all connect.
- **A piece placed by one client appeared on the other**, once as a ramp and once as a breakable
  panel, at the right location **and the right scale** (`GetawayRamp at V(X=1050.00, Z=392.20) scale
  V(X=6.00, Y=4.00, Z=2.00)`), exactly one copy per client.
- **The server refused a second placement at the same spot** (`refused c4 at V(X=1050.00,
  Z=392.20), something is already there`).
- No crashes, no `LogNet` failures and no warnings from the plugin in any of the three logs.

It **compiles clean**, as the plugin always has: UHT with `-WarningsAsErrors` passes and every .cpp
compiles with no warnings.

That headless test is still in the code, behind a command-line switch, so it can be re-run. Start a
server:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\natha\Documents\Unreal Projects\Getaway\Getaway.uproject" /Game/Maps/TestCourse -server -port=7777 -nullrhi -nosound -unattended -nosplash -ABSLOG="server.log"
```

then one client per window with `127.0.0.1:7777 -game -nullrhi -unattended -GetawayNetTest=place`
(asks the server for a piece on a timer, twice, the second time at the spot it just filled) or
`-GetawayNetTest=watch` (just lists the placed pieces it can see). It is
`AGetawayPlacementPawn::NetSelfTestPlace` / `NetSelfTestCount`, compiled out of Shipping builds;
delete it whenever it stops being useful.

### What the spike does not prove

Written before anyone played it; it **was** played with two players on 2026-10-17 and signed off -
see *Played and signed off* at the end. These were the open questions at the time, kept because
they say where to look first if any of it regresses:

- **How remote players look.** The headless clients never moved, so the interpolation was never fed
  real motion. Whether 25 Hz plus `ProxyInterpSpeed` 18 reads as smooth running or as sliding and
  stuttering is the headline unknown, and it is the thing to watch first. The knobs are all on the
  character: raising `NetSendRate` and `ProxyInterpSpeed` tightens it at the cost of bandwidth and
  jitter.
- **Whether the feel survived.** The argument is structural, not felt. The listen-server **host** is
  certainly untouched: an authoritative, locally controlled character runs exactly the standalone
  path. The **client** is the one to judge - its movement is still entirely local and no latency
  touches it, but its movement component now goes through `ReplicateMoveToServer` rather than
  straight to `PerformMovement`, and that path does **client move combining**, which can replay a
  short move from its start location. If the client's movement feels subtly different from the
  host's, try `p.NetEnableMoveCombining 0` in the console before suspecting anything else.
- **Capsules still block each other.** A proxy is moved with `SetActorLocation` without sweeping, so
  one interpolating into you leaves the engine to push you out, which may read as being shoved. If
  players should pass through each other - arguably right for this game - that is a collision
  response change on the capsule, and it is a design decision, so it was left alone.

### Deliberately not done (and what it would take)

The things the spike was told to leave undone, because they are the real work it exists to inform.
The round itself - the global phase, clock and hand, `Backspace` restarting everybody's round, and
the single global finish - was **done on 2026-10-17**; see *The round loop* below. What is left:

- ~~**`AGetawayBreakable` replicates its existence but not its state.**~~ **Done 2026-10-17**, see
  *Fixed: breaking and restoring now reach every machine*.
- ~~**`AGetawayCollapsingBridge`, `AGetawayC4Pickup`, `AGetawayFinish` and `AGetawayC4` are
  untouched.**~~ **Done 2026-10-18** for the bridge, the pickup and the C4, see *Fixed: the bridge,
  the C4 and the pickup now reach every machine* at the end; the **finish** was done with the round
  loop. Note the spike guessed wrong on two of them: the C4 turned out to be better served by a
  plain server-spawned replicated actor than by the "client-spawned cosmetic charge" it proposed,
  and the bridge did not need a "collapsing since time T" - replicating the state enum was enough,
  because both machines start falling from the same place on the same frame.
- **The server cannot tell *where* a client placed from.** It validates the piece and the fit, but
  the placement pawn's transform is not replicated to the server, so there is no camera position to
  check a distance against. A client could place a piece it legitimately drafted anywhere in the
  level. Fixing it means replicating the fly camera's position, which is why it was not done for a
  spike.
- **`R` to restart is now standalone-only.** Reopening the level would have dropped a client's
  connection, or thrown everyone off the listen server.

## The round loop (2026-10-17)

The round moved off the game mode and onto `AGetawayGameState` and `AGetawayPlayerState`, so two
people can actually play a round against each other rather than share one. This is the work the
spike above existed to inform, and it is built on top of it: **`UGetawayMovementComponent` was not
touched, and neither was any of the client-authority machinery** - the two flags, the 25 Hz RPC,
the proxy interpolation and the disabled proxy movement component are all exactly as the spike left
them.

It **compiles clean** and a three-process networked session was driven end to end by script. **It
was played with two players on 2026-10-17 and signed off** - see *Played
and signed off* at the end. *What this does not prove*, at the bottom, was written before that.

### Who owns what now

| Where | What it holds |
|---|---|
| `AGetawayGameMode` | The authority. Every transition, the catalog, the deal, the scoring table. Server only, as always |
| `AGetawayGameState` | Phase, round number, the build clock, the run clock and its deadline, the session best, the standings |
| `AGetawayPlayerState` | **Your** hand, your run state, your finish time and place, your best, your score |

The split is the whole point: a client has no game mode at all, which is why the build clock used
to draw for the host and nobody else. Now the HUD reads nothing but the two replicated states.

- **The clocks are absolute server times, not remaining seconds.** `PlacementEndServerTime` and
  friends are measured against `AGameStateBase::GetServerWorldTimeSeconds()`, which works on both
  sides, so a client counts down smoothly on its own instead of stepping whenever a packet lands -
  and the clock does not have to be replicated every tick to manage it.
- **Your hand is yours.** `COND_OwnerOnly`, because nobody else needs to know what you are holding.
  Score, run state and finish time are public: the scoreboard is the point of the game.
- `AGetawayPlacementPawn` no longer carries the hand. It reads its player's `PlayerState`, which
  also means the hand survives the pawn being destroyed at the start of your run.

### What a round does

1. `StartPlacement()` bumps the round, deals **every player their own hand**, restores the
   breakables and any fallen bridge, sweeps up any thrown C4 still lying about, starts one shared
   `PlacementTime` (30 s) clock, and puts everyone in a fly camera.
2. You place. Spending your hand marks you **done** after `LastPieceDelay`; Enter marks you done by
   hand. Nobody runs until **everybody** is done - or the shared clock runs out, which starts the
   run whatever anyone is still holding. See *Placement is shared* below.
3. You run. Reaching `AGetawayFinish` records your time and your place on your own `PlayerState`.
   The first player home starts a `FinishGraceTime` (10 s) hurry-up for everyone else, and
   `RunTimeLimit` (120 s) calls the run whatever happens.
4. When nobody is left running the round is scored, the banner sits for `FinishLingerTime` (2.5 s),
   and the next placement phase opens.

- **A player still in the fly camera holds the round open.** `CheckRoundOver` treats Building the
  same as Running. Shared placement means nobody can set off early any more, so the window this was
  written for is gone; it is kept because a player who joins mid-round, or one the clock has not yet
  swapped to a character, is still a player the round should not be called without.
- **The run clock needs a limit because a player can always refuse to finish.** Spikes can kill you
  as of 2026-10-19, but nothing forces you onto them: someone who simply stands still would hold
  the round open forever. `RunTimeLimit` 0 turns it off and is fine single-player.
- **`Backspace` now forfeits your own run** instead of calling `StartPlacement()` for everybody.
  You go back to the fly camera, you score nothing that round, and nobody is left waiting on you.
  Alone that means Backspace costs you the 2.5 s banner before the next round, which it did not
  before - a small, deliberate change.
- **The placement RPC looks the slot up in the asking player's hand.** `ServerPlacePiece` reads
  `GetGetawayPlayerState()`, not the game mode, so the spike's guarantee still holds and is now
  per player: you can only place what *you* were dealt.

### Scoring (simplified 2026-10-19)

**A point for getting there, a point for getting there first, and nothing at all in a round
everybody cleared.** That is the whole system. It is on the game mode under *Getaway|Scoring*:

| Knob | Default | What it does |
|---|---|---|
| `GoalPoints` | 1 | Reaching the objective at all |
| `FirstGoalBonus` | 1 | Extra for being first home - including when you were the only one |
| `bNoPointsIfAllFinish` | on | Everybody got there, so the level was too easy and nobody scores |

So, with two players: one home scores 2 and the other 0; both home scores 0 each; neither home
scores 0 each. Alone, finishing is always 2, because you were trivially first.

- **Being the only one home is no longer worth extra by itself.** It used to pay a `SoloGoalBonus`
  of 2 on top of the goal point - the big payoff that made a cruel level worth building. That is
  now carried entirely by `bNoPointsIfAllFinish`: you do not get *more* for being alone, the others
  get *nothing*, and a round everybody cleared is worth nothing to anybody.
- **`FirstGoalBonus` no longer needs company.** It used to require another finisher, with the solo
  case handled by the bonus above; now place 1 pays whether or not anybody else made it.
- **The leader scale-back is gone.** `LeaderBonusScale` halved the leader's bonuses to soften the
  skill gap, and went with the simplification. *Next steps* 4 originally asked for it, so it is a
  deliberate drop rather than an oversight - put it back if one player starts running away with
  sessions, and note that it needs the standings read *before* the round pays out, or a player
  counts as the leader against their own new score.
- **The all-finish rule was kept on purpose.** It was the one part of the old table that is not a
  points award, and it is the only thing left that rewards building something hard - without it a
  straight corridor pays everyone. **Asked and confirmed 2026-10-19.**
- **Kills pay nothing** (decided 2026-10-19, when spikes made kills possible at all). Points come
  from the objective and nowhere else. `TrapKillPoints` existed, defaulted to 2 and was deleted
  rather than zeroed, because two points for a kill was more than winning a round is worth and a
  dead knob in the table reads like an oversight. Killing somebody is already its own reward: it
  keeps them off the scoreboard, which under `bNoPointsIfAllFinish` is the difference between
  everyone scoring nothing and you scoring two.
- **Kills are still counted.** `AGetawayGameMode::PlacePiece` records who placed each piece and
  `RegisterTrapKill(Victim, Piece)` credits the placer - your own trap catching you does not count
  - but nothing reads `GetTrapKills()` any more. It is kept for the log line, which is how you
  find out your trap worked, and because a kill count is the obvious thing to put on the
  scoreboard if that is ever wanted. Delete it if it never is.

### What was verified, and how

By script, over a real dedicated server plus clients on the loopback, with the clocks temporarily
shortened and then restored. From the server logs of those runs:

- **Each player drafts their own hand.** `round 2, placement for 2 player(s), 3 piece(s) each`,
  with each controller getting its own pawn and its own three pieces, re-dealt every round. The
  client's own net-test log shows it asking for a different piece each round (`ramp`, `floor`,
  `wall`) and the server accepting the slot out of that player's hand.
- **Every scoring branch, each one actually observed** - but **of the old table**, which was
  simplified on 2026-10-19. The numbers below are not what the game pays now; they are kept because
  they show the branches were reached, not because the totals still hold:
  - *Solo goal*: `home in 7.05s  +3  (3)` against `did not finish  +0  (0)`. Pays 2 now.
  - *Leader scale-back*: the same player winning again the next round took `+2`, the solo bonus
    halved. That rule no longer exists.
  - *Everybody home*: `everybody got there, so nobody scores`, both players `+0`. Unchanged.
  - *Some home*: three players, two finishing - first `+2`, second `+1`, the straggler `+0`.
    Unchanged, by coincidence rather than design.
- **Per-player finish.** `finished 1st in 4.72s` then `finished 2nd in 7.33s` in the same run, with
  the round carrying on between the two - the first player home no longer ends it for everybody.
- **Both ways a round can end.** `round 1 over` when the last runner got home, and `round 1 over
  (time)` when the hurry-up expired on a straggler.
- **The loop runs round after round** - five rounds back to back in one session, scores
  accumulating, with no crashes, no `LogNet` failures and no warnings from the plugin in any of the
  three logs.

**Rebuilding the test.** The spike's headless harness is still there, with one addition: a
character given `-GetawayFinishAfter=<seconds>` teleports itself onto the objective that long after
it spawns, because a headless client has no hands to run a course with. It goes through the honest
path - the client moves itself, the server takes that position as truth, and the server's own
overlap is what scores - so giving it to one client and not another is how the solo-goal and
everybody-home cases above were produced. It is in `AGetawayCharacter::BeginPlay` under
`#if !UE_BUILD_SHIPPING`; delete it with the rest of the net-test scaffolding.

### What this does not prove

Written before anyone played it; it **was** played with two players on 2026-10-17 and signed off -
see the end of this file. These were the open questions at the time:

- **Whether the numbers are any good.** 1 / 2 / 1 and a half-value leader bonus are a starting
  table, not a tuned one, and whether a 10 s hurry-up feels generous or mean is a feel question.
  Every knob is on the game mode.
- **The HUD was never looked at.** The scoreboard, the hurry-up countdown and the round banner all
  compile and read from replicated state, but no human has seen them draw. The scoreboard hides
  itself below two players, so single-player testing will not show it at all.
- **`Backspace` and the give-up path were not exercised** by the headless runs, which have no way
  to press it.
- **Trap kills are untestable** until something can die.

### Fixed after reviewing the round loop (2026-10-17)

Two things found by reading the round-loop code before it was ever played. Both compile clean;
both were played on 2026-10-17 and are good.

- **You could build during somebody else's run.** `Backspace` now forfeits through `GiveUpRunFor`,
  which hands back the fly camera *mid-round* - and `StartPlacementFor` does not clear your hand,
  only `StartPlacement` does. `ServerPlacePiece` checked the hand slot and the overlap and nothing
  else, so a player who dropped out could keep dropping walls into a course the others were still
  running. There is now `AGetawayPlacementPawn::CanBuildNow()` - phase is Placement and the round
  is not over - checked **on the server in the RPC**, which is the one that counts, and on the
  client to save the round trip. The ghost and the HUD line follow it, so the camera stops
  inviting a click it will not honour. Alone this only closes the 2.5 s banner window; with two
  players it closes the whole run.
- **The breakable camera shake was distance-blind.** `GetPlayerCharacter(this, 0)` got an
  unconditional `AddCameraShake` wherever the viewer was standing. It now fades over `ShakeRadius`
  (1500 cm) and does nothing beyond it. Worth being precise about why: **breaking is still local**
  (`Break()` is called from the movement component and from C4, on the machine that owns the
  player, and nothing replicates it), so today the only person shaken is the one who did it and
  the old code was not actually misbehaving. This is a latent bug paid off early - the moment
  `Break()` is replicated, an unconditional shake is a phantom jolt for everyone in the level.

**Reviewed but found correct**, so nobody needs to look again: per-round counters (`FinishersThisRound`
is reset in both `StartPlacement` and `BeginRun`, `TrapKills` in `BeginRound`); `Logout` calls
`CheckRoundOver`, so a quitter cannot hang a round; `StartRunFor` cannot double-fire; the finish is
authority-guarded before it touches the game mode; the HUD reads only replicated state; and the
scoring table matches Ultimate Chicken Horse, with the all-finish nullify and the solo bonus both
correctly disabled single-player and the standings read before payout.

**The headless harness did not run** when this review tried it: the command documented above, plus
two variants, exited during engine init without loading the map and logged nothing. That may be the
invocation rather than the harness - `-ABSLOG` writes wherever it is pointed and no logs from the
earlier runs survive in `Saved/`. Either way, **the logged evidence quoted under both the spike and
the round loop is unreproduced**, so treat the scoring branches as reasoned rather than observed
until somebody watches them.

### Placement is shared (2026-10-17)

Everybody builds, then everybody runs. Nobody sets off while another player is still placing, which
is how Ultimate Chicken Horse works and what makes the sabotage matter: the piece you drop is one
your opponent has to deal with on the same run you do, not one they met two rounds ago.

`AGetawayPlayerState::bDonePlacing` is the whole mechanism. You are *done* when your hand is spent
(after the `LastPieceDelay` beat) or when you press Enter with pieces to spare.
`AGetawayGameMode::CheckEveryoneReady()` starts the run the moment nobody is `IsStillPlacing()`.

- **Enter no longer starts your run**, it says you have finished building. It used to be a personal
  start, which is exactly the thing that made placement not shared. `ServerStartRun` now calls
  `MarkDonePlacing`.
- **Done is final for the round.** `CanBuildNow()` goes false once you are done, so you cannot keep
  building while the others wait - otherwise "done" would mean nothing. You keep the fly camera and
  can still look around, which doubles as the "short look-around before the run" in *Next steps* 4.
- **The build clock still overrides everything.** It is the only thing that can start a run with
  somebody still holding pieces, and it is what stops one player who has wandered off from freezing
  the session. That is why it is not optional in multiplayer, though `PlacementTime` 0 is still fine
  alone.
- **Quitting releases the wait.** `Logout` calls `CheckEveryoneReady` as well as `CheckRoundOver`,
  so the last player standing is not stuck waiting on somebody who has gone.
- **Single-player is unchanged**, which is worth knowing before testing: you are the only player, so
  you are the last one done, and the run starts exactly when it used to.

The HUD says who it is waiting for - "Done - waiting for 2 other players" under the crosshair, and
the build clock's subtitle changes to say the clock is for the others now. Both count off the
replicated `PlayerArray`, so they draw on a client with no game mode.

**Played with two players on 2026-10-17 and it works.** The headless harness still does not run
here, so that remains the one unverifiable part of this file.

## Played and signed off (2026-10-17)

The multiplayer work was play-tested **with two players**, and it works. This is the first thing in
this file above the movement itself that rests on somebody actually playing rather than on a script
or on reading. Treat the "what this does not prove" lists above as history: they describe what was
open before this, not what is open now.

Confirmed by playing, two players, listen server:

- **Client-authoritative movement holds up.** Remote players read as running rather than sliding or
  stuttering at the spike's 25 Hz and `ProxyInterpSpeed` 18, and the client's own movement feels
  like the host's - so `p.NetEnableMoveCombining` never had to be touched.
- **The shared placement phase.** Both players build, nobody sets off early, the round starts when
  the second one is done, and the build clock's 30 s is about right.
- **The round loop end to end**: per-player hands, the run, the finish, scoring, the banner, and
  straight into the next round.
- Single-player is unaffected by any of it, as intended.

The one fault found was **breakables not replicating**, which was known and is now fixed below.

### Fixed: breaking and restoring now reach every machine (2026-10-17)

Found by playing: a panel you smashed vanished on your screen and stayed solid for everyone else,
because `Break()` ran only on the machine that detected the smash and nothing told the others. The
same was true of `Restore()`, so a client's course never came back between rounds either.

All of it lives in `AGetawayBreakable`; the two callers (`UGetawayMovementComponent::SmashAhead`
and the C4 blast) are untouched and still just call `Break(Velocity)`.

- **A client breaks it locally first, then tells the server.** Smashing a panel at 900 cm/s is the
  one thing here that cannot wait for a round trip, so the client does not: it bursts the panel on
  the spot and reports it, and the server repeats it to everybody else. That is the same bargain as
  the movement - the client is trusted, the server relays.
- **The report goes through the player's character, not the breakable.** A server RPC is only
  routed from an actor the client owns, and nobody owns a panel sitting in the level, so
  `AGetawayCharacter::ServerReportBreak` carries it. That is the only reason it is on the character.
- **`bBroken` is replicated with an OnRep as well as the multicast.** The multicast is the event and
  reaches whoever is connected; the OnRep is the state, and catches somebody who joins after the
  break - they get the panel hidden, without a scrap burst for something they missed.
- **Duplicate reports are normal, not an error.** The server usually smashes the same panel itself a
  frame later, and a chain reaction reports every link from both ends; `bBroken` makes the second
  one a no-op.

Built clean. **Not played yet** - this is the one change in this file that went in after the
play-test above.

### Fixed: the bridge, the C4 and the pickup now reach every machine (2026-10-18)

Reported from play: "c4 and the collapsing bridge don't show up on the other player's screen." The
root of it was blunter than it sounded - **`bReplicates` was simply false** on
`AGetawayCollapsingBridge`, `AGetawayC4` and `AGetawayC4Pickup`, so those three did not exist for
anybody but the machine that made them. A bridge you placed was invisible from the moment it was
placed, not just when it fell. The C4 pickup had the same hole and was not reported only because
nobody had drafted one yet.

`AGetawayFinish` is the fourth non-replicating actor and is deliberately left alone: it is placed in
the map by hand, so it already exists on every machine, and `Reach()` is authority-guarded.

#### The bridge

`bReplicates`, `bAlwaysRelevant`, and the state enum (now a `UENUM`) replicated with an OnRep.

- **The server decides, every machine drops its own copy.** The stand probe and the timers run on
  the authority only - it is the only machine that sees every player, and two copies polling
  independently would give way at different moments.
- **Movement is deliberately not replicated.** A rider needs the platform under them to be local
  and immediate, not streamed a frame late. The two copies therefore fall independently from the
  same impulse and will not land identically, which does not matter: by the time it has stopped
  moving the ride is over. What matters is that both start falling on the same frame, from the same
  place, and `StartFalling` resets the rattle first precisely so they do.
- The rattle is cosmetic and runs wherever the state says Warning, so the warning reaches both
  screens.
- Known rough edge: a client joining while a bridge is already down gets `Fallen` on initial
  replication and drops it from the start position, a second late. It respawns itself out of that.

#### The C4

`bReplicates` with **movement replication on**, and the throw moved to the server.

- **Only the server spawns a thrown charge**, from `AGetawayCharacter::ServerThrowC4`, so there is
  one brick and everybody watches the same arc. That costs the thrower a round trip before the
  charge appears. A lobbed object can afford that; the flight is also short enough that two
  machines integrating it separately would disagree about where it stuck, which is why it streams
  rather than simulating locally.
- **The detonation does not wait, and that is the whole trick.** A rocket jump is aimed and timed
  to the frame. The owning client applies *its own* launch the instant it presses F
  (`PredictLocalBlast`), then asks the server for the real bang; `MulticastDetonate` skips the
  launch on the machine that beat it to it. The server does what only it can - bursting breakables,
  shoving physics - and the breakables replicate themselves from there.
- **Each machine launches only its own character**, because under client authority that is the only
  one it may move. Between them that covers everybody.
- Unchanged on purpose: **only the thrower is pushed**. A blast that shoved everyone nearby might
  suit a party game better, but that is a design change, not a networking fix.
- `Detonate` hides the charge through the multicast and uses `SetLifeSpan` rather than `Destroy()`,
  because tearing down the actor channel in the same frame can take the queued multicast with it.

#### The pickup

`bReplicates` so it exists for everyone - and then a decision worth knowing about, because it is
visible in play: **a pickup is per player**. Each of you can collect it once per respawn, and each
of you sees your own pad empty. `TryGive` returns early unless the character is locally controlled,
because the charge count is that machine's own, exactly like the rest of client authority.

The alternative is a shared pickup - first one there gets it, everyone sees it go - which needs the
grant routed through the server like the breakable's, and `ChargesHeld` replicated to its owner.
**Asked and settled on 2026-10-18: keep it per player.** It is recorded under *Decisions already
made* with what switching it would take.

Built clean. **Not played.**

## Spikes, and death (2026-10-19)

The first trap, and with it the first way to die. Those arrived together because they had to:
nothing in the game could kill you before this, which is why `RegisterTrapKill` had sat wired and
dead since it was written. Spikes are its first live caller.

### The piece

`AGetawaySpikes` - a plate of points, catalog id `spikes`, 400 x 400 x 50, the same three numbers
as a wall read as a plate rather than a slab. Nine cones on a base, built from engine BasicShapes
inside a 100 cm cube, so `bScaleToSize` scales it like everything else.

- **It never blocks you.** Meshes have collision off and the trigger is QueryOnly overlapping pawns
  only, exactly like `AGetawayFinish`. Running into spikes kills you; it does not stop you dead and
  *then* kill you, and you can wallrun past a spiked wall as long as you stay off it.
- **Armed only during a run.** `StartPlacement` disarms every bed, `BeginRun` arms them. Belt and
  braces - a fly camera has no collision to trigger them with - but it means a dead or finished
  player wandering through during the banner does not set anything off.
- Replicated and always relevant, the lesson of 2026-10-18.

### Spikes belong to what they are stuck to (added 2026-10-19)

Spikes are not free-standing scenery: they are **attached to the thing they were placed on**, and
`AGetawaySpikes::Host` is who that is. The server works the host out itself, by tracing back out of
the middle of the plate's face - it is not sent by the client, so nobody can nominate what their
spikes are glued to.

- **Break the wall, lose the spikes.** If the host is an `AGetawayBreakable`, the spikes bind to
  its `OnBroken` and hide themselves, disarmed; `OnRestored` (new, the symmetric half that did not
  exist before) brings them back with the panel at the start of the next round. So a spiked
  breakable panel has an answer: smash it and the trap goes with it.
- **A host that moves carries them.** They are attached with `KeepWorldTransform`, so spikes on a
  collapsing bridge ride it down and come back up when it respawns. That is why this is the one
  placed piece with **movement replication on** - an actor's attachment replication rides on it,
  and without it the spikes would only fall on the server's screen.
- **Nothing about the break goes over the wire.** `OnBroken` and `OnRestored` already fire on every
  machine, so each copy of the spikes reacts locally off its own copy of the panel. Only `Host`
  itself is replicated, and only so a client can find the panel to bind to - with an `OnRep`,
  because the spikes can arrive before their host does.
- Spikes on bare level geometry have no host, which is fine: nothing to break, nothing to follow.

### Surface mounting - a new kind of piece

Asked for: *"the same size as the walls except you can only place it attached to a wall or floor."*
That is `FGetawayPieceDef::bSurfaceMounted`, and it is the first piece that is neither free-standing
nor directional:

- **It turns to face the surface.** The piece's own +Z points out of whatever you aimed at, so one
  catalog entry gives you a floor mat, a spiked wall and a spiked ceiling. `Size` is therefore the
  plate in its *own* axes (400 x 400 across, 50 thick), and the world extent comes out as
  400x400x50 on a floor and 50x400x400 on a wall facing +X. **R** spins it about the normal, which
  on a square plate is invisible.
- **It is the first piece that does not come out at its catalog size**, because of the trim below.
  `FGetawayPlacement::LocalSize` carries what it actually came out as, the RPC sends it, and
  `AGetawayGameMode::PlacePiece` takes a `SizeOverride`. Everything else still spawns at `Def.Size`.
- **The points are laid out in `OnConstruction` from the plate's real size**, not baked into the
  constructor: a fixed pool of components, the count taken from the plate's world size at
  `SpikeSpacing` (133 cm), and each one's relative scale dividing the actor scale back out so a
  point is always `SpikeWidth` x `SpikeHeight` (100 x 35 cm) in the world. Without that last part
  a trimmed plate would show the same nine points squashed into a narrower strip. Same trick as
  `AGetawayRamp`: the scale travels in the spawn transform, so every machine builds its own.
- **It cannot be placed in mid-air.** The ground-plane fallback that every other piece gets is
  skipped, so aiming at nothing gives you nothing to place. That is the whole of "only attached to
  a wall or floor" - ceilings and ramp faces count too, which seemed better than banning them.
- **An overhang is trimmed off, not refused** (2026-10-19). Place spikes over the end of a ledge
  and you get spikes that stop at the edge. `TrimToSupport` probes the footprint on the 50 cm grid,
  starts from the cell you were actually pointing at and grows a rectangle outwards for as long as
  whole rows and columns are backed. The plate comes out at whatever that rectangle is, and the
  HUD says "trimmed to fit".
  - The grown rectangle is the first one found by a **fixed expansion order**, not the largest
    possible. That is deliberate: the client works the trim out and the server re-checks it, so
    the two have to agree, and determinism matters more here than squeezing out the last cell.
  - **The server does not trust the trim**, it verifies it. The size can only shrink - it is
    clamped to the catalog - and `IsFullySupported` then checks all four corners of whatever
    arrived. Lie about the size and the support test throws it out.
  - Corners are tested separately rather than as one surface, so a plate bridging two floors that
    meet under it is supported.
  - The placement only fails outright if the spot you aimed at is itself unbacked, which cannot
    normally happen - you got the aim point by hitting something.
- Flush against the face, standing off by half its thickness, still snapped to the 50 cm grid along
  the surface.
- **`WorldSizeFor` now takes the rotation, not a quarter-turn count**, and `ServerPlacePiece` sends
  an `FRotator` instead of a `uint8`. The server cannot re-derive a surface-mounted rotation from a
  number of turns, because it does not know which face the client was looking at. It still snaps a
  free-standing piece back to a quarter turn rather than trusting what arrived; a surface-mounted
  one is taken as sent, the same trust the aim point already has.
- Rough edge, **fixed 2026-10-26**: the world extent was `Rotation.RotateVector(Size).GetAbs()`,
  exact for the six axis-aligned normals and an approximation on a sloped face, so the ghost and
  the blocked test were both slightly off on the side of a ramp. The ghost and the blocked test are
  oriented boxes now and do not use that extent at all, and `WorldSizeFor` computes true bounds.
  See *Things lie down on slopes*.

### Dying

**Death ends your run for that round. It does not respawn you** - you watch the rest from the fly
camera and score nothing, which is what Ultimate Chicken Horse does and what the round-over logic
already expected. `EGetawayRunState::Dead` is a new state that behaves exactly like `Forfeit`
everywhere; it exists so the scoreboard can say "dead" rather than "out".

- **The victim's own machine decides, immediately.** `AGetawaySpikes` only acts on a locally
  controlled character, calls `AGetawayCharacter::Die`, and that stops the movement there and then
  before telling the server. Dying is the harshest thing that can happen to a run, so it answers to
  the player's own eyes rather than arriving a round trip after they thought they were clear - the
  same bargain as `AGetawayBreakable::Break`.
- **The server decides what it means.** `AGetawayGameMode::PlayerDied` sets the state, credits the
  placer through `RegisterTrapKill` (which still refuses to pay you for your own trap catching
  you), and checks whether that was the last runner.
- **The swap to the fly camera is deferred by `DeathLingerTime` (1.2 s)**, partly so you get a beat
  looking at what got you, and partly because `Die()` can be called from inside the victim's own
  overlap callback and `StartPlacementFor` destroys that pawn.

**Settled 2026-10-19: a kill is worth no points.** `TrapKillPoints` was 2, left over from the old
table, which would have made one kill worth more than winning the round. Rather than retune it the
award was removed outright - see *Scoring*. Killing somebody still pays indirectly, and heavily:
it stops them finishing, which is what turns a round from nobody-scores into two points for you.

Spikes themselves were play-tested on 2026-10-19 and work - killing, dying and the round carrying
on without you. Everything added after that play-test is **not played**: the attachment to a host,
the overhang trim, and the rebuilt point layout that goes with it.

### Breakables are solid until they break (2026-10-19)

Asked for: *"make it so that the destructible objects work like normal objects (can wallrun and
place spikes on them) until they get broken."*

Two lines in `UGetawayMovementComponent` were refusing them outright, and both are gone:

- `TraceWall` had `if (Cast<AGetawayBreakable>(H->GetActor())) return false;`, so a breakable panel
  could never carry a wallrun.
- The mantle's wall probe had the same test, so a crate could never be climbed.

**"Until they get broken" needed no code of its own.** `Break()` sets the mesh to `NoCollision`,
which takes it out of both traces for free; `Restore()` puts it back. The whole feature was those
two deletions.

**Placing on them already worked** and was never changed. An intact breakable and an `AGetawayPiece`
have the identical `BlockAllDynamic` collision profile, and placement only ever sees collision - it
has no idea what class it is aiming at. Spikes on a breakable panel were already supported, trimmed
to the panel (a 300-wide panel gives a 300-wide bed), already attached to it, and already set up to
vanish when it is smashed, which is the `Host` machinery from earlier the same day.

#### A breakable you are wallrunning gives way when you push off it

`SmashAhead` is called every frame from `UpdateCharacterStateBeforeMovement`, whatever the movement
mode, and sweeps the whole capsule along your velocity. Wallrunning was not excluded, so running
along a breakable panel above `SmashSpeed` 900 smashed it out from under you mid-run. **Changed the
same day so it breaks on the kick instead:**

- `UGetawayMovementComponent::WallActor` remembers what the current run is on, set when the run
  starts and refreshed each frame as the wall is re-probed.
- `SmashAhead` skips that one actor while `IsWallRunning()`. Every other breakable in the sweep
  still goes, so wallrunning past a second panel still smashes *that*.
- `WallKick` breaks it on the way out, with the scraps pushed into the wall and lifted, using the
  same `SmashSpeed` test `SmashAhead` would have applied - so a gentle wallrun leaves the panel
  standing and only a fast one takes it with you.

A wall that disappears mid-run drops you with no warning; one that explodes as you push off reads
as a consequence of the push. Only the **kick** breaks it: a run that simply times out, slows below
`WallRunMinSpeed` or reaches the end of the panel leaves it intact, which is what was asked for.

Known gap: once the run has ended that way, the panel is no longer spared, so skimming past it at
over 900 in the next moment can still smash it. Not worth special-casing.

Built clean. **Not played.**

### Two small ones (2026-10-19)

- **Breakables look different now.** A panel that is going to shatter should not be identical to a
  wall that will not, and a breakable dealt from the party box was wearing exactly the same plain
  grey `BasicShapeMaterial` as every other piece. It now takes the engine's
  `WorldGridMaterial` - placeholder art, like the debug-line ghost, but unmistakable at a glance.
  Set in the **constructor, not `OnConstruction`**, and that is load-bearing: a material assigned
  to a breakable in a level overrides the class default, so the test course keeps its orange
  colour-coded panels and only pieces that were never given one pick the grid up. Swap it for a
  real texture when there is art.
- **A C4 pickup grants one charge, not two.** (The HUD then said `C4  1 / 2` and was read as
  claiming two; that is fixed under *Falling off kills you* below.) `MaxCharges` is still 2, so carrying a full load now
  means finding two pickups rather than one, and the rocket jump costs you the charge you went and
  got. One charge is still a rocket jump, so the test course's 500 cm ledge is unaffected.

Built clean. **Not played.**

## Falling off kills you (2026-10-23)

Two things from the same play-test.

### The C4 readout said 2 when you held 1

Reported as *"one charge c4 works but says that you have 2 in your inventory"*. Nothing was wrong
with the count - the grant, the clamp and `ChargesHeld` were all correct, and one charge is what you
could throw. The HUD drew it as `C4  1 / 2`, held over the carry cap, and the cap is what got read.

It now draws **only what you are holding** - `C4  none`, `C4  x1`, `C4  x2  full` - so there is no
second number to misread, and the cap turns up as the one word that is worth knowing at a glance.
Dim at zero, amber once you have something. The pickup's own toast said `(1 / 2)` for the same
reason and now says `(carrying 1)`.

### Falling below the course ends your run

Asked for straight out: *"if you fall below the course you die."* It had never been handled at all -
the engine's own answer to a character under `KillZ` is `Destroy()`, which would have left the
controller holding no pawn, no death and no score, just a stuck camera. A fall was the last hole in
the round loop.

- **`AGetawayCharacter::FallDeathZ` (-100) is the plane**, measured at your **feet** in world Z, so
  it is one metre under a course whose floor is at zero. It is checked every frame in `Tick`, on the
  machine that owns the character only - the same bargain as spikes and breaking, where the victim's
  own eyes decide and the server decides what it means.
- **It goes through `Die(nullptr)`**, so a fall is exactly a spike: your run ends where you fell, you
  watch the rest from the fly camera after `DeathLingerTime`, and you score nothing. A null killer is
  already what `RegisterTrapKill` treats as nobody to credit, so **nobody is paid for your fall** -
  not even the player whose piece you were trying to clear.
- **Only while you are running.** `IsRunning()` is checked first, so a player still standing around
  during the finish banner is not told they have been killed.
- **`FellOutOfWorld` is overridden as well**, on both pawns. On the character it is the backstop: if
  something gets you past `FallDeathZ` it kills you properly instead of deleting you. On
  `AGetawayPlacementPawn` it does **nothing at all** - the fly camera has no collision and flying
  under the level to look up at what you are building is a normal thing to do, so the engine must not
  destroy the camera out from under you at `KillZ`.

**This changes the test course.** The catch floor at -300 and the 7-step staircases out of each pit
were there so a miss cost you a few seconds rather than the run, back when nothing could kill you.
At -100 you die on the way down and never touch them. **Asked for and confirmed 2026-10-23**, so
the pits are meant to be fatal now - but it is still one number to undo: set `FallDeathZ` below
-300 (or to a large negative) on the character and the forgiving course comes back. Nothing was deleted from the map.

Built clean. **Not played.**

## Everything has to touch something (2026-10-23)

Asked for: *"make it so you can only place objects if its touching another object."* A level is now
one connected structure you extend, not a scatter of blocks hanging in mid-air.

`AGetawayPlacementPawn::IsTouchingSomething` is the whole rule, and it is the mirror image of the
blocked test that was already there. `IsBlockedAt` takes the piece's world box, **shrinks** it by
`FitTolerance` (3 cm) and refuses an overlap; this takes the same box, **grows** it by
`ContactReach` (6 cm) and requires one. A piece sits flush against the face it was aimed at, so
whatever it is resting on is inside that skin; a piece in clear air finds nothing. The reach has to
stay above `FitTolerance` or the skin would sit inside the gap the other test leaves.

- **Any face counts, not just the bottom.** Butting a floor onto the side of a wall, or hanging a
  piece off a ceiling, is contact. Nothing has to be *under* it. This is a connectivity rule, not a
  gravity one - it is a game about building silly routes, and a floor jutting out of a wall is one
  of the good ones.
- **It is checked on the server too**, in `ServerPlacePiece`, not only on the client. The aim point
  is taken on trust, so this is the check that actually stops a piece being asked for in open sky;
  the client runs the same test only so the ghost agrees with what the server will do.
- **Surface-mounted pieces are exempt** - spikes, today. `IsFullySupported` already demands the
  whole footprint be backed, which is strictly stronger than one corner of contact, so running both
  would be the same question asked twice.
- **The ghost reads it.** Red whenever the spot is refused rather than only when it is blocked, and
  the HUD line under the crosshair says which it is: "Blocked - something is in the way" against
  "Nothing to build off - it has to touch something".
- The ground-plane fallback for aiming at nothing is still there and is now mostly moot: where
  there is real floor the trace hits it anyway, and where there is not, the ghost comes out red.
  Left in because a red ghost in the right place explains itself better than no ghost at all.

Worth knowing if it ever feels too strict: `ContactReach` is on the placement pawn in the Details
panel, and raising it lets you place with a small gap. Setting it to 0 does not turn the rule off -
clamp is 0.1 - so to go back to free placement, delete the two `IsTouchingSomething` calls.

Built clean. **Not played.**

## Things lie down on slopes (2026-10-26)

Asked for: *"can we make it so the ramp can have things placed on it slanted?"* Aim at a sloped
face and the piece tips onto it instead of sitting square and burying one corner. A floor becomes a
slanted platform, a wall leans, and a ramp carries on up the slope of the ramp under it.

- **Only sloped faces.** `SlantTolerance` (5°) is the gap: a face within that of a world axis - so
  every flat floor, wall and ceiling in the game - keeps exactly the old grid-pinned behaviour, and
  a ramp face at 18.4° tips the piece. Nothing that worked before is touched, which is the point of
  having a threshold rather than tilting onto everything.
- **Yaw first, then tip.** The piece is yawed the way you are looking as usual, and that whole frame
  is then rotated by the shortest turn from world up to the face normal. So **R** still means what
  it meant, and a ramp placed on a ramp points the way you are facing rather than taking some
  arbitrary spin about the normal.
- **Snapping solves for the slope instead of fighting it.** The centre is still snapped to the 50 cm
  grid on the two axes across the face, but the third is then *solved* so the centre sits exactly
  half a thickness off the face's own plane. Snapping all three would bury the piece or float it by
  whatever the slope rises across half a grid square - 8 cm on a ramp, which is more than
  `FitTolerance`, so half of all placements would have come back blocked.
- **Stacking compounds, on purpose.** A piece lying on a ramp presents its own sloped faces, so the
  next piece lies on *that*. Slanted staircases fall out of this for free.

### Oriented boxes, which this needed first

The blocked test, the contact test and the ghost all used `FGetawayPlacement::Size` - the piece's
**world-space** extent - with an identity rotation. That is exact while every piece is axis-aligned
and wrong the moment one is not: a tilted piece would have been tested and drawn as the fat upright
box that contains it. All three now take the piece's own `LocalSize` turned by its own rotation,
which is the same region as before for every axis-aligned piece and the right one for a tilted
piece. `DrawDebugBox` and `OverlapBlockingTestByChannel` both take a quaternion, so this cost
nothing but the signatures.

`WorldSizeFor` survives as the true axis-aligned bounds - each world axis summing a contribution
from all three of the piece's own axes, rather than the old rotate-the-extent-and-take-the-absolute
value, which is only correct for quarter turns. It feeds the free-standing grid pinning, where the
rotation is always a quarter turn, so that path is unchanged to the bit. This also closes the rough
edge logged under *Surface mounting* about spikes on the side of a ramp.

### What the server does with a tilted rotation

`ServerPlacePiece` used to snap every free-standing piece's rotation back to a quarter turn about Z
rather than trust what arrived. It still does **when the piece arrives upright**. A tilted one is
taken as sent, for the same reason a surface-mounted one always was: the server does not know which
face the client was looking at, so it cannot re-derive the rotation to check it against.

That is a real guard being narrowed, so, plainly: what it was protecting against is cosmetic. A
rotation on its own cannot put a piece anywhere - the **aim point** decides that, and the aim point
has been taken on trust since the spike. Everything that matters still holds on the server: the
piece must come out of the hand that player was dealt, it must not overlap anything, and it must
touch something.

Built clean. **Not played.**

## A bridge stays down, and C4 gets swept up (2026-10-26)

Two things asked for together: *"the placed c4 should clear at the end of each round and falling
bridges shouldnt reset mid round."* Both are about the same thing - what a round leaves behind.

### A collapsing bridge stays collapsed

`bRespawns` was on with a `RespawnDelay` of 6 s, so a bridge righted itself mid-run. That makes it
a four-second inconvenience rather than an obstacle: the player who dropped it could watch it fall
and then walk over it on the way past. It is **off by default now**, and the bridge stays down
until the next round - `StartPlacement` already called `Restore()` on every bridge, so the round
was always the right owner of that reset and nothing new was needed to bring it back.

The knob stays, for a bridge standing in a level with no round behind it.

**What this did need is a bridge that survives falling.** The old code read:

```
else if (CleanupDrop > 0.f && ...) { Destroy(); }
```

- that branch only ran for a non-respawning bridge, which until now was the unusual case. Make it
every bridge and the deck deletes itself on the way down, and the next round has nothing to put
back. It now **parks** instead: physics off, collision off, hidden, actor kept, and `StopFalling`
un-parks it when the round rights it. `FellOutOfWorld` is overridden to park as well, because a
deck that stays down for a whole round has plenty of time to reach `KillZ`, where the engine's
default is also `Destroy()`.

Parking is decided **per machine, above the authority check**, next to the rattle rather than in
the server-only state machine. The deck is deliberately not movement-replicated, so every copy
falls on its own and each one has to notice its own copy has gone far enough.

### Thrown C4 is cleared between rounds

`StartPlacement` now destroys every `AGetawayC4` in the level, alongside restoring the breakables
and the bridges. A charge is spent the moment it leaves your hand, so a brick still stuck to a wall
from last round is not a resource anyone can spend - it is a live bomb with no owner, and its blast
would land in a round nobody threw it in.

- **The charges you are carrying are untouched.** You went and collected those; they are yours
  across rounds exactly as before. This only sweeps up what is already stuck to the level.
- **`AGetawayC4Pickup` is a different class and is not swept** - a pickup is a placed piece, and
  pieces anyone placed stay.
- Server only, like everything else in `StartPlacement`, and `Destroy()` on a replicated actor
  reaches the clients by itself.

Built clean. **Not played.**
