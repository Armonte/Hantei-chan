# Effect Type 3 Implementation Status - Hantei-chan

## Completed ✅

### 1. Data Structures Updated
- **Added `isPresetEffect` flag** to `SpawnedPatternInfo` struct (`framestate.h:31`)
- **Added `isPresetEffect` flag** to `ActiveSpawnInstance` struct (`framestate.h:74`)
- **Added `showPresetEffects` toggle** to `VisualizationSettings` struct (`framestate.h:92`)

### 2. Effect Parsing
- **Updated `ParseSpawnedPatterns`** in `framestate.cpp:165` to set `isPresetEffect = true` for Effect Type 3
- Updated comment to reference `Effect_Type3_Implementation.md` for preset number documentation

### 3. Data Flow
- **Updated conversion** from `SpawnedPatternInfo` to `ActiveSpawnInstance` in `main_frame.cpp:137` to copy `isPresetEffect` flag
- **Updated animation spawn** creation in `main_frame.cpp:758` to copy `isPresetEffect` flag

### 4. Rendering Logic
- **Added check** in main render loop (`main_frame.cpp:149`) to detect Effect Type 3
- **Skip pattern loading** for preset effects with `continue` statement
- Added placeholder for crosshair rendering with proper toggle check

---

## In Progress ⚠️

### 5. Crosshair Rendering Implementation
**Status**: Skeleton added, rendering code needed

**Location**: `main_frame.cpp:151-155`

**Current Code**:
```cpp
if (spawnInfo.isPresetEffect) {
    // Render crosshair marker for preset effects (not pattern-based)
    if (state.vizSettings.showPresetEffects) {
        // TODO: Draw crosshair at (spawnInfo.offsetX, spawnInfo.offsetY)
        // Preset number: spawnInfo.patternId
        // For now, skip rendering (will be implemented below)
    }
    continue;  // Skip pattern loading for preset effects
}
```

**Next Steps**:
1. Find appropriate rendering location (after OpenGL scene render, before/during ImGui overlay)
2. Use `ImGui::GetForegroundDrawList()` or `ImGui::GetWindowDrawList()` to get draw list
3. Convert world coordinates `(offsetX, offsetY)` to screen space
4. Draw crosshair using `ImDrawList::AddLine()` functions

**Proposed Implementation** (pseudocode):
```cpp
if (state.vizSettings.showPresetEffects && spawnInfo.isPresetEffect) {
    // Convert world coords to screen coords
    ImVec2 screenPos = WorldToScreen(spawnInfo.offsetX, spawnInfo.offsetY);

    // Get draw list
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Draw crosshair
    float size = 20.0f;  // Crosshair arm length
    ImU32 color = IM_COL32(255, 128, 0, 255);  // Orange
    float thickness = 2.0f;

    // Horizontal line
    drawList->AddLine(
        ImVec2(screenPos.x - size, screenPos.y),
        ImVec2(screenPos.x + size, screenPos.y),
        color, thickness);

    // Vertical line
    drawList->AddLine(
        ImVec2(screenPos.x, screenPos.y - size),
        ImVec2(screenPos.x, screenPos.y + size),
        color, thickness);

    // Center dot
    drawList->AddCircleFilled(screenPos, 4.0f, color);

    // Optional: Label with preset number
    if (state.vizSettings.showLabels) {
        char label[32];
        snprintf(label, sizeof(label), "P%d", spawnInfo.patternId);
        drawList->AddText(
            ImVec2(screenPos.x + 25, screenPos.y - 5),
            color, label);
    }
}
```

---

## Pending 📋

### 6. UI Toggle Checkbox
**Status**: Data structure ready, UI control needed

**Where to Add**: Likely in `right_pane.cpp` or `box_pane.cpp` in visualization settings section

**Code to Add**:
```cpp
ImGui::Checkbox("Show Preset Effects (Type 3)", &state.vizSettings.showPresetEffects);
```

**Search for existing checkboxes**:
```bash
grep -n "showSpawnedPatterns\|showOffsetLines" Hantei-chan/src/*.cpp
```

### 7. Preset Names Mapping
**Status**: Documentation exists, mapping constant needed

**Purpose**: Show "Red Hitspark" instead of "Preset 3" in labels/tooltips

**Location**: Create `src/preset_effects.h` and `src/preset_effects.cpp`

