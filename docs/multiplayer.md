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
   naming what they are doing - GROUND, AIR, SLIDE, WALLRUN, MANTLE - and their speed in cm/s
   (u/s until 2026-11-06, changed to agree with the speedometer and the editor).
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

- The **console survives**, which was how you joined a game before there was a menu: a friend whose
  `.bat` file fails can still open it and type `open 192.168.1.42` by hand. The front end landed on
  2026-11-02 so this is no longer the reason to stay on Development, but it is still a useful
  escape hatch to hand a playtester. See *The front end* at the end.
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
others run **Join-Adrenaline.bat** and type it in. Both ask for a name and pass it as `?Name=`.

**`?Name=` on its own never worked**, and this section claimed it did until 2026-11-02 - the
launchers had never been run, so nobody had looked. The client rebuilds its own login URL with the
online subsystem's nickname, so a game started with `?Name=HostNathan` logs in as
`?Name=RPidgey-9201E4034500...` and the scoreboard shows that instead. It is now fixed in
`AGetawayPlayerController::ApplyChosenName`, which asks again with `ServerChangeName` once it is
in the game - and that reads `?Name=` off the game mode's options, so the launchers work too. See
*Three more bugs, found by playing* at the end.

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

Written before anything had been packaged. **It was packaged on 2026-11-02** - see *It is
packaged* at the end for what that proved and what it did not. The list below is as it stood;
the first two are now answered.

- ~~**Whether the cook is clean.** A missing material or a piece that spawns as a bare cube is the
  likely first fault, and the *Output Log* names it.~~ **Clean.** Every `MI_VB_` material and
  every piece Blueprint is in the package, and no ThirdParty or editor content came with them.
- ~~**Whether the ghost draws in a packaged Development build.**~~ Its material is in the package
  and loads without complaint, so it will. **Nobody has looked at it on screen**, though: the
  packaged runs so far were all `-nullrhi`.
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

- ~~**No way to leave.** Alt+F4 is it. Escape does nothing.~~ **Fixed 2026-11-02** by the front
  end: Escape brings up *Resume* / *Leave to menu* / *Quit*. See *The front end* below.
- ~~**No feedback while connecting.** A failed join is a black screen and then the window closing,
  with no reason given.~~ **Fixed 2026-11-02**: `UGetawayMenuSubsystem` hooks `NetworkFailure` and
  `TravelFailure` and the menu says what went wrong. See *The front end* below.
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

## The front end (2026-11-02)

Step 2 of playing with other people: **a menu you launch into, and a way back out of a game**.
This is what a friend needs instead of a `.bat` file, and what a Shipping build needs instead of
the console.

ADRENALINE over the course turning slowly, with a name box, **Host**, an IP box, **Join** and
**Quit**. In a game, **Escape** brings up the same panel with **Resume**, **Leave to menu** and
**Quit**.

### There is no menu map

The menu runs in `/Game/Maps/TestCourse`, the map you play in, with `AGetawayMenuCamera` possessed
instead of a player. The background is therefore the real course rather than a copy of it that
would drift out of date, and nothing new has to be cooked. Decided 2026-11-02, over duplicating
the map into a `MainMenu.umap`.

It is a third `EGetawayPhase`, `Menu`, **deliberately last in the enum** - the values are
replicated and also stored in Blueprint defaults, so `Placement` has to stay 0 and `Run` 1.

### What tells a menu session from a game session

`AGetawayGameMode::ShouldShowMenu()`: `bStartInMenu`, **and** the net mode is `NM_Standalone`,
**and** the URL this world was opened with carries no `Play` option.

| How it starts | What happens |
|---|---|
| Double-click `Getaway.exe`, or **Play** in the editor | no options, standalone, so the **menu** |
| **Host**, which travels to `TestCourse?listen?Play=1?Name=...` | listen server, so **placement** |
| **Join**, or `Join-Adrenaline.bat` | a client, and a client has no game mode: it takes the server's phase |
| `Host-Adrenaline.bat` (`?listen`) | listen server, so **placement**, exactly as before |
| PIE *Play As Listen Server* (the two-player test) | listen server, so **placement**, exactly as before |

So **both launchers in `Tools/` and the two-player test above are untouched by this**, whatever
`bStartInMenu` says. And `bStartInMenu` on `BP_GetawayGameMode` turns the menu off for fast
iteration when the menu is not what you are working on.

