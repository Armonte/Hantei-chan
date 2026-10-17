// Appending part sets of one .pat to another. See pat_append.h.
#include "pat_append.h"
#include "pat_texconv.h"
#include "parts.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace patrn {

bool ParseIdList(const std::string &s, std::vector<int> &ids, std::string *err)
{
	size_t p = 0;
	while (p <= s.size()) {
		size_t c = s.find(',', p); if (c == std::string::npos) c = s.size();
		const std::string t = s.substr(p, c - p);
		char *e = nullptr; const long a = strtol(t.c_str(), &e, 10); long b = a;
		if (e == t.c_str() || a < 0) { if (err) *err = "bad id list '" + s + "' (expected e.g. 1,5,7-9)"; return false; }
		if (*e == '-') { const char *q = e + 1; b = strtol(q, &e, 10); if (e == q || b < a) { if (err) *err = "bad range '" + t + "'"; return false; } }
		if (*e) { if (err) *err = "bad id list '" + s + "' (expected e.g. 1,5,7-9)"; return false; }
		for (long i = a; i <= b; i++) ids.push_back((int)i);
		p = c + 1;
	}
	std::sort(ids.begin(), ids.end()); ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	return true;
}

static bool SetEmpty(const PartSet<> &ps) { return ps.groups.empty() && ps.name.empty(); }

static bool ShapeEq(const Shape<> &a, const Shape<> &b)
{
	return a.type == b.type && a.vertexCount == b.vertexCount && a.vertexCount2 == b.vertexCount2 && a.length == b.length &&
		a.length2 == b.length2 && a.radius == b.radius && a.dRadius == b.dRadius && a.width == b.width && a.dz == b.dz &&
		a.rectW == b.rectW && a.rectH == b.rectH && a.rectX == b.rectX && a.rectY == b.rectY &&
		std::string(a.name.data(), a.name.size()) == std::string(b.name.data(), b.name.size());
}

// every field but the id and the (render-only) VAO index
static bool CutOutEq(const CutOut<> &a, const CutOut<> &b)
{
	return std::string(a.name.data(), a.name.size()) == std::string(b.name.data(), b.name.size()) &&
		!memcmp(a.uv, b.uv, sizeof a.uv) && !memcmp(a.xy, b.xy, sizeof a.xy) && !memcmp(a.wh, b.wh, sizeof a.wh) &&
		a.texture == b.texture && !memcmp(a.ppjp, b.ppjp, sizeof a.ppjp) && a.colorSlot == b.colorSlot &&
		a.shapeIndex == b.shapeIndex && !memcmp(a.ppte, b.ppte, sizeof a.ppte) && a.pptx == b.pptx &&
		a.altTags == b.altTags && a.nameFixed == b.nameFixed;
}

