# Playing on a controller

Added 2026-10-31. Keyboard and mouse are untouched: every pad binding is an extra key on an
existing action, or a second action that runs beside the mouse one. You can swap between the two
mid-round without doing anything.

All of it is C++. Input in this project is built at runtime in `AGetawayCharacter::BuildDefaultInput`
and `AGetawayPlacementPawn::BuildDefaultInput`, so there are no Input Action or Input Mapping Context
assets to make and nothing to wire up in the editor.

## The run

| Pad | Does | Keyboard twin |
|---|---|---|
| Left stick | Move | W A S D |
| Right stick | Look | Mouse |
| A | Jump. Hold it to auto-hop, press again in the air to double jump | Space |
| LB | Crouch, and a slide at speed | Left Shift / C / Left Ctrl |
| B | Crouch, second option | - |
| RB | Throw C4 | G |
| RT | Detonate | F |
| LT | Throw the grapple hook, and let go of it. Only on a Grapple round | Left click |
| D-pad Down | Restart | R |
| View / Back / Select | Give up the run, back to the placement camera | Backspace |

There is deliberately no pad key on `JumpTapAction`. The mouse wheel is on it so you can time hops
by flicking the wheel; holding A already auto-hops, which does the same job better.

## The placement camera

| Pad | Does | Keyboard twin |
|---|---|---|
| Left stick | Fly | W A S D |
| Right stick | Look | Mouse |
| RT / LT | Fly up / down, analogue | Space / C |
| LB | Fly faster | Left Shift |
| A | Place the piece | Left click |
| X | Turn it | R |
| D-pad Right / Left | Next / previous piece in your hand | Mouse wheel up / down |
| Start / Menu | Done building | Enter |

Start means the same thing Enter does: "I have finished building". It does not start your run on its
own - the round sets everybody off together once the last player is done, or when the build clock
runs out. See `docs/round-loop.md`.

## Why the stick does not use the mouse look code

A mouse reports a delta: "you moved N counts since the last frame". That is already frame-rate
independent, which is why `OnLook` can add it straight onto the control rotation.

A stick reports a position: "I am being held 60% of the way right". That is a rate. It has to be
turned into degrees per second and multiplied by the frame time, or your turn speed changes with
your frame rate - at 150 fps you would spin twice as fast as at 75.

So the right stick gets its own action and its own handler (`OnLookStick`) rather than being a
second key on `LookAction`. Feeding a stick through the mouse formula is the single easiest way to
get this wrong, and it looks fine on one machine and wrong on another.

The move stick is a separate action for a different reason: it wants a dead zone and a full-tilt
threshold, and applying those to the shared `MoveAction` would also reshape WASD. A keyboard
diagonal is already longer than 1 (W and D are each worth a full 1), and the full-tilt code would
squash it back to 1, quietly changing how the keyboard moves.

The shaping itself - radial dead zone, response curve, full tilt - lives in
`Private/GetawayStickInput.h`, shared by both pawns so the two sticks feel the same.

## Pitch sign: the stick is flipped and the mouse is not

Found by playing it, 2026-10-31. Both look handlers were written with the same pitch sign, which is
correct for the mouse and **inverted for the stick**: pushing the stick up looked down. Both stick
handlers now multiply `Stick.Y` by -1 before adding it to pitch, which also means `bInvertStickY`
adds a flip rather than being the flip.

This is the second sign trap in this project - the wallrun camera tilt was the first (see *Camera
effects* in `CLAUDE.md`). The lesson is the same both times: a rotation formula that works for one
input device or one engine cannot be copied across verbatim, because the axis conventions differ and
the result looks plausible until someone plays it. If you touch either `OnLookStick`, flip the sign
in both and check it on the pad, not by reading it.

## Tuning

All of it is `EditAnywhere`. For the run, open **Content/Blueprints/BP_Character**, select the top
row of the Components panel - the one named **BP_Character (Self)**, not `CharacterMovement` - and
look in the Details panel for **Getaway|Look (Pad)** and **Getaway|Move (Pad)**. These live on the
character, not on the movement component, same as `MouseSensitivity` and `BaseFOV`; the movement
values in `docs/blueprint-tuning.md` are the ones under `CharacterMovement`.

The placement camera has its own Blueprint, **Content/Blueprints/BP_PlacementCamera**, added
2026-10-31 for exactly this - before it, the camera spawned straight from the C++ class and nothing
on it could be tuned in the editor. Open it, select the top row of the Components panel, and its
values are under **Getaway|Look (Pad)**, **Getaway|Fly (Pad)** and **Getaway|Fly**.

It is created by `Content/Python/setup_tuning_blueprints.py`, which also points
`BP_GetawayGameMode`'s **Placement Pawn Class** at it. See `docs/blueprint-tuning.md`.

