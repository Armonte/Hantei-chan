#ifndef PART_RENDER_MATH_H_GUARD
#define PART_RENDER_MATH_H_GUARD
// Pure (no GL) rules the renderer follows for .pat part blending, UV flips and AFHK interpolation, kept in one
// header so tests/part_render_math_test.cpp checks exactly the code that draws. Every rule is the game's, read
// in IDA (uni2.exe unless noted; MBAA.exe agrees where it has the feature):
//   PatPart_SetBlendFilterGlobals_PRAL_PRFL 0x413840   PRAL -> blend, PRFL -> filter
//   PatPart_GetCutoutUVWithFlip 0x413C50 / PatPart_DrawPlaneQuad 0x4C0210   PRRV = UV-only mirror bitmask
//   Easing_ApplyCurve_AFHK 0x415400 (MBAA Easing_ApplyCurve 0x406F50)        AFHK curves
//   AnimState_FillDrawInterpCtx 0x431D60 / AnimState_AdvanceTick_ComputeNext 0x4BCFD0   progress, next frame
//   AngleLerpTurns_ShortestWrap (MBAA Math_LerpAngleNormalized 0x4057B0)      rotation lerp
//   Han6Draw_DrawLayer 0x4BD510 (MBAA SpriteAnimation_Render 0x407420)       layer colour/alpha lerp
//   Han6Draw_BuildLayerMatrix_LerpScaleRot 0x4BDC30 (MBAA Animation_BuildTransformMatrix 0x407080)
//   PatPart_LerpDiffuseColor_PRCL 0x413880, PatPart_ComputeSpecularAddColor_PRSP 0x413AE0
//   PrimRect_Setup 0x413E80 + CPAniPrimRender_Rect vtable 0x940A44           shape type 2
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace partrender {

// ---- PRAL ------------------------------------------------------------------------------------------------------
// The byte the game switches on: 1 = additive (SRCALPHA, ONE), 2 = reverse-subtract (SRCALPHA, ONE),
// 3 = blur mask: the part is drawn only into a mask, and the scene behind it is replaced by a 9x9 gaussian of itself
// (uni2 1.42 CharaPtBlur_BeginMaskTarget 0x87FBA0 / CharaPtBlur_DrawBlurWH_Passes 0x87F9A0; part_blur_pass.h).
// Every other value draws normal.
enum PartBlend { kBlendNormal = 0, kBlendAdd = 1, kBlendSub = 2, kBlendBlurMask = 3 };

inline PartBlend BlendFromPral(int pral)
{
	switch (pral) {
	case 1: return kBlendAdd;
	case 2: return kBlendSub;
	case 3: return kBlendBlurMask;
	default: return kBlendNormal;
	}
}

// ---- PRRV ------------------------------------------------------------------------------------------------------
// A UV-only mirror: bit 0 = horizontal, bit 1 = vertical, 3 = both. The geometry is never mirrored.
//   bit 0: u += w - 1; w = -w        bit 1: v += h - 1; h = -h        (UV units are 1/256 of the texture)
struct UvRect { float u, v, w, h; };

inline UvRect FlipUv(int u, int v, int w, int h, int prrv)
{
	UvRect r{(float)u, (float)v, (float)w, (float)h};
	if (prrv & 1) { r.u += r.w - 1.f; r.w = -r.w; }
	if (prrv & 2) { r.v += r.h - 1.f; r.h = -r.h; }
	return r;
}

// ---- Arc (shape type 4) radial shrink ------------------------------------------------------------------------
// uni2 1.42 CPAniPrimRender_Donuts_Setup 0x414590 / _NextEdge 0x4148D0 (IDB names): the inner and outer edge points (0, -r) and
// (0, w - r) are carried from segment to segment by ONE matrix, Rz(step) with its x/y rows scaled by
// s = 1 - (dRadius / radius) / segments and z translated by dz / segments. The shrink is therefore geometric and
// scales the strip's width too (edge e sits at radius r * s^e, inner r - w scaled the same), not a linear
// r - dRadius * e / segments with a constant width. Returns the scale of edge e.
inline float ArcEdgeScale(int radius, int dRadius, int segments, int e)
{
	if (dRadius == 0 || radius == 0 || segments == 0) return 1.f;
	const float s = 1.f - ((float)dRadius / (float)radius) / (float)segments;
	float k = 1.f;
	for (int i = 0; i < e; i++) k *= s;
	return k;
}

// ---- Part UV bias ----------------------------------------------------------------------------------------------
// Every part quad's UVs are (uv / 256) + a bias at each end (uni2 PatPart_DrawPlaneQuad 0x4BFCB0 + sub_4C0430 shapes,
// MBTL 0xA1CB7C / 0xA1CB78, MBAA PatPart_RenderQuad 0x54C970 / 0x54C974). The start-side biases are initialised to
// u0 = 0x3951B717 (2.0e-4) and v0 = 0x394C78EA (1.95e-4), the end-side ones to 0; only on an AMD adapter (VendorId
// 0x1002, uni2 sub_4B0480) does the game swap them to u0 = 0, u1 = -2.0e-4. The non-AMD values were read live from
// uni2 1.42 on the capture machine (NVIDIA, gamecap uni2_uvbias_peek.gcs). On a texture of a few thousand texels this
// moves the sampled texels by ~0.4 texel, which a 4-5x stretched streak turns into a pixel-sized shift (render-match6).
constexpr float kPartUvBiasU0 = 1.99999995e-4f;   // 0x3951B717
constexpr float kPartUvBiasV0 = 1.94999995e-4f;   // 0x394C78EA
constexpr float kPartUvBiasU1 = 0.f, kPartUvBiasV1 = 0.f;
// Bias at fraction f (0 = the quad's first UV, 1 = its last) along u / v. Applied to plane and rect quads, whose
// corners match the games' corner layout; the editor's other shapes map u/v onto their own axes, and biasing them
// moved MBTL flipV p157 from 3.57 to 5.48, so they stay unbiased.
inline float PartUvBiasU(float f) { return kPartUvBiasU0 + (kPartUvBiasU1 - kPartUvBiasU0) * f; }
inline float PartUvBiasV(float f) { return kPartUvBiasV0 + (kPartUvBiasV1 - kPartUvBiasV0) * f; }

// ---- AFHK ------------------------------------------------------------------------------------------------------
// Curve applied to x = 1 - progress (1 on the frame's first tick). The result t is the CURRENT frame's weight:
// value = cur + (next - cur) * (1 - t). 1 linear, 2 slow->fast, 3 fast->slow, 4 and 5 the two S-curves; any
// other type does not interpolate (t = 1).
inline float EaseAfhk(int type, float x)
{
	switch (type) {
	case 1: return x;
	case 2: return 1.f - (1.f - x) * (1.f - x);
	case 3: return x * x;
	case 4:
		if (x <= 0.5f) return (x + x) * (x + x) * 0.5f;
		{ const float y = 1.f - ((x - 0.5f) + (x - 0.5f)); return (1.f - y * y) * 0.5f + 0.5f; }
	case 5:
		if (x <= 0.5f) { const float y = 1.f - (x + x); return (1.f - y * y) * 0.5f; }
		{ const float y = (x - 0.5f) + (x - 0.5f); return y * y * 0.5f + 0.5f; }
	default: return 1.f;
	}
}

// Current-frame weight `ticksInFrame` ticks into a frame of `duration` ticks (0 on the frame's first tick).
// MBAACC (`mbaaDialect`): AnimState_ProcessTransition 0x419C00 computes x = 1 - tick / AFD only when AFD != 0.
// An AFD-0 frame keeps the caller's zeroed x, so its weight is EaseAfhk(type, 0) = 0 for every curve.
inline float CurrentWeight(int afhk, int ticksInFrame, int duration, bool mbaaDialect = false)
{
	if (afhk < 1 || afhk > 5) return 1.f;
	if (mbaaDialect && duration <= 0) return EaseAfhk(afhk, 0.f);
	const float p = std::clamp((float)ticksInFrame / (float)std::max(1, duration), 0.f, 1.f);
	return EaseAfhk(afhk, 1.f - p);
}

// The loop counter value at which the interpolation target is the loop end (AFFE bit 2). UNI2/MBTL
// (AnimState_FillDrawInterpCtx) test 1; MBAA (AnimState_ProcessTransition 0x419C00) tests 0.
inline int LoopEndCounter(bool uniDialect) { return uniDialect ? 1 : 0; }

// The frame the game interpolates toward (same pattern): AFFL 1 = next frame; 2 = jump, or the loop-end target
// on the last loop pass (AFFE bit 2 with the loop counter at `loopEndAt`, see LoopEndCounter); AFFE bits 4/8 make
// jump/loop-end relative. Anything else (end of pattern, hold) interpolates toward the frame itself, i.e. not at all.
inline int NextFrameIndex(int frame, int aniType, unsigned aniFlag, int jump, int loopEnd, int loopCounter,
	int loopEndAt = 1)
{
	if (aniType == 1) return frame + 1;
	if (aniType == 2) {
		if ((aniFlag & 2) && loopCounter == loopEndAt)
			return (aniFlag & 8) ? frame + loopEnd : loopEnd;
		return (aniFlag & 4) ? frame + jump : jump;
	}
	return frame;
}

// PRAS (part pivot offset between scale and rotation): uni2.exe PatPart_BuildTransformMatrix 0x4C0EC0 applies it.
// MBTL does not read it, and MBAA.exe has no PRAS tag at all: its .pat parser (File_ParsePPTPChunks 0x404220) knows
// only PRXY/PRZM/PRAN/..., and its part matrix (PatPart_BuildTransformMatrix 0x405C70) is scale * rotZ * translate.
// So an MBAACC-dialect character ignores a PRAS that a converted or UNI-made .pat carries.
inline bool PartUsesPras(bool uniDialect, bool mbtl) { return uniDialect && !mbtl; }

// Whether a part's transform (position, scale, rotation, PRAS) lerps toward the next part set at current weight t.
// UNI2 1.42 (PatPart_BuildTransformMatrix 0x4C10E0) and MBTL (0x4A39A0) skip the lerp only at t == 1, so t == 0
// draws the next part set's transform. MBAA.exe (PatPart_BuildTransformMatrix 0x405C70) also skips it at t == 0
// and draws the current part's transform. Part colour has no such skip in any of them (MBAA
// Palette_InterpolateARGB 0x4058C0), and the AF layer lerp neither (MBAA Animation_BuildTransformMatrix 0x407080).
inline bool PartLerpsTransform(float t, bool uniDialect) { return t < 1.f && (uniDialect || t != 0.f); }

inline float Lerp(float cur, float next, float t) { return cur + (next - cur) * (1.f - t); }

// Byte channel lerp with the game's truncation toward zero.
inline int LerpByte(int cur, int next, float t) { return (int)((float)(next - cur) * (1.f - t) + (float)cur); }

// Rotation in turns along the shorter way round, then kept inside (-1, 1).
inline float LerpTurns(float cur, float next, float t)
{
	if (cur == next) return cur;
	const float w = 1.f - t;
	if (w == 0.f) return cur;
	if (w == 1.f) return next;
	float d = next - cur;
	if (d > 0.5f) d -= 1.f;
	else if (d <= -0.5f) d += 1.f;
	float r = cur + d * w;
	while (r >= 1.f) r -= 1.f;
	while (r <= -1.f) r += 1.f;
	return r;
}

// AF layer colour toward the next frame's same layer. Alpha is 255 when a layer has no AFAL mode. UNI2/MBTL
// lerp RGB and alpha only when the current layer's AFAL mode is non-zero; MBAACC always lerps RGB.
struct LayerRgba { float r, g, b, a; };

inline LayerRgba LerpLayerColor(const LayerRgba &cur, int curMode, const LayerRgba &next, int nextMode, float t, bool uni)
{
	LayerRgba out = cur;
	out.a = curMode ? cur.a : 1.f;
	if (uni && !curMode) return out;
	out.r = Lerp(cur.r, next.r, t);
	out.g = Lerp(cur.g, next.g, t);
	out.b = Lerp(cur.b, next.b, t);
	if (curMode) out.a = Lerp(cur.a, nextMode ? next.a : 1.f, t);
	return out;
}

// ---- Part-space perspective ----------------------------------------------------------------------------------
// Every .pat part goes through uni2 1.42 PatPart_GetGlobalPerspective640x480 0x4C1860 (PatPart_BuildTransformMatrix
// with a5 == 0): Scale(1/640, 1/480, 1/1024) * LookAtLH(eye (0,0,-cot), at 0, up +y) * PerspectiveFovLH(1.3089969 (75 deg),
// aspect 1, 0.01, 1000) * Scale(640, 480, 1). A part at z = 0 maps 1:1 to pixels; elsewhere it scales by
// E / (E + z) with E = 1024 * cot(fov/2) ~ 1334.5 px. LEFT-HANDED: positive part z is FARTHER (smaller). The
// editor draws with a GL frustum looking down -z, so z is mirrored. `w`, `h` = the editor view in pixels; the
// result maps the part origin to NDC (-1, 1) (the view's top-left, moved by the caller's pre-transform).
// 0x3FA78D36 = 1.3089969 rad = 75 degrees (uni2 0x4C1860 and MBTL 0x4A3DE0 pass the same constant to
// D3DXMatrixPerspectiveFovLH). 1.3045008 was a misread; it put the eye at 1340.7 px instead of 1334.5 and moved the
// tips of deep 3D-rotated parts by ~0.7 px (flipHV_chr019_p153, render-match6).
constexpr float kPartFovY = 1.3089969f;   // radians
inline float PartEyeDistance() { return 1024.f / std::tan(kPartFovY * 0.5f); }

inline glm::mat4 PartPerspective(float w, float h)
{
	constexpr float dist = 1024.f, dist2 = dist * 2.f;
	const float f = 1.f / std::tan(kPartFovY * 0.5f);
	glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(-1, 1, 0)) *
		glm::frustum<float>(-w / dist2, w / dist2, h / dist2, -h / dist2, 1 * f, dist * 2 * f);
	m = glm::translate(m, glm::vec3(0.f, 0.f, -dist * f));
	return glm::scale(m, glm::vec3(1.f, 1.f, -1.f));
}

