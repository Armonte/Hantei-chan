// RBO / GOF2 / GOF1 part placement regression test (headless, no GL).
//
//   fb_part_transform_test [character.DAT ...]
//
// Ground truth: the game's own draw path, decoded in IDA (docs/formats/fb_part_transform.md).
//   rbo_ex3.exe  PosePart_InterpolateTransform 0x44DD30 / PosePart_LerpPosition 0x44DC70 / PosePart_LerpOrigin 0x44DBA0
//                PoseQuad_FillDrawDesc 0x44E150, Pose_SubmitPartQuad 0x44E5B0, Mat_RotateAboutPivotDeg 0x413350,
//                Camera_BuildSpriteWorldMatrix 0x441CD0, Pose_CollectSortedParts 0x44E030, Math_LerpAngle10000ToDeg 0x44D990
//   GOF2.exe     PosePart_ComputeGeometry 0x43FE50, DrawSort_BubbleByPriority 0x43FAA0, sub_440370 (quad), sub_43F850 (matrix)
//   gof.exe      Render_QueueFighterSprite 0x42FFE0 (same formulas)
// GameCorners() below re-implements the engine arithmetic straight from a raw 92-byte part record (integer truncation
// included); the model side goes through the same header the GL renderer uses (partxf::, src/parts/part_transform.h).
// Synthetic poses always run; every character .DAT given on the command line is checked part by part as well.

#include "han2_pat.h"
#include "parts/parts.h"
#include "parts/part_transform.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int g_fail = 0, g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

namespace {
inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
inline void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }
inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }

struct Pt { double x, y; };

// The engine's quad corners for one part record, in actor space (before the frame matrix), no interpolation.
//   x' = x + ox - ox*sx   (truncated)       pivot = x' + trunc(sx*ox)       [PosePart_LerpPosition, PoseQuad_FillDrawDesc]
//   quad = pivot-relative (0.5 - trunc(sx*ox)) .. + trunc(dw*sx)                [Pose_SubmitPartQuad]
//   angle = rot * 0.036 degrees, x' = x cos - y sin, y' = x sin + y cos          [Math_LerpAngle10000ToDeg, Mat_RotateAboutPivotDeg]
void GameCorners(const uint8_t *r, Pt out[4])
{
	const int x = rd32(r), y = rd32(r + 4), dw = rd32(r + 8), dh = rd32(r + 12), rot = rd32(r + 0x1C), ox = rd32(r + 0x40), oy = rd32(r + 0x44);
	const double sx = rd32(r + 0x14) * 0.001, sy = rd32(r + 0x18) * 0.001;
	const long tlx = (long)((double)(x + ox) - (double)ox * sx), tly = (long)((double)(y + oy) - (double)oy * sy);
	const long psx = (long)((double)ox * sx), psy = (long)((double)oy * sy);
	const double pivx = (double)(tlx + psx), pivy = (double)(tly + psy);
	const long w = (long)((double)dw * sx), h = (long)((double)dh * sy);
	const double deg = rot * 0.035999998, rad = deg * 3.14159265358979323846 / 180.0, c = cos(rad), s = sin(rad);
	const double lx[4] = {0.5 - psx, 0.5 - psx + w, 0.5 - psx + w, 0.5 - psx};
	const double ly[4] = {0.5 - psy, 0.5 - psy, 0.5 - psy + h, 0.5 - psy + h};
	for (int i = 0; i < 4; i++) { out[i].x = pivx + lx[i] * c - ly[i] * s; out[i].y = pivy + lx[i] * s + ly[i] * c; }
}

// Our renderer's corners for the model part (the exact matrix Parts::Draw builds), without the frame matrix.
void ModelCorners(const Parts &parts, const PartProperty &pr, bool fb, Pt out[4])
{
	const CutOut<> &co = parts.cutOuts[pr.ppId];
	glm::mat4 m = partxf::PartMatrix(partxf::Pivot(fb, (float)pr.x, (float)pr.y, co), glm::vec3(pr.rotation[1], pr.rotation[2], pr.rotation[3]),
		glm::vec2(pr.pras[0], pr.pras[1]), glm::vec2(pr.scaleX, pr.scaleY));
	const int tx[4] = {0, 1, 1, 0}, ty[4] = {0, 0, 1, 1};
	for (int i = 0; i < 4; i++) { glm::vec4 v = m * glm::vec4(partxf::QuadPoint(co, (float)tx[i], (float)ty[i]), 0.f, 1.f); out[i] = {v.x, v.y}; }
}

double MaxDist(const Pt a[4], const Pt b[4])
{
	double m = 0;
	for (int i = 0; i < 4; i++) m = std::max(m, std::max(fabs(a[i].x - b[i].x), fabs(a[i].y - b[i].y)));
	return m;
}

