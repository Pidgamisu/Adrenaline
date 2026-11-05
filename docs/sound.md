# Sound

Started 2026-11-04. The project had no audio of any kind before this: one unfilled `ExplosionSound`
slot on `AGetawayC4` and nothing else.

## What is built

**Movement sound slots on `UGetawayMovementComponent`.** Nine `USoundBase*` properties and one
array, each fired at the moment it belongs to, plus the tuning that shapes them. The wiring went in
before any audio existed, so that finding a wave and hearing it is a Details-panel job from then on,
with no further C++.

**Footsteps are in and played** (2026-11-04): six cuts of a run cycle, in `Audio/Footsteps/`. Every
other slot is still empty, and an empty slot is simply silent. See *Still unverified* at the
bottom.

## The slots, and what fires each

All ten live on the movement component, under **Getaway|Sound** in the Details panel. Set them on
`BP_Character` with **CharacterMovement** selected in the *Components* panel, the same place the
rest of the movement tuning lives (see `docs/blueprint-tuning.md`).

| Slot | Fired from | When |
|---|---|---|
| `JumpSound` | `UpdateCharacterStateBeforeMovement` | A ground jump, including every bunny hop |
| `DoubleJumpSound` | `UpdateCharacterStateBeforeMovement` | The air jump is spent |
| `LandSound` | `AGetawayCharacter::Landed` → `PlayLandSound` | Touching down above `LandMinSpeed` |
| `FootstepSounds` (array) | `UpdateFootsteps` | One of the pool, every `FootstepStride` cm of ground covered |
| `SlideStartSound` | `UpdateCharacterStateBeforeMovement` | A slide begins |
| `SlideLoopSound` | `StartSoundLoop` / `StopSoundLoop` | Held for the whole slide, pitched by speed |
| `WallRunStartSound` | `TryStartWallRun` | A wall is caught |
| `WallRunLoopSound` | `StartSoundLoop` / `StopSoundLoop` | Held for the whole wallrun |
| `WallKickSound` | `WallKick` | Space pushes you off a wall |
| `MantleSound` | `TryStartMantle` | A climb starts |

The two loops stop through `EndSlide` and `EndWallRun`, which are already the single funnels every
exit from those states goes through - the same reason the slide cooldown lives in `EndSlide`. There
is no way to leave a slide or a wallrun that leaves its loop playing.

`PlayLandSound` is the one sound fired from the character rather than the component, because
`Landed` is an override on the character and is the only place the fall speed is still readable.

## Three decisions worth knowing

**Every movement sound is 2D.** Not placed in the world. The listener is this character's own
camera, so a positioned sound would sit on top of the listener anyway - and a 2D sound needs no
Attenuation asset, which means a slot works the instant you point it at an imported wave, with no
second asset to build. World sounds that are *not* yours (the C4 blast, a breakable) stay 3D through
`PlaySoundAtLocation` and will want an Attenuation asset when they are done.

**You only ever hear yourself.** `SoundsAudible()` gates all of it on the character being the one
this machine is playing. A remote character's movement component is tick-disabled by
`AGetawayCharacter::SetProxySimulation`, so almost none of this code runs for one - but the *host's*
copy of a remote player does tick, and without the gate the host alone would hear that player's
footsteps while the other machine heard nothing. That asymmetry would be worse than silence.

So a rival wallrunning past you makes no noise. Fixing that does **not** need new RPCs: the
character already replicates `FGetawayNetMove`, carrying `Velocity` and a `State` of
Ground/Crouch/Slide/Air/WallRun/Mantle (see `AGetawayCharacter::SampleMove`). A client can watch
other characters' replicated state in its own Tick and play 3D footsteps and landings off the
changes. That is a separate pass and has not been written.

**Footsteps are a pool, not a Sound Cue.** `FootstepSounds` is an array and `PickFootstep` draws
one per step, never the one just played. The usual way to vary a repeated sound is a Sound Cue with
a Random node, and that still works - make the Cue the only entry - but it is a dozen clicks of
graph wiring per sound, and it **cannot be handed over as a script**: `USoundCue::FirstNode` is
`BlueprintReadOnly` and `AllNodes` is editor-only with no accessor, so there is no way to build the
graph from Python. An array is dragging waves into a list.

