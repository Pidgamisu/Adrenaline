# Round effects

> Built and **played 2026-11-03 - all three work.** Read this before touching anything that hands
> a player an ability for one round only. The one part not yet played is how the banner now
> presents itself: see *The banner* below.

Every other round, both players are given the same effect before anybody builds. One at a time, the
same for everybody, and gone the moment the round is.

| Effect | What you get |
|---|---|
| **Double jump** | One jump in the air. The tuned default is `AirJumps` 0, so this is the only way to get one |
| **Loaded** | You start the run carrying one C4, and can hold only the one |
| **Low gravity** | Gravity at 60% - higher jumps, slower falls, and a C4 launch that throws you much further |

Round 1 is plain, round 2 has an effect, round 3 is plain, round 4 has one. `EffectEveryNthRound`
is the rule: an effect lands on every round the number divides, so 2 gives 2, 4, 6. Round 1 is
plain because an effect is a *change* from the way the round before played, and the first round has
nothing to be a change from.

## Where each part lives

The split follows the round loop's: the game mode decides, the game state carries it, and the
character applies it on the machine that owns it.

| Where | What it does |
|---|---|
| `EGetawayEffect` in `GetawayGameState.h` | The list. `None`, `DoubleJump`, `Loaded`, `LowGravity` |
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

## The three, in detail

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

## Still unverified

- **The banner's two seconds and its travel have not been played** in their build-phase home,
  only in the run-start version they replaced.
- **The rhythm and the numbers are a first pass**: whether every other round is right, and whether
  60% gravity is the right amount, are both open.
- **Two machines have never seen it**, like everything else in the project. `ActiveEffect` is one
  replicated enum set 30 seconds before any character spawns, so a client should always have it in
  hand by the time it matters - but that is reasoning, not a play-test.
- **The pool is drawn at random**, so it can take a few rounds to see all three. `bNoRepeatEffect`
  means you will at least never get the same one twice in a row.
