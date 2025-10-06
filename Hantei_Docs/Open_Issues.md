# Open Issues

**Last Updated:** 2025-10-06
**Total Open Issues:** 17 (9 bugs, 8 enhancements)

---

## Bugs

### Effect Preview Issues

#### #28 - [Effect Preview: Effects that have loops do not loop at all](https://github.com/Armonte/Hantei-chan/issues/28)
**Example:** F Kouma pattern 156 (AAD grab)
The flames spawned (patterns 167 and 168) are supposed to loop multiple times, but in the preview they only play through once. Effect animations with loop settings are not respecting the loop count in the spawned patterns visualization.

#### #27 - [Effect Preview: Child effects created by other effects do not inherit the correct offset](https://github.com/Armonte/Hantei-chan/issues/27)
**Example:** F Hime pattern 160
Pattern 160 spawns pattern 163 (geyser), which then spawns pattern 166 (ground breaking) with offset 0,0 relative to itself. However, the ground breaking appears on top of Hime in the preview instead of on top of the geyser. Child effects are not correctly inheriting parent positions.

#### #26 - [Effect Preview: Sprites that are supposed to be completely transparent still show up](https://github.com/Armonte/Hantei-chan/issues/26)
**Example:** F Hime pattern 192
Spawns pattern 165, which should have no visible sprite (alpha set to 0). However, the sprite still renders in the preview. Additionally, something about F Hime's wave patterns (400+) that pattern 165 spawns causes wave sprites to not get drawn when being previewed.

### UI/Editor Issues

#### #34 - [PSTS, Level, and Flag take up a lot of space](https://github.com/Armonte/Hantei-chan/issues/34) *(bug + enhancement)*
The PSTS, Level, and Flag fields currently each take up their own line in the UI, consuming significant screen space. Moving these fields to be displayed in-line would help save screen space without presenting issues, as the values of these fields don't exceed two-digit numbers. This is both a UI bug (inefficient space usage) and an enhancement request (better layout).

#### #32 - [Frames are indexed from 1 to the right of the scroll bar](https://github.com/Armonte/Hantei-chan/issues/32)
Everything in the game uses 0-indexing for frames, but the frame numbers displayed to the right of the scroll bar are indexed from 1. This creates confusion when working with frame data, as there's a mismatch between the displayed frame numbers and the actual internal indexing used by the game.

#### #30 - [Add back Keyboard Shortcuts to be Save Character, is missing at the moment](https://github.com/Armonte/Hantei-chan/issues/30)
The keyboard shortcut for saving characters (Ctrl+S) is currently missing from the shortcuts menu. Additionally, it would be welcome to restore Ctrl + arrow keys for position adjustment of X and Y transforms. The shortcut system should eventually be customizable, so shortcuts should be implemented in a way that makes future customization easier (not hardcoded).

### Rendering/Display Issues

#### #31 - [Rotations are applied incorrectly to the sprite viewer](https://github.com/Armonte/Hantei-chan/issues/31)
**Example:** effect.ha6 pattern 15 (landing effect visual)

The rotation order for sprites appears incorrect. When both Z rotation and X rotation are applied, the sprite displays incorrectly. The issue seems to be that Z rotation is applied after X rotation in Hantei-chan, but in-game it appears to be applied in the opposite order.

**Observed behavior:**
- Z rotation only: Displays correctly
- X rotation only: Displays correctly
- Both rotations: Displays incorrectly (rings should be horizontal with missing part on left, but appear vertical)

This suggests the rotation transformation matrix order needs to be reversed to match in-game rendering.

### Effect System

#### #12 - [Effect 3 (Spawn Preset Effect) Should be included in the Spawned Patterns Visualization view](https://github.com/Armonte/Hantei-chan/issues/12)
Effect Type 3 (Spawn Preset Effect) is currently not displayed in the Spawned Patterns Visualization. These preset effects should be shown in the hierarchy tree for a complete view of all spawned entities.

#### #11 - [Effect 101 "Spawn Relative Pattern" Still Selects Absolute Patterns](https://github.com/Armonte/Hantei-chan/issues/11)
Effect 101's pattern selection behaves incorrectly. It acts as if selecting absolutely from the pattern index instead of relatively.

**Expected behavior:** "Effect 101 number 5" from pattern 100 should spawn pattern 105 (+5 relative offset)
**Actual behavior:** Spawns pattern 5 (absolute index)

This also breaks the spawned patterns preview since it previews the wrong pattern.

### Stability

