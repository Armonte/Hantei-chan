// .pat / AF layer render rules (headless, no GL): PRAL blend modes, PRRV UV flips, AFHK curves, next-frame
// choice, angle and colour lerps. Expected values are worked from the game code quoted in
// src/parts/part_render_math.h (uni2.exe; MBAA.exe for the shared AFHK table).
//
//   part_render_math_test

#include "parts/part_render_math.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0, g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)
static bool Near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

using namespace partrender;

static void TestBlend()
{
	// PatPart_SetBlendFilterGlobals_PRAL_PRFL: 1 -> add, 2 -> reverse subtract, 3 -> normal + blur mask, else normal.
	CHECK(BlendFromPral(0) == kBlendNormal);
	CHECK(BlendFromPral(1) == kBlendAdd);
	CHECK(BlendFromPral(2) == kBlendSub);
	CHECK(BlendFromPral(3) == kBlendBlurMask);
	CHECK(BlendFromPral(4) == kBlendNormal);
	CHECK(BlendFromPral(255) == kBlendNormal);
}

static void TestFlip()
{
	// PatPart_GetCutoutUVWithFlip: bit 0 -> u += w-1, w = -w; bit 1 -> v += h-1, h = -h.
	UvRect r = FlipUv(10, 20, 30, 40, 0);
	CHECK(r.u == 10 && r.v == 20 && r.w == 30 && r.h == 40);
	r = FlipUv(10, 20, 30, 40, 1);
	CHECK(r.u == 39 && r.w == -30 && r.v == 20 && r.h == 40);
	r = FlipUv(10, 20, 30, 40, 2);
	CHECK(r.u == 10 && r.w == 30 && r.v == 59 && r.h == -40);
	r = FlipUv(10, 20, 30, 40, 3);
	CHECK(r.u == 39 && r.w == -30 && r.v == 59 && r.h == -40);
	// H flip mirrors only the horizontal texture axis: the left edge samples the old right edge (minus one
	// UV unit), the top edge is unchanged. (The old renderer turned the part 180 degrees for 1 and 2.)
	r = FlipUv(0, 0, 64, 32, 1);
	CHECK(r.u + r.w * 0.f == 63.f && r.u + r.w * 1.f == -1.f);
	CHECK(r.v + r.h * 0.f == 0.f && r.v + r.h * 1.f == 32.f);
}

static void TestEase()
{
	// x = 1 - progress; the result is the current frame's weight.
	for (int type = 1; type <= 5; ++type) {
		CHECK(Near(EaseAfhk(type, 1.f), 1.f));
		CHECK(Near(EaseAfhk(type, 0.f), 0.f));
	}
	CHECK(Near(EaseAfhk(1, 0.25f), 0.25f));
	CHECK(Near(EaseAfhk(2, 0.5f), 0.75f));       // 1 - (1-x)^2
	CHECK(Near(EaseAfhk(3, 0.5f), 0.25f));       // x^2
	CHECK(Near(EaseAfhk(4, 0.25f), 0.125f));     // 2x^2 below the middle
	CHECK(Near(EaseAfhk(4, 0.75f), 0.875f));
	CHECK(Near(EaseAfhk(5, 0.25f), 0.375f));     // (1-(1-2x)^2)/2
	CHECK(Near(EaseAfhk(5, 0.75f), 0.625f));
	CHECK(Near(EaseAfhk(4, 0.5f), 0.5f) && Near(EaseAfhk(5, 0.5f), 0.5f));
	CHECK(EaseAfhk(0, 0.3f) == 1.f && EaseAfhk(6, 0.3f) == 1.f);

	// Progress: 0 on the frame's first tick, k/duration after k ticks.
	CHECK(CurrentWeight(1, 0, 4) == 1.f);
	CHECK(Near(CurrentWeight(1, 1, 4), 0.75f));
	CHECK(Near(CurrentWeight(1, 3, 4), 0.25f));
	CHECK(Near(CurrentWeight(2, 2, 4), 0.75f));  // "slow->fast": next weight p^2 = 0.25
	CHECK(Near(CurrentWeight(3, 2, 4), 0.25f));  // "fast->slow": next weight 1-(1-p)^2 = 0.75
	CHECK(CurrentWeight(0, 2, 4) == 1.f);
	CHECK(CurrentWeight(1, 2, 0) <= 1.f);        // duration 0 counts as 1, no division by zero
}

