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

Eleven pieces. Eight are sized from the lab's `PTYPES` table (lab metres x 100, Y-up swapped to
Z-up): floor 400x400x50, wall 400x50x400, ledge 200x200x130, breakable panel 300x50x400, crate
100x100x100, ramp 600x400x200, collapsing bridge 1200x400x60, and the C4 pickup. The ninth is
**spikes** (400x400x50), added 2026-10-19, which has no lab equivalent and was the first
surface-mounted piece - see *Spikes, and death*. The tenth is the **jump pad** (200x200x50),
added 2026-10-27, surface mounted the same way - see *The jump pad* below. The eleventh is the
**vortex** (300x300x500), added 2026-10-31, a free-standing column - see *The anti-gravity vortex*
below. The party box deals `HandSize` (3) of them at random without repeats.

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

### The jump pad (added 2026-10-27)

`AGetawayJumpPad`, catalog id `jumppad`, 200 x 200 x 50 - **the second surface-mounted piece, and
the first one that is a route rather than a hazard.** Asked for as *"a jump pad that can be placed
on anything (even horizontally) that launches you in the direction it points"*, and
`bSurfaceMounted` turned out to be the whole of it: the piece's own +Z is turned out of the face
you aim at, and **that axis is the launch direction**. So one catalog entry is a trampoline on a
floor, a horizontal shove off a wall, a slam off a ceiling and a kicker off the side of a ramp.
There is nothing else to aim - the face you stick it to *is* the direction, and R only spins the
plate about the normal, which on a square plate you cannot see. That is also why it needs no
placement code of its own: the flat mount, the no-mid-air rule, the 50 cm grid along the surface,
the overhang trim and the server's re-check all came free from the spikes.

- **It never blocks you,** the pattern every trigger piece in the game follows: the meshes have
  collision off and the trigger is QueryOnly overlapping pawns only. You run, slide or wallrun
  through the plate and it throws you, rather than stopping you first.
- **The launch sets your speed out of the pad and leaves your speed across it alone.** A pad is
  therefore the same height for everybody who touches it - which is what makes it a route you can
  plan a level around - while a fast player still carries their whole run-up through it. It is a
  `FMath::Max`, not an assignment, so arriving faster than the pad fires is never a reason to be
  slowed down.
- **A pad firing along the ground you stand on is tilted upward first** (`GroundLift`, 0.35,
  added 2026-10-27 after the first pass). A wall pad fires horizontally, and a horizontal shove
  handed to somebody standing on the floor is eaten by ground friction before it takes them
  anywhere, so it would read as a shrug rather than a launch. This is the same trick, and the same
  0.35, that `AGetawayC4` uses for a blast at your feet. It applies **only while you are on the
  ground** - hit a wall pad in the air or off a wallrun and it fires exactly where it points, since
  there is no friction there to beat - and never to a pad firing steeply downward, because a pad
  under a ceiling means to slam you into the floor.
- **It reuses `UGetawayMovementComponent::ApplyBlast`** (written for the C4 rocket jump) by handing
  it the change in velocity it wants, so dropping out of a wallrun or a mantle, and coming off the
  ground when the kick points away from it, is already handled.
- **Your own machine throws you,** on the frame you touched the pad - the same client-authority
  bargain as dying on spikes and the rocket jump. Nothing about a launch goes over the wire.
- **Armed only during a run,** like the spikes: `StartPlacement` disarms every pad, `BeginRun` arms
  them, and arming clears the per-player retrigger times so a new round starts clean.
- **It belongs to what it is stuck to,** again like the spikes: attached with `KeepWorldTransform`
  (so a pad on a collapsing bridge rides it down) and bound to a breakable host's `OnBroken` /
  `OnRestored`, so **smashing the panel under a pad takes the pad with it**. That makes the
  breakable the answer to a route built on a pad. The host is worked out by the server tracing back
  out of the plate, not sent by the client.
- **The disc is a disc on purpose.** A plate with points on it is the thing that kills you, and
  these two pieces are otherwise the same shape; a single cylinder sticking out of the plate reads
  as a piston and shows the firing direction at a glance. It keeps its world size however the plate
  is scaled or trimmed, the same trick the spike points use.
- **No ghost arrow**, unlike the directional pieces. The direction is decided entirely by the face
  you aim at, and the ghost box already lies on that face, so an arrow would add nothing. `jumppad`
  is therefore surface-mounted but *not* `bDirectional`.

The tuning is on the actor, under **Getaway|Jump Pad**, so it can be changed per pad in the Details
panel or on a Blueprint child:

| Value | Default | What it does |
|---|---|---|
| `LaunchSpeed` | 1400 cm/s | The launch, along the pad's +Z. About 4.9 m of height off a floor pad against the character's 1.4 m jump, so just over the test course's 500 cm rocket ledge |
| `bAddToIncoming` | off | Off: the pad decides, so every launch is the same. On: added to what you arrive with, so pads stack and jumping into one goes much higher |
| `SideSpeedKept` | 1.0 | How much of your speed *across* the pad survives. 1 keeps the whole arc; lower values straighten your path towards the pad's facing |
| `GroundLift` | 0.35 | The least upward tilt a launch gets while you are standing on the ground, so a wall pad throws you off the floor instead of along it. 0 turns it off; it never applies in the air or to a pad firing steeply down |
| `Retrigger` | 0.25 s | Per player, so one pass through a pad is one launch |
| `LaunchViewPunch` / `LaunchShake` | 1.2 deg / 0.35 | The camera kick, matching the C4 blast's |
| `PistonWidth` / `PistonHeight` | 120 / 30 cm | The disc, held at a constant world size and kept inside a trimmed plate |

**Known duplication:** the host-binding half of this (`Host`, `BindToHost`, `OnHostBroken`,
`SetPresent` and the replication that goes with it) is copied from `AGetawaySpikes`, about forty
lines. Pulling both onto a shared surface-mounted base class is the obvious cleanup, and was
deliberately left out of this pass rather than reparenting a piece that has already been played.

**Not played yet.** Built clean. What is unproven is all feel: whether 1400 is the right launch,
whether 0.35 is the right ground lift, whether a pad should straighten your path (`SideSpeedKept`
below 1), whether a 200 cm plate is a fair target at speed, and whether a wall pad reads as a
kicker or as a surprise.

### The anti-gravity vortex (added 2026-10-31)

`AGetawayVortex`, catalog id `vortex`, 300 x 300 x 500 - **the first piece whose effect is
sustained rather than instant.** Asked for as *"an anti-gravity vortex that lifts you up quickly but
you can move at any time through it"*. A jump pad is one kick on the frame you touch it and gravity
owns you afterwards; the vortex cancels gravity and holds you climbing at `LiftSpeed` for as long as
you are in the column, so **the height you get is decided by how long you stay in it** rather than
by how hard it hit you. That is the whole point of the piece: it is a route you can leave early.

- **It lifts straight up, always** - never along the piece's own axis. It is not surface mounted, so
  it stands on the floor (or against whatever you aimed at), takes the 50 cm grid and the
  *everything touches something* rule unchanged, and R turns a square column invisibly. Placement
  *does* lie a piece down on a sloped face, so the visible cage is held upright in world space to
  say that the lift is not tilted with it; only the invisible trigger box leans.
- **You keep full control the whole way up.** Two parts to that, and the second one is the piece.
  Source air strafing still works, so a fast run-up carries its whole arc through the column. On top
  of it the vortex gives you *direct* horizontal steering (`SteerSpeed`, `SteerAccel`), because plain
  Source air acceleration can only add `AirSpeedCap` (75 cm/s) to a direction you already have speed
  in - without the extra steering, walking into a column would ride you straight up it with no say
  in where you came out. It goes through `SourceAccelerate`, which only ever adds speed *toward* the
  keys you are holding, so the vortex can never slow anybody down.
- **It never blocks you,** the pattern every trigger piece in the game follows: the meshes have
  collision off and the trigger is QueryOnly overlapping pawns only, like the spikes, the jump pad
  and the finish.
- **The lift is offered, not applied.** `UGetawayMovementComponent::AddVortexLift` stores it and
  `ApplyVortexLift` takes it at the top of the component's own update, where the jump and the double
  jump already write vertical speed - the right place, because `CalcVelocity` is handed the vertical
  velocity stripped out while falling, so a lift written there would be thrown away. A vortex
  re-offers every tick it holds you, and the offer is consumed whether or not one came, so **the
  lift expires by itself one frame after you leave** and nothing has to tell the character it is
  out. Two overlapping columns resolve to the stronger `LiftSpeed` rather than to whichever ticked
  last.
- **It pays gravity back.** Gravity is applied inside the move that follows the update, so the lift
  adds `LiftAccel` *and* the `gravity * frame time` the move is about to take off again. Without
  that the hold would sag by a frame's worth of gravity and the climb would be slower on a slower
  machine.
- **It wins against a wall and a ledge.** It drops you out of a wallrun or a mantle the way
  `ApplyBlast` does, and `bBlocksWallrun` (on) stops you grabbing a wall while the column has you -
  otherwise a vortex placed against a wall hands you a wallrun whose own gravity fights the lift and
  you hang there. Turn it off and the wallrun wins instead.
