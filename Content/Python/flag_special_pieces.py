"""
Copies each piece's `bSpecial` flag from the C++ catalog onto BP_GetawayGameMode's Catalog,
so the party box can deal one normal piece and one special one.

  Run it from the editor: Tools > Execute Python Script..., pick this file.

Why it is needed. BP_GetawayGameMode holds its own copy of the Catalog array and stopped
tracking C++ the moment dress_pieces.py first wrote it, so `bSpecial` - added to
FGetawayPieceDef on 2026-11-06 - reads false on all fourteen rows there however the C++ is
written. With every row reading normal the one-of-each deal has no special half to draw from,
and you get a normal piece plus a free one, which is the old behaviour with extra steps.

This is the small, safe way to fix that: it reads the flag out of AGetawayGameMode's class
defaults, matches on Id, and writes **only** `bSpecial` back onto the Blueprint's rows. Actor
Class, size and every other field keep whatever the Blueprint already holds, and no material is
touched - which is the whole reason this exists rather than "re-run dress_pieces.py", since that
script rebuilds the array from scratch and recompiles nine piece Blueprints to do it.

Safe to run twice; it writes nothing if every row already agrees.

After a *new* piece is added to the C++ catalog, run dress_pieces.py instead - only that one can
add a row. This script just corrects the flag on rows that already exist.

Needs the GetawayMovement plugin compiled, and BP_GetawayGameMode to exist.
"""

import unreal

GAMEMODE_PATH = "/Game/Blueprints/BP_GetawayGameMode"
GAMEMODE_FOLDER = "/Game/Blueprints"
GAMEMODE_NAME = "BP_GetawayGameMode"

assets = unreal.EditorAssetLibrary

_problems = []


def log(message=""):
    unreal.log("[Adrenaline special] " + message)


def fail(message):
    _problems.append(message)
    unreal.log_error("[Adrenaline special] " + message)


# What `bSpecial` is called through `get_editor_property`. The engine's own scripts pass the raw
# `bFoo` name, while the generated Python stubs strip the leading b and snake_case the rest - and
# which of those a struct answers to is not worth betting a silent failure on, so try each and
# remember the one that works. `read_special` fails loudly if none of them do, rather than reporting
# fourteen normal pieces.
SPECIAL_NAMES = ("bSpecial", "special", "b_special")
_special_name = None


def special_name(entry):
    """The name this build answers to for bSpecial, worked out once off a real struct."""
    global _special_name
    if _special_name is None:
        for name in SPECIAL_NAMES:
            try:
                entry.get_editor_property(name)
            except Exception:
                continue
            _special_name = name
            log("reading the flag as '%s'." % name)
            break
    return _special_name


def read_special(entry):
    name = special_name(entry)
    if name is None:
        raise RuntimeError(
            "no piece property matching bSpecial (tried %s). Is the C++ built? The flag was added "
            "to FGetawayPieceDef on 2026-11-06." % ", ".join(SPECIAL_NAMES))
    return bool(entry.get_editor_property(name))


def write_special(entry, value):
    entry.set_editor_property(special_name(entry), bool(value))


def generated_class(blueprint):
    """The runnable `_C` class of a Blueprint. Tries the cheap routes, then the asset path."""
    try:
        cls = blueprint.generated_class()
        if cls:
            return cls
    except Exception:
        pass
    try:
        cls = blueprint.get_editor_property("generated_class")
        if cls:
            return cls
    except Exception:
        pass
    return unreal.load_object(None, "%s/%s.%s_C" % (GAMEMODE_FOLDER, GAMEMODE_NAME, GAMEMODE_NAME))


