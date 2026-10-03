// Parts model -> French-Bread PAT v3/v4 block. Companion of han2_pat.cpp (reader); layout in docs/formats/ida/rbo_scripts_pat.md.
//
// The block is rebuilt from the ORIGINAL blob plus the model: header, names, the 92-byte part records and the image head are
// copied from the original and only the fields whose model value differs from what the original bytes decode to are
// re-encoded, so an unedited character rebuilds byte-identically (including fields Hantei-chan has no model for).
#include "han2_pat.h"
#include "parts/parts.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace han2 {

namespace {
constexpr size_t kHeader = 24, kPart = 92, kPartsPerPose = 40, kPose = kPart * kPartsPerPose, kImageHead = 11824;
constexpr int kMaxTex = 50;

inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
inline void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }
inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }

// the field values a part record decodes to (same rules as PatToParts)
struct Dec {
	int x, y, dw, dh, flip, sxp, syp, rot, tex, sx, sy, sw, sh, layer, ox, oy;
	uint8_t bgra[4]; uint8_t add[3];
};
Dec DecodeRec(const uint8_t *r)
{
	Dec d{};
	d.x = rd32(r); d.y = rd32(r + 4); d.dw = rd32(r + 8); d.dh = rd32(r + 12); d.flip = rd32(r + 0x10) & 3;
	d.sxp = rd32(r + 0x14); d.syp = rd32(r + 0x18); d.rot = rd32(r + 0x1C);
	d.bgra[0] = r[0x23]; d.bgra[1] = r[0x22]; d.bgra[2] = r[0x21]; d.bgra[3] = r[0x20];
	d.add[0] = r[0x26]; d.add[1] = r[0x25]; d.add[2] = r[0x24];
	d.tex = rd32(r + 0x28); d.sx = rd16(r + 0x2C); d.sy = rd16(r + 0x30); d.sw = rd16(r + 0x34); d.sh = rd16(r + 0x38);
	d.layer = r[0x3C]; d.ox = rd32(r + 0x40); d.oy = rd32(r + 0x44);
	return d;
}

// the record the model asks for (floats converted back the way the reader converted them)
Dec FromModel(const PartProperty &pr, const CutOut<> &co)
{
	Dec d{};
	d.x = pr.x; d.y = pr.y; d.dw = co.wh[0]; d.dh = co.wh[1]; d.flip = pr.flip & 3;
	d.sxp = (int)lroundf(pr.scaleX * 1000.f); d.syp = (int)lroundf(pr.scaleY * 1000.f);
	d.rot = (int)lroundf(pr.rotation[3] * 10000.f);
	memcpy(d.bgra, pr.bgra, 4);
	d.add[0] = (uint8_t)lroundf(std::clamp(pr.addColor[0], 0.f, 1.f) * 255.f);
	d.add[1] = (uint8_t)lroundf(std::clamp(pr.addColor[1], 0.f, 1.f) * 255.f);
	d.add[2] = (uint8_t)lroundf(std::clamp(pr.addColor[2], 0.f, 1.f) * 255.f);
	d.tex = co.texture; d.sx = co.uv[0]; d.sy = co.uv[1]; d.sw = co.uv[2]; d.sh = co.uv[3];
	d.layer = (int)floorf(pr.priority + 1e-4f); d.ox = co.xy[0]; d.oy = co.xy[1];
	return d;
}

void Blank(uint8_t *r)
{
	memset(r, 0, kPart);
	wr32(r + 0x14, 1000); wr32(r + 0x18, 1000); wr32(r + 0x20, (int32_t)0xFFFFFFFF);
}

bool Same(const Dec &a, const Dec &b) { return memcmp(&a, &b, sizeof(Dec)) == 0; }

