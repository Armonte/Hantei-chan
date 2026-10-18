// cgm_preview_layout_test: the CG manager preview layout (src/cgm/cgm_preview_layout.h). A big image (UNI2 1569x993 at x2, the
// report "cg manager cant seem to see some sprites if they are too big") must be fully visible with Fit, centred, and reachable by
// scrolling with Fit off; the checker rect is exactly the image rect.
#include "cgm/cgm_preview_layout.h"
#include <cmath>
#include <cstdio>
using namespace cgm;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static bool Contains(const PreviewRect &v, const PreviewLayout &L) { return v.l <= L.x && v.t <= L.y && v.r >= L.x + L.w && v.b >= L.y + L.h; }

int main() {
	struct Img { int w, h; } imgs[] = {{1569, 993}, {2048, 1504}, {4096, 1024}, {64, 96}, {1, 1}};
	struct Pane { float w, h; } panes[] = {{480, 300}, {700, 120}, {1600, 900}, {30, 30}};
	for (const Img &im : imgs) for (const Pane &p : panes) for (float z : {0.25f, 1.f, 2.f, 4.f, 8.f}) {
		// Fit: the whole image shows in the unscrolled pane, centred, never above the requested zoom
		PreviewLayout F = ComputePreviewLayout(p.w, p.h, im.w, im.h, z, true);
		CHECK(F.zoom <= z + 1e-6f);
		CHECK(F.w <= p.w + 0.01f && F.h <= p.h + 0.01f);
		CHECK(Contains(PreviewVisible(F, p.w, p.h, 0, 0), F));
		CHECK(F.contentW == p.w && F.contentH == p.h);               // no scrolling needed
		CHECK(std::fabs((F.x + F.w * 0.5f) - p.w * 0.5f) <= 1.f);    // centred
		CHECK(std::fabs((F.y + F.h * 0.5f) - p.h * 0.5f) <= 1.f);
		CHECK(F.w > 0 && F.h > 0);
		// Fit off: exact zoom, content covers the image, every image pixel is reachable by some scroll position
		PreviewLayout E = ComputePreviewLayout(p.w, p.h, im.w, im.h, z, false);
		CHECK(E.zoom == z);
		CHECK(E.w == im.w * z && E.h == im.h * z);
		CHECK(E.x >= 0 && E.y >= 0 && E.x + E.w <= E.contentW && E.y + E.h <= E.contentH);
		const float maxSX = E.contentW - p.w, maxSY = E.contentH - p.h;
		CHECK(maxSX >= 0 && maxSY >= 0);
		PreviewRect tl = PreviewVisible(E, p.w, p.h, 0, 0), br = PreviewVisible(E, p.w, p.h, maxSX, maxSY);
		CHECK(tl.r > tl.l && tl.b > tl.t);                           // some of the image shows at the start
		CHECK(tl.l == E.x && tl.t == E.y && br.r == E.x + E.w && br.b == E.y + E.h);   // both corners reachable
		if (E.w <= p.w && E.h <= p.h) CHECK(Contains(tl, E));
	}
	// the reported case: 1569x993 at x2 in a ~480x300 pane: Fit shows all of it (zoom ~0.3), not a 3138-wide top-left corner
	PreviewLayout R = ComputePreviewLayout(480, 300, 1569, 993, 2.f, true);
	CHECK(R.zoom < 0.31f && R.zoom > 0.29f);
	if (fails) { std::printf("cgm_preview_layout: %d failure(s)\n", fails); return 1; }
	std::printf("cgm_preview_layout: ok\n");
	return 0;
}
