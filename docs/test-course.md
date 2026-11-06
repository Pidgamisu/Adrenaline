# The test course

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

## The test course (`/Game/Maps/TestCourse`)

Built 2026-10-15 and set as both the editor startup map and the default game map, so opening the
project and pressing Play drops you straight into it. It is a linear course along +X; every distance
comes from the plugin's own tuning values, so each obstacle is sized to *require* the mechanic it
tests rather than merely allow it.

| X (cm) | What | Why that size |
|---|---|---|
| -400..2300 | Runway | Long enough to reach RunSpeed 800 before anything asks for it |
| 1700 | Slide bar, gap 110..230 | Clears a crouched player (100 tall), stops a standing one (180) |
| 2300..3700 | Wallrun gap, walls 400 tall both sides | 1400 across - a double jump reaches only ~900, so the wall is the only way over |
| 3700..5600 | Mantle ledges at 60 / 110 / 165 | Spans the MantleMinHeight 30 .. MantleMaxHeight 170 window |
| 5600..6900 | Two breakable panels, then a 5-crate stack | **Unbreakable on foot since 2026-11-06.** One threshold, `SmashSpeed` **1100**, above a sprint (800), a wallrun (920) and even a slide out of a full run (1050) - so these now need a jump pad, a speed ring, a slide down a ramp or C4. They used to break on any slide, on a separate 495 threshold against `SmashSpeed` 900. The course has no pad or ring standing in it, so **this obstacle is currently a dead end unless you place one** |
| 6900..9050 | Three jump platforms, offset sideways | Gaps of 450 with a 500 lateral shift - needs air control to swing sideways mid-jump, not just a hop. Note `AirJumps` is **0** on `BP_Character`, so the double jump this was built around is currently off; and since 2026-11-06 air input steers without gaining, so the sideways shift is easier while the distance is not |
| 9050..9900 | Breakable blast wall, then a ledge at 500 | 500 is above a double jump (~280), so the only way up is a C4 rocket jump |

Colour coding follows the browser lab: cyan = wallrun surface, green = mantle ledge, amber = slide /
finish, orange = breakable, dark grey = ordinary solid.

### What is actually in the map (read 2026-11-01)

The table above is what the course was *built* as. The map has been edited by hand since, and it no
longer matches. Read out of `/Game/Maps/TestCourse` with a headless Python dump of every actor's
bounds, so these are the numbers the traces meet:

| Actor | X | Y | Z |
|---|---|---|---|
| `Floor_Start` | -400..2300 | ±400 | -50..0 |
| `SlideBar` | 1620..1780 | ±400 | 110..230 |
| `WallRun_L` / `_R` | 2300..3700 | -440..-400 / 400..440 | 0..400 |
| `Floor_Mantle` | 3700..5600 | ±400 | -50..0 |
| `Panel_1` (breakable) | 4767.5..4792.5 | ±400 | 0..200 |
| `Panel_2` (breakable) | 5466.4..5491.4 | -402..398 | -1.6..198.4 |
| `Panel_3` (breakable) | 5466.4..5491.4 | -402..398 | 198.4..398.4 |
| `Floor_Break` | 5600..6900 | ±400 | -50..0 |
| `Panel_4` (breakable) | 6126.4..6151.4 | -402..398 | -1.6..198.4 |
| `Jump_1`..`Jump_4` | 7100..7400, 7800..8100, 8297.7..8597.7, 8780..9080 | 600 wide, shifted each time | 0..50, 37.5..87.5, 132.2..182.2, 267.5..317.5 |
| `RocketLedge_500cm` | 9500..9800 | ±300 | 470..500 |
| `Finish` | 9500..9800 | ±150 | 500..900 |

Differences from the table above, all of them things to know before writing a test against this map:

- **The three mantle ledges are gone.** `Floor_Mantle` is 1900 cm of empty floor with `Panel_1`
  standing in the middle of it. There is nothing in the map built to be mantled.
  **Put them back** with `Content/Python/restore_mantle_ledges.py`: open the map, Tools > Execute
  Python Script..., pick the file, then Ctrl+S if you like where they landed (the script saves
  nothing itself). It adds `Ledge_60` at X 3850..4150, `Ledge_110` at X 4250..4550 and `Ledge_165`
  at X 5000..5300, each 300 cm deep and the floor's full 800 cm across, tops at 60 / 110 / 165, in
  the green `M_Course_Ledge`. The gaps dodge the panels: 217 cm clear before `Panel_1` and 166 cm
  after the last ledge. 300 cm deep is deliberate - the landing capsule has room to stand on top, so
  these test the ordinary mantle rather than the thin-ledge path added the same day. Re-running it
  moves the ledges instead of duplicating them, so the table in the script is editable.
