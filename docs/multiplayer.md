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

## Playing with friends on a LAN (2026-11-02)

Step 1 of the plan for playing with other people: **a packaged build that two machines can connect
with over direct IP, and no new gameplay code at all.** There is no menu, no host/join screen and
no lobby - the URL is passed on the command line, which is what the two `.bat` files in `Tools/`
exist to hide. A front end is step 2 and is deliberately not in here.

Nothing in this project had ever been packaged before this, so most of the work was config.

### Development, not Shipping, and why it matters

**Package Development builds.** `Config/DefaultGame.ini` now pins `BuildConfiguration=PPBC_Development`
on purpose, not out of laziness:

- The **console survives**, and with no menu yet the console is how you join a game: a friend whose
  `.bat` file fails can still open it and type `open 192.168.1.42` by hand. That is the escape hatch
  for everything in here, and it is the reason this stays on Development until there is a front end.
- A **C4 detonation is still invisible in Shipping**: `AGetawayC4::PlayBlastEffects` falls back to
  `DrawDebugSphere` when `BP_C4` has no Niagara `ExplosionEffect` assigned, and debug draw is
  compiled out. Filling that slot is the last of *Next steps* 3.

**Two bigger reasons used to be here and were fixed on 2026-11-02**, the day after this section was
written: the placement ghost was `DrawDebugBox` and friends, and four gameplay messages went through
`GEngine->AddOnScreenDebugMessage`, whose body is `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`. A
Shipping package then meant a placement camera you aimed with blind and a game that had quietly
stopped telling you when you picked up C4 or died. The ghost is now real geometry and the messages
are on `AGetawayHUD::ShowMessage` - see *The ghost is real geometry* in `docs/placement.md`. A
Shipping build was compiled on 2026-11-02 to prove the guards hold.

### What was changed

| Where | What |
|---|---|
| `Config/DefaultGame.ini` | a `ProjectPackagingSettings` block: Development, target `Getaway`, `TestCourse` as the map to cook, `Maps` / `Blueprints` / `Materials` always cooked, `__ExternalActors__` never cooked, editor content skipped |
| `Getaway.uproject` | `PythonScriptPlugin` and `EditorScriptingUtilities` given `"TargetAllowList": [ "Editor" ]` |
| `Tools/Host-Adrenaline.bat`, `Tools/Join-Adrenaline.bat` | double-click launchers, copied into the packaged build |

Three of those are worth a sentence each:

- **`Maps`, `Blueprints` and `Materials` are cooked outright**, rather than trusting the map's
  references. The piece Blueprints in `Content/Blueprints/Pieces/` and the Adrenaline materials are
  reached through the game mode's catalog and through soft references, which the cooker does not
  reliably follow. Cheap insurance against a piece that looks right in the editor and spawns as a
  plain cube in the build. `Content/ThirdParty` is deliberately **not** on the list: a pack's demo
  maps and unused meshes should come in only where a piece actually references them.
- **`Content/__ExternalActors__` is explicitly never cooked.** It holds 51 MB of orphaned
  one-file-per-actor packages belonging to a map called `NewMap` that no longer exists -
  `TestCourse.umap` is the only map in the project, and it does not use external actors. Nothing
  references them, so the folder is junk and can be deleted outright. It is named in the ini
  because the first version of this config cooked all of `/Game` and would have dragged the lot
  into the build.
- **`PythonScriptPlugin` was being packaged.** It has a module of type `Runtime`
  (`PythonScriptPluginPreload`), so without an allow list it goes into the build along with a Python
  distribution, for a game that uses Python only as an editor tool. `EditorScriptingUtilities` is
  editor-only by nature and the allow list just makes that explicit.

The runtime plugin itself needed nothing: `GetawayMovement` has no editor includes anywhere
(checked for `GEditor`, `Editor.h`, `UnrealEd`, `Kismet2`), and both build targets - `Getaway` for
the game and `GetawayEditor` - already existed.

### Packaging it, click by click

1. Open `Getaway.uproject`.
2. Top toolbar, **Platforms** (the icon left of *Settings*) - **Windows** - check that
   **Development** is ticked under *Build Configuration*, and that *Build Target* says **Getaway**.
   Both now come from `DefaultGame.ini`, so they should already be right.
3. **Platforms** - **Windows** - **Package Project**.
4. Pick an output folder outside the project - `C:\Users\natha\Desktop\AdrenalineBuild` is fine.
   Never inside the project, or the next cook tries to cook the last one.