**Implementation**:
```cpp
// preset_effects.h
#ifndef PRESET_EFFECTS_H_GUARD
#define PRESET_EFFECTS_H_GUARD

#include <string>

// Get human-readable name for preset effect number
// Based on Effect_Type3_Implementation.md
const char* GetPresetEffectName(int presetNumber);

// Get description of preset effect
const char* GetPresetEffectDescription(int presetNumber);

#endif

// preset_effects.cpp
#include "preset_effects.h"

const char* GetPresetEffectName(int presetNumber) {
    switch(presetNumber) {
        case 0:  return "Jump Effect";
        case 1:  return "Unknown Effect";
        case 2:  return "David Star (Lag Warning)";
        case 3:  return "Red Hitspark";
        case 4:  return "Force Field";
        case 5:  return "Fire Particles";
        case 6:  return "Fire Effect";
        case 7:  return "Snow Particles";
        case 8:  return "Blue Flash";
        case 9:  return "Blue Hitspark";
        case 10: return "Superflash Background";
        case 14: return "Unknown (0x0e)";
        case 15: return "Unknown (0x0f)";
        case 16: return "Unknown (0x10)";
        case 19: return "Unknown (0x13)";
        case 20: return "3D Rotating Waves";
        case 21: return "Foggy Rays";
        case 22: return "Particle Spray";
        case 23: return "Blinding Effect";
        case 24: return "Blinding Effect 2";
        case 27: return "Dust Cloud (Small)";
        case 28: return "Dust Cloud (Large)";
        case 29: return "Dust Cloud (Rotating)";
        case 30: return "Massive Dust Cloud";
        case 256: return "Unknown (0x100)";
        case 265: return "Particle Spray (Large)";
        case 300: return "Unknown (0x12c)";
        case 301: return "Unknown (0x12d)";
        case 302: return "Complex Particle System";
        case 100: return "Boss Aoko Circle";
        case 101: return "Boss Aoko Circle (Fast)";
        default: return "Unknown Preset";
    }
}

const char* GetPresetEffectDescription(int presetNumber) {
    switch(presetNumber) {
        case 3:  return "Red hitspark with radiating sparks";
        case 9:  return "Blue hitspark burst";
        case 10: return "Superflash freeze (no visual)";
        case 27:
        case 28:
        case 29: return "Procedural dust cloud particles";
        default: return "Procedural particle effect";
    }
}
```

### 8. Timeline Display
**Status**: Already works! (Preset effects appear as spawned patterns in timeline)

The timeline in `box_pane.cpp` already shows spawned patterns. Effect Type 3 will appear there automatically since we added it to `ParseSpawnedPatterns`.

**Enhancement**: Color-code Effect Type 3 differently in timeline
- Modify `box_pane.cpp:317` where timeline bars are drawn
- Check `sp.isPresetEffect` and use different color (e.g., orange instead of blue)

```cpp
// In box_pane.cpp timeline drawing loop
ImU32 barColor = sp.isPresetEffect ?
    IM_COL32(255, 128, 0, 180) :  // Orange for preset effects
    IM_COL32(100, 100, 255, 180);  // Blue for pattern spawns
drawList->AddRectFilled(barMin, barMax, barColor);
```

---

## Testing Checklist 🧪

Once implementation is complete:

1. [ ] Load a character with Effect Type 3 in patterns
2. [ ] Verify timeline shows orange bars for preset effects
3. [ ] Verify crosshair appears at correct position in viewport
4. [ ] Verify toggle checkbox hides/shows crosshairs
5. [ ] Verify preset names appear in labels (if name mapping added)
6. [ ] Test with different preset numbers (3, 9, 10, 27, etc.)
7. [ ] Verify no crashes when Effect Type 3 present
8. [ ] Verify spawned pattern count is correct (doesn't try to load non-existent patterns)

---

## Architecture Notes 📐

### Why This Approach Works

**Problem**: Effect Type 3 spawns procedural particle effects, not patterns from .ha6 files.

**Solution**:
1. Mark Effect Type 3 with `isPresetEffect` flag
2. Skip normal pattern loading logic
3. Render simple crosshair marker instead
4. User can position effects visually without needing full particle simulation

**Benefits**:
- ✅ No need to reverse-engineer particle rendering pipeline
- ✅ Simple visual indicator for effect placement
- ✅ Works with existing spawn detection/timeline system
- ✅ Toggle-able for clean view when not needed
- ✅ Extensible: can add preset names/descriptions later

### Data Flow

```
Pattern .ha6 File
    ↓
Frame.EF (Effect Type 3, number, params)
    ↓
ParseSpawnedPatterns() → SpawnedPatternInfo (isPresetEffect=true)
    ↓
During Animation → ActiveSpawnInstance (isPresetEffect=true)
    ↓
Render Loop → Detects isPresetEffect
    ↓
Skip pattern loading → Draw crosshair instead
```

---

## Related Documentation 📚

- **`/mnt/c/games/qoh/gHantei_Docs/Effect_Type3_Implementation.md`** - Complete technical reference for all preset effects
- **`/mnt/c/games/qoh/gHantei_Docs/Effect_Type3_Blocker_Resolution.md`** - Explains why preset effects are hardcoded, visualization options
- **`@gHantei_Docs/Hantei_Effects.md`** - Original effect documentation with Effect Type 3 parameter descriptions

---

## Build Instructions 🔨

When ready to test:

```bash
cd /mnt/c/games/qoh/Hantei-chan/build
cmake .. -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake
cmake --build .
./hantei-chan.exe
```

---

**Last Updated**: 2025-10-05
**Status**: Data structures complete, rendering code in progress
**Next Task**: Implement crosshair rendering using ImGui draw list