`Play=1` is belt and braces next to `listen`: it covers the frame or two during a host's travel
where the net mode is still standalone.

Two things checked in the UE 5.8 source rather than assumed:
`UGameplayStatics::OpenLevel` appends its options after a `?` (`GameplayStatics.cpp`), and `FURL`
inherits a base URL's options **only** for `TRAVEL_Relative` and `TRAVEL_Partial`
(`URL.cpp:159-175`) - which is why absolute travel back to a bare `/Game/Maps/TestCourse` really
does arrive with no `Play` option, and lands in the menu.

### What was added

| Where | What |
|---|---|
| `AGetawayMenuCamera` | the orbiting background camera; measures the level and flies its own circle |
| `UGetawayMenuWidget` | the panel, built entirely in C++ - no Widget Blueprint |
| `AGetawayPlayerController` | Escape, and the only code in the plugin that travels |
| `UGetawayMenuSubsystem` | why the last join failed, and what you typed, across a level change |
| `AGetawayGameMode` | `bStartInMenu`, `MenuPawnClass`, `ShouldShowMenu`, `StartMenu` / `StartMenuFor`, and `PlayerControllerClass` |
| `AGetawayHUD` | draws nothing at all in the `Menu` phase |
| `GetawayMovement.Build.cs` | `UMG`, `Slate`, `SlateCore` |

#### The panel is C++ and there is no `.uasset`

`UUserWidget::Initialize` creates a `WidgetTree` even for a native class, so a widget with no
Widget Blueprint behind it works - but the tree has to be in place before
`UUserWidget::RebuildWidget` reads `WidgetTree->RootWidget`. That rules out `NativeConstruct`,
which runs after. It is built in an override of `RebuildWidget` itself, which also means the
caller can set `Mode` between `CreateWidget` and `AddToViewport`; the two modes build different
trees.

Buttons are a `UBorder` filled pink with 2 px of padding and the button on top, which gives the
edge band of `docs/look.md` with no texture and no font asset. The status line is the one
deliberate break from the palette: a failed join is amber, because it has to read as a problem
rather than as architecture.

#### Escape is a plain key binding, not an input action

Both pawns call `Subsystem->ClearAllMappings()` when they are possessed, so an Enhanced Input
mapping context registered on the controller would be wiped the moment the player got a pawn. The
controller's own `InputComponent->BindKey` sits below the pawn on the input stack and survives
every possession.

**Escape does not work in Play-in-Editor, and no code can fix that.** The editor binds Escape to
*Stop* (`DebuggerCommands.cpp:358`) and `UGameViewportClient::InputKey` consults the editor's
bindings *before* the player controller (`GameViewportClient.cpp:776`). Test it with
**Play > Standalone Game**, or in a packaged build.

#### Nothing pauses

`SetPause` is server-only and meaningless in a multiplayer race, so the Escape panel goes up and
the round carries on underneath it. One behaviour in single player and multiplayer both.

#### Two bugs the headless run caught

Both found by booting the game with `UnrealEditor-Cmd.exe ... -game -nullrhi` and reading the log,
which is worth doing again for anything in here:

- **The menu camera framed TestCourse at a radius of 15.5 km.**
  `AGameplayDebuggerCategoryReplicator` carries a rendering component whose bounds are a 10 km cube
  centred on the origin so that debug draw is never culled, and it exists in every non-Shipping
  build. `IgnoreActorsBiggerThan` (50 m) leaves it, and anything else of that family - a sky
  sphere, an unbound post-process volume - out of the measurement.
- **`FInputModeUIOnly` refused to focus the panel**: "Attempting to focus Non-Focusable widget".
  A `UUserWidget` takes `bIsFocusable` at construction and ignores it after, so
  `SetIsFocusable(true)` has to happen before `AddToViewport`.

And one judgement call that is not a bug: the camera's height started as a fraction of how tall
the level is, which put it 8 m up looking at a 97 m ribbon edge-on from ground level. A race
course is long and flat, so `HeightFraction` is a fraction of the **orbit radius** instead, which
gives the same gentle look down whatever shape the level is.

#### And the one that mattered, found by playing: no input after Host

**Pressing Host got you into placement unable to look or press anything.** This is the trap to
remember if anything else here regresses, because it is invisible to inspection and it survives
a level change:

