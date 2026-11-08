# Project context for Claude

## The game

**Adrenaline** (working titles were Getaway, then Vaultborne; the code, files and classes still use the Getaway name, see *Naming*).

A 3D, first-person take on **Ultimate Chicken Horse**: each round, players draft a piece from a random box, place it in a shared level, then everyone races to the objective. The twist is **deep, skill-based movement** (Source/Apex/Titanfall style), so pieces should change *how* you can move (wallrun surfaces, slide ramps, breakable panels, C4 pickups, crumbling bridges, bounce pads), not only block the way. Scoring follows Ultimate Chicken Horse: no points if everyone finishes, so players try to make the level hard but still beatable for themselves.

Engine: **Unreal Engine 5.8 (C++)**. Everything lives in the `GetawayMovement` plugin.

## Where things stand (2026-11-07)

> **2026-11-07 was a play-test day**, and most of the "built but not played" notes below and in
> `docs/` were answered in one sitting. Twelve things were signed off, two came back as work (the
> jump pad's low-friction window and the pad camera), one opened a design question (whether effects
> should become pieces), and one was not tested (finishing and the watch camera). The verdict table
> is at the end of `docs/todo.md`, and each verdict is written up where the feature lives.

- **Movement:** ported from the browser prototype and signed off.
- **Input:** keyboard and mouse, plus a full controller map for both the run and the placement
  camera. **Played with a pad 2026-11-07 and it feels bad** - the button map drew no complaint, the
  camera tuning did: *"feels bad, will change later."* Nothing changed yet, and the order to try is
  in `docs/controller.md`, where the thing to notice is that `StickLookExponent` is 1 (no curve at
  all) and `StickLookRampTime` is 0 (no acceleration), so three of the four knobs are additions
  rather than corrections. See `docs/controller.md`.
- **Multiplayer:** client-authoritative, two players play-tested 2026-10-17. **Two machines in two
  different states played 2026-11-03 over a Tailscale tailnet, and it worked amazing** - smooth
  movement, and deaths, respawns, breakables, C4, the score and the collapsing bridge all in step.
  The last big unknown in the project, closed. Note what a tailnet means: the game saw a flat
  private network and Tailscale did the NAT traversal, so **a stranger from itch still needs UDP
  7777 forwarded or a mesh VPN of their own.** See *Two machines, two states* in
  `docs/multiplayer.md`.
- **Playing with friends:** packaged 2026-11-02, and **switched to Shipping 2026-11-03** - 609 MB in
  `C:\Users\natha\Desktop\AdrenalineShipping`, down from 946. The cook is clean: every material and
  piece Blueprint is in, and no ThirdParty or editor content. **Played on screen for the first time
  2026-11-03 and it all works** - the placement ghost, the HUD messages and the C4 explosion all
  survive Shipping, so the thing that blocked a distributable build for weeks is closed. Shipping
  writes **no log**, so a crash in a packaged build leaves nothing to read. **Two machines in two
  different states have now played it** (2026-11-03). See *It is packaged* and *Two machines, two states* in
  `docs/multiplayer.md`, and `docs/itch.md`.
- **On itch.io:** <https://rpidgey.itch.io/adrenaline>, **restricted, behind a password, and
  deliberately so** (corrected 2026-11-06). **Two people have played it and that is the intent for
  now.** This file previously claimed the page was public on the strength of an anonymous HEAD
  request returning 200 - that was wrong, and the mistake is worth knowing: a restricted itch page
  serves its *password gate* with a 200, so the HEAD check cannot tell restricted from public. It
  only ever distinguished "exists" from "does not exist". Build **#2293004 (version 2)** is live on
  the `windows` channel, pushed 2026-11-03 after a map edit; patching made it a 129 MB upload
  rather than 609. Updating a release is two commands, package then push - see *Pushing an update*
  in `docs/itch.md`. The page itself is still **empty**: no text, no cover image, no screenshots,
  no video. The text is all drafted in `docs/itch-page.md`.
- **Front end:** a menu with ADRENALINE over the course turning slowly, a name box, Host, an IP box
  and Join, and Quit; Escape in a game gives Resume / Leave to menu / Quit. Built 2026-11-02,
  **Host played 2026-11-02. Joining works** - two processes over the loopback played a full round
  and were scored by name. **The whole front end was played 2026-11-03** in a Shipping build - the
  first time Escape has ever been pressed outside the editor: Resume, Leave to menu, Quit and a join
  that fails all work. **Cancel and the host leaving mid-game were both played 2026-11-07, so there
  is no unpressed button left in the front end.** **Two separate machines, in
  two different states, connected through this menu on 2026-11-03.** A join that fails now gives up after 10 s and says why, and the menu
  shows your own address so others know what to type. **Player names work for the first time**:
  `?Name=` had always been discarded. There is still no lobby - and one would not have fixed any of
  this. See *The front end* in `docs/multiplayer.md`, and the two traps at the end of it: input
  going dead after a travel, and the engine never bringing you back from a failed connection.
