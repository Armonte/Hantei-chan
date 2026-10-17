// Regression tests for the PACNyx-parity work on French-Bread (RBO / GOF2 / GOF1) parts (docs/formats/pacnyx_parity.md).
//  * BUG1  a sprite sheet imported in the PAT editor reaches the saved PAT block (import -> BuildPat -> PatToParts, pixels equal)
//  * limits checked while editing (40 parts per pose, 1000 / 2000 poses, square sheets) and the writer's own messages
//  * removed sheets / named empty poses survive the writer
//  * PAT-editor undo: the parts channel in UndoManager (edit -> commit -> undo -> redo, clean state, sheet restore callback)
//  * PACNyx layer commands (to back / back one / forward one / to front / set) on the engine's draw key (layer << 8) + slot
// Build target: han2_parts_edit_test (CMakeLists.txt). Exit code 0 = pass. Synthetic data only (no game files needed).
#include "han2_pat.h"
#include "han2_parts_edit.h"
#include "parts/parts.h"
#include "parts/parts_undo.h"
#include "undo_manager.h"

#include <cstdio>
#include <set>
#include <cstring>
#include <string>
#include <vector>

std::set<int> numberSet;   // globals some linked sources expect (see undo_manager_test.cpp)
int maxCount;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {
void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }
void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }

constexpr size_t kPoseTab = 24, kImgOffField = 24 + 4000 + 32000, kPoseBase = kImgOffField + 4;
std::vector<uint8_t> Part(int x, int y, int layer, int tex = 0)
{
	std::vector<uint8_t> r(92, 0);
	wr32(r.data(), x); wr32(r.data() + 4, y); wr32(r.data() + 8, 32); wr32(r.data() + 12, 32);
	wr32(r.data() + 0x14, 1000); wr32(r.data() + 0x18, 1000); wr32(r.data() + 0x20, (int32_t)0xFFFFFFFF);
	wr32(r.data() + 0x28, tex); wr16(r.data() + 0x34, 32); wr16(r.data() + 0x38, 32);
	r[0x3C] = (uint8_t)layer;
	return r;
}
// PAT v3 block: poses x parts, one 256 px texture filled with a pattern
std::vector<uint8_t> MakePat(const std::vector<std::vector<std::vector<uint8_t>>> &poses)
{
	const size_t imageOff = kPoseBase + 3680 * poses.size();
	std::vector<uint8_t> b(imageOff + 11824 + 256 * 256 * 4, 0);
	wr32(b.data(), 3); wr32(b.data() + 4, 0x01234567);
	wr32(b.data() + kImgOffField, (int32_t)imageOff);
	for (size_t p = 0; p < poses.size(); p++) {
		wr32(b.data() + kPoseTab + 4 * p, (int32_t)(kPoseBase + 3680 * p));
		memcpy(b.data() + kPoseTab + 4000 + 32 * p, "POSE", 4);
		for (size_t k = 0; k < poses[p].size() && k < 40; k++) memcpy(b.data() + kPoseBase + 3680 * p + 92 * k, poses[p][k].data(), 92);
	}
	wr32(b.data() + imageOff + 0x18, 1);           // texture count
	wr32(b.data() + imageOff + 0x1C, 11824);
	wr32(b.data() + imageOff + 0xD64, 256);
	for (size_t i = 0; i < 256 * 256 * 4; i++) b[imageOff + 11824 + i] = (uint8_t)(i * 7);
	return b;
}
std::vector<uint8_t> Rgba(int w, int h, int seed)
{
	std::vector<uint8_t> px((size_t)w * h * 4);
	for (size_t i = 0; i < px.size(); i++) px[i] = (uint8_t)(i * 13 + seed);
	return px;
}
} // namespace

static void TestSheetImportSaved()
{
	auto pat = MakePat({ { Part(0, 0, 0), Part(10, 0, 1) } });
	Parts &parts = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	std::string err;
	CHECK(han2::PatToParts(pat.data(), pat.size(), parts, &err));
	std::vector<uint8_t> same;
	CHECK(han2::BuildPat(parts, pat, same, &err) && same == pat);   // unedited: byte-identical
	if (same != pat) { size_t i = 0; while (i < same.size() && i < pat.size() && same[i] == pat[i]) i++; std::fprintf(stderr, "  differs at %zu (sizes %zu / %zu)\n", i, same.size(), pat.size()); }

	// replace sheet 0 and add a 512 sheet (BUG1: before the fix only the GL texture changed and the save kept the old pixels)
	auto a = Rgba(256, 256, 1), b = Rgba(512, 512, 2);
	CHECK(han2::edit::ImportSheetRgba(parts, 0, a.data(), 256, 256, &err));
	CHECK(han2::edit::ImportSheetRgba(parts, (int)parts.gfxMeta.size(), b.data(), 512, 512, &err));
	CHECK(parts.gfxMeta.size() == 2);
	std::vector<uint8_t> out;
	CHECK(han2::BuildPat(parts, pat, out, &err));
	Parts &back = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	CHECK(han2::PatToParts(out.data(), out.size(), back, &err));
	CHECK(back.gfxMeta.size() == 2);
	std::vector<uint8_t> px; int w = 0, h = 0;
	CHECK(han2::edit::SheetRgba(back, 0, px, w, h) && w == 256 && h == 256 && px == a);
	CHECK(han2::edit::SheetRgba(back, 1, px, w, h) && w == 512 && h == 512 && px == b);
	CHECK(back.partSets.size() == 1 && back.partSets[0].groups.size() == 2);   // poses untouched

	// square check (PACNyx: width must equal height) and slot limit
	auto r = Rgba(256, 128, 3);
	CHECK(!han2::edit::ImportSheetRgba(parts, 0, r.data(), 256, 128, &err) && err.find("square") != std::string::npos);

	// in-use count, remove (refused while used, then forced) -> the writer drops the texture
	CHECK(han2::edit::SheetUsers(parts, 0) == 2);
	CHECK(han2::edit::SheetUsers(parts, 1) == 0);
	CHECK(!han2::edit::RemoveSheet(parts, 0, false, &err));
	CHECK(han2::edit::RemoveSheet(parts, 1, false, &err));
	CHECK(han2::BuildPat(parts, pat, out, &err));
	Parts &back2 = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	CHECK(han2::PatToParts(out.data(), out.size(), back2, &err) && back2.gfxMeta.size() == 1);
}

