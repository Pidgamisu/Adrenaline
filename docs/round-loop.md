# Round loop, scoring and sync fixes

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

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

## Crossing the line now says so, and takes your body away (2026-11-04)

Reported from the first two-machine game: **"after someone reaches the end, they can still fall and
they get no prompt that they made it."** Three separate faults, and the reason none of them had ever
been seen is that **all three need a second player.** Solo, the round ends on the same frame you
cross the line, so the round-over banner covered the gap and there was no gap left to fall into.

### 1. The prompt was tied to the round, not to you

`AGetawayHUD::DrawRoundBanner` opened with `if (!GS->IsRoundOver()) return;`. "FINISHED" and your
time lived there and nowhere else, so the first player home got **nothing at all** until the last
one was in or the grace clock ran out - up to `FinishGraceTime` (10 s) of silence after winning.

`DrawFinishedBanner` now draws for the window in between: phase `Run`, round not over, your own
`PlayerState` reading `Finished`. Big **YOU MADE IT** with your time and place while you still have
your body, one line under the run clock once the camera has lifted you out of it.

It reads `HasFinished`, `GetFinishTime` and `GetFinishPlace` off the replicated `PlayerState`, so it
draws on a client with no game mode to ask - the same bargain the scoreboard already made.

**`DrawRunClock` and `DrawRoundBanner` moved up with it**, above the placement-pawn branch in
`DrawHUD`. That branch `return`s, so anybody in a fly camera during a run - a finisher, or someone
who gave up - was being shown no clock, no result and no time. Now all three belong to the round
rather than to whatever pawn you are holding.

### 2. `FellOutOfWorld` was missing a guard its sibling had

`AGetawayCharacter::CheckFallDeath` has always refused to kill a player who is not running, and the
comment there says why: *"a character that is still there during the finish banner should not be
told it has been killed."* **The engine's own KillZ route, `FellOutOfWorld`, had no such check** and
called `Die()` outright.

So: cross the line at speed, carry on over the edge, and you were told **"Killed. Back to the
start"** after you had already won. Then nothing brought you back, because `PlayerDied` and
`RespawnRunner` both refuse a player who is not `Running` - you hung there as a dead frozen pawn
until the round ended. The same guard is now on both routes.

### 3. Nothing happened to you when you finished

The actual hole. You kept full control of a character in a level you were done with, and the only
thing left to do with it was fall off.

**Asked and settled 2026-11-04: a finisher watches the rest from the fly camera.** After
`FinishWatchDelay` (2 s) holding the banner where you landed, `ReachedFinish` calls
`StartPlacementFor` - the same place giving up already puts you, and the same place the next
placement phase will find you. `bWatchAfterFinish` off restores the old keep-your-body behaviour.

Two things that had to be got right:

- **The watch spot is read at the moment of crossing, not when the timer fires.** You arrive at
  speed and may well be over the edge and falling two seconds later, and `StartPlacementFor` opens
  its fly camera 300 up from wherever the body is - which would have been thousands of units under
  the course. The finisher is teleported back to the remembered spot a line before the swap, which
  is never seen, because the same call destroys that body.
- **It cannot hold the round open.** `StartPlacementFor` only swaps the pawn; it does not touch the
  run state, so `Finished` survives and `CheckRoundOver` - which counts `IsRunning` and
  `IsBuilding`, not pawns - still calls the round. The placement pawn also refuses to place outside
  the `Placement` phase, so a finisher can fly and watch but cannot build early.

The timer bails if the phase changed, the round ended, or the player is no longer `Finished`, so
the last player home (whose arrival ends the round outright) is left to `StartPlacement` as before.

### Still unverified

- **None of this has been played**, and that is still true after the 2026-11-07 play-test, where it
  was on the list and came back *"untested"*. Both targets compile clean; nothing has been seen on
  screen. It is now the **largest single unplayed thing in the game**, and the only one left that
  can change what happens at the end of a round: everything else on that list was either signed off
  or given a verdict. Worth doing deliberately rather than hoping to stumble into it - finishing
  requires actually crossing the line, which is easy to avoid for weeks while testing pieces.
- **The compact line's position** (centred, `y = 92`, under the run clock) has not been looked at
  against the placement HUD drawn behind it.
- **Two players at once**: that the second finisher gets the banner too, with `2nd` on it, and that
  the round is still called properly when both are watching.
- **A finisher who falls** past KillZ in the two seconds before the camera lifts: the guard stops
  the death, and the teleport should put the camera at the finish, but the fall itself has not been
  watched.
