# Launching on Steam

Written 2026-11-03, when the ask changed from itch.io to Steam. **Nothing here has been done yet.**
This is the plan, the real costs and the gates, so that the order of work is clear before any code
is written.

Read this with *It is packaged* and *The front end* in `docs/multiplayer.md`. The short version of
those: there is a packaged Development build that boots and plays, two machines have never
connected, and nobody has looked at the package on screen.

## The thing to understand first

**Steam is not a build step. It is a two-to-three month track with gates you cannot hurry**, and
most of the work is not code. The hard sequence Valve imposes:

1. Pay the fee, do the paperwork, get an **AppID**. Nothing in the engine can be configured until
   this exists.
2. Build the **store page** and submit it for review - typically **3-5 business days**, and Valve
   recommends submitting **at least 7 business days** before you want it live.
3. The store page must clear review **before you may submit the build** for review. Both have to
   pass.
4. Once the page is posted publicly as *Coming Soon*, it must stay up **at least two weeks** before
   you can release. This applies to Early Access too.

So the earliest plausible release from a standing start is about **six to eight weeks**, and that
is assuming the store art and the trailer exist, which they do not.

Also worth knowing: once the page is live and the release date is within 14 days, **the date locks**
and changing it means contacting Valve.

## What it costs

- **$100 USD per app**, plus VAT/GST depending on country. Steam wallet funds cannot pay it.
- Older coverage describes the fee as recouped once the app earns $1,000. **The current official
  docs do not say this**, so do not count on it.
- The fee attaches an app credit to the Steam account of whoever paid, and only that person can
  activate it.
- Third-party guides mention a **~30 day wait** between a first-time publisher paying the fee and
  being able to release. Not confirmed in Valve's own docs; budget for it anyway.

Paperwork is identity verification, tax forms (a W-9 for a US individual) and bank details for
payouts. This is Nathan's job, not a code job, and it is the long pole - **start it first**, because
everything in the next section is blocked on the AppID.

## The code work

### 1. A Shipping build (do this first, it needs no AppID)

This is step one for *any* launch, Steam or itch.io, and it is the only piece that can start today.

`Config/DefaultGame.ini` currently says `BuildConfiguration=PPBC_Development` and
`ForDistribution=False`, with a comment explaining why. **That comment is now out of date**: the
last reason it gave - an invisible C4 blast in Shipping - was fixed on 2026-11-02 when the
explosion became real geometry (`AGetawayBlast`). So the switch is to `PPBC_Shipping` and
`ForDistribution=True`.

It has never been tried, and the risk is specific: **Shipping removes the `~` console.** Today a
friend whose join fails has the console as an escape hatch. In Shipping they do not, which makes
the menu's error paths load-bearing - and per *Still unverified* in `docs/multiplayer.md`, Escape,
Leave and Cancel have never been played. Those get tested properly as part of this step.

### 2. Steam integration (blocked on the AppID)

UE 5.8 ships the plugins for this - checked in
`C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Online`: `OnlineSubsystemSteam`,
`SocketSubsystemSteamIP`, and under `Runtime\Steam`, `SteamSockets` and `SteamShared`. Note there
is **no `OnlineServicesSteam`** in the newer `OnlineServices` family - that only has EOS - so the
classic `OnlineSubsystem` path is the one to use.

The work:

- Enable `OnlineSubsystemSteam` in `Getaway.uproject`, add `OnlineSubsystem` and
  `OnlineSubsystemUtils` to `Plugins/GetawayMovement/Source/GetawayMovement/GetawayMovement.Build.cs`
  (per CLAUDE.md, a new engine module must be added there), and configure the AppID in
  `Config/DefaultEngine.ini`. A `steam_appid.txt` sits next to the exe during development.
- **Then the real job: replace typing an IP address with Steam sessions.** Create, find and join a
  session, and support the Steam friends invite overlay. This is the single biggest piece of code in
  the whole plan, and it is what makes it a Steam game rather than a LAN game with a Steam icon.
  Nobody buys a multiplayer game on Steam and then asks their friend for an IP address.
- The menu already has the right shape for it - Host, a box, Join - so this is replacing what those
  buttons *do*, not redesigning the front end. `UGetawayMenuSubsystem::FindLocalAddresses` and the
  address display become dead weight and come out.

