# Releasing on itch.io

Started 2026-11-03. The first public release of Adrenaline. See `docs/steam.md` for why itch.io
comes first - short version: it is free, there is no review and no two-week clock, so the game can
be up the day the build works.

## Why itch.io first

- **No fee and no gatekeeping.** No $100, no identity verification, no store-page review, no
  release-date lock. A project goes from draft to public whenever you click the button.
- **The build is the same work either way.** A Shipping package is step one for Steam too, so
  nothing done here is thrown away.
- **It answers the only question that matters**: do people who are not us find this fun? Spending
  the Steam fee and locking a release date before knowing that is the wrong order.

itch.io takes a revenue share you set yourself (the default is 10%).

## The build

**Switched to Shipping on 2026-11-03.** `Config/DefaultGame.ini` now has
`BuildConfiguration=PPBC_Shipping` and `ForDistribution=True`; the comment in that file explains
what Shipping costs and why all three old blockers are gone.

Packaged with the same command as the Development build in *It is packaged*
(`docs/multiplayer.md`), with `-clientconfig=Shipping` and a separate archive folder so the working
Development build is not overwritten:

    "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
      -project="C:\Users\natha\Documents\Unreal Projects\Getaway\Getaway.uproject" ^
      -noP4 -platform=Win64 -target=Getaway -clientconfig=Shipping ^
      -build -cook -stage -pak -archive ^
      -archivedirectory="C:\Users\natha\Desktop\AdrenalineShipping" -prereqs -nocompileeditor

Keep the Development package. When something goes wrong in a Shipping build there is no `~` console,
no `GetawayNetTest=` harness and no remote-state overlay, so the Development build is the one that
can be debugged.

### What the Shipping package proved (2026-11-03)

`BUILD SUCCESSFUL`, exit 0, first try. The C++ Shipping compile of the game target needed no code
changes at all - the groundwork laid on 2026-11-02 (real ghost components, `AGetawayHUD`,
`AGetawayBlast`) is what made that true, and every remaining `#if !UE_BUILD_SHIPPING` guard is
correctly paired.

**609 MB and 32 files, down from 946 MB and 57.** Shipping saved 337 MB off the download - worth
knowing, because that is the number on a store page. Breakdown: a 159 MB
`Getaway-Win64-Shipping.exe`, the 11 MB pak, the 128 MB `.ucas`, a 3 MB `global.ucas`, and 30 MB of
VC redist. One easy trim is left: `-prereqs` stages `vc_redist.arm64.exe` as well as the x64 one,
and 12 MB of ARM64 installer is no use to a Win64 build.

The cook was listed with `UnrealPak <utoc> -List` and checked asset by asset:

- **All 14 project Blueprints are in**, including all 11 `BP_Piece_*` pieces, `BP_Character`,
  `BP_GetawayGameMode` and `BP_PlacementCamera`. Nothing in `Content/Blueprints` is missing.
- **All 3 `M_VB_` masters and all 14 `MI_VB_` instances**, `MI_VB_Ghost` among them.
- `TestCourse`, and no `ThirdParty`, no `/Game/Python`, no `__ExternalActors__`. The exclusions hold
  in Shipping exactly as they did in Development.

It also **runs**: launched headless, the process stayed alive 35 seconds with a 232 MB working set,
so the engine and the content loaded without crashing.

### Shipping writes no log, and that is a problem worth fixing

`Getaway\Saved\Logs` **is never created** by the Shipping build. `bUseLoggingInShipping` is not set
in `Source/Getaway.Target.cs`, so logging is compiled out by the engine default.

Two consequences:

1. The headless verification used throughout `docs/multiplayer.md` - run with `-nullrhi`, read
   `Getaway.log`, confirm the menu framed the level and the placement phase dealt a hand - **does not
   work on a Shipping build.** There is nothing to read. Checking a Shipping build means looking at
   it on screen.
2. More importantly, **a player who hits a bug has nothing to send you.** For a public release that
   is the wrong trade. Adding `bUseLoggingInShipping = true;` to `GetawayTarget` turns logs back on
   at almost no cost, and is recommended before the first upload.

## Uploading: use butler, not the browser

