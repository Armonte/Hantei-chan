#ifndef HA4_RAW_H_GUARD
#define HA4_RAW_H_GUARD

// Per-frame / per-pattern HA4 (MBAC Hantei4 .DAT) source bytes.
//
// An HA4 character is mapped into the regular HA6 in-memory model (Frame /
// Sequence), so every editor view works on it. HA4 has bytes with no HA6
// field (flip-mode enum, AS pads, heap garbage in the index array, IF params
// 9..11, the 40-byte pattern header tail, the name bytes after the NUL, ...)
// and a few mappings are lossy (parts-frame boxes are stored at double
// resolution; flip modes vs free rotation). The loader keeps the original
// records here; the writer starts from them and only re-encodes a field when
// the model value differs from what the original bytes decode to. That makes
// load -> save byte-identical and keeps every untouched HA4-only value.
//
// Plain fixed-size POD on purpose: Frame_T is copied across allocators
// (LinearAllocator clipboard in shared memory), so no pointers here.
// See docs/HANTEI_MBAC_SUPPORT.md and docs/bg_research/HA4_SECTION.md.

#include <cstdint>
#include <cstring>

struct Ha4FrameRaw
{
	bool     valid = false;
	bool     hadAT = false;           // original frame had an AT record (idx[0] != -1)
	uint8_t  rec[216]{};              // AF 44 | AS 56 | int16 idx[58]
	uint8_t  at[88]{};                // AT record (valid when hadAT)
	uint8_t  ifr[8][52]{};            // IF record per slot (slot used when idx[8+k] != -1)
	uint8_t  efr[8][52]{};            // EF record per slot (slot used when idx[16+k] != -1)
	int16_t  box[33][4]{};            // raw box coords, HA6 numbering (0..24 normal, 25..32 attack)
	uint64_t boxMask = 0;             // bit k set = raw box k present

	int16_t idx(int i) const { int16_t v; memcpy(&v, rec + 100 + 2*i, 2); return v; }
};

struct Ha4SeqRaw
{
	bool    valid = false;            // pattern existed in the source file
	bool    nameValid = false;        // name bytes came from the source file
	uint8_t hdr[0x44]{};              // pattern header (frame count, moveInfo, level, table offsets, garbage tail)
	uint8_t name[64]{};               // raw CP932 name slot (bytes after the NUL are kept)
};

#endif /* HA4_RAW_H_GUARD */