static void TestNextFrame()
{
	// AnimState_AdvanceTick_ComputeNext.
	CHECK(NextFrameIndex(3, 1, 0, 0, 0, 0) == 4);            // next
	CHECK(NextFrameIndex(3, 0, 0, 7, 0, 0) == 3);            // end: interpolates toward itself (no blend)
	CHECK(NextFrameIndex(3, 2, 0, 1, 0, 0) == 1);            // jump
	CHECK(NextFrameIndex(3, 2, 4, -2, 0, 0) == 1);           // relative jump
	CHECK(NextFrameIndex(3, 2, 2, 1, 6, 2) == 1);            // loop still running
	CHECK(NextFrameIndex(3, 2, 2, 1, 6, 1) == 6);            // last pass -> loop end
	CHECK(NextFrameIndex(3, 2, 2 | 8, 1, 2, 1) == 5);        // relative loop end
	// MBAA (AnimState_ProcessTransition 0x419C00) picks the loop end at counter 0, UNI2/MBTL at 1.
	CHECK(LoopEndCounter(true) == 1 && LoopEndCounter(false) == 0);
	CHECK(NextFrameIndex(3, 2, 2, 1, 6, 0, LoopEndCounter(false)) == 6);
	CHECK(NextFrameIndex(3, 2, 2, 1, 6, 1, LoopEndCounter(false)) == 1);
	CHECK(NextFrameIndex(3, 2, 2, 1, 6, 1, LoopEndCounter(true)) == 6);
}

// patscale (2026-10-14): G_CHAOS p100 (MBAC -> CC, AFHK 1 on every frame) was reported drawing its parts 1.6x / 6x
// larger after S1. It is not: the shots compared different view zooms. These pin the two rules that could have
// done it, so they cannot regress.
static void TestFirstFrameAndMbaaDialect()
{
	// First tick of a frame (and a frame picked in the editor, ticksInFrame 0): no blend at all, any curve and
	// duration, so the next part set is never read.
	for (int type = 0; type <= 6; ++type)
		for (int dur = 0; dur <= 10; ++dur)
			CHECK(CurrentWeight(type, 0, dur) == 1.f);
	// A stale or garbage tick count (state left over from another pattern right after load) clamps: the weight
	// stays in [0, 1], so a lerp never extrapolates past either frame's scale.
	for (int ticks : {-5, 3, 100, 1 << 30}) {
		const float w = CurrentWeight(1, ticks, 2);
		CHECK(w >= 0.f && w <= 1.f);
		const float s = Lerp(2.f, 2.06f, w);   // PRZM of part set 89 -> 90
		CHECK(s >= 2.f && s <= 2.06f);
	}
	// PRAS: UNI2 applies it; MBTL ignores it; MBAA has no PRAS tag (File_ParsePPTPChunks 0x404220) and its
	// part matrix is scale * rotZ * translate (PatPart_BuildTransformMatrix 0x405C70).
	CHECK(PartUsesPras(true, false));
	CHECK(!PartUsesPras(true, true));
	CHECK(!PartUsesPras(false, false));
}

