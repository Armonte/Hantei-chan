# MBAACC Background System Integration Plan for Hantei-chan

## Executive Summary

This document outlines the technical plan for integrating u4ick's bgmake tool functionality into Hantei-chan, enabling both background viewing in the character editor viewport and a full-featured background editor/creator. The integration leverages the existing viewport architecture while adding sophisticated parallax camera simulation and stage rendering capabilities.

## Project Goals

1. **Viewport Integration**: Display MBAACC stages behind characters in the main editor viewport with accurate parallax camera simulation
2. **Auto-Loading**: Automatically discover and load stages from the MBAACC game directory
3. **Background Editor**: Create a dedicated tab/mode for editing and creating new background files
4. **Format Compatibility**: Full support for MBAACC `.dat` background files and embedded CG sprite data
5. **Real-time Preview**: Live preview of stage animations synchronized with character movement

## Architecture Overview

### Core Components

#### 1. Background System Module (`src/background/`)
New subsystem containing:
- `bgmake_file.h/cpp` - Background file loader and serializer
- `bgmake_object.h/cpp` - Background object data structures
- `bgmake_frame.h/cpp` - Frame animation data
- `bg_renderer.h/cpp` - OpenGL rendering implementation
- `camera_sim.h/cpp` - Parallax camera simulation
- **Note**: CG sprite loading uses existing `src/cg.h/cpp` (already handles MBAACC format!)

#### 2. Integration Points

**Main Viewport (`character_view.cpp`, `render.cpp`)**
- Add background rendering layer behind character sprites
- Implement camera position tracking for parallax
- Add toggle for enabling/disabling background display

**Status Bar**
- Add checkbox: "Enable Stage & Camera"
- Display current stage name when enabled
- Camera position/zoom display

**Tab System**
- New tab type: "Background Editor"
- Separate view for background-only editing
- Object list, frame timeline, layer management

## Technical Implementation Details

### Phase 1: Data Structure Foundation

#### Background File Structure (Port from C#)

```cpp
// bgmake_file.h
class BGMakeFile {
public:
    struct Header {
        char magic[16];          // "bgmake" (6 bytes) + 10 null bytes
        int32_t unk;             // Typically 1
        int32_t pat_file_off;    // Unused (-1)
        int32_t pat_file_len;    // Unused (0)
        int32_t cg_file_off;     // CG data offset
        int32_t cg_file_len;     // CG data length
        uint8_t reserved[48];
    };

    struct OffsetTable {
        int32_t offsets[256];    // Object offset table (-1 = unused)
    };

    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path);
    
    std::vector<BGMakeObject> objects;
    std::vector<uint8_t> cgData;
    CGFile* cgFile = nullptr;

private:
    Header header;
    OffsetTable offsetTable;
};
```

#### Background Object Structure

```cpp
// bgmake_object.h
class BGMakeObject {
public:
    struct ObjectHeader {
        int32_t numFrames;
        int32_t parallax;        // Default: 256 (1.0x)
        int32_t layer;           // Default: 128
        int32_t reserved1;       // -1
        int32_t reserved2;       // -1
        uint8_t padding[40];
    };

    std::string name;
    int32_t frameIndex = 0;           // Current animation frame
    int32_t frameDurationIndex = 0;   // Frame timer
    std::vector<BGMakeFrame> frames;
    
    // Runtime state
    float currentX = 0.0f;
    float currentY = 0.0f;
    
    void Update();                     // Advance animation
    void Reset();                      // Reset to frame 0
};
```

#### Frame Data Structure

```cpp
// bgmake_frame.h
class BGMakeFrame {
public:
    // File format data (132 bytes)
    int16_t spriteId;         // Sprite ID (stored as +10000 in file)
    int16_t xOffset;
    int16_t yOffset;
    int16_t duration;         // Duration in frames (60fps)
    uint8_t unk;
    uint8_t drawType;         // 0=normal, 2=additive
    uint8_t opacity;          // 0-255
    uint8_t animType;         // 1=loop, 2=jump
    uint8_t jumpFrame;        // Target frame for type 2
    uint8_t enableXVec;
    uint8_t enableYVec;
    int16_t xVec;             // Movement vector
    int16_t yVec;
    
    // Computed runtime data
    float computedX = 0.0f;
    float computedY = 0.0f;
};
```

