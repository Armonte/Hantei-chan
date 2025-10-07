# Background System - READY TO INTEGRATE! ✓

## What We Just Built

The MBAACC background system is now **fully implemented and compiled** into Hantei-chan! 🚀

### Core Components Created

✅ **`src/background/bg_types.h`** - Data structures (Frame, Object, Camera)
✅ **`src/background/bg_file.h/cpp`** - .dat file loader (ha4-like format)
✅ **`src/background/bg_renderer.h/cpp`** - OpenGL rendering with parallax
✅ **`src/render.h/cpp`** - Integration hooks added
✅ **`CMakeLists.txt`** - Build system updated

### Key Features

- **File Loading**: Loads MBAA `.dat` stage files (bgmake format)
- **Embedded CG**: Uses your existing CG loader (no cglib needed!)
- **Modern OpenGL**: Uses same VAO/shader system as main app
- **Parallax Camera**: Full camera simulation with parallax layers
- **Animation**: Loop and jump animation types, 60fps timing
- **Texture Caching**: Smart caching prevents reloading sprites

## Next Steps: Wire It Into MainFrame

### Step 1: Add Members to MainFrame

In `src/main_frame.h`:

```cpp
#include "background/bg_file.h"
#include "background/bg_renderer.h"
#include "background/bg_types.h"

class MainFrame {
    // ... existing members ...
    
    // Background system
    bg::File* currentBgFile = nullptr;
    bg::Renderer bgRenderer;
    bg::Camera bgCamera;
};
```

### Step 2: Initialize in Constructor

In `src/main_frame.cpp` constructor:

```cpp
MainFrame::MainFrame(ContextGl* context) {
    // ... existing code ...
    
    // Initialize background system
    render->SetBackgroundRenderer(&bgRenderer, &bgCamera);
}
```

### Step 3: Add Menu to Load Stages

```cpp
// In DrawMenuBar() or similar
if (ImGui::BeginMenu("Stage")) {
    if (ImGui::MenuItem("Load Stage...")) {
        // Use your existing file dialog
        std::wstring path = OpenFileDialog(
            mainWindowHandle,
            L"MBAA Stage Files (*.dat)\0*.dat\0",
            nullptr
        );
        
        if (!path.empty()) {
            // Convert to char*
            char pathMb[512];
            wcstombs(pathMb, path.c_str(), sizeof(pathMb));
            
            // Clear old stage
            if (currentBgFile) {
                delete currentBgFile;
            }
            
            // Load new stage
            currentBgFile = new bg::File();
            if (currentBgFile->Load(pathMb)) {
                bgRenderer.SetFile(currentBgFile);
                bgRenderer.SetEnabled(true);
            } else {
                delete currentBgFile;
                currentBgFile = nullptr;
            }
        }
    }
    
    bool enabled = bgRenderer.IsEnabled();
    if (ImGui::Checkbox("Enable Background", &enabled)) {
        bgRenderer.SetEnabled(enabled);
    }
    
    ImGui::EndMenu();
}
```

### Step 4: Update Animations

In your main loop (probably `MainFrame::Draw()`):

```cpp
void MainFrame::Draw() {
    // Update background animations
    bgRenderer.Update();
    
    // ... existing draw code ...
}
```

### Step 5: Optional - Camera Control

To pan background with character view:

```cpp
void MainFrame::HandleMouseDrag(int dx, int dy, bool rightDrag, bool leftDrag) {
    if (leftDrag) {
        // Existing code
        render->x += dx;
        render->y += dy;
        
        // Also pan background
        if (bgRenderer.IsEnabled()) {
            bgCamera.Pan(-dx / render->scale, -dy / render->scale);
        }
    }
}
```

## Testing

