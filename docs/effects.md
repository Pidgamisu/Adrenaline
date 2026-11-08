# Round effects

> # THE LAYER IS OFF
>
> **Decided 2026-11-08: effects are gone.** `bRoundEffects` defaults to **false**, so no effect is
> ever dealt and no banner is ever shown. Nothing was deleted - every word below still describes
> working code - but none of it runs. Read *Effects are gone* at the end of this file before
> turning it back on or building anything on it.
>
> The short version: abilities belong to **pieces**, and low gravity was actively bad.

> The first three were built and **played 2026-11-03 - all three work.** The fourth, the **grapple
> hook**, was played 2026-11-07 and **signed off at its first-pass numbers** - *"plays very well and
> feels like a slingshot, which is the intended effect."* Read this before touching anything that
> hands a player an ability for one round only. The part still not played is how the banner now
> presents itself: see *The banner* below.
>
> **But the whole layer is now in question**, as of the same play-test: the leaning is to stop
> dealing effects per round and let **pieces** hand out the same abilities instead. Read *Effects
> may become pieces* at the end of this file before building anything new on top of the round
> layer.

Every other round, both players are given the same effect before anybody builds. One at a time, the
same for everybody, and gone the moment the round is.

| Effect | What you get |
|---|---|
| **Double jump** | One jump in the air. The tuned default is `AirJumps` 0, so this is the only way to get one |
| **Loaded** | You start the run carrying one C4, and can hold only the one |
| **Low gravity** | Gravity at 60% - higher jumps, slower falls, and a C4 launch that throws you much further |
| **Grapple hook** | One hook. Left click pulls you to whatever you are looking at; click again to let go. Also a piece you can place - see below |

Round 1 is plain, round 2 has an effect, round 3 is plain, round 4 has one. `EffectEveryNthRound`
is the rule: an effect lands on every round the number divides, so 2 gives 2, 4, 6. Round 1 is
plain because an effect is a *change* from the way the round before played, and the first round has
nothing to be a change from.

## Where each part lives

The split follows the round loop's: the game mode decides, the game state carries it, and the
character applies it on the machine that owns it.

| Where | What it does |
|---|---|
| `EGetawayEffect` in `GetawayGameState.h` | The list. `None`, `DoubleJump`, `Loaded`, `LowGravity`, `Grapple` |
| `AGetawayGameMode::PickEffectForRound` | Draws one, or `None` on a plain round. Server only |
| `AGetawayGameState::ActiveEffect` | Replicated, plus `GetEffectName` / `GetEffectBlurb` for the HUD |
| `AGetawayCharacter::ApplyRoundEffect` | What an effect *means*, applied in `BeginPlay` |
| `AGetawayHUD` | The banner: centre screen as the build phase opens, then the corner all round |

- **It is decided in `StartPlacement`**, before a hand is dealt or a piece is placed, and drawn
  straight away. That is the whole point of effects being a layer rather than a surprise: knowing
  everyone has low gravity this round is meant to change *what you build*, not only how you run it.
- **One replicated value is why both players cannot differ.** There is nothing per-player to go out
  of step - a client reads the same enum the server wrote, and the rule for when an effect lands
  never has to exist on a client at all.
- **They cannot stack and they cannot leak into the next round.** `ActiveEffect` is overwritten
  every placement phase (to `None` on a plain round), and the character that carries the effect is
  thrown away and respawned at the start of every round anyway. There is no "clear the effect" step
  to forget, because nothing survives to be cleared.
- **The character applies it locally**, which is the same bargain as the rest of the movement: each
  machine tunes its own character and nobody else's. A proxy has its movement component switched
  off and its position streamed, so there is nothing on it worth tuning.

## The four, in detail

- **Double jump** calls `UGetawayMovementComponent::SetAirJumps`, which exists for this and sets
  `AirJumpsLeft` as well as `AirJumps`. Writing the property alone is not enough: the component has
  already begun play by the time the character's `BeginPlay` runs, and it took its own copy of
  `AirJumps` into `AirJumpsLeft` when it entered walking mode - so you would stand there with the
  jumps you had a frame ago (none) until the next time you landed.
  It takes `FMath::Max(AirJumps, EffectAirJumps)`, so an effect round can never hand you *fewer*
  jumps than normal if the tuned default is ever raised off zero.
