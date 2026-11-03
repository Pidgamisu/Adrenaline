# Project context for Claude

## The game

**Adrenaline** (working titles were Getaway, then Vaultborne; the code, files and classes still use the Getaway name, see *Naming*).

A 3D, first-person take on **Ultimate Chicken Horse**: each round, players draft a piece from a random box, place it in a shared level, then everyone races to the objective. The twist is **deep, skill-based movement** (Source/Apex/Titanfall style), so pieces should change *how* you can move (wallrun surfaces, slide ramps, breakable panels, C4 pickups, crumbling bridges, bounce pads), not only block the way. Scoring follows Ultimate Chicken Horse: no points if everyone finishes, so players try to make the level hard but still beatable for themselves.

Engine: **Unreal Engine 5.8 (C++)**. Everything lives in the `GetawayMovement` plugin.

## Where things stand (2026-10-27)

- **Movement:** ported from the browser prototype and signed off.
- **Input:** keyboard and mouse, plus a full controller map for both the run and the placement camera (not yet play-tested with a pad in hand). See `docs/controller.md`.
- **Multiplayer:** client-authoritative, two players play-tested 2026-10-17.
- **Playing with friends:** packaged 2026-11-02, and **switched to Shipping 2026-11-03** - 609 MB in
  `C:\Users\natha\Desktop\AdrenalineShipping`, down from 946. The cook is clean: every material and
  piece Blueprint is in, and no ThirdParty or editor content. **Played on screen for the first time
  2026-11-03 and it all works** - the placement ghost, the HUD messages and the C4 explosion all
  survive Shipping, so the thing that blocked a distributable build for weeks is closed. Shipping
  writes **no log**, so a crash in a packaged build leaves nothing to read. **Two machines have
  still never connected.** See *It is packaged* in `docs/multiplayer.md` and `docs/itch.md`.
- **On itch.io:** <https://rpidgey.itch.io/adrenaline>, **a private draft**. The Shipping build is
  uploaded and processed - build #2287901 on the `windows` channel, pushed with
  `Tools/itch/push.ps1`, which patches so later versions upload in seconds. The page itself is
  **empty**: no text, no cover image, no screenshots, no video. The text is all drafted in
  `docs/itch-page.md`. See `docs/itch.md`.
- **Front end:** a menu with ADRENALINE over the course turning slowly, a name box, Host, an IP box
  and Join, and Quit; Escape in a game gives Resume / Leave to menu / Quit. Built 2026-11-02,
  **Host played 2026-11-02. Joining works** - two processes over the loopback played a full round
  and were scored by name. **Escape's Resume and Quit were played 2026-11-03**, in a Shipping build -
  the first time Escape has ever been pressed outside the editor. **Leave to menu and Cancel still
  have not been**, and no two separate machines have connected. A join that fails now gives up after 10 s and says why, and the menu
  shows your own address so others know what to type. **Player names work for the first time**:
  `?Name=` had always been discarded. There is still no lobby - and one would not have fixed any of
  this. See *The front end* in `docs/multiplayer.md`, and the two traps at the end of it: input
  going dead after a travel, and the engine never bringing you back from a failed connection.
- **Round loop:** shared placement phase with a build clock, the run, Ultimate Chicken Horse scoring, and deaths (spikes, falling off the course).
- **Pieces:** wall, floor, ramp, breakable panel, crate, collapsing bridge, C4 pickup, spikes, jump pad, anti-gravity vortex.
- **Art:** an art direction, not assets. Unlit pink on black, each face see-through with a solid
  band round its rim. Three passes 2026-11-02, **played and signed off at speed**. See
  `docs/look.md`. The point of it is that greybox *is* the look, so meshes stop being a blocker -
  which is also why the placement ghost and the C4 explosion are both built geometry rather than
  debug draw or Niagara.

## Where the details live

This file is kept short on purpose. The history and the how-it-works notes are in `docs/`. Read the matching file **before** working in that area, and write new notes there, not here.