// write the fields of `n` over record `r`, touching only what differs from `o`
void Apply(uint8_t *r, const Dec &n, const Dec &o)
{
	if (n.x != o.x) wr32(r, n.x);
	if (n.y != o.y) wr32(r + 4, n.y);
	if (n.dw != o.dw) wr32(r + 8, n.dw);
	if (n.dh != o.dh) wr32(r + 12, n.dh);
	if (n.flip != o.flip) wr32(r + 0x10, (rd32(r + 0x10) & ~3) | n.flip);
	if (n.sxp != o.sxp) wr32(r + 0x14, n.sxp);
	if (n.syp != o.syp) wr32(r + 0x18, n.syp);
	if (n.rot != o.rot) wr32(r + 0x1C, n.rot);
	if (memcmp(n.bgra, o.bgra, 4)) { r[0x23] = n.bgra[0]; r[0x22] = n.bgra[1]; r[0x21] = n.bgra[2]; r[0x20] = n.bgra[3]; }
	if (memcmp(n.add, o.add, 3)) { r[0x26] = n.add[0]; r[0x25] = n.add[1]; r[0x24] = n.add[2]; }
	if (n.tex != o.tex) wr32(r + 0x28, n.tex);
	if (n.sx != o.sx) wr16(r + 0x2C, n.sx);
	if (n.sy != o.sy) wr16(r + 0x30, n.sy);
	if (n.sw != o.sw) wr16(r + 0x34, n.sw);
	if (n.sh != o.sh) wr16(r + 0x38, n.sh);
	if (n.layer != o.layer) r[0x3C] = (uint8_t)n.layer;
	if (n.ox != o.ox) wr32(r + 0x40, n.ox);
	if (n.oy != o.oy) wr32(r + 0x44, n.oy);
}

std::string FixedSJ(const std::string &utf8, size_t n)
{
	std::string sj = utf82sj(utf8);
	size_t m = std::min(sj.size(), n - 1), i = 0;
	while (i < m) {
		unsigned char c = (unsigned char)sj[i];
		size_t w = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1;
		if (i + w > m) break;
		i += w;
	}
	return sj.substr(0, i);
}
} // namespace

