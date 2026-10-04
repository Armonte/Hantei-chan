# Hantei-chan: undo transactions and editing tools (phase F)

Branch `feat/undo` (worktree `wt/undo`, based on `update/ex-mbac`).

| Commit | What |
|---|---|
| `5ef4f8f` | Shortcut registry and router, plus a standalone test |
| `0ef8fe7` | Document-level undo transactions, routed shortcuts, J/K/L transport, position tool |

This covers inventory items F1 (undo transactions and multi-pattern snapshots), F2 (a box drag is one undo step), F3 (a subset of the position tool), F5 (shortcut router) and E2 (J/K/L).

## 1. Undo model

### What the old model did

`UndoManager` used to copy only the visible pattern at the start of every UI frame and commit it when `markModified()` had been called. This caused four problems:

- A drag or held `+` button produced one step per frame.
- Any edit to another pattern was invisible to undo. That includes Pop-all-and-paste and any code that touches a pattern other than the viewed one.
- The clean marker was an undo-stack depth, so trimming the stack moved it.
- Ctrl+Z was only reachable when ImGui was not capturing the keyboard, which is correct. But undo then restored whatever the current pattern was.

### What it does now

The code is in `src/undo_manager.h` and `src/undo_manager.cpp`.

**Shadow document.** The manager is bound to `FrameData::m_sequences`. It holds one immutable `shared_ptr<const Sequence>` per pattern: the *shadow*, which is the last committed state. The baseline is captured lazily on the first UI frame after a load, so creating a character costs nothing. A reload calls `reset()`.

**Commit = diff.** A commit compares every live pattern with its shadow entry. Each pattern that differs becomes a `PatternDelta{index, before, after}`. `before` is the old shadow pointer and `after` is a fresh copy, which then replaces the shadow entry.

- Unchanged patterns are never copied again.
- A step holds only the patterns it changed. Those copies are shared between the shadow, the undo stack and the redo stack.
- Pattern-list size changes are also representable (`countBefore`/`countAfter`), although today only a load changes the size.

**Coverage.** A commit diffs the whole document, so callers only have to say *when* a step ends, not *what* changed. Covered edits:

- all AF/AS/AT/EF/IF widgets
- boxes
- frame insert, append and delete
- range paste
- pattern copy/paste and Pop-all-and-paste (a multi-pattern step)
- pattern name and metadata
- position-tool edits
- any code that edits a pattern other than the visible one

**Comparison.**

- **Plain-data blocks** (AS, AT, EF, IF, hitbox coordinates) are compared with `memcmp`. They are trivially copyable, so a missed change is impossible. The worst case is a spurious no-op step when padding bytes differ.
- **Layer, Frame_AF and Sequence** are compared field by field. `static_assert`s on their sizes fail the build when someone adds a member, so a new field cannot silently drop out of change detection. This applies to parallel work such as the MBAC/HA4 fields.
- **`Frame`** is asserted to contain exactly AF + AS + AT + EF + IF + boxes. That means growing `Frame_AT` does not trip the assert.
- **`Sequence::modified`** ("changed since load", used by Save-as-MOD) is bookkeeping, not content. It is ignored in the comparison and recomputed for restored patterns by comparing the shadow pointer with the load-time pointer.
- **A keyframe with no layers** compares equal to one default layer. The renderer and the AF panel push that layer just by displaying such a keyframe. Without this rule, viewing a freshly inserted blank keyframe would look like an edit and reappear after every undo.

### Grouping

| Call | When |
|---|---|
| `markModified()` | Any edit (existing pane callbacks, unchanged) |
| `endFrame(gestureActive)` | Once per UI frame, after the panes are drawn. `gestureActive` is `ImGui::IsAnyItemActive()` or any mouse button being held. While it is true, or while a transaction is open, edits accumulate into one pending step. The step commits when the gesture ends. The falling edge also runs a diff even without `markModified()`, which catches widgets that ignore their return value (the bug EX hunted with traces). |
| `beginTransaction(label)` / `commitTransaction()` | Explicit, nestable grouping for viewport gestures: right-drag box drawing and position-tool drags |
| `cancelTransaction()` | Restores the live document to the last commit, with no entry. Used when a position drag is cancelled with Escape or by losing focus. |
| `flush()` / `markClean()` | Commit now. Save calls `markClean()`, which flushes first. |