| File | Read it before touching |
|---|---|
| `docs/test-course.md` | `/Game/Maps/TestCourse`, its layout and what each obstacle tests |
| `docs/placement.md` | the placement phase, build clock, catalog, piece positioning, directional pieces, the objective |
| `docs/multiplayer.md` | replication, client authority, running the two-player test in the editor, packaging and LAN play with friends, the menu and Escape |
| `docs/round-loop.md` | phases, scoring, shared placement, syncing breakables, bridge, C4 and pickups |
| `docs/spikes-and-death.md` | spikes, surface-mounted pieces, dying, breakables |
| `docs/falling-slopes-cleanup.md` | fall deaths, the "everything touches something" rule, pieces on slopes, between-round cleanup |
| `docs/controller.md` | controller support: the button map, stick tuning, why a stick can't use the mouse look code |
| `docs/blueprint-tuning.md` | `BP_Character`, `BP_GetawayGameMode`, tuning values in the editor, where the game mode is named, giving a piece a material |
| `docs/look.md` | the unlit pink look: materials, the palette and what each colour is allowed to mean, lighting, bloom |
| `docs/itch.md` | the itch.io release: the Shipping build, butler, pushing a new version, the page |
| `docs/itch-page.md` | the text for the itch.io page: tagline, description, tags, controls, pricing |
| `docs/steam.md` | launching on Steam later: the fee, Valve's gates, Steam sessions, client authority |
| `Plugins/GetawayMovement/README.md` | setup, controls and the full tuning table |
| `Reference/getaway-lab.html` | the browser prototype: the reference for how movement should **feel** |

`docs/archive/` holds the old full CLAUDE.md. Don't read it unless asked.

## Working with me

I'm new to Unreal C++ and I don't write code. The ideas and the feel are mine; you write the code.

- **One feature at a time.** Build it, I play it, we commit it. Don't start the next feature in the same pass.
- **For anything big** (a new piece, a new phase, a rename), show me a plan in plain English before writing code.
- **Build after every C++ change** with the command under *Build*, and fix every error and warning before telling me it's done.
- **Finish every change with three things:** what changed, in plain English; exactly what to click and do in the editor to test it; and what you could *not* verify.
- **Explain editor steps click by click.** Name the panel, the menu and the field.
- **Make tuning tweakable.** Put feel and balance values in `UPROPERTY(EditAnywhere)` with a clear `Category`, so I can change them in the Details panel myself.
- **Never edit `.uasset` or `.umap` files directly**, or anything under `Content/ThirdParty/`. For editor-side changes, give me click steps or a Python script I run in the editor.
- **Header changes, new `UPROPERTY`s, `UFUNCTION`s or classes:** tell me to close the editor first, then do a full build. Live Coding isn't enough for those.
- **A new engine module** (UMG, Niagara, etc.) must be added to `GetawayMovement.Build.cs`.
- **When I settle a feel or rules question**, add it to *Decisions already made* below.
- **Commit after I've played a feature and said it's good.** Use a short message saying what the feature is. Never commit without asking.
- **Bug reports:** when I say something broke, read the newest log in `Saved/Logs/` yourself before guessing.
- **Check API names.** If you're not sure a function exists in UE 5.8, find it in the engine source under `C:\Program Files\Epic Games\UE_5.8\Engine\Source` before using it.

## Naming

The game is called **Adrenaline**. The display name is set in `Config/DefaultGame.ini`. The project file, the `Getaway` module, the `GetawayMovement` plugin and every `AGetaway…` / `UGetaway…` class keep the old name. **Don't rename any of them unless I ask.** The `M_VB_…` / `MI_VB_…` material assets keep their prefix too, because the maps store their paths. A full rename of the code needs CoreRedirects in `DefaultEngine.ini` (the maps store class paths), a Git commit first, and its own session.

## Third-party assets

Each pack goes in its own folder, `Content/ThirdParty/<PackName>/`, and is never edited in place: copy what needs changing into our own folders. Every pack gets a row in `CREDITS.md` (name, source, license, link, attribution needed?) the day it's added. Logic stays in C++; art, sounds and meshes get hooked up in Blueprint subclasses.

## Decisions already made (don't undo without asking)

- **Movement feel:** Source-style ground friction and air strafing, lossless bunny hops, auto-hop while holding jump, jump buffering, mouse-wheel jump.
- **Double jump:** 1 air jump. It redirects your momentum toward the keys you're holding, without losing speed. It is not spent by a jump pressed just before landing (that press is kept for a bunny hop).
- **Slide:** crouch at speed. Entry boost on a 1 s cooldown, very low friction, speeds up on slopes.
  **Slides are on a cooldown** (decided 2026-10-31): once a slide ends, for whatever reason - crouch
  released, slowed below `SlideStopSpeed`, jumped out of it, slid off a ledge - you can't start
  another for `SlideCooldown` seconds (1.5 by default). Holding crouch through the cooldown does not
  queue one up; let go and press again. `SlideCooldown = 0` restores the old tap-whenever behaviour.