Excluding the last one played matters more than the pool being big. A plain random pick of six
repeats itself about one step in six, and the same footstep twice in a row is what gives away that
a sound is being repeated at all.

**Footsteps are distance-based, not timed.** A tally of centimetres covered, spent a step at a time.
Steps keep pace with you for free, and anything that takes your feet off the floor - a jump, a
slide, a wallrun - simply stops adding to it. There is no animation to hang notifies off, the
character being a box, so this is the whole footstep system. Landing clears the tally so the first
step after a landing is a full stride away rather than arriving on top of the thump.

## The looping slots need looping assets

`SlideLoopSound` and `WallRunLoopSound` are held for as long as the state lasts, but **the asset
itself has to loop**: the *Looping* checkbox on a Sound Wave, or a Looping node in a Sound Cue. A
one-shot in one of those slots plays once and goes quiet, which looks like a bug and is not.

The loops are spawned with `SpawnSoundAttached` on the capsule, with
`bStopWhenAttachedToDestroyed`, rather than as 2D sounds. A 2D loop outlives whatever started it, so
dying mid-slide would leave the scrape playing over an empty level for the rest of the round.

## Tuning

Under **Getaway|Sound** and **Getaway|Sound Tuning** on the movement component.

| Value | Default | What it does |
|---|---|---|
| `bMovementSounds` | true | Master switch for all ten |
| `MovementSoundVolume` | 1.0 | Multiplies every movement sound |
| `FootstepStride` | 340 cm | Ground covered between footsteps |
| `FootstepMinInterval` | 0.3 s | Floor on how often a step may fire, whatever the speed |
| `FootstepMinSpeed` | 150 cm/s | Below this you move silently |
| `FootstepPitchJitter` | 0.08 | Pitch spread per step, so one wave is not mechanical |
| `LandMinSpeed` | 200 cm/s | Landing slower than this is silent |
| `LandFullSpeed` | 1600 cm/s | Landing this fast is full volume |
| `LandMinVolume` | 0.35 | Quietest an audible landing can be |
| `SlideLoopPitchMin` / `Max` | 0.85 / 1.3 | Slide loop pitch at `SlideStopSpeed` and at full tilt |
| `LoopFadeTime` | 0.12 s | Fade when a loop ends, so it does not click off |

## Getting a sound in

1. Put the pack in `Content/ThirdParty/<PackName>/` and give it a row in `CREDITS.md` the day you
   add it. `.wav` and `.ogg` are already Git LFS-tracked in `.gitattributes`, so audio does not
   bloat the repo.
2. Drag the `.wav` into the Content Browser. 16-bit PCM at 44.1 or 48 kHz imports cleanly.
3. Open `BP_Character`, select **CharacterMovement** in *Components*, find the slot under
   **Getaway|Sound** in *Details*, and pick the asset. **Compile** and **Save**.
4. For footsteps, `FootstepSounds` is a list: press **+** once per cut and set each entry. No Sound
   Cue needed - `FootstepPitchJitter` already spreads the pitch per step.

### A two-minute smoke test with no downloads

Windows ships usable wavs in `C:\Windows\Media`. Import `chimes.wav` and `Windows Ding.wav`, set
them as `JumpSound` and the first entry of `FootstepSounds`, and you can confirm the whole chain
works before sourcing a single real asset. Delete them afterwards - they must not reach a build.

## Raw audio sources, and the run cycle we sliced

**`Audio/` at the project root holds raw source audio**, outside `Content/` because `Content/` is
for `.uasset`s. `*.wav` and `*.mp3` are already Git LFS-tracked in `.gitattributes`, so sources are
versioned without bloating the repo. You cannot get a usable wav back out of a cooked `.uasset`, so
the source files are the thing worth keeping.

`Audio/Footsteps/Step_01.wav` .. `Step_06.wav` (added 2026-11-04) were cut out of a 7.18-second
running recording. Worth knowing what the analysis found, because it decided the approach:

- **20 footfalls, median gap 0.289 s**, very steady (0.254 to 0.324). That is a cadence of 207 steps
  per minute.
- That steadiness is why the clip could not be used whole. A 7-second run cycle in a slot that fires
  once per step would have had about 26 copies of itself overlapping at run speed.