`FInputModeUIOnly::ApplyInputMode` calls `SetIgnoreInput(true)` and
`SetMouseCaptureMode(NoCapture)` on the **`UGameViewportClient`** (`PlayerController.cpp`), and the
viewport client belongs to the **game instance, not to the world**. So putting the menu up marks
the viewport "ignore all input", travel tears the world down and builds a new one, and the flag is
still set on the other side. Nothing in the engine puts it back: `bShowMouseCursor` lives on the
player controller and does reset, which is why the cursor vanished and the game still took nothing.

The fix is in `AGetawayPlayerController::BeginPlay`: a fresh controller asserts
`FInputModeGameOnly` **unless a panel is already up**. That covers Host, Join, Leave and a failed
join in one place, rather than every call site remembering to undo it.

The "unless" is load-bearing, not defensive. In a menu session `ShowMainMenu` really does run
first - `HandleStartingNewPlayer` fires at login, before the world's `BeginPlay` - so without the
guard the controller would immediately take the mouse away from the menu it had just opened. Both
orders were watched with a log on `UGameViewportClient::IgnoreInput()`:

    INPUTPROBE arrived: ignoring input = TRUE, menu open = TRUE     <- the menu, left alone
    Browse: /Game/Maps/TestCourse?listen?Play=1
    INPUTPROBE arrived: ignoring input = TRUE, menu open = false    <- the bug, after Host
    INPUTPROBE after fix: ignoring input = false

That was produced without a mouse, by starting the real game at the menu and handing it
`-ExecCmds="open /Game/Maps/TestCourse?listen?Play=1"` - which is the exact URL the Host button
travels to, and the deferred command runs after the menu has gone up. Worth reusing: it reproduces
the whole Host path in one command line.

    UnrealEditor.exe "<project>.uproject" /Game/Maps/TestCourse -game -windowed -resx=640 -resy=360
      -nosound -nosplash -ExecCmds="open /Game/Maps/TestCourse?listen?Play=1" -ABSLOG=<file>

### What was verified, and how

By running the real game headless, three times, with no errors in any log:

- **no options** gave the menu, the camera possessed, and the level framed at
  `V(X=4450, Y=-9, Z=425)`, radius 7518, height 2857 - about 20 degrees above the horizontal.
- **`?Play=1`** gave placement, the round dealt and run exactly as before.
- **`?listen`** gave placement, so the launchers and the two-player test are provably unaffected.

And by running the real game in a window and travelling the Host URL from the menu, which is the
whole of the Host path: menu, framed, `Browse: /Game/Maps/TestCourse?listen?Play=1`, placement,
run - with the viewport handing input back on arrival, and no errors or Slate warnings in the log.

**Host itself was played on 2026-11-02** and got into a game.

Both build targets - `GetawayEditor` and `Getaway` - compile and link with no errors and no
warnings.

### Still unverified

Most of this list was closed on 2026-11-03 by playing a Shipping package - see *Played and signed
off* in `docs/itch.md`. What is actually left:

- ~~**No two separate machines have connected through the menu.**~~ **Two did, on 2026-11-03,
  from two different states, and it worked.** See *Two machines, two states* at the end of this
  file for what that proves and the details nobody wrote down at the time.
- ~~**The Cancel button has not been clicked**, only the give-up clock it shares a code path with.~~
  **Clicked 2026-11-07 and it works.** The smallest open item in the front end is closed, and with
  the host-leaves path played the same day, **every button in the front end has now been pressed.**
- **Travel from Play-in-Editor into a listen server.** PIE is not the real thing here; a packaged
  build is the test.
- **Whether the auto-framed camera picks a flattering shot.** The menu has been seen now and nobody
  complained, which is not the same as saying it is well composed.

Closed on 2026-11-03, all by playing a packaged Shipping build:

- ~~**How any of it looks.**~~ The panel, the pink frames, the title and the course turning behind
  them have all been seen, outside the editor, and they work.
- ~~**Escape, Resume and Leave to menu have not been played.**~~ All three played, plus Quit and a
  failed join. **Leave was the one to worry about** - it is a travel, and the Host travel is where
  the dead-input bug lived - and it is safe for a reason worth keeping: `LeaveToMenu` calls
  `HideMenu` *before* travelling, and `HideMenu` ends in `ApplyInputMode(false)`, the very call that
  puts the viewport's `SetIgnoreInput` back to false. Host broke because it travelled with a panel
  still up; Leave tears its panel down first, so the flag never crosses.
