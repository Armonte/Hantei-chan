# CG Loading Strategy: Using Hantei-chan's Existing System

## TL;DR: Use What You Have! ✓

**Recommendation: Stick with Hantei-chan's existing CG loader.** No need to port cglib.

## Why Hantei-chan's CG System is Perfect

### 1. Already Handles MBAACC Format

Your existing `cg.cpp`/`cg.h` already supports:
- ✓ "BMP Cutter3" format (MBAACC standard)
- ✓ Palette loading and switching
- ✓ Multiple render types (type_id: 1, 2, 3, 4)
- ✓ 8bpp and 32bpp formats
- ✓ Tiled/cell-based decompression
- ✓ Proper alpha handling
- ✓ Custom palettes per-image

### 2. Same Format for Characters and Backgrounds

Both character `.cg` files and background embedded CG data use the **same format**:
- Same "BMP Cutter3" header
- Same compression scheme
- Same tile/cell structure
- Same palette system

u4ick used cglib in bgmake because it was convenient, not because the format is different.

### 3. Already Integrated with Your Rendering

Your `CG` class already:
- Generates `ImageData` with RGBA pixels
- Works with your OpenGL texture system
- Handles palette swapping (useful for stage variants!)
- Has proper memory management

### 4. No Additional Dependencies

Using your existing system means:
- No porting C# code to C++
- No external library dependencies
- No licensing concerns
- Already compiled and working

## Implementation Strategy

### Modify Background System to Use Existing CG Class

Instead of creating a new `CGLoader` class, just reuse your `CG` class:

```cpp
// bgmake_file.h
class BGMakeFile {
public:
    // ... existing members ...
    
    CG* cgFile = nullptr;  // Use existing CG class!
    
    bool LoadFromFile(const std::string& path);
};

// bgmake_file.cpp
bool BGMakeFile::LoadFromFile(const std::string& path) {
    // ... load objects as planned ...
    
    // Extract embedded CG data
    if (cg_len > 0) {
        file.seekg(cg_off);
        cgData.resize(cg_len);
        file.read((char*)cgData.data(), cg_len);
        
        // Save CG data to temporary file
        std::string tempCgPath = "temp_bg_stage.cg";
        std::ofstream cgOut(tempCgPath, std::ios::binary);
        cgOut.write((char*)cgData.data(), cgData.size());
        cgOut.close();
        
        // Load using existing CG system
        cgFile = new CG();
        if (!cgFile->load(tempCgPath.c_str())) {
            delete cgFile;
            cgFile = nullptr;
            // Not fatal - continue without sprites
        }
        
        // Clean up temp file
        std::remove(tempCgPath.c_str());
    }
    
    return true;
}
```

### Alternative: Load CG from Memory

Even better - modify `CG::load()` to accept memory buffer:

```cpp
// cg.h
class CG {
public:
    bool load(const char *name);
    bool loadFromMemory(const char* data, unsigned int size);  // NEW
    // ... rest ...
};

// cg.cpp
bool CG::loadFromMemory(const char* data, unsigned int size) {
    if (m_loaded) {
        free();
    }
    
    // Verify size and header
    if (size < 0x4f30 || memcmp(data, "BMP Cutter3", 11)) {
        return false;
    }
    
    // Allocate our own copy
    char* ourData = new char[size];
    memcpy(ourData, data, size);
    
    // Rest of parsing is identical to load()
    // (factor out common code into private method)
    
    // ... palette setup ...
    // ... header parsing ...
    // ... build_image_table() ...
    
    m_loaded = true;
    return true;
}
```

Then in bgmake_file.cpp:

```cpp
bool BGMakeFile::LoadFromFile(const std::string& path) {
    // ... load header and objects ...
    
    // Load CG directly from embedded data
    if (cg_len > 0) {
        file.seekg(cg_off);
        cgData.resize(cg_len);
        file.read((char*)cgData.data(), cg_len);
        
        cgFile = new CG();
        if (!cgFile->loadFromMemory((char*)cgData.data(), cgData.size())) {
            delete cgFile;
            cgFile = nullptr;
        }
    }
    
    return true;
}
```

### Rendering Background Sprites

In your `BGRenderer::RenderObject()`:

```cpp
void BGRenderer::RenderObject(const BGMakeObject& obj, const CameraState& camera) {
    if (!bgFile || !bgFile->cgFile) return;
    if (obj.frames.empty()) return;
    
    const BGMakeFrame& frame = obj.frames[obj.currentFrame];
    
    // Calculate screen position with parallax
    float screenX = camera.ApplyParallaxX(
        frame.xOffset + frame.runtimeX, 
        obj.parallax
    );
    float screenY = camera.ApplyParallaxY(
        frame.yOffset + frame.runtimeY, 
        obj.parallax
    );
    
    // Get sprite from existing CG system
    ImageData* sprite = bgFile->cgFile->draw_texture(
        frame.spriteId,  // Sprite ID from frame
        false,           // Don't need pow2 for modern GL
        false            // 32bpp RGBA
    );
    
    if (!sprite) return;
    
    // Create or update texture (cache by sprite ID)
    GLuint texId = GetOrCreateTexture(frame.spriteId, sprite);
    
    // Set blend mode
    if (frame.drawType == DrawType::Additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    // Set opacity
    float alpha = frame.opacity / 255.0f;
    
    // Render sprite quad at (screenX, screenY)
    RenderSprite(texId, screenX, screenY, 
                 sprite->width, sprite->height, 
                 alpha);
    
    delete sprite;  // ImageData cleans up pixel data
}
```

## Texture Caching Strategy

Don't regenerate textures every frame - cache them:

