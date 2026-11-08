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
wall always reads the same way round relative to your view - **running away from you**, not across
it as the lab's did; settled by playing on 2026-11-02, see the end of this file. Aiming at nothing
falls back to the ground plane. A piece that
would overlap existing geometry turns red and cannot be placed - and since 2026-10-23 so does one
that would touch nothing at all; see *Everything has to touch something* at the end. Aim at a
**sloped** face and what happens depends on the piece: a surface-mounted one (spikes, a jump pad)
lies down on it, and a free-standing one stands square on it, resting on the slope. See *Things lie
down on slopes* and *Only attached things follow a slope* in `docs/falling-slopes-cleanup.md`.

### The catalog

Fourteen pieces. Eight are sized from the lab's `PTYPES` table (lab metres x 100, Y-up swapped to
Z-up), six of them retuned by hand since, the last three on 2026-11-05: floor 400x400x50, wall 400x50x400, ledge 300x300x150,
breakable panel 400x50x400, crate 250x250x250, ramp 600x400x200, collapsing bridge 1000x400x60, and
the C4 pickup. The ninth is
**spikes** (400x400x50), added 2026-10-19, which has no lab equivalent and was the first
surface-mounted piece - see *Spikes, and death*. The tenth is the **jump pad** (200x200x50),
added 2026-10-27, surface mounted the same way - see *The jump pad* below. The eleventh is the
**vortex** (200x200x400), added 2026-10-31, a free-standing column - see *The anti-gravity vortex*
below. The twelfth is the **windmill** (350x700x540), added 2026-11-03 - see *The paddle wheel*.
The thirteenth is the **speed ring** (100x300x300), added 2026-11-05, a hoop you run through for
three seconds of extra speed - see *The speed ring* at the end of this file. The fourteenth is the
**grapple pickup** (110x110x110), added 2026-11-05: the same pad as the C4 pickup with a hook
turning above it, and the second thing a player can carry - see *The grapple pickup* at the end of
this file. Fourteen in all; the party box deals `HandSize` of them without repeats - **2** in
`BP_GetawayGameMode`, where the C++ says 3 - and since 2026-11-06 it deals **one normal and one
special**, not two at random.

Edit the catalog on a Blueprint child of `GetawayGameMode` - it is `EditDefaultsOnly`.

### One normal and one special (added 2026-11-06)

**Played the day it was built and signed off at the first split** - *"works great"*. No piece was
moved across after playing, which is worth noting given the four interesting calls below were all
reasoned about rather than tested.

A hand is one piece you build the course **out of** and one piece that changes how it is **moved
through**. `FGetawayPieceDef::bSpecial` says which a piece is, and the split came from going through
the catalog a row at a time:

| Normal - structure you place and stand on | Special - pickups, hazards, speed devices |
|---|---|
| Floor, wall, ledge | C4 pickup |
| Ramp | Grapple pickup |
| Breakable panel | Spikes |
| Crate | Jump pad |
| Windmill | Vortex |
| Collapsing bridge | Speed ring |

Eight normal, six special. The four interesting calls are all on the normal side:

- **The breakable panel and the crate.** Since *Breaking is granted by the level*, `SmashSpeed`
  1100 is out of reach on foot - so a panel nobody has put a pad or a ring in front of is a wall,
  and a crate is a 250 cm block you climb round. They are structure with a conditional twist, and
  the twist needs a *special* piece placed near them to go off, which is exactly the combination
  the one-of-each hand is for.
- **The windmill** is a lift you ride. It moves, but what it does is carry you up and over, which
  is what a staircase would do.
- **The collapsing bridge** is a floor that happens to be a lie. It spans a gap like a floor and is
  placed like one; it just stops being there.

`AGetawayGameMode::DealHandFor` draws one out of each pool first, in that order - **normal in slot
1, special in slot 2, every round**, deliberately not shuffled, so 1 and 2 mean the same thing every
time you build. Any slot past the second is free, dealt from whatever is left; at `HandSize` 2 there
are none. `bOneOfEach` off on the game mode deals from the whole catalog as before.

If either pool is empty - a flag cleared, or every pickup retired from the catalog - nothing is
drawn from it and the free loop fills the slot instead, so a hand is never short.

**The flag lives in C++ and has to be pushed into the Blueprint.** `BP_GetawayGameMode` stores the
whole `Catalog` array and stopped tracking C++ long ago, so all fourteen of its rows read
`bSpecial` false until something writes them. `Content/Python/flag_special_pieces.py` copies just
that one field across, matching on Id and touching nothing else - no Actor Class, no sizes, no
materials. `dress_pieces.py` also carries it, because it rebuilds the array from C++ wholesale, but
it recompiles nine piece Blueprints to do it and is the wrong tool unless a piece is being *added*.
Until the script is run the deal is a normal piece plus a free one, which looks like it half works.

**The sizes that play are `BP_GetawayGameMode`'s, not the C++ ones.** The Blueprint stores the whole
`Catalog` array, so its copy wins outright and a size changed in C++ never reaches the game. The two
are kept equal by hand; the C++ constructor is documentation at this point.

**Changing a size in the Details panel needs Compile *and* Save.** It is stored in the `.uasset`,
and a C++ rebuild reinstances the Blueprint and silently drops anything unsaved - which is how the
panel, crate and bridge sizes were lost once on 2026-11-03 and had to be typed in again. The
retune: panel 300 -> **400** wide (a full wall rather than three quarters of one), crate 100 ->
**200** cubed, bridge 1200 -> **1000** long.

### Directional pieces (added 2026-10-15)

The ramp and the bridge care which way round they are - the lab calls these `DIRTYPES` and stores a
facing `d` on the piece. Here `FGetawayPieceDef::bDirectional` marks them, and the facing *is* the
piece's rotation: **a directional piece runs its length along its own +X and rises or falls that
way**, so `Size.X` is the length along the facing, not along world X. That is why the ramp is
600x400x200 rather than the lab's bare 400x600x200: placement lines a piece's +X up with the way
you are looking, so a ramp slopes away from you and a bridge drops away from you until you press R.

