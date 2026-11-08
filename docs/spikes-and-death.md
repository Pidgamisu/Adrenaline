# Spikes, surface mounting and death

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

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
- **They attach to the component, not to the host actor** (2026-11-03). `AttachToHost` takes a
  `UPrimitiveComponent*`, and `AGetawayPlacementPawn::FindHostAt` keeps `Hit.GetComponent()` rather
  than throwing it away for `Hit.GetActor()`. Attaching to an actor means attaching to its *root*,
  which was invisible as a limitation while every host either moved as one piece or did not move at
  all. The windmill broke that: its plates orbit inside a hub that never moves, so spikes stuck to a
  plate hung in the air while the plate went round under them. `Host` is still the actor, because
  the breakable events are on the actor. `AGetawayC4` already stuck to the component it hit, so this
  only brings the placed pieces in line with the charge. `FRepAttachment::AttachComponent` carries
  the exact component, so a client attaches to the same plate the server did.
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

> **Superseded 2026-10-31: death puts you back at the start instead.** See *Dying puts you back at
> the start* at the end of this file. The old behaviour below is still there behind
> `bRespawnOnDeath = false`, and everything about *who* decides a death still holds.

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
on without you. Of what was added after that play-test, **the attachment to a host was played
2026-11-07 and works** - spikes stuck to a windmill plate ride round with it, which is the hardest
case the attachment has, since the host both moves and rotates. See *Spikes and pads ride a plate*
in `docs/placement.md`. Still not played: the **overhang trim** and the rebuilt point layout that
goes with it.

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
along a breakable panel above `SmashSpeed` (900 at the time, 800 now) smashed it out from under you
mid-run. **Changed the
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

#### What breaks a breakable, as of 2026-11-06

Two changes landed together, and the pair is the point - see *Reversed: breaking beats climbing* in
`docs/placement.md` for the full note.

- **One speed, and it is out of reach on foot.** `SmashSpeed` is **1100**, the only test there is.
  Sliding used to get a second threshold of its own at `SlideMinSpeed * 0.9` - **450** - so almost
  any slide burst almost anything it brushed. That branch was dropped rather than retuned.
- **Breaking beats climbing.** `bMantleOverSmash` defaults to **false**, so a breakable you could
  have mantled is burst if you were going fast enough to burst it. The speed test at the top of
  `SmashAhead` is the whole decision and the two rules cannot overlap: below it the function returns
  before the break loop, so slower approaches climb exactly as they always did.

The arithmetic is the whole decision, and it is worth writing down because the number alone does not
show it. **Everything you can do on your own feet is below the line:**

| How you arrive | Speed | Breaks at 1100? |
|---|---|---|
| Walking, crouch-walking | 272 or less | no |
| Slide entered at the minimum (`SlideMinSpeed` 500 + `SlideBoost` 250) | 750 | no |
| Full sprint (`RunSpeed`) | 800 | no |
| Wallrun (capped at `RunSpeed * 1.15`) | 920 | no |
| Slide out of a full run (800 + 250) | 1050 | **no, and only just** |
| Through a speed ring (1.5 x `RunSpeed`) | up to 1200 | **yes** |
| Off a jump pad (`LaunchSpeed`) | 1400 | **yes** |
| Slide down a ramp | gravity carries you past it | **yes** |
| C4 | - | yes, always |

Since `bAirStrafeGain` went off there is no strafing past any of the first five either, so the list
is exhaustive: **breaking is granted by the level, not by running hard.** That is the point - you
have to be going exceptionally fast, and only a placed piece makes you that fast. The cost is that
"breaking things is done by hitting them at speed (slide or jump into them)", a rule that stood from
the prototype, **is dead on flat ground**: sliding into a panel on the level now does nothing.

Since this is also the climb-or-break line, a breakable on foot is **always a step**, and
`bMantleOverSmash` only decides anything when you arrive by piece. Note too that the second slide
started inside `SlideBoostCooldown` (0.5 s) gets no entry boost and runs 250 slower - the one case
where two apparently identical slides behave differently.

