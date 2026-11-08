# To do

Started 2026-11-07 from `CLAUDE.md`'s *Next steps* plus the **Still unverified** section of every
other file in `docs/`. **Rewritten the same day**, after the play-test backlog was played in one
sitting: seventeen of the eighteen items came back with a verdict, and this file now records what is
left rather than what was never looked at.

**What changed in a day.** The project's main debt was code nobody had played. That is mostly paid:
the grapple, both pickups, the one-item hand, the one-button hand, the windmill plates, pieces on
slopes, the ramp, the bridge, the HUD, the controller, the front end's last button and the C4 blast
have all now been on screen. Two came back as work - the jump pad's friction window and the pad
camera - one came back as an open design question, and one was not tested at all.

Tick an item by playing it and either signing it off or saying what to change.

---

## 1. Decided and not yet written

- [ ] **Retune the pad camera.** *"Feels bad, will change later."* No specifics came back, so this
      is a Details-panel pass with a pad in hand, not a build. The order to try, with what the
      defaults actually are: `StickLookYawSpeed` 230 / `StickLookPitchSpeed` 180, then
      `StickLookExponent` **1** (linear - there is no curve at all right now), then
      `StickLookRampTime` **0** (acceleration is off), then `StickLookDeadZone` 0.05. All in
      **Getaway|Look (Pad)**. See `docs/controller.md`.

## 2. Open questions

- [ ] **Should effects become pieces?** *"Honestly I think i should get rid of effects and just have
      objects that give you that effect, might change in the future."* Not a decision, and nothing
      should be built on the round layer until it is settled. Two of the four effects already have
      their piece (the C4 pad and the grapple pad), and the other two are one property each -
      `AirJumps` and `GravityScale` - so the work is the catalog, the hand and the balance, not the
      abilities. What would be lost is that both players currently get the same thing without
      anybody spending a piece on it. `bRoundEffects` off already lets the two be played against
      each other. See *Effects may become pieces* in `docs/effects.md`.
- [ ] **Make a pickup land.** Asked for as "a notification for picking up" - and **both pads already
      notify**, so this is prominence, not a missing feature: the lines fire for a second or so into
      the message stack at the exact moment you are least able to read anything. Cheapest fix first:
      a flash on the hand readout, a centre card like the effect banner, or the pickup sting from
      `docs/sound.md`, which is arguably the right answer since a pickup is exactly the rare
      deliberate event that decision reserves sound for. See *A pickup that tells you nothing* in
      `docs/hud.md`.
- [ ] **Make both pickup pads readable.** *"The grapple pickup isn't very clear but neither is the
      c4, they will be edited later."* One job across both pieces - two cyan-and-dark discs on the
      floor that mean different things - and deliberately deferred. Levers are the pad material and
      the thing turning over it, both in the Details panel. `docs/placement.md`.
- [ ] **The build-phase HUD is "a little cluttered, might change later but feels okay for now."**
      Left alone deliberately. Note it is the same verdict the run's HUD got before being cut back
      twice, and the cut-backs are what made the run readable - so if it gets a pass, take things
      out rather than shrink them. `docs/hud.md`.

## 3. Still unplayed

- [ ] **Finishing and the watch camera.** Came back *"untested"*, and it is now the largest unplayed
      thing in the game and the only one left that can change what happens at the end of a round:
      **YOU MADE IT** with your time and place, the 2 s delay, then the camera lifting your body
      away. Also unplayed: the compact line's position against the placement HUD, a second finisher
      getting `2nd`, and a finisher who falls in those two seconds. Worth doing deliberately -
      finishing means actually crossing the line, which is easy to avoid for weeks while testing
      pieces. `docs/round-loop.md`.
- [ ] **The anti-gravity vortex.** Not on the play-test list and still never played: whether 1000 is
      the right climb for a 5 m column, whether 400 of steering is too loose, whether shooting out
      of the top beats cresting, and whether the spinning cage reads as a vortex at all.
      `docs/placement.md`.
- [ ] **The placement ghost as lit geometry.** Still unplayed, and the one question the ramp and
      bridge sign-off left open: whether a see-through box reads against a level made of the same
      additive pink panes, and whether cyan and red still work now that it is a surface rather than
      a 3 px line. `docs/placement.md`.
- [ ] **The effect banner in its build-phase home** - the 2 s hold and the travel to the corner. No
      verdict, and no point tuning it until the question above is settled. `docs/effects.md`.
- [ ] **A material on a piece in play**, the ramp's procedural-mesh slot especially, since its timing
      is the only one that could be wrong. `docs/blueprint-tuning.md`.
- [ ] **The spikes' overhang trim** and the rebuilt point layout. The attachment half of that work
      was played on a windmill plate; the trim was not. `docs/spikes-and-death.md`.
- [ ] **A piece on a face steeper than 18.4 degrees**, where the normal's dominant axis is not Z.
      `docs/falling-slopes-cleanup.md`.
- [ ] **Two machines**, for everything built since 2026-11-03: the grapple (including a rival's rope
      drawing at all), the windmill's derived spin, pieces square on slopes, the one-button hand.
      Each is reasoned to be safe and none has been watched. `docs/effects.md`,
      `docs/placement.md`, `docs/falling-slopes-cleanup.md`.

## 4. Small jobs with a clear end