The ghost draws the arrow (and the HUD prints "facing north", the lab's `DIRNAME`) only for
directional pieces. Since 2026-11-02 a ramp's ghost is the **real wedge** rather than the box it
half fills - see *The ghost is real geometry* below. The blocked test still uses the whole box, so a
ramp is called blocked by something sitting in the empty air above its low end.

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
- **It hands you a low-friction window** (`FrictionScale` 0.3 for `FrictionTime` 3 s, added
  2026-11-06). See *The low-friction window* below: this is what makes the launch last past the
  landing instead of being eaten by friction the moment you touch down.
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
| `LaunchSpeed` | **1800 cm/s** | The launch, along the pad's +Z, and since 2026-11-07 the whole of what a pad gives you. About **8.1 m** of height off a floor pad against the character's 1.4 m jump. Was 1400 (4.9 m) while the friction window existed |
| `bAddToIncoming` | off | Off: the pad decides, so every launch is the same. On: added to what you arrive with, so pads stack and jumping into one goes much higher |
| `SideSpeedKept` | 1.0 | How much of your speed *across* the pad survives. 1 keeps the whole arc; lower values straighten your path towards the pad's facing |
| `GroundLift` | 0.35 | The least upward tilt a launch gets while you are standing on the ground, so a wall pad throws you off the floor instead of along it. 0 turns it off; it never applies in the air or to a pad firing steeply down |
| `Retrigger` | 0.25 s | Per player, so one pass through a pad is one launch |
| `FrictionScale` | **1 (off)** | How much friction would survive the window. 1 is no change at all, 0.3 is what was played, 0 is frictionless ice |
| `FrictionTime` | **0 (off)** | How long the window would last after the launch. Set these two to 0.3 and 3 to put the old behaviour back, no build needed |
| `LaunchViewPunch` / `LaunchShake` | 1.2 deg / 0.35 | The camera kick, matching the C4 blast's |
| `PistonWidth` / `PistonHeight` | 120 / 30 cm | The disc, held at a constant world size and kept inside a trimmed plate |

#### The low-friction window (added 2026-11-06, off since 2026-11-07)

> **It is off.** It was played and taken back out the next day - read *The window came out* below
> for what happened and what the code looks like now. What follows is the design as it was built,
> kept because the reasoning is still the clearest statement of the difference between this lever
> and the speed ring's, and because two properties on the pad put it back.

A pad used to throw you 1400 cm/s and then watch Source friction take all of it back on the first
touchdown. `GroundFrictionSource` always drags you toward `StopSpeed`, so the height was the only
thing a pad was really worth: you landed, you were jogging, and the launch had bought you a gap
rather than a line. Now the launch comes with `FrictionScale` 0.3 for `FrictionTime` 3 s, and you
keep what you were given.

It is deliberately **the opposite lever to the speed ring's**, and the two are worth holding apart
because they look similar and are not:

| | Speed ring | Jump pad |
|---|---|---|
| What it moves | `RunSpeed`, the ceiling | `GroundFrictionSource` and `SlideFriction`, the drag |
| What it gives you | a speed you could not otherwise **reach** | the ability to **keep** a speed you were handed |
| Why that way round | a velocity kick would be eaten in a second, so raising the ceiling is the only thing that lasts | the pad already hands you far more speed than you can run at; the problem was never reaching it |

So the ring is `AddSpeedBoost` and the pad is `AddLowFriction`, and neither touches your velocity.
Both are handed over **once** and then run on the character's own clock, because both pieces are
behind you a frame later - a window outliving the piece is the same trick, and for the same reason,
as the ring's boost outliving the hoop.

Details worth knowing:

- **It covers sliding too.** `SlideFriction` is scaled by the same number, so landing out of a pad
  into a slide is the fast line rather than a way to throw the launch away. That is the main thing
  to try when playing it.
- **It can never make you faster than you landed.** Nothing is done to `Velocity`; low friction
  only stops you losing speed. A pad cannot become a speed boost by accident.
- **Windows refresh rather than stack**, exactly like boosts: the lower scale wins and the longer
  clock wins, so two pads in a row give you three more seconds instead of compounding into ice.
  `LowFrictionScale` is put back to 1 the instant the clock runs out, so an expired window cannot
  drag a stale value into the next one.
- **There is no air friction in this game to reduce.** This was asked for as "air and ground
  friction", and the air half turned out to need no code: `FallingLateralFriction` is 0 and
  `CalcVelocity` applies no drag at all while you are airborne, so speed through the air is already
  lossless. The window has nothing to do until you touch down, which is also exactly where the
  problem was. If air speed ever *does* feel lossy, the number to look at is `AirSpeedCap`
  (Blueprint **35** against a C++ 75), which limits how hard air strafing can bite - but that is
  air control, not friction, and it cannot slow you down.

**Played 2026-11-07, and it is out.** *"Honestly it doesn't feel good, i think the change should be
just increasing the jump pad strength."* So the window goes and `LaunchSpeed` goes up instead,
which reverses *A jump pad takes the drag away rather than raising your ceiling* in `CLAUDE.md`.
**Not yet done in code** - see *The window is coming out* below.

The reasoning that lost is worth keeping, because none of it was wrong. Source friction really does
eat a whole launch on the first touchdown, and 0.3 for 3 s really does stop that. What it cost was
the thing a pad is *for*: a launch is a moment you aim at, and a friction window is three seconds
of the floor behaving differently afterwards, which you did not ask for and cannot see. The pad
stopped being a kick and became a surface condition. Compare the speed ring, which gets away with a
three-second effect because a raised ceiling is something you **spend** - on the jump or the
wallrun you pick next - where low friction is something that merely happens to you.

**Known duplication:** the host-binding half of this (`Host`, `BindToHost`, `OnHostBroken`,
`SetPresent` and the replication that goes with it) is copied from `AGetawaySpikes`, about forty
lines. Pulling both onto a shared surface-mounted base class is the obvious cleanup, and was
deliberately left out of this pass rather than reparenting a piece that has already been played.

**Played 2026-11-07.** The pad itself works and **`LaunchSpeed` is the lever** - the verdict on the
low-friction window (above) was that the pad should simply be stronger, so it was 1400 that wanted
raising rather than defending. Still not reported on, because nothing in the play-test turned on
them: whether 0.35 is the right ground lift, whether a pad should straighten your path
(`SideSpeedKept` below 1), whether a 200 cm plate is a fair target at speed, and whether a wall pad
reads as a kicker or as a surprise.

#### The window came out (2026-11-07, written the next day)

What the play-test asked for, in one line: **take the low-friction window out and raise
`LaunchSpeed`.** Done, and it is a small change - three edits and no new code.

**`LaunchSpeed` 1400 -> 1800.** Height off a floor pad is `LaunchSpeed² / 2g`, with
g = 980 x `GravityScale` 2.04 = 1999 cm/s²:

| `LaunchSpeed` | Height off a floor pad |
|---|---|
| 1400 (was) | 4.9 m |
| 1600 | 6.4 m |
| **1800 (is)** | **8.1 m** |
| 2000 | 10.0 m |

1800 was picked rather than something larger because 8.1 m already doubles the old throw and the
test course's tallest intended climb is the 500 cm rocket ledge. It is one field in the Details
panel, under **Getaway|Jump Pad** on the pad, and nothing in `BP_Piece_JumpPad` overrides it
(checked - the Blueprint's name table holds none of the pad's tuning properties), so the C++ value
is what plays and changing it there needs no build.

**Why raising the launch answers the friction complaint at all**, which is worth stating because it
is not obvious: a pad never touches your speed *across* its face, so a floor pad does not make you
faster along the ground - it buys you **air**. And air in this game is lossless
(`FallingLateralFriction` is 0, and `CalcVelocity` applies no drag while airborne), so a bigger
launch means longer spent in the one place friction cannot reach you, carrying the run-up you
arrived with. The window tried to protect the landing; height avoids needing to.

**What the code looks like now:**

- **The window is off, not deleted.** `FrictionScale` is **1** and `FrictionTime` is **0** on
  `AGetawayJumpPad`, and the call site is wrapped in `if (FrictionScale < 1.f && FrictionTime > 0.f)`
  so the off state reads at the call site rather than two files away. `AddLowFriction` already
  refused a scale of 1 or a duration of 0, so that branch changes no behaviour.
- **`UGetawayMovementComponent::AddLowFriction` and its state stay**, now with **no caller in the
  project**. That is a deliberate change of mind from the plan written here on 2026-11-07, which
  said the whole mechanism should go. Two reasons: putting the old behaviour back is then two
  numbers in the Details panel instead of a build, which is how every other reversed decision in
  this project is kept (`bSlantOnSlopes`, `bMantleOverSmash`, `SlideCooldown` 0); and the lever may
  suit some later piece better than it suited a pad - it is only *a pad* that it was wrong for,
  because a pad is a moment. Its header comment now says plainly that nothing calls it.
- **`SmashSpeed` 1100 is unaffected in kind.** A pad was already past it - a floor pad at 1400 with
  a run-up is `sqrt(800² + 1400²)` = 1612 cm/s - and at 1800 it is 1970. Wider margin, same rule,
  and per *Breaking is granted by the level* a pad being one of the few things that can burst a
  breakable is the intent.
- **Nothing else moved.** `docs/sound.md`'s jump pad sting is unaffected, and so is the ring:
  `GetBoostedRunSpeed` is a separate lever and it survived its own play-test.

**Played 2026-11-08 and signed off at 1800** - *"the pad feels much better, this was definitely the
better fix."* No retune: 8.1 m of air is the right amount, and nothing about the landing wanted the
window back. So the number that was pure arithmetic a day earlier held, and **the reversal is
settled rather than provisional.**

Worth keeping for the next piece that goes wrong, because this is now a worked example of a
specific kind of mistake: the window was not a bad mechanism, it was **a correct fix aimed at the
wrong axis**. Source friction really does eat a launch - that observation was right, the way the
void's prompting observation was right - and the instinct was to go after the friction. The fix was
to go after the *launch*, which is the thing the piece actually is. When a piece feels weak, raise
what it does at the moment you touch it before changing what the world does afterwards.

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
  Air input still works, so a fast run-up carries its whole arc through the column - and since
  2026-11-06 it steers harder than it used to, because redirect-only air control opens the gate at
  every angle rather than only near perpendicular. On top
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
  rather than a per-round deck; one normal and one special (2026-11-06) is the only shape it has.
- ~~**The ghost is drawn with debug lines**, which are compiled out of Shipping builds.~~
  **Done 2026-11-02**, see *The ghost is real geometry* at the end of this file.

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
anything whose top is outside `MantleMinHeight..MantleMaxHeight` - the 400 cm panel always, and
since the crate went to 200 cm on 2026-11-03, **a single crate too**: its top sits above
`MantleMaxHeight` 170, so a crate is now a wall you smash rather than a step you climb. Raising
`MantleMaxHeight` past 200 on `BP_Character` would hand it back, at the cost of making everything
else that tall climbable as well. And C4. `bMantleOverSmash` on the movement component turns the whole thing
off for the old behaviour.

### Reversed: breaking beats climbing (2026-11-06)

The rule above was turned around. `bMantleOverSmash` now defaults to **false**: hit a breakable
fast enough to break it and it breaks, climbable top or not. The sparing code is all still there
behind the flag, reach argument and all, so turning it back on restores 2026-11-01 exactly.

The reason it is safe to be this blunt is that **speed already separates the two cases and they
cannot overlap**. `SmashAhead` returns on its first line unless you are above the smash threshold,
so every slower approach never reaches the break loop at all and climbs precisely as it always did.
What changed is only the band above the threshold, which used to be the one place a crate could be
both climbable and smashable at once - and the old tie-break there was never played, only reasoned
about (see *Still unverified* below).

Changed with it: **sliding lost its separate threshold.** It used to break things at
`SlideMinSpeed * 0.9` - 450 - where everything else needed `SmashSpeed`, so almost any slide burst
almost anything it brushed. That second test is gone, and `SmashSpeed` is now **1100**: one rule for
running, sliding, falling, a pad launch and a ring boost alike.

Getting to 1100 took two passes and both were instructive. The first raised the slide threshold to
`SlideMinSpeed * 1.1` (550) instead of dropping it, and **playing it showed why that could never
work**: `SlideBoost` adds 250 on entry, so even a slide started at the bare minimum is instantly at
750 and cleared a 550 bar every time. The bar could only ever catch a slide already decaying towards
a stop. The second pass dropped the branch and set one rule at 800 - `RunSpeed`, so a full sprint
broke things and nothing less did.

**1100 is where it actually landed**, chosen deliberately to sit above *everything reachable on
foot*: sprint 800, wallrun 920, slide out of a full run 1050. With `bAirStrafeGain` off there is no
strafing past any of them, so breaking is granted by the level - a jump pad, a speed ring, a slide
down a ramp, or C4. See the table in *What breaks a breakable* in `docs/spikes-and-death.md`.

That reframes the climb band rather than narrowing it: a breakable on foot is now **always** a step,
and breaking-beats-climbing only decides anything when you arrive by piece. `SmashSpeed` is still
the one number to move if that balance feels wrong.

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

### Played and signed off (2026-11-01)

The three fixes above were played the day they were written and confirmed working. Two of them the
test course exercises directly: every breakable in it is 25 cm thick, which is exactly what the
thin-ledge mantle is for, and the surface-mounted ghost fix shows up placing pieces at the edge of a
wall or a ledge.

Still resting on reading rather than playing: **climbing a breakable instead of bursting it**,
unless a crate was placed during the placement phase to try it. No crate stands in the map (see
*What is actually in the map* in `docs/test-course.md`), so `bMantleOverSmash` has no standing
obstacle to prove itself against. To settle it, place a crate and jump at it above `SmashSpeed`: it
should be a step up, not scrap.

**It was never settled, and on 2026-11-06 it was reversed instead** - see *Reversed: breaking beats
climbing* above. So the behaviour that went unplayed has now been turned off by default rather than
proved, which is the cheapest possible outcome for it: nothing depends on the untested path any
more. What wants playing now is the other side, and it is the same experiment - place a crate, hit
it above the threshold and confirm it bursts, then hit it below and confirm you climb.

### Still unverified (placement and pieces)

The placement loop itself is confirmed working: place a piece, Enter to run, and the character moves
(that last part only after the Enhanced Input context fix above). **The ramp and the collapsing
bridge were played 2026-11-07 and both feel correct** - which closes the two open numbers below,
the bridge's 0.45 s hold and whether its tip impulse reads as a collapse rather than a shrug.
What *was*
checked without playing, by script against the running engine: the catalog deals eight pieces with
the right sizes and flags, the wedge builds with the same triangle winding the engine's own
`GenerateBoxMesh` uses (so it is not inside out), traces onto the slope land exactly where the maths
says with a constant normal, and the bridge's collapse and restore flip its state. What is **still**
not checked, now that the ramp and the bridge have been played: whether the ghost is legible for a
wedge, which is a question about the ghost rather than the piece and is open under *Still
unverified* for the ghost below.

The build clock (2026-10-17) was **played and signed off the same day** - see *Played and signed
off* at the end of this file. It was written wondering whether 30 s is enough to fly out, pick a
spot and place three pieces, and whether the 1 s hand-spent beat reads as a hitch. Both turned out
fine; both are single values on the game mode if that changes.

**Settled by playing, 2026-11-02: the wall and panel stay as they are.** They are rotated 90° from
the lab - the lab's wall at facing 0 lies *across* your view (its thin axis is the facing axis),
while here `Size` is 400x50x400, so the thin axis is Y and the wall runs *away* from you. Asked
whether to match the lab, the answer was that the rotation is good as it is, which is what the
guess under *How placement positions a piece* hoped: **a wall running away from you is the better
wallrun piece**, and this game is about wallrunning in a way the lab was not.

If it ever needs reversing it is two catalog entries - swap X and Y on `wall` and `break`. The
description under *How placement positions a piece* used to claim the lab's behaviour and has been
corrected to say what the game actually does.

## The ghost is real geometry (2026-11-02)

The placement ghost was `DrawDebugBox`, `DrawDebugDirectionalArrow` and two `DrawDebugLine`s, and
**every one of those is compiled out of Shipping and Test builds**. A Shipping package would have
shipped a placement camera with nothing under the crosshair at all - you would have been clicking
blind. This was *Next steps* 3 in CLAUDE.md and the last thing standing between the project and a
build fit to put in front of strangers.

It is now four components on `AGetawayPlacementPawn`, and it needed no new look: `M_VB_Pane` is
already additive, two-sided and hollow-faced with an edge band, which is a hologram. See *The
placement ghost* in `docs/look.md` for the material side.

| Component | What it is |
|---|---|
| `GhostBox` | the engine cube at the piece's footprint. Hidden for a ramp |
| `GhostWedge` | a procedural mesh carrying the ramp's own wedge. Hidden for everything else |
| `GhostArrowShaft` | a 120 x 7 x 7 cm bar out of the top face, along the piece's +X |
| `GhostArrowHead` | a 26 cm cube turned 45 degrees about the piece's up axis, so it reads as a point |

- **A ramp's ghost is the real wedge.** `AGetawayRamp::BuildWedgeGeometry` was pulled out of
  `BuildWedge` as a public static, and the pawn builds its ghost from the same call, so the shape
  you line up is the shape you get rather than a box with two lines traced along one edge. Both the
  cube and the wedge fill the same 100 cm cube, so `LocalSize / 100` scales either - the same rule
  the spawned piece uses. **Only what is drawn changes:** the overlap test is still run on the whole
  footprint box, which is why a ramp is still refused where its empty upper half would not fit.
  Asked and settled 2026-11-02; the alternative kept was a box plus arrow plus slope edges.
- **Which piece is a wedge is asked of the class**, `Def.ActorClass->IsChildOf(AGetawayRamp)`, on
  the new `FGetawayPlacement::bWedge`. Not a flag in the catalog, so a Blueprint child of
  `AGetawayRamp` is drawn correctly without anyone remembering to tick a box.
- **The arrow stays on a wedge**, even though the wedge shows its own slope. From behind, the
  wedge's tall back wall is all you can see, and the arrow is the only thing that still reads.
- **Absolute transforms.** All four hang off the pawn's root but set
  `SetUsingAbsoluteLocation/Rotation/Scale`, so they sit where the placement is instead of being
  dragged around by the fly camera they are attached to.
- **Invisible by default, and built only where they are flown.** The server holds a copy of every
  client's placement pawn, and `Tick` returns early there, so the components would otherwise sit
  visible at the world origin. The materials are built lazily on the first frame the ghost is
  actually drawn, which is only ever reached on the machine flying the camera - see *Fixed: the
  ghost went black after the first round* below for why that is not in `BeginPlay`.
- **Two dynamic material instances, not one.** The boxes keep `BandMode` 0, which measures the edge
  band in centimetres off the bounding box; the wedge takes `BandMode` 1, the UV band, for exactly
  the reason `MI_VB_Ramp` does - a bounding box cannot find a wedge's sloping face. See *How the
  edge band works* in `docs/look.md`.
- **Cyan for a spot you may use, red for one you may not**, which is what the debug lines did.
  Settled 2026-11-02, and a deliberate exception to the colour rule in `docs/look.md`: the ghost is
  a cursor, not architecture, so it gets its own hues. `GhostEdgeColour`,
  `GhostBlockedEdgeColour` and `GhostFaceScale` are on the pawn in the Details panel. The face is
  only tinted, not lit: the material is additive, and a bright face washes out the thing you are
  lining the piece up against.
- **`bDrawGhost` still turns the whole thing off**, and `UpdateGhost` hides itself when building is
  shut, so a box tracking your crosshair never invites a click the server will refuse.

### The ghost material is content, so it is built by script

`MI_VB_Ghost` is a new row in `Content/Python/adrenaline_look.py`'s palette - additive, cyan-edged,
`GridStrength` 0 so a cursor does not carry the world grid. **Run that script once in the editor or
there is no ghost material.** The pawn holds it as a soft reference resolved at `BeginPlay`, not a
constructor `FObjectFinder`, which would warn on every editor start until the script had been run.
If it will not load, the log says so in one line and the engine default material stands in: ugly,
but visible, which is the entire point of the change.

### Still unverified

**Not played.** It builds clean, and the reasoning above is reasoning. Open:

- **Whether the ghost reads at all** against a level made of the same additive pink panes. The
  ghost is a see-through box inside see-through geometry, and that is the obvious risk.
- **Whether cyan and red still work** now that the ghost is a lit surface rather than a 3 px line.
  Red on near-black was easy to see as a wireframe; as an additive face it may just look dim.
- **Whether the edge band suits a 7 cm bar.** At `BorderWidth` 6-8 the arrow should fill solid,
  which is what is wanted, but it has not been looked at.
- **Whether the wedge ghost is clearer than the old box.** It should be, and it was asked for, but
  a ramp whose ghost no longer shows the footprint may make the "blocked by empty air above the low
  end" rule harder to understand rather than easier.
- **The 45-degree arrowhead.** A cube turned about the piece's own up axis should read as a point
  from any angle; on a piece lying over on a slope that axis is tilted, and nobody has looked at
  one.

### Fixed: the ghost went black after the first round (2026-11-02)

Reported from playing, the same day the ghost landed: "the first placement works then it goes
black" - the outline, not the screen. Round 1's ghost was cyan; every placement phase after it drew
a black box.

**The ghost was wearing the engine's default material, which is lit.** In a world whose lights are
dimmed to zero that renders pure black - the same thing that makes an undressed `BP_Character` a
black silhouette (see *Known gaps and traps* in `docs/look.md`). So the symptom was not a colour
being set wrong; it was `GhostDynamic` never being created, leaving the components on whatever
material they were born with.

The cause was one line in the wrong function:

```cpp
void AGetawayPlacementPawn::BeginPlay()
{
    if (IsLocallyControlled())   // false here for every pawn after the first
    {
        // ... create the dynamic materials
    }
}
```

`AGetawayGameMode::StartPlacementFor` does `World->SpawnActor<AGetawayPlacementPawn>(...)` and
possesses the result on a following line. For an actor spawned into a world that has already begun
play, **`BeginPlay` runs inside `SpawnActor`** - before the `Possess` call - so the pawn has no
controller yet and `IsLocallyControlled()` is false.

Round 1 worked for a reason that makes the bug look stranger than it is: that pawn is created from
`HandleStartingNewPlayer` during world start, and an actor spawned before the world's BeginPlay
dispatch has its `BeginPlay` called afterwards, by which point possession has happened. One code
path, two orderings, and only the rarer one was ever looked at.

**The fix is `EnsureGhostMaterials()`**, called from `UpdateGhost` and building the two dynamic
instances and the wedge section on the first frame the ghost is actually drawn. Nothing then has to
ask who owns the pawn at all: `Tick` already returns early when the pawn is not locally controlled,
so `UpdateGhost` is only ever reached on the machine flying the camera. It also covers a joining
client, where the replicated `Controller` arrives a frame or two after the pawn.

Worth recording because **this is the third time this project has been bitten by asking who owns a
pawn too early.** The other two are in `docs/multiplayer.md`: `AGetawayCharacter::SetProxySimulation`
checks `ROLE_AutonomousProxy` rather than `IsLocallyControlled()` precisely because the role is set
when the actor channel opens and the controller arrives later, and the placement pawn's own `Tick`
guard was written for the same reason. The rule that keeps coming out of it: **do not ask who holds
a pawn in `BeginPlay`.** Ask on the frame you need the answer.

It compiles clean. **The fix itself has not been played** - the diagnosis explains the symptom
exactly and the mechanism is checked against the engine's spawn path, but nobody has yet watched a
second placement phase draw a cyan ghost.

## The paddle wheel (added 2026-11-03)

Asked for: *"lets add an object that's 4 walls rotating kind of like windmill blades you can stand
on."* `AGetawayWindmill`, catalog id `windmill`, display name **Windmill**: four plates orbiting a
horizontal axle, carrying you up and over.

### The plates stay level, and that was the question

Asked and answered before any code: a **true windmill**, whose blades tilt as the wheel turns, or a
**paddle wheel**, whose plates orbit without turning like the cars of a ferris wheel. The answer was
level plates, and the reasons are worth keeping:

- A tilting blade stops being a floor the moment its top face passes the walkable angle, which is
  about 44 degrees. That is a quarter turn of ride and then you slide off - a timing piece, not a
  lift. Level plates are a floor the whole way round.
- A tilting blade also turns **underfoot**, and the engine pipes a movement base's rotation into
  your view: `UpdateBasedMovement` composes the base's delta rotation onto the pawn and then calls
  `UpdateBasedRotation`, which moves the camera with it. Standing on a windmill blade would spin
  your look. The way out is `bIgnoreBaseRotation` on the character movement component, and nothing
  else in this game turns underfoot, so that would be a character-wide setting bought for one piece.
- With level plates, a plate can only ever hand a rider a **translation**. There is no case where
  the ride fights the camera, and no new character property at all. The character was not touched.

So there is deliberately no `bBladesTilt`. If the tilting version is ever wanted it is a different
piece, with that character setting as part of its price.

### The shape

Free-standing and **directional**: the placement arrow runs along the axle, so a wheel goes up
facing you, plates climbing in front of you, and **R** turns it 90 degrees to sweep across your path
instead. The plate at the bottom of the stroke sits 40 cm off the floor, which is a step up, so you
walk onto it rather than having to aim at it.

It is **not scaled to its catalog Size** - `bScaleToSize` is false, as for the C4 pickup. The piece
builds itself in plain cm from its own properties, which is why none of its layout code has to
divide an actor scale back out the way `AGetawayVortex::LayOutColumn` does. The catalog Size is
therefore only the placement ghost, and it has to equal the box the turning wheel actually sweeps:
`AGetawayWindmill::GetSweptSize()` is that box, and the catalog entry carries the same three numbers
by hand. **At the defaults they are 350 x 700 x 540.** Retune `BladeReach`, `BladeWidth` or
`BladeThickness` and the ghost starts lying about the footprint until the catalog is retyped to
match. That is the one maintenance trap in the piece.

| Part | What it is |
|---|---|
| `Mount` | The root. Never moves; the axle is its local +X |
| `Wheel` | Turns. The hub and the spokes hang off it, so they are what visibly rotates |
| `Hub` | A barrel lying along the axle. Cosmetic, no collision |
| `Arm0..7` | Spokes, hub out to each plate. Cosmetic, pooled, **no collision on purpose** - the one place a spoke crosses is exactly where a rider stands |
| `Blade0..7` | The plates. Movable blocking cubes, attached to `Mount` rather than `Wheel` so their orientation never changes |
| `Contact0..7` | A box per plate, 12 cm bigger than it, query-only against pawns. What a sweep is noticed in |

The contact boxes hang off `Mount` beside their plate rather than inside it, because a box parented
to a scaled mesh is multiplied by that mesh's scale and its extent would stop being in cm.

### Standing on one is the engine's own code

A plate is a `Movable` blocking primitive, which is the whole of what
`UCharacterMovementComponent::UpdateBasedMovement` asks for: it carries whoever is standing on a
movable base by that base's transform delta, every frame, before anything in our movement component
runs. `UGetawayMovementComponent` overrides `CalcVelocity`, `PhysCustom` and friends but **not**
`PerformMovement`, so nothing in the Source-style movement had to learn about moving floors.

Two pleasant consequences that fall out for free: `bImpartBaseVelocityZ/X/Y` default to true, so
**jumping off a rising plate carries the plate's motion with you**, and bunny-hopping along a plate
works exactly as it does on a floor.

The wheel ticks in `TG_PrePhysics`, the group characters tick in, so the plates have moved before a
rider's movement asks its base where it is.

### The spin is derived, not replicated

The wheel angle is a pure function of `AGetawayGameState::GetRunTime()`:

```cpp
return StartAngle + SpinRate * Elapsed;
```

`GetRunTime` is itself computed from the **replicated** run start time, so every machine lands on
the same angle on the same frame, with nothing going over the wire and no drift to correct. This is
not the same trick as `AGetawayVortex`'s spinning cage, which each machine runs on its own clock
because it is cosmetic. Here the spin decides where the floor is, so it has to agree - and an
*absolute* angle agrees in a way an accumulated one never does: a client that hitched, or joined
late, is simply right on its next frame rather than having to catch up.

It also hands us the whole round loop for nothing. `GetRunTime` returns 0 outside a run, so the
wheel sits at `StartAngle` through placement - you build against a still wheel - and it freezes
where it stood when the round ends. `bSpinOutsideRun` falls back to world time for a wheel dropped
into a map by hand, which is the only case with no run to read.

`SetArmed` is still wired into both of `AGetawayGameMode`'s arm/disarm loops beside the spikes, the
pads and the vortex, because the **shove** does need gating; the spin gates itself.

### Being hit by a plate

Moving a component without sweeping does not push a character out of the way - it interpenetrates,
and the character's own depenetration decides where they end up, which can be through the wall
behind them. So each plate hands its own velocity to anybody its contact box has caught:

- The velocity is `omega x r` about the axle. A plate never turns, so **every point on it moves at
  the same velocity** and there is nothing to work out per contact point.
- It is delivered through `UGetawayMovementComponent::ApplyBlast`, the same path as the jump pad and
  the C4 blast, so it already knows how to drop you out of a wallrun or a mantle and off the ground.
- **Only on the machine that owns that character**, like every other piece here. Client authority.
- `ShoveScale` 1.3 by default: 1 is exactly the plate's speed, which reads as being shoved along by
  it, above that it bats you, and 0 leaves the plates as plain blocking geometry.
- A rising plate catching you under the arm throws you up; a descending one drives you down. Both
  fall out of the same line and both seem right.

**Rider or victim** is two tests, not one. The movement base is the honest answer while you are
standing on a plate; a height test along the plate's own up axis covers the frames around a jump,
when the base has been cleared but you are still inside the contact box. Without the second test,
hopping on a plate would bat you sideways.

It **never kills you.** Only spikes and falling do that - though a plate throwing you off the course
certainly can, and that is the piece's best trap.

### Tuning it

All in the Details panel under **Getaway|Windmill**, on `BP_Piece_Windmill`:

| Property | Default | What it does |
|---|---|---|
| `BladeCount` | 4 | Plates round the axle. 2 is a see-saw with a long wait, 6 a near-continuous escalator |
| `BladeReach` | 250 | Wheel radius, so also the lift: 500 cm bottom of the stroke to the top |
| `BladeLength` / `BladeWidth` / `BladeThickness` | 350 / 200 / 40 | The plate: along the axle, across it, thick |
| `SpinRate` | 60 | Degrees a second. A turn every 6 s, 3 s to ride bottom to top. **Negative reverses it** |
| `StartAngle` | 0 | Where it stands while nobody is running. 0 puts a plate at the top |
| `bSpinOutsideRun` | off | On: turns on world time with no round on, for a wheel placed by hand |
| `ShoveScale` | 1.3 | How much of a plate's velocity a sweep hands you. 0 turns the shove off |
| `ShoveRetrigger` | 0.5 | Seconds before the wheel can hit the same player again |
| `ShoveViewPunch` / `ShoveShake` | 1.2 / **0** | Camera kick on being hit. The shake is 0 since 2026-11-03: a plate can catch you twice in a pass and it read as the screen wobbling rather than as being hit |
| `ArmThickness` / `HubRadius` / `HubWidth` | 24 / 55 / 140 | The frame |
| `BladeMaterial` / `ArmMaterial` | from the script | Cover all the pooled plates and spokes at once |

### A new piece did not reach the game by itself (hit, then fixed, 2026-11-03)

It happened immediately, so here is the whole of it. **Adding a catalog entry in C++ was not
enough.** `BP_GetawayGameMode` holds an *overridden* copy of `Catalog` - `dress_pieces.py` writes
that array when it rewires every entry's Actor Class - and an overridden property stops tracking
C++. The windmill was in `AGetawayGameMode`'s constructor, the Blueprint still held the eleven
older entries, and the piece was never dealt. `BP_Piece_Windmill` existed and looked right in the
Content Browser, which made it look like a dealing bug rather than a catalog one.

The check added with the piece did catch it, in the Output Log:

> Error: [Adrenaline dress] BP_GetawayGameMode's Catalog has no 'windmill' entry, so that piece
> will never be dealt.

**The fix is in the script, not in a click.** `rewire_catalog` now reads the array from
`AGetawayGameMode`'s own class defaults - the C++ values - copies each entry, sets its Actor Class,
and writes the result to the Blueprint. The Blueprint's catalog is rebuilt from C++ on every run,
so a new piece reaches the party box as soon as the C++ is built and the script is run once. No
Reset to Default, nothing to remember.

Two consequences worth knowing:

- **Everything that decides what a piece is - id, display name, size, directional, surface mounted,
  scaled - now lives in C++ and only in C++.** Editing those fields in the Blueprint's Catalog array
  is pointless; the next run of the script overwrites them. **Actor Class** is the one field the
  Blueprint owns, and the script sets that too.
- **A piece must keep its row in the script's `PIECES` table even if the row dresses nothing**, or
  the rewire leaves that entry pointing at the raw C++ class instead of its Blueprint. That is why
  the hand-dressed spikes row is `{}, {}` rather than deleted.

### Spikes and pads ride a plate (added 2026-11-03)

Asked for straight after the wheel was played: *"can we make it so spikes and pieces that attach to
it move with the piece its attached to?"* They did not, and the reason was one discarded value
rather than anything missing.

`AGetawayPlacementPawn::FindHostAt` traces back out of the face a surface-mounted piece was stuck to
and returned `Hit.GetActor()`; `AttachToHost` then called `AttachToActor`, which attaches to that
actor's **root component**. For every host there had ever been - a wall, a floor, a breakable panel,
a collapsing bridge - the root *is* the thing that moves, so nothing was ever wrong with it. The
windmill is the first piece whose moving parts are children of a root that stands still, so spikes
stuck to a plate attached to the hub and hung in the air while the plate orbited out from under them.

The fix is to keep the component. `FindHostAt` returns `Hit.GetComponent()`, and both
`AGetawaySpikes::AttachToHost` and `AGetawayJumpPad::AttachToHost` take a `UPrimitiveComponent*` and
call `AttachToComponent`. `Host` stays the owning actor, taken off the component, because that is
what the breakable `OnBroken` / `OnRestored` binding needs.

- **Nothing new goes over the wire.** `FRepAttachment` already carries `AttachComponent` alongside
  `AttachParent`, and `AActor::OnRep_AttachmentReplication` attaches the root to that exact
  component when it is set (checked in `Actor.cpp`, and the server fills it from
  `RootComponent->GetAttachParent()` in `ActorReplication.cpp`). Both pieces already had movement
  replication on for the collapsing bridge, which is the same mechanism.
- **`AGetawayC4` was already doing this** - a thrown charge sticks to the component it hit - so a
  charge stuck to a plate went round with it before any of this. This only brings the placed pieces
  in line with the charge.
- **Every existing host behaves exactly as before.** On a piece whose root is its only moving part,
  the component the trace hits *is* that root, so the attachment is identical to what it was.
- **The trim still applies.** Spikes are 400 x 400 and a plate is 350 x 200, so the bed is trimmed
  to the plate it is backed by rather than refused - which is the behaviour surface mounting already
  had, and it means a plate comes out fully spiked.
- **What still does not ride:** a free-standing piece resting on a plate. Attaching those would be a
  different decision - a floor placed on a plate would become part of the wheel - and it was not
  asked for.

**Played 2026-11-07: it works as intended.** Spikes and a jump pad stuck to a plate ride round with
it. So attaching a surface-mounted piece to the **component** it was placed on, rather than to the
host actor's root, was the right call and needs no follow-up - and the bet that nothing in the
movement component had to change for a rider on a moving plate has now been collected twice, once
for the plate itself and once for what is bolted to it.

### Still unverified

- ~~**Nothing about it has been played.**~~ **Played 2026-11-03, and it works** - *"everything
  works"*. The based-movement bet came off: a plate carries you with no change to the movement
  component at all. The first attempt that day never got a wheel into a hand, which was the catalog
  trap above, not the piece.
- **How a ride feels at the top of the stroke**, where a level plate is moving sideways as fast as it
  is moving up. 60 degrees a second is a first guess.
- **Being batted by a plate.** `ShoveScale` 1.3 is a guess, and the rider-or-victim height test is
  the part most likely to want a different number than 8 cm.
- **Two machines.** The spin is derived from the replicated run clock on purpose, which is the right
  shape, but no wheel has turned on two machines at once - and a rider's position is their own
  machine's business, so a plate that disagreed by a few degrees would show up as somebody standing
  slightly off their plate.
- **A piece placed inside the swept circle** is swept through by a plate rather than stopping it.
  Placement only tests the ghost against where the plates are standing at `StartAngle`.
- **Free-standing pieces resting on a plate do not ride it.** A floor or a crate dropped onto a
  plate is placed in the world where the plate happens to be, and the wheel sweeps out from under
  it. Only *surface-mounted* pieces attach, because attaching is what surface mounting is - and
  spikes and jump pads on a plate do ride, as of 2026-11-03; see the section below.

## The speed ring (added 2026-11-05)

`AGetawaySpeedRing`, catalog id `speedring`, 100 x 300 x 300 - **the first piece whose effect
outlives the piece.** Asked for as *"a ring (physical ring you can stand in and go through) that
speeds you up for 3 seconds when you go through it"*.

The three movement pieces now read as a set, and what separates them is how long they have you for:

| Piece | How long it has you | Where the effect is useful |
|---|---|---|
| Jump pad | one frame | at the pad |
| Vortex | as long as you stay in the column | in the column |
| **Speed ring** | **a tenth of a second** | **for three seconds afterwards** |

That is the whole point of it. You are through the hoop before you have registered it, and the
boost then runs on your own character's clock, so the ring is not where the speed is useful - it is
where you *collect* it, and what you do with it is the jump, the wallrun or the slide that comes
next. It is the first piece that rewards planning ahead rather than aiming.

### What the boost actually is

**It raises `RunSpeed` by `BoostScale` (1.5, so 800 -> 1200 cm/s) for `BoostTime` (3 s), and
nothing else.** That is `UGetawayMovementComponent::AddSpeedBoost`, and the reason it is a raised
ceiling rather than a velocity kick is Source friction: friction always drags you back toward
`StopSpeed` whatever your top speed is, so a bare kick is eaten in about a second and "for three
seconds" would have meant nothing. Raising the ceiling instead means **holding W keeps you up
there** until the clock runs out.

One number, read in one place, reaches everything at once. `GetBoostedRunSpeed()` is `RunSpeed`
times the live boost, and every place that treated `RunSpeed` as *how fast you are allowed to go*
now goes through it:

- ground acceleration and the crouch-walk speed (`CalcVelocity`)
- air strafing, including while sliding - and note `SourceAirAccelerate` caps the *target* at
  `AirSpeedCap` but scales the acceleration by the full wish speed, so a boost makes strafing bite
  harder too. Deliberate: a jump out of a ring should carry further. (Since 2026-11-06 the air path
  only takes this route while `bAirStrafeGain` is on. With it off the boost still reaches the air,
  as the ratchet's ceiling is the boosted wish speed, so a ring lets you hold 1200 in flight -
  you just cannot strafe your way past it. Slides keep the old path either way.)
- the double jump's redirect floor (`RunSpeed * 0.85`)
- the wallrun's speed target (`RunSpeed * 1.15`), so a boosted wallrun runs at the boosted pace
- `GetMaxSpeed()`, which is the ceiling the engine itself asks about
- the slide loop's pitch, so a boosted slide is not just pegged at the top note

The one deliberate exception is **the camera's speed FOV, which still reads the raw `RunSpeed`**.
It is relative by design, so if it read the boosted value a boosted run would look like a normal
jog again - exactly backwards.

On top of the ceiling, `bSnapToSpeed` scales the speed you arrive with **straight up to the new
one** on the frame you pass through, so the ring hits like a surge rather than like homework. It
only ever multiplies what you already have, never redirects it and never slows you: the ring makes
you faster *along the line you took through it*, which is why it works going through from either
side. Vertical speed is left exactly alone, so jumping through a ring keeps the arc you aimed for
and simply covers more ground with it.

`SnapMinSpeed` (300) is the floor under that. Below it the snap is skipped and only the ceiling
goes up, so creeping into a ring does not fling you at 1200 cm/s in whatever direction you happened
to be nudging - you accelerate into the boost yourself instead.

**Boosts do not stack.** The stronger `BoostScale` wins and the longer clock wins, so running two
rings back to back refreshes the three seconds rather than compounding into something nobody tuned.

### How you know it is live

A **camera punch** (0.7 degrees, lighter than a jump pad's 1.2 - the ring does not change your
direction, so a big punch would read as a collision) and a **flat FOV bump** of `BoostFOV`
(9 degrees, on `AGetawayCharacter` under Getaway|Look) for as long as the boost lasts. The existing
`FInterpTo` eases it in and out over about a sixth of a second, so it reads as a surge arriving and
then relaxing rather than as two cuts.

It has to be a flat bump rather than more speed FOV for the reason above: the speed FOV is relative
to `RunSpeed`, so a boost that is merely *held* - coasting at the new ceiling, not strafing - would
otherwise look like a slightly fast jog. Between them the two give about 12.6 degrees at 1200 cm/s.

**There is no HUD message for it**, asked for that way. The FOV is the signal.

### The piece

- **It is a gate, so it is directional.** The axis of the hole is the piece's own +X, which is the
  way the placement arrow points, so a ring faces you as you place it and R turns it side on. The
  facing says which way the hoop *lies*, not which way it pushes - it works from either side.
- **The rim is not solid.** No blocking collision anywhere on the piece: clipping the edge of a
  hoop at 1200 cm/s and being stopped dead by it would be the worst thing in the game. Like the
  spikes, the pad, the vortex and the finish, the meshes have collision off and the trigger is
  QueryOnly overlapping pawns only.
- **The trigger is inset to the hole**, so what you have to hit is the gap rather than the piece. A
  box's inscribed circle is the hole exactly and its corners reach a little past the rim, which
  errs on the forgiving side - the same bargain as the wallrun's generous attach.
- **Free-standing, not surface mounted.** It stands on the floor (or against whatever you aimed
  at), takes the 50 cm grid and the *everything touches something* rule unchanged. At 300 x 300 the
  hoop's lowest point sits about 2 cm off the floor, so you really can run through it on the
  ground; the hole starts 22 cm up, which a running capsule clears without thinking.
- **The hoop is not held upright in world space**, unlike the vortex's cage. The vortex always
  lifts straight up whatever it is sitting on, so a tilted cage would lie about it; a ring's axis is
  only the gap you have to fly through and is free to tilt with the piece. In practice it never
  does, because free-standing pieces stand square on a slope since 2026-11-05.
- **There is no torus in the engine's BasicShapes set**, and this plugin ships no content, so the
  hoop is a ring of stretched cylinders - the same answer as the vortex's cage of bars and the
  ramp's hand-built wedge. Dashed rather than solid (`SegmentFill` 0.72), because the gaps are what
  make the spin visible at all. Rolled about the piece's +X only, so the non-uniform catalog scale
  cannot shear a segment.
- **Your own machine boosts you**, on the frame you pass through - the same client-authority
  bargain as the jump pad, dying on spikes and the rocket jump. Nothing about a boost goes over the
  wire; the ring itself replicates so the piece exists for everyone, with movement replication off
  because it never moves.
- **Armed only during a run**, like the spikes, the pad and the vortex: `StartPlacement` disarms
  every ring, `BeginRun` arms them, and arming clears the per-player retrigger times so a new round
  starts clean. The spin stops when it is disarmed, which is how you can see it is not live.
- **Cyan.** The palette says cyan for anything that helps you, and the ring is nothing but help, so
  `dress_pieces.py` gives the rim `MI_VB_Pickup` - the brightest thing in the palette, the same
  material the C4 pickup's charge wears.

### Tuning it

On the actor, under **Getaway|Speed Ring**, so it can be changed per ring in the Details panel or
on `BP_Piece_SpeedRing`:

| Value | Default | What it does |
|---|---|---|
| `BoostScale` | 1.5 | What the boost multiplies your top speed by. 800 -> 1200 cm/s |
| `BoostTime` | 3 s | How long it lasts after you pass through |
| `bSnapToSpeed` | on | On: lifted to the boosted speed instantly, so it hits like a surge. Off: only the ceiling rises and you accelerate into it yourself |
| `SnapMinSpeed` | 300 cm/s | The least speed you must already have for that snap, so creeping in does not fling you |
| `Retrigger` | 0.4 s | Per player, so one pass through the hoop is one boost |
| `BoostViewPunch` / `BoostShake` | 0.7 deg / 0.2 | The camera kick. Lighter than the pad's, because the ring does not redirect you |
| `SegmentCount` / `SegmentFill` | 12 / 0.72 | The dashed rim. Fill 1 closes the gaps and the spin stops being visible |
| `RingThickness` | 20 cm | World thickness of the rim, held constant however the piece is scaled |
| `SpinRate` | 70 deg/s | How fast the hoop turns about its own axis. 0 stops it |
| `RingMaterial` | from the script | The rim. Pooled, so it cannot be dressed from the Components panel |

`BoostFOV` (9 degrees) is on `AGetawayCharacter` under **Getaway|Look**, not on the ring, because
the FOV belongs to your camera rather than to any one piece. `bCameraEffects` off takes it with
everything else.

### A pre-existing build error that this piece uncovered

Adding `GetawaySpeedRing.cpp` reshuffled the adaptive unity build's groupings and put
`GetawayHUD.cpp` and `GetawayStickInput.h` in the same blob for the first time. Both had something
called `Live` - the HUD's cyan, and a local in `ApplyDeadZone` - and C4459, *declaration of 'Live'
hides global declaration*, is an error in this project's warning settings. The local is now
`Travel`. Nothing to do with the ring, and it would have bitten the next new file either way.

### Played and signed off (2026-11-05)

**Played the day it was built and it works** - *"all works, speed ring is awesome"*. Signed off at
the defaults, so none of the first-guess numbers needed changing: `BoostScale` 1.5 and `BoostTime`
3 s are the tuning, the snap at `SnapMinSpeed` 300 reads as a surge rather than a shove, a 300 cm
hoop is a fair target at speed, and **the FOV alone is enough** to tell you the boost is live - no
HUD line was wanted and none is needed. The suspicion recorded before playing, that 3 s might be
too long for a 300 cm hoop, did not come off: it is the three seconds afterwards that make the
piece, which is the whole bet the design was built on.

It is also the first piece that was right on the first pass, with no retune after playing - the
jump pad needed `GroundLift`, the panel and crate and bridge all needed resizing, and the wallrun
tilt needed its sign flipped.

### Still unverified (the speed ring)

- **Two machines.** Nothing about the boost crosses the wire, which is the same shape as the jump
  pad and the vortex, so it should be fine - but no ring has been run through on two machines.
- **Whether a boosted wallrun or a boosted slide is too much.** The boost reaches both on purpose
  and the ring was signed off as a whole, but neither was aimed at deliberately, so neither is
  *proved* so much as not complained about.
- **Two rings back to back.** The refresh-rather-than-stack rule is written and reasoned but was
  not built into a course and run.
- **A ring on a slope, or stuck in mid-air against a wall.** It takes the free-standing placement
  rules unchanged, so this is inherited rather than new behaviour, but it was not tried.

## The grapple pickup

> Added 2026-11-05, the day after the hook itself. **Built but not played.**

The fourteenth piece, and the second thing a player can carry. `AGetawayGrapplePickup` is a
near-copy of `AGetawayC4Pickup` on purpose: the same 110 cm pad, the same per-player rule, the same
coming back on death, the same trigger that overlaps pawns and nothing else so you can run, slide or
wallrun through it at full tilt. What floats above the pad is a hook rather than a brick - a shank
and a bend made of chords of a circle, so the shape is four numbers (`HookHeight`, `HookThickness`,
`ShankLength`, `BendRadius`) and not a mesh.

The hook *it gives you* is the one from `docs/effects.md`: read that for how it flies.

### Your hand holds one item

The rule that makes this piece interesting, and the reason it is not simply "C4 but a rope":

**`AGetawayCharacter::ItemCapacity` is 1, and it counts C4 charges and grapple hooks together.** A
pickup of either kind gives you nothing while you are holding anything, so a C4 pad and a hook pad
in the same stretch of course are a *choice* rather than a shopping trip. Both pads also lock
themselves to you while your hand is full - they wait, and hand over the moment it is empty, which
is the same waiting behaviour the C4 pickup already had for a full load.

- **Throwing counts as spending.** A charge stuck to a wall waiting for the click has left your hand, so you
  may pick up a hook while it sits there. Firing the hook is the same: the shot goes when it
  catches, so the moment you are swinging you are free to collect something else.
- **It replaced "carry up to two C4"** (decided 2026-10-15, retired 2026-11-05), so `MaxCharges` is
  1 now as well. `ItemCapacity` 2 with `MaxCharges` 2 puts the old behaviour back, and the two caps
  are separate on purpose: the hand is a cap across both items and never a floor, so C4 can still be
  limited on its own.
- **One line of HUD for the hand**, not one per item: `GRAPPLE`, `GRAPPLE out`, `C4 live`, `C4`, or
  `HANDS empty`. Two lines where one always said "none" was the thing the single hand made silly.

### One button for the whole hand

Added 2026-11-06, and it falls straight out of the rule above: if the hand holds one item then one
button can use it, so **left click is now the only button you need**. `OnUseItem` takes the click
and picks what it means, in this order:

| | if | the click |
|---|---|---|
| 1 | a rope is out | lets go |
| 2 | a charge is live | sets it off |
| 3 | a charge is in your hand | throws it |
| 4 | a hook is in your hand | fires it |

**Why 2 sits above 3**, which is the only ordering question here: the click after a throw must
always be the bang, or the change has not bought anything. It also settles the one case where two
things really do compete - you threw a brick, ran back over a C4 pad and restocked while the first
is still stuck, so you are holding one *and* have one live. Detonate-first means you clear the
course and then throw again; throw-first would leave you holding a charge you could not set off
without finding F.

- **A live charge is `HasLiveCharges()`**, and it is not simply `Charges.Num()`: on a client that
  array is always empty, because `ServerThrowC4` spawns the brick on the authority. A client asks
  the world for C4 actors it owns instead - the same handle `DetonateC4` uses to predict its own
  launch, and it works because an actor's `Owner` replicates down to the owning client.
- **The HUD says `C4 live`** while one is out, in the same slot as the rest of your hand, because
  otherwise the line reads `HANDS empty` at the exact moment the click does the most dangerous
  thing in the game.
- **G and F are still mapped.** Nothing was taken away; the two-key C4 works as it always did.
- **The pad keeps three buttons** - RB throw, RT detonate, LT hook. A dedicated button is never
  worse than a modal one and a pad has them spare. Collapsing the pad onto one button later is a
  two-line change if the split turns out to confuse anybody.
- **The C4 pickup now teaches the button**, since the run's HUD lists no keys any more: "Picked up
  C4. Left click to throw, again to set off", or the RB/RT wording on a pad.

**Played 2026-11-07 and signed off** - *"works"*. Both of the questions it was written with are
answered: detonate-before-throw is right, and `C4 live` in the hand slot reads clearly enough to
click next to. Committed as `5cd97af`.

### A pickup comes back when you die

Changed 2026-11-05, straight after playing the 8 s timer, and it applies to **both** pickups - C4's
as well as the hook's.

A pad you have emptied stays empty for as long as you keep yourself alive. Dying is what restocks
you, so it is one charge or one hook **per life**, and the pickups stop being a tap you stand next
to and start being a thing you spend.

- **`bReturnsOnDeath`** (on) is the rule; **`RespawnDelay`** is now 0 rather than 8 and adds a timer
  on top if you want one back.
- **It is named "returns", not "respawns"**, because `AGetawayGameMode::bRespawnOnDeath` already
  means the *player* coming back. The two do interact: with the game mode's flag off a death ends
  your run, so the pad never comes back for you that round.
- **The pickup watches the character that took it weakly**, and that is the whole mechanism - a
  death destroys the character, so the stale pointer *is* the news. Nothing subscribes to anything,
  nothing has to be told, and it falls out for free that every pickup frees itself between rounds,
  where all the characters are thrown away anyway. `IsDead()` is checked as well as validity, so the
  pad is ready by the time you are back at the start rather than a beat after it.
- ~~**The loop it opens, and it is not played yet:**~~ **Played 2026-11-07, and it is not a loop at
  all.** Returning on death works, and *"theres never a reason to intentionally die since you want
  to go fast."* The cost was priced correctly by accident: a death hands back the whole run-up, and
  in a race that is more than a charge is worth. **No rule needed**, and worth remembering as the
  general case - in a game scored on finishing, anything that costs you time polices itself, so a
  resource does not also need a cooldown.

### Both pads are per player, and always were

Worth writing down because it is easy to doubt: if you take a C4 pad, your rival can still take
*their own* charge from that same pad, and each of you sees your own pad empty.

Nothing arranges that. `TryGive` only ever hands out on the machine that owns the character
(`IsLocallyControlled`), because what you are carrying is that machine's count - client authority
again - and `bAvailable` is plain local state on each machine's copy of the actor, not replicated.
So a pickup is per player by construction, and making it *shared* would be the change that costs
work: routing the grant through the server like `AGetawayBreakable::Break` and replicating the
count. Decided for C4 on 2026-10-18 and inherited by the hook; see CLAUDE.md.

### Getting it into the party box

Like every piece: the catalog entry is in `AGetawayGameMode`'s constructor, but
**`BP_GetawayGameMode` stores the whole array**, so run `Content/Python/dress_pieces.py` from
Tools > Execute Python Script... once. That rebuilds the Blueprint's catalog from C++ and makes
`BP_Piece_GrapplePickup` with the pad in `MI_VB_Dark` and the hook in `MI_VB_Pickup` - cyan, by the
colour rule, because it helps you.

### Played 2026-11-07 (the grapple pickup)

- **It works, and the pad is not clear enough.** *"The grapple pickup isn't very clear but neither
  is the c4, they will be edited later."* So this is **not a grapple problem** - the two pads share
  it, and the C4's has been in the game since 2026-10-15 without the complaint surfacing, which
  suggests it took a second pad to make it visible: two cyan-and-dark discs on the floor that mean
  different things. Logged as one job on both pieces rather than a fix to this one, and deferred
  deliberately. The levers are the pad's own material and the thing turning over it (a hook here, a
  brick there), both in the Details panel of the piece's Blueprint, and neither is load-bearing to
  anything else.
