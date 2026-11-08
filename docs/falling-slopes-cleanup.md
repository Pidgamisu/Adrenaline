# Falling deaths, grounding, slopes and round cleanup

> Moved out of CLAUDE.md on 2026-10-26. These are dated notes; if they disagree with the code or with CLAUDE.md, the code and CLAUDE.md win.

## Falling off kills you (2026-10-23)

Two things from the same play-test.

### The C4 readout said 2 when you held 1

Reported as *"one charge c4 works but says that you have 2 in your inventory"*. Nothing was wrong
with the count - the grant, the clamp and `ChargesHeld` were all correct, and one charge is what you
could throw. The HUD drew it as `C4  1 / 2`, held over the carry cap, and the cap is what got read.

It now draws **only what you are holding** - `C4  none`, `C4  x1`, `C4  x2  full` - so there is no
second number to misread, and the cap turns up as the one word that is worth knowing at a glance.
Dim at zero, amber once you have something. The pickup's own toast said `(1 / 2)` for the same
reason and now says `(carrying 1)`.

### Falling below the course ends your run

Asked for straight out: *"if you fall below the course you die."* It had never been handled at all -
the engine's own answer to a character under `KillZ` is `Destroy()`, which would have left the
controller holding no pawn, no death and no score, just a stuck camera. A fall was the last hole in
the round loop.

- **`AGetawayCharacter::FallDeathZ` (-100) is the plane**, measured at your **feet** in world Z, so
  it is one metre under a course whose floor is at zero. It is checked every frame in `Tick`, on the
  machine that owns the character only - the same bargain as spikes and breaking, where the victim's
  own eyes decide and the server decides what it means.
- **It goes through `Die(nullptr)`**, so a fall is exactly a spike: whatever dying costs you, falling
  off costs you the same thing. Since 2026-10-31 that is a trip back to the start after
  `DeathLingerTime` rather than the end of your run - see *Dying puts you back at the start* in
  `docs/spikes-and-death.md`. A null killer is already what `RegisterTrapKill` treats as nobody to
  credit, so **nobody is paid for your fall** - not even the player whose piece you were trying to
  clear.
- **Only while you are running.** `IsRunning()` is checked first, so a player still standing around
  during the finish banner is not told they have been killed.
- **`FellOutOfWorld` is overridden as well**, on both pawns. On the character it is the backstop: if
  something gets you past `FallDeathZ` it kills you properly instead of deleting you. On
  `AGetawayPlacementPawn` it does **nothing at all** - the fly camera has no collision and flying
  under the level to look up at what you are building is a normal thing to do, so the engine must not
  destroy the camera out from under you at `KillZ`.

**This changes the test course.** The catch floor at -300 and the 7-step staircases out of each pit
were there so a miss cost you a few seconds rather than the run, back when nothing could kill you.
At -100 you die on the way down and never touch them. **Asked for and confirmed 2026-10-23**, so
the pits are meant to be fatal now - but it is still one number to undo: set `FallDeathZ` below
-300 (or to a large negative) on the character and the forgiving course comes back. Nothing was deleted from the map.

Built clean. **Not played.**

## Everything has to touch something (2026-10-23)

Asked for: *"make it so you can only place objects if its touching another object."* A level is now
one connected structure you extend, not a scatter of blocks hanging in mid-air.

`AGetawayPlacementPawn::IsTouchingSomething` is the whole rule, and it is the mirror image of the
blocked test that was already there. `IsBlockedAt` takes the piece's world box, **shrinks** it by
`FitTolerance` (3 cm) and refuses an overlap; this takes the same box, **grows** it by
`ContactReach` (6 cm) and requires one. A piece sits flush against the face it was aimed at, so
whatever it is resting on is inside that skin; a piece in clear air finds nothing. The reach has to
stay above `FitTolerance` or the skin would sit inside the gap the other test leaves.

- **Any face counts, not just the bottom.** Butting a floor onto the side of a wall, or hanging a
  piece off a ceiling, is contact. Nothing has to be *under* it. This is a connectivity rule, not a
  gravity one - it is a game about building silly routes, and a floor jutting out of a wall is one
  of the good ones.
- **It is checked on the server too**, in `ServerPlacePiece`, not only on the client. The aim point
  is taken on trust, so this is the check that actually stops a piece being asked for in open sky;
  the client runs the same test only so the ghost agrees with what the server will do.
- **Surface-mounted pieces are exempt** - spikes, today. `IsFullySupported` already demands the
  whole footprint be backed, which is strictly stronger than one corner of contact, so running both
  would be the same question asked twice.
- **The ghost reads it.** Red whenever the spot is refused rather than only when it is blocked, and
  the HUD line under the crosshair says which it is: "Blocked - something is in the way" against
  "Nothing to build off - it has to touch something".
- The ground-plane fallback for aiming at nothing is still there and is now mostly moot: where
  there is real floor the trace hits it anyway, and where there is not, the ghost comes out red.
  Left in because a red ghost in the right place explains itself better than no ghost at all.

