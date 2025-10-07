# BGMAKE Integration Quick-Start Guide

## Getting Started Immediately

This guide provides copy-paste ready code to start integrating bgmake functionality into Hantei-chan. Start with the minimal viable integration, then expand.

## Phase 0: Minimal Viable Integration (1-2 days)

### Step 1: Create Basic Header Files

Create `src/background/bgmake_types.h`:

```cpp
#ifndef BGMAKE_TYPES_H_GUARD
#define BGMAKE_TYPES_H_GUARD

#include <cstdint>
#include <vector>
#include <string>

namespace bgmake {

// Animation types
enum class AnimType : uint8_t {
    Loop = 1,
    Jump = 2
};

// Draw/blend modes
enum class DrawType : uint8_t {
    Normal = 0,
    Additive = 2
};

// Frame data - exactly matches MBAACC file format
struct Frame {
    int16_t spriteId;        // Sprite ID (subtract 10000 from file)
    int16_t xOffset;
    int16_t yOffset;
    int16_t duration;        // Duration in frames @ 60fps
    uint8_t unk;
    DrawType drawType;
    uint8_t opacity;         // 0-255
    AnimType animType;
    uint8_t jumpFrame;       // Target frame for Jump animation
    uint8_t enableXVec;
    uint8_t enableYVec;
    int16_t xVec;            // Movement per frame
    int16_t yVec;
    
    // Runtime computed position
    float runtimeX = 0.0f;
    float runtimeY = 0.0f;
};

// Background object - collection of animated frames
struct Object {
    std::string name;
    int32_t parallax = 256;  // 256 = 1.0x camera speed
    int32_t layer = 128;     // Higher = render in front
    std::vector<Frame> frames;
    
    // Animation state
    int32_t currentFrame = 0;
    int32_t frameDuration = 0;
    
    void Update();
    void Reset();
};

// Complete background file
struct File {
    std::vector<Object> objects;
    std::vector<uint8_t> cgData;  // Embedded CG sprite data
    
    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path);
};

} // namespace bgmake

#endif /* BGMAKE_TYPES_H_GUARD */
```

### Step 2: Implement File Loader

Create `src/background/bgmake_loader.cpp`:

```cpp
#include "bgmake_types.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace bgmake {

bool File::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << path << std::endl;
        return false;
    }
    
    // Read header (64 bytes)
    char magic[16];
    file.read(magic, 16);
    if (strncmp(magic, "bgmake", 6) != 0) {
        std::cerr << "Invalid magic header" << std::endl;
        return false;
    }
    
    int32_t unk, pat_off, pat_len, cg_off, cg_len;
    file.read((char*)&unk, 4);
    file.read((char*)&pat_off, 4);
    file.read((char*)&pat_len, 4);
    file.read((char*)&cg_off, 4);
    file.read((char*)&cg_len, 4);
    
    file.seekg(48, std::ios::cur); // Skip reserved
    
    // Read offset table (256 entries)
    int32_t offsets[256];
    file.read((char*)offsets, 256 * 4);
    
    // Read objects
    objects.clear();
    for (int i = 0; i < 256; i++) {
        if (offsets[i] == -1) continue;
        
        file.seekg(offsets[i]);
        
        Object obj;
        obj.name = "obj_" + std::to_string(i);
        
        // Read object header (60 bytes)
        int32_t numFrames;
        file.read((char*)&numFrames, 4);
        file.read((char*)&obj.parallax, 4);
        file.read((char*)&obj.layer, 4);
        file.seekg(8, std::ios::cur);  // Skip reserved
        file.seekg(40, std::ios::cur); // Skip padding
        
        // Read frames
        for (int f = 0; f < numFrames; f++) {
            Frame frame;
            
            file.read((char*)&frame.spriteId, 2);
            frame.spriteId -= 10000;  // Adjust offset
            
            file.read((char*)&frame.xOffset, 2);
            file.read((char*)&frame.yOffset, 2);
            file.read((char*)&frame.duration, 2);
            file.read((char*)&frame.unk, 1);
            file.read((char*)&frame.drawType, 1);
            file.read((char*)&frame.opacity, 1);
            file.read((char*)&frame.animType, 1);
            file.read((char*)&frame.jumpFrame, 1);
            
            file.seekg(32, std::ios::cur); // Skip reserved
            
            file.read((char*)&frame.enableXVec, 1);
            file.read((char*)&frame.enableYVec, 1);
            file.seekg(4, std::ios::cur);
            file.read((char*)&frame.xVec, 2);
            file.read((char*)&frame.yVec, 2);
            
            file.seekg(77, std::ios::cur); // Skip remaining reserved
            
            obj.frames.push_back(frame);
        }
        
        objects.push_back(obj);
    }
    
    // Read embedded CG data
    if (cg_len > 0) {
        file.seekg(cg_off);
        cgData.resize(cg_len);
        file.read((char*)cgData.data(), cg_len);
    }
    
    std::cout << "Loaded " << objects.size() << " objects, "
              << cgData.size() << " bytes of CG data" << std::endl;
    
    return true;
}

void Object::Update() {
    if (frames.empty()) return;
    
    frameDuration++;
    
    Frame& frame = frames[currentFrame];
    if (frameDuration >= frame.duration) {
        frameDuration = 0;
        
        switch (frame.animType) {
            case AnimType::Loop:
                currentFrame++;
                if (currentFrame >= (int)frames.size()) {
                    currentFrame = 0;
                }
                break;
                
            case AnimType::Jump:
                currentFrame = frame.jumpFrame;
                if (currentFrame >= (int)frames.size()) {
                    currentFrame = 0;
                }
                break;
        }
    }
    
    // Apply movement vectors
    if (frame.enableXVec) {
        frame.runtimeX += frame.xVec;
    }
    if (frame.enableYVec) {
        frame.runtimeY += frame.yVec;
    }
}

void Object::Reset() {
    currentFrame = 0;
    frameDuration = 0;
    for (auto& frame : frames) {
        frame.runtimeX = 0.0f;
        frame.runtimeY = 0.0f;
    }
}

} // namespace bgmake
```