- **The one-item hand is signed off.** *"One c4 was intentional and feels good."* Losing the double
  rocket jump cost nothing anybody misses, and `ItemCapacity` 1 stands - which also means
  *One button for the whole hand* rests on a rule that has now been played, not just reasoned about.
- **Coming back on death works**, on both pads, and it closes the loop it was thought to open - see
  *A pickup comes back when you die* above.

### Still unverified (the grapple pickup)

- **Two machines.** The grant runs on the machine that owns the character, exactly like the C4
  pickup's, so it is inherited rather than new - but it has not been watched.
- **Whether the pickup's own HUD line lands.** `AddGrappleShots` does notify - "Grapple hook. Left
  click to fire" - and the play-test still asked for a notification on pickup, so the line exists
  and did not register. See *A pickup that tells you nothing* in `docs/hud.md`.

## The void, and why it was scrapped (2026-11-06)

**A fifteenth piece was built and removed on the same day. Don't build it again without reading
this.** A see-through sphere that pulled you toward its middle and killed you at the centre. Five
versions, four plays, scrapped - *"it still feels awful, I think it's time to scrap it."*

The source is parked in `docs/archive/scrapped-void/` rather than deleted, because it was never
committed. It is not in the build, not in the catalog and not in the movement component.

### Why it failed, which is the part worth keeping

