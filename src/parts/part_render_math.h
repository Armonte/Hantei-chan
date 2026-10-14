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

namespace partrender {

// ---- PRAL ------------------------------------------------------------------------------------------------------
// The byte the game switches on: 1 = additive (SRCALPHA, ONE), 2 = reverse-subtract (SRCALPHA, ONE),
// 3 = normal blend, and the part is also the mask of a full-screen blur (PatPralMode3_Begin_BlurMaskPass
// 0x872B10 / _End_CompositeBlur 0x872A60). Every other value draws normal.
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
inline float CurrentWeight(int afhk, int ticksInFrame, int duration)
{
	if (afhk < 1 || afhk > 5) return 1.f;
	const float p = std::clamp((float)ticksInFrame / (float)std::max(1, duration), 0.f, 1.f);
	return EaseAfhk(afhk, 1.f - p);
}

// The frame the game interpolates toward (same pattern): AFFL 1 = next frame; 2 = jump, or the loop-end target
// on the last loop pass (AFFE bit 2 with the loop counter at 1); AFFE bits 4/8 make jump/loop-end relative.
// Anything else (end of pattern, hold) interpolates toward the frame itself, i.e. not at all.
inline int NextFrameIndex(int frame, int aniType, unsigned aniFlag, int jump, int loopEnd, int loopCounter)
{
	if (aniType == 1) return frame + 1;
	if (aniType == 2) {
		if ((aniFlag & 2) && loopCounter == 1)
			return (aniFlag & 8) ? frame + loopEnd : loopEnd;
		return (aniFlag & 4) ? frame + jump : jump;
	}
	return frame;
}

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

} // namespace partrender
#endif
