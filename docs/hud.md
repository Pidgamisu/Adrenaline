# The HUD: three things, bigger and glowing

Everything on screen during a round is `AGetawayHUD`, drawn with the canvas in `DrawHUD`. There is
no UMG in it and no widget asset: the front end is the only part of this game built out of Slate
widgets (`UGetawayMenuWidget`), and the in-game readouts are all text and tiles on the canvas.

This file is about **how** it is drawn. What each readout says, and why, is in the file for the
thing it reports: `docs/round-loop.md` for the clocks and the scoreboard, `docs/placement.md` for
the build phase's lines, `docs/effects.md` for the effect banner.

## The pass of 2026-11-06

Asked for in one line: *larger, glowing, and wrapping around the player vision slightly*. Built the
same day, then cut back twice on the same day - first to three readouts, then flat. **Not played
yet.**

Read the whole file in order and it is the history; what the HUD **is** right now is: bigger text
rasterised at its real size, a glow under every line, three readouts in a run, no frame, no curve.
The size work is the part that survived unchanged, and it is the part that was a bug fix as much as
a look change.

Three things now happen to every line of text on its way out, and no call site knows about any of
them. That is the whole design: `DrawRunClock` still just asks for a clock at a size and a place.

### 1. Size, and why it had to stop using the engine's fonts

The HUD used to ask `GEngine->GetSmallFont()` and `GetLargeFont()` for a **multiple** of
themselves - the speedometer was `Scale 2.2`, a hint line was `Scale 1.0`.

Both of those are the same font, and it is 10 px.

`BaseEngine.ini` has `SmallFontName`, `LargeFontName` and the rest **commented out** (lines 149-153),
and `UEngine::InitializeFontObject` falls back to `CreateFontObjectFromDefaultFont(..., "Regular", 10)`
when the name is empty. So `GetSmallFont()` and `GetLargeFont()` both return one runtime Roboto face
made at size **10**, and the comment in `DrawEffectBanner` claiming the large font "has the pixels"
for a banner was simply wrong - there was never any difference between them.

That matters because the runtime font cache rasterises a glyph at the font's own size and the canvas
item then **stretches the quad**: `FCanvasSimpleTextItem::DrawStringInternal_RuntimeCache` caches at
`FontScale = DPIScale` and never looks at `Scale`. So the old speedometer was a 10 px raster blown
up to 22 px, and "make the HUD larger" by turning `UIScale` up would have meant nothing but more
blur - the biggest text on screen would have been the worst-looking.

So text now goes through `AGetawayHUD::Face(Px, bBold)`, which takes
`GEngine->GetSmallFont()->GetLegacySlateFontInfo()` and overrides its `Size` and
`TypefaceFontName`. Glyphs are then cached at the size they are drawn at, so **`UIScale` costs
nothing in sharpness**, and `Bold` exists for the first time (the speedometer, the clocks, the
banners, your hand, the movement state).

### The trap that cost a build: canvas text needs a UFont, silently

The obvious way to write `Face` is the way the menu does it,
`FCoreStyle::GetDefaultFontStyle("Bold", 33)`. That version drew **every tile and not one letter**,
with nothing in the log.

```cpp
// FCanvasSimpleTextItem, CanvasItem.h
FCanvasSimpleTextItem(const FVector2D&, const FSlateFontInfo& InFontInfo, const FLinearColor&)
    : Font(Cast<const UFont>(InFontInfo.FontObject))   // null for a FCoreStyle font info
...
// CanvasItem.cpp
bool FCanvasSimpleTextItem::HasValidText() const { return Font != nullptr; }
```

and `FCanvasTextItemBase::Draw` returns early when `HasValidText()` is false. A font info built by
`FCoreStyle` carries a **composite font** and leaves `FontObject` null, so the item is dropped on
the floor without a warning, a check or an ensure. Slate widgets are perfectly happy with such a
font info - which is exactly why the menu can use it and the HUD cannot.

Going through the `UFont` loses nothing: a `UFont` whose `RuntimeFontSource` is `CoreStyleDefault`
returns `FCoreStyle::GetDefaultFont()` as its composite font (`UFont::GetCompositeFont`), and
`GetLegacySlateFontInfo()` hands back `FSlateFontInfo(this, LegacyFontSize, LegacyFontName)` with
the UFont as the object. Same Roboto, same typefaces, and `DrawStringInternal_RuntimeCache` reads
the `Size` off the font info we pass rather than off the font, so overriding it is what makes the
text sharp.