- **Matching the recording's real cadence was a trap.** `FootstepStride` started at 220, which fires
  a step every 0.275 s at `RunSpeed` 800 - within 5% of the recording's own 0.289 s, so it looked
  like a free win. Played, it was far too quick: a true sprint cadence heard from a first-person
  camera reads as frantic rather than fast, and it left the samples no room between hits. 340 cm
  (0.43 s) was the fix. Physical accuracy is not the target; weight is.
- **The recording is bone dry.** Every footfall falls 20 dB inside 20 to 50 ms, and what follows the
  last one is room noise at 0.001, not a reverb tail. So the first cuts felt cut short even though
  nothing had been cut off them - there was simply nothing there. The ring-out had to be *added*;
  see below.

### The tail is baked in, not played back dry

`Tools/reverb_footsteps.py` is what actually produced the files in use. It cuts the steps as above
and then runs each one through a Schroeder/Freeverb network - eight damped comb filters in parallel
into four allpass filters in series - padded out so the tail has somewhere to ring. The finished
samples are **720 ms** long: the hit, then a tail falling steadily to silence by about 600 ms,
against a dry footfall that is over inside 50.

Baked rather than done in the engine on purpose. A submix with a `SubmixEffectReverbPreset` would be
the engine-side way, and it is two `.uasset`s to build by hand plus a field set on every wave, where
this is a Reimport. Baking also means the tail is identical in every build and costs nothing at
runtime. The trade is that it cannot be changed without re-rendering - which is why the dry cuts are
kept in `Audio/Footsteps/Dry/`.

**Point the script at that `Dry` folder, not at a recording.** Given a folder of wavs it skips the
slicing and only re-renders the room, which is what every change after the first one wants - and it
is the only route left once the original recording is gone, which here happened within the hour. A
cooked `.uasset` cannot be turned back into a wav, so `Audio/Footsteps/Dry/` is the master copy of
these sounds.

Every constant can be overridden on the command line, so a retune is not a file edit:

    "...\python.exe" -I Tools/reverb_footsteps.py Audio\Footsteps\Dry Audio\Footsteps WET_LEVEL=0.55 TAIL_DECAY_MS=900

**Two numbers decide how it sounds.** `WET_LEVEL` is the tail's loudness as a fraction of the dry
hit - 0.40 now, and much past 0.5 the reverb bloom overtakes the hit and it reads splashy.
`TAIL_DECAY_MS` is how long it rings, being the time the tail is forced down to -60 dB - 650 now.
`ROOM_SIZE` and `DAMPING` change only the colour of it, and `LENGTH_MS` has to stay a little longer
than `TAIL_DECAY_MS` or the end fade clips the tail off.

Samples are 720 ms and steps land every 430, so the previous step's tail is **still sounding when
the next one hits**. That overlap is the echo. It also means the mix stacks: if it ever distorts,
`MovementSoundVolume` on the movement component is the knob, not a re-render.

#### Three traps, each of which cost a round of guessing

**Freeverb's output level swings with the room.** The comb filters are resonant, so a long room
builds up far more amplitude than a short one, and the fixed 0.015 output gain in the original only
balances at one setting. Mixing dry against wet by those raw numbers gave a wall of reverb at
`ROOM_SIZE` 0.88 and a tail 38 dB down and inaudible at 0.45. `reverb()` now returns the wet signal
on its own and the caller levels it against the dry hit, so the balance no longer depends on the
room at all.

**Freeverb's decay is a side effect, not a setting.** It falls out of the feedback coefficient, and
reading a decay time off that coefficient is guesswork: `ROOM_SIZE` 0.88 with `DAMPING` 0.20 worked
out at roughly 3.7 seconds, so inside a 1.15 s sample the tail never audibly fell at all - it held a
flat wash about 13 dB down for 350 ms and then met the end fade. That is exactly what *it doesn't
fade out* meant. The wet is now forced down an explicit exponential, so the decay is something you
set rather than something you discover.

**The measurement hid both of them.** The first metric was "time to fall 20 dB below the sample's
peak", and the dry hit is a 5 ms transient far above the tail, so that was satisfied at 330 ms while
the wash carried on for another 700 - it reported a healthy number for a sound that did not decay.
Decay is now measured off the RMS envelope against its own maximum, which cannot be fooled the same
way. What actually settled it was dumping the level every 50 ms and reading the shape of the curve,
and that is the check worth repeating on any sound that is supposed to fade.

