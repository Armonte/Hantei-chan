# BGMAKE Integration Architecture

## System Overview Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                         HANTEI-CHAN                                │
│                   Character Frame Data Editor                      │
└────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ Integrates
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│                     BGMAKE INTEGRATION LAYER                       │
│              (Port of u4ick's bgmaketool to C++)                   │
└────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ Renders
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│                     MBAACC BACKGROUND SYSTEM                       │
│            (Parallax layers, animated sprites, camera)             │
└────────────────────────────────────────────────────────────────────┘
```

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              HANTEI-CHAN UI                             │
├──────────────────────┬──────────────────────┬───────────────────────────┤
│   Main Editor View   │  Background Editor   │      Status Bar           │
│   (Character Edit)   │      (Stage Edit)    │   [✓] Enable Stage        │
│                      │                      │      Cam: (120, 0)        │
│  ┌────────────────┐  │  ┌───────────────┐  │                           │
│  │                │  │  │ Object List   │  │                           │
│  │  Character +   │  │  │ ────────────  │  │                           │
│  │  Background    │  │  │ □ Sky         │  │                           │
│  │  Viewport      │  │  │ □ Mountains   │  │                           │
│  │                │  │  │ ☑ Trees       │  │                           │
│  │  [Stage Here]  │  │  │ □ Ground      │  │                           │
│  │                │  │  │               │  │                           │
│  └────────────────┘  │  └───────────────┘  │                           │
│                      │                      │                           │
└──────────────────────┴──────────────────────┴───────────────────────────┘
                                 │
                                 │ Uses
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        BACKGROUND SYSTEM CORE                           │
├─────────────────────┬──────────────────────┬────────────────────────────┤
│  BGMakeFile         │  BGRenderer          │  Camera                    │
│  ──────────         │  ──────────          │  ──────                    │
│  - objects[]        │  - Update()          │  - x, y, zoom              │
│  - cgData[]         │  - Render()          │  - ApplyParallax()         │
│  - Load/Save        │  - Sort by layer     │  - Pan(), SetZoom()        │
│                     │  - Batch render      │                            │
├─────────────────────┼──────────────────────┼────────────────────────────┤
│  BGMakeObject       │  CGLoader            │  BackgroundManager         │
│  ──────────────     │  ────────            │  ─────────────────         │
│  - name             │  - ParseCG()         │  - ScanMBAAPath()          │
│  - parallax         │  - Decompress()      │  - availableStages[]       │
│  - layer            │  - GenerateTexture() │  - LoadStage()             │
│  - frames[]         │                      │  - currentStage            │
│  - Update()         │                      │                            │
├─────────────────────┴──────────────────────┴────────────────────────────┤
│  BGMakeFrame                                                            │
│  ─────────────                                                          │
│  - spriteId, xOffset, yOffset, duration                                 │
│  - drawType (normal/additive), opacity, animType (loop/jump)            │
│  - xVec, yVec (movement vectors)                                        │
└─────────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ Reads
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           MBAACC DATA FILES                             │
├─────────────────────────────────────────────────────────────────────────┤
│  stage00.dat, stage01.dat, ... (Background files)                       │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ Header (64 bytes)                                                  │ │
│  │ - Magic: "bgmake" (6 bytes) + 10 null bytes                       │ │
│  │ - CG offset/length                                                 │ │
│  ├───────────────────────────────────────────────────────────────────┤ │
│  │ Offset Table (1024 bytes)                                          │ │
│  │ - 256 int32 offsets to objects                                     │ │
│  ├───────────────────────────────────────────────────────────────────┤ │
│  │ Object Data (variable size)                                        │ │
│  │ - Object headers (60 bytes each)                                   │ │
│  │ - Frame data (132 bytes per frame)                                 │ │
│  ├───────────────────────────────────────────────────────────────────┤ │
│  │ Embedded CG Data (variable size)                                   │ │
│  │ - Compressed sprite data                                           │ │
│  │ - Palette data                                                     │ │
│  └───────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

## Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          FRAME RENDER CYCLE                             │
│                             (60 FPS)                                    │
└─────────────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │  Update Animation      │
                    │  State for All Objects │
                    └────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │  Sort Objects by       │
                    │  Layer Value           │
                    └────────────────────────┘
                                 │
                                 ▼
        ┌───────────────────────────────────────────────┐
        │         FOR EACH OBJECT (sorted)              │
        ├───────────────────────────────────────────────┤
        │  1. Get current frame                         │
        │  2. Calculate parallax offset:                │
        │     screenX = worldX - (camera.x * factor)    │
        │     screenY = worldY - (camera.y * factor)    │
        │  3. Set blend mode (normal/additive)          │
        │  4. Set opacity                               │
        │  5. Render sprite at (screenX, screenY)       │
        └───────────────────────────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │  Advance Frame Timers  │
                    │  Handle Loop/Jump      │
                    └────────────────────────┘
```

## Data Flow: Loading a Stage

```
User Action: Menu → Stage → Load Stage File
              │
              ▼
    ┌─────────────────────┐
    │  File Dialog Opens  │
    └─────────────────────┘
              │
              ▼ User selects stg00.dat
    ┌──────────────────────────────────┐
    │  BGMakeFile::LoadFromFile()      │
    ├──────────────────────────────────┤
    │  1. Validate magic header        │
    │  2. Read offset table            │
    │  3. Parse each object:           │
    │     - Read object header         │
    │     - Read all frames            │
    │  4. Extract embedded CG data     │
    └──────────────────────────────────┘
              │
              ▼
    ┌──────────────────────────────────┐
    │  CGLoader::LoadFromMemory()      │
    ├──────────────────────────────────┤
    │  1. Parse CG file header         │
    │  2. For each sprite:             │
    │     - Decompress pixel data      │
    │     - Apply palette (if indexed) │
    │     - Convert to RGBA            │
    │  3. Generate OpenGL textures     │
    └──────────────────────────────────┘
              │
              ▼
    ┌──────────────────────────────────┐
    │  Render::SetBackground()         │
    │  Render::EnableBackground(true)  │
    └──────────────────────────────────┘
              │
              ▼
    ┌──────────────────────────────────┐
    │  Stage now visible in viewport   │
    └──────────────────────────────────┘
```

## Parallax System Explained

```
                    Camera Position: x = 100
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Far Background (Parallax = 128, factor = 0.5x)                  │
│  Moves half as fast as camera = slower = appears distant         │
│  Screen offset = 100 * 0.5 = 50 pixels                           │
│                                                                   │
│  [   Mountains   ]                                                │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  Mid Background (Parallax = 256, factor = 1.0x)                  │
│  Moves same speed as camera = normal                             │
│  Screen offset = 100 * 1.0 = 100 pixels                          │
│                                                                   │
│      [    Trees    ]                                              │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  Character Layer (Fixed to camera)                               │
│  Always centered in viewport                                     │
│                                                                   │
│         [Character]                                               │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  Foreground (Parallax = 384, factor = 1.5x)                      │
│  Moves faster than camera = closer appearance                    │
│  Screen offset = 100 * 1.5 = 150 pixels                          │
│                                                                   │
│            [ Fence ]                                              │
└──────────────────────────────────────────────────────────────────┘

When camera moves RIGHT (+100):
- Mountains move LEFT by 50 (appears far)
- Trees move LEFT by 100 (normal depth)  
- Character stays centered
- Fence moves LEFT by 150 (appears close)

This creates the illusion of 3D depth in a 2D scene!
```

## Animation System Flow

```
Object with 4 frames, each 60 frames duration:

Frame 0: Duration=60  ┌─────────────────────────────────────────┐
                      │ ████████████████████████████████████████│ (60 frames)
                      └─────────────────────────────────────────┘
                                    │ Timer hits 60
                                    ▼
Frame 1: Duration=60  ┌─────────────────────────────────────────┐
                      │ ████████████████████████████████████████│ (60 frames)
                      └─────────────────────────────────────────┘
                                    │ Timer hits 60
                                    ▼
Frame 2: Duration=30  ┌──────────────────────┐
                      │ ████████████████████ │ (30 frames)
                      └──────────────────────┘
                                    │ Timer hits 30
                                    ▼
Frame 3: AnimType=Jump, JumpFrame=1
                      │ Immediately jumps back to Frame 1
                      │ (Creates a 1→2→3→1→2→3 loop)
                      └──────────────────────────────────

Loop Animation (AnimType=1):
    0 → 1 → 2 → 3 → 0 → 1 → 2 → 3 → ... (continuous)

Jump Animation (AnimType=2):
    0 → 1 → 2 → 1 → 2 → 1 → 2 → ... (conditional branching)
```

## Layer Sorting Example

```
Before Sorting:            After Sorting (render order):
─────────────────          ──────────────────────────

Object A (Layer=200)       Object C (Layer=50)  ← Renders FIRST (back)
Object B (Layer=150)       Object B (Layer=150)
Object C (Layer=50)        Object A (Layer=200) ← Renders LAST (front)

Result: C appears behind B, which appears behind A
```

## Integration Points with Hantei-chan

```
┌─────────────────────────────────────────────────────────────────────┐
│                        HANTEI-CHAN EXISTING                         │
├──────────────────────┬────────────────────────┬─────────────────────┤
│  main_frame.cpp      │  render.cpp            │  character_view.cpp │
│  ───────────────     │  ───────────           │  ──────────────     │
│                      │                        │                     │
│  + DrawMenuBar()     │  + Draw()              │  + getState()       │
│    └→ Add "Stage"    │    └→ Call             │    └→ Track         │
│       menu           │       DrawBackground() │       character pos │
│                      │                        │                     │
│  + DrawStatusBar()   │  + DrawBackground()    │                     │
│    └→ Add checkbox   │    └→ NEW METHOD       │                     │
│       toggle         │       Render BG        │                     │
│                      │                        │                     │
│  + LoadStageFile()   │  + bgCamera            │                     │
│    └→ NEW METHOD     │    └→ NEW MEMBER       │                     │
│       Load .dat      │       Camera state     │                     │
│                      │                        │                     │
└──────────────────────┴────────────────────────┴─────────────────────┘
                                 │
                                 │ Links to
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     NEW BACKGROUND SUBSYSTEM                        │
│                    (src/background/ directory)                      │
└─────────────────────────────────────────────────────────────────────┘
```

## File Organization

```
Hantei-chan/
├── src/
│   ├── main.cpp                      (existing)
│   ├── main_frame.cpp                (modify: add stage menu)
│   ├── render.cpp                    (modify: add DrawBackground())
│   ├── render.h                      (modify: add bg members)
│   │
│   └── background/                   *** NEW DIRECTORY ***
│       ├── bgmake_types.h           (data structures)
│       ├── bgmake_file.cpp          (file I/O)
│       ├── bgmake_object.cpp        (animation logic)
│       ├── bg_renderer.cpp          (OpenGL rendering)
│       ├── bg_manager.cpp           (stage discovery)
│       ├── camera_sim.cpp           (parallax camera)
│       ├── cg_loader.cpp            (sprite decompression)
│       └── background_editor.cpp    (editor UI)
│
├── CMakeLists.txt                    (modify: add background/ sources)
├── BGMAKE_INTEGRATION_PLAN.md       (this plan)
├── BGMAKE_QUICKSTART.md             (implementation guide)
└── BGMAKE_ARCHITECTURE.md           (this document)
```

## CMake Integration

```cmake
# Add to CMakeLists.txt

# Background system sources
set(BACKGROUND_SOURCES
    src/background/bgmake_file.cpp
    src/background/bgmake_object.cpp
    src/background/bg_renderer.cpp
    src/background/bg_manager.cpp
    src/background/camera_sim.cpp
    src/background/cg_loader.cpp
)

# Optional: Background editor (can be separate feature flag)
if(ENABLE_BG_EDITOR)
    list(APPEND BACKGROUND_SOURCES
        src/background/background_editor.cpp
    )
endif()

# Add to main target
target_sources(${PROJECT_NAME} PRIVATE
    ${BACKGROUND_SOURCES}
)

# Include directory
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

## Memory Management Strategy

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MEMORY OWNERSHIP                             │
├─────────────────────────────────────────────────────────────────────┤
│  MainFrame                                                          │
│  ├── unique_ptr<BGManager> bgManager    (owns stage files)         │
│  │   └── unique_ptr<BGMakeFile> currentStage                       │
│  │       ├── vector<BGMakeObject> objects                          │
│  │       └── vector<uint8_t> cgData                                │
│  │                                                                  │
│  ├── Render* render                                                 │
│  │   ├── BGMakeFile* backgroundFile     (non-owning pointer)       │
│  │   ├── BGRenderer bgRenderer                                     │
│  │   │   └── vector<GLuint> textures    (owns GL textures)         │
│  │   └── Camera bgCamera                                           │
│  │                                                                  │
│  └── BackgroundEditorView* editorView                              │
│      └── BGMakeFile* currentFile        (non-owning pointer)       │
└─────────────────────────────────────────────────────────────────────┘

Lifetime Rules:
1. BGManager owns the loaded stage data
2. Render receives non-owning pointer to display
3. Editor receives non-owning pointer to edit
4. Textures are owned by BGRenderer, regenerated on CG load
5. When stage changes, old textures are deleted, new ones created
```

## Thread Safety Considerations

```
Current Design: Single-threaded (Simple and Safe)

┌─────────────────────────────────────┐
│  Main Thread (UI + Render)          │
│  ────────────────────────            │
│  1. Update UI (ImGui)                │
│  2. Update animations (60fps)        │
│  3. Render frame                     │
│  4. Swap buffers                     │
└─────────────────────────────────────┘

Future Enhancement: Async Loading

┌─────────────────────────────────────┐
│  Main Thread                        │
│  ────────────                       │
│  - Render current stage             │
│  - Update UI                        │
└─────────────────────────────────────┘
                │
                │ Triggers load
                ▼
┌─────────────────────────────────────┐
│  Worker Thread                      │
│  ─────────────                      │
│  1. Load .dat file                  │
│  2. Parse objects                   │
│  3. Decompress CG data              │
│  4. Signal completion               │
└─────────────────────────────────────┘
                │
                │ On complete
                ▼
┌─────────────────────────────────────┐
│  Main Thread (next frame)           │
│  ─────────────────────              │
│  - Generate GL textures (GL context)│
│  - Swap to new stage                │
└─────────────────────────────────────┘
```

## Performance Targets

```
┌────────────────────────────────────────────────────────────┐
│  Performance Goals                                         │
├────────────────────────────────────────────────────────────┤
│  Frame Rate:        Maintain 60 FPS with BG enabled        │
│  Stage Load Time:   < 500ms for typical stage              │
│  Memory Usage:      < 100MB additional for loaded stage    │
│  Texture Memory:    Efficient sharing, lazy loading        │
│  Animation Update:  < 1ms per frame for all objects        │
│  Rendering:         < 5ms per frame for all layers         │
└────────────────────────────────────────────────────────────┘

Optimization Strategies:
1. Texture Atlasing - combine sprites into single texture
2. Batch Rendering - group by blend mode and texture
3. Frustum Culling - don't render off-screen objects
4. Lazy Loading - only load textures for visible sprites
5. Object Pooling - reuse animation state objects
```

## Testing Strategy

```
┌─────────────────────────────────────────────────────────────────────┐
│                          TESTING PYRAMID                            │
├─────────────────────────────────────────────────────────────────────┤
│  Integration Tests (E2E)                                            │
│  ────────────────────────                                           │
│  - Load real MBAACC stage files                                     │
│  - Verify rendering matches in-game appearance                      │
│  - Test with character animation + background                       │
│  - Editor save/load round-trip                                      │
└─────────────────────────────────────────────────────────────────────┘
                                 ▲
┌─────────────────────────────────────────────────────────────────────┐
│  Component Tests                                                    │
│  ───────────────                                                    │
│  - File parsing (valid, invalid, corrupted)                         │
│  - Animation state machine (loop, jump)                             │
│  - Parallax calculations                                            │
│  - Layer sorting                                                    │
│  - CG decompression (various formats)                               │
└─────────────────────────────────────────────────────────────────────┘
                                 ▲
┌─────────────────────────────────────────────────────────────────────┐
│  Unit Tests                                                         │
│  ──────────                                                         │
│  - BGMakeFrame data structure                                       │
│  - Camera::ApplyParallax() math                                     │
│  - Object::Update() logic                                           │
│  - Layer comparison function                                        │
│  - File header validation                                           │
└─────────────────────────────────────────────────────────────────────┘
```

## Conclusion

This architecture integrates the bgmake functionality into Hantei-chan as a modular subsystem. The design:

- **Maintains separation of concerns**: Background system is independent
- **Minimal changes to existing code**: Only render pipeline and menu additions
- **Extensible**: Easy to add features like advanced camera effects
- **Performant**: Efficient rendering with batching and culling
- **Testable**: Clear interfaces allow comprehensive testing

The integration brings MBAACC stage editing to Hantei-chan while preserving the tool's primary focus on character frame data editing.

---

*Architecture designed for integration of u4ick's bgmake tool with Hantei-chan character editor*