Consequences:

- One drag is one step.
- A held repeat button is one step.
- A whole InputText session, from focus to deactivation, is one step.
- A click on a button or checkbox is one step.

### Redo and dirty state

- Every committed entry gets a unique revision id. `markClean()` records the current revision (`isClean()` means current revision == saved revision, with nothing pending).
- Undoing back to the saved revision clears the tab `*`. Redoing to it clears it too.
- A new edit after an undo discards redo but keeps the saved revision reachable along the undo path.
- If trimming (200 levels) drops the saved state, it can never be current again, so the document stays dirty. The old depth counter got this wrong.
- `DrawUi` syncs `CharacterInstance::m_isModified` from `isClean()` every idle frame.

`undo()` and `redo()` first settle pending work:

- **A notified pending edit** (Ctrl+Z pressed before the release frame) is committed as its own step and is what gets undone. This is EX's `commitPending` behaviour.
- **Unnotified drift** that no gesture boundary has picked up is absorbed into the shadow without an entry, so it can never block the real history.

### Navigation after undo

- Each entry stores the pattern and frame in view when the step started. If the edit touched only other patterns (for example Pop-all-and-paste), the entry stores the first touched pattern instead.
- If the active view is not showing an affected pattern, undo/redo navigates there.
- Every view of the character is then refreshed: the frame is clamped, the tick is re-derived, the sprite is reloaded, and the spawn tree and active spawns are rebuilt. Pattern names are regenerated.

### Memory and speed

Measured with the test's synthetic character: 1000 slots, 700 used, 12 keyframes each, MinGW -O2.

- Baseline capture: about 5–6 ms, once per load.
- Full-document diff plus commit: about 0.6–1.3 ms per step. It runs only when a step ends, never per frame during a drag.
- 100 single-pattern steps hold about 780 KiB of history, roughly one pattern copy per step, against a per-step document copy in a naive design.
- Total overhead is one extra copy of the document plus the changed patterns.

The Edit menu shows the undo/redo counts and the approximate history size.

### Text fields keep native Ctrl+Z

- `WM_KEYDOWN` now always goes to `HandleKeys(vkey, repeat, WantCaptureKeyboard, WantTextInput)`.
- If ImGui wants the keyboard for a non-text reason (a slider being dragged, a modal), nothing fires.
- If a text or scalar field is focused, only bindings marked `duringTextInput` fire. At the moment those are Ctrl+S and Ctrl+Shift+S, and the save flushes the pending text step first.
- Ctrl+Z/Ctrl+Y therefore stay with the field, whose own stb_textedit history works. After the field deactivates, the whole edit is one document step.

## 2. Shortcut router

The code is in `src/shortcut_router.h` and `src/shortcut_router.cpp`. It is pure standard C++ and is tested standalone.

### Registry

`ShortcutRegistry` is the single table. Each binding has:

- an action
- a display name
- a chord (Win32 VK plus Ctrl/Shift/Alt)
- a reach: `focusedContext`, `editorViews` (any document tab) or `application`
- an owning context
- `duringTextInput`
- `repeat`