bool BuildPat(const Parts &parts, const std::vector<uint8_t> &orig, std::vector<uint8_t> &out, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (!IsPat(orig.data(), orig.size())) return fail("original PAT block missing");
	const int nPoses = rd32(orig.data()) == 4 ? 2000 : 1000;
	const size_t offsTab = kHeader, namesTab = kHeader + 4 * (size_t)nPoses;
	const size_t imgOffField = namesTab + 32 * (size_t)nPoses, poseBase = imgOffField + 4;
	const size_t imageOff = (size_t)(uint32_t)rd32(orig.data() + imgOffField);
	if (imageOff < poseBase || imageOff + kImageHead > orig.size()) return fail("bad original PAT block");

	// original pose blocks by pose index
	std::vector<const uint8_t *> origPose(nPoses, nullptr);
	for (int p = 0; p < nPoses; p++) {
		uint32_t off = (uint32_t)rd32(orig.data() + offsTab + 4 * p);
		if (off) origPose[p] = orig.data() + off;
	}
	// poses to write: every original pose, plus model pose sets that gained parts
	std::vector<std::vector<uint8_t>> pose(nPoses);
	std::vector<bool> present(nPoses, false);
	for (int p = 0; p < nPoses; p++) {
		const PartSet<> *ps = p < (int)parts.partSets.size() ? &parts.partSets[p] : nullptr;
		bool modelHas = false;
		if (ps) for (auto &g : ps->groups) if (g.ppId >= 0) modelHas = true;
		if (!origPose[p] && !modelHas) continue;
		if (origPose[p] && ps && !ps->wasLoaded && !modelHas) { /* original pose the model cleared: keep it blank */ }
		present[p] = true;
		pose[p].assign(kPose, 0);
		for (size_t k = 0; k < kPartsPerPose; k++) {
			uint8_t *r = pose[p].data() + kPart * k;
			if (origPose[p]) memcpy(r, origPose[p] + kPart * k, kPart); else Blank(r);
			const Dec od = DecodeRec(r);
			const PartProperty *pr = (ps && k < ps->groups.size() && ps->groups[k].ppId >= 0) ? &ps->groups[k] : nullptr;
			const bool origUsed = rd32(r + 0x34) != 0 && (uint32_t)rd32(r + 0x28) < (uint32_t)parts.gfxMeta.size();   // other records (clip parts, bogus texture ids) have no model part and are kept verbatim
			if (!pr) {
				if (origUsed && ps) Blank(r);          // part removed in the editor (a clip part, which the model never holds, is kept)
				continue;
			}
			if (pr->ppId >= (int)parts.cutOuts.size()) return fail("part refers to a missing cutout");
			Dec nd = FromModel(*pr, parts.cutOuts[pr->ppId]);
			if (origUsed) Apply(r, nd, od);
			else { Blank(r); Apply(r, nd, DecodeRec(r)); }
		}
	}
	// ---- image area: head + textures ----
	std::vector<uint8_t> head(orig.begin() + imageOff, orig.begin() + imageOff + kImageHead);
	std::vector<std::vector<uint8_t>> tex(kMaxTex);
	for (int i = 0; i < kMaxTex; i++) {
		uint32_t off = (uint32_t)rd32(head.data() + 0x1C + 4 * i); int res = rd32(head.data() + 0xD64 + 4 * i);
		if (off && res > 0) {
			size_t bytes = (size_t)res * res * 4;
			if (imageOff + off + bytes > orig.size()) return fail("original texture runs past the block");
			tex[i].assign(orig.begin() + imageOff + off, orig.begin() + imageOff + off + bytes);
		}
	}
	for (size_t i = 0; i < parts.gfxMeta.size() && i < (size_t)kMaxTex; i++) {
		const auto &g = parts.gfxMeta[i];
		if (!g.data || g.w <= 0 || g.w != g.h) continue;
		size_t bytes = (size_t)g.w * g.h * 4;
		if (tex[i].empty()) {                           // new texture slot
			tex[i].assign((const uint8_t *)g.data, (const uint8_t *)g.data + bytes);
			wr32(head.data() + 0xD64 + 4 * i, g.w);
		} else if (tex[i].size() == bytes) {
			memcpy(tex[i].data(), g.data, bytes);
		} else {                                        // resized texture
			tex[i].assign((const uint8_t *)g.data, (const uint8_t *)g.data + bytes);
			wr32(head.data() + 0xD64 + 4 * i, g.w);
		}
	}
	size_t cur = kImageHead; int count = 0;
	for (int i = 0; i < kMaxTex; i++) {
		if (tex[i].empty()) { wr32(head.data() + 0x1C + 4 * i, 0); continue; }
		wr32(head.data() + 0x1C + 4 * i, (int32_t)cur); cur += tex[i].size(); count++;
	}
	wr32(head.data() + 0x18, count);

	// ---- assemble ----
	out.assign(orig.begin(), orig.begin() + kHeader);
	std::vector<uint8_t> offs(4 * (size_t)nPoses, 0), names(32 * (size_t)nPoses, 0);
	size_t rank = 0;
	for (int p = 0; p < nPoses; p++) {
		if (!present[p]) continue;
		wr32(offs.data() + 4 * p, (int32_t)(poseBase + kPose * rank)); rank++;
	}
	for (int p = 0; p < nPoses; p++) {
		memcpy(names.data() + 32 * (size_t)p, orig.data() + namesTab + 32 * (size_t)p, 32);
		if (p < (int)parts.partSets.size() && (parts.partSets[p].wasLoaded || present[p])) {   // names of absent poses stay as stored
			const std::string &nm = parts.partSets[p].name;
			char cur32[33]{}; memcpy(cur32, names.data() + 32 * (size_t)p, 32);
			if (sj2utf8(std::string(cur32)) != nm) { std::string sj = FixedSJ(nm, 32); memset(names.data() + 32 * (size_t)p, 0, 32); memcpy(names.data() + 32 * (size_t)p, sj.data(), sj.size()); }
		}
	}
	out.insert(out.end(), offs.begin(), offs.end());
	out.insert(out.end(), names.begin(), names.end());
	uint8_t imgf[4]; wr32(imgf, (int32_t)(poseBase + kPose * rank)); out.insert(out.end(), imgf, imgf + 4);
	for (int p = 0; p < nPoses; p++) if (present[p]) out.insert(out.end(), pose[p].begin(), pose[p].end());
	out.insert(out.end(), head.begin(), head.end());
	for (int i = 0; i < kMaxTex; i++) out.insert(out.end(), tex[i].begin(), tex[i].end());
	return true;
}

} // namespace han2