- ~~**Anything packaged.**~~ Packaged 2026-11-02, switched to Shipping and played 2026-11-03. See
  `docs/itch.md`.

### What the front end does not do

- **A pad cannot drive the menu.** Every obvious button is already spoken for - `Special_Left` is
  back-to-placement on the character, `Special_Right` is done-building in placement - so picking
  one is a decision, and moving between buttons with a stick is its own pass. See
  `docs/controller.md`.
- **No lobby, no player list, no settings.** Host opens the round straight away, which the round
  loop already copes with: `HandleStartingNewPlayer` deals a late joiner their own hand.
- **A host leaving drops everybody.** Absolute travel off a listen server takes the net driver
  down with the world, and the other players' failure handler puts them back at their own menu
  with a message. Fine on a LAN among friends; worth revisiting with Steam.
- **No resolution, volume or sensitivity settings**, and no key rebinding.

### Three more bugs, found by playing (2026-11-02)

"Joining doesn't work. It says connecting then gets stuck." Three separate faults behind it, and
only one of them was the connection.

#### 1. The address was not this machine

The join was aimed at `204.77.151.204`, a public address. This machine's only real address is
`10.10.240.77` on Wi-Fi; the rest are link-local `169.254.x` ones Windows hands to unplugged
adapters. So the packets went out to the internet and nothing answered - correct behaviour, no bug
in the game.

The bug was that **nothing told the host what to tell their friends.** That job used to belong to
`Tools/Host-Adrenaline.bat`, which greps `ipconfig` and prints it. Hosting from the menu skipped
the `.bat` file entirely, and the menu said nothing.

The menu now shows it: *"Your address is 10.10.240.77 - that is what others type to join you"*,
from `UGetawayMenuSubsystem::FindLanAddress`, which reads the adapters through
`ISocketSubsystem::GetLocalAdapterAddresses` and prefers a private LAN address, skipping loopback
and `169.254.x`. That needed `Sockets` adding to `GetawayMovement.Build.cs`.

**On one machine, join `127.0.0.1`.** The give-up message says so.

#### Reading out the right address (2026-11-03)

The fix above was right for a friend in the same house and **wrong for a friend in another city**,
which came up the moment one was asked about.

`FindLanAddress` returned a single address and preferred a private one - `10.x`, `192.168.x`,
`172.16-31.x` - returning the first it found. A mesh VPN is the answer for playing with somebody
far away (see *Hosting and joining* above), and the address Radmin, Tailscale or Hamachi hands you
is **not** in those ranges, so it could only ever land in the `Fallback` slot, which is used only
when nothing private was found. This machine has a `10.10.240.77` home address, so the menu would
have confidently displayed that - useless to anybody outside the house - and the VPN address it
should have shown would never appear at all.

So it now lists them all, rather than choosing:

- `UGetawayMenuSubsystem::FindLanAddress` became **`FindLocalAddresses`**, returning
  `TArray<FGetawayLocalAddress>` - an address plus a short note on what it is for. Loopback and
  `169.254.x` are still skipped, duplicates are dropped, and the order is VPN first, then local,
  then anything else. VPN first because a machine only has one if somebody installed it on purpose,
  and the reason to do that is a friend who is not in the house.
- `UGetawayMenuWidget::SetLocalAddress` became **`SetLocalAddresses`**. One address reads as before;
  several get *"Others type one of these to join you:"* and a line each, capped by
  `MaxAddressLines` (3).
- The log line on the controller now names every address found rather than one.

**The labels are guesses from the address range**, and that is the known soft spot. Radmin is
always `26.x`, Hamachi `25.x`, and Tailscale uses the carrier-grade NAT block `100.64.0.0/10`, so
those three can be named with confidence. **ZeroTier picks its own range** and usually lands in
`10.x`, where it gets called a local address - on this machine, indistinguishable from the real Wi-Fi
one. That is survivable precisely because nothing is chosen for you: a mislabelled address is still
on the list, and the person reading it out knows what they installed. Do not "fix" this by going
back to picking one.

`Tools/Host-Adrenaline.bat` already printed every IPv4 address from `ipconfig` and needed no change
- it was briefly the better tool for this job than the menu that replaced it.