1. **Build (already done!)**: `cmake --build build`
2. **Run**: `./build/gonptechan.exe`
3. **Load a stage**: Menu → Stage → Load Stage
4. **Find stage files**: Look in your MBAA installation:
   - `C:/Games/MBAACC/data/stg00.dat`
   - `C:/Games/MBAACC/data/stg01.dat`
   - etc.

## File Format Info

### What's in a .dat file

```
Header (64 bytes)
├── Magic: "bgmake" (6 bytes) + 10 null bytes
├── CG offset/length (embedded sprite data)
└── Reserved

Offset Table (1024 bytes)
└── 256 int32 entries pointing to objects

Object Data (per object)
├── Object header (60 bytes)
│   ├── num_frames, parallax, layer
│   └── reserved
└── Frame data (132 bytes each)
    ├── spriteId, offsetX, offsetY, duration
    ├── blendMode, opacity, aniType
    └── movement vectors

Embedded CG File (end of file)
└── Complete .cg file with all sprites
```

### Frame Animation

- **aniType = 0**: End (stop)
- **aniType = 1**: Loop (continuous)
- **aniType = 2**: Jump (go to jumpFrame)

### Parallax Values

- `64` = 0.25x speed (far background, slow)
- `128` = 0.5x speed (mid background)
- `256` = 1.0x speed (normal, moves with camera)
- `384` = 1.5x speed (foreground)
- `512` = 2.0x speed (close foreground, fast)

### Layer Values

- Lower values = render behind
- Higher values = render in front
- Default: 128

## Architecture

```
MainFrame
├── bgRenderer (renders backgrounds)
│   ├── File (loaded .dat)
│   │   ├── objects[] (animated layers)
│   │   └── cg (sprites via existing CG system)
│   └── textureCache (OpenGL textures)
└── bgCamera (parallax calculations)

Render::Draw()
├── 1. DrawBackground() ← NEW! Renders bg first
├── 2. DrawGridLines() (existing)
├── 3. DrawSpriteOnly() (existing, character)
└── 4. DrawBoxes() (existing, hitboxes)
```

## Performance Notes

- **Texture Cache**: Sprites loaded once, cached as GL textures
- **Layer Sorting**: Objects sorted by layer value each frame
- **Modern OpenGL**: Uses VAO/VBO, no legacy immediate mode
- **60 FPS**: Animation updates synchronized with game timing

## Common MBAA Stages to Test

- `stg00.dat` - Training stage (simple, good first test)
- `stg01.dat` - School rooftop (has parallax)
- `stg02.dat` - Park (animated trees/water)
- `stg03.dat` - Night street (complex layers)
- `stg04.dat` - Alleyway (particles)

## Troubleshooting

### Background not visible
1. Check `bgRenderer.IsEnabled()` is true
2. Verify `.dat` file loaded (check console output)
3. Make sure `DrawBackground()` is being called

### No sprites/black boxes
- CG might not have loaded (check temp file permissions)
- Sprite IDs might be invalid (check console for errors)

### Animation not working
- Call `bgRenderer.Update()` every frame
- Check frame durations aren't all 0

### Performance issues
- Use texture cache (already implemented)
- Consider culling off-screen objects (future enhancement)

## Next Enhancements (Future)

Once basic rendering works:

1. **Auto-Discovery**: Scan MBAA folder for all stages
2. **Stage Picker UI**: Dropdown with thumbnails
3. **Background Editor**: Timeline, object list, property editor
4. **Export/Import**: Save custom stages
5. **Palette Swapping**: Stage color variants
6. **Camera Effects**: Zoom, shake, smooth following

## Credits

- **u4ick**: Original bgmake tool and reverse engineering
- **Hantei-chan team**: Existing CG loader that makes this possible
- **MBAACC community**: Years of modding knowledge

---

**Status**: ✅ COMPILED AND READY
**Next**: Wire into MainFrame (5-10 minutes of work)
**Result**: Animated MBAACC stages behind your characters!

See `src/background/INTEGRATION_GUIDE.md` for detailed step-by-step instructions.

