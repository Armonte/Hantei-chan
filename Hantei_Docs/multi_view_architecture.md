# Multi-View Architecture Implementation

## Overview

This document tracks the implementation of multi-view support in Hantei-chan, allowing users to open multiple independent views of the same character data for synchronized editing.

## Architecture Design

### Core Concepts

**Before (Old System):**
```
CharacterInstance = FrameData + FrameState + CG
Each tab = 1 CharacterInstance (separate data, no sharing)
```

**After (New System):**
```
CharacterInstance = FrameData + CG (shared data)
CharacterView = pointer to CharacterInstance + independent FrameState
Each tab = 1 CharacterView (can share CharacterInstance)
```

### Benefits

1. **Synchronized Editing**: Edit pattern in View 1 → instantly visible in View 2
2. **Independent Navigation**: Each view can look at different patterns/frames
3. **Memory Efficiency**: Character data loaded once, shared across views
4. **Duplicate Prevention**: Can't accidentally load same file twice

## Implementation Status

### ✅ Phase 1: Core Classes (COMPLETED)

**Files Created:**
- `src/character_view.h` - CharacterView class definition
- `src/character_view.cpp` - CharacterView implementation
- Updated `CMakeLists.txt` - Added character_view.cpp to build

**Key Features:**
```cpp
class CharacterView {
    CharacterInstance* m_character;  // Pointer to shared data (not owned)
    FrameState m_state;              // Independent view state
    int m_viewNumber;                // 0 = primary, 1+ = additional views
    std::unique_ptr<MainPane> m_mainPane;
    std::unique_ptr<RightPane> m_rightPane;
    std::unique_ptr<BoxPane> m_boxPane;
};
```

### ✅ Phase 2: MainFrame Refactor (COMPLETED)

**File: src/main_frame.h**

Changed members:
```cpp
// OLD:
std::vector<std::unique_ptr<CharacterInstance>> characters;
int activeCharacterIndex;

// NEW:
std::vector<std::unique_ptr<CharacterInstance>> characters;  // Shared data
std::vector<std::unique_ptr<CharacterView>> views;           // Independent views
int activeViewIndex;
int contextMenuViewIndex;  // For right-click menu
int pendingCloseViewIndex; // For unsaved dialog
```

New helper methods:
```cpp
CharacterView* getActiveView();
CharacterInstance* getActiveCharacter();
void setActiveView(int index);
void closeView(int index);
bool tryCloseView(int index);
CharacterInstance* findCharacterByPath(const std::string& path);
void createViewForCharacter(CharacterInstance* character);
int countViewsForCharacter(CharacterInstance* character);
```

### ✅ Phase 3: UI Updates (COMPLETED)

**Tab Bar Rendering** (lines 100-150):
- Iterate over `views` instead of `characters`
- Show view display name (includes "(2)" for second view)
- Right-click context menu: "New View of Character"
- Duplicate file error popup

**Pane Access** (lines 419-425):
```cpp
auto* view = getActiveView();
if (view) {
    if (view->getMainPane()) view->getMainPane()->Draw();
    if (view->getRightPane()) view->getRightPane()->Draw();
    if (view->getBoxPane()) view->getBoxPane()->Draw();
}
```

**Dialogs** (lines 270-307):
- Updated "Unsaved Changes" dialog to use `pendingCloseViewIndex`
- Only prompts if last view and character is modified

### ✅ Phase 4: Character Loading (COMPLETED)

**Duplicate Prevention:**
```cpp
if (findCharacterByPath(path)) {
    ImGui::OpenPopup("DuplicateFileError");
} else {
    auto character = std::make_unique<CharacterInstance>();
    if (character->loadFromTxt(path)) {
        characters.push_back(std::move(character));
        createViewForCharacter(characters.back().get());
    }
}
```

Applied to:
- "+" button popup (3 locations)
- File menu (3 locations)

### ✅ Phase 5: Input Handlers (COMPLETED)

**Keyboard Shortcuts:**
- Ctrl+Tab: Switch to next view
- Ctrl+W: Close current view
- Arrow keys, Z/X: Use view's state

**Mouse Handlers:**
- HandleMouseDrag: Access boxPane via view
- RightClick: Access boxPane via view

### ✅ Phase 6: Rendering (COMPLETED)

**RenderUpdate()** - Uses view's state:
```cpp
auto* view = getActiveView();
auto& state = view->getState();
// Use state.pattern, state.frame, state.spriteId
```

**DrawBack()** - Uses character's render position

**SwitchImage() Bug Fix:**
```cpp
// Always process when id == -1 to ensure texture clears
if(cg && (id != curImageId || id == -1) && cg->m_loaded)
```

### ✅ Phase 7: Project Management - Partial (COMPLETED)

