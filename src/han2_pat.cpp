// French-Bread PAT v3 (RBO, embedded in the .DAT) / v4 (GOF2 .PAT) -> Hantei-chan Parts model.
// Layout and evidence: docs/formats/frenchbread_rbo_gof.md section 4 (IDA: Han2Dat_LoadPartsPatBlock 0x4037D0,
// Han2Dat_LoadPatTextures, Actor_DrawPartsPose 0x447720, PosePart_* 0x445xxx).
#include "han2_pat.h"
#include "parts/parts.h"
#include "misc.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

namespace han2 {

namespace {
constexpr size_t kHeader = 24;
constexpr size_t kPartSize = 92;
constexpr size_t kPartsPerPose = 40;
constexpr size_t kImageHead = 11824;
constexpr int kMaxTextures = 50;

inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }

std::string FixedStr(const uint8_t *p, size_t n)
{
	size_t len = 0;
	while (len < n && p[len]) len++;
	return sj2utf8(std::string((const char *)p, len));
}
} // namespace

bool IsPat(const uint8_t *blob, size_t size)
{
	if (size < 8) return false;
	uint32_t v, m;
	memcpy(&v, blob, 4); memcpy(&m, blob + 4, 4);
	return (v == 3 || v == 4) && m == 0x01234567u;
}