Worth knowing if it ever feels too strict: `ContactReach` is on the placement pawn in the Details
panel, and raising it lets you place with a small gap. Setting it to 0 does not turn the rule off -
clamp is 0.1 - so to go back to free placement, delete the two `IsTouchingSomething` calls.

Built clean. **Not played.**

## Things lie down on slopes (2026-10-26)

Asked for: *"can we make it so the ramp can have things placed on it slanted?"* Aim at a sloped
face and the piece tips onto it instead of sitting square and burying one corner. A floor becomes a
slanted platform, a wall leans, and a ramp carries on up the slope of the ramp under it.

- **Only sloped faces.** `SlantTolerance` (5°) is the gap: a face within that of a world axis - so
  every flat floor, wall and ceiling in the game - keeps exactly the old grid-pinned behaviour, and
  a ramp face at 18.4° tips the piece. Nothing that worked before is touched, which is the point of
  having a threshold rather than tilting onto everything.
- **Yaw first, then tip.** The piece is yawed the way you are looking as usual, and that whole frame
  is then rotated by the shortest turn from world up to the face normal. So **R** still means what
  it meant, and a ramp placed on a ramp points the way you are facing rather than taking some
  arbitrary spin about the normal.
- **Snapping solves for the slope instead of fighting it.** The centre is still snapped to the 50 cm
  grid on the two axes across the face, but the third is then *solved* so the centre sits exactly
  half a thickness off the face's own plane. Snapping all three would bury the piece or float it by
  whatever the slope rises across half a grid square - 8 cm on a ramp, which is more than
  `FitTolerance`, so half of all placements would have come back blocked.
- **Stacking compounds, on purpose.** A piece lying on a ramp presents its own sloped faces, so the
  next piece lies on *that*. Slanted staircases fall out of this for free.

### Only attached things follow a slope (2026-11-05)

Asked for: *"is it possible to make it so it slopes for objects that attach (spikes and jump pad)
but not objects like walls"*. Yes, and it cost almost nothing, because `ComputePlacement` was
already split the right way. It has three branches - surface-mounted, sloped face, flat face - and
the surface-mounted one turns the piece's own +Z out of whatever face it is stuck to without ever
consulting `SlantTolerance`. So **it was already doing the asked-for thing**, and only
*free-standing* pieces ever reach the sloped branch. The whole change is inside that branch.

- **`bSlantOnSlopes` on the placement pawn, default off.** Turning it on puts 2026-10-26 back
  exactly. Nothing new distinguishes the two kinds of piece: `bSurfaceMounted` is already a
  per-piece catalog flag, so this needed **no catalog change, no `dress_pieces.py` run and no
  Blueprint edit** - which matters, because `BP_GetawayGameMode` overrides the catalog outright.
- **Upright means yaw only**, so **R** keeps meaning what it meant.
- **It rests *on* the slope rather than cutting into it.** The slanted path solves the dominant axis
  so the centre sits half a thickness off the face's plane; the upright path keeps that solve and
  changes only the distance, to the upright box's reach along the face normal,
  `|N.x|*Hx + |N.y|*Hy + |N.z|*Hz`. That is exactly the distance at which the box's lowest corner
  touches the plane without crossing it. It has to be that rather than the aim point for the reason
  the *Snapping solves for the slope* bullet above already gives: `IsBlockedAt` forgives only
  `FitTolerance` (3 cm) of overlap, so a buried corner comes back blocked. Resting also satisfies
  `IsTouchingSomething`, since resting on something is touching it.
- **It degrades into the flat branch.** As the slope goes to zero, that reach becomes half the
  piece's height - exactly where the flat branch would have put the piece. So for a free-standing
  piece there is no longer a visible step in behaviour at `SlantTolerance`; the two paths meet.
- **The server needed no change, and got slightly stricter.** `ServerPlacePiece` already snapped an
  upright free-standing rotation back to a quarter turn and took a tilted one on trust. With this
  off nothing free-standing arrives tilted, so that trust is never exercised.

**The price of standing square**, and it is real: an upright box over a slope touches at one bottom
edge and lifts away across its footprint by the slope's rise over that distance. On an 18.4° ramp:

| Piece, as laid | Gap at the far end |
|---|---|
| Wall across the slope (50 cm along the rise) | 17 cm |
| Crate (200 cubed) | 67 cm |
| Wall or floor along the slope (400) | 133 cm |
| Collapsing bridge along the slope (1000) | 333 cm |

There is no third option: upright over a slope is either floating or buried, and buried is refused.
If the floating turns out to matter, the fix is to let `IsBlockedAt` ignore the single actor the
placement was aimed at, letting a piece sink into the ramp instead. That loosens a check which is
also the server's, so it was not done unasked.

**What this gives up:** *"a ramp carries on up the slope of the ramp under it"*, and *"slanted
staircases fall out of this for free"* - both from 2026-10-26. A ramp is free-standing, so a ramp
placed on a ramp now steps instead of continuing the slope. Offered and turned down on 2026-11-05;
if it is missed, the one-line version is `Def.bSurfaceMounted ||
Def.ActorClass->IsChildOf(AGetawayRamp::StaticClass())` as the branch condition.