`Blit` now also falls back to the old `AHUD::DrawText` path if it is ever handed a font info with no
`FontObject`. Blurry text beats no text, and no text is a bug that announces itself only by being
invisible.

Sizes live in one `namespace Px` block at the top of `GetawayHUD.cpp`, in px before `UIScale`. They
are the sizes the old HUD worked out to - `Scale 2.2` on a 10 px face became `Px::Speedo = 22` -
which makes `UIScale` mean exactly what it says: **1.5 is half again the size this was always played
at.**

Measuring moved with it. `AHUD::GetTextSize` only takes a `UFont`, so widths come from
`FEngineFontServices::Get().GetFontMeasure()` instead. `MeasureTo` uses the
`Measure(Text, Start, End, ...)` overload, which gives the width of the first *n* characters without
allocating a substring and keeps the kerning against the character before.

### 2. The curve

`Warp(X, Y)` is the whole of it:

```
Out  = (X - half the screen) / half the screen        -1 at the left edge, +1 at the right
Keep = 1 - WrapAmount * Out * Out
Y'   = middle + (Y - middle) * Keep
```

Sideways position is untouched; the **vertical distance from the middle of the screen is squeezed**
the further out you are. That is what a cylinder wrapped around your eye does to anything printed on
it: the surface leans away at the edges, so a thing at the top of it sits lower than it would on
flat glass. Squared, so the middle is flat and the bend builds towards the edges.

At `WrapAmount` 0.15 on a 1920 x 1080 screen, something 500 px above the middle of the screen at the
very edge drops about 70 px. The top corners hang lower, the bottom corners ride higher, and nothing
in the middle moves at all - so the clock and the speedometer, which you read, are exactly where
they were, and the furniture around them curls towards you.

`Bent` then bows each line **along its own length** as well. It walks the line a character at a time
and breaks it wherever the bow has moved on, then draws each piece as a string, so spacing and
kerning inside a piece are the font's own and only the joins step. The step is a tenth of the line's
own bow, which caps a line at about ten pieces however hard it curves - a 2 px joint under a glow is
invisible, and a thousand draw calls for one hint line is not. A short line, or any line near the
middle of the screen, bows less than a pixel and is drawn in one go: that early-out is what keeps
the glow affordable.

`TextBend` is how much of the surface's bow a line takes on, and it is **0.4 rather than 1** on
purpose. At 1 a long hint line in the bottom corner sweeps up 60 px across its own width, which
reads as a mistake rather than as a curve. The layout does most of the wrapping; the text only
leans into it.

**`AnchorX` is why columns do not wobble.** A line hangs from one height, taken at its alignment's
anchor - `CX` for centred text, `Right` for the scoreboard, its own X for left-aligned. Hang a
right-aligned row from its *own left edge* instead and two rows of different name lengths get
different heights, which jitters the column by a few px as the status words change.

### 3. The glow

`Glow` lays rings of the text's own colour under it before drawing it: eight copies to a ring, ring
*n* at radius *n* and alpha `GlowStrength / n²`.

**It cannot be additive.** `FCanvasSimpleTextItem::GetTextBlendMode` has a
`check(BlendMode == SE_BLEND_Translucent || ...)` that fires whenever the runtime font cache is in
use, which is always, because that cache is an alpha-only texture and the shader has to match. So
asking for `SE_BLEND_Additive` on canvas text is not a worse glow, it is an assert. Stacked
translucent copies over this game's black world come to the same thing.

`GlowStrength` is the alpha of **one** copy, and the copies overlap, so it is much lower than the
brightness you see: 0.18 reads as about half-lit at the edge of a glyph. The radius is widened by
the square root of the text's size against a middling 14 px line, or the banners would wear the same
hairline halo as a hint line and read as flat beside them.

The one allocation worth caring about is `FText::FromString`, so `Glow` makes it once and hands the
same `FText` to all seventeen draws.

### 4. The visor frame, which is now off

Two arcs, stitched out of tiles, across the top and bottom of the screen, bowed by the same `Warp`
as everything else and faded out at both ends.