The registry also provides `conflicts()` (overlap-aware), `setChord()` (the basis for remappable bindings, inventory #9) and `ChordLabel()` (used by the Edit menu and its "Keyboard shortcuts" list).

### Router

- `ShortcutRouter::beginFrame()` / `claimFocus(context, viewId)` / `endFrame()`: `DrawUi` claims `characterView`, `patEditor` or `stageView` for the active tab. A later claim in the same frame wins. Keys arrive between frames, so they are routed to the owner recorded by the last complete frame.
- `setContextHandler(context, fn)`: a workspace with its own history gets first refusal on actions while it has focus.

**Integration hook for the `_c` workspace agent:** call `shortcuts.claimFocus(ShortcutContext::commands)` while its window is focused, and register a handler for `undo`/`redo`. While `commands` owns focus, Ctrl+Z/Y never fall through to the HA6 character history, even if no handler is registered.

### Default bindings

| Chord | Action | Reach |
|---|---|---|
| Ctrl+Z / Ctrl+Y | Undo / redo | app (not in text fields) |
| Ctrl+S / Ctrl+Shift+S | Save (project, else character) / Save project as | app, also in text fields |
| Ctrl+O / Ctrl+N | Open / new project (deferred, as before) | app |
| Ctrl+Tab / Ctrl+W | Next tab / close tab | app |
| Up/Down, Left/Right | Pattern / keyframe | editor tabs |
| Z / X | Previous / next box | editor tabs |
| J / K / L | Play reverse / stop-or-play / play forward | character view |
| Shift+J / Shift+L | Step one tick back / forward (repeat OK) | character view |
| Esc | Cancel position drag | character view |

## 3. J/K/L transport

The code is in `ui/editor_tools_impl.h`.

- **L**: forward playback through the existing `FrameState::animating` loop.
- **J**: reverse playback. `MainFrame` steps one tick per UI frame for one view, using `CalculateFrameFromTick` and re-simulating spawns with `SimulateSpawnsToTick`, the same path as the timeline scrub. It stops at tick 0, on a tab switch, or when forward playback starts.
- **K**: stops either direction, or resumes forward when stopped.
- **Shift+J/L**: step one tick, with auto-repeat.

K-held + J/L stepping was dropped: K itself toggles playback, so holding it would start playback for a frame first. Left/Right still step keyframes.

These are added in `MainFrame`, with no new `FrameState` fields, so `framestate.*` is untouched.

## 4. Position tool

Turn it on with the "Position tool" checkbox in Box Controls. It is a cleaned-up subset of EX's tool.

### Handles

Handles are drawn on the ImGui background draw list for the current keyframe:

- **AF layers** (cyan circles): offset X/Y. Each handle sits at the layer origin as the renderer places it: scale, then XYZ rotation in AFRT order (PAT layers use Z,Y,X order), multiplied by the zoom.
- **Effects with known X/Y slots** (squares):
  - EF 1, 101, 8 and 3: params 0 and 1
  - EF 11 and 111: params 1 and 2

  The slots match `ParseSpawnedPatterns` and the preset marker's actor-space mapping.

### Dragging

- A left-press on a handle starts a drag. The viewport does not pan while it runs.
- The screen delta is mapped back through the inverse of the handle's 2×2 transform. A near-singular transform is shown as locked, not moved.
- Alt gives 0.15 precision. Shift locks to the dominant axis.
- The value is always recomputed from the drag start, so there is no drift.
- Release commits one labelled undo step ("Move Layer 0 (sprite 12)").
- Escape, `WM_CANCELMODE` or window deactivation cancels and restores through `cancelTransaction()`.
- Changing pattern or frame mid-drag, or the target disappearing, also cancels.

### Not ported, deliberately

EX's version depends on its renderer and simulator refactors (`RenderViewSurface`, per-layer target ids and tints, spawn provenance), which other agents own here. These parts were left out:

- sprite picking with a tint
- the Ctrl-click overlap chooser and wheel cycling
- Pick/Solo buttons
- the "Trace" matrix inspector
- rotation dragging
- persistent spawned-EF handles on later keyframes
- EF2 subtype 50
- JSONL traces (rejected in the inventory)

Handles only exist for records on the current keyframe. The main candidates to add later are rotation drag (Ctrl+right-drag) and wheel cycling.

## 5. Box drawing (F2)

- `RightClick` opens a "Draw box" transaction before `BoxStart`.
- `WM_RBUTTONUP` now calls `HandleMouseUp(true, false)`, which commits it. `WM_CANCELMODE` and deactivation commit it too.
- The whole rubber-band draw is one step. It is also covered by the held-mouse gesture rule.

## 6. How this differs from Gonptechan EX

| Topic | EX | Hantei-chan |
|---|---|---|
| Snapshot scope | One pattern per commit, and "the source pattern's transaction" for cross-pattern gizmo edits | Whole document diffed at commit. Multi-pattern steps. Only changed patterns are stored, shared by pointer. |
| Gesture coalescing | Keeps the pre-gesture snapshot while the left button is held. Guards against a second view replacing the snapshot. | Any active ImGui item, any held button, or an explicit transaction. There is no per-view snapshot, so the multi-view race cannot happen. |
| Unnotified widgets | Fixed by editing 91 widgets to call `markModified()` from their return value | Gesture-end diff records them anyway. The widget edits are not needed. |
| Ctrl+Z in text fields | Routed before ImGui, so field-local undo is lost | Routed after ImGui. Fields keep native undo, and the step commits when the field deactivates. |
| Clean state | Stack depth, which breaks on trim | Revision ids. Trim-aware. `Sequence::modified` recomputed. |
| Undo target | Restores `snapshot->patternIndex` from whatever view is active | Restores all affected patterns, navigates to them and refreshes every view of the character |
| Shortcuts | `ShortcutRegistry`/`ShortcutRouter` with 4 contexts; Ctrl+Z/Y application-level before ImGui | Same idea. Adds `editorViews` reach, text-input and repeat flags per binding, context handlers, and focus latched per complete frame. Ctrl+Z/Y are application-level, but after ImGui. |
| Position tool | Full: sprite picking, chooser, Pick/Solo, rotation, spawn provenance, traces | Subset (above). No renderer or simulator dependencies. No traces. |
| J/K/L | J/L previous/next tick with held 60 Hz playback, K toggle | J reverse play, K stop/play, L forward play, Shift+J/L tick step |

## 7. Tests

The targets are `undo_manager_test` and `shortcut_router_test` in `CMakeLists.txt`, registered with `add_test`. They are built by `./build.sh` and run from WSL:

```
./build/undo_manager_test.exe     # UNDO_MANAGER_TEST_PASS
./build/shortcut_router_test.exe  # SHORTCUT_ROUTER_TEST_PASS
```

`undo_manager_test` has 12 cases:

- drag coalescing
- an unnotified edit caught at the gesture edge
- Ctrl+Z before the release frame
- a cross-pattern, multi-pattern paste as one step, with focus navigation
- frame insert/delete and boxes
- transactions: explicit, cancel, nested
- redo branching and the dirty state across save/undo/redo, plus `Sequence::modified` recomputation
- a trimmed saved state staying dirty
- a no-op edit not being recorded
- display-time layer normalisation not being an edit
- unnotified drift not blocking undo
- a timing and memory-sharing budget

`shortcut_router_test` checks:

- focus latching, replacement and clearing
- that the defaults have no conflicts
- J/K/L and navigation scoping
- application reach
- text input keeping Ctrl+Z but allowing Ctrl+S
- auto-repeat rules
- conflict detection after a rebind
- chord labels
- context handlers

### Manual checks still needed

This work was build- and unit-tested only; no UI session was run.

1. Drag a DragInt: one Ctrl+Z restores it.
2. Type a pattern name, press Ctrl+Z inside the field (field-local undo), click away, then Ctrl+Z (whole rename undone).
3. Pop-all-and-paste into three patterns: one Ctrl+Z restores all three and jumps to the first.
4. Save, edit, Ctrl+Z: the `*` clears.
5. Right-drag a box: one step.
6. Position tool:
   - drag a rotated or scaled layer and check the origin follows the cursor
   - Esc restores the value
   - Alt and Shift behave as described
7. J/K/L and Shift+J/L on a pattern with spawns.

## 8. Safety fixes kept intact

None of these files were changed:

- **Atomic save:** `framedata_save.cpp` and `FrameData::save` are untouched. `CharacterInstance::save*` still marks clean only after a successful write, now through `markClean()`.
- **Deferred project actions:** the Ctrl+O and Ctrl+N shortcuts call `openProject()`/`newProject()`, which only `requestProjectAction(...)`. `processDeferredProjectAction()` is untouched.
- **Keyframe insert/delete:** `main_pane.cpp` is untouched. The deferred `KeyframeOp` is still applied after the last `frame` reference is used, and the frame is re-validated after the mutation.

## 9. Limits and follow-ups

- **Not tracked:**
  - `FrameData::m_commands` (the `_c` editor has its own history)
  - PAT parts edits
  - `effect.ha6` data, which is not editable from character panes
- **Pattern-list size changes** are represented in entries, but `FrameData::m_nsequences` is not updated by undo. Only load changes the size today.
- **Other characters in the project** only commit on explicit transactions or the next undo/save. Edits only come from the active view, so this is theoretical.
- **Remappable bindings:** `setChord` and `conflicts` exist, but there is no settings UI or persistence yet.
