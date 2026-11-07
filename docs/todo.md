# To do

Written 2026-11-07, from `CLAUDE.md`'s *Next steps* plus the **Still unverified** section of every
other file in `docs/`. Nothing here is new work I have decided on; it is what those files already
say is outstanding, put in one order.

**The shape of the list is the point.** Fourteen pieces, four effects, a front end, a HUD and a
round loop are *built*; a lot of it has never been on screen. So section 1 is play-tests, not code,
and it is first because every number in the unplayed features is a first pass and tuning them is
cheap once they have been felt. Code that has not been played is the project's main debt right now.

Tick an item by playing it and either signing it off or saying what to change.

---

## 0. Uncommitted in the tree right now

- [ ] **Play the one-button hand and commit it.** Left click (or RB/RT on a pad) now uses whatever
      is in your hand: let go of a rope that is out, set off a live charge, throw the charge, fire
      the hook - in that order. Built 2026-11-06, **not played**, and sitting uncommitted along
      with the HUD's single hand line and the C4 pickup's new wording. What to watch: whether
      detonate-before-throw is right when you are holding one brick and have another stuck, and
      whether `C4 live` in the hand slot is clear enough. See *One button for the whole hand* in
      `docs/placement.md`.

## 1. Play what is already built

In the order I would play it, riskiest first. Most of these are one round each.

- [ ] **The grapple hook** (effect, built 2026-11-05). The single biggest unplayed thing in the
      game, and every number in it is a first guess: 4500 of pull, an 1800 ceiling, 0.35 of view
      steer, and all of gravity held up. Open questions in `docs/effects.md`: hook or catapult, can
      you actually swing with it, is it too much like a zip line, and does `GrappleCutSlack` drop
      the rope when it should not.
- [ ] **The grapple pickup** (piece, built 2026-11-05) and with it **the one-item hand** - whether
      losing the second C4 charge is felt, and whether a hook pad next to a C4 pad reads as a
      choice. `docs/placement.md`.
- [ ] **The HUD.** Nobody has played either pass; both were checked by screenshot. Specifically:
      is it readable at 800 cm/s, does the glow turn to mush over bright additive geometry, and
      **the whole build-phase half has never been looked at** - the hand list, the status line, the
      build clock, the two key lists, a banner, a two-name scoreboard, the effect announcement.
      `docs/hud.md`.
- [ ] **The effect banner in its build-phase home** - the big centre card, the 2 s hold, the travel
      to the top left corner. Played only in the run-start version it replaced. `docs/effects.md`.
- [ ] **Finishing and the watch camera** (2026-11-04 work). **YOU MADE IT**, the time and place, the
      2 s delay, then the lift. Unplayed: the compact line's position against the placement HUD,
      the second finisher getting `2nd`, and a finisher who falls in the two seconds before the
      camera takes over. `docs/round-loop.md`.
- [ ] **A pickup coming back on death**, on both pads - the restock itself, and the loop it opens:
      dying is free, so running off the course on purpose is now a way to restock. Decide whether
      that is fine. `docs/placement.md`.
- [ ] **The jump pad's low-friction window** (`FrictionScale` 0.3 for 3 s). Not played, and the one
      value with real risk in it - 0.3 may read as ice. Landing out of a pad into a slide should be
      the fast line. `docs/placement.md`.
- [ ] **Spikes and jump pads riding a windmill plate.** Built 2026-11-03, never played: a
      surface-mounted piece attaches to the component it was placed on, so it should orbit with the
      plate. `docs/placement.md`, `docs/spikes-and-death.md`.
- [ ] **Pieces standing square on a slope** (2026-11-05). Unplayed: how 133 cm of air under the
      downhill end of a 400 cm floor reads at speed, a piece on a face steeper than 18.4 degrees,
      and stacking on a ramp. `docs/falling-slopes-cleanup.md`.
- [ ] **The ramp and the collapsing bridge**, which have still never been played as pieces - is the
      0.45 s hold right, does the tip read as a collapse or a shrug, and is the ghost legible for a
      wedge. `docs/placement.md`.
- [ ] **The C4 blast, heard.** The only sound wired in the whole game and nobody has heard it. Press
      Play, throw a charge, click. It also proves the audio chain exists at all, and `ATT_World`'s
      falloff distances have only ever been set, never heard. `docs/sound.md`.
- [ ] **The controller, with a pad in hand.** Look speed, curve and ramp are starting points; a pad
      has been held far enough to catch an inverted pitch and no further. Also: can a stick hold an
      air strafe at all (`MoveStickFullTilt` 0.9), which trigger should be up, and drop either LB or
      B once one of them is clearly the slide button. `docs/controller.md`.