`butler` is itch.io's command-line uploader. It is worth the ten minutes of setup over dragging a
zip into the browser, for one reason: **it uploads only what changed.** The first push sends the
whole build; every patch after that sends a few MB rather than another half a gigabyte. For a 946 MB
folder that is the difference between a release being easy to update and being a chore.

It also means no zipping - butler takes the folder directly.

**Installed 2026-11-03: butler v15.32.0**, at
`C:\Users\natha\Downloads\butler-windows-amd64\butler.exe`. It runs. Two things are still open:

- **It is not on `PATH`, and it is in `Downloads`**, which is a folder that gets cleared. Move the
  whole `butler-windows-amd64` folder somewhere permanent - `C:\Tools\butler` is a reasonable home -
  and add that folder to `PATH` so `butler` works from any terminal. The `7z.dll` and `c7zip.dll`
  beside `butler.exe` must travel with it.
- **`butler login` has not been run.** There is no credentials file. This has to be done by hand
  because it opens a browser and asks you to authorise the account - it is not something to
  automate.

**The project exists, created 2026-11-03: <https://rpidgey.itch.io/adrenaline>**, which makes the
butler target `rpidgey/adrenaline`. It was a draft at creation - a HEAD request as an anonymous
visitor returned 404, which is how to check that from a terminal without logging out:

    Invoke-WebRequest -Uri "https://rpidgey.itch.io/adrenaline" -Method Head -UseBasicParsing

404 means draft or restricted; 200 means the whole world can see it. **Worth running before any
push**, because a build pushed to a public page is downloadable the moment it finishes processing.

Then every release is one command, via the wrapper in `Tools/itch/`:

    .\Tools\itch\push.ps1 -Target rpidgey/adrenaline

Note that `butler status rpidgey/adrenaline` says `No channel found` until the first push has
processed. That is not an error - it means the game exists and butler is authenticated, but nothing
has been uploaded to it yet. A target that does not exist fails differently.

The wrapper is there because a raw `butler push` would miss a step: it copies `.itch.toml` into the
build root first (see below). It also refuses to run if the folder does not look like a staged
build, and warns if it looks like a Development build rather than a Shipping one, so a wrong folder
cannot be published by accident. `-DryRun` prints what it would do and stops; `-Channel`,
`-BuildDir` and `-Butler` override the defaults if any of them move.

Notes from the butler docs:

- **The channel name sets the platform tag.** A channel containing `win` or `windows` is tagged as a
  Windows build, which is what makes the itch app offer it to Windows users. Use lowercase with
  dashes.
- Builds get an auto-incrementing version per channel unless `--userversion` is passed.
- The backend rejects builds over 30 GB uncompressed, so 946 MB is nowhere near a limit.

### The first push (2026-11-03)

It worked. `push.ps1 -Target rpidgey/adrenaline`, exit 0:

- **608.78 MiB uploaded** across 33 files, at 10-15 MiB/s.
- **Stored as a 323.59 MiB patch - a 46.85% saving.** itch compresses and diffs server-side, so the
  download a player gets is a good deal smaller than the folder on disk. The 609 MB figure is what
  leaves this machine, not what a player pulls down.
- butler printed `Scanning launch targets...`, which is it reading the build to work out what can be
  launched.
- **Processing took about 75 seconds** after the upload finished. During that window
  `butler status` keeps saying `No channel found`, which is alarming and means nothing - the build
  is being compressed and verified. It is worth waiting through before concluding anything is wrong.

Result:

    +---------+-----------+------------+---------+
    | CHANNEL |  UPLOAD   |   BUILD    | VERSION |
    +---------+-----------+------------+---------+
    | windows | #20255107 | √ #2287901 |       1 |
    +---------+-----------+------------+---------+

The √ is the thing to look for: it means the build processed without errors. The page was still a
draft throughout, confirmed by a 404 to an anonymous HEAD request before and after.

### Pushing an update (2026-11-03, version 2)

The map was edited in the editor, so the build had to be remade before pushing. **Updating a
release is two commands, in that order** - package, then push:

1. Close the editor, then run the `BuildCookRun` command in *The build* above. **Cooking is a full
   cook, not incremental**, and that is the engine's own default - `BaseEditor.ini` in the engine
   folder carries `[CookSettings] CookIncrementalDefaultIncremental=false`, and nothing in this
   project overrides it. So all 548 packages are recooked every time and a changed `.umap` is
   certain to be in. It took **88 seconds** end to end, because the C++ was already built - a map
   change costs a cook and a stage, not a compile.
2. `.\Tools\itch\push.ps1 -Target rpidgey/adrenaline`.

**The patching is the payoff.** The upload re-used 39.49% of the old build and sent 368 MiB fresh,
which itch stored as a **129 MiB patch - a 78.76% saving** against the 609 MiB folder. butler
printed `last build is 2287901, downloading its signature`, which is it fetching what is already up
there to diff against. Processing finished inside 15 seconds this time, not 75.

    +---------+-----------+----------------------------+---------+
    | CHANNEL |  UPLOAD   |           BUILD            | VERSION |
    +---------+-----------+----------------------------+---------+
    | windows | #20255107 | √ #2293004 (from #2287901) |       2 |
    +---------+-----------+----------------------------+---------+

`(from #2287901)` is the patch chain - a player on version 1 downloads the 129 MiB diff, not the
whole game again.

**The page was public by this point.** An anonymous HEAD request returned **200**, not the 404 it
gave at creation. Nothing records when it changed. The consequence: a push is now a *publish* - the
build is downloadable by anyone the moment processing ends, with no second click. **Run the HEAD
check before every push**; that is exactly what it is for.

### A trap: running the package command from a bash shell

`RunUAT.bat` failed instantly with `'C:\Program' is not recognized as an internal or external
command`, even with the path quoted, when launched from a POSIX shell - the quoting does not survive
the hand-off into `cmd`. **Run it from PowerShell with the call operator**:

    & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ...

Same for `push.ps1`. Nothing is wrong with the project when this happens; it is the shell.

## Telling the itch app which exe to run

The staged `Windows` folder has `Getaway.exe` and, because of `-prereqs`, two VC redist installers.
The itch app will see all three and may offer an installer as something to launch. A `.itch.toml` at
the root of the build fixes that, and it is now written, at `Tools/itch/.itch.toml`:

    [[actions]]
    name = "play"
    path = "Getaway.exe"

It has to live at the root of the *staged build*, not in the project, and the staged build is
replaced wholesale by every package. Rather than add it to the project's staging rules, which is
more machinery than two lines deserve, `Tools/itch/push.ps1` copies it in on every push. That has
been dry-run tested; whether the itch app actually honours it is unproven until a real upload exists
to install from.

## The page

What itch.io actually asks for:

- **Title**, a short description ("tagline"), and the long description.
- **Classification** *Game*, **kind of project** *Downloadable*.
- **Cover image.** 630x500 is the recommended size. Browse listings render covers at 315x250 as a
  centre crop, so keep anything important away from the edges. Both numbers come from itch.io
  community posts rather than an official page - **the upload dialog on the project itself shows the
  current recommended size and the file-size limit**, so check there.
- **Screenshots.** No fixed size. At least four or five.
- A trailer is optional here, unlike Steam, but a short gif or video of movement at speed would do
  more for this game than any screenshot can - the whole appeal is motion.
- **Genre and tags**, and the Windows platform checkbox.
- **Pricing**: free, paid, or "name your own price". For a first public release of a one-map game,
  free is the honest option and the one that gets played.
- **Visibility**: draft, restricted or public. Draft is private; restricted lets specific people in.
  Both are useful for a look before going public.

The unlit pink look is an asset here. It is distinctive at thumbnail size, which matters on a
storefront where a cover is 315 px wide.

## The thing that most needs deciding: multiplayer

**This is the real risk of a public release, and it is not a code bug.**

The game joins by typing an IP address. For that to work over the internet the host has to forward
UDP 7777 through their router, and both machines have to answer a firewall prompt. Most people will
not do that, and a stranger who downloads this and cannot get a game with a friend will simply
assume it is broken.

On top of which, per *Still not done* in `docs/multiplayer.md`: **two separate machines have never
connected.** Everything so far has been one process, or two on the loopback. Publishing a
multiplayer game whose multiplayer has never once crossed a network is a bet.

