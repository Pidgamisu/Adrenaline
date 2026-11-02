# Credits and asset licenses

Every third-party asset pack, sound, font or tool shipped with Vaultborne gets a row here the day it is added.
Check each license allows use in a commercial game on Steam, and whether it requires attribution.

| Asset / pack | Folder | Source (link) | License | Attribution required? | Added |
|---|---|---|---|---|---|
| GlowGrid Starter | `Content/ThirdParty/GlowGridStarter/` | Fab, free pack - [listing URL still to add](https://www.fab.com/library) | Fab Standard License, Personal tier (to confirm on the listing) | No, under the Standard License | 2026-11-02 |

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
