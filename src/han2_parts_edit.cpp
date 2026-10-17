// PACNyx-style edits of a French-Bread parts model. See han2_parts_edit.h.
#include "han2_parts_edit.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace han2 {
namespace edit {

namespace {
bool Used(const PartProperty &p) { return p.ppId >= 0; }
bool PoseEmpty(const PartSet<> &ps)
{
	if (ps.wasLoaded || !ps.name.empty()) return false;
	for (auto &g : ps.groups) if (Used(g)) return false;
	return true;
}
void SetLayer(PartProperty &p, int slot, int layer)
{
	layer = std::clamp(layer, 0, 255);
	p.priority = (float)layer + slot / 1000.f;   // same encoding as PatToParts: layer + slot order
}
} // namespace

int PxToUnits(int px, int sheetSize) { return sheetSize > 0 ? (int)lround(px * 256.0 / sheetSize) : px; }
int UnitsToPx(int units, int sheetSize) { return sheetSize > 0 ? (int)lround(units * sheetSize / 256.0) : units; }

int LayerOf(const PartProperty &p) { return std::clamp((int)std::floor(p.priority + 1e-4f), 0, 255); }

int MaxPoses(const Parts &parts) { return parts.fbMaxPoses > 0 ? parts.fbMaxPoses : 1000; }

int UsedPartSlots(const PartSet<> &ps)
{
	int used = 0;
	for (size_t k = 0; k < ps.groups.size(); k++) if (Used(ps.groups[k])) used = (int)k + 1;
	return used;
}

int FreePartSlot(const PartSet<> &ps)
{
	for (int k = 0; k < kPartsPerPose; k++) if (k >= (int)ps.groups.size() || !Used(ps.groups[k])) return k;
	return -1;
}

bool CanAddPart(const Parts &parts, int pose, std::string *why)
{
	if (pose < 0 || pose >= (int)parts.partSets.size()) { if (why) *why = "no pose selected"; return false; }
	if (FreePartSlot(parts.partSets[pose]) < 0) {
		if (why) *why = "This pose already has 40 parts, the most the game reads per pose.";
		return false;
	}
	return true;
}

bool CanAddPose(const Parts &parts, std::string *why)
{
	const int maxP = MaxPoses(parts);
	if ((int)parts.partSets.size() < maxP) return true;
	for (auto &ps : parts.partSets) if (PoseEmpty(ps)) return true;
	if (why) *why = "All " + std::to_string(maxP) + " pose slots of this PAT version are in use.";
	return false;
}

bool ImportSheetRgba(Parts &parts, int slot, const uint8_t *rgba, int w, int h, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (!rgba || w <= 0 || h <= 0) return fail("the image is empty");
	if (w != h) return fail("A sprite sheet must be square (the width must equal the height): this image is " + std::to_string(w) + " x " + std::to_string(h) + ".");
	if (w > kMaxSheetSize) return fail("A sprite sheet can be at most " + std::to_string(kMaxSheetSize) + " x " + std::to_string(kMaxSheetSize) + " pixels.");
	if (slot < 0 || slot > (int)parts.gfxMeta.size()) return fail("no such sprite sheet slot");
	if (slot >= kMaxSheets) return fail("The PAT holds at most " + std::to_string(kMaxSheets) + " sprite sheets.");
	if (slot == (int)parts.gfxMeta.size()) { parts.gfxMeta.emplace_back(); parts.gfxMeta.back().id = slot; }
	const size_t bytes = (size_t)w * h * 4;
	char *dst = parts.AdoptPixels(bytes);
	for (size_t i = 0; i < (size_t)w * h; i++) {   // the PAT stores B,G,R,A
		dst[i * 4 + 0] = (char)rgba[i * 4 + 2]; dst[i * 4 + 1] = (char)rgba[i * 4 + 1];
		dst[i * 4 + 2] = (char)rgba[i * 4 + 0]; dst[i * 4 + 3] = (char)rgba[i * 4 + 3];
	}
	auto &g = parts.gfxMeta[slot];
	g.id = slot;
	if (g.name.empty()) g.name = "tex" + std::to_string(slot);
	g.data = dst; g.w = g.h = w; g.bpp = 32;
	g.type = 0; g.imageSize = 0; g.s3tc = nullptr; g.imageData = nullptr;
	return true;
}

bool SheetRgba(const Parts &parts, int slot, std::vector<uint8_t> &rgba, int &w, int &h)
{
	if (slot < 0 || slot >= (int)parts.gfxMeta.size()) return false;
	const auto &g = parts.gfxMeta[slot];
	if (!g.data || g.w <= 0 || g.h <= 0 || g.bpp != 32) return false;
	w = g.w; h = g.h;
	rgba.resize((size_t)w * h * 4);
	const uint8_t *s = (const uint8_t *)g.data;
	for (size_t i = 0; i < (size_t)w * h; i++) {
		rgba[i * 4 + 0] = s[i * 4 + 2]; rgba[i * 4 + 1] = s[i * 4 + 1]; rgba[i * 4 + 2] = s[i * 4 + 0]; rgba[i * 4 + 3] = s[i * 4 + 3];
	}
	return true;
}

int SheetUsers(const Parts &parts, int slot, std::vector<int> *poses)
{
	int n = 0;
	std::set<int> ps;
	for (size_t p = 0; p < parts.partSets.size(); p++)
		for (auto &g : parts.partSets[p].groups)
			if (Used(g) && g.ppId < (int)parts.cutOuts.size() && parts.cutOuts[g.ppId].texture == slot) { n++; ps.insert((int)p); }
	if (poses) poses->assign(ps.begin(), ps.end());
	return n;
}

bool RemoveSheet(Parts &parts, int slot, bool force, std::string *err)
{
	if (slot < 0 || slot >= (int)parts.gfxMeta.size()) { if (err) *err = "no such sprite sheet"; return false; }
	const int users = SheetUsers(parts, slot);
	if (users && !force) { if (err) *err = std::to_string(users) + " parts use this sprite sheet."; return false; }
	auto &g = parts.gfxMeta[slot];
	const int id = g.id;
	g = {};
	g.id = id;   // an empty slot: the writer drops the texture, the other sheets keep their indices
	return true;
}

bool AddPose(Parts &parts, const std::string &name, int *newIndex, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (name.empty()) return fail("A pose needs a name.");
	for (auto &ps : parts.partSets) if (ps.name == name && (ps.wasLoaded || !PoseEmpty(ps))) return fail("A pose named '" + name + "' already exists.");
	const int maxP = MaxPoses(parts);
	int at = -1;
	for (size_t p = 0; p < parts.partSets.size(); p++) if (PoseEmpty(parts.partSets[p])) { at = (int)p; break; }
	if (at < 0) {
		if ((int)parts.partSets.size() >= maxP) return fail("All " + std::to_string(maxP) + " pose slots of this PAT version are in use.");
		at = (int)parts.partSets.size();
		parts.partSets.emplace_back();
	}
	auto &ps = parts.partSets[at];
	ps.partId = at;
	ps.name = name;
	ps.groups.clear();
	if (newIndex) *newIndex = at;
	return true;
}

bool SwapPoses(Parts &parts, int a, int b, std::string *err)
{
	const int n = (int)parts.partSets.size();
	if (a < 0 || b < 0 || a >= MaxPoses(parts) || b >= MaxPoses(parts)) { if (err) *err = "pose index out of range"; return false; }
	if (a == b) return true;
	if (std::max(a, b) >= n) parts.partSets.resize(std::max(a, b) + 1);
	for (int i = n; i < (int)parts.partSets.size(); i++) parts.partSets[i].partId = i;
	auto &A = parts.partSets[a], &B = parts.partSets[b];
	const bool la = A.wasLoaded, lb = B.wasLoaded;   // "came from the file" belongs to the slot, not the content
	std::swap(A.name, B.name);
	std::swap(A.groups, B.groups);
	A.wasLoaded = la; B.wasLoaded = lb;
	A.partId = a; B.partId = b;
	return true;
}

void ShiftPose(PartSet<> &ps, int dx, int dy)
{
	for (auto &g : ps.groups) if (Used(g)) { g.x += dx; g.y += dy; }
}

int UniqueCutOut(Parts &parts, int pose, int slot)
{
	if (pose < 0 || pose >= (int)parts.partSets.size()) return -1;
	auto &ps = parts.partSets[pose];
	if (slot < 0 || slot >= (int)ps.groups.size() || !Used(ps.groups[slot])) return -1;
	const int c = ps.groups[slot].ppId;
	if (c >= (int)parts.cutOuts.size()) return -1;
	int users = 0;
	for (auto &s : parts.partSets) for (auto &g : s.groups) if (g.ppId == c) users++;
	if (users <= 1) return c;
	CutOut<> copy = parts.cutOuts[c];
	copy.id = (int)parts.cutOuts.size();
	copy.name = "c" + std::to_string(copy.id);
	parts.cutOuts.push_back(copy);
	ps.groups[slot].ppId = copy.id;
	return copy.id;
}

int AddPartFromSheet(Parts &parts, int pose, int sheet, int sx, int sy, int sw, int sh, int belowSlot, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return -1; };
	if (!CanAddPart(parts, pose, err)) return -1;
	if (sheet < 0 || sheet >= (int)parts.gfxMeta.size() || !parts.gfxMeta[sheet].data) return fail("Pick a sprite sheet that has an image.");
	const auto &g = parts.gfxMeta[sheet];
	if (sw <= 0 || sh <= 0) return fail("Select a rectangle on the sprite sheet first.");
	if (sx < 0 || sy < 0 || sx + sw > g.w || sy + sh > g.h) return fail("The rectangle is outside the sprite sheet.");
	auto &ps = parts.partSets[pose];
	const int slot = FreePartSlot(ps);
	if ((int)ps.groups.size() <= slot) {
		const size_t old = ps.groups.size();
		ps.groups.resize(slot + 1);
		for (size_t k = old; k < ps.groups.size(); k++) ps.groups[k].propId = (int)k;
	}
	CutOut<> co;
	co.id = (int)parts.cutOuts.size();
	co.name = "c" + std::to_string(co.id);
	co.texture = sheet;
	// source rect in 1/256 of the sheet (the PAT's unit: a 512 sheet has 2 px per unit)
	co.uv[0] = PxToUnits(sx, g.w); co.uv[1] = PxToUnits(sy, g.h); co.uv[2] = PxToUnits(sw, g.w); co.uv[3] = PxToUnits(sh, g.h);
	co.wh[0] = sw; co.wh[1] = sh;
	co.xy[0] = sw / 2; co.xy[1] = sh / 2;
	parts.cutOuts.push_back(co);
	PartProperty pr{};
	pr.propId = slot;
	pr.ppId = co.id;
	pr.x = -sw / 2; pr.y = -sh;          // standing on the base point, centred on it
	// layer: just below the selected part (PACNyx: selected layer - 1, the others move up when that layer is taken)
	int layer = 0;
	if (belowSlot >= 0 && belowSlot < (int)ps.groups.size() && Used(ps.groups[belowSlot])) layer = std::max(LayerOf(ps.groups[belowSlot]) - 1, 0);
	bool taken = false;
	for (size_t k = 0; k < ps.groups.size(); k++) if (Used(ps.groups[k]) && LayerOf(ps.groups[k]) == layer) taken = true;
	if (taken) for (size_t k = 0; k < ps.groups.size(); k++) if (Used(ps.groups[k])) SetLayer(ps.groups[k], (int)k, LayerOf(ps.groups[k]) + (LayerOf(ps.groups[k]) >= layer ? 1 : 0));
	SetLayer(pr, slot, layer);
	ps.groups[slot] = pr;
	return slot;
}

