# Tuning movement in the editor

Added 2026-10-26. How to change a feel value and keep it, without touching C++.

## The two Blueprints

| Asset | Parent | What it is for |
|---|---|---|
| `/Game/Blueprints/BP_Character` | `AGetawayCharacter` | The pawn that actually spawns. Tune movement here. |
| `/Game/Blueprints/BP_GetawayGameMode` | `AGetawayGameMode` | Changes nothing except `Default Pawn Class = BP_Character`. |

Both are created by `Content/Python/setup_tuning_blueprints.py` (*Tools > Execute Python Script...*).
It is safe to re-run: it reuses assets that already exist, so tuning is never overwritten.

## Why the game mode needs a Blueprint too

`AGetawayGameMode::StartRunFor` spawns the runner from `DefaultPawnClass`, so pointing that at a
Blueprint is all it takes - no C++ change. But *setting* it is the problem. Project Settings and
World Settings both show a **Default Pawn Class** field under the selected game mode, and both grey
it out when the game mode is a C++ class. The engine is explicit about it, in
`Engine/Source/Editor/UnrealEd/Public/GameModeInfoCustomizer.h`:

> Only allow editing GameMode BP, not native class!

So the only way to set a pawn class from the editor is for the game mode itself to be a Blueprint.
That is the whole reason `BP_GetawayGameMode` exists. The alternative - loading the Blueprint by
asset path in the C++ constructor - was turned down: it ties the portable plugin to this project's
content and cannot be changed without a rebuild.

`BP_GetawayGameMode` is also the place to edit `HandSize`, `PlacementTime`, `FinishGraceTime` and
the `Catalog`, which are all `EditDefaultsOnly` and were unreachable for the same reason.

## Where the game mode is named

Two places, and **both** have to point at `BP_GetawayGameMode` or the C++ pawn comes back:

1. **Project Settings > Project > Maps & Modes > Default Modes > Default GameMode.** Writes
   `GlobalDefaultGameMode` in `Config/DefaultEngine.ini`, as the `BP_GetawayGameMode_C` class path.
   This is what a map with no override uses.
2. **TestCourse > Window > World Settings > Game Mode > GameMode Override.** Stored in the `.umap`.
   This wins over the project setting for that map, which is why setting only one of them looks
   like nothing happened.

## Which game mode is actually live

The reliable check is the log, not the Outliner. Press Play, stop, then look in the newest
`Saved/Logs/Getaway.log` for these two lines:

    LogLoad: Game class is 'BP_GetawayGameMode_C'
    Getaway: PlayerController_0 is running as BP_Character_C_1

The second line is `AGetawayGameMode::StartRunFor` naming the pawn it just spawned, which answers
the question directly and is cheaper than hunting through the Outliner.

If it says `GetawayGameMode` instead, the map override is still the C++ class and the C++ pawn is
spawning. The pawn in the Outliner is named after whichever won: `BP_Character_C_0` or
`GetawayCharacter_0`.

## Tuning, and the two ways to lose your work

Open `BP_Character`, select **CharacterMovement** in the *Components* panel, and edit the
**Getaway** categories in *Details*. Compile and save.

- **Editing the pawn during Play does not persist.** The Details panel for a possessed pawn in PIE
  is editing that one throwaway instance. Stop, open the Blueprint asset, change it there.
- **An overridden value stops tracking C++.** Once `BP_Character` holds its own `RunSpeed`, later
  changes to the C++ default no longer reach it. The yellow *Reset to Default* arrow beside the
  field drops the override and puts the C++ value back. Worth checking that arrow first when a
  tuning change in code appears to do nothing.

The C++ defaults are unchanged and stay the source of truth; the tuned table is in
`Plugins/GetawayMovement/README.md`. When a value settles, it belongs in the C++ default and in
that table, not only in the Blueprint - a Blueprint-only value is invisible to anyone reading code.

## Not verified yet

- **Single player: played 2026-10-26 and fine.** `BP_GetawayGameMode_C` spawning `BP_Character_C`
  was confirmed in the log, and the run played normally.
- **The two-player test has not been re-run** since the pawn became a Blueprint. Steps are in
  `docs/multiplayer.md`. It adds no logic and the host path is unchanged, so nothing is expected to
  move, but it is the one gap left.
- **Changing a value in `BP_Character` and feeling it has not been done.** The wiring is proved; the
  round trip through the Details panel is not.