- **Round loop:** shared placement phase with a build clock, the run, Ultimate Chicken Horse scoring, and deaths (spikes, falling off the course).
- **Pieces:** wall, floor, ramp, breakable panel, crate, collapsing bridge, C4 pickup, spikes,
  jump pad, anti-gravity vortex, paddle wheel (the Windmill, added 2026-11-03: four level
  plates orbiting a horizontal axle that carry you up and over, and bat you off if you are
  standing in the way). **Played 2026-11-03 and it works.** Spikes and jump pads stuck to a plate
  ride round with it, which took attaching a surface-mounted piece to the **component** it was
  placed on rather than to the host actor's root - see *Spikes and pads ride a plate* in
  `docs/placement.md`. **Played 2026-11-07 and it works as intended.**
  The thirteenth is the **speed ring** (added 2026-11-05): a hoop you run through that raises your
  top speed to 1200 cm/s for three seconds, so the effect outlives the piece and you spend it on
  the jump, wallrun or slide that comes next. **Played the day it was built and signed off at the
  defaults** - the first piece that needed no retune after playing. See *The speed ring* in
  `docs/placement.md`.
  The fourteenth is the **grapple pickup** (added 2026-11-05): the C4 pickup's pad with a hook
  turning over it, which hands you one grapple hook. **Played 2026-11-07: it works, and neither pad
  is clear enough to read** - *"the grapple pickup isn't very clear but neither is the c4, they will
  be edited later"*, which is one art job across both pieces, deliberately deferred. Your hand holds
  **one item**, C4 and hooks counted together, so a C4 pad and a hook pad near each other are a
  choice between them - **played and signed off**, *"one c4 was intentional and feels good"*. See
  *The grapple pickup* in `docs/placement.md`.
  A fifteenth, the **void** - a sphere that pulled you toward a lethal centre - was built and
  **scrapped the same day** after five versions and four plays. The source is parked in
  `docs/archive/scrapped-void/`. Read *The void, and why it was scrapped* in `docs/placement.md`
  before building anything like it; the short version is that it moved you instead of letting you
  move, and no amount of tuning or payoff rescued that.
- **Effects:** a layer on top of the round, built and **played 2026-11-03 - all three work**.
  Every other round (1 plain, 2 with, 3 plain) both players are given the same effect before
  anybody builds: double jump, one C4, or 60% gravity. One at a time, gone at the end of the round.
  The fourth is the **grapple hook** (added 2026-11-05): one hook, left click to throw it at what
  you are looking at and click again to let go, and you keep air strafing and steer the swing with
  the camera the whole time. **Played 2026-11-07 and signed off at its first-pass numbers** -
  *"plays very well and feels like a slingshot, which is the intended effect"* - the second piece in
  a row to need no retune, after the speed ring.
  The effect lands big in the middle of the screen as the build phase opens, holds 2 s and then
  travels to the top left corner, where it stays for the round; **that part is not played.**
  **The whole layer is now in question**: the leaning after that play-test is to stop dealing
  effects per round and let *pieces* hand out the same abilities, which two of the four already have
  (the C4 pad and the grapple pad). Not a decision - read *Effects may become pieces* in
  `docs/effects.md` before building anything on the round layer. See `docs/effects.md`.
- **Art:** an art direction, not assets. Unlit pink on black, each face see-through with a solid
  band round its rim. Three passes 2026-11-02, **played and signed off at speed**. See
  `docs/look.md`. The point of it is that greybox *is* the look, so meshes stop being a blocker -
  which is also why the placement ghost and the C4 explosion are both built geometry rather than
  debug draw or Niagara.
  **There is a logo now** (2026-11-06): a pink-and-black square split by a white diagonal with an
  outlined A over it, already the look in one picture. The master is `Branding/adrenaline-logo.png`
  and `Tools/make_brand.py` turns it into the packaged exe's icon (`Build/Windows/Application.ico`)
  and both splash screens (`Content/Splash/Splash.bmp` and `EdSplash.bmp`) - all three picked up by
  filename, nothing to set in the editor. **Not applied in the game itself**: the menu still spells
  the word out as text, and the itch page has no cover. See *The logo, the icon and the splash* in
  `docs/look.md`.