// ---- D3D9 pixel-centre rule -----------------------------------------------------------------------------------
// UNI2 1.42, MBTL 25646393 and MBAA all draw through D3D9 with no half-pixel correction anywhere in the part, CG or
// camera chain (uni2 PatPart_GetGlobalPerspective640x480 0x4C1860, CDxCamera_ctor 0x4AAE30, CgLayer_DrawWithCharaShader
// 0x4BE140, shader VS_CharaPtDrawV / VS_CharaDrawV; MBTL 0x4A3DE0 / CDxCamera_ctor 0x48C220; MBAA
// Camera_UpdateMatrices 0x44BE95). In D3D9 the centre of pixel k is window coordinate k; in GL it is k + 0.5. The same
// geometry therefore covers the same pixels in both, but every pixel samples its texture half a pixel further left and
// up in the game. Translating the whole drawn image by +0.5 px (right, and down in window y) in the editor reproduces
// the game's samples exactly: a 6-px rect at x = 0 then samples u = k / 6 at pixel k (pixel 3 = the texture centre,
// a 1-px core on the origin pixel) instead of (k + 0.5) / 6 (a 2-px core half a pixel right). Window y is down, so
// the NDC shift is (+1/w, -1/h). Applied to game-drawn sprites and parts only, not to editor overlays.
//
// Edge ties (render-match5, rect_chr024_p76): an edge that lies exactly on a pixel centre is a tie. D3D9's top-left rule
// keeps the pixel under a TOP edge and drops it under a BOTTOM edge. GL draws y-up, so the same hardware rule lands
// on the other edge: GL dropped the top row of the line, which the game draws (its g_mWorld puts the line's top at
// window y 639.0000047, read live via gamecap v2r6_rect_mtx.gcs, which the rasteriser snaps to 639.0). Moving the image up
// by kGlEdgeTieBiasPx (more than the 1/256 px sub-pixel snap, far below anything visible) breaks every y tie the
// D3D9 way: a top edge on a centre then sits just above it (pixel in), and a bottom edge on a centre also sits just
// above it (pixel out). x needs nothing: left-in / right-out is the same in both APIs.
constexpr float kD3d9PixelCentreShiftPx = 0.5f;
// 0.005 px (render-match6): just above the 1/256 px snap. 1/64 also broke the ties, but every world pixel moved by it and the
// rotated point-sampled CG of afhk_cg_chr020_p435 picked other texels along its edges (2.56 vs 2.22 at 0.005, 2.06 at 0;
// rect_chr024_p76's top row is exact at 0.005 and at 1/64, wrong at 0).
constexpr float kGlEdgeTieBiasPx = 0.005f;