**Clear Operations:**
- newProject(): Clears both `views` and `characters`
- closeProject(): Clears both `views` and `characters`
- All project close dialogs updated

## ✅ All Implementation Complete!

### ✅ Phase 8: Project Save/Load (COMPLETED)

**Files Updated:**
- `src/project_manager.h` ✓
- `src/project_manager.cpp` ✓
- `src/main_frame.cpp` ✓

**New Signatures Implemented:**
```cpp
// New view-based functions
SaveProject(path, characters, views, activeViewIndex, ...)
LoadProject(path, characters, views, activeViewIndex, render, ...)

// Legacy functions still available for backward compatibility
SaveProject(path, characters, activeCharacterIndex, ...)
LoadProject(path, characters, activeCharacterIndex, ...)
```

**JSON Format (Version 2.0):**
```json
{
  "characters": [
    {
      "txtPath": "...",
      "ha6Paths": [...],
      "cgPath": "...",
      "renderX": 0,
      "renderY": 150,
      "zoom": 3.0,
      "palette": 0
    }
  ],
  "views": [
    {
      "characterIndex": 0,
      "viewNumber": 0,
      "pattern": 5,
      "frame": 2,
      "selectedLayer": 0,
      "spriteId": -1
    },
    {
      "characterIndex": 0,  // Second view of same character
      "viewNumber": 1,
      "pattern": 10,
      "frame": 0,
      "selectedLayer": 0,
      "spriteId": -1
    }
  ],
  "activeViewIndex": 1,
  "ui": {
    "theme": 0,
    "zoom": 3.0,
    "smooth": false,
    "clearColor": [0.324, 0.409, 0.185]
  }
}
```

**Implementation Details:**

1. **Load Process** - Fully implemented in project_manager.cpp:
   - Loads all characters first
   - Creates views pointing to correct characters with proper view numbers
   - Restores each view's navigation state (pattern, frame, layer)
   - Handles legacy v1.0 projects (creates one view per character)
   - Sets active view index

2. **Save Process** - Fully implemented:
   - Saves all characters with render state
   - Saves all views with character index and navigation state
   - Uses version 2.0 format for view support
   - Backward compatible with legacy format

3. **MainFrame Updates** - All completed:
   - openProject() uses new view-based load (line 1270)
   - saveProject() uses new view-based save (line 1301)
   - saveProjectAs() uses new view-based save (line 1324)
   - openRecentProject() uses new view-based load (line 1450)

## 🧪 Testing Guide

### Prerequisites
1. Fix the `utf82sj` compilation error in framedata_save.cpp:496
2. Rebuild the project: `cmake --build build`
3. Run gonptechan.exe

### Testing Checklist

#### 1. Basic Functionality (5 minutes)
- [ ] **Load character**: File → Load from .txt → verify tab appears
- [ ] **Switch tabs**: Click different tabs → verify data changes in panes
- [ ] **Close tab**: Click X on tab → verify tab closes
- [ ] **Keyboard shortcuts**:
  - Ctrl+Tab: cycles through tabs
  - Ctrl+W: closes current tab
  - Arrow keys/Z/X: navigate frames

#### 2. Multi-View Features (10 minutes) ⭐ CORE FEATURE
- [ ] **Create second view**:
  1. Load a character
  2. Right-click the character's tab
  3. Select "New View of Character"
  4. Verify: Second tab appears with name ending in "(2)"

- [ ] **Synchronized editing**:
  1. With 2 views open, go to View 1
  2. Edit pattern data (add/remove hitbox, change frame timing, etc.)
  3. Switch to View 2
  4. Verify: Changes appear immediately in View 2

- [ ] **Independent navigation**:
  1. View 1: Navigate to Pattern 5, Frame 3
  2. View 2: Navigate to Pattern 10, Frame 0
  3. Switch between tabs
  4. Verify: Each view stays at its own position

- [ ] **Close one view**:
  1. Close View 2 (the "(2)" tab)
  2. Verify: View 1 still works, character still loaded
  3. Verify: Can still edit in View 1

- [ ] **Close last view**:
  1. Close View 1 (the last remaining view)
  2. Verify: Character data unloaded, sprite clears

#### 3. Duplicate Prevention (5 minutes)
- [ ] **Duplicate .txt file**:
  1. Load a character from .txt
  2. Try to load the same .txt again (File → Load from .txt)
  3. Verify: Error dialog appears
  4. Verify: Dialog message mentions using "New View"

- [ ] **Duplicate .ha6 file**:
  1. Load a character from .ha6
  2. Try to load the same .ha6 again
  3. Verify: Error dialog appears

#### 4. Unsaved Changes (5 minutes)
- [ ] **Close with unsaved (last view)**:
  1. Load character, make an edit
  2. Click X on the tab
  3. Verify: "Unsaved changes" dialog appears
  4. Test all three options:
     - Save: saves and closes ✓
     - Don't Save: closes without saving ✓
     - Cancel: stays open ✓