bool PatToParts(const uint8_t *blob, size_t size, Parts &parts, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (!IsPat(blob, size)) return fail("not a PAT v3/v4 block");
	const int nPoses = rd32(blob) == 4 ? 2000 : 1000;
	const size_t offsTab = kHeader, namesTab = kHeader + 4 * (size_t)nPoses;
	const size_t imgOffField = namesTab + 32 * (size_t)nPoses;
	const size_t poseBase = imgOffField + 4;
	if (size < poseBase) return fail("PAT block shorter than its header");
	const size_t imageOff = (size_t)(uint32_t)rd32(blob + imgOffField);
	if (imageOff < poseBase || imageOff > size) return fail("bad image area offset");
	if (imageOff + kImageHead > size) return fail("image area shorter than its head");
	const uint8_t *img = blob + imageOff;
	const size_t imgSize = size - imageOff;

	// ---- textures: B,G,R,A, square (256 or 512), packed per the offset table of the image head ----
	parts.Free();
	delete[] parts.data;
	parts.data = nullptr;
	parts.filePath.clear();
	parts.useMBAACCFormat = true;

	int texCount = 0, texRes[kMaxTextures]{}; size_t texOff[kMaxTextures]{};
	size_t total = 0;
	for (int i = 0; i < kMaxTextures; i++) {
		texOff[i] = (size_t)(uint32_t)rd32(img + 0x1C + 4 * i);
		texRes[i] = rd32(img + 0xD64 + 4 * i);
		if (!texOff[i]) continue;
		if (texRes[i] <= 0 || texRes[i] > 4096) return fail("bad texture size in the image head");
		size_t bytes = (size_t)texRes[i] * texRes[i] * 4;
		if (texOff[i] + bytes > imgSize) return fail("texture runs past the PAT block");
		total += bytes;
		texCount = i + 1;
	}
	char *buf = new char[total ? total : 1];
	size_t cur = 0;
	parts.gfxMeta.resize(texCount);
	for (int i = 0; i < texCount; i++) {
		auto &g = parts.gfxMeta[i];
		g.id = i;
		if (!texOff[i]) continue;
		size_t bytes = (size_t)texRes[i] * texRes[i] * 4;
		memcpy(buf + cur, img + texOff[i], bytes);
		g.name = "tex" + std::to_string(i);
		g.data = buf + cur;
		g.w = g.h = texRes[i];
		g.bpp = 32;
		cur += bytes;
	}
	parts.data = buf;

	// ---- poses -> part sets; each distinct (texture, source rect, quad size, origin) becomes one cutout ----
	std::map<std::tuple<int, int, int, int, int, int, int, int, int>, int> cutIndex;
	parts.cutOuts.clear();
	int maxPose = -1;
	std::vector<int> poseFirst(nPoses, -1);
	for (int p = 0; p < nPoses; p++) {
		uint32_t off = (uint32_t)rd32(blob + offsTab + 4 * p);
		if (!off) continue;
		if (off < poseBase || (off - poseBase) % kPartSize) return fail("bad pose offset");
		poseFirst[p] = (int)((off - poseBase) / kPartSize);
		maxPose = p;
	}
	parts.partSets.resize(maxPose + 1);
	for (int p = 0; p <= maxPose; p++) parts.partSets[p].partId = p;
	for (int p = 0; p <= maxPose; p++) {
		if (poseFirst[p] < 0) continue;
		auto &ps = parts.partSets[p];
		ps.wasLoaded = true;
		ps.name = FixedStr(blob + namesTab + 32 * (size_t)p, 32);
		size_t base = poseBase + kPartSize * (size_t)poseFirst[p];
		if (base + kPartSize * kPartsPerPose > imageOff) return fail("pose runs past the pose area");
		int maxSlot = -1;
		for (size_t k = 0; k < kPartsPerPose; k++) {
			const uint8_t *r = blob + base + kPartSize * k;
			int tex = rd32(r + 0x28);
			int sw = rd16(r + 0x34), sh = rd16(r + 0x38);
			if (sw == 0 && rd32(r + 0x34) == 0) continue;      // unused slot (Pose_CollectSortedParts tests the dword)
			if ((uint32_t)tex == 0xFFFF || tex < 0 || tex >= texCount) continue;   // clip part: the engine draws nothing
			maxSlot = (int)k;
		}
		ps.groups.resize(maxSlot + 1);
		for (int k = 0; k <= maxSlot; k++) ps.groups[k].propId = k;
		for (int k = 0; k <= maxSlot; k++) {
			const uint8_t *r = blob + base + kPartSize * (size_t)k;
			int tex = rd32(r + 0x28);
			int sx = rd16(r + 0x2C), sy = rd16(r + 0x30), sw = rd16(r + 0x34), sh = rd16(r + 0x38);
			if (rd32(r + 0x34) == 0 || (uint32_t)tex == 0xFFFF || tex < 0 || tex >= texCount) continue;
			int dw = rd32(r + 0x08), dh = rd32(r + 0x0C), ox = rd32(r + 0x40), oy = rd32(r + 0x44);
			auto key = std::make_tuple(tex, sx, sy, sw, sh, dw, dh, ox, oy);
			auto it = cutIndex.find(key);
			int ci;
			if (it == cutIndex.end()) {
				ci = (int)parts.cutOuts.size();
				cutIndex[key] = ci;
				parts.cutOuts.emplace_back();
				auto &co = parts.cutOuts.back();
				co.id = ci;
				co.uv[0] = sx; co.uv[1] = sy; co.uv[2] = sw; co.uv[3] = sh;
				co.wh[0] = dw; co.wh[1] = dh;
				co.xy[0] = ox; co.xy[1] = oy;
				co.texture = tex;
				co.name = "c" + std::to_string(ci);
			} else ci = it->second;
			PartProperty &pr = ps.groups[k];
			pr.propId = k;
			pr.ppId = ci;
			pr.x = rd32(r + 0x00);
			pr.y = rd32(r + 0x04);
			pr.scaleX = rd32(r + 0x14) / 1000.f;
			pr.scaleY = rd32(r + 0x18) / 1000.f;
			pr.rotation[3] = rd32(r + 0x1C) / 10000.f;
			pr.flip = rd32(r + 0x10) & 3;
			uint8_t layer = r[0x3C];
			pr.priority = (float)layer + k / 1000.f;            // draw order: ascending (layer << 8) + part index
			pr.bgra[0] = r[0x23]; pr.bgra[1] = r[0x22]; pr.bgra[2] = r[0x21]; pr.bgra[3] = r[0x20];   // bytes A,R,G,B in memory
			pr.addColor[0] = r[0x26] / 255.f; pr.addColor[1] = r[0x25] / 255.f; pr.addColor[2] = r[0x24] / 255.f;
			pr.addColor[3] = 0.f;
		}
	}
	parts.loaded = true;
	return true;
}

} // namespace han2