bool AppendPartSets(Parts &dst, const Parts &src, const std::vector<int> &srcIdsIn, AppendResult &res, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	res = AppendResult();
	std::vector<int> ids = srcIdsIn;
	if (ids.empty()) for (size_t i = 0; i < src.partSets.size(); i++) if (!SetEmpty(src.partSets[i])) ids.push_back((int)i);
	for (int id : ids) if (id < 0 || id >= (int)src.partSets.size()) return fail("part set " + std::to_string(id) + " is not in the source (0-" + std::to_string((int)src.partSets.size() - 1) + ")");

	const bool mbaacc = dst.useMBAACCFormat;
	bool dstPgtp = false;
	for (auto &g : dst.gfxMeta) dstPgtp |= g.pgtp != 0;

	// --- textures (by what the source cut-outs used by the requested sets draw) ---
	std::map<int, int> texMap, cutMap, shapeMap;
	std::map<int, std::vector<uint8_t>> dstRgba;   // decoded lazily, for reuse
	auto mapTexture = [&](int t) -> int {
		auto it = texMap.find(t); if (it != texMap.end()) return it->second;
		if (t < 0 || t >= (int)src.gfxMeta.size()) { res.notes.push_back("a cut-out draws texture " + std::to_string(t) + ", which the source does not have: kept as is"); return texMap[t] = t; }
		const PartGfx<> &sg = src.gfxMeta[t];
		std::vector<uint8_t> rgba; int w = 0, h = 0; std::string e;
		if (!pattex::Decode(sg, rgba, w, h, &e)) { res.notes.push_back("source texture " + std::to_string(t) + ": " + e + " (not copied)"); return texMap[t] = -1; }
		for (size_t k = 0; k < dst.gfxMeta.size(); k++) {
			const PartGfx<> &dg = dst.gfxMeta[k];
			if (dg.w != w || dg.h != h) continue;
			auto c = dstRgba.find((int)k);
			if (c == dstRgba.end()) { std::vector<uint8_t> px; int dw, dh; if (!pattex::Decode(dg, px, dw, dh, nullptr)) px.clear(); c = dstRgba.emplace((int)k, std::move(px)).first; }
			if (c->second == rgba) { res.texturesReused++; return texMap[t] = (int)k; }
		}
		PartGfx<> g;
		g.id = (int)dst.gfxMeta.size();
		g.name = sg.name.empty() ? ("tex" + std::to_string(g.id)).c_str() : sg.name;   // a texture without a name is not saved
		if (mbaacc) {
			char *px = dst.AdoptPixels((size_t)w * h * 4);
			for (size_t i = 0; i < (size_t)w * h; i++) { px[i * 4] = (char)rgba[i * 4 + 2]; px[i * 4 + 1] = (char)rgba[i * 4 + 1]; px[i * 4 + 2] = (char)rgba[i * 4]; px[i * 4 + 3] = (char)rgba[i * 4 + 3]; }
			g.data = px; g.w = w; g.h = h; g.bpp = 32; g.imageSize = 0;
			g.pgtp = sg.pgtp ? sg.pgtp : dstPgtp ? w : 0;
		} else if (!pattex::Encode(g, rgba.data(), w, h, 21, &e)) {
			res.notes.push_back("source texture " + std::to_string(t) + ": " + e + " (not copied)"); return texMap[t] = -1;
		}
		dst.gfxMeta.push_back(g);
		dstRgba[g.id] = std::move(rgba);
		res.texturesAdded++;
		return texMap[t] = g.id;
	};
	auto mapShape = [&](int s) -> int {
		auto it = shapeMap.find(s); if (it != shapeMap.end()) return it->second;
		Shape<> sh;   // a .pat without shapes (MBAACC) draws every cut-out as a plane
		if (s >= 0 && s < (int)src.shapes.size()) sh = src.shapes[s];
		if (mbaacc && dst.shapes.empty()) {
			if (sh.type != ShapeType::PLANE) res.notes.push_back("source shape " + std::to_string(s) + " is not a plane: MBAACC has no shapes, its cut-outs draw as planes");
			return shapeMap[s] = 0;
		}
		for (size_t k = 0; k < dst.shapes.size(); k++)
			if (src.shapes.empty() ? dst.shapes[k].type == ShapeType::PLANE : ShapeEq(dst.shapes[k], sh)) { res.shapesReused++; return shapeMap[s] = (int)k; }
		sh.id = (int)dst.shapes.size();
		dst.shapes.push_back(sh);
		res.shapesAdded++;
		return shapeMap[s] = sh.id;
	};
	auto mapCutOut = [&](int c) -> int {
		auto it = cutMap.find(c); if (it != cutMap.end()) return it->second;
		if (c < 0 || c >= (int)src.cutOuts.size()) return cutMap[c] = -1;   // draws nothing in the source either
		CutOut<> co = src.cutOuts[c];
		co.texture = mapTexture(co.texture);
		co.shapeIndex = mapShape(co.shapeIndex);
		co.vaoIndex = 0;
		for (size_t k = 0; k < dst.cutOuts.size(); k++) if (CutOutEq(dst.cutOuts[k], co)) { res.cutOutsReused++; return cutMap[c] = (int)k; }
		co.id = (int)dst.cutOuts.size();
		dst.cutOuts.push_back(co);
		res.cutOutsAdded++;
		return cutMap[c] = co.id;
	};

	// --- part sets ---
	for (int id : ids) {
		const PartSet<> &s = src.partSets[id];
		if (SetEmpty(s)) { res.skippedEmpty.push_back(id); continue; }
		PartSet<> ps;
		ps.partId = (int)dst.partSets.size();
		ps.origId = -1;   // not on disk: Parts::Save encodes it, the destination's own records keep their bytes
		ps.name = s.name;
		ps.groups = s.groups;
		for (auto &pr : ps.groups) if (pr.ppId >= 0) pr.ppId = mapCutOut(pr.ppId);
		dst.partSets.push_back(std::move(ps));
		res.setMap.push_back({id, (int)dst.partSets.size() - 1});
	}
	return true;
}

} // namespace patrn
