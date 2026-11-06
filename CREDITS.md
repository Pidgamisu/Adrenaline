# Credits and asset licenses

Every third-party asset pack, sound, font or tool shipped with Adrenaline gets a row here the day it is added.
Check each license allows use in a commercial game on Steam, and whether it requires attribution.

| Asset / pack | Folder | Source (link) | License | Attribution required? | Added |
|---|---|---|---|---|---|
| GlowGrid Starter | `Content/ThirdParty/GlowGridStarter/` | Fab, free pack - [listing URL still to add](https://www.fab.com/library) | Fab Standard License, Personal tier (to confirm on the listing) | No, under the Standard License | 2026-11-02 |
| Suspenseful boom (C4 blast) | `Audio/C4/`, `Content/Audio/C4/` | Pixabay sound effects, listing id `451863` (`dragon-studio-suspenseful-boom-451863`) | Pixabay Content License, royalty-free | No | 2026-11-04 |
| Footstep run cycle | `Audio/Footsteps/`, `Content/Audio/Footsteps/` | Pixabay sound effects - **listing not identified**, see below | Pixabay Content License, royalty-free | No | 2026-11-04 |

## GlowGrid Starter

Added 2026-11-02. Greybox kit from **Fab**, taken as a free pack. 60 assets: a grid master
material `M_GG_Grid` with thirteen colour instances, one texture, and meshes under *Blocks,
Cover, Floors, Labels, Markers, Openings, Platforms, Props, Ramps, Shapes, Stairs, Structure,
Traversal, Walls*. Also two demo maps, `Maps/Overview` and `Maps/StarterRoom`, which the game
does not use and which can be deleted once the kit has been looked through.

`M_GG_Grid` is built on the engine's `WorldAlignedTexture` function, which is why it suits placed
pieces: the grid holds its size however a piece is scaled. See *Dressing a piece: materials* in
`docs/blueprint-tuning.md`.

**Commercial use looks fine, and two small things are still open.**

Fab sells almost everything under one **Standard License**, priced in two tiers by the buyer's
own gross revenue over the last 12 months: *Personal* at or below $100,000 USD, *Professional*
above it. A free pack is the Personal tier, which does allow commercial use - shipping on Steam
included - and asks for no attribution. Crossing the threshold later does not retroactively
require an upgrade. The Fab EULA is what actually binds; the summary pages are not.

Still to do, both one-click jobs:

1. **Paste the listing URL into the row above.** Searching the web for "GlowGrid Starter" finds
   nothing on Fab, so either the listing is named differently from the folder or it is not
   publicly listed. The URL is in *fab.com > My Library*, on the pack's tile.
2. **Check the license line on that listing.** Not every Fab item uses the Standard License -
   some are Creative Commons, and CC BY *does* require attribution in the credits screen. If the
   listing says CC BY, update the row's last column to yes and fill in the attribution text
   below.

Neither blocks greyboxing with the kit now.

## Attribution text for the game's credits screen

*(Fill in for any pack whose license requires attribution.)*

## Two sounds that were here and are gone

Both movement sounds were **deleted from the repository on 2026-11-06**, the same day
*Music carries the game, effects are for major events only* landed in CLAUDE.md. They are off the
table above because the files are no longer in the project, and this section exists so that removing
them did not also destroy the trail back to them:

| Was | Source | License | Lifetime |
|---|---|---|---|
| The wall kick, `Audio/Movement/WallKick.wav`, imported as `/Game/Audio/walljump/Walljump` | Pixabay sound effects, listing id `398256` (`u_qqkrn9bn55-jumplanding-398256`) - a *jump landing* sample | Pixabay Content License, royalty-free, no attribution | added 2026-11-05, deleted 2026-11-06, never heard in play |
| The landing thump, `Audio/Movement/Land.wav`, imported as `/Game/Audio/Movement/mixkit-quest-game-heavy-stomp-v-3049` | **Mixkit** free sound effects, `mixkit-quest-game-heavy-stomp-v-3049` (id `3049`) - the only non-Pixabay sound the project ever had | Mixkit Free License, free for commercial use, no attribution - **never confirmed on the listing** | added and deleted 2026-11-06, never heard in play |

Both are still in git history, LFS content included, so either can be restored with
`git checkout <commit> -- <path>` rather than downloaded again.

The Mixkit license question dies with the file. Nothing from that library ships now, so the two
things that were open - confirming on the listing that its free license covers commercial use
without attribution, and its rule against redistributing a sound as a standalone file - only matter
again if that sound comes back.

**That leaves `Audio/Footsteps/` as the only decided-against sound still in the repo**, with its six
imported `f1`..`f6` waves. Kept on purpose: they are not a download but something this project cut
and rendered out of a recording that no longer exists, so deleting them would be the one
irreversible deletion in the set. See *The tail is baked in* in `docs/sound.md`.

## Pixabay sound effects

Every sound in the game came from **Pixabay's sound effects**, royalty-free and with no attribution
required - stated by the project owner on 2026-11-05, and recorded here on that word. The one
exception came from Mixkit and has since been deleted; see above. I have not opened the individual listings to confirm them one by one,
so this is a record of what was stated rather than an audit.

**The number at the end of a Pixabay download filename is the listing id**, and it is the only trail
back to the source once the file has been renamed: `u_qqkrn9bn55-jumplanding-398256.mp3` is listing
`398256`. Keep the original filename, or paste the listing URL into the table above, before
renaming anything. Two of the three are traceable that way.

Three things worth knowing:

- **The footstep run cycle has no id.** It arrived as `Untitled song (Nov 4, 2026 12:56 PM).mp3`, a
  re-export with the original name already lost, so there is nothing to trace it back to. It is
  covered by the same statement, but if the provenance of that one ever has to be proved, the
  listing will have to be found again. The dry cuts in `Audio/Footsteps/Dry/` are what it became.
- **The wall kick came from a jump landing sample**, and both are now deleted - see
  *Two sounds that were here and are gone*. The worry recorded here was that using one sample for
  both the landing and the wall kick would be audibly repetitive. It was answered by sourcing the
  landing from a different library, and then made moot a day later when neither sound survived the
  music-led decision.
- **Pixabay hosts user uploads.** The license it grants is only as good as the uploader's right to
  grant it, which is the standard risk with any user-upload library. Acceptable for an itch.io
  release; worth a second look before anything larger, which is the same note as *Revisit client
  authority before Steam*.
