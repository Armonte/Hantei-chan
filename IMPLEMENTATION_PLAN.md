# Hantei-chan Feature Parity Implementation Plan

## Goal
Achieve full feature parity with Eiton_hantei fork, including:
- Multi-layer rendering system
- Cross-instance copy/paste via shared memory
- Range paste operations
- Complete hitbox copy/paste UI
- Foundation for future multi-character tab support

---

## Current Status

### ✅ Completed Features
- [x] Modified pattern tracking with asterisk indicator
- [x] Pattern copy/paste with push/pop stack
- [x] Frame copy/paste
- [x] Component copy/paste (AS/AF/AT)
- [x] EF/IF group copy/paste
- [x] Per-item EF/IF copy buttons
- [x] CopyData structure in FrameState
- [x] CopyVectorContents helper function

### ⏳ Missing Features
- [ ] Multi-layer rendering system
- [ ] Cross-instance copy/paste (shared memory)
- [ ] Range paste window with batch operations
- [ ] Hitbox copy/paste UI
- [ ] Layer-aware save/load functions

---

## Architecture Overview

### Layer System
**Current**: Frame_AF contains single sprite properties directly
```cpp
struct Frame_AF {
    int spriteId;
    int offset_x, offset_y;
    float rgba[4];
    float rotation[3];
    float scale[2];
    int blend_mode;
    int priority;
    // ... animation fields
};
```

**Target**: Frame_AF contains vector of Layer structs
```cpp
struct Layer {
    int spriteId = -1;
    bool usePat;
    int offset_x, offset_y;
    float rgba[4]{1,1,1,1};
    float rotation[3]{};
    float scale[2]{1,1};
    int blend_mode;
    int priority;
};

struct Frame_AF {
    std::vector<Layer> layers;
    // ... animation fields (duration, aniType, jump, etc.)
};
```

### Shared Memory Architecture
**Current**: CopyData embedded in FrameState
```cpp
struct FrameState {
    struct CopyData { ... } copied;
};
```

**Target**: CopyData pointer in shared memory
```cpp
struct FrameState {
    CopyData *copied;  // Points to shared memory
private:
    void *sharedMemHandle;
    void *sharedMem;
};
```

---

## Implementation Phases

## Phase 1: Layer System Foundation
**Priority**: CRITICAL (blocks range paste features)
**Estimated Effort**: Medium-High
**Files Modified**: framedata.h, framedata.cpp, frame_disp.h, render.cpp, main_pane.cpp

### Tasks:

#### 1.1: Create Layer Structure
- [ ] Add `struct Layer` to framedata.h with all sprite properties
- [ ] Add selectedLayer to FrameState for UI tracking

#### 1.2: Refactor Frame_AF
- [ ] Extract sprite properties into Layer struct
- [ ] Replace direct properties with `std::vector<Layer> layers`
- [ ] Keep animation control fields (duration, aniType, jump, etc.)
- [ ] Update Frame_AF constructor to initialize with 1 default layer

#### 1.3: Update Save/Load Functions
- [ ] Modify WriteSequence to handle layer vectors
- [ ] Update load() to read layers (backward compatible: single layer for old format)
- [ ] Add version detection or migration logic for old .HA6 files

#### 1.4: Update AfDisplay UI
- [ ] Add layer slider: `SliderInt("Layer", &selectedLayer, 0, maxLayers-1)`
- [ ] Add "Add" button to push new layer
- [ ] Add "Del" button to remove layer (enforce min 1 layer)
- [ ] Update all property editors to modify `layers[selectedLayer]`
- [ ] Add layer count indicator

#### 1.5: Update Rendering Code
- [ ] Modify render.cpp to iterate through all layers
- [ ] Apply priority sorting if needed
- [ ] Ensure each layer renders with correct sprite/transform

#### 1.6: Update Copy/Paste
- [ ] Ensure Frame_AF copy includes all layers
- [ ] Update range paste foundation (will implement in Phase 3)

**Migration Strategy**:
- Initialize existing frames with 1 layer containing current properties
- Old save files load as single-layer frames
- New save files include layer count

---

## Phase 2: Shared Memory Cross-Instance Support
**Priority**: LOW (Future enhancement)
**Status**: DEFERRED
**Reason**: Requires extensive templating of all data structures with custom allocator (Frame_T, Sequence_T, etc.). For MBAACC editing, single-instance clipboard is sufficient. Future implementation should use serialization approach instead.

### Tasks:

#### 2.1: Extract FrameState to Separate Files
- [ ] Create framestate.h and framestate.cpp
- [ ] Move FrameState struct from draw_window.h
- [ ] Add constructor/destructor for memory management

#### 2.2: Implement Shared Memory
- [ ] Add Windows API includes (`windows.h`)
- [ ] Add shared memory handles to FrameState:
```cpp
private:
    void *sharedMemHandle = nullptr;
    void *sharedMem = nullptr;
```

#### 2.3: Create Shared Memory in Constructor
- [ ] Use `CreateFileMapping` with name "hanteichan-shared_mem"
- [ ] Allocate 16MB buffer (adjust as needed)
- [ ] Use `MapViewOfFileEx` to fixed address
- [ ] Detect if first instance (GetLastError != ERROR_ALREADY_EXISTS)
- [ ] Place CopyData at end of buffer

#### 2.4: Initialize CopyData Pointer
- [ ] First instance: placement new `CopyData` in shared memory
- [ ] Later instances: reinterpret_cast to existing CopyData
- [ ] Update all code from `currState.copied.X` to `currState.copied->X`

#### 2.5: Cleanup in Destructor
- [ ] UnmapViewOfFile
- [ ] CloseHandle on shared memory

**Note**: May need LinearAllocator for vectors in CopyData (Eiton uses custom allocator)