- [ ] **Close non-last view (no prompt)**:
  1. Load character, create 2nd view
  2. Make edit in View 1
  3. Close View 2 (not the last view)
  4. Verify: NO dialog, closes immediately

- [ ] **Close last view (with prompt)**:
  1. With 2 views open and edits made
  2. Close View 1
  3. Close View 2 (last view)
  4. Verify: Dialog appears on last close only

#### 5. Sprite Rendering (5 minutes)
- [ ] **Sprite displays**: Load character with CG → verify sprite shows in viewport
- [ ] **Sprite clears**: Navigate to pattern with no frames → sprite disappears
- [ ] **Switch between views**:
  1. View 1: Pattern with sprite
  2. View 2: Pattern without sprite
  3. Switch tabs → sprite updates correctly
- [ ] **Close all**: Close all tabs → sprite clears

#### 6. Project Save/Load (10 minutes) ⭐ CRITICAL
- [ ] **Save multi-view project**:
  1. Load a character
  2. Create 2nd view (right-click → New View)
  3. View 1: Go to Pattern 5, Frame 2
  4. View 2: Go to Pattern 10, Frame 7
  5. File → Save Project As → save as "test_multiview.hproj"

- [ ] **Load and verify**:
  1. File → Close Project
  2. File → Open Project → open "test_multiview.hproj"
  3. Verify: Both tabs appear with correct names
  4. Verify: View 1 is at Pattern 5, Frame 2
  5. Verify: View 2 is at Pattern 10, Frame 7
  6. Verify: Correct tab is active

- [ ] **Legacy project compatibility**:
  1. Load an old v1.0 .hproj file (from before multi-view)
  2. Verify: Loads successfully with one view per character

- [ ] **Recent projects**:
  1. Save a multi-view project
  2. File → Recent Projects → select it
  3. Verify: Loads with all views restored

#### 7. Edge Cases (5 minutes)
- [ ] **Empty project**: Create new project → verify no crashes
- [ ] **Multiple characters**: Load 2 different characters → create views for each → verify all work
- [ ] **Max views**: Create 5+ views of same character → verify all display correctly
- [ ] **Quick switching**: Rapidly Ctrl+Tab through many tabs → no crashes

### 🐛 Known Issues to Watch For
1. Sprite sticking when switching to empty patterns (FIXED)
2. Duplicate file loading (FIXED)
3. Tab close not working (FIXED)

### 📝 Bug Report Template
If you find issues:
```
**Bug**: [Brief description]
**Steps to Reproduce**:
1.
2.
3.
**Expected**: [What should happen]
**Actual**: [What actually happens]
**View Config**: [How many views, which patterns/frames]
```

## File Changes Summary

### New Files
1. `src/character_view.h` - View class definition
2. `src/character_view.cpp` - View implementation

### Modified Files
1. `CMakeLists.txt` - Added character_view.cpp
2. `src/main_frame.h` - New view architecture members & methods
3. `src/main_frame.cpp` - Complete refactor (~50+ changes):
   - Tab bar rendering (views loop)
   - All getActive() → getActiveCharacter()
   - All pane access via view
   - Character loading with duplicate checks
   - Input handlers updated
   - Render updates use view state
   - Project clear operations
4. `src/render.cpp` - SwitchImage bug fix (line 224)

### Files Needing Updates
1. `src/project_manager.h` - Update save/load signatures
2. `src/project_manager.cpp` - Implement view serialization

## Known Issues

### Resolved
✅ Sprite not clearing when switching to empty pattern - Fixed with SwitchImage update
✅ Duplicate file loading - Added path checking
✅ Tab overlap with same file - Prevented via error dialog

### Future Enhancements
- Add view management UI (close all views of character, etc.)
- Add visual indicator showing which views share same character
- Consider adding view-specific render settings (different zoom per view)
- Add "Clone View" to duplicate current view's navigation state

## Implementation Timeline

**Session 1:**
- Created CharacterView class
- Updated MainFrame headers
- Began main_frame.cpp refactor

**Session 2 (CURRENT):**
- Completed tab bar, loading, input handlers
- Updated rendering to use view state
- Cleaned up project management (partial)
- **NEXT**: Update ProjectManager for view save/load

## Notes

- View numbers are assigned based on how many views already exist for that character
- First view = 0 (displays as "Character Name")
- Second view = 1 (displays as "Character Name (2)")
- Third view = 2 (displays as "Character Name (3)")
- When a view is closed, view numbers are NOT reassigned (keeps UI stable)

## References

- Original issue: Sprite sticking when switching to empty pattern
- User request: Support for synchronized multi-view editing
- Design inspiration: Similar to text editor split views
