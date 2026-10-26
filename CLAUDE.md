# Project context for Claude

## The game

**Vaultborne** (working title was Getaway; the code, files and classes still use that name, see *Naming*).

A 3D, first-person take on **Ultimate Chicken Horse**: each round, players draft a piece from a random box, place it in a shared level, then everyone races to the objective. The twist is **deep, skill-based movement** (Source/Apex/Titanfall style), so pieces should change *how* you can move (wallrun surfaces, slide ramps, breakable panels, C4 pickups, crumbling bridges, bounce pads), not only block the way. Scoring follows Ultimate Chicken Horse: no points if everyone finishes, so players try to make the level hard but still beatable for themselves.

Engine: **Unreal Engine 5.8 (C++)**. Everything lives in the `GetawayMovement` plugin.

## Where things stand (2026-10-26)

- **Movement:** ported from the browser prototype and signed off.
- **Multiplayer:** client-authoritative, two players play-tested 2026-10-17.
- **Round loop:** shared placement phase with a build clock, the run, Ultimate Chicken Horse scoring, and deaths (spikes, falling off the course).
- **Pieces:** wall, floor, ramp, breakable panel, crate, collapsing bridge, C4 pickup, spikes.
- **Art:** none yet. Everything is grey boxes and debug-line ghosts. Free third-party assets come later.

## Where the details live

This file is kept short on purpose. The history and the how-it-works notes are in `docs/`. Read the matching file **before** working in that area, and write new notes there, not here.

| File | Read it before touching |
|---|---|
| `docs/test-course.md` | `/Game/Maps/TestCourse`, its layout and what each obstacle tests |
| `docs/placement.md` | the placement phase, build clock, catalog, piece positioning, directional pieces, the objective |
| `docs/multiplayer.md` | replication, client authority, running the two-player test in the editor |
| `docs/round-loop.md` | phases, scoring, shared placement, syncing breakables, bridge, C4 and pickups |
| `docs/spikes-and-death.md` | spikes, surface-mounted pieces, dying, breakables |
| `docs/falling-slopes-cleanup.md` | fall deaths, the "everything touches something" rule, pieces on slopes, between-round cleanup |
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

The game is called **Vaultborne**. The display name is set in `Config/DefaultGame.ini`. The project file, the `Getaway` module, the `GetawayMovement` plugin and every `AGetaway…` / `UGetaway…` class keep the old name. **Don't rename any of them unless I ask.** A full rename needs CoreRedirects in `DefaultEngine.ini` (the maps store class paths), a Git commit first, and its own session.

## Third-party assets

Each pack goes in its own folder, `Content/ThirdParty/<PackName>/`, and is never edited in place: copy what needs changing into our own folders. Every pack gets a row in `CREDITS.md` (name, source, license, link, attribution needed?) the day it's added. Logic stays in C++; art, sounds and meshes get hooked up in Blueprint subclasses.

## Decisions already made (don't undo without asking)

- **Movement feel:** Source-style ground friction and air strafing, lossless bunny hops, auto-hop while holding jump, jump buffering, mouse-wheel jump.
- **Double jump:** 1 air jump. It redirects your momentum toward the keys you're holding, without losing speed. It is not spent by a jump pressed just before landing (that press is kept for a bunny hop).
- **Slide:** crouch at speed. Entry boost on a 1 s cooldown, very low friction, speeds up on slopes.
- **Wallrun:**
  - Only when the wall is **beside** you (within about 40 degrees of straight left or right of your view), never in front.
  - Forgiving attach: 60 cm reach, snaps onto the wall, works at low speed and while falling fast.
  - Space kicks off.
  - In the Unreal version you can't re-grab the same wall until you land.
- **Mantle:** stays in. Keeps 75% of your run-up speed (it doesn't stop you) and does a small camera jiggle.
- **No kick/punch button.** Breaking things is done by hitting them at speed (slide or jump into them) or with C4.
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

- **Local first, Steam later** (decided 2026-10-26). Build and test for local/LAN play now. Steamworks, online sessions and achievements wait until the game is fun locally.
- **Client-authoritative multiplayer is fine for local play. Revisit it before Steam.** Each machine decides where its own player is, which makes cheating easy with strangers online. That is a deliberate trade-off for now, not something to fix unasked.

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

`/Game/Maps/TestCourse` is the startup map, so pressing Play drops you into it. `bStartInPlacement` on the game mode is on, so Play opens the placement camera; press Enter to run straight away. Two-player testing: see `docs/multiplayer.md`.

## Source control

Git with Git LFS for `.uasset`, `.umap` and media files (rules in `.gitattributes`; build output ignored in `.gitignore`). Commit only with my OK.

## Next steps

1. More traps. Spikes were the first.
2. The party box deals from the whole catalog. A per-round deck is still to do.
3. The placement ghost is drawn with debug lines, which don't exist in Shipping builds. It needs a real translucent material before release.
4. Settle whether the wall and breakable panel should be rotated to match the lab (see *Still unverified* in `docs/placement.md`). It's a feel question for me.
5. Before each new piece, check the *still unverified* / *not proved* notes in the matching `docs/` file for things I haven't played yet.
