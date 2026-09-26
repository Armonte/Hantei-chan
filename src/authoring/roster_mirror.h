#ifndef AUTHORING_ROSTER_MIRROR_H_GUARD
#define AUTHORING_ROSTER_MIRROR_H_GUARD
// [authoring] The MBAACC CSS roster, for when no game is linked (docs/HANTEI_AUTHORING_MODE.md §5.3). When linked, the
// game's own LinkRoster (QueryRoster) replaces it.
//
// MIRRORS PovertyCaster pc-adapters/mbaacc/include/mbaacc/MbaaccRoster.hpp kCssRoster (selector, chara, name; grid
// order) plus File1 / File2 / TagType from the game's g_CharaSelectDataTable (data\charaselect.txt, dumped in
// docs/tag_research/data/charaselect.txt). tests/authoring_model_test.cpp checks the rows against MbaaccRoster.hpp when
// it is on disk (PC_ROSTER_HPP).
#include <cstdint>

namespace authoring {

struct RosterMirrorRow {
	uint8_t selector;     // CSS grid cell
	int16_t chara;        // g_CharaSelectDataTable index
	const char* name;     // CCCaster short name
	const char* file1;    // lower-case data-file name
	const char* file2;    // "" = none (a duo when set)
	uint8_t tagType;      // charaselect TagType (0 = a single character)
	const char* fullName; // the tooltip name
};

inline constexpr RosterMirrorRow kRosterMirror[] = {
	{  2, 22, "Aoko",     "aoko",      "",          0, "Aoko Aozaki" },
	{  3,  7, "Tohno",    "shiki",     "",          0, "Shiki Tohno" },
	{  4, 51, "Hime",     "p_arc",     "p_arc_d",   255, "Archetype: Earth (Hime)" },
	{  5, 15, "Nanaya",   "nanaya",    "",          0, "Shiki Nanaya" },
	{  6, 28, "Kouma",    "kishima",   "",          0, "Kouma Kishima" },
	{ 10,  8, "Miyako",   "miyako",    "",          0, "Miyako Arima" },
	{ 11,  2, "Ciel",     "ciel",      "",          0, "Ciel" },
	{ 12,  0, "Sion",     "sion",      "",          0, "Sion Eltnam Atlasia" },
	{ 13, 30, "Ries",     "ries",      "",          0, "Riesbyfe Stridberg" },
	{ 14, 11, "V.Sion",   "v_sion",    "",          0, "Sion TATARI (V.Sion)" },
	{ 15,  9, "Wara",     "warakia",   "",          0, "Wallachia" },
	{ 16, 31, "Roa",      "roa",       "",          0, "Michael Roa Valdamjong" },
	{ 19,  4, "Maids",    "hisui",     "kohaku",    1, "Hisui & Kohaku" },
	{ 20,  3, "Akiha",    "akiha",     "",          0, "Akiha Tohno" },
	{ 21,  1, "Arc",      "arc",       "",          0, "Arcueid Brunestud" },
	{ 22, 19, "P.Ciel",   "p_ciel",    "",          0, "Powered Ciel" },
	{ 23, 12, "Warc",     "warc",      "",          0, "Red Arcueid" },
	{ 24, 13, "V.Akiha",  "akaakiha",  "",          0, "Akiha Vermillion" },
	{ 25, 14, "M.Hisui",  "m_hisui",   "",          0, "Mech-Hisui" },
	{ 28, 29, "S.Akiha",  "s_akiha",   "",          0, "Seifuku Akiha" },
	{ 29, 17, "Satsuki",  "satsuki",   "",          0, "Satsuki Yumizuka" },
	{ 30, 18, "Len",      "len",       "",          0, "Len" },
	{ 31, 33, "Ryougi",   "ryougi",    "",          0, "Shiki Ryougi" },
	{ 32, 23, "W.Len",    "wlen",      "",          0, "White Len" },
	{ 33, 10, "Nero",     "nero",      "",          0, "Nrvnqsr Chaos" },
	{ 34, 25, "NAC",      "nechaos",   "",          0, "Neco-Arc Chaos" },
	{ 38, 35, "KohaMech", "kohaku_m",  "m_hisui_p", 2, "Kohaku & Mech-Hisui" },
	{ 39,  5, "Hisui",    "hisui",     "",          0, "Hisui" },
	{ 40, 20, "Neko",     "neco",      "",          0, "Neco-Arc" },
	{ 41,  6, "Kohaku",   "kohaku",    "",          0, "Kohaku" },
	{ 42, 34, "NekoMech", "m_hisui_m", "neco_p",    3, "Neco & Mech" },
};
inline constexpr int kRosterMirrorCount = (int)(sizeof(kRosterMirror) / sizeof(kRosterMirror[0]));
static_assert(kRosterMirrorCount == 31, "kCssRoster has 31 selectable cells");

// The CSS grid rows (kCssRoster's row comments): first index of each row, for laying the grid out like the game.
inline constexpr int kRosterRowStart[] = { 0, 5, 12, 19, 26, 31 };

// TAG CSS assist choices (TagCss.hpp kAssistMotionChoices): choice k (1-based) = kAssistMotionChoices[k-1], 0 = the
// tuning's action.
inline constexpr const char* kAssistMotionChoices[] = {
	"236A", "236B", "623A", "623B", "214A", "214B", "22A",  "22B",
	"421A", "421B", "41236A", "63214A", "236C", "623C", "214C", "22C",
};
inline constexpr int kAssistMotionCount = (int)(sizeof(kAssistMotionChoices) / sizeof(kAssistMotionChoices[0]));

// The MBAC (Act Cadenza) character for an MBAACC chara id, or "" when MBAC has none: the 22 shared characters
// (TagHud.hpp mbacBannerId: MBAC's ids equal MBAA's except Kouma 28 -> MBAC 24). The stem names MBAC's files
// (<STEM>.DAT, <STEM>_C.TXT in the MBAC install's 02 archive).
inline const char* MbacStemForChara(int chara)
{
	switch (chara) {
	case 0: return "SION"; case 1: return "ARC"; case 2: return "CIEL"; case 3: return "AKIHA"; case 5: return "HISUI";
	case 6: return "KOHAKU"; case 7: return "SHIKI"; case 8: return "MIYAKO"; case 9: return "WARAKIA"; case 10: return "NERO";
	case 11: return "V_SION"; case 12: return "WARC"; case 13: return "AKAAKIHA"; case 14: return "M_HISUI";
	case 15: return "NANAYA"; case 17: return "SATSUKI"; case 18: return "LEN"; case 20: return "NECO"; case 22: return "AOKO";
	case 23: return "WLEN"; case 25: return "NECHAOS"; case 28: return "KISHIMA";
	default: return "";
	}
}

} // namespace authoring

#endif