- [ ] **Tuning round-trip through the Details panel.** Change a value on `BP_Character` and on
      `BP_PlacementCamera` and feel it. The wiring is proved by reading; the round trip is not.
      `docs/blueprint-tuning.md`.
- [ ] **A material on a piece, in play** - the ramp's procedural-mesh slot especially, since its
      timing is the one that could be wrong. `docs/blueprint-tuning.md`.
- [ ] **The Cancel button on a failed join.** The smallest open item in the front end: only the
      give-up clock it shares a code path with has been clicked. `docs/itch.md`.
- [ ] **The host leaving mid-game.** Still unplayed; see *What the front end does not do* in
      `docs/multiplayer.md`.
- [ ] **`stat unit` on the HUD.** A worst-case frame is a few hundred canvas text draws rather than
      a dozen, never measured, in a game that cares about frame time. `GlowRings 1` halves it,
      `TextBend 0` halves it again. `docs/hud.md`.

## 2. Small jobs with a clear end

- [ ] **Logging in Shipping.** One line - `bUseLoggingInShipping` in `Source/Getaway.Target.cs` - and
      without it a packaged build writes no log, so a player who crashes has nothing to send. Worth
      doing before the itch page is opened up. (`CLAUDE.md` *Next steps* 8.)
- [ ] **A Blueprint child for the HUD.** The game mode sets `HUDClass` in C++, so not one knob in
      the HUD can be touched from the Details panel. Six clicks, and it fixes `docs/effects.md`'s
      same complaint at the same time. `docs/hud.md`.
- [ ] **The Music / SFX Sound Class split.** A music-led game with no volume slider is one players
      mute. It matters early now rather than eventually. `docs/sound.md`.
- [ ] **A slide-cooldown tell near the crosshair.** The debt the no-foley decision took on: every
      other piece of movement feedback has a visual and nothing says when your next slide is ready.
      `docs/hud.md`, `docs/sound.md`.
- [ ] **Apply the logo in the game.** The exe icon and both splash screens are done; the menu still
      spells ADRENALINE out as text. `docs/look.md`.
- [ ] **Clear `BP_Character`'s `SmashSpeed` override**, now redundant at 1100 either way. `CLAUDE.md`.
- [ ] **Close the two GlowGrid Starter license items in `CREDITS.md`.** The pack does not ship, so
      this is tidiness. `docs/itch.md`.

## 3. The itch.io page

The build is up and patching works; the page is a blank gate with a download behind it.

- [ ] **A short video of movement at speed.** The highest-value thing missing anywhere in the
      project - motion is the whole pitch and no still can show a wallrun into a slide into a C4
      jump.
- [ ] **Paste the text in.** It is all drafted in `docs/itch-page.md`; none of it is on the page.
- [ ] **Cover image and screenshots.** The logo is the obvious cover.
- [ ] **Download and run a build from the page**, as opposed to the folder on this machine. Nobody
      has. It would also answer whether the itch app honours `.itch.toml`.
- [ ] Decide when the password comes off. Restricted is deliberate for now.

## 4. Sound, in order

From *What to do next, in order* in `docs/sound.md`, which is built around *Music carries the game*.
All ten movement slots are deliberately empty and stay that way.

- [ ] Music: a track per phase.
- [ ] The Music/SFX split (also listed above).
- [ ] The devices you trigger: jump pad, speed ring, pickup, grapple.
- [ ] The round and UI stings: build clock running out, GO, **YOU MADE IT**, dying.
- [ ] Breakables bursting.

## 5. Features

- [ ] **More traps.** Spikes were the first. Check any new piece against *Every piece must grant
      ability or test precision* in `CLAUDE.md` **before** writing code - that rule is what the
      scrapped void bought, and `docs/placement.md`'s *The void, and why it was scrapped* is the
      worked example.
- [ ] **A per-round deck** for the party box. One normal and one special piece landed 2026-11-06;
      drawing from a deck rather than at random is what is left.
- [ ] **A lobby, and a settings page.** Neither exists, and a lobby has never been the thing that
      broke. `docs/multiplayer.md`.
- [ ] **Pad support for the menus.** A controller cannot drive the front end or the HUD at all.

## 6. Known and deliberately not doing

Here so none of it gets re-raised as a bug.

- **Movement foley.** Decided, not deferred. `bMovementSounds` stays on with empty slots.
- **Client-authoritative movement.** Fine for local play; revisit before Steam.
- **Steam, sessions, achievements.** After the game is fun locally.
- **A stranger hosting** needs UDP 7777 forwarded or a mesh VPN of their own until SteamSockets.
  Two machines on a tailnet played fine 2026-11-03.
- **A shared (rather than per-player) pickup**, **reversing the wall's 90 degrees**, and **raising
  `MantleMaxHeight` past 200** - all asked and settled. Don't.