- **The HUD** was redone 2026-11-06: bigger and glowing, then **cut back twice the same day** to
  three things in a run - your hand top left, the clock top centre, the speedometer bottom centre -
  and then flattened. The visor arcs are off, the curve is off, the key list is gone from the run
  (the build phase keeps its own), and so are the `GROUND` / `SLIDE` movement state and the u/s
  line. The lesson worth keeping is in `docs/hud.md`: **a curve needs something straight to argue
  with and a frame to belong to**, so once the arcs and the long key lists went, the curve had
  nothing left to read against and came out rather than being tuned down.
  The first version drew no text at all: canvas text is dropped silently unless its `FSlateFontInfo`
  carries a `UFont`, which the obvious `FCoreStyle` call does not - see *The trap that cost a build*
  in `docs/hud.md`, along with the terminal one-liner that launches the game into a round and
  screenshots it, which is how it was checked. The size change is also a fix - the HUD used
  to ask the engine's small and large fonts for a *multiple* of themselves and both of those are the
  same 10 px face, so every readout was a stretched raster and "larger" would only have meant
  blurrier. Text is now rasterised at the size it is drawn at, and **nothing in it can be tuned from
  the Details panel yet** (the game mode sets `HUDClass` in C++ and there is no Blueprint child; six
  clicks fix that and `docs/effects.md`'s same complaint at once). **Played 2026-11-07: the run's
  HUD is readable at speed**, so cutting it back worked, and no frame cost was visible. The **build
  phase** is *"a little cluttered, might change later but feels okay for now"* - which is the
  verdict the run's HUD got before it was cut back twice, so if it gets a pass, take things out
  rather than shrink them. The play-test also asked for a notification on picking something up, and
  **both pads already notify** - so that is a prominence job, not a missing feature. See
  `docs/hud.md`.

## Where the details live

This file is kept short on purpose. The history and the how-it-works notes are in `docs/`. Read the matching file **before** working in that area, and write new notes there, not here.

| File | Read it before touching |
|---|---|
| `docs/todo.md` | what is outstanding, in one list: the play-test backlog first, then jobs, the itch page, sound, features |
| `docs/test-course.md` | `/Game/Maps/TestCourse`, its layout and what each obstacle tests |
| `docs/placement.md` | the placement phase, build clock, catalog, piece positioning, directional pieces, the objective |
| `docs/multiplayer.md` | replication, client authority, running the two-player test in the editor, packaging and LAN play with friends, the menu and Escape |
| `docs/round-loop.md` | phases, scoring, shared placement, syncing breakables, bridge, C4 and pickups |
| `docs/effects.md` | round effects: the every-other-round rule, the three effects, where each part lives |
| `docs/spikes-and-death.md` | spikes, surface-mounted pieces, dying, breakables |
| `docs/falling-slopes-cleanup.md` | fall deaths, the "everything touches something" rule, pieces on slopes, between-round cleanup |
| `docs/controller.md` | controller support: the button map, stick tuning, why a stick can't use the mouse look code |
| `docs/blueprint-tuning.md` | `BP_Character`, `BP_GetawayGameMode`, tuning values in the editor, where the game mode is named, giving a piece a material |
| `docs/look.md` | the unlit pink look: materials, the palette and what each colour is allowed to mean, lighting, bloom, the logo and where it is applied |
| `docs/hud.md` | the in-game HUD: how text is sized, curved and made to glow, the visor frame, the knobs and why none of them is in the Details panel yet |
| `docs/sound.md` | sound and music: the movement sound slots, what fires each, the 2D and own-sounds-only decisions, what has no audio yet |
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

- **Some of the values below are overridden in `BP_Character` and `BP_GetawayGameMode`, and the
  Blueprint is what you play** (read out of the assets and confirmed deliberate 2026-11-05).
  Twenty-one differ from the C++ defaults quoted here. Most are tuning, but three turn documented
  behaviour *off*: `AirJumps` **0** (no double jump) and `GoalPoints` 0 with
  `bNoPointsIfAllFinish` **false** (no Ultimate Chicken Horse scoring). `SlideCooldown` plays at
  0.5 rather than 1.5, `WallAttachRange` at 50 rather than 60, `AirSpeedCap` at 35 rather than 75.
  The full list is *What the Blueprints actually play at* in `Plugins/GetawayMovement/README.md`,
  and `Content/Python/export_tuning.py` dumps it again at any time. Clearing an override in the
  Details panel puts the C++ default back.
- **Movement feel:** Source-style ground friction, lossless bunny hops, auto-hop while holding jump, jump buffering, mouse-wheel jump. **Air strafing no longer gains speed** - see *Speed comes from the pieces* below.
- **Speed comes from the pieces, not from strafing** (decided and played 2026-11-06, signed off the
  same day - *"I like this a lot more, it's much more consistent"*). Air input
  **steers your momentum without lengthening it**: `bAirStrafeGain` is **off**, so a bunny hop
  preserves speed rather than building it, and nothing you can do with the keyboard and mouse takes
  you past `RunSpeed` 800. Top speed comes only from what is placed in the level - jump pads, speed
  rings, slides down slopes, the C4 jump, the grapple - which makes placing a ring or a pad a real
  decision instead of a shortcut for somebody who cannot strafe. It is also what lets `SmashSpeed`
  1100 sit out of reach on foot - see *Breaking is granted by the level* below.
  What you get in exchange is **real air control**: `AirSpeedCap` 35 was only ever that small to
  stop strafing gaining for free, and the cost was that you could influence your flight only within
  about 2.5 degrees of perpendicular to your own momentum. With the gain gone the cap stops being
  load-bearing, so redirect-only mode ignores it and targets the full wish speed, opening the gate
  at every angle. `AirControlAccelerate` 3 is the turn strength, worth about 170 deg/s at full
  speed. `bAirStrafeGain` back on restores classic Source strafing exactly, with no retuning.
  `AirControlAccelerate` 3 was played at its first-pass value and kept.
- **Turning the camera while you run does not speed you up** (fixed 2026-11-06; this one was a bug,
  not a feel change). Holding forward and turning settled at about **1030 cm/s**, because the ground
  acceleration cap is on your speed *projected* onto the direction you are asking for, never on your
  actual speed - so turning keeps the projection under 800 while the real speed climbs, and the push
  along your own momentum outruns friction. `bGroundStrafeGain` off ratchets it: acceleration still
  steers you freely but cannot raise your speed past what you are asking for. Faithful Source
  behaviour either way (the same formulas with Source's constants give a 1.30x overspeed against our
  1.29x), just far easier to exploit with a mouse here.
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
- **No kick/punch button.** Breaking things is done by arriving at `SmashSpeed` 1100 or with C4 - and since 2026-11-06 that is a speed only the pieces can give you, so a slide or a jump on flat ground is no longer enough. See *Breaking is granted by the level* below.
- **Breaking beats climbing** (decided 2026-11-06, reversing "climbing beats breaking" of
  2026-11-01): if you hit a breakable **fast enough to break it, it breaks** - it is never spared
  because its top happened to be climbable. Speed is the whole decision, and the two rules can
  never overlap: `SmashAhead` only runs at all above the smash threshold, so anything slower than
  that still climbs exactly as before. `bMantleOverSmash` on the movement component (now **false**
  by default) puts the old rule back.
  The old rule was never played, only reasoned about - see *Still unverified* in `docs/placement.md`.
  **A crate was already outside it** (settled 2026-11-03): the crate was retuned to 200 cm cubed -
  **250 since 2026-11-05** - which puts its top above `MantleMaxHeight` 170, so a crate standing on
  the floor beside you smashes rather than being climbed whatever this flag says. Asked and left
  that way deliberately - a 2 m block reads as a wall, not a step. Raising `MantleMaxHeight` past
  200 on `BP_Character` would hand climbing back, and make everything else that tall climbable with
  it. Don't.
- **Breaking is granted by the level, not by running hard** (decided and played 2026-11-06).
  `SmashSpeed` is **1100**, one rule for every way of moving, and it is deliberately **out of reach
  on foot**: a full sprint is 800, a slide out of that run is 1050, a wallrun is capped at 920, and
  with `bAirStrafeGain` off there is no strafing past any of them. So the only things that break a
  breakable are a **jump pad** (1400), a **speed ring** (1200), a **slide down a ramp** where
  gravity carries you past it, and **C4**. You have to be going exceptionally fast, and only the
  pieces make you that fast - the same thought as *Speed comes from the pieces* above, applied to
  destruction.
  Two older rules died with it. Sliding had a **second, much lower threshold** of its own
  (`SlideMinSpeed * 0.9`, so 450), so almost any slide burst almost anything it brushed - that
  branch is gone rather than retuned, and the `SlideSmashSpeedScale` knob that briefly replaced it
  was dropped too. And **"hitting them at speed (slide or jump into them)" no longer holds on the
  flat**: sliding into a panel on level ground now does nothing. Since this is also the
  climb-or-break line, a breakable on foot is always a step, and `bMantleOverSmash` only decides
  anything when you arrive by piece.
  The C++ default is 1100 as well, so `BP_Character`'s override is now redundant and can be cleared
  at leisure - same value either way.
- **A hand is one normal piece and one special one** (decided and played 2026-11-06, going through
  the catalog a row at a time, and signed off at the first split with no piece moved across). The party box no longer deals two at random: `bOneOfEach` on the game mode draws
  one piece you build the course **out of** and one that changes how it is **moved through**, normal
  in slot 1 and special in slot 2, the same order every round. `FGetawayPieceDef::bSpecial` says
  which a piece is. **Special is six:** C4 pickup, grapple pickup, spikes, jump pad, vortex, speed
  ring - pickups, hazards and speed devices. **Normal is the other eight:** floor, wall, ledge, ramp,
  breakable panel, crate, windmill, collapsing bridge. The panel and the crate are normal because
  since *Breaking is granted by the level* nothing you can do on foot breaks them, so placed alone
  they are a wall and a block and the twist needs a *special* piece nearby to go off - which is the
  combination the hand now guarantees. The windmill is a lift you ride; the bridge is a floor that
  happens to be a lie. `bOneOfEach` off deals from the whole catalog as before.
  **The flag has to be pushed into `BP_GetawayGameMode`** with
  `Content/Python/flag_special_pieces.py`, because the Blueprint stores the whole `Catalog` array
  and every row reads false until something writes it. See *One normal and one special* in
  `docs/placement.md`.
- **One item in your hand** (decided 2026-11-05, replacing "carry up to two charges" of C4).
  `ItemCapacity` on the character is 1 and counts C4 charges and grapple hooks together, so no
  pickup of either kind gives you anything until you have spent what you are carrying. Throwing or
  firing counts as spending - a charge stuck to a wall waiting for F has left your hand. It makes a
  C4 pad and a hook pad placed near each other a choice rather than a shopping trip, and it is why
  `MaxCharges` is 1 now. `ItemCapacity` 2 with `MaxCharges` 2 restores the old behaviour. The cost
  is the double rocket jump, which nobody has tried to miss yet. See *Your hand holds one item* in
  `docs/placement.md`.
- **C4:**
  - One charge at a time, G to throw, F to detonate.
  - **C4 is a collectable resource** (decided 2026-10-15, replacing "charges return when they detonate"): a thrown
    charge is spent for good. You spawn with `StartingCharges` (0 by default) and restock by running through an
    `AGetawayC4Pickup`, which grants `ChargesGranted` (**1** since 2026-10-19).
    Since the one-item hand (2026-11-05) a pickup also gives you nothing while you are holding a
    grapple hook. `StartingCharges = MaxCharges` restores the old always-loaded behaviour.
  - **Inherits 100% of your velocity** when thrown.
  - Sticks to surfaces.
  - Rocket-jump style launch.
  - Bursts breakables.
  - Never damages walls or floors.
  - **A pickup comes back when you die, not on a timer** (decided 2026-11-05, after playing the
    8 s clock, and it applies to both pickups). One charge or one hook per life: a pad you have
    emptied stays empty for as long as you keep yourself alive, so restocking costs you a death
    and the walk back. `bReturnsOnDeath` on the pickup, with `RespawnDelay` above 0 to add a timer
    as well. The pickup watches the character that took it weakly, so a death is the pointer going
    stale and there is nothing to subscribe to - which also frees every pickup between rounds.
    **Played 2026-11-07, and the loop it was thought to open is not one:** *"theres never a reason
    to intentionally die since you want to go fast."* A death costs the whole run-up, which in a
    race is worth more than a charge, so no rule is needed - and that generalises: in a game scored
    on finishing, anything that costs you time polices itself.
  - **A pickup is per player** (decided 2026-10-18, after playing): each player can collect the
    same `AGetawayC4Pickup` once per life, and each sees their own pad empty. The alternative -
    a shared pickup that the first player there takes from everyone - was considered and turned
    down: per-player is fairer in a race, and it falls straight out of client authority, since the
    charge count lives on the machine that owns the character. Making it shared means routing the
    grant through the server like `AGetawayBreakable::Break` and replicating `ChargesHeld` to its
    owner. Don't switch it without asking.
- **Destruction stays simple:** only panels and crates break, into loose scraps the player can shove around. A Finals-style fracture/collapse system was tried in the prototype and **scrapped**: it was too heavy and didn't feel like The Finals. If big destruction comes back later, use Chaos Destruction (Geometry Collections), not custom code.
- **Camera effects:** wallrun tilt, speed FOV, shake, landing kick and mantle jiggle are wanted, with a toggle to turn them off. Note when porting camera effects: roll signs invert between the lab and Unreal. three.js rolls about a camera Z axis pointing back at the viewer; Unreal rolls about forward X in the aviation sense (positive = right side down). Copying a roll formula across verbatim tilts it the wrong way, which is exactly what happened to the wallrun tilt (fixed 2026-10-15). A camera "shaking" bug was reported in the prototype and was likely frame-timing judder. Movement should look smooth at any refresh rate.

- **Dying puts you back at the start** (decided 2026-10-31, replacing "death ends your run for the round"). A fresh character at the PlayerStart after `DeathLingerTime`, your run state untouched, as many times as the run clock allows. Your C4 charges do not come back with you (confirmed 2026-10-31). The game mode's `bRespawnOnDeath` turns it off for the old Ultimate Chicken Horse behaviour. See `docs/spikes-and-death.md`.
- **Finishing lifts you into the fly camera to watch** (decided 2026-11-04, after it was played
  across two machines). Crossing the line gives you a big **YOU MADE IT** with your time and place
  where you landed, and `FinishWatchDelay` (2 s) later the camera takes your body away and you
  watch the rest of the field from above. Before this you kept your character, got no prompt at
  all until the round ended, and could fall off the course after winning - which told you
  "Killed" and then left you hanging, since nothing respawns a player who is not running. The
  alternatives were a death-proof victory lap and freezing you at the pad; watching won because
  the course has nothing left to offer somebody who is already home, and watching your rivals
  struggle is half of Ultimate Chicken Horse. `bWatchAfterFinish` off restores the old behaviour.
  See `docs/round-loop.md`.
- **Only attached pieces follow a slope** (decided 2026-11-05, narrowing the 2026-10-26 decision
  that everything lies down on one). Spikes and jump pads still lie flat on a ramp face - they are
  surface mounted, which is to say attached to whatever they are stuck to. A wall, floor, crate or
  ramp placed on a slope now stands **square**, resting on the slope rather than tipping onto it.
  `bSlantOnSlopes` on the placement pawn puts the old behaviour back. The cost is a wedge of air
  under the downhill end of a long piece - 133 cm for a 400 cm piece on an 18.4 degree ramp -
  because burying it instead is what `FitTolerance` refuses. It also means a ramp placed on a ramp
  steps rather than continuing the slope. See *Only attached things follow a slope* in
  `docs/falling-slopes-cleanup.md`.
- **Local first, Steam later** (decided 2026-10-26). Build and test for local/LAN play now. Steamworks, online sessions and achievements wait until the game is fun locally.
- **Client-authoritative multiplayer is fine for local play. Revisit it before Steam.** Each machine decides where its own player is, which makes cheating easy with strangers online. That is a deliberate trade-off for now, not something to fix unasked.
- **The wall and breakable panel stay rotated 90° from the lab** (decided 2026-11-02, after
  playing). A wall you place runs *away* from you rather than across your view, which makes it the
  better wallrun surface - and this game is about wallrunning in a way the browser prototype was
  not. Reversing it would be a swap of X and Y on the `wall` and `break` catalog entries. See
  `docs/placement.md`.
- **Effects are an every-other-round layer** (decided 2026-11-03, played the same day). Both
  players always get the *same* effect, it is dealt and shown before anybody builds so it changes
  what you place, only one is live at a time, and it dies with the round. Round 1 is always plain.
  `bRoundEffects` off plays the game as it was. See `docs/effects.md`.
- **A speed boost raises your ceiling, it does not shove you** (decided and played 2026-11-05, with
  the speed ring). Source friction always drags you back toward `StopSpeed`, so a velocity kick is
  eaten in about a second and a boost "for three seconds" would mean nothing. So the speed ring
  multiplies `RunSpeed` for the duration instead, which is one number
  (`UGetawayMovementComponent::GetBoostedRunSpeed`) read by running, crouch-walking, air strafing,
  the double jump's redirect and the wallrun alike. The snap on entry only ever scales the speed
  you already have, so **a ring makes you faster along the line you took through it** and never
  redirects you - which is why it works from either side. The camera's speed FOV deliberately
  keeps reading the *raw* `RunSpeed`, or a boosted run would look like a normal jog; the boost's own
  signal is a flat `BoostFOV` bump. Boosts refresh rather than stack. See *The speed ring* in
  `docs/placement.md`.
- **A jump pad just launches you harder** (decided 2026-11-07, reversing *a jump pad takes the drag
  away rather than raising your ceiling* of 2026-11-06 after playing it - *"honestly it doesn't feel
  good, i think the change should be just increasing the jump pad strength"*). The low-friction
  window goes and `LaunchSpeed` goes up. **The code still does the old thing** - see *The window is
  coming out* in `docs/placement.md` for what it touches, including the warning to check the new
  launch against `SmashSpeed` 1100. What follows is the reasoning that lost, kept because none of it
  was wrong - only on the wrong axis, which is the void's lesson at a smaller scale: **a piece
  should change what you can do at the moment you touch it, not change the rules for a while
  afterwards.** A launch is a moment you aim at; a friction window is three seconds of the floor
  behaving differently, which you did not ask for and cannot see. The speed ring survives the same
  test because a raised ceiling is something you *spend*, on the jump or wallrun you pick next. The
  other half of the decision above, and the opposite lever. A ring raises `RunSpeed` so you can
  *climb* to a speed you could not hold; a pad already throws you at `LaunchSpeed` 1400, far above
  anything you can run at, and the problem is **keeping** it - Source friction eats the whole launch
  on the first touchdown. So the pad hands you a low-friction window instead: `FrictionScale` 0.3
  for `FrictionTime` 3 s, multiplying `GroundFrictionSource` *and* `SlideFriction`
  (`UGetawayMovementComponent::AddLowFriction`), so landing out of a pad into a slide is the fast
  line. It is not a push and never touches your velocity, so a window can only ever let you keep
  what you landed with. Windows refresh rather than stack, like boosts.
  **There is no air friction in this game to reduce**: `FallingLateralFriction` is 0 and
  `CalcVelocity` applies no drag at all while airborne, so speed through the air is already
  lossless and the window has nothing to do until you touch down.
- **Every piece must grant ability or test precision. None may take ability away** (decided
  2026-11-06, after building the void wrong and playing it three times). This is the most useful
  thing the void taught, and it is about the whole catalog rather than one piece. The pad, ring,
  vortex, grapple and windmill all *grant* you movement; the spikes and the panel *test your
  precision* - you can wallrun straight past a spiked wall if you keep off it. The first void did
  neither: it pulled you in, lifted you off the floor and damped your input to 0.45 of normal, so
  once the field had you the outcome was decided and your job was to watch. In a game whose entire
  appeal is movement control, **reducing movement control is the one thing a piece cannot do** -
  and no amount of tuning helped, because every number was on the wrong axis.
  Note how plausible the wrong turn was: the observation that prompted it ("you can still move at
  full speed in it, so the pull never matters") was correct, and damping the input is the obvious
  fix. The right fix was to give the player a reason to be in there. So `InputScale` is gone,
  `WishAnalog` is back to being unscaled by anything, and the void got a **payoff** instead: graze
  the lethal core and it slings you out with a speed boost and a low-friction window scaled by how
  close you cut. The question went from "can I escape it" (no) to "how close dare I cut it" (your
  call), which is the Ultimate Chicken Horse bargain. **That did not rescue it either, and the void
  was scrapped** - a piece whose basic verb is wrong cannot be fixed by a payoff bolted onto it.
  The rule stands whatever happens to that piece: check a new piece against it **before** writing
  code. See *The void, and why it was scrapped*
  in `docs/placement.md`.
- **Hand-dressed pieces are protected by emptying their row, not by recording the colour**
  (settled 2026-11-05, after `dress_pieces.py` put its table back over an afternoon's editor work).
  Five rows in `PIECES` are now `{}, {}` - spikes, panel, jump pad, C4 pickup, vortex. The table
  cannot hold the right value because **nothing can read a component material back off a
  Blueprint's class default object**, which is the same limit that stops `export_tuning.py`
  reporting them. The row itself must stay, or the catalog points that piece at the raw C++ class.
  A fresh clone gets those five undressed, and that is the accepted cost. See *A script run that
  resets a colour with nothing saved and nothing to revert* in `docs/blueprint-tuning.md`.
- **Music carries the game, effects are for major events only** (decided 2026-11-06). **There is no
  movement foley.** No footsteps, no jump, no landing thump, no wall kick, no slide or wallrun loop,
  no mantle. A track per phase does the work those would have done, and sound effects are kept for
  things that happen rarely and on purpose: the C4 blast, a jump pad, a speed ring, a pickup, the
  grapple, a breakable bursting, the build clock running out, GO, **YOU MADE IT**, dying.
  Three reasons, each of which is a decision already on this list rather than a new preference.
  **The look**: greybox *is* the look, so the game is already abstract, and realistic foley on an
  unlit wireframe raises a question the art does not answer - whose feet, on what material.
  **You only ever hear yourself**: the usual reason a first-person game needs footsteps is hearing a
  rival, and `SoundsAudible()` ruled that out long ago, so what was left was flavour, which a track
  does better. **The build phase makes no sound at all** - half of every round is a placement camera
  with nothing moving in it, and music is the only thing that can reach it.
  It also costs the least and risks the least: the loops were the hardest thing on the sound list
  and are now simply gone, and a driving track trailers the game far better than footsteps for the
  itch page video.
  Two sounds were built before this landed and **both were cleared**: the wall kick (2026-11-05) and
  the landing thump (2026-11-06, the same day it was made). The landing was the close call - weight
  on impact is the one thing a screen cannot convey - and it went out because with auto-hop it fires
  every 750 ms, which is a metronome under the track. **Both sounds were then deleted outright** (asked for the same
  day), wavs and imported assets alike, so `Audio/Movement/` and `Content/Audio/walljump/` no longer
  exist. What stays is the machinery: the ten slots are still wired, still tuned and still
  documented, and `bMovementSounds` stays **on** with every slot empty - **clear a slot to express
  this, never the master switch**, because off is how the wall kick spent a day silent with nothing
  to say why. Both deleted sounds are recoverable from git history, and their listing ids are in
  `CREDITS.md` under *Two sounds that were here and are gone*.
  **The one debt it takes on is the slide cooldown.** Every other piece of movement feedback has a
  visual - wallrun tilt, the landing camera kick, speed FOV, the speedometer - but nothing at all
  tells you when your next slide is ready, and a sound was the obvious answer. That is now a HUD
  job: a small tell near the crosshair. See *What this list no longer contains* in `docs/sound.md`.
  It also makes the **Music / SFX Sound Class split** matter early rather than eventually, because a
  music-led game with no volume slider is one players mute.
- **The unlit pink look is the look** (confirmed 2026-11-02, by playing it at speed). The edge band
  is what carries shape and depth once the lights are off; pass one was flat without it. See
  `docs/look.md` before changing any of it.

## Units and tuning

Unreal uses cm. The prototype showed Source units: **1 u = 2.5 cm** (40 u = 1 m, 320 u/s = 800 cm/s). The tuned defaults are in the plugin README table.

**The in-game speedometer reads cm/s** (changed 2026-11-06), so the number on screen is the number
in the Details panel. It showed the lab's u/s before that, and every reading was 2.5x smaller than
the property being tuned against it - which made a working 550 smash threshold look broken, because
a slide reading "300" is really 750 cm/s. The lab's u/s is still on the small grey line under it,
for comparing feel against `Reference/getaway-lab.html`. The remote-player debug line over other
players reads cm/s too.

Key values:

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
2. The party box deals one normal piece and one special one from the whole catalog (2026-11-06). A
   per-round deck is still to do.
3. ~~**A front end**: a menu with Host, Join and an IP box, and Escape to leave in game.~~ **Done
   2026-11-02, and fully played in a Shipping build 2026-11-03** - Resume, Leave to menu, Quit and a
   join that fails all work, the first time Escape has been pressed outside the editor. See *The
   front end* in `docs/multiplayer.md`. **Cancel and the host-leaves path were played 2026-11-07
   and both work**, so every button in it has now been pressed. What is left of it: a pad cannot
   drive it, and there is no lobby or settings page.
4. ~~**Play with somebody on a second machine.**~~ **Done 2026-11-03** - two machines in two
   different states played over a **Tailscale** tailnet and it worked amazing. No port forwarding
   and no firewall prompt, the address read straight off the menu, which labels and sorts a
   Tailscale address first. What is still unproved is a **stranger** hosting: they are not on your
   tailnet, so it is UDP 7777 forwarded or their own mesh VPN until SteamSockets. The host-leaves
   path is also still unplayed. See *Two machines, two states* in `docs/multiplayer.md`.
5. ~~A Niagara explosion effect for `BP_C4`. Without one the blast falls back to a debug sphere,
   which is invisible in a Shipping build.~~ **Done 2026-11-02, and not Niagara** - Niagara cannot
   be scripted at all, so it is `AGetawayBlast`: real additive geometry, a flash and glowing
   shards, in every build. The `ExplosionEffect` slot still overrides it. **Looked at and confirmed
   working in a Shipping build 2026-11-03**, along with the placement ghost and the HUD messages -
   see *The explosion* in `docs/look.md`.
6. **Finish the itch.io page.** The Shipping build is uploaded and the page is **restricted behind
   a password, intentionally** - two people have played it - at
   <https://rpidgey.itch.io/adrenaline>, but it has no cover image, no screenshots and no video,
   and none of the drafted text is in - so anyone let through the gate sees a blank page with a
   download on it. **A short video of movement at speed is the
   highest-value thing missing** - motion is the whole pitch, and no still can show a wallrun into a
   slide into a C4 jump. See `docs/itch.md` and `docs/itch-page.md`.
7. **Sound.** Still the biggest single improvement left per hour spent, and since
   *Music carries the game* (above) the order is short: **music first**, then the Music/SFX split,
   then the devices you trigger, then the round and UI stings, then the breakables. All ten movement
   slots are deliberately empty. The only sound wired is the C4 blast (`BP_C4` Explosion Sound =
   `c4_blast`, with `ATT_World` for falloff) and **it was heard for the first time 2026-11-07 and
   sounds great**, so item zero is done: the audio chain is proved end to end, which makes the
   devices (one property and one `PlaySoundAtLocation` each) the cheapest work on the list. The list, with the reasoning, is *What to do next, in
   order* in `docs/sound.md`.
   **Read the assets, not the notes**, before believing any claim about a slot being empty: the
   first draft of that list opened with three jobs that were already done.
8. **Logging in Shipping.** `bUseLoggingInShipping` is not set in `Source/Getaway.Target.cs`, so a
   packaged build writes no log at all and a player who hits a crash has nothing to send you. One
   line, and worth it before the page is opened up.
9. Before each new piece, check the *still unverified* / *not proved* notes in the matching `docs/` file for things I haven't played yet.