### 3. Client authority has to be revisited

CLAUDE.md already flags this: *"Client-authoritative multiplayer is fine for local play. Revisit it
before Steam."* This is that moment.

Each machine decides where its own player is. Among friends on a LAN that is a sensible trade.
Among strangers on Steam it means **speed and teleport cheats are trivial in a racing game where
position is the entire score.** Two ways to go, and it is a design call:

- **Accept it.** Plenty of small party games do, and the social cost of cheating in a 4-player
  lobby of friends is self-policing.
- **Validate the parts that score**, server-side - the finish, the death, the timing - while leaving
  movement local. Much smaller than full server-authoritative movement, and it kills the cheats that
  actually matter. Note the related hole already logged in `docs/multiplayer.md`: the server cannot
  tell *where* a client placed a piece from, because the placement pawn's transform is not
  replicated.

Full server-authoritative movement would mean re-doing the thing that has been signed off as feeling
right, so it is not on the table without a very good reason.

### 4. The 946 MB

For a 10 MB pak and a 128 MB container, 946 MB is almost entirely engine and the staged VC redist.
Worth a pass before shipping - it is a real download size on a store page - but it blocks nothing.

## What is not code, and is the actual gate

**Store art, at fixed pixel sizes.** Valve requires a set of capsules and library assets, and these
are hard requirements with no flexibility. Roughly: header capsule 460x215, small capsule 231x87,
main capsule 616x353, vertical capsule 374x448, library capsule 600x900, library header 920x430,
library hero 3840x1240, and a logo. **Check the current Steamworks docs for the live list** - Valve
changes it, and these are from memory plus the docs as of writing, not verified against the page.

**At least five screenshots**, and realistically **a trailer**. A Steam page without a trailer
converts badly enough that it is not optional.

Then store text, tags, genre, system requirements, the content survey and age rating questions.

The unlit pink look helps here more than it might seem - it is distinctive, it photographs well, and
`docs/look.md` says the point of it is that greybox *is* the look. A capsule can be made from this
game as it stands.

## The honest state of the game against a store page

Not an argument against Steam, just the list, so the decision is made with open eyes:

- **One map.** `/Game/Maps/TestCourse` is the only `.umap` in the project.
- **Silent.** No sound assets at all - checked, `Content` has no audio of any kind. There is a
  single empty `ExplosionSound` slot on `AGetawayC4` and nothing fills it. A movement game with no
  footsteps, no wallrun whoosh and no explosion is a much weaker game than the same one with them,
  and audio is probably the highest value-per-hour work left anywhere in the project.
- **Never played by two separate machines.** Still the open unknown from `docs/multiplayer.md`.
- **No lobby**, no matchmaking, no settings page, and a pad cannot drive the menu.
- **One free Fab pack in `Content/ThirdParty/`** (GlowGrid Starter). It is *not* cooked into builds -
  `DefaultGame.ini` deliberately excludes `ThirdParty` - but `CREDITS.md` has two open items on its
  license that should be closed before commercial distribution either way.

## Recommended order

1. **Start the Steamworks paperwork now.** It is slow, it is not code, and everything else waits on
   the AppID.
2. **Switch to Shipping and test the menu's error paths properly.** Needs no AppID, needed for any
   launch.
3. **Play it on two real machines.** Still the biggest unknown in the project, and it would be
   embarrassing to find it on a customer's PC.
4. **Audio.** Cheapest large improvement available.
5. **Steam sessions and invites**, once the AppID exists.
6. **Decide the client-authority question.**
7. **Store art and a trailer**, then the page, then the two-week clock.

A thought worth putting on the record: **itch.io was the better first move**, and the two are not
exclusive. It is free, same-day, has no gatekeeping and no two-week clock, and it is how you find
out whether strangers find the game fun before $100 and a locked release date are involved.
Shipping on itch.io while the Steam paperwork grinds costs almost nothing, because step 2 above - a
Shipping build - is the same work for both.

## Still unverified

Everything in this file. Specifically:

- The Shipping build has never been run.
- No Steam plugin has been enabled, and no AppID exists.
- The capsule sizes above are from memory and should be checked against the live Steamworks docs.
- The $100 fee is confirmed in Valve's docs; the $1,000 recoup and the 30-day first-publisher wait
  are from third-party coverage only.