### Played 2026-11-07: it works as intended

Signed off. Which answers the one real doubt in the decision - **the wedge of air under the downhill
end never read as broken.** 133 cm under a 400 cm floor on an 18.4 degree ramp is what the
arithmetic promised and it is simply not noticed in play, so `FitTolerance` refusing to bury a piece
instead was the right trade. `bSlantOnSlopes` stays off.

### Still unverified (upright on slopes)

- **A piece on a steep face**, where the normal's dominant axis is X or Y rather than Z. The solve
  is written for any normal, but every slope played so far is a ramp face at 18.4°.
- ~~**Stacking.**~~ Covered by the 2026-11-07 sign-off above: a piece standing square on a slope
  presents flat faces again, so the compounding that slanted stacking had is gone and stacking on a
  ramp behaves.
- **Two machines.** Nothing new crosses the wire - the client still sends a centre and a rotation
  and the server still re-tests both - but it has not been played.

### Oriented boxes, which this needed first

The blocked test, the contact test and the ghost all used `FGetawayPlacement::Size` - the piece's
**world-space** extent - with an identity rotation. That is exact while every piece is axis-aligned
and wrong the moment one is not: a tilted piece would have been tested and drawn as the fat upright
box that contains it. All three now take the piece's own `LocalSize` turned by its own rotation,
which is the same region as before for every axis-aligned piece and the right one for a tilted
piece. `DrawDebugBox` and `OverlapBlockingTestByChannel` both take a quaternion, so this cost
nothing but the signatures.

`WorldSizeFor` survives as the true axis-aligned bounds - each world axis summing a contribution
from all three of the piece's own axes, rather than the old rotate-the-extent-and-take-the-absolute
value, which is only correct for quarter turns. It feeds the free-standing grid pinning, where the
rotation is always a quarter turn, so that path is unchanged to the bit. This also closes the rough
edge logged under *Surface mounting* about spikes on the side of a ramp.

### What the server does with a tilted rotation

`ServerPlacePiece` used to snap every free-standing piece's rotation back to a quarter turn about Z
rather than trust what arrived. It still does **when the piece arrives upright**. A tilted one is
taken as sent, for the same reason a surface-mounted one always was: the server does not know which
face the client was looking at, so it cannot re-derive the rotation to check it against.

That is a real guard being narrowed, so, plainly: what it was protecting against is cosmetic. A
rotation on its own cannot put a piece anywhere - the **aim point** decides that, and the aim point
has been taken on trust since the spike. Everything that matters still holds on the server: the
piece must come out of the hand that player was dealt, it must not overlap anything, and it must
touch something.

Built clean. **Not played.**

## A bridge stays down, and C4 gets swept up (2026-10-26)

Two things asked for together: *"the placed c4 should clear at the end of each round and falling
bridges shouldnt reset mid round."* Both are about the same thing - what a round leaves behind.

### A collapsing bridge stays collapsed

`bRespawns` was on with a `RespawnDelay` of 6 s, so a bridge righted itself mid-run. That makes it
a four-second inconvenience rather than an obstacle: the player who dropped it could watch it fall
and then walk over it on the way past. It is **off by default now**, and the bridge stays down
until the next round - `StartPlacement` already called `Restore()` on every bridge, so the round
was always the right owner of that reset and nothing new was needed to bring it back.

The knob stays, for a bridge standing in a level with no round behind it.

**What this did need is a bridge that survives falling.** The old code read:

```
else if (CleanupDrop > 0.f && ...) { Destroy(); }
```

- that branch only ran for a non-respawning bridge, which until now was the unusual case. Make it
every bridge and the deck deletes itself on the way down, and the next round has nothing to put
back. It now **parks** instead: physics off, collision off, hidden, actor kept, and `StopFalling`
un-parks it when the round rights it. `FellOutOfWorld` is overridden to park as well, because a
deck that stays down for a whole round has plenty of time to reach `KillZ`, where the engine's
default is also `Destroy()`.

Parking is decided **per machine, above the authority check**, next to the rattle rather than in
the server-only state machine. The deck is deliberately not movement-replicated, so every copy
falls on its own and each one has to notice its own copy has gone far enough.

### Thrown C4 is cleared between rounds

`StartPlacement` now destroys every `AGetawayC4` in the level, alongside restoring the breakables
and the bridges. A charge is spent the moment it leaves your hand, so a brick still stuck to a wall
from last round is not a resource anyone can spend - it is a live bomb with no owner, and its blast
would land in a round nobody threw it in.

- **The charges you are carrying are untouched.** You went and collected those; they are yours
  across rounds exactly as before. This only sweeps up what is already stuck to the level.
- **`AGetawayC4Pickup` is a different class and is not swept** - a pickup is a placed piece, and
  pieces anyone placed stay.
- Server only, like everything else in `StartPlacement`, and `Destroy()` on a replicated actor
  reaches the clients by itself.

Built clean. **Not played.**