- **Loaded** sets `MaxCharges` to 1 as well as giving you the charge. The cap is the half that is
  easy to miss: a `AGetawayC4Pickup` anybody placed in an earlier round is still standing, and
  without the cap you could top up to the usual two and the effect would quietly be "two C4".
  A pickup still works - it tops you back up to one and no further.
- **Low gravity** multiplies the movement component's `GravityScale` rather than setting it, so it
  stays relative to whatever the character is tuned to (2.04 → about 1.22). `AGetawayC4::Launch`
  reads `GravityScale` too, which is why a rocket jump goes so much further on these rounds - that
  falls out of it rather than being coded, and it is the best thing about this effect.

- **Grapple hook** is the only one of the four that is a *button* rather than a changed number, so
  it is split across three files: `AGetawayCharacter::FireGrapple` aims it and spends the shot,
  `UGetawayMovementComponent::StartGrapple` / `ApplyGrapplePull` do the pulling, and the character
  draws the rope. Three things about it are decisions rather than details:
  - **It is not a movement mode.** You stay in falling, or on the ground, and the pull is simply
    velocity added every frame on top of whatever you were doing - so Source air strafing, the
    slide, even catching a wallrun mid-swing all keep working while the rope is attached. "You can
    still move while grappled" therefore cost nothing to build; a `MOVE_Custom` grapple would have
    had to re-implement all of it.
  - **The pull leans toward where you are looking** (`GrappleViewSteer`, 0.35), rather than running
    straight down the rope. That is the steering: look off to one side and you swing round the
    anchor in an arc instead of being winched into it. Normalising a unit vector plus a shorter one
    can never turn it a full 90 degrees, which is the geometry behind "you can never pull yourself
    *away* from the hook" - it holds at any value of the knob.
  - **It ends itself.** Arriving (within `GrappleReleaseDistance`, 150 cm), the line being cut by
    geometry, or the thing you hooked being destroyed all drop the rope, and so does dying. The
    weak pointer to the anchor actor is the whole of that last test: smash the panel somebody is
    hanging off and their rope goes with it, with nothing told about anything.
  - **Breaking it never slows you.** `StopGrapple` does not touch your velocity, by any of those
    routes. The arc you are on is what you spent the hook for.
  - **It is also a pickup**, since 2026-11-05: `AGetawayGrapplePickup` is a placeable piece that
    hands one over, so the hook is not only an effect-round gift. The effect round and the pickup
    write the same counter, `GrappleShots`, and both are capped by the **one-item hand** -
    `AGetawayCharacter::ItemCapacity` 1, counting C4 and hooks together. The consequence worth
    knowing for the effect: on a Grapple round everybody starts holding a hook, so every C4 pad on
    the course is shut to them until they fire it. See *The grapple pickup* in `docs/placement.md`.

## The banner

Changed 2026-11-03, straight after playing the first version: the effect **sits big in the middle
of the screen for two seconds as the build phase opens, then travels up to the top left corner**
and stays there, small, for the rest of the round - placement and run both. It was previously a
small line in the corner from the first frame, plus a crosshair message, and the thing you most
need to read was the thing easiest to miss.

**It is announced as the build phase opens, not as the run starts** (asked for 2026-11-03). That is
the moment it can still change something: the effect is dealt before anybody builds precisely so it
changes what you place, and an announcement at the start of the run would be telling you after the
decision was made. The run inherits the line in the corner and plays no arrival of its own.

`AGetawayHUD::DrawEffectBanner` is the whole of it, called from both phases, and it is drawn rather
than animated:

- **It is positioned off a replicated clock**, not a local timer: `GetPlacementElapsed`, which is
  why `PlacementStartServerTime` was added to the game state. It counts *up* from the start of the
  phase rather than down to its end, because the banner wants the phase's first two seconds and
  that has to work with `PlacementTime` 0, where there is no end to count down to. Both machines
  therefore hold it for the same two seconds and it cannot drift.
- **The run passes a negative elapsed**, which the banner reads as "already arrived" and draws at
  rest. One function, two phases, no second arrival.
- **The travel is one eased interpolation** of position and size, with the name measured at the size
  it is being drawn at this frame. Without that the text jumps as it arrives.
- **The blurb fades out with the move** and the name travels on alone, because the explanation is
  for the moment you are reading it and not for the whole run.
