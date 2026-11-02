"""
Moves a third-party asset pack into Content/ThirdParty/<PackName>/, the layout CLAUDE.md
asks for, without breaking the references inside it.

  /Game/GlowGridStarter  ->  /Game/ThirdParty/GlowGridStarter

Run it from the editor: Tools > Execute Python Script..., pick this file.

Why a script and not a drag in the Content Browser: the pack is 60-odd assets whose
materials, meshes and maps all point at each other by path (/Game/GlowGridStarter/...).
Moving the folder on disk would break every one of those. unreal.EditorAssetLibrary's
rename is the Content Browser's own Move, so the editor rewrites the references as it goes.

**If the pack contains maps, the editor stops and asks.** Renaming a world pops a modal
dialog - "Source code, config INI, and text files may need Find/Replace for: <map names>
... Continue with rename?" - and answering Cancel aborts the whole move, which is what
happened on the first run, 2026-11-02. The script names the maps up front so the dialog is
expected. Click OK. Deleting the pack's demo maps first avoids the dialog entirely.

It does NOT finish the job. Fixing up redirectors is not exposed to Python in UE 5.8
(IAssetTools::FixupReferencers is not a UFUNCTION), so the script stops and prints what is
left. In 5.8 that menu entry is called **Update Redirector References**, under Bulk
Operations, not "Fix Up Redirectors in Folder" as in older versions.

To move the next pack, change PACK below.
"""

import unreal

PACK = "GlowGridStarter"
SOURCE = "/Game/" + PACK
THIRD_PARTY = "/Game/ThirdParty"
DESTINATION = THIRD_PARTY + "/" + PACK

assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log("[Adrenaline pack move] " + message)


def fail(message):
    unreal.log_error("[Adrenaline pack move] " + message)


def world_paths(paths):
    """The maps among these assets. They are what makes the rename stop and ask."""
    worlds = []
    for path in paths:
        data = assets.find_asset_data(path)
        if data and str(data.asset_class_path.asset_name) == "World":
            worlds.append(path)
    return worlds


def run():
    if not assets.does_directory_exist(SOURCE):
        if assets.does_directory_exist(DESTINATION):
            log("%s is already at %s. Nothing to do." % (PACK, DESTINATION))
        else:
            fail("%s does not exist. Is the pack still called %s?" % (SOURCE, PACK))
        return

    # Refuse to merge into a destination that already holds something: a half-moved pack is
    # far more work to untangle than a move that declined to start.
    if assets.does_directory_have_assets(DESTINATION):
        fail("%s already has assets in it. Sort that out by hand before moving again."
             % DESTINATION)
        return

    before = assets.list_assets(SOURCE, recursive=True, include_folder=False)
    maps = world_paths(before)

    log("Moving %d assets from %s to %s." % (len(before), SOURCE, DESTINATION))
    if maps:
        log("--------------------------------------------------------------------")
        log("%d of them are maps, so the editor is about to put a dialog in front of you:" % len(maps))
        for path in maps:
            log("    " + path)
        log("It says config INI and text files 'may need Find/Replace' for those names and")
        log("asks 'Continue with rename?'. CLICK OK. Cancel aborts the whole move.")
        log("Nothing in this project refers to those maps, so the warning does not apply.")
        log("--------------------------------------------------------------------")

    assets.make_directory(THIRD_PARTY)
    if not assets.rename_directory(SOURCE, DESTINATION):
        fail("The move failed. Nothing has been saved, so the pack is as it was and is still "
             "the only copy - do NOT delete the old folder.")
        if maps:
            fail("Most likely: the 'Continue with rename?' dialog was answered Cancel. Run "
                 "this again and click OK. Or delete the pack's demo maps first, listed "
                 "above, and the dialog will not appear at all.")
        fail("Other causes: one of the pack's own maps being the level you have open (open "
             "/Game/Maps/TestCourse first), or a file checked out elsewhere. The Output Log "
             "above this line says which.")
        return

    # Save now. Until this runs, the moved assets only exist in memory, and a crash or a
    # "don't save" on the way out would leave the pack half in each place.
    if not assets.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True):
        fail("Moved, but saving %s reported a problem. Check the Output Log, and save "
             "everything by hand (File > Save All) before closing the editor." % DESTINATION)

    after = assets.list_assets(DESTINATION, recursive=True, include_folder=False)
    log("%s now holds %d assets." % (DESTINATION, len(after)))
    if len(after) != len(before):
        fail("Started with %d and ended with %d. Check the Output Log for what did not move."
             % (len(before), len(after)))

    leftovers = []
    if assets.does_directory_exist(SOURCE):
        leftovers = assets.list_assets(SOURCE, recursive=True, include_folder=False)

    log("Done moving. What is left, by hand:")
    if leftovers:
        log("  1. Right-click the old %s folder > Update Redirector References. (%d "
            "redirector(s) are in it. The entry is under Bulk Operations, just below "
            "Delete - it was called 'Fix Up Redirectors in Folder' before UE 5.8.)"
            % (PACK, len(leftovers)))
        log("  2. Right-click it again > Delete, to clear the now-empty folder.")
    else:
        log("  1. No redirectors were left behind, so there is nothing to update.")
        log("  2. Right-click the old %s folder > Delete, to clear the empty folder." % PACK)
    log("Only delete that folder once this script has said the move succeeded. Before that "
        "it still holds the whole pack.")
    log("Then add the pack's row to CREDITS.md if it is not there yet.")


run()