- **Wallrun:**
  - Only when the wall is **beside** you (within about 40 degrees of straight left or right of your view), never in front.
  - Forgiving attach: 60 cm reach, snaps onto the wall, works at low speed and while falling fast.
  - Space kicks off.
  - In the Unreal version you can't re-grab the same wall until you land.
- **Mantle:** stays in. Keeps 75% of your run-up speed (it doesn't stop you) and does a small camera jiggle.
- **No kick/punch button.** Breaking things is done by hitting them at speed (slide or jump into them) or with C4.
- **Climbing beats breaking** (decided 2026-11-01): a breakable whose top you could mantle is
  climbed, not burst, so a crate is a step up as well as a wall to smash. `bMantleOverSmash` on the
  movement component turns it off. Panels, crate stacks and anything else outside the mantle window
  still break on contact at speed.
- **C4:**
  - Carry up to two charges at a time, G to throw, F to detonate.
  - **C4 is a collectable resource** (decided 2026-10-15, replacing "charges return when they detonate"): a thrown
    charge is spent for good. You spawn with `StartingCharges` (0 by default) and restock by running through an
    `AGetawayC4Pickup`, which grants `ChargesGranted` (**1** since 2026-10-19, was 2, so a full load of
    `MaxCharges` 2 costs two pickups) and respawns after `RespawnDelay` (8 s) so a placed pickup
    stays useful for a whole round. `StartingCharges = MaxCharges` restores the old always-loaded behaviour.
  - **Inherits 100% of your velocity** when thrown.
  - Sticks to surfaces.
  - Rocket-jump style launch.
  - Bursts breakables.
  - Never damages walls or floors.
  - **A pickup is per player** (decided 2026-10-18, after playing): each player can collect the
    same `AGetawayC4Pickup` once per respawn, and each sees their own pad empty. The alternative -
    a shared pickup that the first player there takes from everyone - was considered and turned
    down: per-player is fairer in a race, and it falls straight out of client authority, since the
    charge count lives on the machine that owns the character. Making it shared means routing the
    grant through the server like `AGetawayBreakable::Break` and replicating `ChargesHeld` to its
    owner. Don't switch it without asking.
- **Destruction stays simple:** only panels and crates break, into loose scraps the player can shove around. A Finals-style fracture/collapse system was tried in the prototype and **scrapped**: it was too heavy and didn't feel like The Finals. If big destruction comes back later, use Chaos Destruction (Geometry Collections), not custom code.
- **Camera effects:** wallrun tilt, speed FOV, shake, landing kick and mantle jiggle are wanted, with a toggle to turn them off. Note when porting camera effects: roll signs invert between the lab and Unreal. three.js rolls about a camera Z axis pointing back at the viewer; Unreal rolls about forward X in the aviation sense (positive = right side down). Copying a roll formula across verbatim tilts it the wrong way, which is exactly what happened to the wallrun tilt (fixed 2026-10-15). A camera "shaking" bug was reported in the prototype and was likely frame-timing judder. Movement should look smooth at any refresh rate.

- **Dying puts you back at the start** (decided 2026-10-31, replacing "death ends your run for the round"). A fresh character at the PlayerStart after `DeathLingerTime`, your run state untouched, as many times as the run clock allows. Your C4 charges do not come back with you (confirmed 2026-10-31). The game mode's `bRespawnOnDeath` turns it off for the old Ultimate Chicken Horse behaviour. See `docs/spikes-and-death.md`.
- **Local first, Steam later** (decided 2026-10-26). Build and test for local/LAN play now. Steamworks, online sessions and achievements wait until the game is fun locally.
- **Client-authoritative multiplayer is fine for local play. Revisit it before Steam.** Each machine decides where its own player is, which makes cheating easy with strangers online. That is a deliberate trade-off for now, not something to fix unasked.
- **The wall and breakable panel stay rotated 90° from the lab** (decided 2026-11-02, after
  playing). A wall you place runs *away* from you rather than across your view, which makes it the
  better wallrun surface - and this game is about wallrunning in a way the browser prototype was
  not. Reversing it would be a swap of X and Y on the `wall` and `break` catalog entries. See
  `docs/placement.md`.
- **The unlit pink look is the look** (confirmed 2026-11-02, by playing it at speed). The edge band
  is what carries shape and depth once the lights are off; pass one was flat without it. See
  `docs/look.md` before changing any of it.

## Units and tuning

Unreal uses cm. The prototype showed Source units: **1 u = 2.5 cm** (40 u = 1 m, 320 u/s = 800 cm/s). The tuned defaults are in the plugin README table. Key values:

- RunSpeed 800
- GroundAccelerate 10, GroundFrictionSource 5, StopSpeed 250
- AirAccelerate 12, AirSpeedCap 75
- GravityScale 2.04
- JumpSpeed 750

## Build

Toolchain: Unreal Engine 5.8.3 at `C:\Program Files\Epic Games\UE_5.8`, Visual Studio Community 2022 (17.14) with the *Game development with C++* workload. Build from a terminal with:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GetawayEditor Win64 Development -Project="C:\Users\natha\Documents\Unreal Projects\Getaway\Getaway.uproject" -WaitMutex
```

Or double-click `Getaway.uproject` and let the editor rebuild when it asks.

## Testing

`/Game/Maps/TestCourse` is the startup map, so pressing Play drops you into it. Since the front end landed, **Play opens the menu**; click Host to get into a round. Untick `bStartInMenu` on `BP_GetawayGameMode` to go straight into placement, which is what you want while working on anything that is not the menu. `bStartInPlacement` is on, so that drops you into the placement camera; press Enter to run straight away.

**Escape only works outside Play-in-Editor** - the editor binds it to Stop and gets it first. Use Play ▸ Standalone Game to test the menu and Escape. Two-player testing: see `docs/multiplayer.md`.

## Source control

Git with Git LFS for `.uasset`, `.umap` and media files (rules in `.gitattributes`; build output ignored in `.gitignore`). Commit only with my OK.

## Next steps

1. More traps. Spikes were the first.
2. The party box deals from the whole catalog. A per-round deck is still to do.
3. ~~**A front end**: a menu with Host, Join and an IP box, and Escape to leave in game.~~ **Done
   2026-11-02, and played in a Shipping build 2026-11-03** - Escape's Resume and Quit both work, the
   first time Escape has been pressed outside the editor. See *The front end* in
   `docs/multiplayer.md`. What is left of it: **Leave to menu** and a failed join have not been
   pressed, a pad cannot drive it, and there is no lobby or settings page.
4. **Play with somebody on a second machine.** Packaging is done and the cook is proved
   (2026-11-02); what is left is two real computers, a firewall prompt, and how the movement feels
   over a connection that is not the loopback. See *It is packaged* in `docs/multiplayer.md`.
5. ~~A Niagara explosion effect for `BP_C4`. Without one the blast falls back to a debug sphere,
   which is invisible in a Shipping build.~~ **Done 2026-11-02, and not Niagara** - Niagara cannot
   be scripted at all, so it is `AGetawayBlast`: real additive geometry, a flash and glowing
   shards, in every build. The `ExplosionEffect` slot still overrides it. **Looked at and confirmed
   working in a Shipping build 2026-11-03**, along with the placement ghost and the HUD messages -
   see *The explosion* in `docs/look.md`.
6. **Finish the itch.io page.** The Shipping build is uploaded to a private draft at
   <https://rpidgey.itch.io/adrenaline>, but the page has no cover image, no screenshots and no
   video, and none of the drafted text is in. **A short video of movement at speed is the
   highest-value thing missing** - motion is the whole pitch, and no still can show a wallrun into a
   slide into a C4 jump. See `docs/itch.md` and `docs/itch-page.md`.
7. **Sound.** There is no audio in the project at all - no footsteps, no wallrun, no landing, no
   explosion. `AGetawayC4` has an `ExplosionSound` slot and nothing fills it. For a game whose whole
   appeal is movement, this is probably the biggest single improvement left per hour spent.
8. **Logging in Shipping.** `bUseLoggingInShipping` is not set in `Source/Getaway.Target.cs`, so a
   packaged build writes no log at all and a player who hits a crash has nothing to send you. One
   line, and worth it before the page goes public.
9. Before each new piece, check the *still unverified* / *not proved* notes in the matching `docs/` file for things I haven't played yet.