- **The character says nothing.** `ApplyRoundEffect` used to push a crosshair message, which would
  now be a third telling of the same thing. The comment there says so.
- `EffectBannerTime` (2 s) and `EffectMoveTime` (0.35 s) are on the HUD under **Getaway|Effects**.
  `EffectMoveTime` 0 snaps it to the corner instead of travelling.

## The knobs

`BP_GetawayGameMode`, **Getaway|Effects**:

| Knob | Default | What it does |
|---|---|---|
| `bRoundEffects` | on | Off plays the game exactly as it was before effects existed |
| `EffectEveryNthRound` | 2 | An effect on every Nth round. 1 gives every round one |
| `EffectPool` | all three | Drop one here to retire it without a rebuild |
| `bNoRepeatEffect` | on | Never the same effect two effect rounds running |

The HUD has no Blueprint of its own - the game mode sets `HUDClass` to `AGetawayHUD` directly - so
`EffectBannerTime` (2 s) and `EffectMoveTime` (0.35 s) are C++ defaults for now. Make a Blueprint
child of `AGetawayHUD` and point `HUDClass` at it if you want to tune them in the editor.

`BP_Character`, **Getaway|Effects**:

| Knob | Default | What it does |
|---|---|---|
| `EffectAirJumps` | 1 | Air jumps on a Double Jump round |
| `EffectC4Charges` | 1 | Charges you start a Loaded round with |
| `EffectC4MaxCharges` | 1 | How many you may hold on a Loaded round |
| `EffectGravityScale` | 0.6 | Gravity on a Low Gravity round, as a fraction of normal |
| `EffectGrappleShots` | 1 | Hooks you get on a Grapple round, capped by `MaxGrappleShots` and the hand |

`BP_Character`, **Getaway|Grapple** - the hook as a thing you aim and look at:

| Knob | Default | What it does |
|---|---|---|
| `GrappleRange` | 3500 | How far the hook reaches, in cm. 35 m |
| `RopeThickness` | 5 | How thick the rope draws, in cm |
| `RopeColour` | cyan | The colour rule: cyan is the things that help you |
| `RopeBrightness` | 1.8 | Above 1 is what blooms |
| `RopeHandOffset` | (30, 20, -22) | Where the rope leaves you, measured from the camera |
| `RopeMaterial` | `M_VB_Pane` | Additive and unlit, tinted to `RopeColour` in a dynamic instance |

`BP_Character`'s **movement component**, **Getaway|Grapple** - the hook as a force:

| Knob | Default | What it does |
|---|---|---|
| `GrapplePull` | 4500 | How hard it reels you in, cm/s^2. Only ever adds speed |
| `GrappleMaxSpeed` | 1800 | The fastest the rope itself will drag you along its line. Speed you earned elsewhere is never taken off you |
| `GrappleViewSteer` | 0.35 | How far the pull leans toward your view. 0 winches you straight in |
| `GrappleGravityPayback` | 1 | How much of gravity the rope holds up. 1 is a zip line, lower sags and swings |
| `GrappleReleaseDistance` | 150 | Get this close and it lets go, because you have arrived |
| `GrappleCutSlack` | 60 | How far short of the anchor something must block the line to count as cutting it |

## Decided along the way

- **"Double jump" means one air jump, not an extra one.** `AirJumps` is 0 in `BP_Character` despite
  the 1 in the C++ default, so the base game has no air jump and this effect is the whole thing
  rather than a top-up. If that BP value is ever raised to 1, this effect becomes invisible - raise
  `EffectAirJumps` with it.
- **A respawn mid-round gives the effect again**, including the Loaded charge, because a fresh
  character runs `ApplyRoundEffect` like any other. That reads as the right answer - the effect
  says *you start the run with one C4*, and a death puts you back at the start - and it is a
  separate thing from the 2026-10-31 rule that charges you went and *collected* do not come back
  with you. Those still do not.
- **The effect is announced, not hidden.** A one-line message as the run starts plus the name
  top-left for the whole run. Guessing why you are floating is not a mechanic.
- **One hook, spent on the catch.** A click that hits nothing costs nothing and says "nothing in
  reach" - one hook a round is meant to be a decision about *where* in the course to spend it, and
  losing it to a click at open sky would be a decision nobody made. Letting go costs nothing
  either, because the shot went when the hook caught.
