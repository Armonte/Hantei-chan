#include "ha4_parts.h"
#include "parts/parts.h"
#include "background/bg_pat.h"
#include "misc.h"

#include <cstring>
#include <vector>

namespace ha4 {

static constexpr size_t kPartNamesOff = 8040;   // 1000 x 32-byte pattern names (hantei4 LoadPartsFile 0x40F170)

static std::string FixedStr(const uint8_t *p, size_t n)
{
	size_t len = 0;
	while (len < n && p[len]) len++;
	return sj2utf8(std::string((const char *)p, len));
}

bool OldPatToParts(const uint8_t *blob, size_t size, Parts &parts, std::string *err)
{
	bg::OldPat op;
	if (!bg::OldPat::IsOldPat(blob, size) || !op.Parse(blob, size)) {
		if (err) *err = "parts blob is not an old-format PAT";
		return false;
	}
	auto rd32 = [&](size_t o) { int32_t v = 0; if (o + 4 <= size) memcpy(&v, blob + o, 4); return v; };

	parts.Free();
	delete[] parts.data;
	parts.data = nullptr;
	parts.filePath.clear();
	parts.useMBAACCFormat = true;

	// --- textures: one buffer owned by parts.data ---
	const auto &tex = op.Textures();
	size_t total = 0;
	for (auto &t : tex) total += t.bgra.size();
	char *buf = new char[total ? total : 1];
	size_t cur = 0;
	parts.gfxMeta.resize(tex.size());
	for (size_t i = 0; i < tex.size(); i++) {
		auto &g = parts.gfxMeta[i];
		g.id = (int)i;
		if (tex[i].bgra.empty()) continue;
		g.name = "tex" + std::to_string(i);   // PGNM; unnamed textures are not saved
		memcpy(buf + cur, tex[i].bgra.data(), tex[i].bgra.size());
		g.data = buf + cur;
		g.w = g.h = tex[i].size;
		g.bpp = 32;
		cur += tex[i].bgra.size();
	}
	while (!parts.gfxMeta.empty() && parts.gfxMeta.back().data == nullptr) parts.gfxMeta.pop_back();
	parts.data = buf;

	// --- cutouts ---
	int maxCut = -1;
	for (auto &c : op.Cutouts()) maxCut = std::max(maxCut, c.first);
	parts.cutOuts.resize(maxCut + 1);
	for (int i = 0; i <= maxCut; i++) parts.cutOuts[i].id = i;
	for (auto &c : op.Cutouts()) {
		auto &co = parts.cutOuts[c.first];
		const bg::PatCutout &s = c.second;
		co.uv[0] = s.srcX; co.uv[1] = s.srcY; co.uv[2] = s.srcW; co.uv[3] = s.srcH;
		co.wh[0] = s.quadW; co.wh[1] = s.quadH;
		co.xy[0] = s.originX; co.xy[1] = s.originY;
		co.texture = s.texture;
		size_t off = (size_t)rd32(4040 + 4 * (size_t)c.first);
		co.pptx = rd32(off + 4);
		if (off + 84 <= size) co.name = FixedStr(blob + off + 52, 32);
	}

	// --- pattern -> part set ---
	int maxPat = -1;
	for (auto &p : op.Patterns()) maxPat = std::max(maxPat, p.first);
	parts.partSets.resize(maxPat + 1);
	for (int i = 0; i <= maxPat; i++) parts.partSets[i].partId = i;
	for (auto &p : op.Patterns()) {
		auto &ps = parts.partSets[p.first];
		ps.wasLoaded = true;
		if (kPartNamesOff + 32 * (size_t)(p.first + 1) <= size)
			ps.name = FixedStr(blob + kPartNamesOff + 32 * (size_t)p.first, 32);
		int maxSlot = -1;
		for (auto &pt : p.second.parts) maxSlot = std::max(maxSlot, pt.partIndex);
		ps.groups.resize(maxSlot + 1);
		for (int k = 0; k <= maxSlot; k++) ps.groups[k].propId = k;
		for (auto &pt : p.second.parts) {
			PartProperty &pr = ps.groups[pt.partIndex];
			pr.propId = pt.partIndex;
			pr.ppId = pt.cutoutRef;
			pr.x = (int)pt.posX - 320;
			pr.y = (int)pt.posY - 448;
			pr.scaleX = pt.scaleX;
			pr.scaleY = pt.scaleY;
			pr.rotation[3] = pt.rotation / 10000.f;
			pr.flip = pt.flip;
			pr.additive = pt.additive;
			pr.filter = pt.linearFilter;
			pr.priority = pt.priority + pt.partIndex / 1000.f;   // same sub-order as PrLoad
			pr.bgra[0] = pt.colB; pr.bgra[1] = pt.colG; pr.bgra[2] = pt.colR; pr.bgra[3] = pt.colA;
			pr.addColor[0] = pt.addB / 255.f; pr.addColor[1] = pt.addG / 255.f; pr.addColor[2] = pt.addR / 255.f;
			pr.addColor[3] = 0.f;
		}
	}
	parts.loaded = true;
	return true;
}

} // namespace ha4