`Tools/slice_footsteps.py` is the earlier, dry-only version, kept because it is the simpler thing to
reach for when a recording already has the tail you want. Run either with the Python that ships with
the engine:

    "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" -I Tools/slice_footsteps.py <input> <output folder>

It decodes through the `libsndfile-1.dll` in the engine's `Binaries/ThirdParty`, which is how it
reads an mp3 without any other tool installed. It peak-picks the footfalls, keeps the six loudest,
trims each to its own attack and decay, fades both ends and normalises them to a common peak, then
writes 16-bit mono wavs. Mono because these are 2D sounds - half the size, and the right starting
point if they ever become 3D for remote players.

## The C4 bang

`AGetawayC4::ExplosionSound` was the only sound slot that existed before any of this, and it is
fired from `PlayBlastEffects` inside `MulticastDetonate_Implementation`, so **filling it is heard on
every machine with no further work** - no RPC, no replication, no C++ at all.

**But there is no `BP_C4`, and that is the catch.** `AGetawayCharacter::C4Class` is set to
`AGetawayC4::StaticClass()` in the constructor, so the thrown charge spawns as the raw C++ class -
and a C++ class has no Details panel, so there is nowhere to put the sound. A Blueprint subclass has
to exist first. CLAUDE.md's next-steps list refers to "`BP_C4`" as though it does; it never has.

`BP_Piece_C4Pickup` is **not** it. That is the pad you place during the build phase, a subclass of
`AGetawayC4Pickup`. The bang belongs to the charge you *throw*, which is `AGetawayC4`, and the two
are different classes with different jobs.

So, once per project:

1. `Content/Blueprints`, right-click ▸ **Blueprint Class**, expand **All Classes**, search
   `GetawayC4` and pick **GetawayC4** - not `GetawayC4Pickup`. Name it `BP_C4`.
2. In `BP_C4`, the slot is **Explosion Sound** under **Getaway|Effects** - not under a sound
   category. It sits next to `ExplosionEffect` and `BlastEffectClass` because the three were written
   as one feature.
3. In `BP_Character`, toolbar ▸ **Class Defaults**, then **Getaway|C4 ▸ C4 Class** = `BP_C4`.
   Without this last step the charge still spawns as the C++ class and the sound never plays, with
   nothing to say why.

`C4Class` falling back to the C++ class when unset is deliberate - the same pattern as
`PlacementPawnClass` on the game mode, so a broken or empty value still spawns something playable
rather than failing silently. The cost is exactly the confusion above.

It is also the easiest sound in the game to get right, and should have been first. A footstep has to
land at a moment the player feels in their legs, and this game gives them no legs; an explosion
happens when you press F. The moment is a fact, so it cannot feel mistimed. The same is true of the
landing thump, the wall kick and the jump pad.

**Three candidates are in `Audio/C4/`**, made by `Tools/c4_bang.py` rather than sourced:

| File | Character | Decay to -30 dB | Low end |
|---|---|---|---|
| `C4_Thump.wav` | concussive and close, almost no crack, all chest | 120 ms | +3.9 dB |
| `C4_Crack.wav` | sharp and dry, a charge on something hard, reads fast | 40 ms | +0.3 dB |
| `C4_Boom.wav` | cinematic: deep sweep, long rumble, debris after | 360 ms | +5.1 dB |

An explosion is four layers: a **crack** (a few ms of bright noise), a **body** (a pitch sweeping
downward, which is what reads as force rather than as a pop), a **rumble** (low-passed noise with a
slow decay) and optionally **debris** (sparse clicks afterwards). The three above are the same four
layers at different balances, which is why they are properly different rather than variations.

Synthesised rather than downloaded because nobody knows what C4 actually sounds like - it only has
to read as force - and because three candidates cost the same as one. **That is the point.** Tuning
a sound by measuring it does not work, as the footsteps proved three times over; generating several
and letting a pair of ears choose does.

They are rendered **dry on purpose**. The tail on them is the explosion's own rumble, not a room. If
the reverb submix below goes in, they pick the room up from it; anything with a room baked in would
be reverbed twice.

### The blast actually chosen: a library boom