- **Your own machine lifts you,** on the frames you are inside - the same client-authority bargain
  as the jump pad, dying on spikes and the rocket jump. Nothing about the lift goes over the wire;
  the actor replicates so everybody sees the column, and `SetReplicateMovement(false)` because it
  never moves.
- **Armed only during a run,** like the spikes and the pad: `StartPlacement` disarms every vortex,
  `BeginRun` arms them, and arming clears who was riding so the column catches you - camera kick and
  all - on the first frame of the new round.
- **The cage is a cage because you have to see through it.** The plugin ships no content and there is
  no translucent material, so the column is six thin vertical bars round its edge plus a thin disc at
  each end, all collisionless, built from engine BasicShapes. The bars **spin while the piece is
  armed**, which is the only thing that says it is live; they stop between rounds. They hang off a
  `Column` scene component rather than the actor, because turning the actor would turn the trigger
  box and change the footprint the column catches you in. Bar and disc thickness are held at a
  constant world size however the piece is scaled, the same trick as the jump pad's disc.
- **`GetStateName()` reports `VORTEX`,** so the HUD's state readout shows when the column has you.

The tuning is on the actor, under **Getaway|Vortex**, so it can be changed per column in the Details
panel or on a Blueprint child:

| Value | Default | What it does |
|---|---|---|
| `LiftSpeed` | 1000 cm/s | The climb it holds you at. Crosses the 500 cm column in about half a second and leaves you with 1000 cm/s, which coasts a further 250 cm - so around 7.5 m in all if you ride the whole thing, against the 140 cm of a plain jump |
| `LiftAccel` | 6000 cm/s² | How fast you reach it. It has to beat gravity (about 2000 here) by a long way or the catch feels soft; 6000 gets there in about 0.17 s, the first 85 cm of the column |
| `SteerSpeed` | 400 cm/s | How fast you can move sideways inside the column under your own steam. A target, not a limit: run in faster and you keep it |
| `SteerAccel` | 8 | How sharply that steering answers the keys. A Source coefficient, like `GroundAccelerate`'s 10 |
| `bBlocksWallrun` | on | You cannot grab a wall while the column has you. Off lets a wallrun override the lift |
| `LiftViewPunch` / `LiftShake` | 0.8 deg / 0.2 | The camera kick as it catches you, lighter than the pad's |
| `BarCount` / `BarThickness` | 8 / 12 cm | The cage. Eight because the 300 cm ring leaves six of them over a metre apart. Up to 12 bars; they are pooled in the constructor and the spares hidden, as the spikes pool their points |
| `DiscThickness` | 10 cm | The discs at each end |
| `SpinRate` | 220 deg/s | How fast the cage turns while armed. 0 stops it |

It was built at 100 x 100 x 200 and resized to 300 x 300 x 500 the same day, before it was ever
played - a 1 m mouth was too small to hit at a run. The size is one line in the catalog and
everything in the piece is driven off the actor scale, so nothing else had to change; `LiftAccel`
came down from 9000 to 6000 and the cage went from six bars to eight, both because the piece is no
longer short and narrow.

**Not played yet.** Built clean, no warnings. What is unproven is all feel: whether 1000 is the
right climb for a 5 m column, whether 400 of steering is too loose or too stiff, whether a 3 m
footprint is now too easy a target, whether shooting out of the top at full climb is better than
cresting, and whether the spinning cage reads as a vortex at all.

**Known quirks, left alone deliberately:**

- **A double jump inside the column is wasted.** You are falling, so the air jump is available, but
  it sets vertical speed to `JumpSpeed * 0.92` (690), which is below the lift's hold, so the lift
  simply tops it back up and nothing visible happens. Refilling or refusing the air jump inside a
  vortex are both one line, but which one is right is a feel question.
- **A vortex on a slope has an upright cage and a leaning trigger box,** so a bar can poke a few
  centimetres outside the volume that actually lifts you. Cosmetic, and only on slopes.
- **Nothing but pawns is affected.** C4 charges, loose scraps and a collapsing bridge fall through a
  column as if it were not there.

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

### Fixed: a thin ledge could not be mantled (2026-11-01)

**This is the one that mattered.** Every breakable in `/Game/Maps/TestCourse` is 25 cm thick (read
out of the map; see *What is actually in the map* in `docs/test-course.md`), and the mantle put its
landing probe `R + 10` = 44 cm past the wall face. That is 19 cm *past* a 25 cm panel, so the probe
came down in open air behind it, found the floor 200 cm below or nothing at all, and the climb was
refused. None of the four panels could be mantled from either side, at any speed, holding anything -
and nor could any other thin wall, breakable or not. The report was "you can't mantle the breakable
objects in the original world", and it was not about breakables at all.

