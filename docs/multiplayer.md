# Multiplayer

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

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