Added 2026-11-04, from a downloaded sound rather than the synthesised candidates. Two versions are
in `Audio/C4/`, both prepared by `Tools/prep_sound.py` - downmixed to mono, levelled to 0.9 and
faded, from a 1.62 s 44.1 kHz stereo mp3 that already started on the bang:

| File | Length | What it is |
|---|---|---|
| `C4_Blast.wav` | 1.62 s | the whole thing, **two hits**: the blast, then a second one at 750 ms |
| `C4_Blast_Short.wav` | 0.70 s | cut to the first event, so it reads as one explosion |

The second hit is the thing worth knowing. Dumping the level every 50 ms shows it decaying from
-3.4 dB at 50 ms down to **-34 dB at 700**, then jumping **back up to -14 dB at 750** and wandering
around -20 to -31 dB until 1500 ms. That is a designed cinematic boom with layers, and on a C4 charge
a second bang most of a second after you press F reads as two explosions. `length=700` on
`prep_sound.py` is what cuts it back to one.

**`C4_Blast.wav`, the full one, is the chosen blast** (2026-11-04, picked by ear). The second hit
was heard and kept: a detonation is a bang followed by a heavier one, which suits a charge meant to
be the biggest thing that happens in a round. `C4_Blast_Short.wav` stays as the fallback.

That choice has one consequence. `AGetawayCharacter::DetonateC4` loops over every charge the player
holds and detonates all of them in the same frame, so blowing two at once fires
`PlaySoundAtLocation` twice on one frame - two copies of a 1.62 s sound exactly on top of each
other, about 6 dB louder, from a source already levelled to 0.9. That will clip. The long tail also
means the pile-up lasts the better part of two seconds, where the short version would be over in
0.7.

No code needed if it turns out to matter. On the sound asset's Details panel under **Voice
Management**, tick **Override Concurrency** and set **Max Count** to 1 with the resolution rule set
to stop the oldest; two simultaneous blasts then play as a single sound. Left undone deliberately -
two charges at once *should* be louder, and the only real question is whether it distorts, which
needs ears.

**Inspect a downloaded sound before using it.** `Tools/inspect_audio.py` reports the three things
that are usually wrong - silence at the head, which makes a one-shot arrive late; a level far below
full scale; and stereo, which is the wrong shape for a positional sound - plus how many separate
events are in it and how it decays. The two library files downloaded the same day both had head
silence (62 and 71 ms) and one was **19 dB quiet**, neither of which is visible by looking at a
waveform in a browser.

### The blast has no distance falloff

`AGetawayC4::PlayBlastEffects` calls `PlaySoundAtLocation` with no attenuation, so **the explosion
is the same volume anywhere on the map** - a charge detonated across the course is as loud as one at
your feet, which on two machines is unpleasant.

It needs no C++ to fix. `FAudioDevice::PlaySoundAtLocation` falls back to the sound's own settings
when none are passed - `AttenuationSettings ? &AttenuationSettings->Attenuation :
Sound->GetAttenuationSettingsToApply()` - so a **Sound Attenuation** asset set on the blast's own
Details panel is enough.

**It goes on the sound asset, not on `BP_C4`.** That is the whole point of the fallback above: the
code passes nothing, so the sound's own setting is what gets used. `BP_C4` holds *which* sound
plays; the sound holds how far it carries.

1. `Content/Audio`, right-click ▸ **Audio ▸ Sound Attenuation**, name it `ATT_World`. One asset can
   serve every world sound, so it is not worth making a per-sound one.
2. Open it. Under **Attenuation (Distance)**: **Attenuation Function** = `Natural Sound`,
   **Attenuation Shape** = `Sphere`, **Attenuation Shape Extents ▸ X** = `1000` (the radius held at
   full volume, so 10 m), **Falloff Distance** = `7000` (the 70 m over which it fades away).
   For a sphere only X is read; Y and Z are ignored.
3. Open the imported `C4_Blast` **Sound Wave** and set **Attenuation Settings** under the **Sound**
   category to `ATT_World`.

Those distances suit this course: at `RunSpeed` 800 a player covers 8 m a second, so 70 m is a few
seconds of running - far enough to hear a charge go off across the middle of a level, not far enough
to hear one at the far end.