inline glm::mat4 D3d9PixelCentreNdc(float w, float h)
{
	return glm::translate(glm::mat4(1.f), glm::vec3(2.f * kD3d9PixelCentreShiftPx / w,
		-2.f * (kD3d9PixelCentreShiftPx - kGlEdgeTieBiasPx) / h, 0.f));
}

// Window y (y-down) at which the editor's game projection puts game window y `y`: the D3D9 pixel-centre shift minus
// the edge-tie bias. A pixel is covered by a top edge at yEdge when its centre (k + 0.5) is at or below the mapped
// edge; this mirrors D3D9's top-left rule for edges on pixel centres.
inline float EditorWindowYForGameY(float y) { return y + kD3d9PixelCentreShiftPx - kGlEdgeTieBiasPx; }

// CG layer UVs (uni2 CgLayer_DrawWithCharaShader 0x4BE140): u runs from 0.25 texel to w texels across the w-px quad
// (and v likewise), so a point-sampled 1:1 sprite under the D3D9 rule samples 0.25 texel into each texel, never on
// a texel boundary. Returned as the start of the [start, 1] normalised UV range of a `texels`-wide image.
constexpr float kCgUvStartTexel = 0.25f;
inline float CgUvStart(int texels) { return texels > 0 ? kCgUvStartTexel / (float)texels : 0.f; }