#### 2. There was no way out, and never any feedback

This is the one worth remembering, because it is a trap rather than a slip:

**the engine will not take you back from a failed connection in this project.**
`UEngine::HandleNetworkFailure` ends in `CallHandleDisconnectForFailure`, and that only acts
through `GetGameInstance()->GetOnlineSession()` - which is null here, because *Local first, Steam
later* means there is no online subsystem session yet. So the engine logs the failure and does
nothing else: no travel, no message, and the panel sits on "Connecting..." for ever. The 60 second
`InitialConnectTimeout` would not have helped either, because nothing was listening for it.

Hence a watchdog of our own, `AGetawayPlayerController::TickConnecting`, checked four times a
second while connecting:

- an engine-reported failure is shown the moment it is recorded, so a wrong build or a refusal
  reads correctly rather than as "no answer";
- otherwise `JoinTimeout` (10 s, tunable) gives up, says what to check, and drops the half-open
  connection with `GEngine->CancelPending` so it cannot land you in a game minutes later;
- while connecting, **Host and Join grey out and a CANCEL appears**, and Escape cancels too - the
  panel is never a dead end.

#### 3. Nobody has ever had a name

See *Hosting and joining* above: `?Name=` was silently discarded, for the host as well as for
joiners, and had been since the launchers were written. Fixed by asking again from inside the game
with `ServerChangeName`.

Two smaller things fell out of chasing it, both in `AGetawayPlayerController`: `RememberTyping`
would wipe a known-good name with an empty box when `EndPlay` ran during a travel, so both it and
the address are now only overwritten by a non-empty box; and `ShowMainMenu` returns early if the
panel is already up, since the game mode calls it twice for the first player and it was taking the
failure message off the subsystem twice.

`JoinGame` is also an `Exec` now, so `JoinGame 10.10.240.77 Nathan` works from the console. That is
a better escape hatch than the engine's `open <ip>`, which has no give-up clock on it, and it is
how all of this was tested without a mouse.

#### What two instances on the loopback proved

Run as two windowed processes, host first, joiner 22 seconds later:

    UnrealEditor.exe "<project>.uproject" "/Game/Maps/TestCourse?listen?Play=1?Name=HostNathan" -game -windowed -resx=640 -resy=360
    UnrealEditor.exe "<project>.uproject" /Game/Maps/TestCourse -game -windowed -resx=640 -resy=360 -ExecCmds="JoinGame 127.0.0.1 Sam"

From the host's log, in order: the joiner was accepted, dealt their own hand and their own fly
camera (`GetawayPlayerController_1 is building, with BP_PlacementCamera_C_1`), both swapped to
characters, the round was scored **by name** -

    Getaway:   HostNathan did not finish  +0  (0)
    Getaway:   Sam did not finish  +0  (0)
    Getaway: round 1 over (time)

- and round 2 opened for both players. So joining, late joining, the shared round, scoring and the
names all work together. No errors on either side.

A bad address was checked the same way and gave up on the clock:

    Getaway: joining 204.77.151.204?Name=Nathan
    Getaway: gave up joining 204.77.151.204 - No answer from 204.77.151.204. Check the address, ...

#### Would a lobby have helped?

Asked at the time, and worth writing down: **no, not with this.** A lobby or LAN browser replaces
*typing an address* - it would have stopped the wrong address being typed, which was fault 1 - but
discovery ends by connecting to the game it found, in exactly the same way. Faults 2 and 3 would
have been identical, and a browser that finds a game and then hangs for ever on "Connecting..." is
the same dead end with extra steps.

It is still worth doing one day, and the cheap moment is **with Steam**, where sessions give a real
lobby, a player list and NAT punch-through together (*Local first, Steam later*). Done before then
it means either the NULL subsystem's LAN beacon or a hand-rolled UDP broadcast, it only ever finds
games on the same subnet - no use over Tailscale, which is the answer for playing with somebody in
another city - and the IP box has to stay anyway for exactly that case.

## It is packaged (2026-11-02)

*Next steps* 4, half of it. **The cook has now been run and the packaged game boots and plays.**
What has still not happened is two separate machines connecting to each other.

### How it was packaged, without the editor

