# Background System Integration Guide

## Quick Start - Wire Everything Together

### Step 1: Add Background System to MainFrame

In `main_frame.h`, add these members:

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
	
	// ... rest of class ...
};
```

### Step 2: Initialize in Constructor

In `main_frame.cpp` constructor:

```cpp
MainFrame::MainFrame(ContextGl* context) {
	// ... existing initialization ...
	
	// Set up background renderer
	render->SetBackgroundRenderer(&bgRenderer, &bgCamera);
}
```

### Step 3: Add Menu Item

In `MainFrame::DrawMenuBar()` or wherever you have menus:

```cpp
if (ImGui::BeginMenu("Stage")) {
	if (ImGui::MenuItem("Load Stage File...")) {
		std::wstring path = OpenFileDialog(
			mainWindowHandle, 
			L"MBAACC Stage Files (*.dat)\0*.dat\0All Files\0*.*\0",
			nullptr
		);
		
		if (!path.empty()) {
			LoadStageFile(path);
		}
	}
	
	if (ImGui::MenuItem("Clear Stage")) {
		ClearStage();
	}
	
	ImGui::Separator();
	
	bool bgEnabled = bgRenderer.IsEnabled();
	if (ImGui::Checkbox("Enable Stage Rendering", &bgEnabled)) {
		bgRenderer.SetEnabled(bgEnabled);
	}
	
	ImGui::EndMenu();
}
```

### Step 4: Add Load/Clear Methods

```cpp
void MainFrame::LoadStageFile(const std::wstring& path) {
	// Convert wstring to char*
	char pathMb[512];
	wcstombs(pathMb, path.c_str(), sizeof(pathMb));
	
	// Clear previous stage
	ClearStage();
	
	// Load new stage
	currentBgFile = new bg::File();
	if (currentBgFile->Load(pathMb)) {
		bgRenderer.SetFile(currentBgFile);
		bgRenderer.SetEnabled(true);
		std::cout << "Loaded stage: " << pathMb << std::endl;
	} else {
		delete currentBgFile;
		currentBgFile = nullptr;
		std::cerr << "Failed to load stage" << std::endl;
	}
}

void MainFrame::ClearStage() {
	if (currentBgFile) {
		bgRenderer.SetFile(nullptr);
		bgRenderer.SetEnabled(false);
		delete currentBgFile;
		currentBgFile = nullptr;
	}
}
```

### Step 5: Update Animations

In your main render loop (probably in `MainFrame::Draw()`):

```cpp
void MainFrame::Draw() {
	// ... existing code ...
	
	// Update background animations (60fps)
	bgRenderer.Update();
	
	// ... rest of drawing ...
}
```

### Step 6: Camera Control (Optional)

To make the background move with character panning:

```cpp
void MainFrame::HandleMouseDrag(int dx, int dy, bool rightDrag, bool leftDrag) {
	if (leftDrag) {
		// Existing character view panning
		render->x += dx;
		render->y += dy;
		
		// Also pan background camera
		if (bgRenderer.IsEnabled()) {
			bgCamera.Pan(-dx / render->scale, -dy / render->scale);
		}
	}
	
	// ... rest of method ...
}
```

### Step 7: Status Bar Display (Optional)

Add to your status bar:

```cpp
void MainFrame::DrawStatusBar() {
	// ... existing status items ...
	
	if (bgRenderer.IsEnabled() && currentBgFile) {
		ImGui::Text("| Stage: %d objects", currentBgFile->GetObjects().size());
		ImGui::Text("| Cam: (%.0f, %.0f)", bgCamera.x, bgCamera.y);
	}
}
```

## Testing

1. **Build:**
   ```bash
   cd /mnt/c/games/qoh/Hantei-chan
   cmake -B build -G "MinGW Makefiles"
   cmake --build build
   ```

2. **Run:**
   - Launch Hantei-chan
   - Menu → Stage → Load Stage File
   - Navigate to MBAA stage files (usually `data/stg*.dat`)
   - Select a stage file
   - Background should render behind the character!

3. **Test with a real stage:**
   - Find your MBAACC installation
   - Look in `data/` folder for `stg00.dat`, `stg01.dat`, etc.
   - Load one and you should see animated backgrounds!

## Troubleshooting

### Background not showing
- Check that `bgRenderer.IsEnabled()` returns true
- Verify the .dat file loaded successfully (check console output)
- Make sure `DrawBackground()` is being called in `Render::Draw()`

### Sprites not rendering
- The embedded CG might have failed to load
- Check temp file permissions (creates `temp_bg_stage.cg`)
- Verify sprite IDs are valid (check console for errors)

### Animation not working
- Make sure `bgRenderer.Update()` is being called every frame
- Check frame durations aren't 0

### Performance issues
- Texture cache should prevent reloading sprites
- Use ClearTextureCache() if memory is an issue
- Consider limiting number of rendered objects

## Next Steps

Once basic rendering works:
1. Add auto-discovery of stage files from MBAA folder
2. Add settings to save/load default stage
3. Add background editor UI (timeline, object list, etc.)
4. Improve camera controls (zoom, reset, etc.)
5. Add palette switching support for stage variants

## Example Stage Files

Common MBAA stage files to test with:
- `stg00.dat` - Training stage (simple, good for testing)
- `stg01.dat` - School rooftop (parallax layers)
- `stg02.dat` - Park (animated elements)
- `stg03.dat` - Night street (complex layering)

Look in your MBAACC installation under `/data/` directory.

## File Format Reference

The .dat files are essentially ha4 format:
- Header: "bgmake" magic (6 bytes) + 10 null bytes (64 bytes total)
- Offset table: 256 int32 entries (1024 bytes)
- Object data: Rendering frames (60 bytes + 132 bytes per frame)
- Embedded CG: Complete .cg file appended at end

Each frame has:
- spriteId, offsetX, offsetY, duration
- blendMode, opacity, aniType
- Movement vectors (xVec, yVec)

Objects have:
- parallax value (256 = 1.0x camera speed)
- layer value (higher = render in front)
- Collection of frames

The CG system loads sprites on demand and caches textures for performance.