The field is editable on a Sound Wave: `USoundBase::IsAttenuationSettingsEditable` returns true, and
only `USoundCue` overrides it to false, when **Override Attenuation** is ticked on the cue. So if the
slot is ever greyed out, it is because the asset is a Cue with its own inline attenuation.

Not done yet.

## Making everything echo: the reverb submix

The right way to put a room on the whole game, and the opposite trade to the baked footstep tails:
more setup once, but then **the knob is in the editor and you turn it with your ears while the game
runs**, instead of someone re-rendering wavs blind.

Every sound falls back to the project's Master Submix when its own **Base Submix** is unset - that is
stated outright on `USoundBase::SoundSubmixObject`. So one reverb on the master submix is heard on
everything, with nothing to set per asset.

What it takes:

1. **Content Browser ▸ right-click ▸ Audio ▸ Mix ▸ Sound Submix**, name it `SM_Master`.
2. **Right-click ▸ Audio ▸ Effects ▸ Submix Effect Reverb Preset**, name it `FX_Reverb`.
3. Open `SM_Master`, find **Submix Effect Chain**, press **+**, set the entry to `FX_Reverb`.
4. **Project Settings ▸ Engine ▸ Audio ▸ Mix ▸ Master Submix** = `SM_Master`.
5. Open `FX_Reverb` and fix the two defaults in the next paragraph, or the game goes silent.

**The trap: `Gain` and `DryLevel` both default to 0.** On `FSubmixEffectReverbSettings`, `Gain` is
`0.0f` (the comment says "0.0 < 0.32 < 1.0", but the C++ default is zero) and `DryLevel` is `0.0f`
too. A submix effect processes the submix buffer in place, so an insert with no dry level and no
reverb gain passes nothing through. Set **Dry Level 1.0** and **Gain** around 0.3 before judging
anything. `Decay Time (seconds)` is then the one that matters - it defaults to 1.49, which is a
large hall; a corridor is nearer 0.6.

Not built yet, and the five steps above are all editor-side, so they are click steps rather than
something that can be scripted: a Sound Submix and an effect preset are both `.uasset`s.

## Still unverified

- **Only footsteps have been played.** The other nine slots are empty and have never made a sound.
- The **looping** slots have never had a looping asset in them: the fade-out, the slide loop's pitch
  ramp, and `bStopWhenAttachedToDestroyed` taking a loop away on death are all untested.
- **None of the three C4 bangs has been heard**, by me or by anyone. They are measured as being
  different from each other, which is not the same as any of them being good.
- **The reverb submix has not been built**, so "everything echoes" is a verified plan and nothing
  more. In particular the claim that a wet master submix reaches 2D sounds is read off the engine
  source, not observed.
- **The reverb baked into the footsteps has not been heard in game**, and I cannot hear it either - the room was tuned by
  measuring the decay, not by ear, and two measurements in a row flattered a sound that was wrong.
  Whether a 500 ms fade reads as a room, whether `WET_LEVEL` 0.40 is the right balance, and whether
  the overlapping tails muddy at speed are all open.
- `FootstepStride` 340 and `FootstepMinInterval` 0.3 are a first move away from "way too quick",
  not a tuned answer. The cap has never actually been hit in play: it only bites above about
  1130 cm/s, which needs a few bunny hops.
- The `LandMinSpeed`..`LandFullSpeed` curve is a guess too.
- Nothing has been heard in a **packaged** build, or with two players.

## What is not built

- **Pieces**: jump pad, spikes, breakable smash, crate crack, C4 throw/stick/arm, pickup collect,
  bridge collapse, windmill axle, vortex hum. The C4 *blast* has candidates waiting - see *The C4
  bang* above.
- **Round and UI stings**: build phase opening, the effect card, the last-seconds countdown, GO,
  **YOU MADE IT**, round-end scoring, menu clicks.
- **Music.** A track per phase, swapped off the replicated `Phase` on `AGetawayGameState` so every
  machine switches by itself. The trap: it must be owned by something that survives a round, or it
  restarts every time the level is rebuilt - a game instance subsystem or the player controller
  holding one `UAudioComponent`, not an actor.
- **Sound Class / Sound Mix split** into Music and SFX, so a settings page can have two sliders.
  Worth doing before there are many assets; retrofitting means touching every one.
- **Remote players' movement sound**, off `FGetawayNetMove` as described above.
