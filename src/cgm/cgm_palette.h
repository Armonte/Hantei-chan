#ifndef CGM_PALETTE_H_GUARD
#define CGM_PALETTE_H_GUARD
// Palette model + tools: .pal sets (MBAACC "count + n x 256 RGBA" and the UNI/MBTL "FFFF,split,0,count" header), single palettes
// (256 x 0xAABBGGRR in memory order R,G,B,A), colour operations, whole-bank recolour, .act / GIMP .gpl / PNG-strip / RIFF files.
#include "cgm_bank.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace cgm {

struct PalSet {
	std::vector<uint8_t> raw;     // the file as read; edits touch only the colour bytes, so an unedited set serialises byte-identical
	int offsetDw = 1;             // 1 = MBAACC (count at dword 0), 4 = UNI/MBTL header
	int count = 0;
	static bool parse(const uint8_t *d, size_t n, PalSet &out, std::string *err);
	uint32_t *pal(int i) { return (uint32_t *)raw.data() + offsetDw + 256 * i; }
	const uint32_t *pal(int i) const { return (const uint32_t *)raw.data() + offsetDw + 256 * i; }
	bool addCopy(int from, std::string *err = nullptr);          // appends a copy of palette `from` (MBAACC layout only)
	bool remove(int i, std::string *err = nullptr);              // MBAACC layout only
	void swapPalettes(int a, int b);
	void movePalette(int from, int to);
	// UNI2 / MBTL split files (header FFFF, split != 0): palette c + count/2 is colour c's alternate set (the game copies c to row
	// slot and c + count/2 to slot + 8). Moves and swaps on such a file act on the pair, so the two never part.
	uint32_t split() const { uint32_t v = 0; if (offsetDw == 4 && raw.size() >= 8) memcpy(&v, raw.data() + 4, 4); return v; }
	bool paired() const { return offsetDw == 4 && split() != 0 && count >= 2 && count % 2 == 0; }
	int half() const { return paired() ? count / 2 : count; }
	int alternateOf(int c) const { return paired() ? (c < half() ? c + half() : c - half()) : -1; }
	int colourOf(int i) const { return paired() && i >= half() ? i - half() : i; }   // the colour slot palette i belongs to
	void movePair(int fromColour, int toColour);   // paired files: moves palettes c and c+half together (other files: movePalette)
	void swapPair(int a, int b);
	// The same colours in the other layout. toUni: FFFF,1,0,130 with colours 0..64 from palettes 0..64 and each alternate a copy of
	// its colour (or the source's own pair when it is already paired); !toUni: MBAACC count + palettes (a paired file gives its 130).
	PalSet converted(bool toUni) const;
	static PalSet makeMbaacc(int count, const uint32_t *fill);   // `count` copies of one palette (fill = 256 colours)
};
enum class PalLayout { Mbaacc, Uni };

// ---- colour helpers (memory RGBA dwords) ----
inline uint32_t Rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24); }
inline uint8_t R(uint32_t c) { return c & 255; }
inline uint8_t G(uint32_t c) { return (c >> 8) & 255; }
inline uint8_t B(uint32_t c) { return (c >> 16) & 255; }
inline uint8_t A(uint32_t c) { return c >> 24; }

struct Hsv { float h = 0, s = 0, v = 0; };   // h in degrees [0,360)
Hsv ToHsv(uint32_t c);
uint32_t FromHsv(Hsv h, uint8_t alpha);

struct ColorAdjust {            // applied to the RGB of a colour; alpha is kept. Neutral values return the colour unchanged.
	float hueDeg = 0, satMul = 1, valMul = 1;
	int satAdd = 0, valAdd = 0;   // -255..255 in 8-bit steps, applied after the multipliers
	bool neutral() const { return hueDeg == 0 && satMul == 1 && valMul == 1 && !satAdd && !valAdd; }
};
uint32_t Adjust(uint32_t c, const ColorAdjust &a);

// ---- single palette operations: indices [a,b] inclusive, entry alpha preserved ----
void FillRange(uint32_t *pal, int a, int b, uint32_t rgb);
void Gradient(uint32_t *pal, int a, int b, uint32_t from, uint32_t to, bool viaHsv);   // endpoints are pal[a] and pal[b] colours when from/to == pal values
void AdjustRange(uint32_t *pal, int a, int b, const ColorAdjust &adj);
void ReverseRange(uint32_t *pal, int a, int b);
void CopyRange(const uint32_t *src, int a, int b, uint32_t *dst, int at);
int ReplaceColor(uint32_t *pal, uint32_t from, uint32_t to, int tolerance);              // returns entries changed

// ---- whole-bank recolour: palette sets and the palettes embedded in the sprite bank ----
struct RecolorScope { bool allPalettes = true; int onlyPalette = -1; int fromIndex = 1; int toIndex = 255; };
int RecolorSet(PalSet &set, const ColorAdjust &adj, const RecolorScope &scope);          // returns palettes touched
// Bank slots (0-7) and the own palettes of type 2/4 images (and the colour of type 3 images); entry 0 is never changed.
int RecolorBank(Bank &bank, const ColorAdjust &adj, bool slots, bool imagePalettes, std::vector<int> *changedImages = nullptr);

// ---- files ----
// .act, .gpl, .pal (raw 1024 / RIFF / MBAACC or UNI set: palette `index`), .png (indexed palette or first 256 pixels)
bool ReadPalFileColors(const std::string &utf8Path, uint32_t out[256], std::string *err, int index = 0);
int PalFileCount(const std::string &utf8Path);   // palettes in a .pal set (1 for every single-palette format, 0 = unreadable)
// extension decides: .act .gpl .png (16x16 strip) .pal (MBAACC "count 1 + palette", or with PalLayout::Uni the UNI header FFFF,0,0,1)
bool WritePalFileColors(const std::string &utf8Path, const uint32_t pal[256], std::string *err, PalLayout layout = PalLayout::Mbaacc);
bool WritePalSet(const std::string &utf8Path, const PalSet &set, bool backup, std::string *err);   // atomic; backup keeps <file>.bak once
// PUPS banks (UNI2 / MBTL; MBAACC ignores them): <cg stem>_p<n>.pal, n = 1..7. Writes a new bank file from `base` (refuses to
// overwrite an existing one). Returns the path.
std::string PupsPath(const std::string &utf8CgOrPalPath, int n);
bool CreatePupsBank(const std::string &utf8CgPath, int n, const PalSet &base, std::string *outPath, std::string *err);

} // namespace cgm
#endif
