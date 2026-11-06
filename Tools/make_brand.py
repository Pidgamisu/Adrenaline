"""Build the Windows icon and the two splash screens out of the logo.

    python.exe -I Tools/make_brand.py [Branding/adrenaline-logo.png]

Reads `Branding/adrenaline-logo.png` - the 2000x2000 square with the pink field,
the black field and the white A - and writes three files that Unreal picks up by
filename alone, with nothing to set in the editor:

    Build/Windows/Application.ico   the packaged .exe's icon. UnrealBuildTool compiles it into
                                    the executable's resources, and the staging step copies it
                                    beside the exe, so it is what Explorer, the taskbar and
                                    anybody who downloads the build from itch sees.
    Content/Splash/Splash.bmp       the window the packaged game shows while it loads.
    Content/Splash/EdSplash.bmp     the same for the editor, with its bottom 64 px left clear.

Two engine details decide the shape of this script, both read out of UE 5.8's source on
2026-11-06 rather than guessed:

  * **BMP, not PNG.** `GetSplashFilename` in GenericPlatformSplash.cpp prefers `Splash.png` over
    `Splash.bmp`, so a PNG would look right in the editor - but `WinPlatform.Automation.cs` only
    ever stages `Content/Splash/Splash.bmp`. A PNG would be silently missing from every package.
  * **Uncompressed icon entries.** Pillow writes 256 px icon entries as embedded PNG by default,
    and the resource compiler UBT runs over the icon rejects those. `bitmap_format="bmp"` keeps
    every entry a plain DIB.

Needs Pillow, which Unreal's bundled Python does not ship. Install it once, into a folder beside
this script that git ignores:

    "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe" -m pip install --target Tools/pylibs pillow

Then run the line at the top with that same python.exe. It only needs running again if the logo
itself changes.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, "pylibs"))

try:
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
except ImportError:
    sys.exit(
        "Pillow is not installed. Run this first, from the project folder:\n\n"
        '  "C:\\Program Files\\Epic Games\\UE_5.8\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe"'
        " -m pip install --target Tools/pylibs pillow\n"
    )

# The logo's own three colours, sampled out of the PNG. The pink is the logo's, not the
# EDGE_PINK of Content/Python/adrenaline_look.py - that one is a linear value for a material
# and comes out lighter on screen.
PINK = (255, 103, 171)
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

# The same face the menu's title is set in, so the splash and the front end match.
ROBOTO_BOLD = r"C:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts\Roboto-Bold.ttf"

# What the engine's own splashes are.
SPLASH_W, SPLASH_H = 720, 370

# The editor draws its own text over the bottom of its splash - the game name, the engine version,
# the copyright line and "Loading..." - in rects running from bmHeight-60 downwards
# (WindowsPlatformSplash.cpp). Nothing of ours goes below this line on EdSplash. The game's splash
# is custom, which stops the engine writing anything over it at all, so that one gets the full
# height.
TEXT_BAND = 64

ICON_SIZES = [256, 128, 64, 48, 32, 24, 16]

# The A is drawn as thin outlines, so a plain downsample turns it to grey mush below about 48 px.
# Each small size gets its strokes fattened first, by this many pixels measured on a 256 px working
# copy. Judged by eye against a 4x blow-up of every size; above 64 the logo needs no help.
ICON_FATTEN = {16: 5, 24: 3, 32: 2, 48: 1, 64: 0}
WORK = 256


def fatten(img, px):
    """Grow the white strokes outwards by px, leaving the pink and the black alone."""
    if px <= 0:
        return img
    mask = img.convert("L").point(lambda v: 255 if v > 170 else 0)
    mask = mask.filter(ImageFilter.MaxFilter(px * 2 + 1))
    out = img.copy()
    out.paste(WHITE, (0, 0), mask)
    return out


def icon_frames(logo):
    """One drawing per icon size, each fattened for the size it will be seen at."""
    flat = logo.convert("RGB")
    work = flat.resize((WORK, WORK), Image.LANCZOS)
    frames = []
    for s in ICON_SIZES:
        px = ICON_FATTEN.get(s, 0)
        source = fatten(work, px) if px else flat
        frames.append(source.resize((s, s), Image.LANCZOS).convert("RGBA"))
    return frames


def write_icon(logo, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    frames = icon_frames(logo)
    frames[0].save(
        path,
        format="ICO",
        sizes=[(s, s) for s in ICON_SIZES],
        bitmap_format="bmp",
        append_images=frames[1:],
    )


def tracked_text(draw, xy, text, font, fill, tracking):
    """Draw the text a letter at a time with extra space between the letters, the way the menu
    spells out A D R E N A L I N E."""
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += draw.textlength(ch, font=font) + tracking


def tracked_width(draw, text, font, tracking):
    return sum(draw.textlength(ch, font=font) + tracking for ch in text) - tracking


def make_splash(logo, band):
    """band = pixels of the bottom to keep clear for the editor's own text."""
    img = Image.new("RGB", (SPLASH_W, SPLASH_H), BLACK)
    draw = ImageDraw.Draw(img)

    # The mark, left, centred in whatever height is ours.
    mark = 196
    mx, my = 56, (SPLASH_H - band - mark) // 2
    img.paste(logo.convert("RGB").resize((mark, mark), Image.LANCZOS), (mx, my))
    # The logo's own lower half is black, so on a black splash its bottom and right edges
    # disappear. A hairline keeps it reading as the square it is.
    draw.rectangle([mx, my, mx + mark - 1, my + mark - 1], outline=PINK, width=1)

    # The word, right of it.
    font = ImageFont.truetype(ROBOTO_BOLD, 50)
    tracking = 9.0
    word = "ADRENALINE"
    tx = mx + mark + 46
    bbox = draw.textbbox((0, 0), word, font=font)
    ty = my + mark // 2 - (bbox[1] + bbox[3]) // 2 - 10
    tracked_text(draw, (tx, ty), word, font, WHITE, tracking)

    # A pink rule under it: docs/look.md's "pink is architecture".
    rule_y = ty + bbox[3] + 22
    draw.rectangle([tx, rule_y, tx + int(tracked_width(draw, word, font, tracking)), rule_y + 3], fill=PINK)

    sub = ImageFont.truetype(ROBOTO_BOLD, 17)
    tracked_text(draw, (tx + 2, rule_y + 16), "FIRST PERSON MOVEMENT PARTY GAME", sub, PINK, 2.0)

    return img


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(PROJECT, "Branding", "adrenaline-logo.png")
    logo = Image.open(src).convert("RGBA")

    icon = os.path.join(PROJECT, "Build", "Windows", "Application.ico")
    write_icon(logo, icon)

    splash_dir = os.path.join(PROJECT, "Content", "Splash")
    os.makedirs(splash_dir, exist_ok=True)
    make_splash(logo, band=0).save(os.path.join(splash_dir, "Splash.bmp"))
    make_splash(logo, band=TEXT_BAND).save(os.path.join(splash_dir, "EdSplash.bmp"))

    print("logo  ", src)
    print("icon  ", icon, os.path.getsize(icon), "bytes")
    print("splash", os.path.join(splash_dir, "Splash.bmp"))
    print("splash", os.path.join(splash_dir, "EdSplash.bmp"))


main()