`FindMantle` now tries the deepest landing first and walks in towards the face - `R + 10`, `0.7 R`,
`0.35 R`, 6 cm - taking the first that holds the capsule up at a height inside the mantle window. A
thick ledge is found on the first try and behaves exactly as it did, so the ramp fix above is
untouched. A thin one lands you on top of it, and since a mantle keeps 75% of your run-up speed
pointed over the top, a 25 cm panel reads as a vault rather than a perch.

The height window moved inside that loop rather than being applied to whatever the single probe hit.
It has to be: a probe that overshoots a thin ledge usually *does* hit something, namely the floor far
below, and the old code had already committed to that reading.

### Fixed: a breakable could not be mantled at speed (2026-11-01)

A crate is 100 cm tall, well inside the mantle window, and nothing in the mantle code ever skipped
breakables - but jumping at one above `SmashSpeed` always burst it instead of climbing it. (Found by
reading, not by playing: no crate exists in the test course, so this one had never been hit.) The two
tests race each other and the break sweep is always in front: `SmashAhead` sweeps the capsule along
`Velocity * (DeltaTime + 0.05)`, so at 900 cm/s it reaches about 94 cm past the capsule axis and
further the faster you go, while the mantle wall probe is a fixed `R + 30` = 64 cm. Every crate in
that 30 cm gap was already scrap by the time the mantle probe could see it, which at bunny-hop
speeds is every crate.

`SmashAhead` now asks the mantle question first, from as far out as its own sweep reaches, and
spares the breakable it would climb. The geometry moved out of `TryStartMantle` into
`FindMantle(ExtraReach, Out)`, which changes nothing and decides nothing - `TryStartMantle` passes 0
and then commits to the answer, so how close you have to be to climb is exactly what it was. The
prediction is re-made every frame, so a crate that turns out not to be climbable after all (a
ceiling you only see from close) breaks on the frame the mantle test starts failing.

What still breaks a breakable: hitting it without holding forward into it, sliding into it,
anything whose top is outside `MantleMinHeight..MantleMaxHeight` - the 400 cm panel always, a stack
of two crates at 200 - and C4. `bMantleOverSmash` on the movement component turns the whole thing
off for the old behaviour.

### Fixed: a surface-mounted piece the ghost said yes to (2026-11-01)

Four bugs, all of them only on spikes and the jump pad - the pieces that go through
`TrimToSupport` - and all but the overhang silent:

- **The click re-asked the question.** `OnPlace` ran `ComputePlacement` again instead of using the
  `Planned` the ghost was drawn from. A click is handled a hair after that tick, by which time the
  mouse has moved a pixel or two, and the trim grows the plate out of **the grid cell you are
  pointing at**: near the edge of a wall or a ledge one cell is backed and its neighbour is not, so
  the answer flipped between the frame you saw and the frame you clicked. The cyan box did nothing
  and nothing reached the log, because the request was never sent. It now places the ghost you
  clicked on. The hand slot is checked too, so scrolling the hand in the same frame as the click
  cannot send one piece's box with another piece's slot.
- **The trim kept cells it had only half tested, so the plate overhung the edge.** It probed one
  point per cell, at the centre, and kept any cell whose centre was backed - including a cell whose
  outer half hangs over the edge of the ledge. That is the reported "it seems to have overhang which
  it shouldn't": up to half a cell, 25 cm with the default grid, of spikes or of jump pad sticking
  out into the air, launching off a face that is not there. A cell is now kept only if all four of
  its **corners** are backed. Corners are shared between neighbouring cells, so it is one trace per
  lattice point, not four per cell, and the whole lattice is pulled in 2 cm (`SupportEdgeBias`) so a
  plate flush with the edge still counts - 2 cm is now the most a piece can overhang.
- **The server asked a different question again.** `IsFullySupported` probed the four corners of the
  trimmed plate pulled in 12 cm, which agreed with neither version of the trim. It is now
  `IsBackedAcross`, the same corner lattice over the trimmed plate, off the shared `SupportCells`
  layout, so a cyan ghost is a piece the server accepts.
- **Aiming at the lip no longer refuses the placement.** The trim grows out of the cell under the
  crosshair and gave up if that one cell was not backed, which with corner probing is any aim within
  half a cell of an edge - exactly where you aim when you want spikes *at* the edge. It now slides
  to the nearest fully backed cell, scanned in a fixed order so the server reaches the same answer.
  That also kills the blink: the ghost used to vanish as the aim point crossed a cell boundary.

Not verified by playing yet.

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