`BP_Character` also pins `SmashSpeed` to 1100, which is now the same as the C++ default, so that
override is redundant and can be cleared whenever. See *What the Blueprints actually play at* in the
plugin README - and read the trap there about running `export_tuning.py` from a path with spaces.

The `WallKick` path above tests the same `SmashSpeed`, so dropping the slide branch changed nothing
there: you are wallrunning, not sliding, so it could never have applied.

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

## Dying puts you back at the start (2026-10-31)

Asked for: *"can we make it so that if you die you just go back to the start."*

So `AGetawayGameMode::bRespawnOnDeath` (on, under *Getaway|Round*). Death no longer ends your run:
after the same `DeathLingerTime` beat you get a **fresh character at the PlayerStart** and carry on,
as many times as the run clock allows. Nothing else about dying changed - the victim's own machine
still decides, the server still decides what it means, and `RegisterTrapKill` still credits whoever
placed the thing (and still refuses to pay you for your own trap).

- **Your run state never changes.** With respawning on, `PlayerDied` does not touch
  `EGetawayRunState`: you were `Running` and you stay `Running`, which is what keeps the round open
  for you and makes the `CheckRoundOver` call at the end of `PlayerDied` a no-op. That is why the
  change is small - the round loop already knew how to wait for a slow runner.
- **`EGetawayRunState::Dead` is not dead code.** It is still what `bRespawnOnDeath = false` sets,
  and the HUD still draws "dead" for it. Turning the knob off restores the Ultimate Chicken Horse
  behaviour exactly.
- **`AGetawayGameMode::RespawnRunner` is the new half**, and it is `StartRunFor`'s spawn block with
  the placement-pawn handling taken out: find the PlayerStart, unpossess and destroy the corpse,
  spawn the `DefaultPawnClass` there, possess it, point the control rotation down the start's yaw.
  It refuses to run outside `EGetawayPhase::Run` or for a player who is not `IsRunning()`, so a
  death that lands on the same frame as the round ending cannot put a character back on a course
  nobody is running.
- **A fresh character, not a teleport.** `Die()` has already stopped the old one's movement and set
  its `bDead`, so reviving it means undoing both by hand; a new pawn is the path the run itself
  starts down and there is no half-reset state to get wrong. The price is that **your C4 goes with
  it** - a respawn hands you `StartingCharges` (0), so dying costs you any charges you had picked
  up. **Asked and confirmed 2026-10-31**, so it is settled rather than assumed: dying should cost
  something now that it does not cost the round. Since 2026-11-05 it also *gives* something back:
  the pickups themselves come back when you die (`bReturnsOnDeath`), so what a death costs you is
  the item in your hand and the walk from the start, not the pad you took it from. See *A pickup
  comes back when you die* in `docs/placement.md`.
- **Which route a death takes is read when you die**, not when the timer fires, and captured into
  the lambda. Flipping the knob mid-run can therefore change what the *next* death does but cannot
  land a player halfway between a respawn and the fly camera.
- **The on-screen message now has to know the knob**, and a client has no game mode to ask.
  `AGetawayCharacter::WillRespawnOnDeath()` reads it off `AGameStateBase::GameModeClass` - which
  *is* replicated - via `GetDefaultObject<AGetawayGameMode>()`, which is sound because the knob is
  `EditDefaultsOnly`, so the class default is the live value. It falls back to "yes" if the game
  state has not arrived or the game mode is somebody else's class.
- **What stops a round now is the clock, not the deaths.** `RunTimeLimit` (120 s) and the
  `FinishGraceTime` (10 s) hurry-up after the first player home are the only things that end a run
  you refuse to leave. Unlimited respawns make those two knobs load-bearing in a way they were not
  before: a player who dies over and over holds the round open until one of them fires.

Built clean, and **played on 2026-10-31: dying puts you back at the start and the run carries on.**