// ---- a synthetic PAT v3 block: `poses` x 40 parts, one 256 px texture ----
constexpr size_t kPoseTab = 24, kImgOffField = 24 + 4000 + 32000, kPoseBase = kImgOffField + 4;
std::vector<uint8_t> BuildPat(const std::vector<std::vector<std::vector<uint8_t>>> &poses)
{
	const size_t imageOff = kPoseBase + 3680 * poses.size();
	std::vector<uint8_t> b(imageOff + 11824 + 256 * 256 * 4, 0);
	wr32(b.data(), 3); wr32(b.data() + 4, 0x01234567);
	wr32(b.data() + kImgOffField, (int32_t)imageOff);
	for (size_t p = 0; p < poses.size(); p++) {
		wr32(b.data() + kPoseTab + 4 * p, (int32_t)(kPoseBase + 3680 * p));
		for (size_t k = 0; k < poses[p].size() && k < 40; k++) memcpy(b.data() + kPoseBase + 3680 * p + 92 * k, poses[p][k].data(), 92);
	}
	wr32(b.data() + imageOff + 0x1C, 11824);       // texture 0 offset
	wr32(b.data() + imageOff + 0xD64, 256);        // texture 0 size
	return b;
}
std::vector<uint8_t> Part(int x, int y, int dw, int dh, int sxp, int syp, int rot, int ox, int oy, int layer, int flip = 0)
{
	std::vector<uint8_t> r(92, 0);
	wr32(r.data(), x); wr32(r.data() + 4, y); wr32(r.data() + 8, dw); wr32(r.data() + 12, dh); wr32(r.data() + 0x10, flip);
	wr32(r.data() + 0x14, sxp); wr32(r.data() + 0x18, syp); wr32(r.data() + 0x1C, rot); wr32(r.data() + 0x20, (int32_t)0xFFFFFFFF);
	wr32(r.data() + 0x28, 0); wr16(r.data() + 0x2C, 0); wr16(r.data() + 0x30, 0); wr16(r.data() + 0x34, 64); wr16(r.data() + 0x38, 64);
	r[0x3C] = (uint8_t)layer; wr32(r.data() + 0x40, ox); wr32(r.data() + 0x44, oy);
	return r;
}

void CheckPose(const char *label, const uint8_t *pat, size_t patSize, Parts &parts, int pose, double tol)
{
	const int nPoses = rd32(pat) == 4 ? 2000 : 1000;
	const uint32_t off = (uint32_t)rd32(pat + kPoseTab + 4 * pose);
	if (!off) return;
	const PartSet<> &ps = parts.partSets[pose];
	// draw order: the engine sorts (slot, record, key = slot + (layer << 8)) ascending, skipping unused slots
	std::vector<std::pair<int, int>> keys;   // key, slot
	for (int k = 0; k < 40; k++) {
		const uint8_t *r = pat + off + 92 * k;
		if (off + 92 * (k + 1) > patSize || rd32(r + 0x34) == 0) continue;
		if ((uint32_t)rd32(r + 0x28) == 0xFFFF) continue;
		keys.push_back({k + (r[0x3C] << 8), k});
	}
	std::sort(keys.begin(), keys.end());
	std::vector<PartProperty> sorted = ps.groups;
	partxf::SortForDraw(true, sorted);
	std::vector<int> drawn;
	for (const auto &g : sorted) if (g.ppId >= 0) drawn.push_back(g.propId);
	bool orderOk = drawn.size() == keys.size();
	for (size_t i = 0; orderOk && i < keys.size(); i++) orderOk = drawn[i] == keys[i].second;
	++g_checks; if (!orderOk) { std::printf("%s pose %d: draw order differs from Pose_CollectSortedParts\n", label, pose); ++g_fail; }
	for (const auto &g : ps.groups) {
		if (g.ppId < 0) continue;
		Pt a[4], b[4];
		GameCorners(pat + off + 92 * g.propId, a);
		ModelCorners(parts, g, true, b);
		double d = MaxDist(a, b);
		++g_checks;
		// the engine truncates the pivot, the scaled origin and the scaled size to integers (up to ~1 px each, so ~3 px at the
		// far corner of a scaled and rotated part); unscaled parts must agree to the half-pixel the D3D quad adds
		const uint8_t *rr = pat + off + 92 * g.propId;
		const double lim = (rd32(rr + 0x14) == 1000 && rd32(rr + 0x18) == 1000) ? tol : 3.01;
		if (d > lim) { std::printf("%s pose %d slot %d: corner distance %.2f px (game vs renderer)\n", label, pose, g.propId, d); ++g_fail; }
	}
	(void)nPoses;
}
} // namespace