### Step 3: Add Camera Parallax

Create `src/background/bgmake_camera.h`:

```cpp
#ifndef BGMAKE_CAMERA_H_GUARD
#define BGMAKE_CAMERA_H_GUARD

namespace bgmake {

class Camera {
public:
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    
    // Apply parallax effect to world position
    // parallaxValue: 256 = 1.0x (moves with camera)
    //                128 = 0.5x (background, moves slower)
    //                512 = 2.0x (foreground, moves faster)
    float ApplyParallaxX(float worldX, int parallaxValue) const {
        float factor = parallaxValue / 256.0f;
        return worldX - (x * factor);
    }
    
    float ApplyParallaxY(float worldY, int parallaxValue) const {
        float factor = parallaxValue / 256.0f;
        return worldY - (y * factor);
    }
    
    // Manual camera control
    void Pan(float dx, float dy) {
        x += dx;
        y += dy;
    }
    
    void SetZoom(float z) {
        zoom = z;
    }
};

} // namespace bgmake

#endif /* BGMAKE_CAMERA_H_GUARD */
```

### Step 4: Quick Test Program

Create `test/test_bgmake.cpp`:

```cpp
#include "../src/background/bgmake_types.h"
#include "../src/background/bgmake_camera.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: test_bgmake <path_to_stage.dat>" << std::endl;
        return 1;
    }
    
    bgmake::File bgFile;
    if (!bgFile.LoadFromFile(argv[1])) {
        std::cerr << "Failed to load file" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Background File Info ===" << std::endl;
    std::cout << "Objects: " << bgFile.objects.size() << std::endl;
    std::cout << "CG Data: " << bgFile.cgData.size() << " bytes" << std::endl;
    
    for (size_t i = 0; i < bgFile.objects.size(); i++) {
        const auto& obj = bgFile.objects[i];
        std::cout << "\n--- " << obj.name << " ---" << std::endl;
        std::cout << "  Frames: " << obj.frames.size() << std::endl;
        std::cout << "  Parallax: " << obj.parallax << std::endl;
        std::cout << "  Layer: " << obj.layer << std::endl;
        
        if (!obj.frames.empty()) {
            const auto& frame = obj.frames[0];
            std::cout << "  First frame: sprite=" << frame.spriteId
                      << " pos=(" << frame.xOffset << "," << frame.yOffset << ")"
                      << " dur=" << frame.duration << std::endl;
        }
    }
    
    // Test camera parallax
    bgmake::Camera cam;
    cam.x = 100.0f;
    
    std::cout << "\n=== Parallax Test (camera.x = 100) ===" << std::endl;
    for (int pv : {64, 128, 256, 384, 512}) {
        float result = cam.ApplyParallaxX(0.0f, pv);
        std::cout << "  Parallax " << pv << ": offset = " << result << std::endl;
    }
    
    return 0;
}
```

