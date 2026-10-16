#ifndef PART_BLUR_PASS_H_GUARD
#define PART_BLUR_PASS_H_GUARD

#include <functional>

// PRAL 3 .pat parts: UNI2's masked screen blur (uni2 1.42, sh_paniblur.txt).
//
// The game does not draw a PRAL 3 part on screen. PatPart_PrepareDraw_ColorSpecBlendMatrix 0x4150E0 and
// Han6Draw_DrawPatLayerParts_2D 0x4C01D0 queue, around the part's own prims, the callbacks registered by
// CharaPtBlur_RegisterCallbacks 0x87FC70. The prim bucket is LIFO (PrimQueue_LinkPrimIntoBucket 0x4BC570), so they
// run in reverse order of queueing:
//   1. CharaPtBlur_BeginMaskTarget 0x87FBA0: save the render targets, copy the scene RT into RT B, bind the mask RT,
//      clear it to opaque black (0xFF000000).
//   2. The part's prims draw into the mask RT (normal blend, its usual colour).
//   3. CharaPtBlur_DrawBlurWH_Passes 0x87F9A0: bind RT C, TecCharaPtBlurDrawH (vertical 9 taps of B, no mask test);
//      restore the scene RT, TecCharaPtBlurDrawW (horizontal 9 taps of C, returns 0 where mask.r < 0.001) drawn
//      over the scene with normal blend. Taps are 1/1280, 1/720 of the 1280x720 targets (one game pixel).
// So the part is a mask: the scene behind it, where its red channel is not 0, is replaced by a 9x9 separable
// gaussian of itself.
namespace PartBlurPass
{
	// drawMask draws the part with the current program and matrix. pxPerGamePx is the view zoom (editor pixels per
	// game pixel), which scales the tap spacing. Runs on the currently bound draw framebuffer and viewport.
	void Run(const std::function<void()>& drawMask, float pxPerGamePx);
	// Kernel of PS_CharaPtBlurFuncW/H: weight of tap 0..4 (taps 1..4 are used on both sides).
	extern const float kWeights[5];
}

#endif /* PART_BLUR_PASS_H_GUARD */
