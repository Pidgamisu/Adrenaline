# Getaway Movement (Unreal Engine 5 plugin)

Port of the Getaway Movement Lab browser prototype. Source-style first-person movement, throwable C4 and simple breakables, as a drop-in C++ plugin. Written for UE 5.3 to 5.5. Single-player.

What's in it:

| Class | What it does |
|---|---|
| `UGetawayMovementComponent` | Source friction and acceleration, air strafing, bunny hopping (auto-hop, jump buffer), double jump, slide with boost, wallrun (wall must be beside you), wall kick, mantle that keeps 75% of your speed, smashing breakables at speed, shoving loose scraps |
| `AGetawayCharacter` | First-person player. Input is created in code, so no input assets are needed. Camera effects: wallrun tilt, speed FOV, shake, landing kick, mantle jiggle |
| `AGetawayC4` | Thrown charge that carries your momentum, sticks to surfaces, blinks, launches you on detonation and bursts breakables |
| `AGetawayC4Pickup` | Floating charge on a pad that restocks your C4, then respawns after `RespawnDelay` |
| `AGetawayBreakable` | Panel or crate that bursts into loose physics scraps when hit fast or blasted |
| `AGetawayPiece` | A plain solid block for floors, walls and ledges. Scale it to size |
| `AGetawayRamp` | A wedge you run up, rising along its own +X. The mesh is built in code, so it needs no wedge asset |
| `AGetawayCollapsingBridge` | A long deck that holds for `CollapseDelay` once you stand on it, then drops under physics and comes back after `RespawnDelay` |
| `AGetawaySpikes` | A bed of spikes that kills whoever touches it. Mounts flat on the floor, wall or ceiling you aim at |
| `AGetawayJumpPad` | A pad that throws you out of whatever it is stuck to, at `LaunchSpeed`. Mounts flat the same way, so a wall pad fires you sideways |
| `AGetawayVortex` | An anti-gravity column that holds you climbing at `LiftSpeed` for as long as you are inside it, while you keep full steering |
| `AGetawayPlacementPawn` | The placement-phase fly camera: aim at a surface, drop the piece in your hand |
| `AGetawayHUD` | Crosshair, speedometer in the lab's units, movement state, C4 count, the dealt hand while placing |
| `AGetawayGameMode` | Uses the character and HUD above, owns the placement / run phases and the piece catalog |

## Setup

1. **You need a C++ project.** If your project is Blueprint-only, open it and use *Tools > New C++ Class > None > Create Class* once. That converts it; close the editor afterwards.
2. Copy the `GetawayMovement` folder into your project's `Plugins` folder (create `Plugins` next to your `.uproject` if it doesn't exist). You should end up with `YourProject/Plugins/GetawayMovement/GetawayMovement.uplugin`.
3. Right-click your `.uproject` and choose *Generate Visual Studio project files* (or the Rider / Xcode equivalent).
4. Open the solution and build the *Development Editor* target, or just double-click the `.uproject` and let the editor build the plugin when it asks.
5. In the editor, check *Edit > Plugins* that **Getaway Movement**, **Enhanced Input** and **Niagara** are enabled.
6. Check *Project Settings > Input > Default Classes*: Default Player Input Class should be `EnhancedPlayerInput` and Default Input Component Class `EnhancedInputComponent`. These are the defaults in new UE5 projects.
7. Open a level, then in *World Settings* set **GameMode Override** to `GetawayGameMode`. Press Play.