The *Package Project* button is one way; this is the same thing from a terminal, which is
repeatable and leaves a log to read:

    "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
      -project="C:\Users\natha\Documents\Unreal Projects\Getaway\Getaway.uproject" ^
      -noP4 -platform=Win64 -target=Getaway -clientconfig=Development ^
      -build -cook -stage -pak -archive ^
      -archivedirectory="C:\Users\natha\Desktop\AdrenalineBuild" -prereqs -nocompileeditor

Two deliberate choices. **No `-map`**, so the cook reads `MapsToCook` and
`DirectoriesToAlwaysCook` out of `DefaultGame.ini` - that config was the thing being tested, and
naming the map on the command line would have tested the command line instead. And **`-prereqs`**,
which stages the VC redist installer that a friend's machine may need and that a zip of the folder
would not otherwise contain.

`Development` and target `Getaway` are not typed in twice: they come from `DefaultGame.ini`, so
this is the same build the editor's button makes.

**Result: `BUILD SUCCESSFUL`, exit 0, 946 MB, 57 files** in
`C:\Users\natha\Desktop\AdrenalineBuild\Windows` - `Getaway.exe`, a 10 MB `Getaway-Windows.pak`
and a 128 MB `Getaway-Windows.ucas` IoStore container, plus the two launchers copied in by hand.

It took 39 seconds, which looks wrong and is not. The cook really did run - `Saved/Cooked/Windows`
was created by that run - but every shader was already in the local Zen/DDC cache from a day of
editor sessions. On a cold cache, or another machine, expect minutes rather than seconds for the
same result.

### What the cook proved

Listed straight out of the containers with `UnrealPak <file> -List`, 2889 entries:

- **All three masters and all fourteen `MI_VB_` instances** are in, `MI_VB_Ghost` and `M_VB_Pane`
  among them - so the placement ghost and the explosion both have their materials. This was the
  headline worry: those are reached through soft references and the game mode's catalog, which the
  cooker does not reliably follow, which is why `DirectoriesToAlwaysCook` names the folders
  outright.
- **All eleven piece Blueprints**, plus `BP_Character`, `BP_GetawayGameMode` and
  `BP_PlacementCamera`. No piece will spawn as a bare cube.
- `TestCourse.umap` and the five `M_Course_` materials.
- **No `Content/ThirdParty`, no `/Game/Python`, no `__ExternalActors__`.** The exclusions hold.
  (`Content/__ExternalActors__` has since been deleted from the project outright, so that one line
  in the ini is now belt and braces.)

### What the packaged game does when you run it

Both runs headless with `-nullrhi -unattended`, reading
`AdrenalineBuild\Windows\Getaway\Saved\Logs\Getaway.log`:

- **No options** gives the front end: `GetawayPlayerController_... is at the menu`, the LAN address
  found (`this machine is 10.10.240.77`), and the level framed at exactly the numbers the editor
  gives - `V(X=4450.00, Y=-9.00, Z=425.00)`, radius 7518, height 2857.
- **`?Play=1?Name=Nathan`** deals a hand, runs the placement phase, swaps to `BP_Character_C` when
  the build clock expires, and renames the player to `Nathan`.
- **No warnings about materials, missing assets or failed loads in either log.**

So the thing `docs/look.md` and this file were both worried about - that a Shipping-style package
would lose the ghost, the messages or the explosion - does not happen, and the front end works
outside the editor.

### Still not done

- ~~**Two machines have never connected.**~~ Closed 2026-11-03: two machines in two different
  states played over the internet. See *Two machines, two states* at the end of this file.
- **Nobody has looked at the packaged build on screen.** Both runs above were `-nullrhi`, so the
  menu, the pink, the ghost and the explosion have been proved to *load* in a package, not to
  *look* right in one.
- **Still `Development`, not `Shipping`.** Nothing known blocks the switch now that the explosion
  is real geometry, but it has not been tried, and Development keeps the console as an escape
  hatch for a friend whose join fails.
- **The `.bat` launchers still have not been run**, and matter less now that the menu exists. They
  also still read the IPv4 address by matching the English string "IPv4 Address" out of
  `ipconfig`.

### Handing it to somebody

Zip the whole `Windows` folder - 946 MB - and make sure you are both on the same build; a client
and a server from different builds refuse each other. Then *Hosting and joining* above applies,
except that nobody needs the `.bat` files any more: the host reads their address off the menu,
and the joiner types it into the menu's box.

## Two machines, two states (2026-11-03)