static void Synthetic()
{
	// slot 0: plain; 1: scaled 150%, origin (32,16); 2: rotated 15 deg clockwise (rot field 417);
	// 3: rotated 270 deg and scaled 60/130%; 4: negative origin, rot 9885 (355 deg), layer 3; 5/6: ties on layer 1 (slot order decides)
	std::vector<std::vector<uint8_t>> pose = {
		Part(-40, -120, 64, 64, 1000, 1000, 0, 0, 0, 2),
		Part(10, -90, 80, 48, 1500, 1500, 0, 32, 16, 1),
		Part(-20, -60, 64, 128, 1000, 1000, 417, 32, 64, 0),
		Part(50, 30, 100, 40, 600, 1300, 7500, 50, 20, 0),
		Part(-5, -30, 96, 96, 1000, 1000, 9885, -10, -20, 3),
		Part(0, 0, 32, 32, 1000, 1000, 0, 16, 16, 1),
		Part(8, 8, 32, 32, 1000, 1000, 0, 0, 0, 1),
	};
	auto pat = BuildPat({pose});
	Parts &parts = *new Parts(nullptr);   // never destroyed: ~Parts() frees GL textures, and there is no GL context here (as pat_roundtrip)
	std::string err;
	CHECK(han2::PatToParts(pat.data(), pat.size(), parts, &err));
	CHECK(parts.fbPartModel);
	CheckPose("synthetic", pat.data(), pat.size(), parts, 0, 1.01);

	// The regression this test exists for: with the old reading (position == pivot) a part whose origin is not (0,0)
	// lands (ox, oy) away from where the game puts it, and a rotated one swings about the wrong point.
	for (const auto &g : parts.partSets[0].groups) {
		if (g.ppId < 0) continue;
		const CutOut<> &co = parts.cutOuts[g.ppId];
		if (co.xy[0] == 0 && co.xy[1] == 0) continue;
		Pt a[4], legacy[4];
		GameCorners(pat.data() + kPoseBase + 92 * g.propId, a);
		ModelCorners(parts, g, false, legacy);
		++g_checks;
		if (MaxDist(a, legacy) < 4.0) { std::printf("legacy placement unexpectedly matched the game for slot %d\n", g.propId); ++g_fail; }
	}

	// explicit numbers: slot 1 = x10 y-90, 80x48 at 150%, origin (32,16): pivot (42,-74), quad -48..+72 x -24..+48 around it
	Pt c[4];
	ModelCorners(parts, parts.partSets[0].groups[1], true, c);
	CHECK(fabs(c[0].x - (42 - 48)) < 1e-3 && fabs(c[0].y - (-74 - 24)) < 1e-3);
	CHECK(fabs(c[2].x - (42 - 48 + 120)) < 1e-3 && fabs(c[2].y - (-74 - 24 + 72)) < 1e-3);
	// slot 2: ~15 deg clockwise about pivot (12,4): the quad's top-left (-32,-64 from the pivot) moves to
	// pivot + (-32 cos + 64 sin, -32 sin - 64 cos)
	ModelCorners(parts, parts.partSets[0].groups[2], true, c);
	const double rad = 417 * 0.036 * 3.14159265358979323846 / 180.0;   // rot field 417 = 417/10000 turn = 15.012 degrees
	CHECK(fabs(c[0].x - (12 + (-32 * cos(rad) + 64 * sin(rad)))) < 0.05);
	CHECK(fabs(c[0].y - (4 + (-32 * sin(rad) - 64 * cos(rad)))) < 0.05);
}

static void RealFile(const char *path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { std::printf("cannot read %s\n", path); ++g_fail; return; }
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (b.size() < 0x40 || memcmp(b.data(), "HAN2RBO ", 8) != 0) { std::printf("%s: not a HAN2RBO file\n", path); ++g_fail; return; }
	const uint32_t off = (uint32_t)rd32(b.data() + 0x28), size = (uint32_t)rd32(b.data() + 0x2C);
	if (!size || off + size > b.size()) { std::printf("%s: no PAT area\n", path); ++g_fail; return; }
	const uint8_t *pat = b.data() + off;
	Parts &parts = *new Parts(nullptr);
	std::string err;
	if (!han2::PatToParts(pat, size, parts, &err)) { std::printf("%s: %s\n", path, err.c_str()); ++g_fail; return; }
	CHECK(parts.fbPartModel);
	int before = g_checks;
	for (size_t p = 0; p < parts.partSets.size(); p++) if (parts.partSets[p].wasLoaded) CheckPose(path, pat, size, parts, (int)p, 1.01);
	std::printf("%s: %zu poses, %d checks\n", path, parts.partSets.size(), g_checks - before);
}

int main(int argc, char **argv)
{
	Synthetic();
	for (int i = 1; i < argc; i++) RealFile(argv[i]);
	std::printf("fb_part_transform_test: %d checks, %d failed\n", g_checks, g_fail);
	std::fflush(stdout);
	return g_fail ? 1 : 0;
}