To tweak values in the editor, make a Blueprint child of `GetawayCharacter` (right-click the class in the Content Browser's C++ Classes folder > *Create Blueprint class based on GetawayCharacter*) and edit the **Getaway** categories in the Details panel. Movement values are on the *CharacterMovement* component.

To make the game spawn it you also need a Blueprint child of `GetawayGameMode`, with its **Default Pawn Class** set to your character Blueprint. A Blueprint game mode is not optional here: the editor greys out Default Pawn Class whenever the selected game mode is a C++ class. Set that Blueprint as the GameMode Override in World Settings.

## Controls

| Key | Action |
|---|---|
| W A S D | Move |
| Space / mouse wheel | Jump. Hold Space to auto-hop. Press again in the air to double jump |
| C / Left Shift / Left Ctrl | Crouch. At speed it becomes a slide, which can't start again for 1.5 s after the last one ends |
| A/D + mouse in the air | Air-strafe to gain speed and curve |
| Run along a wall beside you | Wallrun, Space to kick off |
| W into a ledge mid-jump | Mantle |
| G, then F | Throw C4 (two carried at a time), detonate. A thrown charge is spent: restock at an `AGetawayC4Pickup` |
| R | Restart the level |

A controller works too, keyboard and mouse untouched - swap between them mid-round. Left stick
moves, right stick looks, A jumps, LB or B crouches and slides, RB throws C4 and RT detonates.
The placement camera flies on the left stick with the triggers for up and down, A places, X turns.
Full map and the tuning in `docs/controller.md`.

## Building a test course

- Use plain static meshes (cubes, BSP) for floors, walls and ledges. They never break.
- Drag **GetawayBreakable** in from the C++ Classes folder for panels and crates, scale it, and give its mesh a material (amber in the lab).
- Wallruns need walls at least 1.2 m tall, roughly vertical, beside your path.
- Mantles work on ledges 30 to 170 cm above your feet.

## Values carried over from the lab

Unreal works in centimetres; the lab showed Source units (u), where 40 u = 1 m, so **1 u = 2.5 cm**. If you changed sliders in the lab's tuning bench, multiply any u/s value by 2.5 to get cm/s.

| Lab setting | Lab value | Unreal property | Value |
|---|---|---|---|
| Run speed | 320 u/s | RunSpeed | 800 |
| sv_accelerate | 10 | GroundAccelerate | 10 |
| sv_friction | 5 | GroundFrictionSource | 5 |
| sv_stopspeed | 100 u/s | StopSpeed | 250 |
| sv_airaccelerate | 12 | AirAccelerate | 12 |
| Air wishspeed cap | 30 u | AirSpeedCap | 75 |
| sv_gravity | 800 | GravityScale | 2.04 (with default world gravity -980) |
| Jump impulse | 300 u/s | JumpSpeed | 750 |
| Air jumps | 1 | AirJumps | 1 |
| Slide entry boost | 100 u/s | SlideBoost | 250 |
| Slide friction | 0.30 | SlideFriction | 0.3 |
| Speed to start slide | 220 u/s | SlideMinSpeed | 550 |
| Wait between slides | - | SlideCooldown | 1.5 s (0 = slide whenever you like) |
| Wallrun gravity scale | 0.18 | WallRunGravityScale | 0.18 |
| Wallrun max duration | 1.60 s | WallRunMaxTime | 1.6 |
| Kick-off push | 220 u/s | WallKickSpeed | 550 |
| Attach range | 0.60 m | WallAttachRange | 60 |
| Mantle speed kept | 75% | MantleSpeedKept | 0.75 |
| Smash speed | 360 u/s | SmashSpeed | 900 |
| Sensitivity | 2.20 | MouseSensitivity (character) | 2.2 |
| Pad turn rate | Apex 4 Linear | StickLookYawSpeed / StickLookPitchSpeed (character) | 230 / 180 deg/s |
| Pad stick curve | Apex Linear | StickLookExponent (character) | 1.0 |
| Pad dead zone | - | StickLookDeadZone / MoveStickDeadZone (character) | 0.05 / 0.12 |
| Pad turn spin-up | Apex extra yaw off | StickLookRampTime (character) | 0 (off) |
| Pad full-tilt point | - | MoveStickFullTilt (character) | 0.9 |
| FOV | 100 | BaseFOV (character) | 100 |
| C4 blast boost | 600 u/s | C4BlastBoost (character) | 1500 |
| C4 blast radius | 6.0 m | C4BlastRadius (character) | 600 |
| C4 throw speed | 600 u/s | C4ThrowSpeed (character) | 1500 |
| C4 momentum carried | 100% | C4MomentumCarried (character) | 1.0 |
| C4 carried at once | 2 | MaxCharges (character) | 2 |
| C4 you spawn with | - | StartingCharges (character) | 0 |
| C4 given by a pickup | - | ChargesGranted (pickup) | 1 |
| C4 pickup respawn | - | RespawnDelay (pickup) | 8 s |
| Speed ring boost | - | BoostScale / BoostTime (speed ring) | 1.5x for 3 s |
| Speed ring snap | - | bSnapToSpeed / SnapMinSpeed (speed ring) | on / 300 |
| Speed boost FOV | - | BoostFOV (character) | 9 deg |

Presets from the lab, applied to the movement component:

- **Classic Source:** SlideBoost 0, SlideFriction 0.8, SlideMinSpeed 650, WallRunGravityScale 0.35, WallRunMaxTime 0.9, WallKickSpeed 450, AirJumps 0
- **Apex-like:** RunSpeed 750, GroundAccelerate 12, GroundFrictionSource 6, AirAccelerate 5, GravityScale 1.84, JumpSpeed 700, SlideBoost 220, SlideFriction 0.25, SlideMinSpeed 500, WallRunGravityScale 0.3, WallRunMaxTime 1.2, WallKickSpeed 500
- **Titanfall-like:** RunSpeed 850, GroundAccelerate 12, AirAccelerate 22, AirSpeedCap 90, GravityScale 1.73, JumpSpeed 720, SlideBoost 300, SlideFriction 0.18, SlideMinSpeed 500, WallRunGravityScale 0.1, WallRunMaxTime 2.2, WallKickSpeed 650
- **Surf server:** AirAccelerate 100, GroundFrictionSource 4, SlideBoost 100

## What the Blueprints actually play at (2026-11-05)

The table above is the **C++ default**. `BP_Character` and `BP_GetawayGameMode` override some of it,
and the Blueprint is what you actually play. These were read out of the assets on 2026-11-05 and
confirmed deliberate. Anything not listed here is the C++ default.

| Property | C++ | Blueprint |
|---|---|---|
| AirJumps | 1 | **0** - no double jump |
| AirSpeedCap | 75 | 35 |
| JumpSpeed | 750 | 700 |
| GroundFrictionSource | 5.0 | 5.5 |
| SlideCooldown | 1.5 | 0.5 |
| SlideBoostCooldown | 1.0 | 0.5 |
| SlideMinSpeed | 550 | 500 |
| SlideStopSpeed | 280 | 260 |
| WallAttachRange | 60 | 50 |
| WallKickSpeed | 550 | 600 |
| WallRunMinSpeed | 250 | 260 |
| MantleDuration | 0.22 | 0.30 |
| MantleMinHeight | 30 | 50 |
| SmashSpeed | 900 | 1000 |
| FootstepStride | 340 | 250 |
| FootstepMinInterval | 0.3 s | 0.0 |
| FallDeathZ (character) | -100 | -200 |
| HandSize (game mode) | 3 | 2 |
| GoalPoints (game mode) | 1 | **0** |
| bNoPointsIfAllFinish (game mode) | true | **false** |
| bStartInMenu (game mode) | true | false, so Play goes straight to placement |

Three of those turn documented behaviour *off* rather than tuning it: **AirJumps 0** removes the
double jump, and **GoalPoints 0** with **bNoPointsIfAllFinish false** removes Ultimate Chicken Horse
scoring. They live in the Blueprint, not the C++, so clearing the override in the Details panel puts
each one back.

**Reading them back.** These values are inside a `.uasset`, which nothing but the editor can read -
which is how a hand-set value drifts from what the docs claim without anyone noticing. Run
`Content/Python/export_tuning.py` to dump them again, either from Tools > Execute Python Script or
headless with the editor closed:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "<path>\Getaway.uproject" -run=pythonscript ^
  -Script="<path>\Content\Python\export_tuning.py" -unattended -nopause -nosplash
```

It compares each Blueprint's class default object against the native class's and prints only what
differs, so the list stays short as the C++ defaults move.

That command is written for **cmd.exe**. PowerShell strips the inner quotes before the engine
sees them, and because this project has a space in its path, the script path then arrives
truncated at that space - the commandlet reports *"Could not load Python file"* and exits 255.
Run it from cmd.exe. Escaping the quotes for PowerShell was tried and did not help.

## Notes

- **Mouse:** look uses the Source formula (Sensitivity x 0.022 degrees per mouse count), so it should feel like the lab and CS. Unreal pre-scales mouse input by its Axis Config sensitivity (0.07 by default) and the character divides that back out using *Mouse Input Scale*. If turning feels far too fast or slow, that's the setting to adjust. If up and down are reversed, tick *Invert Mouse Y*.
- **Wallruns:** once you leave a wall you can't grab the same wall again until you land (another wall is fine). This stops endless climbs up one wall.
- **C4 is a resource:** a thrown charge is spent for good - detonating it doesn't give it back. You spawn with `StartingCharges` (0 by default) and restock by running through an `AGetawayC4Pickup`, which hands over `ChargesGranted` and comes back `RespawnDelay` seconds later. Set `StartingCharges` to `MaxCharges` if you'd rather always carry a full load.
- **C4 effects:** with no Niagara effect assigned, a debug sphere flashes on detonation (editor and development builds only). Assign `ExplosionEffect` and `ExplosionSound` on a Blueprint child of `GetawayC4`, then set it as the character's `C4Class`.
- **Not networked:** the movement doesn't use saved moves, so it isn't client-predicted. Fine for single-player.
- **Pieces:** the party box deals from `AGetawayGameMode::Catalog` - floor, wall, ledge, breakable panel, crate, ramp, collapsing bridge, C4 pickup, spikes, jump pad, vortex. The spikes and the jump pad are *surface mounted*: `bSurfaceMounted` makes the piece lie flat on the face you aim at with its own +Z out of it, and it cannot be placed in mid-air. For the jump pad that +Z is the launch direction, so the surface you choose is the direction it fires. The ramp and the bridge are *directional*: `bDirectional` on the piece makes the placement ghost draw an arrow, and the piece's own +X is the way a ramp rises and a bridge drops, which is the way you are looking until you press R. The vortex is neither: it is a free-standing column that lifts straight up however it is placed, and it is the only piece whose effect is *sustained* rather than instant - the height you get out of it is decided by how long you stay in it.
- **Ramps need no asset:** `AGetawayRamp` builds its wedge in `OnConstruction` with `UProceduralMeshComponent` - six vertices and a convex hull, filling a 100 cm cube so the catalog can scale it like any other piece. The hull is deliberately used for simple collision (`bUseComplexAsSimpleCollision = false`) so the slope reads as one flat plane to the movement component.
- **Not ported yet:** the in-browser level editor, share codes, run timer and checkpoints. The Unreal editor replaces the level editor; the timer and checkpoints are easy next additions.
- **Building destruction:** for Finals-style buildings later, Unreal's Chaos Destruction (Geometry Collections with fracture levels and connection graphs) is the right tool, rather than extending `GetawayBreakable`.