```cpp
class BGRenderer {
private:
    struct CachedTexture {
        GLuint textureId;
        int width;
        int height;
        int spriteId;
    };
    
    std::unordered_map<int, CachedTexture> textureCache;
    
    GLuint GetOrCreateTexture(int spriteId, ImageData* sprite);
    void ClearTextureCache();
};

GLuint BGRenderer::GetOrCreateTexture(int spriteId, ImageData* sprite) {
    // Check cache first
    auto it = textureCache.find(spriteId);
    if (it != textureCache.end()) {
        delete sprite;  // Don't need the data
        return it->second.textureId;
    }
    
    // Create new texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 sprite->width, sprite->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 sprite->pixels);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Cache it
    textureCache[spriteId] = {texId, sprite->width, sprite->height, spriteId};
    
    return texId;
}

void BGRenderer::ClearTextureCache() {
    for (auto& pair : textureCache) {
        glDeleteTextures(1, &pair.second.textureId);
    }
    textureCache.clear();
}
```

## Comparison: Hantei-chan CG vs cglib

| Feature | Hantei-chan CG | cglib (C#) |
|---------|----------------|------------|
| Format Support | ✓ BMP Cutter3 | ✓ BMP Cutter3 |
| Palette Support | ✓ Multiple palettes | ✓ Multiple palettes |
| Compression | ✓ Cell-based | ✓ Cell-based |
| Integration | ✓ Already in Hantei-chan | ✗ Need to port |
| Language | ✓ C++ (native) | ✗ C# (foreign) |
| Dependencies | ✓ None | ✗ .NET/Mono |
| Performance | ✓ Direct memory access | ✗ Interop overhead |
| Licensing | ✓ Same as Hantei-chan | ? Unknown |

## Testing Plan

1. **Verify Background CG Format**
   ```bash
   # Extract CG from a background .dat file
   # Compare header with character .cg file
   hexdump -C stage00.dat | head -20
   hexdump -C ciel.cg | head -20
   # Should both show "BMP Cutter3" header
   ```

2. **Test Sprite Loading**
   ```cpp
   // Load a stage file
   BGMakeFile stage;
   stage.LoadFromFile("stage00.dat");
   
   // Try to load first sprite
   if (stage.cgFile) {
       ImageData* sprite = stage.cgFile->draw_texture(0, false, false);
       if (sprite) {
           printf("Success! Sprite %dx%d\n", sprite->width, sprite->height);
           delete sprite;
       }
   }
   ```

3. **Verify Sprite IDs Match**
   - Background frames reference sprite IDs (minus 10000 offset)
   - Check that IDs are in valid range for the CG file
   - Test rendering a few different sprite IDs

## Advantages of This Approach

### 1. Simpler Integration
- No new CG parsing code
- No porting C# to C++
- Reuse battle-tested code

### 2. Consistency
- Same CG handling throughout Hantei-chan
- Same texture format
- Same memory management patterns

### 3. Maintainability
- One CG loader to maintain
- Bugs fixed in one place benefit both systems
- Easier for contributors to understand

### 4. Feature Parity
- Character palette swapping → Stage palette variants!
- Same sprite types work for both
- Same optimization opportunities

### 5. Performance
- No C#/C++ interop overhead
- Direct memory access
- Already optimized for your rendering pipeline

## Potential Issues and Solutions

### Issue: Embedded CG might be slightly different format
**Likelihood**: Low (same game, same tools)
**Solution**: Add version detection, fall back gracefully

### Issue: Sprite ID offset confusion
**Likelihood**: Medium (already handled with +10000 offset)
**Solution**: Clear documentation, helper functions:

```cpp
// Helper functions to make sprite ID handling clear
inline int FileToSpriteId(int16_t fileId) {
    return fileId - 10000;  // File stores as ID+10000
}

inline int16_t SpriteToFileId(int spriteId) {
    return spriteId + 10000;  // For saving
}
```

### Issue: Memory usage with multiple CG files
**Likelihood**: Low (stages are loaded one at a time)
**Solution**: Unload previous stage CG when loading new one

## Updated Integration Plan

Replace the "Phase 2: CG Sprite System" section with:

### Phase 2: CG Integration (1-2 days instead of 1-2 weeks!)

- [x] Hantei-chan already has CG loader ✓
- [ ] Add `CG::loadFromMemory()` method (or use temp file)
- [ ] Test loading embedded CG from .dat file
- [ ] Verify sprite IDs work correctly
- [ ] Add texture caching in BGRenderer
- [ ] Done!

## Conclusion

**Use Hantei-chan's existing CG system.** It already does everything you need:
- ✓ Loads MBAACC CG format
- ✓ Handles all compression types
- ✓ Integrated with your renderer
- ✓ Battle-tested on character files
- ✓ No external dependencies

The only modification needed is loading from memory instead of file (or just use a temp file initially). This saves **weeks** of work and reduces complexity significantly.

---

## Quick Implementation Checklist

```cpp
// 1. Extract embedded CG from .dat file ✓ (already in file loader)

// 2. Add memory loading to CG class
bool CG::loadFromMemory(const char* data, unsigned int size);

// 3. Use it in BGMakeFile
cgFile = new CG();
cgFile->loadFromMemory((char*)cgData.data(), cgData.size());

// 4. Render sprites in BGRenderer
ImageData* sprite = bgFile->cgFile->draw_texture(frame.spriteId, false, false);
GLuint tex = GetOrCreateTexture(frame.spriteId, sprite);
RenderSprite(tex, x, y, w, h, alpha);

// Done! Background sprites rendering.
```

This is the path of least resistance and maximum code reuse. Your existing CG system is a gem - use it!