static void TestLimits()
{
	std::vector<std::vector<uint8_t>> full;
	for (int k = 0; k < 40; k++) full.push_back(Part(k, 0, 0));
	auto pat = MakePat({ full, { Part(0, 0, 0) } });
	Parts &parts = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	std::string err;
	CHECK(han2::PatToParts(pat.data(), pat.size(), parts, &err));
	CHECK(han2::edit::MaxPoses(parts) == 1000);
	CHECK(!han2::edit::CanAddPart(parts, 0, &err) && err.find("40") != std::string::npos);
	CHECK(han2::edit::CanAddPart(parts, 1, &err));
	CHECK(han2::edit::AddPartFromSheet(parts, 0, 0, 0, 0, 16, 16, -1, &err) < 0);
	// a model that bypasses the UI: the writer refuses with the limit in the message (shown by the Save error popup, BUG4)
	parts.partSets[0].groups.push_back(parts.partSets[0].groups[0]);
	std::vector<uint8_t> out;
	CHECK(!han2::BuildPat(parts, pat, out, &err) && err.find("at most 40") != std::string::npos);
	parts.partSets[0].groups.pop_back();
	// poses: a name is required; a named empty pose is written and read back
	int at = -1;
	CHECK(!han2::edit::AddPose(parts, "", &at, &err));
	CHECK(han2::edit::AddPose(parts, "NEWPOSE", &at, &err) && at == 2);
	CHECK(!han2::edit::AddPose(parts, "NEWPOSE", &at, &err));
	CHECK(han2::BuildPat(parts, pat, out, &err));
	Parts &back = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	CHECK(han2::PatToParts(out.data(), out.size(), back, &err) && back.partSets.size() == 3 && back.partSets[2].name == "NEWPOSE");
	// GOF2 v4: 2000 poses
	Parts &g = *new Parts(nullptr); g.fbMaxPoses = 2000;
	CHECK(han2::edit::MaxPoses(g) == 2000);
}

static void TestAddPartAndUnits()
{
	auto pat = MakePat({ { Part(0, 0, 3) } });
	Parts &parts = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	std::string err;
	CHECK(han2::PatToParts(pat.data(), pat.size(), parts, &err));
	auto big = Rgba(512, 512, 5);
	CHECK(han2::edit::ImportSheetRgba(parts, 1, big.data(), 512, 512, &err));
	const int slot = han2::edit::AddPartFromSheet(parts, 0, 1, 64, 32, 100, 50, 0, &err);
	CHECK(slot == 1);
	const auto &pr = parts.partSets[0].groups[slot];
	const auto &co = parts.cutOuts[pr.ppId];
	CHECK(co.texture == 1 && co.uv[0] == 32 && co.uv[1] == 16 && co.uv[2] == 50 && co.uv[3] == 25);   // 1/256 units of a 512 sheet
	CHECK(co.wh[0] == 100 && co.wh[1] == 50 && co.xy[0] == 50 && co.xy[1] == 25);
	CHECK(han2::edit::LayerOf(pr) == 2);                         // just below the selected part (layer 3)
	CHECK(han2::edit::AddPartFromSheet(parts, 0, 1, 500, 0, 100, 10, 0, &err) < 0);   // outside the sheet
	std::vector<uint8_t> out; Parts &back = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	CHECK(han2::BuildPat(parts, pat, out, &err) && han2::PatToParts(out.data(), out.size(), back, &err));
	CHECK(back.partSets[0].groups.size() == 2 && back.cutOuts[back.partSets[0].groups[1].ppId].uv[2] == 50);
	// a shared cut-out gets its own copy before a per-part edit
	parts.partSets[0].groups.push_back(parts.partSets[0].groups[0]);
	parts.partSets[0].groups.back().propId = 2;
	const int c0 = parts.partSets[0].groups[0].ppId;
	const int c2 = han2::edit::UniqueCutOut(parts, 0, 2);
	CHECK(c2 != c0 && parts.partSets[0].groups[0].ppId == c0);
	CHECK(han2::edit::UniqueCutOut(parts, 0, 2) == c2);
}