### Phase 2: CG Sprite System

#### Use Hantei-chan's Existing CG Loader! ✓

**Great news:** Hantei-chan already has a complete CG loader (`cg.cpp`/`cg.h`) that handles MBAACC format perfectly!

**No need to port cglib** - the existing system already supports:
- ✓ "BMP Cutter3" format (MBAACC standard)
- ✓ All compression types and render modes
- ✓ Palette loading and switching
- ✓ Integration with existing renderer
- ✓ Battle-tested on character files

**Simple Integration Strategy:**

```cpp
// Just add memory loading capability to existing CG class
// cg.h
class CG {
public:
    bool load(const char *name);                              // Existing
    bool loadFromMemory(const char* data, unsigned int size); // NEW - simple addition
    // ... rest of existing interface ...
};

// Then use it in bgmake_file.cpp
bool BGMakeFile::LoadFromFile(const std::string& path) {
    // ... load objects ...
    
    // Load embedded CG using existing system
    if (cg_len > 0) {
        file.seekg(cg_off);
        cgData.resize(cg_len);
        file.read((char*)cgData.data(), cg_len);
        
        cgFile = new CG();  // Use existing CG class!
        cgFile->loadFromMemory((char*)cgData.data(), cgData.size());
    }
    
    return true;
}

// Render sprites using existing ImageData system
ImageData* sprite = bgFile->cgFile->draw_texture(frame.spriteId, false, false);
// Create texture from sprite->pixels (RGBA format)
// Cache texture by sprite ID for performance
```

This approach saves weeks of implementation time and ensures consistency with character rendering. See `BGMAKE_CG_ANALYSIS.md` for detailed comparison and implementation guide.

### Phase 3: Rendering System

#### Background Renderer

```cpp
// bg_renderer.h
class BGRenderer {
public:
    BGRenderer();
    ~BGRenderer();

    void SetBackgroundFile(BGMakeFile* file);
    void Update();                          // Update animation state
    void Render(const CameraState& camera); // Render with parallax
    
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled; }
    
    // Editor mode
    void SetSelectedObject(int index);
    int GetSelectedObject() const { return selectedObjectIndex; }

private:
    BGMakeFile* bgFile = nullptr;
    bool enabled = false;
    int selectedObjectIndex = -1;
    
    void RenderObject(const BGMakeObject& obj, const CameraState& camera);
    void ApplyParallax(float& x, float& y, float parallax, const CameraState& camera);
    void SetupBlendMode(uint8_t drawType);
};
```

#### Camera Simulation

```cpp
// camera_sim.h
class CameraState {
public:
    float x = 0.0f;              // World X position
    float y = 0.0f;              // World Y position
    float zoom = 1.0f;           // Zoom level
    
    // Character-following camera
    void FollowCharacter(float charX, float charY);
    
    // Manual control (editor mode)
    void Pan(float deltaX, float deltaY);
    void Zoom(float deltaZoom);
    
    // Parallax calculation
    float ApplyParallaxX(float worldX, int parallaxValue) const;
    float ApplyParallaxY(float worldY, int parallaxValue) const;
    
private:
    static constexpr int DEFAULT_PARALLAX = 256;
};

// Parallax formula from bgmake
inline float CameraState::ApplyParallaxX(float worldX, int parallaxValue) const {
    float parallaxFactor = parallaxValue / 256.0f;
    return worldX + (x * parallaxFactor);
}

inline float CameraState::ApplyParallaxY(float worldY, int parallaxValue) const {
    float parallaxFactor = parallaxValue / 256.0f;
    return worldY + (y * parallaxFactor);
}
```

### Phase 4: Render Pipeline Integration

#### Integration with Existing Render Class

Modify `render.h` and `render.cpp`:

```cpp
// Add to Render class
class Render {
    // ... existing members ...
    
    // Background rendering
    BGRenderer* bgRenderer = nullptr;
    CameraState cameraState;
    
public:
    // Existing methods...
    
    // New background methods
    void SetBackgroundRenderer(BGRenderer* renderer);
    void EnableBackground(bool enable);
    bool IsBackgroundEnabled() const;
    
    void UpdateCamera(float charX, float charY);
    CameraState& GetCamera() { return cameraState; }
    
    // Modified Draw method
    void Draw() {
        // 1. Draw background layers (layers < 128 or all if no char)
        if (bgRenderer && bgRenderer->IsEnabled()) {
            bgRenderer->Render(cameraState);
        }
        
        // 2. Draw character sprite (existing code)
        DrawSpriteOnly();
        
        // 3. Draw hitboxes/UI overlays (existing code)
        // ... existing code ...
    }
};
```

### Phase 5: UI Integration

#### Status Bar Toggle

```cpp
// In main_frame.cpp or status_bar component
void MainFrame::DrawStatusBar() {
    ImGui::Begin("Status Bar");
    
    // ... existing status items ...
    
    ImGui::Separator();
    bool bgEnabled = render->IsBackgroundEnabled();
    if (ImGui::Checkbox("Enable Stage & Camera", &bgEnabled)) {
        render->EnableBackground(bgEnabled);
    }
    
    if (bgEnabled && bgManager) {
        ImGui::SameLine();
        ImGui::Text("| Stage: %s", bgManager->GetCurrentStageName().c_str());
        
        auto& cam = render->GetCamera();
        ImGui::SameLine();
        ImGui::Text("| Cam: (%.0f, %.0f) %.2fx", cam.x, cam.y, cam.zoom);
    }
    
    ImGui::End();
}
```

#### Stage Selection Menu

```cpp
// Add to main menu or toolbar
void MainFrame::DrawStageMenu() {
    if (ImGui::BeginMenu("Stage")) {
        if (ImGui::MenuItem("Load Stage...", "Ctrl+Shift+B")) {
            // Open file dialog for .dat files
            std::string path = OpenFileDialog("MBAACC Stage Files (*.dat)\0*.dat\0");
            if (!path.empty()) {
                bgManager->LoadStage(path);
            }
        }
        
        if (ImGui::MenuItem("Auto-discover Stages")) {
            bgManager->ScanMBAADirectory();
        }
        
        ImGui::Separator();
        
        // List discovered stages
        auto stages = bgManager->GetAvailableStages();
        for (size_t i = 0; i < stages.size(); i++) {
            if (ImGui::MenuItem(stages[i].name.c_str())) {
                bgManager->LoadStage(stages[i].path);
            }
        }
        
        ImGui::EndMenu();
    }
}
```

### Phase 6: Background Editor Tab

#### New Tab System Component

```cpp
// background_editor_view.h
class BackgroundEditorView {
public:
    BackgroundEditorView(BGRenderer* renderer);
    
    void Draw();  // Main ImGui drawing
    
private:
    BGRenderer* bgRenderer;
    BGMakeFile* currentFile = nullptr;
    int selectedObjectIndex = -1;
    int selectedFrameIndex = -1;
    
    // UI panels
    void DrawObjectList();
    void DrawObjectProperties();
    void DrawFrameTimeline();
    void DrawFrameProperties();
    void DrawLayerManager();
    void DrawViewport();
    void DrawToolbar();
    
    // Editing operations
    void AddNewObject();
    void DeleteObject(int index);
    void DuplicateObject(int index);
    void AddFrame();
    void DeleteFrame(int frameIndex);
    void CopyFrame();
    void PasteFrame();
    
    // File operations
    void NewFile();
    void LoadFile();
    void SaveFile();
    void SaveFileAs();
};
```

#### Editor Layout