Compile and test:
```bash
g++ -std=c++17 test/test_bgmake.cpp src/background/bgmake_loader.cpp -o test_bgmake
./test_bgmake /path/to/mbaa/data/stage00.dat
```

## Phase 1: Integrate with Hantei-chan Viewport

### Step 5: Add to Render System

Modify `src/render.h`:

```cpp
// Add includes at top
#include "background/bgmake_types.h"
#include "background/bgmake_camera.h"

// Add to Render class
class Render {
    // ... existing members ...
    
private:
    // Background rendering
    bgmake::File* backgroundFile = nullptr;
    bgmake::Camera bgCamera;
    bool backgroundEnabled = false;
    GLuint dummyTexture = 0;  // Temporary until CG loading works
    
public:
    // Background control
    void SetBackground(bgmake::File* file) { 
        backgroundFile = file; 
    }
    
    void EnableBackground(bool enable) { 
        backgroundEnabled = enable; 
    }
    
    bool IsBackgroundEnabled() const { 
        return backgroundEnabled; 
    }
    
    bgmake::Camera& GetBGCamera() { 
        return bgCamera; 
    }
    
    void DrawBackground();  // New method
};
```

Modify `src/render.cpp`, add to `Draw()` method:

```cpp
void Render::Draw() {
    // STEP 1: Draw background FIRST (behind character)
    if (backgroundEnabled && backgroundFile) {
        DrawBackground();
    }
    
    // STEP 2: Draw character sprite (existing code)
    DrawSpriteOnly();
    
    // STEP 3: Draw hitboxes and UI (existing code)
    // ... existing code ...
}

void Render::DrawBackground() {
    if (!backgroundFile) return;
    
    // Update animations (call once per frame)
    static int frameCounter = 0;
    if (frameCounter % 1 == 0) {  // 60fps
        for (auto& obj : backgroundFile->objects) {
            obj.Update();
        }
    }
    frameCounter++;
    
    // Sort objects by layer (lower layers render first)
    std::vector<bgmake::Object*> sortedObjs;
    for (auto& obj : backgroundFile->objects) {
        sortedObjs.push_back(&obj);
    }
    std::sort(sortedObjs.begin(), sortedObjs.end(),
        [](const bgmake::Object* a, const bgmake::Object* b) {
            return a->layer < b->layer;
        });
    
    // Render each object
    for (auto* obj : sortedObjs) {
        if (obj->frames.empty()) continue;
        
        const bgmake::Frame& frame = obj->frames[obj->currentFrame];
        
        // Calculate screen position with parallax
        float screenX = bgCamera.ApplyParallaxX(
            frame.xOffset + frame.runtimeX, 
            obj->parallax
        );
        float screenY = bgCamera.ApplyParallaxY(
            frame.yOffset + frame.runtimeY, 
            obj->parallax
        );
        
        // Set blend mode
        if (frame.drawType == bgmake::DrawType::Additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        // TODO: Render sprite at (screenX, screenY)
        // For now, draw a colored rectangle as placeholder
        float alpha = frame.opacity / 255.0f;
        
        // Use simple shader to draw placeholder
        sSimple.Use();
        glUniformMatrix4fv(lProjectionS, 1, GL_FALSE, &projection[0][0]);
        
        // Set color based on layer (temporary visualization)
        float hue = (obj->layer % 256) / 256.0f;
        glUniform4f(lAlphaS, hue, 0.5f, 1.0f - hue, alpha);
        
        // Draw a small square at the position
        // (Replace with actual sprite rendering later)
        std::vector<float> quad = {
            screenX, screenY,
            screenX + 32, screenY,
            screenX + 32, screenY + 32,
            screenX, screenY,
            screenX + 32, screenY + 32,
            screenX, screenY + 32
        };
        
        vGeometry.Bind();
        vGeometry.BufferClientData(quad.data(), quad.size() * sizeof(float));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    // Reset blend mode
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
```

### Step 6: Add UI Toggle

In `src/main_frame.cpp` or wherever you draw status bar:

```cpp
void MainFrame::DrawStatusBar() {
    ImGui::Begin("Status");
    
    // ... existing status items ...
    
    ImGui::Separator();
    
    bool bgEnabled = render->IsBackgroundEnabled();
    if (ImGui::Checkbox("Enable Stage & Camera", &bgEnabled)) {
        render->EnableBackground(bgEnabled);
    }
    
    if (bgEnabled) {
        ImGui::SameLine();
        auto& cam = render->GetBGCamera();
        ImGui::Text("| Cam: (%.0f, %.0f)", cam.x, cam.y);
    }
    
    ImGui::End();
}
```

### Step 7: Add Menu to Load Stage

In `src/main_frame.cpp`:

```cpp
void MainFrame::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // ... existing menus ...
        
        if (ImGui::BeginMenu("Stage")) {
            if (ImGui::MenuItem("Load Stage File...")) {
                // Use your existing file dialog system
                std::string path = /* OpenFileDialog for .dat */;
                if (!path.empty()) {
                    LoadStageFile(path);
                }
            }
            
            if (ImGui::MenuItem("Clear Stage")) {
                render->SetBackground(nullptr);
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void MainFrame::LoadStageFile(const std::string& path) {
    // Create persistent storage for loaded stage
    static bgmake::File currentStage;
    
    if (currentStage.LoadFromFile(path)) {
        render->SetBackground(&currentStage);
        render->EnableBackground(true);
        std::cout << "Loaded stage: " << path << std::endl;
    } else {
        std::cerr << "Failed to load stage: " << path << std::endl;
    }
}
```

### Step 8: Camera Control

Add camera panning to your existing mouse drag handler:

```cpp
void MainFrame::HandleMouseDrag(int dx, int dy, bool rightDrag, bool leftDrag) {
    if (leftDrag) {
        // Existing character view panning
        // ... existing code ...
        
        // Also pan background camera if enabled
        if (render->IsBackgroundEnabled()) {
            auto& bgCam = render->GetBGCamera();
            bgCam.Pan(dx / render->scale, dy / render->scale);
        }
    }
    
    // ... rest of existing code ...
}
```

## Testing Your Integration

1. **Build Hantei-chan with new background files**
```bash
cd /mnt/c/games/qoh/Hantei-chan
cmake -B build
cmake --build build
```

2. **Get a test stage file**
   - From your MBAACC installation: `C:/Games/MBAACC/data/stg*.dat`
   - Or test with the stage files in your MBAA folder

3. **Run and test**
   - Launch Hantei-chan
   - Menu → Stage → Load Stage File
   - Select a `.dat` file
   - Check "Enable Stage & Camera" in status bar
   - You should see colored placeholder rectangles animating
   - Pan the view and watch parallax in action

## Next Steps

After the basic integration works:

1. **Implement CG Sprite Loading**
   - Parse CG file format
   - Decompress sprite data
   - Create OpenGL textures
   - Replace placeholder rectangles with actual sprites

2. **Add Background Editor Tab**
   - Create `src/background/background_editor.cpp`
   - Add tab to main window tab bar
   - Implement object list, properties panel, timeline

3. **Auto-Discovery**
   - Add settings for MBAA path
   - Scan for all stage files
   - Create dropdown menu with stage names

4. **Polish**
   - Better camera controls
   - Animation playback controls
   - Performance optimization

## Common Issues & Solutions

### Issue: Files won't load
**Solution**: Check that the file is actually an MBAACC .dat file with the correct magic header "bgmake_array"

### Issue: Nothing renders
**Solution**: Check that objects have valid frame data and the background is enabled in the UI

### Issue: Parallax looks wrong
**Solution**: Verify camera coordinates are being updated correctly and parallax values are reasonable (256 = normal speed)

### Issue: Animation too fast/slow
**Solution**: Make sure Update() is called exactly once per frame at 60fps

## Example Stage Files

Common MBAACC stage files to test with:
- `stg00.dat` - Basic outdoor stage
- `stg01.dat` - School rooftop
- `stg02.dat` - Park stage with parallax
- `stg03.dat` - Indoor stage

Look in your MBAACC installation under `/data/` directory.

---

This quick-start guide gets you from zero to a working background system in hours rather than weeks. Start here, test thoroughly, then expand to the full feature set outlined in the main integration plan.