// PPPA colour-slot tint palette. MBTL (build 25646393) PatPart_SetupDraw_ShaderWorldMtx 0x411C10 sets
// g_fPtLastColor = drawParams+0x2C[PPPA] bytes 0..2 / 255 (alpha unused). Han6Object_Draw 0x597070 points that at
// CCharaDataSet+0x118, a 256-colour copy that CharaDataSet_ApplyColorBuildPalTexAndTintCopy 0x593030 takes from
// palette element slot 0: the base <cg>.pal of the selected colour. A pattern's PUPS only moves the sprite's
// palette texture row (fPalPlusV), so the tint never reads <cg>_pN.pal. Returns the palette to tint from: bank 0
// when the character has one, else the palette in effect.
inline const unsigned *PartTintPalette(const unsigned *bank0Palette, const unsigned *currentPalette)
{
	return bank0Palette ? bank0Palette : currentPalette;
}
// RGB multipliers of a palette entry as the game reads it (memory bytes 0, 1, 2 = R, G, B).
inline void PaletteEntryRgb(unsigned entry, float rgb[3])
{
	rgb[0] = (float)(entry & 0xFF) / 255.f;
	rgb[1] = (float)((entry >> 8) & 0xFF) / 255.f;
	rgb[2] = (float)((entry >> 16) & 0xFF) / 255.f;
}

} // namespace partrender
#endif