- **A grapple round says nothing on any other round.** `bGrappleHook` on the character is what
  tells `FireGrapple` and the HUD whether this player has a hook at all, which is why left click is
  silent the rest of the time rather than answering every click with a line of text.

## Still unverified

- **The banner's two seconds and its travel have not been played** in their build-phase home,
  only in the run-start version they replaced. It was put on the 2026-11-07 play-test list and came
  back with the answer below instead - that the layer itself may go - so there is no verdict on it,
  and no point tuning it until that is settled.
- **The rhythm and the numbers are a first pass**: whether every other round is right, and whether
  60% gravity is the right amount, are both open.
- **Two machines have never seen it**, like everything else in the project. `ActiveEffect` is one
  replicated enum set 30 seconds before any character spawns, so a client should always have it in
  hand by the time it matters - but that is reasoning, not a play-test.
- **The pool is drawn at random**, so it can take a few rounds to see all four. `bNoRepeatEffect`
  means you will at least never get the same one twice in a row.
- ~~**The grapple hook has never been played.**~~ **Played 2026-11-07 and signed off unchanged** -
  *"plays very well and feels like a slingshot, which is the intended effect."* Every number held:
  `GrapplePull` 4500, the 1800 ceiling, `GrappleViewSteer` 0.35 and holding all of gravity up. The
  second piece in a row to need no retune after playing, after the speed ring. **Note which word
  came back** - slingshot, not zip line, which was the risk written down here before it was played:
  holding gravity up does not flatten the swing, because the pull is strong enough that you arrive
  with speed to spend rather than gliding. `GrappleCutSlack` was not complained about either, so the
  rope cutting behind a corner is reading as deliberate.
- **A rival's rope has never been seen.** `bRemoteGrappling` and `RemoteGrappleAnchor` are pushed
  to the server on the frame the rope goes out or comes in, which should be enough for something
  cosmetic, but no second machine has watched one.

## Effects are gone (decided 2026-11-08)

*"Lets remove effects."* Settled the day after it was raised, and with a second reason that arrived
with it from a play-test: *"the feedback i got from my playtest was that low gravity mode felt awful
cause it takes away the feeling you get of the game (like how far jumps get you and such)."*

`bRoundEffects` is **false** by default. One line, and the switch was already there because the
layer was built with it from the start. What follows is the open question as it was written on the
7th, kept because the argument is the record of why.

### Why the layer went

Two reasons, and they are different in kind - one is design tidiness, the other is a play-test
telling you the feature was bad.

**1. Abilities belong to pieces.** Since *Speed comes from the pieces, not from strafing*
(2026-11-06) everything you can do in this game comes from what somebody chose to place. An effect
dealt by the game mode was the last ability the level did not grant, so it had become the odd one
out rather than the norm. And *A hand is one normal piece and one special one* (2026-11-06) made the
hand the place a round is decided - where an effect arrived before anybody built, applied to both
players equally, and was therefore never something you chose. That is the opposite of the Ultimate
Chicken Horse bargain the rest of the catalog runs on.

**2. Low gravity broke the calibration the game is built on**, which is the more interesting half,
because it is the one that could not have been reasoned out from a design rule. See *A piece may not
rescale what you have learned* below.

### What the game actually loses

| Effect | Covered by | Lost? |
|---|---|---|
| One C4 (`Loaded`) | `AGetawayC4Pickup`, which grants exactly this | No |
| Grapple hook | `AGetawayGrapplePickup`, built 2026-11-05 | No |
| Double jump | nothing | **Yes, entirely** |
| Low gravity | nothing, deliberately | **Yes, and good** |

Two of the four already had their piece, which is what made the removal cheap. **Double jump is now
nowhere in the game**: `BP_Character` plays `AirJumps` **0**, so the effect was the only way anyone
ever got one, and *Double jump: 1 air jump* in `CLAUDE.md` currently describes something you cannot
do. That is either a piece to build or a 0 to change to a 1 on `BP_Character`, and it is a feel
question rather than a code one.

### A piece may not rescale what you have learned

The rule already on the list is *every piece must grant ability or test precision; none may take
ability away*. **Low gravity passes that rule and was still awful**, so the rule was not finished.