#### #8 - [gonptechan can crash if you paste patterns](https://github.com/Armonte/Hantei-chan/issues/8)
The application can crash when pasting patterns. The exact cause is unclear, but this issue also existed in the original hantei. Memory management or clipboard handling may be involved.

---

## Enhancements / Feature Requests

### UI/UX Improvements

#### #15 - [Add the ability to filter/search patterns by name](https://github.com/Armonte/Hantei-chan/issues/15)
Add a live filter field at the top of the pattern dropdown that searches by pattern name. The filter should update automatically as you type each letter, making it easier to find specific patterns in large character files.

#### #33 - [Allow pattern numbers to be entered manually](https://github.com/Armonte/Hantei-chan/issues/33)
Similar to how you can either select a Hit effect from a dropdown or enter it manually into the ID field, it would be very convenient to have the same functionality for pattern selection. Currently, you can only select patterns from a dropdown menu. Adding a manual input field alongside the dropdown (like the Hit Effect UI) would make it faster to select specific patterns when you know the pattern number.

**Proposed change:** Add an ID input field next to the pattern dropdown, matching the pattern used for Hit Effects.

#### #10 - [Extra functions and behaviors to the Timeline](https://github.com/Hantei-chan/issues/10)
Major enhancement proposal to expand the "Spawn Timeline" into a full-featured "Timeline" tool. This is a long-term project with 12 proposed features:

**Visibility & Navigation:**
1. Make Timeline visible even for patterns without Effects
2. Left-click drag through timeline frames
3. Zoom and pan support (Scroll + Middle Click)

**Effect Manipulation:**
4. Right-click drag to reposition Effect spawns
5. Alt + Right Click to duplicate effects

**Frame Editing:**
6. Display individual pattern frames when zoomed (with frame duration separators)
7. Drag and reposition pattern frames in the timeline
8. Copy/paste selected frames with option to paste before/after playhead

**Visual Indicators:**
9. Color-coded display showing which frames have: Collision Box, Hurt Box, Special Box, Clash Box, Projectile Box, Attack Box
10. Indicator for Landing Frames
11. Indicator for frames with Effects
12. Indicator for frames with Conditions

Includes concept mockup showing the proposed timeline interface.

#### #9 - [Add remappable keyboard bindings](https://github.com/Armonte/Hantei-chan/issues/9)
Add customization options for keyboard and mouse bindings to accommodate different user preferences. This includes:
- Keyboard shortcuts customization
- Swappable mouse button functions (left/right/middle click)
- Reversible zoom behavior
- Other mouse function preferences

#### #4 - [Make the tabs display some of the beginning of the open Pattern's name](https://github.com/Armonte/Hantei-chan/issues/4)
**Minor Enhancement**
Currently tabs only show the character file name. Expand the horizontal space tabs occupy to show ~12 characters of the pattern name for better tab identification when multiple patterns are open.

#### #3 - [Detach tabs to new windows for extra views](https://github.com/Armonte/Hantei-chan/issues/3)
Enable detaching tabs into separate windows for multi-monitor workflows. Currently you can have multiple tabs for different views and reorganize their position, but cannot detach them. This feature should also include the ability to re-attach windows.

#### #2 - [Align the text from the Box Controls Pane to align with the boxes](https://github.com/Armonte/Hantei-chan/issues/2)
**Minor Enhancement**
The text labels in the Box Controls Pane are currently misaligned with the boxes. The title of each box category should align with the first box of that category. May require moving boxes to the right to make space for proper text alignment.

### Game Support

#### #14 - [UNI2/MBTL SUPPORT](https://github.com/Armonte/Hantei-chan/issues/14) *(help wanted)*
Need to pull in features from various community forks to achieve feature parity and support UNI2/MBTL formats. Goal is to unify the various forks into a single build that benefits the entire community.

UNI modders are already making great progress with their own mods, but a unified tool would improve the ecosystem. May involve adding sosfiro's beta as a submodule or similar integration approach.

---

## Issue Categories Summary

**By Priority:**
- Critical bugs affecting core functionality: 3 (Effect Preview issues)
- Effect system bugs: 2
- UI/Editor bugs: 3 (#34, #32, #30)
- Rendering/Display bugs: 1 (#31)
- Stability issues: 1
- Major enhancements: 2 (#10 Timeline, #14 Game Support)
- Minor enhancements: 6

**By Area:**
- Effect System: 5 issues (3 preview + 2 system)
- UI/UX: 9 issues (6 enhancements + 3 bugs)
- Rendering: 1 issue
- Timeline: 1 issue (major)
- Game Support: 1 issue
- Stability: 1 issue

**Recently Closed:**
- #29 - Copy button for Condition 35 (Fixed 2025-10-06)
