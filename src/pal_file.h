#ifndef PAL_FILE_H_GUARD
#define PAL_FILE_H_GUARD

// Character palette bank (.pal), lossless load/save.
//
// Two layouts exist (CharaPalette_ParsePalToSlot, MBTL.exe 0x5928A0):
//  - MBAACC: u32 count, then count x 256 BGRA (64 palettes, 65,540 bytes).
//  - UNI2/MBTL: u32 0xFFFF, u32 split, u32 0, u32 count, then count x 256
//    BGRA. All shipped files have split = 1 and count = 130: 65 colours x 2
//    sets, where palette c + 65 is colour c's alternate set (the game copies
//    palette c to row slot and palette c + 65 to row slot + 8). split = 0
//    would make all `count` palettes plain colours.
// Palette files 1..7 of the same character are <name>_p<n>.pal (PUPS).
// Colours are kept exactly as stored (CG makes alpha binary only for display).

#include <cstdint>
#include <string>
#include <vector>

struct PalFile
{
	bool modern = false;          // FFFF header
	uint32_t split = 1;           // modern: second word
	uint32_t reserved = 0;        // modern: third word
	std::vector<uint32_t> colors; // count * 256
	std::string trailing;         // bytes after the last palette, if any

	int count() const { return (int)(colors.size() / 256); }
	// Colours of the pair set (split files): palette index of colour c's alternate.
	int alternateOf(int c) const { return (modern && split && c < count() / 2) ? c + count() / 2 : -1; }

	bool load(const char *path);
	bool save(const char *path) const;
	std::string serialize() const;
};

#endif