```
+----------------------------------------------------------+
| Toolbar: [New] [Open] [Save] [Import CG] [Export] [Play]|
+------------------+----------------------------+-----------+
|  Object List     |   Viewport                 | Frame     |
|  ================|   (Preview with grid)      | Props     |
|  □ Background    |                            |-----------|
|  □ Midground     |   [Stage preview here]     | Sprite ID:|
|  ☑ Clouds        |                            | [   42  ] |
|  □ Foreground    |                            | X Offset: |
|  □ Particles     |                            | [  100  ] |
|                  |                            | Y Offset: |
|  [Add Object]    |                            | [  -50  ] |
|  [Delete]        |   Camera: (0, 0)           | Duration: |
|  [Duplicate]     |   Zoom: 1.0x               | [   60  ] |
|                  |                            | Draw Type:|
|------------------|                            | [Normal▼] |
| Object Props     |                            | Opacity:  |
| ================ |                            | [===255] |
| Name: [Clouds  ] |                            | Anim Type:|
| Parallax: [128 ] |                            | [Loop  ▼] |
| Layer:    [100 ] +----------------------------+-----------+
+------------------+ Timeline                              |
                    [====|====|====|====|====|====|====]   |
                    Frame: 1/12  60fps                     |
                    [◀◀] [◀] [▶] [▶▶] [Loop] [Onion Skin]  |
                    +----------------------------------------+
```

### Phase 7: Auto-Discovery System

#### Stage Manager

```cpp
// bg_manager.h
class BackgroundManager {
public:
    struct StageInfo {
        std::string name;
        std::string path;
        std::string category;  // "Game Stages", "Custom", etc.
    };

    BackgroundManager();
    
    // Discovery
    void ScanMBAADirectory(const std::string& mbaaPath = "");
    void ScanDirectory(const std::string& path);
    std::vector<StageInfo> GetAvailableStages() const;
    
    // Loading
    bool LoadStage(const std::string& path);
    BGMakeFile* GetCurrentStage() { return currentStage.get(); }
    std::string GetCurrentStageName() const { return currentStageName; }
    
    // Settings
    void SetMBAAPath(const std::string& path);
    std::string GetMBAAPath() const;

private:
    std::vector<StageInfo> availableStages;
    std::unique_ptr<BGMakeFile> currentStage;
    std::string currentStageName;
    std::string mbaaBasePath;
    
    void LoadSettings();
    void SaveSettings();
    
    // Common MBAA stage paths
    static constexpr const char* STAGE_PATHS[] = {
        "data/stage/",
        "data/",
        "stage/"
    };
};
```

#### Settings Integration

Add to `ini.h/cpp`:

```cpp
struct Settings {
    // ... existing settings ...
    
    // Background system
    char mbaaPath[512] = "";
    bool enableBackgrounds = false;
    bool autoLoadDefaultStage = false;
    char defaultStage[256] = "";
    float bgOpacity = 1.0f;
    bool showCameraInfo = true;
};
```

### Phase 8: Animation System

#### Frame Animation Logic (from bgmake)

```cpp
void BGMakeObject::Update() {
    if (frames.empty()) return;
    
    auto& currentFrame = frames[frameIndex];
    
    // Advance duration counter
    frameDurationIndex++;
    
    // Check if frame is complete
    if (frameDurationIndex >= currentFrame.duration) {
        frameDurationIndex = 0;
        
        // Handle animation type
        switch (currentFrame.animType) {
            case 1:  // Loop animation
                frameIndex++;
                if (frameIndex >= frames.size()) {
                    frameIndex = 0;  // Loop back
                }
                break;
                
            case 2:  // Jump animation
                frameIndex = currentFrame.jumpFrame;
                if (frameIndex >= frames.size()) {
                    frameIndex = 0;  // Safety clamp
                }
                break;
                
            default:
                frameIndex = 0;
                break;
        }
    }
    
    // Apply movement vectors if enabled
    if (currentFrame.enableXVec) {
        currentX += currentFrame.xVec;
    }
    if (currentFrame.enableYVec) {
        currentY += currentFrame.yVec;
    }
}
```

## Implementation Phases

### Phase 1: Foundation (Week 1-2)
- [ ] Create `src/background/` directory structure
- [ ] Implement data structures (BGMakeFile, BGMakeObject, BGMakeFrame)
- [ ] Implement .dat file loader (read-only first)
- [ ] Add unit tests for file parsing

### Phase 2: CG Integration & Basic Rendering (Week 2)
- [ ] Add `CG::loadFromMemory()` method (simple addition to existing class)
- [ ] Test loading embedded CG from .dat files
- [ ] Verify sprite IDs and rendering
- [ ] Create BGRenderer class with texture caching
- [ ] Integrate with existing Render class
- [ ] Test rendering static backgrounds

