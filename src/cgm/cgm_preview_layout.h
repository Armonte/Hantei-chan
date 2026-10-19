#pragma once
// CG manager image preview layout (Browser, CG + .pat textures, Palettes): pure math, no ImGui, so tests/cgm_preview_layout_test.cpp
// can check it. A preview pane of paneW x paneH shows an image of imgW x imgH:
//   - zoom: the effective scale. With `fit` on it never exceeds the scale that fits the whole image in the pane, so a big image
//     (UNI2 1569x993, MBTL / UNI sheets over 2048 px) starts fully visible instead of showing only its empty top-left corner.
//   - contentW/H: the scrollable area (at least the pane, so a small image is centred in it; larger when zoomed past the pane).
//   - x, y: the image's top-left inside that area; the checker and the image both cover exactly [x, x + w) x [y, y + h).
#include <algorithm>
#include <cmath>

namespace cgm {

struct PreviewLayout {
	float zoom = 1.f;
	float x = 0.f, y = 0.f, w = 0.f, h = 0.f;   // image rect in content coordinates
	float contentW = 0.f, contentH = 0.f;
};

inline float PreviewFitZoom(float paneW, float paneH, int imgW, int imgH) {
	if (imgW <= 0 || imgH <= 0 || paneW <= 0.f || paneH <= 0.f) return 1.f;
	return std::min(paneW / (float)imgW, paneH / (float)imgH);
}

inline PreviewLayout ComputePreviewLayout(float paneW, float paneH, int imgW, int imgH, float zoom, bool fit) {
	PreviewLayout L;
	paneW = std::max(paneW, 1.f); paneH = std::max(paneH, 1.f);
	if (imgW <= 0 || imgH <= 0) { L.contentW = paneW; L.contentH = paneH; return L; }
	L.zoom = zoom > 0.f ? zoom : 1.f;
	if (fit) L.zoom = std::min(L.zoom, PreviewFitZoom(paneW, paneH, imgW, imgH));
	L.w = imgW * L.zoom; L.h = imgH * L.zoom;
	L.contentW = std::max(paneW, L.w); L.contentH = std::max(paneH, L.h);
	L.x = std::floor((L.contentW - L.w) * 0.5f); L.y = std::floor((L.contentH - L.h) * 0.5f);
	return L;
}

// The part of the image rect visible through the pane scrolled to (scrollX, scrollY), in content coordinates. Empty (r <= l) when
// nothing of the image shows.
struct PreviewRect { float l = 0.f, t = 0.f, r = 0.f, b = 0.f; };
inline PreviewRect PreviewVisible(const PreviewLayout &L, float paneW, float paneH, float scrollX, float scrollY) {
	PreviewRect v;
	v.l = std::max(L.x, scrollX); v.t = std::max(L.y, scrollY);
	v.r = std::min(L.x + L.w, scrollX + paneW); v.b = std::min(L.y + L.h, scrollY + paneH);
	return v;
}

} // namespace cgm
