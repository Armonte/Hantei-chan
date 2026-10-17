# Hantei-chan safety fixes (Phase A + cheap Phase B items)

Source: `GONPTECHAN_EX_INVENTORY.md` section 4 (items 1-4, 8, 9) and section 5 Phase A (A1-A4), B2, B3.
Tree: `/mnt/c/dev/hantei-chan/Hantei-chan`. Nothing was committed. All edits sit on top of the existing uncommitted work.
No EX code was copied. The EX `_c` save pattern (a backup name with 1-second resolution, plus `ofstream` + `MoveFileExA` without a disk flush) was deliberately not used.

## 1. Atomic HA6 save (A1, inventory 4.1)

The bug was real. `FrameData::save` / `save_modified_only`:
- truncated the target in place with `std::ofstream`
- returned `void`
- deleted degenerate boxes and swapped inverted boxes in the live `m_sequences`

`CharacterInstance::save*` cleared the dirty flag no matter what happened.

Changes:
- `src/misc.h`, `src/misc.cpp`: new `WriteFileAtomic(path, data, size)`:
  1. It writes a temp file in the same directory. The name is `<target>.tmp<pid>_<tick64>_<counter>`, opened with `CREATE_NEW`, so it cannot collide.
  2. It calls `WriteFile` in a loop that checks the byte count, then `FlushFileBuffers`.
  3. It replaces the target with `MoveFileExA(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
  4. On any failure it deletes the temp file, and the original is not touched.
- `src/framedata_save.cpp`, `src/framedata.h`: the `Write*` serializers now take `std::ostream&` instead of `std::ofstream&`. This is a signature change only. The PDS2 seek-back still works on `ostringstream`.
- `src/framedata.cpp`:
  - `FrameData::save` / `save_modified_only` now return `bool`.
  - Both go through the new `WriteHA6File`, which serializes into memory and then calls `WriteFileAtomic`.
  - Box cleanup now runs as `SequenceNeedsBoxFix` + `FixBoxesForSave`, applied to a copy of just the affected sequence. The in-memory data is never changed.
- `src/character_instance.cpp`:
  - `save`, `saveAs` and `saveModifiedOnly` return false on a write failure.
  - They clear `m_isModified` and call `markCleanState` only on success.
  - `saveAs` updates the paths only on success.
  - `saveModifiedOnly` touches the `.txt` only on success.
- UI (`src/main_frame.cpp/.h`, `src/ui/main_menu_impl.h`, `src/ui/main_ui_impl.h`):
  - New `saveCharacter` / `saveCharacterAs` wrappers, plus a new "Save Error" modal that shows the path.
  - They are used by Save Character, Save As, Save as MOD, Ctrl+S, and the per-view "Unsaved Changes" dialog.
  - If Save fails in that dialog, it now closes and leaves the view open. Before, the failure was silent.
- `src/roundtrip.cpp`: checks the `save` return value and exits with code 6 on failure.

## 2. Non-destructive, index-safe project load (A2, inventory 4.2)

These bugs were real:
- `LoadProject` cleared `characters`/`views` before parsing any characters.
- A skipped character shifted `character_index` for every later view.
- The version was compared as a string.
- The `.hproj` was written in place.
- Also found: `SaveProject` wrote `character_index` as the position in `characters`, but it skips characters that have no path. A project with an unsaved PAT-editor character therefore saved wrong indices for every view after it.

Changes:
- `src/project_manager.cpp/.h`, view-based `LoadProject`:
  - It builds characters and views in local containers and commits them (views first, then characters) only after the whole file is processed. A parse error or exception leaves the caller's state untouched.
  - Views bind through a `fileToLoaded[]` map from file index to `CharacterInstance*`. Views of characters that failed to load are dropped and never re-bound.
  - `active_view` / `active_character` follow the view they named in the file, not its position.
  - The version is read as a numeric major version (`ProjectMajorVersion`).
  - The new optional `outFailedCharacters` parameter returns the paths that failed to load.
- `SaveProject` (both overloads):
  - It writes through `WriteFileAtomic`.
  - The view-based overload maps views to their index in the *written* characters array.
- `src/main_frame.cpp`, new `loadProjectFromPath`:
  - It loads into fresh containers.
  - It resets the render's CG/parts pointers before the old project is destroyed, then swaps the new project in.
  - If some characters were missing, it shows "Project Load Error" with the list and leaves the project marked modified, because saving would drop those characters from the file.
  - A failed load of a recent project still removes that project from the recent list.

## 3. Deferred project actions (A3, inventory 4.3)

These bugs were real:
- If the unsaved-changes prompt appeared, OpenRecent lost its path, because the dialog called `openProject()` and opened a file dialog.
- `openRecentProject` could erase from `gSettings.recentProjects` while the menu was iterating over it with a range-for.
- The `OpenPopup("Project Load/Save Error")` calls were made from inside menus or popups. Their IDs did not match the top-level `BeginPopupModal`, so the error popups never appeared.
- The "Unsaved Project" dialog's Save button saved only the `.hproj`, then discarded modified characters.

Changes (`src/main_frame.cpp/.h`, `src/ui/main_ui_impl.h`):
- `newProject` / `openProject` / `closeProject` / `openRecentProject` now only queue work through `requestProjectAction(action, path, confirmed)`.
- `processDeferredProjectAction()` runs at the top level of `DrawUi`, right after the dock window ends. It calls `runProjectAction`, which shows the unsaved-changes prompt if needed, clears the project, or loads it.
- The pending Open path is kept in `m_pendingProjectPath` while the prompt is open. Save and Don't Save re-queue the action as confirmed, and Cancel clears it.
- In the unsaved-project prompt, Save first saves all modified characters through `saveAllModifiedCharacters`, then saves the project if it is modified. It runs the action only if everything succeeded. Otherwise it cancels the action and shows the error.
- Error popups are queued with `requestErrorPopup(name, detail)` and opened at the top level. The detail text is shown in the Project Load/Save Error popups.

## 4. Keyframe tools (A4, inventory 4.4)

These bugs were real:
- `seq->frames.insert(pos, {})` resolved to the `initializer_list` overload, so a blank insert did nothing.
- After Append, Insert or Delete, the pane kept using the `Frame &frame` reference, which had been invalidated by reallocation or erase. It was still used by later buttons in the same draw.
- Deleting the only frame left `currState.frame == -1`.

Changes in `src/main_pane.cpp` (`MainPane::Draw`):
- The Append, Insert and Delete buttons now only set a local `keyframeOp`.
- The op runs at the end of the `nframes >= 0` block, after the last use of `frame`. It copies the source frame into a local first, then does `insert(pos, std::move(newFrame))`.
- Afterwards it re-clamps `currState.frame` to `[0, size-1]` (0 when the list is empty), recomputes `currentTick`, clears `activeSpawns`, and marks the pattern modified.
- A new blank frame is still `Frame{}`, the same as Append already used. The "format-aware blank frame" part of A4 was **not** done.

## 5. Cheap preview fixes (B2, B3; inventory 4.8, 4.9)

- The PAT additive colour really was divided by 255 twice. `PrLoad` stores PRSP as 0..1, which the PatEditor `ColorEdit4` and the ×255 on save rely on, and then `parts.cpp` divided by 255 again.
  - Fixed in `src/parts/parts.cpp`: the `setAddColor(...)` call no longer divides.
- The z-sort was unstable: `Render::SortLayersByZPriority` used `std::sort`, so equal-priority layers could swap from frame to frame.
  - Fixed in `src/render.cpp`: it now uses `std::stable_sort`, so ties keep insertion order. No new tie rule was added.

## Verification

- The MinGW build passes (`flock ... ./build.sh`, exit 0, no new warnings in the touched files).
- **Byte-identical save output:**
  - Before editing anything, I built the baseline `roundtrip.exe` and kept a copy.
  - I copied all 269 `*.HA6` files from `/mnt/c/games/mbaacc/data` to scratch, saved each one with both the baseline and the new `roundtrip.exe`, and compared the outputs with `cmp`.
  - Result: **269/269 identical.**
- **No mutation:**
  - With the old build, `roundtrip` reported 0 diffs on every file. That was because the old save had already "fixed" the in-memory boxes before the comparison.
  - With the new build, 31 files (for example aoko, kishima, warc) report `hitbox N xy differs` / box-count diffs between the loaded data and the reloaded save, while the file bytes are still identical.
  - This shows the loaded data now keeps its original inverted/degenerate boxes and the cleanup happens only in the written output.
  - Note: `roundtrip.exe` now exits with code 5 on those files. This is expected, not a regression.
- **Failure path:**
  - Saving to a path in a directory that does not exist returns false ("save failed").
  - Saving over a read-only file on NTFS fails, the original's md5 is unchanged, and no `.tmp*` file is left behind.
  - Overwriting an existing writable file works.
- **Not runtime-tested:** the GUI paths (project load/remap, deferred actions, error popups, keyframe buttons). These were checked by code review and compilation only, and need a manual pass in the app.

## Skipped / follow-ups

- `.pat` save (`Parts::Save`) is still non-atomic. It is outside this task, but `WriteFileAtomic` can be reused for it.
- "Close Character" still closes the view from inside the menu. It is not a project action, and I did not see a crash path.
- A5 (`ReadInMem`), A6 (strict `utf82sj`) and A7 (`OFN_NOCHANGEDIR`) were not in scope.
- Logging: no log lines were added. Failures go to the UI popups instead. The project has no quill; the existing code uses `printf`.