**Every piece in this catalog either grants you movement or tests your precision. The void did
neither.** Pad, ring, vortex, grapple, windmill: all grants. Spikes and the panel: precision - you
can wallrun straight past a spiked wall if you keep off it. The void pulled you somewhere you did
not want to go and then killed you there, and no version of that ever produced a moment of skill.

The first version was a **capture**: a strong pull that lifted you off the floor and damped your
input to 0.45. At 360 cm/s of run against a 500 cm/s pull, with the floor gone and the pull
*strengthening* the deeper you fell in, it was arithmetically inescapable - a death sentence with a
two-second delay where you mashed keys and watched. In a game whose entire appeal is movement
control, **reducing movement control is the one thing a piece cannot do.**

The redesign went the other way and made it a gravity-well slingshot: damping and lift gone, uniform
field, and grazing the lethal core paid a speed boost and a low-friction window scaled by how close
you cut, with a translucent ball to aim at. On paper that fixed everything - it was a route you
chose, with death as the price of greed, and it used the project's own levers to pay out. It still
felt awful.

**That is the actual lesson, and it is more useful than the first one:** a piece whose basic verb is
wrong cannot be rescued by a payoff bolted onto it. The sphere's problem was never the reward
schedule, it was that a volume which moves you is a thing that happens *to* you, and this game is
about what you do. Nothing in five passes changed that, because every pass changed numbers and
mechanisms around an unchanged centre.

