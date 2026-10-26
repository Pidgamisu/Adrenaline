# Placement phase and pieces

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

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