---

## Phase 3: Range Paste Window
**Priority**: HIGH (depends on Phase 1)
**Estimated Effort**: Medium
**Files Modified**: main_pane.h, main_pane.cpp

### Tasks:

#### 3.1: Add Range Paste UI State
- [ ] Add `bool rangeWindow = false;` to MainPane
- [ ] Add `int ranges[2] = {0, 0};` for frame range

#### 3.2: Create Range Paste Window
- [ ] Add menu button to open range paste window
- [ ] Create `ImGui::Begin("Range paste", &rangeWindow)`
- [ ] Add `InputInt2("Range of frames", ranges)` for range selection

#### 3.3: Implement Paste Color to Range
- [ ] Button: "Paste color"
- [ ] Copy rgba and blend_mode from current layer to all layers in range
```cpp
auto &iLayer = seq->frames[currState.frame].AF.layers[currState.selectedLayer];
for(int i = ranges[0]; i <= ranges[1] && i < seq->frames.size(); i++)
    for(auto &oLayer : seq->frames[i].AF.layers)
        // Copy rgba, blend_mode
```

#### 3.4: Implement Set Landing Frame
- [ ] Button: "Set landing frame"
- [ ] Set `landJump` to specified value for all frames in range
- [ ] Input field for landing frame number

#### 3.5: Implement Copy/Paste Multiple Frames
- [ ] Button: "Copy frames" - copy entire frame range to clipboard
- [ ] Button: "Paste frames" - insert copied frames at position
- [ ] Use `currState.copied->frames` vector

#### 3.6: Implement Paste Transform
- [ ] Button: "Paste transform (Current layer only)"
- [ ] Copy offset, scale, rotation from current layer to range
- [ ] Only affects current layer index in target frames

#### 3.7: Add Range Validation
- [ ] Clamp ranges to valid frame indices
- [ ] Show warning if range is invalid

---

## Phase 4: Hitbox Copy/Paste UI
**Priority**: MEDIUM (independent)
**Estimated Effort**: Low
**Files Modified**: box_pane.cpp

### Tasks:

#### 4.1: Add Hitbox Group Copy/Paste
- [ ] Find appropriate location in box_pane.cpp UI
- [ ] Add button: "Copy all boxes"
- [ ] Add button: "Paste all boxes"
- [ ] Use `currState.copied->boxes` (already in CopyData)
- [ ] Mark modified on paste

#### 4.2: Add Single Hitbox Copy/Paste
- [ ] Add button: "Copy params" next to individual boxes
- [ ] Add button: "Paste params"
- [ ] Use `currState.copied->box` (already in CopyData)
- [ ] Mark modified on paste

---

## Phase 5: Testing & Polish
**Priority**: CRITICAL
**Estimated Effort**: Medium

### Tasks:

#### 5.1: Layer System Testing
- [ ] Test single layer works (backward compatibility)
- [ ] Test adding/removing layers
- [ ] Test layer rendering with multiple sprites
- [ ] Test save/load with layers
- [ ] Test copy/paste preserves layers

#### 5.2: Shared Memory Testing
- [ ] Launch multiple instances
- [ ] Copy pattern in instance 1, paste in instance 2
- [ ] Verify all data types copy correctly
- [ ] Test frame/AS/AF/AT/EF/IF cross-instance
- [ ] Test crash recovery (one instance closes)

#### 5.3: Range Paste Testing
- [ ] Test paste color across range
- [ ] Test paste transform across range
- [ ] Test set landing frame
- [ ] Test copy/paste multiple frames
- [ ] Test range validation

#### 5.4: Hitbox Testing
- [ ] Test copy/paste all boxes
- [ ] Test copy/paste single box params

---

## Implementation Order & Dependencies

```
Phase 1: Layer System (MUST DO FIRST)
    ↓
Phase 3: Range Paste (depends on layers)

Phase 2: Shared Memory (can do in parallel with Phase 3)

Phase 4: Hitbox Copy/Paste (can do anytime)

Phase 5: Testing & Polish (LAST)
```

**Recommended Order**:
1. **Phase 1** (Layer System) - Enables everything else
2. **Phase 2** (Shared Memory) - Major architectural change
3. **Phase 3** (Range Paste) - Complex UI/logic
4. **Phase 4** (Hitbox) - Quick win
5. **Phase 5** (Testing) - Validate everything

---

## Future Enhancements (Post-Parity)

### Multi-Character Tab Support
- Tab-based UI for loading multiple characters
- Shared clipboard across tabs
- Quick character switching
- Cross-character copy/paste

### Additional Features to Consider
- Undo/redo system
- Batch export modified patterns
- Pattern search/filter
- Frame timeline view
- Animation preview

---

## Risk Assessment

### High Risk
- **Layer system refactor**: Touches many files, breaks save compatibility
  - *Mitigation*: Version detection, migration code, thorough testing
- **Shared memory**: Memory corruption, pointer issues, crash on cleanup
  - *Mitigation*: Defensive checks, handle errors gracefully

### Medium Risk
- **Range paste**: Off-by-one errors, invalid ranges
  - *Mitigation*: Range clamping, validation
- **Multi-instance**: Race conditions, stale data
  - *Mitigation*: Shared memory handles synchronization

### Low Risk
- **Hitbox copy/paste**: Simple UI additions
  - *Mitigation*: Standard copy/paste pattern

---

## Success Criteria

- ✅ Can add/remove/edit multiple layers per frame
- ✅ Can save/load multi-layer frames
- ✅ Can copy/paste between different application instances
- ✅ Can perform range paste operations (color, transform, landing)
- ✅ Can copy/paste hitboxes individually and in groups
- ✅ All existing features continue to work
- ✅ No crashes or data corruption in multi-instance scenarios