### Four passes of tuning that were all on the wrong axis

Worth recording because each looked like the fix at the time, and the pattern - plausible local fix,
no improvement - is the signal that the piece is wrong rather than the numbers:

1. **The pull did nothing.** `MaxPullSpeed` was a cap on *your* velocity projected onto the pull, and
   the pull points where you are already heading whenever you run at something, so a sprint spent
   the whole allowance before the field added anything. Also, zeroing the vertical pull on the
   ground threw most of it away, because a floor-standing sphere has its core above your head.
2. **It worked in the air and nowhere else.** `SourceFriction` drops
   `max(Speed, StopSpeed) * Friction * dt`, so at rest that is a 1250 cm/s^2 floor against a 700
   cm/s^2 pull: the pull was *incapable* of moving a grounded player. Fixed by paying the friction
   back on the field's own share, the same trick `ApplyVortexLift` uses for gravity.
3. **You could still run out of it** - so input damping went in, and made it worse rather than better
   (see above).
4. **The slingshot redesign**, which was a real design rather than a number, and also did not land.

Two of those four were genuine bugs with real lessons, and both survive this piece:

- **Source friction eats anything handed to you**, which is why the speed ring raises your ceiling
  and the jump pad takes the drag away. See *A speed boost raises your ceiling* and *A jump pad
  takes the drag away* in CLAUDE.md. A third answer - pay the drag back on the piece's own share -
  worked and is gone with the code; `docs/archive/scrapped-void/GetawayVoid.cpp` has it if a future
  piece needs it.
- **A limit on "your speed in the direction a piece is pushing" measures what *you* did, not what
  the piece did**, whenever the piece picks its own direction. Every other piece hands you speed
  along a line you chose, so a ceiling on your speed is the right shape for all of them.

### If a void-shaped thing ever comes back

Not as a volume that moves you. The ideas that survive contact with the above:

- **A kill volume with no pull at all** - floating spikes, essentially. Legible, and it tests
  precision, which is a thing this game does. It is also boring, which is an honest reason not to.
- **A hazard that makes *other pieces* dangerous** rather than acting on a running player: something
  that only matters when a pad or a ring or a hook has already committed you to a line. The danger
  is then in your own route choice, which is where this game keeps its difficulty.
- Whatever it is, it should pass the test in *Every piece must grant ability or test precision* in
  CLAUDE.md before any code is written. That decision is the one thing this day produced that was
  worth the day.