They were there because **a slight curve is invisible without something straight to argue with**. A
bent line of text on its own just looks like a bent line of text; two arcs that bow the same way say
the whole HUD is wrapped around you. Together they make a lens shape, which was the point.

**`bVisorFrame` is off by default since the strip below**, asked for the same day they were built.
Against a black world with three readouts left on it they were the loudest thing on screen. The code
is all still here and still tuned; one tick puts them back.

They are **tiles rather than lines** because `FBatchedElements::AddLine` forces alpha to 1 on the way
in - *"Ensure the line isn't masked out. Some legacy code relies on Color.A being ignored."* - so a
canvas line cannot be faint, and a frame like this has to be faint. The gaps between stitches also
hide the fact that each stitch is level while the arc it belongs to is not.

Pink, out of the look's own palette (`EDGE_PINK`, 1.00 / 0.35 / 0.68 - see `docs/look.md`), because
pink is what this game is. It is the only pink on the HUD: the readouts keep the colours they always
had, and the colour rule - cyan helps you, lime kills you - is about the world, not the interface.

## The strip (2026-11-06, the same day)

Seen running, the HUD was still saying too much. What a run now carries is **three things**:

| | |
|---|---|
| `HANDS empty` | top left - what is in your hand, and nothing about what to press |
| `0.00 s` | top centre - the run clock |
| `0 cm/s` | bottom centre - the speedometer |

plus the crosshair, the messages under it, the scoreboard when there is somebody to stand against,
and the banners when the round ends. Gone from the run:

- **The visor arcs.** Off, as above.
- **The key list** under your hand - the hook, C4's two keys and the forfeit. A run is where you
  already know your keys. **The placement phase keeps its own**, because that camera, the wheel for
  picking a piece and the key that turns it are taught by nothing else in the game.
- **The movement state** - `GROUND`, `AIR`, `SLIDE`, `WALLRUN`, `MANTLE` - which also means the one
  colour cue for what the movement component thinks you are doing is gone. The information is still
  one line of code away (`UGetawayMovementComponent::GetStateName`), and the obvious place to put it
  back without the clutter is the **colour of the speed number** rather than a word of its own.
- **The u/s line.** It was there to compare against `Reference/getaway-lab.html`, which is a thing
  you do while tuning movement, not while playing. 1 u = 2.5 cm if you ever need it in your head.

The speedometer moved down with them, from `Bottom - 125` to `Bottom - 100`: it had a state line
above it and a unit line below it holding it in place, and with both gone it floated in the middle
of an empty third of the screen. The slide cooldown tell, the only other thing that can appear down
there, moved to `Bottom - 128` to sit just above it.

`UIScale` came down from 1.5 to **1.3** in the same pass.

## And then the curve came out too (same day, an hour later)

**`WrapAmount` and `TextBend` are both 0. The HUD is flat.**

This is the strip finishing what it started, and it is worth writing down as a lesson rather than a
setting. The arcs were what made the wrap legible, and the two longest lines of text - the key lists
- were what showed the bend. Strip those out and what is left is three short readouts, two of them
centred, where the curve is flat by definition. The only thing still showing any of it was the hand
line in the corner, hanging low and tilting up towards the middle, with nothing anywhere near it to
say why - which reads as *crooked*, not as *wrapped*.

**A curve needs something straight to argue with, and a frame to belong to. Three readouts on a
black screen are neither.** It was not that 0.15 was too much; it is that the thing the curve was
for had been removed, and the right move was to take the curve with it rather than to tune it down.

Everything that draws it is still here, costs nothing while it is off (`Warp` returns the position
it was given, and `Bent` falls straight through to a single draw), and comes back with two numbers:
`WrapAmount` 0.15, `TextBend` 0.4, and `bVisorFrame` on to make it mean anything.

## The knobs

All on `AGetawayHUD`, under **Getaway|HUD**:

| Knob | Default | What it does |
|---|---|---|
| `UIScale` | 1.3 | Every size and every gap. 1 is the HUD as it was before this pass |
| `WrapAmount` | **0** | How far the layout bows around you. Flat since the curve came out; 0.15 was the setting |
| `TextBend` | **0** | How much of that bow a line takes on along its own length. Moot while `WrapAmount` is 0; 0.4 was the setting |
| `GlowRadius` | 2 | Px the halo reaches at `UIScale` 1, widened by the text's size |
| `GlowStrength` | 0.18 | Alpha of one copy of the halo. 0 is no glow |
| `GlowRings` | 2 | 1 is a tight edge, 2 a soft bloom, 3 a fog. Eight draws a ring |
| `CrosshairSize` | 2 | Half the dot, in px at `UIScale` 1 |
| `bVisorFrame` | **off** | The two arcs. Off since the strip; one tick puts them back |
| `VisorColour` | the look's pink | |
| `VisorAlpha` | 0.35 | |
| `VisorStitches` | 56 | Stitches in each arc |

**None of them can be reached from the Details panel yet.** `AGetawayGameMode` sets
`HUDClass = AGetawayHUD::StaticClass()` in C++ and there is no Blueprint child, so these are C++
defaults - the same gap `docs/effects.md` records for `EffectBannerTime`. Fixing it is six clicks and
fixes both at once:

1. Content Browser ▸ `Content/Blueprints` ▸ right-click ▸ **Blueprint Class**.
2. Expand **All Classes**, search `GetawayHUD`, pick it, **Select**.
3. Name it `BP_GetawayHUD`.
4. Open `BP_GetawayGameMode` ▸ **Class Defaults** ▸ **Classes** ▸ **HUD Class** ▸ `BP_GetawayHUD`.
5. Compile and Save both.

Then everything in the table above is a field in `BP_GetawayHUD`'s Details panel, live between
plays.

## How it was checked without playing it

`AGetawayPlayerController::HostGame` travels with `?Play=1`, and that option is all the game mode
needs to open in a round rather than the menu - so the whole thing can be launched from a terminal
and photographed:

```
UnrealEditor.exe Getaway.uproject /Game/Maps/TestCourse?Play=1?Name=Nathan
    -game -windowed -ResX=1600 -ResY=900 -nosplash -ExecCmds="shot showui"
```

The PNG lands in `Saved/Screenshots/WindowsEditor/`. `showui` is the part that matters: without it
the capture is the scene with no canvas on it, which would have looked exactly like the bug. This
is worth remembering for anything else on the HUD - it is a 30 second check that does not need the
editor open.

## Still unverified

- **Nobody has played it.** Both passes were checked by the screenshot trick above, which proves
  what is on screen and nothing about what it is like to read at 800 cm/s.
- **Seen in a run, not in a build phase.** The placement HUD - the hand list, the status line, the
  build clock, the two key lists it keeps - uses the same `Face`/`Bent`/`Glow` path and nothing
  specific to it has been looked at. Nor has a banner, a scoreboard with two names in it, or the
  effect announcement. The build phase is now where almost all the HUD's remaining text lives, so
  it is also where a layout mistake would show up.
- ~~**Whether the curve still reads as a curve.**~~ Settled by taking it out - see *And then the
  curve came out too*. What is left to verify there is only that nothing looks oddly placed now
  that every line is level again; the running HUD was screenshotted flat and looks right.
- **Whether the glow is too much over bright geometry.** The halo is translucent, and the world is
  additive pink: text over a wall is text over something already bright, so a glow that reads well
  against black may read as mush against a lit face.
- **Cost.** A worst-case frame is a few hundred canvas text draws rather than a dozen. It is well
  within what a canvas handles, but it has not been measured with `stat unit`, and this is a game
  that cares about frame time. `GlowRings 1` halves it; `TextBend 0` roughly halves it again.
- **Small windows.** The message stack is lifted off the speedometer when a full stack of four
  would reach it. Worked out on paper, and the strip gave it more room than it needs.
- **A pad cannot drive any of it**, same as before - this pass changed nothing about input.

## What this pass did not do

- **The slide cooldown tell.** `docs/sound.md` records it as a debt this HUD owes: every other piece
  of movement feedback has a visual, and nothing says when your next slide is ready. There is still
  only the `slide 0.4s` line above the speedometer, which is text in the bottom third of the screen
  rather than *a small tell near the crosshair*. The curve and the glow make a little arc under the
  crosshair cheap to draw now, but that is its own job.
- **The menu.** `UGetawayMenuWidget` is Slate and untouched by any of this. The in-game Escape panel
  is the menu, so it does not glow or curve either.
- **Colours.** Every readout kept the colour it had.