**It works.** Two separate computers, in two different US states, connected through the menu and
played. This closes *Next steps* 4 and the item that both lists above called the last big unknown
in the project: every previous success had been one process, or two processes on the loopback.

Reported as working **amazing** - so the client-authoritative model, which was only ever defended
as good enough for local play, holds up across the country. That is a better result than this file
expected. Two cautions before taking much comfort from it: it went over a **Tailscale** tailnet, so
the hard part of the internet was never the game's problem (see *What a tailnet does and does not
prove* below), and nothing about it makes cheating any harder - read *Client authority* above, and
the Steam note in `docs/steam.md` still stands.

### What this proves

- The packaged Shipping build hosts and joins between two machines on different networks, hundreds
  of miles apart - not just two processes on one box.
- The menu's address box and the give-up clock are good enough to get two people connected
  without a lobby, a browser or a `.bat` file.
- Movement at speed is playable at inter-state latency. This was the open worry: the whole game is
  Source-style momentum, and client authority means each machine draws the *other* player from
  replicated movement. It did not ruin it.

### How it was actually connected: Tailscale

**Over a Tailscale tailnet, with the address read off the menu. No port forwarding, and no
firewall prompt** (possibly one appeared and was forgotten; more likely the executable was already
allowed from the loopback runs on 2026-11-02 and 11-03, or the rule already covered Tailscale's
adapter).

This is exactly the route *Hosting and joining* above recommends for "different cities", so the
advice is now proved rather than guessed. It also means the menu's address list earned its keep
first time out: Tailscale hands out `100.64.0.0/10`, `NoteForAddress` in
`GetawayMenuSubsystem.cpp` labels that range **"Tailscale - a friend somewhere else"**, and the VPN
bucket is sorted *above* the local `10.x` one, so the address worth reading down a phone was at the
top of the list. That ordering was a guess when it was written. It was the right one.

**Leave the firewall advice in *Hosting and joining* as it stands.** One machine not prompting is
no reason to stop telling the next person to expect it.

### What a tailnet does and does not prove

Tailscale is a WireGuard mesh VPN: every machine on the tailnet gets a stable `100.x` address and
they all behave as if they were on one flat private network, with Tailscale doing the NAT traversal
underneath. **So the game saw the same thing it was built for - a plain private address on a local
network - and the distance was Tailscale's problem, not the game's.**

What that proves, and it is the important half: **the netcode, the round loop and the movement hold
up at real inter-state latency.** That was the actual worry.

What it does **not** prove: that someone who downloads the build from itch.io can host to a friend.
They are not on your tailnet. For them it is still either **UDP 7777 forwarded on the host's
router**, or **both of them on a mesh VPN of their own**. The game still cannot do NAT
punch-through, and nothing about this session changed that - it arrives with SteamSockets, see
`docs/steam.md`. **The itch.io page has to say so**, or the first thing a stranger meets is a join
that times out.

### What was played, and what held

All reported working, with nothing singled out as wrong:

- **Movement looked really smooth.** No snapping or rubber-banding on the other player. This is the
  one that could have sunk the whole design, since client authority means each machine draws the
  other player from replicated movement.
- **Deaths, respawns, breakables, C4 and the score all worked.** Every system the round loop
  replicates, over a real connection, in one sitting.
- **The collapsing bridge gave way in the same place on both screens**, which surprised the player
  and is worth knowing *why*: it is the design in *The bridge* in `docs/round-loop.md` paying off.
  The server alone runs the stand probe and the timers, and `StartFalling` resets the rattle before
  it drops, so both copies begin falling on the same frame from the same place. Only the fall
  itself is local and unreplicated - deliberately, so a rider has the platform under them
  immediately rather than a frame late. That note says the two copies "will not land identically,
  which does not matter". At inter-state latency, nobody could tell.
- ~~**Nobody left mid-game**, so the host-leaves path is still unplayed.~~ **Played 2026-11-07 and
  it works.** So the behaviour described under *What the front end does not do* - a host leaving
  drops everybody, the client's failure handler catches it and puts them back at their own menu
  with a message - is now observed rather than reasoned. That path was the one most likely to hang,
  because it is the net driver going down underneath a live world, and it is the same handler that
  rescues a failed join; the engine not bringing you back from a connection failure by itself is the
  trap recorded at the end of this file, and this is the other half of that fix proving out.
