#ifndef BG_TYPES_H_GUARD
#define BG_TYPES_H_GUARD

#include <cstdint>
#include <vector>
#include <string>

namespace bg {

// Background frame - similar to Frame_AF but simpler (no hitboxes, AS, AT, etc.)
struct Frame {
	// Core rendering data (same as Frame_AF)
	int16_t spriteId = -1;
	int16_t offsetX = 0;
	int16_t offsetY = 0;
	int16_t duration = 1;
	
	// Draw properties
	uint8_t blendMode = 0;     // 0=normal, 2=additive
	uint8_t opacity = 255;     // 0-255
	
	// Animation control (same as Frame_AF)
	uint8_t aniType = 1;       // 0=end, 1=loop, 2=jump
	uint8_t jumpFrame = 0;     // For aniType=2
	
	// Movement vectors (bgmake-specific)
	uint8_t enableXVec = 0;
	uint8_t enableYVec = 0;
	int16_t xVec = 0;
	int16_t yVec = 0;
	
	// Runtime computed position (for vector movement)
	float runtimeX = 0.0f;
	float runtimeY = 0.0f;
};

// Background object - collection of frames with parallax/layer
struct Object {
	std::string name;
	int32_t parallax = 256;    // 256 = 1.0x camera speed
	int32_t layer = 128;       // Higher = render in front
	
	std::vector<Frame> frames;
	
	// Animation state
	int32_t currentFrame = 0;
	int32_t frameDuration = 0;
	
	void Update();
	void Reset();
};

// Camera for parallax calculations
struct Camera {
	float x = 0.0f;
	float y = 0.0f;
	float zoom = 1.0f;
	
	// Apply parallax effect (256 = 1.0x speed)
	inline float ApplyParallaxX(float worldX, int parallax) const {
		float factor = parallax / 256.0f;
		return worldX - (x * factor);
	}
	
	inline float ApplyParallaxY(float worldY, int parallax) const {
		float factor = parallax / 256.0f;
		return worldY - (y * factor);
	}
	
	void Pan(float dx, float dy) { x += dx; y += dy; }
	void SetZoom(float z) { zoom = z; }
};

} // namespace bg

#endif /* BG_TYPES_H_GUARD */