### Phase 3: Camera & Parallax (Week 3-4)
- [ ] Implement CameraState class
- [ ] Add parallax calculation logic
- [ ] Test with various parallax values
- [ ] Add camera following character movement

### Phase 4: Animation (Week 4-5)
- [ ] Implement frame animation logic
- [ ] Add 60fps timing system
- [ ] Support loop and jump animation types
- [ ] Test with animated backgrounds

### Phase 5: UI Integration (Week 5-6)
- [ ] Add status bar toggle
- [ ] Create stage selection menu
- [ ] Implement auto-discovery system
- [ ] Add settings for MBAA path

### Phase 6: Background Editor (Week 6-8)
- [ ] Create BackgroundEditorView class
- [ ] Implement object list UI
- [ ] Implement frame timeline UI
- [ ] Add editing operations (add, delete, modify)
- [ ] Implement file save functionality

### Phase 7: Polish & Testing (Week 8-10)
- [ ] Performance optimization
- [ ] Memory management improvements
- [ ] Bug fixes and edge cases
- [ ] Documentation and user guide

## Technical Challenges & Solutions

### Challenge 1: CG File Format
**Problem**: ~~Proprietary format with compression~~ **SOLVED!**
**Solution**: ✓ Use Hantei-chan's existing CG loader - already handles all MBAACC formats perfectly. Just add `loadFromMemory()` method and reuse all existing decompression logic.

### Challenge 2: C# to C++ Port
**Problem**: bgmake is C# with .NET dependencies
**Solution**:
- Port core logic, not UI code
- Replace ObservableCollection with std::vector
- Replace System.Drawing with OpenGL textures
- Use GLM for math instead of XNA Framework

### Challenge 3: Performance
**Problem**: Rendering many animated sprites at 60fps
**Solution**:
- Texture atlasing for sprites
- Batch rendering by blend mode/layer
- Culling off-screen objects
- Lazy texture loading

### Challenge 4: Integration with Character Editor
**Problem**: Background rendering shouldn't interfere with hitbox editing
**Solution**:
- Render backgrounds in separate pass
- Toggle visibility independently
- Different blend modes for background vs editor UI
- Clear depth buffer between passes

## File Format Reference

### MBAACC Background .dat File Structure

```
Offset | Size  | Type   | Description
-------|-------|--------|----------------------------------
0x0000 | 6     | char[] | Magic: "bgmake"
0x0006 | 10    | bytes  | Padding (null bytes)
0x0010 | 4     | int32  | Unknown (1)
0x0014 | 4     | int32  | Pattern offset (unused, -1)
0x0018 | 4     | int32  | Pattern length (unused, 0)
0x001C | 4     | int32  | CG file offset
0x0020 | 4     | int32  | CG file length
0x0024 | 48    | bytes  | Reserved
0x0040 | 1024  | int32[]| Object offset table (256 entries)
0x0440 | var   | ...    | Object data
...    | var   | bytes  | Embedded CG file data
```

### Object Data Structure (60 bytes + frames)

```
Offset | Size | Type   | Description
-------|------|--------|---------------------------
0x00   | 4    | int32  | Number of frames
0x04   | 4    | int32  | Parallax value (256 = 1.0x)
0x08   | 4    | int32  | Layer value (128 = default)
0x0C   | 4    | int32  | Reserved (-1)
0x10   | 4    | int32  | Reserved (-1)
0x14   | 40   | bytes  | Padding
0x3C   | var  | frame[]| Frame data (132 bytes each)
```

### Frame Data Structure (132 bytes)