static void TestLerps()
{
	CHECK(Lerp(10.f, 20.f, 1.f) == 10.f);
	CHECK(Lerp(10.f, 20.f, 0.f) == 20.f);
	CHECK(Near(Lerp(10.f, 20.f, 0.25f), 17.5f));
	CHECK(LerpByte(0, 255, 0.5f) == 127);      // truncation like the game's cvttss2si
	CHECK(LerpByte(255, 0, 0.5f) == 127);
	CHECK(LerpByte(200, 100, 1.f) == 200);

	// Shortest way round: 0.9 -> 0.1 passes through 1.0 (= 0), not back through 0.5.
	CHECK(Near(LerpTurns(0.9f, 0.1f, 0.5f), 0.f) || Near(LerpTurns(0.9f, 0.1f, 0.5f), 1.f));
	CHECK(Near(LerpTurns(0.1f, 0.9f, 0.5f), 0.f));
	CHECK(Near(LerpTurns(0.f, 0.25f, 0.5f), 0.125f));
	CHECK(LerpTurns(0.3f, 0.7f, 1.f) == 0.3f);
	CHECK(LerpTurns(0.3f, 0.7f, 0.f) == 0.7f);

	// Layer colour. UNI: no AFAL mode -> alpha 255 and no colour lerp at all.
	LayerRgba cur{1.f, 0.f, 0.f, 0.5f}, next{0.f, 0.f, 1.f, 0.25f};
	LayerRgba o = LerpLayerColor(cur, 0, next, 1, 0.5f, true);
	CHECK(o.r == 1.f && o.b == 0.f && o.a == 1.f);
	// UNI with a mode: RGB and alpha lerp; the next layer's alpha counts only when it has a mode.
	o = LerpLayerColor(cur, 1, next, 1, 0.5f, true);
	CHECK(Near(o.r, 0.5f) && Near(o.b, 0.5f) && Near(o.a, 0.375f));
	o = LerpLayerColor(cur, 1, next, 0, 0.5f, true);
	CHECK(Near(o.a, 0.75f));
	// MBAACC lerps RGB even without a mode.
	o = LerpLayerColor(cur, 0, next, 0, 0.5f, false);
	CHECK(Near(o.r, 0.5f) && o.a == 1.f);
}

// AFHK weight 0. MBAA.exe: an AFD-0 frame never sets x (AnimState_ProcessTransition 0x419C00), so the weight is
// 0 for every curve; the part transform then keeps the current part (PatPart_BuildTransformMatrix 0x405C70) while
// UNI2 1.42 / MBTL lerp it fully to the next part set. Every game skips the part lerp at weight 1.
static void TestWeightZero()
{
	for (int type = 1; type <= 5; ++type) {
		CHECK(CurrentWeight(type, 0, 0, true) == 0.f);   // MBAACC, AFD 0
		CHECK(CurrentWeight(type, 3, 0, true) == 0.f);   // any tick count
		CHECK(CurrentWeight(type, 0, 0, false) == 1.f);  // UNI2/MBTL, AFD 0: unchanged (first tick, no blend)
		CHECK(CurrentWeight(type, 0, 4, true) == 1.f);   // MBAACC with AFD != 0: same as before
		CHECK(CurrentWeight(type, 2, 4, true) == CurrentWeight(type, 2, 4, false));
	}
	CHECK(CurrentWeight(0, 0, 0, true) == 1.f && CurrentWeight(6, 0, 0, true) == 1.f);   // no AFHK

	// Part transform lerp gate.
	CHECK(!PartLerpsTransform(0.f, false));   // MBAACC: weight 0 -> current part
	CHECK(PartLerpsTransform(0.f, true));     // UNI2/MBTL: weight 0 -> next part
	CHECK(!PartLerpsTransform(1.f, false) && !PartLerpsTransform(1.f, true));
	CHECK(PartLerpsTransform(0.5f, false) && PartLerpsTransform(0.5f, true));
	CHECK(PartLerpsTransform(1e-6f, false));

	// What a weight-0 part draws, per game (the renderer applies Lerp only when the gate is open).
	const float cur = 2.f, next = 3.f;
	auto drawn = [&](float t, bool uni) { return PartLerpsTransform(t, uni) ? Lerp(cur, next, t) : cur; };
	CHECK(drawn(0.f, false) == cur);
	CHECK(drawn(0.f, true) == next);
	CHECK(Near(drawn(0.5f, false), 2.5f) && Near(drawn(0.5f, true), 2.5f));
	// Layer values (scale/rotation/colour) have no weight-0 skip in any game: weight 0 is the next frame's.
	CHECK(Lerp(cur, next, 0.f) == next && LerpTurns(0.1f, 0.2f, 0.f) == 0.2f);
}

int main()
{
	TestBlend();
	TestFlip();
	TestEase();
	TestNextFrame();
	TestLerps();
	TestFirstFrameAndMbaaDialect();
	TestWeightZero();
	std::printf("part_render_math_test: %d checks, %d failed\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
