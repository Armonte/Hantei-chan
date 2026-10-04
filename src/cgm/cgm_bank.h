#ifndef CGM_BANK_H_GUARD
#define CGM_BANK_H_GUARD
// CG manager core: a byte-exact editable model of a "BMP Cutter3/2" sprite bank (MBAACC .cg, MBAC CG blobs, GOF2 CHP/CG).
// Layout and invariants: docs/cg/cg_manager_design.md section 1. No GL / ImGui in here (CLI and tests link it).
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cgm {

constexpr int kMaxImages = 3000;           // size of the image offset table
constexpr uint32_t kAbsent = 0xFFFFFFFFu;  // table entry of a missing image

struct Block {                              // one alignment-table record (24 bytes)
	int32_t x = 0, y = 0, w = 0, h = 0;      // destination rectangle on the image canvas
	int16_t sx = 0, sy = 0, page = 0;        // source cells on the atlas page
	int16_t copy = 0;                        // != 0: owns no data, draws cells owned by another image
	uint32_t dataOff = 0;                    // derived: offset of this block's data inside the blob (owned blocks only)
};

using Blob = std::shared_ptr<const std::vector<uint8_t>>;

struct Image {
	bool present = false;
	char name[32] = {};
	int32_t type = 0, w = 0, h = 0, bpp = 0;   // storage type (0 8-bit, 1 BGRA, 2 pal+idx, 3 colour+alpha, 4 pal+idx+alpha, 5 idx+alpha on the bank palette (engine-supported, no shipped bank), -1 undrawable)
	int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;    // canvas bounds, inclusive
	std::vector<Block> blocks;
	Blob blob;                                  // data after the 72-byte header (shared between snapshots)
	int boundsW() const { return x2 - x1 + 1; }
	int boundsH() const { return y2 - y1 + 1; }
	bool drawable() const { return present && type != -1 && x2 >= x1 && y2 >= y1; }
};

// Pixel order everywhere in this model: memory R,G,B,A (value 0xAABBGGRR), straight alpha.
struct Rgba { std::vector<uint8_t> px; int w = 0, h = 0; };

struct Bank {
	std::array<uint8_t, 0x14> head{};           // "BMP Cutter3\0..." + dword
	std::array<uint8_t, 0x2000> palettes{};     // 8 x 256 x RGBA (slot 0 is the one the engine path uses)
	uint32_t H[12] = {};                        // H0 = pages-1, H1, H2 = nAlign (derived), H3 = nImages (derived), H4 = cell unit, ...
	uint32_t tailMid = 0;                       // the zero dword between align offset and file size
	std::vector<Image> images;                  // size == H3
	int hidden = 0;                             // images stored past the declared count H3 (the engine never reaches them; one RBO bank has one)
	bool alignSequential = true;                // blocks of image n directly follow those of image n-1 (verified on load)

	int cellUnit() const { return (H[4] >= 1 && H[4] < 16) ? (int)H[4] : 16; }
	int pages() const { return (int)H[0] + 1; }

	static bool parse(const uint8_t *data, size_t size, Bank &out, std::string *err);
	void serialize(std::vector<uint8_t> &out) const;

	// Palette slot as engine-normalised RGBA (binary alpha, entry 0 transparent).
	void palette(int slot, uint32_t out[256]) const;
	void setPaletteSlot(int slot, const uint32_t rgba[256]);   // writes raw memory RGBA

	// Pixels of image n (its bounds rectangle). pal = 256 entries to use for type 0 (null: bank slot 0).
	bool decode(int n, Rgba &out, const uint32_t *pal = nullptr) const;
	// 8-bit view for indexed export: type 0 -> bank indices, type 2/4 -> embedded palette. alpha8 filled for type 4 (else empty).
	bool decodeIndexed(int n, std::vector<uint8_t> &idx, uint32_t pal[256], std::vector<uint8_t> *alpha8 = nullptr, const uint32_t *bankPal = nullptr) const;

	// Atlas: which image owns each (page, cell). key = page*65536 + cy*256 + cx.
	std::unordered_map<uint32_t, int> buildAtlas() const;
	static uint32_t cellKey(int page, int cx, int cy) { return (uint32_t)page * 65536u + (uint32_t)cy * 256u + (uint32_t)cx; }
	// Images that draw cells owned by image n / images whose cells image n draws.
	std::vector<int> dependants(int n, const std::unordered_map<uint32_t, int> &atlas) const;
	std::vector<int> owners(int n, const std::unordered_map<uint32_t, int> &atlas) const;

	// Byte range [start,end) of image n's data blob in the serialized bank.
	bool blobSpan(int n, size_t &start, size_t &end) const;
	void recomputeLayout();   // blocks' dataOff + H2/H3
};

uint64_t HashRgba(const uint8_t *px, size_t n);   // FNV-1a 64 over straight RGBA with transparent pixels' RGB zeroed

} // namespace cgm
#endif