| Property | Default | What it does |
|---|---|---|
| `StickLookYawSpeed` | 230 (character), 150 (placement) | Degrees per second of turn at full stick |
| `StickLookPitchSpeed` | 180 (character), 110 (placement) | Same, up and down. Slower on purpose |
| `StickLookExponent` | 1.0 | 1 is linear - one straight line from nothing to full rate. Above 1, the inner half of the stick is slow and precise and the outer edge is fast |
| `StickLookDeadZone` | 0.05 | Radial dead zone. The travel past it is rescaled back to the full range |
| `StickLookRampTime` | 0 (off) | Turn spin-up, so small corrections do not snap |
| `StickLookRampStartScale` | 0.5 (character only) | What fraction of the full rate a flick starts at, when the ramp is on |
| `bInvertStickY` | off | Adds a flip. Separate from `bInvertMouseY` - see the pitch sign note above |
| `MoveStickDeadZone` | 0.12 | Radial dead zone on the move stick. Wider than the look stick's on purpose |
| `MoveStickFullTilt` | 0.9 (character only) | Push past this and it counts as pushed all the way. See below |
| `TriggerDeadZone` | 0.1 (placement only) | Dead zone on the up/down triggers |

The placement camera has no turn ramp and never did, whatever `StickLookRampTime` says - the field
is only on the character. A ramp means the first moments of a nudge move you less than you asked
for, which is wrong when you are lining a piece up with a grid square.

### The defaults aim at Apex Legends' 4 Linear

Asked for 2026-10-31. Apex's own **Advanced Look Controls** expose "Yaw Speed" and "Pitch Speed" in
**degrees per second**, which is the same unit as `StickLookYawSpeed` and `StickLookPitchSpeed`, so
a number read off Apex's settings screen can be typed straight in here. That is the one piece of
luck in this: no conversion, no guessing at a scale factor.

The rest is community data and should be treated as a starting point, not a match:

- **230 / 180 deg/s.** Apex's sensitivity levels 1-8 are not published as rates. The best anchor
  found was a player mapping Yaw Speed 260 to about look sensitivity 4.5 and Pitch Speed 200 to
  about the same, which works back to roughly 230 and 180 at level 4. One person's calibration.
- **Exponent 1.0.** This part is solid: Linear means a straight line from nothing to full rate, and
  that is what exponent 1 is.
- **Ramp off.** Apex's "Turning Extra Yaw" and its ramp-up time and delay are what Linear players
  set to 0, because a ramp makes the same stick position turn you at different rates depending on
  how long you have held it. `StickLookRampTime = 0` is the same choice.
- **Dead zone 0.05.** Apex's dead zone options are separate from the curve; Linear players run it
  small or off, because a wide dead zone throws away the slow end of the stick, which on a linear
  curve is the part you aim with.

**To check it rather than trust it:** hold the stick fully left and time a complete 360. At 230
deg/s that is about 1.57 seconds. Time the same spin in Apex at 4 Linear and compare. If Apex is
faster, raise `StickLookYawSpeed` by the ratio - the relationship is exactly linear, so if Apex
takes 1.3 s, multiply by 1.57 / 1.3. That measurement beats any chart, including the one above.

### `MoveStickFullTilt` and air strafing

Source air acceleration grows with how far sideways your input points. A keyboard key is always
worth a full 1, so A and D give you the strongest possible air strafe. A stick held 80% of the way
gives you noticeably less, which would make a pad worse at the game's core skill rather than just
different.

`MoveStickFullTilt` rounds the outer fifth of the stick up to 1, so a pad air strafes as hard as a
keyboard while keeping the analogue range underneath for walking. Set it to 1 for honest analogue
input.

### The project's own dead zone

`Config/DefaultInput.ini` used to apply a 0.25 dead zone to `Gamepad_LeftX`, `LeftY`, `RightX` and
`RightY`. Those are now 0, because they are **per axis**: a per-axis dead zone makes a diagonal push
clear two separate thresholds, which feels notchy near the corners. The radial dead zone in
`GetawayStickInput.h` replaces it and is the only one now. If you ever put a value back in the ini,
the two will stack.

## The HUD hints

The HUD's hint lines name whatever you are actually holding - "A places" on a pad, "Click places" on
a mouse. Each pawn tracks this itself in `bUsingGamepad`: any mouse movement sets it to keyboard,
and `GamepadProbeAction` - one spare action mapped to every pad key the game uses - sets it to pad.
The probe exists because most bindings share an action with their keyboard twin (space and A are
both the jump action) and an Enhanced Input handler is not told which key fired it.

The flag is local to each machine and does not replicate; it is only ever used for drawing text.

## Still unverified

- **Everything about feel.** The look speeds, the curve and the ramp are starting points, not tuned
  values. A pad has been in hand far enough to catch the inverted pitch, and no further.
- **Air strafing on a stick.** `MoveStickFullTilt` is the right idea in theory. Whether 0.9 is the
  right threshold, and whether a pad can actually hold a Source air strafe, is a play test.
- **Two players, one PC, one pad.** Unreal's editor play-in-editor gives both windows the same
  input device by default, so a keyboard player and a pad player on one machine is the awkward case.
  Two machines is the real test. See `docs/multiplayer.md`.
- **Which trigger should be up.** RT up / LT down matches flight and build cameras elsewhere, but
  the opposite is just as defensible.
- **The slide button.** LB and B both crouch. If one of them turns out to be clearly right, drop the
  other rather than keeping a redundant binding.
