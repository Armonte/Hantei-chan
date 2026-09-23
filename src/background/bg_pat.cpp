#include "bg_pat.h"
#include <algorithm>
#include <cstring>

namespace bg {

namespace {

// File-region offsets of the three top-level tables.
constexpr size_t kPatternTableOff = 40;     // 1000 dwords
constexpr size_t kCutoutTableOff  = 4040;   // 1000 dwords
constexpr size_t kTexRegionPtrOff = 40040;  // dword -> texture-region offset
constexpr int    kTableEntries    = 1000;

// A pattern is exactly this many parts of this stride.
constexpr int    kPartsPerPattern = 40;
constexpr size_t kPartStride      = 104;

// Texture-region internal offsets.
constexpr size_t kTexOffArray   = 28;    // 50 dwords: data offset per texture
constexpr size_t kTexSizeArray  = 3428;  // 50 dwords: square size per texture
constexpr int    kMaxTextures   = 50;

} // namespace

bool OldPat::IsOldPat(const uint8_t* data, size_t size) {
	if (!data || size < 8) return false;
	uint32_t m0, m1;
	std::memcpy(&m0, data, 4);
	std::memcpy(&m1, data + 4, 4);
	return m0 == 2u && m1 == 0x01234567u;
}

bool OldPat::Parse(const uint8_t* data, size_t size) {
	valid = false;
	patterns.clear();
	cutouts.clear();
	textures.clear();

	if (!IsOldPat(data, size)) return false;

	auto rd32 = [&](size_t off) -> int32_t {
		int32_t v = 0;
		if (off + 4 <= size) std::memcpy(&v, data + off, 4);
		return v;
	};
	auto rdf = [&](size_t off) -> float {
		float v = 0.0f;
		if (off + 4 <= size) std::memcpy(&v, data + off, 4);
		return v;
	};
	(void)rdf;   // no float fields left in the part record (pos is int32)

	// --- textures -----------------------------------------------------------
	// The texture region carries a 50-slot offset array and a parallel 50-slot
	// square-size array. Pixel data is raw BGRA8888 at texReg + dataOffset.
	int32_t texRegOff = rd32(kTexRegionPtrOff);
	if (texRegOff > 0 && (size_t)texRegOff < size) {
		textures.resize(kMaxTextures);
		for (int i = 0; i < kMaxTextures; ++i) {
			int32_t dataOff = rd32((size_t)texRegOff + kTexOffArray + 4 * i);
			int32_t texSize = rd32((size_t)texRegOff + kTexSizeArray + 4 * i);
			if (dataOff == 0 || texSize <= 0) continue;
			size_t abs   = (size_t)texRegOff + (size_t)dataOff;
			size_t bytes = (size_t)texSize * (size_t)texSize * 4;
			if (abs > size || bytes > size - abs) continue;
			textures[i].size = texSize;
			textures[i].bgra.assign(data + abs, data + abs + bytes);
		}
	}

	// --- cutouts ------------------------------------------------------------
	for (int i = 0; i < kTableEntries; ++i) {
		int32_t co = rd32(kCutoutTableOff + 4 * i);
		if (co == 0) continue;
		if ((size_t)co + 40 > size) continue;
		PatCutout c;
		c.texture = rd32((size_t)co + 0);
		c.srcX    = rd32((size_t)co + 8);
		c.srcY    = rd32((size_t)co + 12);
		c.srcW    = rd32((size_t)co + 16);
		c.srcH    = rd32((size_t)co + 20);
		c.quadW   = rd32((size_t)co + 24);
		c.quadH   = rd32((size_t)co + 28);
		c.originX = rd32((size_t)co + 32);
		c.originY = rd32((size_t)co + 36);
		cutouts[i] = c;
	}

	// --- patterns -----------------------------------------------------------
	for (int i = 0; i < kTableEntries; ++i) {
		int32_t po = rd32(kPatternTableOff + 4 * i);
		if (po == 0) continue;
		if ((size_t)po + kPartsPerPattern * kPartStride > size) continue;

		PatPattern pat;
		for (int p = 0; p < kPartsPerPattern; ++p) {
			size_t pp = (size_t)po + kPartStride * p;
			int32_t cutRef = rd32(pp + 32);
			if (cutRef == -1) continue;   // empty part

			PatPart part;
			part.partIndex = p;
			part.cutoutRef = cutRef;
			// Position is int32 (MBAA.exe fild at 0x4b75c1/0x4b75d0).
			part.posX      = (float)rd32(pp + 36);
			part.posY      = (float)rd32(pp + 40);
			part.flip      = data[pp + 48];
			part.additive  = data[pp + 49] != 0;      // blend mode 2
			part.linearFilter = data[pp + 50] != 0;   // sampler mode 2
			part.rotation  = rd32(pp + 60);
			part.addR      = data[pp + 68];
			part.addG      = data[pp + 69];
			part.addB      = data[pp + 70];
			part.scaleX    = rd32(pp + 52) * 0.001f;
			part.scaleY    = rd32(pp + 56) * 0.001f;
			part.colA      = data[pp + 64];
			part.colR      = data[pp + 65];
			part.colG      = data[pp + 66];
			part.colB      = data[pp + 67];
			part.priority  = data[pp + 71];
			pat.parts.push_back(part);
		}

		// Back-to-front order. MBAACC's Background_BuildLayerIndexMap draws
		// higher-priority parts first; equal priority breaks on part index,
		// also highest-first. We sort so parts[0] is drawn first (back).
		std::stable_sort(pat.parts.begin(), pat.parts.end(),
			[](const PatPart& a, const PatPart& b) {
				if (a.priority != b.priority) return a.priority > b.priority;
				return a.partIndex > b.partIndex;
			});

		patterns[i] = std::move(pat);
	}

	valid = !patterns.empty();
	return valid;
}

const PatPattern* OldPat::GetPattern(int id) const {
	auto it = patterns.find(id);
	return it == patterns.end() ? nullptr : &it->second;
}

const PatCutout* OldPat::GetCutout(int id) const {
	auto it = cutouts.find(id);
	return it == cutouts.end() ? nullptr : &it->second;
}

const PatTexture* OldPat::GetTexture(int id) const {
	if (id < 0 || id >= (int)textures.size()) return nullptr;
	if (textures[id].size == 0) return nullptr;
	return &textures[id];
}

} // namespace bg