Three honest options:

1. ~~**Test two machines first, then release.**~~ **Done 2026-11-03** - two machines in two
   different states played, over a **Tailscale** tailnet, and it worked. This was the oldest open
   item in the project. It does not let the page stay quiet about connecting, though: a stranger is
   not on your tailnet, so the page has to say **UDP 7777 forwarded on the host, or both of you on
   a mesh VPN**.
2. **Release it and describe it accurately** - "local/LAN multiplayer, direct IP" on the page, in
   the tagline, not buried in the description. Expect it to limit who plays together.
3. **Release it as a single-player time-attack** and say multiplayer is coming. The placement phase
   and the course work fine solo, and it sets expectations honestly.

Option 1 does not block making the page, writing the description or preparing the art, all of which
can happen now.

## Played and signed off (2026-11-03)

**The first time a packaged build of this game has been looked at on screen, and it all works.**

That matters more than it sounds. Every check before this was headless - `-nullrhi`, read the log,
confirm things loaded - and Shipping is precisely the configuration that strips the three features
this game leaned on debug draw for. All three survived:

- the **placement ghost** draws,
- the **HUD messages** appear,
- the **C4 explosion** is visible.

So the 2026-11-02 groundwork - real ghost components, `AGetawayHUD`, `AGetawayBlast` - did exactly
what it was for, and the long-standing worry that a distributable build would lose them is closed.

**Escape was also played for the first time ever**, and Resume and Quit both work. Those had never
been pressed outside the editor in any configuration, because the editor binds Escape to Stop.

**Leave to menu and a failed join were played too, and both work.** That is the whole front end
exercised: Resume, Leave to menu, Quit, and a join that fails reporting properly and letting you
back out - in a Shipping build, where there is no console to recover with if any of them had hung.

Leave was the one worth worrying about, because `docs/multiplayer.md` records input going dead after
the Host travel and Leave is also a travel. Reading the code first showed why it is safe:
`LeaveToMenu` calls `HideMenu` before travelling, and `HideMenu` ends in
`ApplyInputMode(false)` - `FInputModeGameOnly`, which is the call that puts the viewport's
`SetIgnoreInput` back to false. Host was vulnerable precisely because it travelled *with* a panel
still up. Leave tears its panel down first, so the flag never crosses the travel.

Still not clicked: the **Cancel** button itself, as opposed to the give-up clock it shares a code
path with.

## Still unverified

- **The Cancel button on a join has not been clicked**, only the give-up clock it shares a code path
  with. The smallest open item left in the front end.
- ~~**Two machines have never connected.**~~ **Two did on 2026-11-03**, from two different states,
  over a Tailscale tailnet, and it worked - smooth movement, and every replicated system in step.
  **A stranger hosting is still unproved**, and needs port forwarding or a VPN of their own. See
  *Two machines, two states* in `docs/multiplayer.md`.
- ~~**`butler login` has not been run and no itch.io project exists yet.**~~ **Both done 2026-11-03**:
  butler is authenticated (credentials at `C:\Users\natha\.config\itch\butler_creds`) and the
  project is <https://rpidgey.itch.io/adrenaline>.
- ~~**Nothing has been uploaded.**~~ **Done 2026-11-03** - see *The first push* above, and
  *Pushing an update* for version 2. Build `#2293004` is live on the `windows` channel, and the
  page is **public**, so that build is downloadable by anyone right now.
- **Nobody has downloaded and run a build from the page.** Every look at a packaged build so far
  has been the folder on this machine, not an install pulled from itch.
- **Whether the itch app honours `.itch.toml` is still unknown.** It needs the draft installed
  through the app itself, which nobody has done. butler's `Scanning launch targets...` line is
  encouraging but not proof.
- **The page itself is empty.** The build is uploaded; none of the text in `docs/itch-page.md` has
  been pasted in, and there is no cover image, no screenshots and no video.
- Cover image dimensions are from community posts, not official docs.
- `CREDITS.md` has two open items on the GlowGrid Starter license. The pack is not cooked into the
  build - `DefaultGame.ini` excludes `ThirdParty` - so it does not ship, but closing them is tidy.