def run():
    native = getattr(unreal, "GetawayGameMode", None)
    if native is None:
        fail("unreal.GetawayGameMode does not exist. Is the GetawayMovement plugin compiled "
             "and enabled?")
        return

    native_catalog = unreal.get_default_object(native).get_editor_property("catalog")
    if not native_catalog:
        fail("AGetawayGameMode's own Catalog came back empty, so there is nothing to read. "
             "Is the C++ built?")
        return

    # The truth about which piece is special lives in C++ and only in C++, the same deal as size
    # and the directional and surface-mounted flags. This script never names a piece itself.
    wanted = {}
    try:
        for entry in native_catalog:
            wanted[str(entry.get_editor_property("id"))] = read_special(entry)
    except Exception as error:
        fail(str(error))
        return

    specials = sorted(name for name, is_special in wanted.items() if is_special)
    if not specials:
        fail("C++ says not one of the %d pieces is special, which cannot be right - six rows are "
             "wrapped in Special() in AGetawayGameMode. Nothing written. Is the build current?"
             % len(wanted))
        return
    log("C++ says %d of %d pieces are special: %s"
        % (len(specials), len(wanted), ", ".join(specials) or "none"))

    if not assets.does_asset_exist(GAMEMODE_PATH):
        fail("%s is not there. Run setup_tuning_blueprints.py and dress_pieces.py first."
             % GAMEMODE_PATH)
        return

    blueprint = assets.load_asset(GAMEMODE_PATH)
    cls = generated_class(blueprint)
    if cls is None:
        fail("could not find %s_C. Set the Catalog's Special tickboxes by hand instead."
             % GAMEMODE_NAME)
        return

    defaults = unreal.get_default_object(cls)
    catalog = defaults.get_editor_property("catalog")
    if not catalog:
        fail("%s's Catalog is empty, so there is nothing to flag. Run dress_pieces.py first."
             % GAMEMODE_NAME)
        return

    entries = []
    changed = []
    unknown = []
    for source in catalog:
        # A copy: these structs belong to the class defaults and must not be edited in place.
        entry = source.copy()
        entry_id = str(entry.get_editor_property("id"))
        if entry_id not in wanted:
            unknown.append(entry_id)
        else:
            is_special = wanted[entry_id]
            if read_special(entry) != is_special:
                write_special(entry, is_special)
                changed.append("%s -> %s" % (entry_id, "special" if is_special else "normal"))
        entries.append(entry)

    for entry_id in unknown:
        log("Blueprint row '%s' is not in the C++ catalog; left exactly as it is." % entry_id)

    for entry_id in sorted(set(wanted) - set(str(e.get_editor_property("id")) for e in catalog)):
        fail("%s's Catalog has no '%s' row, so that piece is never dealt at all. Run "
             "dress_pieces.py, which is the only script that can add one." % (GAMEMODE_NAME, entry_id))

    if not changed:
        log("Every row already agrees with C++. Nothing written.")
    else:
        log("Changing %d row(s): %s" % (len(changed), ", ".join(changed)))
        defaults.set_editor_property("catalog", entries)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        # Forced, because writing to a class default object does not dirty the package - the bug
        # recorded in docs/blueprint-tuning.md, and the reason an afternoon of dressing once looked
        # like it had been thrown away. An ordinary save here would log "already saved" and leave
        # the flag alive only until the editor is closed.
        try:
            saved = assets.save_loaded_asset(blueprint, False)
        except Exception:
            saved = assets.save_loaded_asset(blueprint)
        if not saved:
            fail("the save of %s did not report success. Open it, press Compile then Save by hand "
                 "before playing, or the flags are lost when the editor closes." % GAMEMODE_NAME)

    # Read it back off the class defaults rather than trusting the write, the same check
    # dress_pieces.py does. A set_editor_property on a CDO does not dirty the package on its own
    # (see docs/blueprint-tuning.md), so "it says Succeeded" is not evidence on its own.
    written = unreal.get_default_object(cls).get_editor_property("catalog")
    agreed = 0
    for entry in written:
        entry_id = str(entry.get_editor_property("id"))
        if entry_id not in wanted:
            continue
        if read_special(entry) == wanted[entry_id]:
            agreed += 1
        else:
            fail("row '%s' still reads %s. Tick Special by hand in BP_GetawayGameMode > Catalog."
                 % (entry_id, read_special(entry)))
    log("%d of %d rows now match C++." % (agreed, len(wanted)))


log("======================== START ========================")
run()
if _problems:
    log()
    log("Finished with %d problem(s):" % len(_problems))
    for problem in _problems:
        log("  - " + problem)
else:
    log("Done. The party box can tell a special piece from a normal one.")
log("========================= END =========================")
