#ifndef CGM_CCPAL_H_GUARD
#define CGM_CCPAL_H_GUARD
// MBAA:CC custom palettes for PovertyCaster / CCCaster (docs/mbac_giant_chaos/CC_SELECT_PALETTE_FINDINGS_cg2.md and
// docs/cc_select_patch/DESIGN.md section 4):
//  - per palette an indexed PNG <game>\palettes\<charID>_<palNum>.png whose PLTE is the 256 colours (palNum 1-based, 1..252);
//  - optionally the full data\<File>.pal and data\_csel\<File>.pal with count grown past 36 (new slots = palette 0, entry 0 zeroed);
//  - swatch_indices.json: the two palette entries the stock color_cNN.png swatch cells show, per character.
// Palette ids past 36 are shown in banks ("generated tabs") of 36: effective id = bank * 36 + local (DESIGN.md 2.3).
#include "cgm_palette.h"
#include <map>
#include <string>
#include <vector>

namespace cgm {

constexpr int kCcPalettesPerBank = 36;   // the native list (CharaSelectColor_*: 36 entries, 6 pages of 6)
constexpr int kCcMaxPalettes = 252;      // 7 banks (actor palette byte / PovertyCaster tuple byte)

// The layout hook: where palette slot `palNum` (1-based) appears in the selector. Stock: bank 0 = 1..36.
struct CcSlotPlace { int bank = 0, local = 0, page = 0, row = 0; };   // local 0..35, page (6 per bank) and row 0..5 in the carousel
CcSlotPlace CcPlaceOf(int palNum);
int CcPalNum(int bank, int local);

// PovertyCaster's charaFromPalStem (CCCaster's getIndexFromCharName) plus the S7 boss ids. -1 = not a selectable palette file.
int CcCharaFromStem(const std::string &stem);
std::string CcPaletteFileName(int chara, int palNum);   // "<chara>_<palNum>.png"

// Indexed PNG for PovertyCaster: a 16 x 16 picture of indices 0..255 whose PLTE is `pal` (memory RGBA).
bool WriteCcPalettePng(const std::string &utf8Path, const uint32_t pal[256], std::string *err);
// Reads one back (the PLTE), as PovertyCaster does.
bool ReadCcPalettePng(const std::string &utf8Path, uint32_t pal[256], std::string *err);

struct CcExportItem { int sourcePalette = 0; int palNum = 1; };   // palette index in the set -> 1-based slot
struct CcExportResult { std::vector<std::string> written; std::string error; };
bool ExportCcPalettePngs(const PalSet &set, int chara, const std::vector<CcExportItem> &items, const std::string &utf8Dir, CcExportResult &res);

// MBAACC .pal grown to `count` palettes (filler = palette 0 with entry 0 zeroed, like the shipped 36..63) with the items written in.
bool BuildCcPalSet(const PalSet &set, const std::vector<CcExportItem> &items, int count, PalSet &out, std::string *err);

// Swatch calibration: for the 36 stock palettes and the stock swatch strip (256 x 128, 8 x 8 cells of 32 x 16; cell i at
// (32 (i & 7), 16 (i >> 3))), the palette entries whose colour every one of the 36 cells shows, best first.
std::vector<int> CcSwatchIndices(const PalSet &stock, const uint8_t *stripRgba, int w, int h);
std::string CcSwatchJson(const std::map<int, std::vector<int>> &perChara);

} // namespace cgm
#endif