bool LayerCommand(PartSet<> &ps, int slot, LayerCmd cmd, int value)
{
	if (slot < 0 || slot >= (int)ps.groups.size() || !Used(ps.groups[slot])) return false;
	auto key = [&](int k) { return LayerOf(ps.groups[k]) * 256 + k; };   // the engine's draw-order key
	PartProperty &me = ps.groups[slot];
	switch (cmd) {
	case LayerCmd::Set:
		SetLayer(me, slot, value);
		return true;
	case LayerCmd::ToFront: {
		int top = -1;
		for (size_t k = 0; k < ps.groups.size(); k++) if ((int)k != slot && Used(ps.groups[k])) top = std::max(top, LayerOf(ps.groups[k]));
		if (top < 255) { SetLayer(me, slot, std::max(top + 1, LayerOf(me))); return true; }
		for (size_t k = 0; k < ps.groups.size(); k++) if ((int)k != slot && Used(ps.groups[k])) SetLayer(ps.groups[k], (int)k, LayerOf(ps.groups[k]) - 1);   // make room
		SetLayer(me, slot, 255);
		return true;
	}
	case LayerCmd::ToBack: {
		int bottom = 256;
		for (size_t k = 0; k < ps.groups.size(); k++) if ((int)k != slot && Used(ps.groups[k])) bottom = std::min(bottom, LayerOf(ps.groups[k]));
		if (bottom > 0) { SetLayer(me, slot, std::min(bottom - 1, LayerOf(me))); return true; }
		for (size_t k = 0; k < ps.groups.size(); k++) if ((int)k != slot && Used(ps.groups[k])) SetLayer(ps.groups[k], (int)k, LayerOf(ps.groups[k]) + 1);
		SetLayer(me, slot, 0);
		return true;
	}
	case LayerCmd::Forward:
	case LayerCmd::Backward: {
		// swap layers with the neighbour in draw order (PACNyx push back / pull forward)
		const bool fwd = cmd == LayerCmd::Forward;
		int best = -1;
		for (size_t k = 0; k < ps.groups.size(); k++) {
			if ((int)k == slot || !Used(ps.groups[k])) continue;
			const int kk = key((int)k);
			if (fwd ? kk > key(slot) : kk < key(slot))
				if (best < 0 || (fwd ? kk < key(best) : kk > key(best))) best = (int)k;
		}
		if (best < 0) return false;
		const int la = LayerOf(me), lb = LayerOf(ps.groups[best]);
		if (la != lb) { SetLayer(me, slot, lb); SetLayer(ps.groups[best], best, la); return true; }
		// same layer: the slot decides; move one layer past the neighbour
		SetLayer(me, slot, fwd ? std::min(la + 1, 255) : std::max(la - 1, 0));
		return true;
	}
	}
	return false;
}

} // namespace edit
} // namespace han2