```
Offset | Size | Type   | Description
-------|------|--------|--------------------------------
0x00   | 2    | int16  | Sprite ID (+10000 in file)
0x02   | 2    | int16  | X offset
0x04   | 2    | int16  | Y offset
0x06   | 2    | int16  | Duration (frames at 60fps)
0x08   | 1    | byte   | Unknown
0x09   | 1    | byte   | Draw type (0=normal, 2=add)
0x0A   | 1    | byte   | Opacity (0-255)
0x0B   | 1    | byte   | Animation type (1=loop, 2=jump)
0x0C   | 1    | byte   | Jump frame (for type 2)
0x0D   | 32   | bytes  | Reserved
0x2D   | 1    | byte   | Enable X vector
0x2E   | 1    | byte   | Enable Y vector
0x2F   | 4    | bytes  | Reserved
0x33   | 2    | int16  | X vector (movement/frame)
0x35   | 2    | int16  | Y vector (movement/frame)
0x37   | 45   | bytes  | Reserved
0x64   | 32   | bytes  | Reserved
```

## Dependencies & Build System

### New Dependencies
```cmake
# CMakeLists.txt additions

# Background system source files
set(BACKGROUND_SOURCES
    src/background/bgmake_file.cpp
    src/background/bgmake_object.cpp
    src/background/bgmake_frame.cpp
    src/background/bg_renderer.cpp
    src/background/bg_manager.cpp
    src/background/camera_sim.cpp
    # Note: CG loading uses existing src/cg.cpp - no new file needed!
)

# Background editor UI
set(BACKGROUND_EDITOR_SOURCES
    src/background/background_editor_view.cpp
)

target_sources(${PROJECT_NAME} PRIVATE 
    ${BACKGROUND_SOURCES}
    ${BACKGROUND_EDITOR_SOURCES}
)

# Existing dependencies already sufficient:
# - OpenGL (rendering)
# - GLM (math)
# - ImGui (UI)
# - GLAD (OpenGL loading)
```

## Testing Strategy

### Unit Tests
```cpp
// test_bgmake_loader.cpp
TEST(BGMakeLoader, LoadValidFile) {
    BGMakeFile file;
    ASSERT_TRUE(file.LoadFromFile("testdata/stage00.dat"));
    ASSERT_GT(file.objects.size(), 0);
}

TEST(BGMakeLoader, ParseObjectHeader) {
    BGMakeFile file;
    file.LoadFromFile("testdata/stage00.dat");
    auto& obj = file.objects[0];
    ASSERT_EQ(obj.header.parallax, 256);
    ASSERT_EQ(obj.header.layer, 128);
}

TEST(ParallaxCamera, CalculateOffset) {
    CameraState camera;
    camera.x = 100.0f;
    
    // Background (half speed)
    float bgX = camera.ApplyParallaxX(0.0f, 128);
    ASSERT_FLOAT_EQ(bgX, 50.0f);
    
    // Foreground (double speed)
    float fgX = camera.ApplyParallaxX(0.0f, 512);
    ASSERT_FLOAT_EQ(fgX, 200.0f);
}
```

### Integration Tests
- Load all official MBAACC stages
- Verify no crashes on malformed files
- Check animation timing accuracy
- Validate parallax calculations against in-game behavior

## Future Enhancements

### Post-MVP Features
1. **Stage Pack Export**: Bundle stages with all assets for distribution
2. **Advanced Camera**: Camera shake, zoom transitions, cinematic modes
3. **Particle Effects**: Additional particle system beyond sprite animation
4. **Sound Integration**: Background music/ambience playback
5. **Character-Stage Interaction**: Dust particles, footprints, stage-specific effects
6. **Stage Collisions**: Define walkable areas, stage boundaries
7. **3D Layer Support**: Add pseudo-3D layers with perspective transforms
8. **Animation Curves**: Bezier curve motion paths for objects
9. **Weather Effects**: Rain, snow, fog overlays
10. **Day/Night Variants**: Time-of-day variants for stages

## Conclusion

This integration brings powerful background editing capabilities to Hantei-chan while maintaining the tool's focus on character frame data editing. The modular design allows incremental implementation, with each phase delivering usable functionality. The viewport integration provides immediate value to character editors by showing stages in context, while the dedicated background editor opens new creative possibilities for stage creators in the MBAACC community.

By leveraging u4ick's groundbreaking reverse engineering work and combining it with Hantei-chan's modern C++ architecture, this creates a comprehensive toolset for MBAACC content creation.

---

*Special thanks to u4ick for the original bgmake tool and reverse engineering work that makes this integration possible.*