- [ ] **Logging in Shipping.** One line - `bUseLoggingInShipping` in `Source/Getaway.Target.cs` - and
      without it a packaged build writes no log, so a player who crashes has nothing to send.
- [ ] **A Blueprint child for the HUD.** The game mode sets `HUDClass` in C++, so not one HUD knob
      is reachable from the Details panel - which matters more now that the Details-panel round trip
      is proved to work. Six clicks, and it fixes `docs/effects.md`'s same complaint too.
- [ ] **The Music / SFX Sound Class split.** A music-led game with no volume slider is one players
      mute. Cheapest now, most expensive later. `docs/sound.md`.
- [ ] **A slide-cooldown tell near the crosshair.** The debt the no-foley decision took on: every
      other piece of movement feedback has a visual and nothing says when your next slide is ready.
- [ ] **Apply the logo in the game.** The exe icon and both splash screens are done; the menu still
      spells ADRENALINE out as text. `docs/look.md`.
- [ ] **Clear `BP_Character`'s `SmashSpeed` override**, redundant at 1100 either way.
- [ ] **Close the two GlowGrid Starter license items in `CREDITS.md`.** The pack does not ship, so
      this is tidiness.

## 5. The itch.io page

The build is up and patching works; the page is a blank gate with a download behind it.

- [ ] **A short video of movement at speed.** Still the highest-value thing missing anywhere - and
      easier to shoot now, because the grapple reads as a slingshot and everything in a run has been
      played at least once.
- [ ] **Paste the text in.** All drafted in `docs/itch-page.md`; none of it is on the page.
- [ ] **Cover image and screenshots.** The logo is the obvious cover.
- [ ] **Download and run a build from the page**, as opposed to the folder on this machine. Would
      also answer whether the itch app honours `.itch.toml`.
- [ ] Decide when the password comes off. Restricted is deliberate for now.

## 6. Sound, in order

From *What to do next, in order* in `docs/sound.md`. **Item zero is done** - the C4 blast was heard
2026-11-07 and sounds great, which proves the whole audio chain, so item 3 is now the cheapest work
on this list rather than a leap of faith. All ten movement slots stay deliberately empty.

- [ ] Music: a track per phase. The one item with real unknowns left - the track must be owned by
      something that survives a round, or it restarts every time the level is rebuilt.
- [ ] The Music/SFX split (also above).
- [ ] The devices you trigger: jump pad, speed ring, both pickups, the grapple. One property and one
      `PlaySoundAtLocation` each, against a chain now known to work. A pickup sting would also close
      the "notification for picking up" job in section 2.
- [ ] The round and UI stings: build clock running out, GO, **YOU MADE IT**, dying.
- [ ] Breakables bursting.
- [ ] `ATT_World`'s falloff, which still has not been judged - the blast was heard at your own feet.

## 7. Features

- [ ] **More traps.** Check any new piece against *Every piece must grant ability or test precision*
      in `CLAUDE.md` **before** writing code. The jump pad's friction window is the newest worked
      example of getting it wrong in a smaller way than the void did: it changed the rules for three
      seconds after you touched it instead of changing what you could do at the moment you did.
- [ ] **A per-round deck** for the party box. One normal and one special landed 2026-11-06; drawing
      from a deck rather than at random is what is left.
- [ ] **A lobby, and a settings page.** Neither exists, and a lobby has never been the thing that
      broke.
- [ ] **Pad support for the menus.** A controller cannot drive the front end or the HUD at all - and
      with the pad camera needing a pass anyway, these two are the same afternoon.

## 8. Signed off

Here so none of it gets re-opened. Each one is written up where it lives. Everything is from
2026-11-07 except the jump pad, which was played on the 8th.

| What | Verdict |
|---|---|
| **The jump pad, stronger** (2026-11-08) | *"The pad feels much better, this was definitely the better fix."* `LaunchSpeed` 1800, window off, no retune |
| One button for the whole hand | *"Works."* Committed `5cd97af` |
| The grapple hook | *"Plays very well and feels like a slingshot, which is the intended effect."* No retune |
| The one-item hand | *"One c4 was intentional and feels good."* `ItemCapacity` 1 stands |
| Pickups returning on death | Works, and no loop: *"theres never a reason to intentionally die since you want to go fast"* |
| Spikes and pads riding a windmill plate | Works as intended |
| Pieces standing square on a slope | Works as intended, gap and stacking included |
| The ramp and the collapsing bridge | Feel correct - closes the 0.45 s hold and the tip impulse |
| The run's HUD | Readable at speed. The flattening worked |
| The HUD's frame cost | Nothing visible; taken as closed |
| The C4 blast | *"Sounds great."* The audio chain works end to end |
| The Details-panel round trip | Changes work, on the character and the placement camera |
| Cancel on a failed join, and the host leaving mid-game | Both work. No unpressed button left in the front end |

## 9. Known and deliberately not doing

- **Movement foley.** Decided, not deferred. `bMovementSounds` stays on with empty slots.
- **Client-authoritative movement.** Fine for local play; revisit before Steam.
- **Steam, sessions, achievements.** After the game is fun locally.
- **A stranger hosting** needs UDP 7777 forwarded or a mesh VPN of their own until SteamSockets.
- **A shared (rather than per-player) pickup**, **reversing the wall's 90 degrees**, and **raising
  `MantleMaxHeight` past 200** - all asked and settled. Don't.