5. It takes a while the first time (shaders). Watch the *Output Log* for `BUILD SUCCESSFUL`.
6. The result is `<output>\Windows\Getaway.exe` plus a `Getaway` folder beside it. **The exe is
   still called `Getaway.exe`** - the display name is Adrenaline but the binary keeps the module
   name, as everything else does (see *Naming* in CLAUDE.md).
7. Copy both files from `Tools/` into the folder that holds `Getaway.exe`.

To share it, zip the whole `Windows` folder. It is a few hundred MB and everyone needs the same
build - a client and a server on different builds will refuse each other.

### Hosting and joining

The host runs **Host-Adrenaline.bat**, reads the address it prints to everyone else, and the
others run **Join-Adrenaline.bat** and type it in. Both ask for a name, which is passed as
`?Name=` and is what the scoreboard draws (`AGetawayPlayerState::GetPlayerName`, truncated to 20
characters by the engine in `AGameModeBase::InitNewPlayer`). Without it you are "Player 0".

By hand, the same thing is:

    Getaway.exe "/Game/Maps/TestCourse?listen?Name=Nathan" -windowed -resx=1600 -resy=900
    Getaway.exe "192.168.1.42?Name=Sam" -windowed -resx=1600 -resy=900

`-windowed` is there so you can alt-tab, which you will want while sorting out addresses.

- **The host's Windows Firewall will ask.** Allow it, and tick **Private networks**. A joiner who
  hangs on a black screen and then fails is almost always this.
- **Same house**: the `192.168.x.x` or `10.x.x.x` address the host script prints.
- **Different cities**: either forward **UDP 7777** on the host's router, or - much easier - put
  everyone on a mesh VPN (Tailscale, ZeroTier, Radmin) and use the address it gives you. The game
  cannot do NAT punch-through on its own; that arrives with SteamSockets, which is step 4.
- **Anyone can join mid-session.** `HandleStartingNewPlayer` deals a late joiner their own hand and
  their own fly camera, and `Logout` releases a round that was waiting on someone who quit. That is
  why there is no lobby here: the round loop already copes.

### Still unverified

**None of this has been packaged or played.** The config is right by inspection, the game target
compiles, and every setting name was checked against
`Engine/Source/Developer/DeveloperToolSettings/Classes/Settings/ProjectPackagingSettings.h`. What
nobody has done is press *Package Project* and then connect two machines. Specifically open:

- **Whether the cook is clean.** A missing material or a piece that spawns as a bare cube is the
  likely first fault, and the *Output Log* names it.
- **Whether the ghost draws in a packaged Development build.** It should - debug draw is compiled in
  outside Shipping - but it has only ever been seen in the editor.
- **Whether two separate machines connect**, as opposed to two PIE windows. Everything up to now,
  including the two-player sign-off on 2026-10-17, was one process, or three on the loopback.
- **How it feels over a real connection.** Every play-test so far has had zero latency. The knobs
  are `NetSendRate`, `ProxyInterpSpeed`, `ProxyExtrapolationLimit` and `ProxySnapDistance` on the
  character.
- **The `.bat` files have not been run.** They read the IPv4 address out of `ipconfig` by matching
  the English string "IPv4 Address", which is fine here and would need changing on a non-English
  Windows.

### Rough edges you will meet, and which step fixes them

Known and left alone on purpose, because step 1 was meant to be config only:

- **No way to leave.** Alt+F4 is it. Escape does nothing. Step 2 (the menu) adds *Leave*.
- **No feedback while connecting.** A failed join is a black screen and then the window closing,
  with no reason given. Step 3 hooks `NetworkFailure` / `TravelFailure`.
- **Everyone spawns at one `PlayerStart`** and the engine shoves them apart. Drop a second one
  beside the first - *Place Actors* panel, search `PlayerStart`, drag it into the viewport next to
  the existing one, same Z, about a metre to the side. `FindPlayerStart` picks an unoccupied one per
  player, so nothing in code has to change. Keep them level with each other or you have changed the
  race.
- **`R` to restart is single-player only** and says so on screen. The session runs rounds forever;
  step 3 adds an end to it.
- **A client could place a piece anywhere in the level.** The server checks that you own the piece
  and that it fits, but not where you placed it from. Fine among friends, not fine on Steam - see
  *Deliberately not done* above.