- **There is no crate stack and no breakable blast wall.** Every breakable in the course is the same
  piece: 800 wide, 200 tall and **25 cm thick**. `Panel_2` and `Panel_3` are stacked into one 400
  tall wall; `Panel_1` and `Panel_4` stand alone at 200.
- **There is no C4 pickup actor**, so the section below about `C4Pickup_1` at X=6100 is stale too and
  the rocket-jump ledge cannot be reached on charges collected in the map. Set the character's
  `StartingCharges` to test the blast.
- There are **four** jump platforms, not three, and the last one at 267.5..317.5 is a long way under
  the 470 rocket ledge.
- The 25 cm thickness is why no breakable in this map could be mantled until 2026-11-01; see *Fixed:
  a thin ledge could not be mantled* in `docs/placement.md`.

A catch floor runs under the whole course at z=-300 so a miss never drops you into the void, and
7-step staircases (40 cm rise, under the 50 cm MaxStepHeight) climb out of each pit at its far end.
Falling used to cost a few seconds and skip that obstacle instead of ending the run, which also
meant someone who could not wallrun yet could still reach the breakables and the C4 section to test
them. **As of 2026-10-23 you die before you get there**: `FallDeathZ` is -100 and the catch floor is
at -300, so the floor and its staircases are now a backstop nothing reaches. See *Falling off kills
you* at the end - that is one number on the character if the forgiving version is wanted back.

**The C4 pickup is at X=6100** (`C4Pickup_1`, an `AGetawayC4Pickup`, added 2026-10-15). It sits on the
floor in the gap between the two breakable panels, straight in the running line, so a slide-smash run
through Panel_1 collects it on the way past. It grants 1 charge (2 until 2026-10-19) and comes back
8 s after being taken; one charge is still a rocket jump, so the ledge at 9050 is still clearable;
while depleted only the ground pad is left, with the floating brick and its light hidden.

It is there because **C4 is now a collectable resource**: you spawn with `StartingCharges = 0`, so the
rocket-jump ledge at 9050 is only reachable if you picked the charges up first. That makes the pickup
part of the course rather than decoration - but it also means the C4 section can't be tested at all if
the pickup fails to trigger. If that happens, set the character's `StartingCharges` to 2 in the
Details panel to carry on testing the blast itself.

The level was generated by script rather than by hand; the scripts are not kept in the project, so
edit the map in the editor from here on.

### Rebuilding or extending it

`Config/DefaultEngine.ini` points `GameDefaultMap` and `EditorStartupMap` at it, and the level's own
World Settings carry a `GetawayGameMode` override plus `KillZ = -2000`. `PythonScriptPlugin` and
`EditorScriptingUtilities` were enabled in `Getaway.uproject` to allow script-driven level editing;
nothing else depends on them, so they can be turned off if unwanted.

### Fixed after the first play-test (2026-10-15)

- **Wallrun camera tilted the wrong way.** The port copied the lab formula including its sign; see the
  roll-convention note under *Decisions already made*. The slide tilt shared the same root cause and
  was flipped too, though it was not reported.
- **Mantling almost never caught in mid-air.** The ledge was found by a single ray at feet + 35 cm, so
  the ledge top had to clear that before the wall was seen at all - on the 60 cm ledge that is a
  window of feet < 25 cm, roughly two frames of a jump. It now probes up the capsule and takes the
  lowest wall, restoring the full 30..170 window. The climb-clearance sweep was also widened from
  2 cm of slack to half capsule width, since it could report a blocking hit on the very wall being
  climbed.

Mantle still deliberately requires you to be airborne, matching the lab (`!P.onGround`): you jump
into a ledge rather than walking at it. That is a design decision, not a bug.

### Verification status

Both fixes above were play-tested and confirmed good, so the course's movement side is settled.

The C4 chain is confirmed too (2026-10-16): the pickup catches a player at speed and the blast
clears the 500 cm rocket ledge. The whole course is now completable.
