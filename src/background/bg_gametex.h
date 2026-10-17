// Game-accurate stage textures (docs/bg_research/STAGE_AUDIT.md, "Textures").
//
// MBAA composes every CG image of the stage bank into its own D3D texture
// (SpriteFrame_ComposeToCache 0x406750, driven by
// SpriteBank_PrecomposeAllWithDxtBudget 0x406a00):
//   - size = the image bounds rounded up to a power of two, minimum 16
//     (D3D_CreateTextureWithRetry 0x4bf060); the image sits at texel (0,0)
//   - the alignment cells are copied in table order with
//     D3DXLoadSurfaceFromSurface(filter NONE)
//   - format: 8bpp images -> A1R5G5B5 through the palette; everything else
//     A8R8G8B8, or DXT5 when the bank's budget
//       sum over non-8bpp images of pow2(max(64,w)) * pow2(max(64,h)) * 4 / 1024
//     is over 30000 KB (bg01/26/29/51/55 in the shipped data)
//   - samplers clamp (D3D_InitializeDeviceRenderStates: ADDRESSU/V = CLAMP)
//   - sprite UVs run from 0.25 texel to w texels (Sprite_EmitTransformedQuad)
//
// For DXT5 the cells are pushed through the same D3DX function the game
// uses (D3DXLoadSurfaceFromMemory into a DXT5 surface, cell by cell) when a
// d3dx9_36.dll can be loaded; otherwise a built-in BC3 encoder stands in
// and the result is close but not bit-identical (EncoderName() says which).
#ifndef BG_GAMETEX_H_GUARD
#define BG_GAMETEX_H_GUARD

#include <cstdint>
#include <string>
#include <vector>

class CG;

namespace bg {

constexpr long kDxtBudgetKB = 30000;

struct GameTexture {
	int texW = 0, texH = 0;      // power-of-two texture size
	int w = 0, h = 0;            // image bounds size
	int originX = 0, originY = 0; // bounds_x1 / bounds_y1 in the CG canvas
	bool dxt5 = false;
	std::vector<uint8_t> rgba;   // texW * texH * 4 (non-DXT), straight alpha
	std::vector<uint8_t> blocks; // texW/4 * texH/4 * 16 (DXT5)
};

// SpriteBank_PrecomposeAllWithDxtBudget's budget, in KB.
long GameTextureBudgetKB(CG& cg);

// Compose image n as the game does. `dxt5` = the bank is over budget.
bool ComposeGameTexture(CG& cg, int n, bool dxt5, GameTexture& out);

// Decode DXT5 blocks to RGBA (used when the GPU cannot take S3TC and for
// tests); the GL renderer uploads the blocks as-is so the GPU decodes them
// the way the game's D3D device does.
void DecodeDxt5(const uint8_t* blocks, int texW, int texH, std::vector<uint8_t>& rgba);

// "d3dx9_36-x86" (bg_dxt32.exe, the game's own 32-bit path), "d3dx9_36-x64"
// (in-process, a few blocks round differently), "builtin-bc3", or "".
const char* EncoderName();

// Compress every 32-bit image of an over-budget bank in one go, as the game
// precomposes the whole bank at load. Runs bg_dxt32.exe (next to the running
// executable) once; later ComposeGameTexture calls for this CG take their
// blocks from the cache. Safe to call again (no-op for the same CG state).
void PrecomposeDxtBank(CG& cg);
// FPU mode the helper uses: "" (thread default), "fpu24" or "fpu53".
void SetDxtHelperMode(const char* mode);

} // namespace bg

#endif