static void TestLayers()
{
	PartSet<> ps;
	ps.groups.resize(4);
	for (int k = 0; k < 4; k++) { ps.groups[k].ppId = 0; ps.groups[k].propId = k; ps.groups[k].priority = (float)k + k / 1000.f; }   // layers 0,1,2,3
	using han2::edit::LayerCmd; using han2::edit::LayerOf;
	CHECK(han2::edit::LayerCommand(ps, 0, LayerCmd::ToFront));
	CHECK(LayerOf(ps.groups[0]) == 4);
	CHECK(han2::edit::LayerCommand(ps, 0, LayerCmd::Backward));   // swap with the part below (layer 3)
	CHECK(LayerOf(ps.groups[0]) == 3 && LayerOf(ps.groups[3]) == 4);
	CHECK(han2::edit::LayerCommand(ps, 3, LayerCmd::ToBack));
	CHECK(LayerOf(ps.groups[3]) == 0);
	CHECK(han2::edit::LayerCommand(ps, 1, LayerCmd::Set, 200) && LayerOf(ps.groups[1]) == 200);
	CHECK(han2::edit::LayerCommand(ps, 2, LayerCmd::Forward));
	// fractional part keeps the slot order (writer reads floor(priority))
	CHECK(ps.groups[2].priority - LayerOf(ps.groups[2]) < 0.01f);
	{
		PartSet<> p2 = ps;
		han2::edit::ShiftPose(p2, 5, -3);
		CHECK(p2.groups[1].x == ps.groups[1].x + 5 && p2.groups[1].y == ps.groups[1].y - 3);
	}
}

static void TestPartsUndo()
{
	auto pat = MakePat({ { Part(0, 0, 0), Part(10, 0, 1) }, { Part(5, 5, 0) } });
	Parts &parts = *new Parts(nullptr);   // never destroyed: ~Parts() calls GL (no context here), as fb_part_transform_test
	std::string err;
	CHECK(han2::PatToParts(pat.data(), pat.size(), parts, &err));
	std::vector<Sequence> doc(1);
	UndoManager u;
	u.attach(&doc);
	PartsUndoChannel ch(&parts);
	int restoredSheets = 0;
	ch.onSheetsRestored = [&] { restoredSheets++; };
	u.addChannel(&ch);
	u.ensureBaseline();
	u.markClean();
	CHECK(u.isClean());
	// a drag on the part position: one step at the end of the gesture
	for (int v = 1; v <= 5; v++) { parts.partSets[0].groups[1].x = 10 + v; u.endFrame(true); }
	CHECK(u.undoCount() == 0);
	u.endFrame(false);
	CHECK(u.undoCount() == 1 && !u.isClean());
	CHECK(u.peekUndo()->changes.empty() && u.peekUndo()->channels.size() == 1);
	// sheet import: another step; the undo restores the sheet record and asks for a GL re-upload
	auto px = Rgba(256, 256, 9);
	const char *before = parts.gfxMeta[0].data;
	CHECK(han2::edit::ImportSheetRgba(parts, 0, px.data(), 256, 256, &err));
	u.markModified(); u.endFrame(false);
	CHECK(u.undoCount() == 2);
	CHECK(u.undo());
	CHECK(parts.gfxMeta[0].data == before && restoredSheets == 1);
	CHECK(u.undo());
	CHECK(parts.partSets[0].groups[1].x == 10 && u.isClean());
	CHECK(u.redo());
	CHECK(parts.partSets[0].groups[1].x == 15);
	CHECK(u.redo());
	CHECK(parts.gfxMeta[0].data != before && restoredSheets == 2);
	// unchanged poses are shared between snapshots (memory grows with what changed)
	CHECK(u.historyBytes() > 0);
	// an inactive channel records nothing
	bool on = false;
	ch.enabled = [&] { return on; };
	parts.partSets[1].groups[0].y = 99;
	u.markModified(); u.endFrame(false);
	CHECK(u.undoCount() == 2);
	// frames and parts in one gesture are one entry
	on = true;
	u.markModified(); u.endFrame(false);   // the channel re-captures its baseline when it becomes active (no entry)
	CHECK(u.undoCount() == 2);
	doc[0].name = "edited";
	parts.partSets[1].groups[0].y = 100;
	u.markModified(); u.endFrame(false);
	CHECK(u.peekUndo() && u.peekUndo()->changes.size() == 1 && u.peekUndo()->channels.size() == 1);
	CHECK(u.undo() && doc[0].name.empty() && parts.partSets[1].groups[0].y == 99);
	u.removeChannel(&ch);
}

int main()
{
	TestSheetImportSaved();
	TestLimits();
	TestAddPartAndUnits();
	TestLayers();
	TestPartsUndo();
	std::printf("han2_parts_edit_test: %d checks, %d failures\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