Low gravity does not reduce your control - you can jump higher and go further, which is strictly
more. What it does is **invalidate everything you know about your own movement**. This game's
pleasure is a learned calibration: how far a jump carries, how much a slide out of a run adds,
where a wallrun runs out, whether that gap is yours. You build that up over hours. Low gravity
rescales all of it at once, so for one round you are a beginner again - and being handed a new body
is not the same as being handed a new ability.

So, the wider rule: **a piece may extend the calibration but may not rescale it.** A jump pad, a
speed ring, a grapple and a C4 jump all take you further *in the units you already know* - the
ground still pulls the same way, your jump is still your jump, and the piece adds to it. Gravity is
the unit itself.

Note what this does **not** condemn:

- **The anti-gravity vortex** (`AGetawayVortex`) is low gravity done as a piece, and it is fine. It
  is bounded in space, you enter it on purpose, you can leave it whenever you like, and it *grants
  a climb* rather than changing what your legs do everywhere. The lesson is not "gravity is
  off-limits", it is "not for the whole round, not everywhere, and not without being asked for".
- **The grapple hook holds all of gravity up** while the rope is out, and played as a slingshot. Same
  defence: you fired it, it is bounded by the rope, and you let go when you want.

Which means **low gravity as a piece is already built**, and the round-wide version was simply the
broken form of it. That is worth knowing before anybody designs a low-gravity volume.

### If it is ever turned back on

`bRoundEffects` true is the whole switch; `BP_GetawayGameMode` holds no override for it or for any
other effect property (checked in the asset), so the C++ default is what plays. Drop
`EGetawayEffect::LowGravity` out of `EffectPool` if it comes back - that is the one the play-test
condemned, and the pool is an array on the game mode exactly so a single effect can be retired.

### The argument as it stood on 2026-11-07

Kept verbatim below, because it is the reasoning the decision was made on - and because its cost
estimate is what the next person will want if abilities do become pieces. One correction with
hindsight: where it says "something placed for low gravity", the rule above says don't, and the
vortex is already the bounded version.

What it would mean: no dealt effect, no every-other-round rhythm and no banner. Instead a **piece**
hands out each ability, the way the C4 pickup and the grapple pickup already do - a pad you run
over for a double jump, something placed for low gravity - and it is in the level because somebody
chose to put it there.

**Why it is the stronger idea, in this game specifically.** Two decisions already on the list point
straight at it:

- *Speed comes from the pieces, not from strafing* (2026-11-06) moved every source of speed into
  the level. An effect dealt by the game mode is the last ability left that the level does not
  grant, so it is now the odd one out rather than the norm.
- *A hand is one normal piece and one special one* (2026-11-06) made the hand the place where the
  round is decided. An effect arrives *before* anybody builds and applies to both players equally,
  so it changes what you place without ever being something you chose - which is the opposite of
  the Ultimate Chicken Horse bargain the rest of the catalog is built on.

And the grapple is the proof of concept, by accident: it exists **both** ways already - as a round
effect and as `AGetawayGrapplePickup`, a pad that grants one hook. The pickup is the version that
fits the game, and it was built second.

**What it would cost.** Less than it looks, because the abilities are not implemented in the effect
layer - they are properties on the character and the movement component that the layer merely sets.

| Effect | Where the ability actually lives | What a piece would do |
|---|---|---|
| Double jump | `AirJumps` on the character (Blueprint plays at 0) | Set it to 1 for the rest of the round, or for a life |
| One C4 | `AddCharges`, which `AGetawayC4Pickup` already calls | Nothing new at all - the pickup *is* this |
| 60% gravity | `GravityScale` on the movement component | Set it while you are in a volume, or for a duration |
| Grapple hook | `AddGrappleShots`, which `AGetawayGrapplePickup` already calls | Nothing new - the pickup is this too |

So two of the four already have their piece, and the other two are one property each. The work is
not the abilities; it is **the catalog, the hand and the balance** - six special pieces becoming
eight, and `bOneOfEach` dealing from a bigger special pool. That is a design pass, not a port.

**What would be lost, and it is worth weighing.** The effect layer guarantees *both players get the
same thing*, which is what makes it fair without anybody spending a piece on it, and it changes a
round without using up a slot in the hand. Abilities as pieces means the player who draws the
double-jump pad decides whether anybody gets one - which is more interesting and less even.

If this goes ahead, `bRoundEffects` off is already the switch that turns the layer off, so the two
can be played against each other without deleting anything.

*(That is what happened: the switch is off, nothing was deleted.)*
