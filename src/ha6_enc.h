#ifndef HA6_ENC_H_GUARD
#define HA6_ENC_H_GUARD

// Per-frame / per-pattern HA6 encoding choices, recorded by the loader so the
// UNI/MBTL writer (framedata_save.cpp, WriteSequenceUni) can reproduce the
// original bytes for unmodified data. None of this is game state: the game
// resolves ASSM/HRNS/HRAS references and slot indices while loading
// (han6files.cpp: Han6_LoadPatternFrames in uni2.exe 0x4C4E70 / MBTL.exe
// 0x4A6CC0), so any encoding that yields the same model is equivalent. The
// original FB tool shares state blocks and boxes by pointer, which is not
// derivable from content, so the choices are kept here instead.
//
// Rare tags that the game reads but no shipped UNI2/MBTL file uses (AFAN,
// ASV1, ASVA, ASVC, ASAT, ASKV, ASSS, ASDF, ASCL, ASSE, ASDE, ASF2, ASF3,
// ATAB, ATBG, ATGE, ATKZ, ATGS, ATF2, HRFF, ...) are kept verbatim in
// `extra` and written back at the end of their block.
//
// Plain fixed-size POD on purpose (see ha4_raw.h): Frame_T is copied across
// allocators and compared bytewise by the undo manager.
// See docs/HANTEI_UNI_MBTL.md.

#include <cstdint>
#include <cstring>

enum Ha6ExtraBlock : uint8_t {
	HA6X_FRAME = 0,     // frame level, written after the boxes
	HA6X_AF = 1,        // AF frame level, before AFED
	HA6X_AFLAYER0 = 2,  // AF layer n = HA6X_AFLAYER0 + n, after that layer's tags
	HA6X_AS = 10,       // before ASED
	HA6X_AT = 11,       // before ATED
};

struct Ha6ExtraTag
{
	char     tag[4];
	uint8_t  block;
	uint8_t  nwords;
	int32_t  w[7];
};

// Canonical tag order inside each block. Derived from every UNI2 and MBTL
// HA6 (262 files, 161,778 frames): no two tags ever appear in both orders
// (tools/uni/ha6_order.py), so writing present tags in this order reproduces
// the shipped files. Also indexes the "present at load" bits below.
namespace ha6order {
	// AFGX layer: AFGX id pat sprite, then
	static const char *const kLayer[] = {"AFOF","AFAZ","AFAX","AFRT","AFAY","AFAL","AFZM","AFRG","AFPL"};
	// AF frame level (AFD* = AFD1..9/AFDL, AFF* = AFF1/AFF2/AFFL)
	static const char *const kAF[] = {"AFD*","AFF*","AFFE","AFPR","AFHK","AFJP","AFLP","AFID","AFPA","AFJH","AFJC","AFCT","AFRT"};
	// AS (ASV* = ASV0/ASVX, ASS* = ASS1/ASS2)
	static const char *const kAS[] = {"ASV*","AST0","ASS*","ASMV","ASAA","ASCN","ASCS","ASF0","ASF1","ASCF","ASMX","ASYS","ASCT"};
	// AT (ATS* = ATS1..ATS6)
	static const char *const kAT[] = {"ATGD","ATV2","ATHE","ATS*","ATSU","ATSN","ATBC","ATKK","ATNG","ATSP","ATSA","ATHS",
	                                  "ATRF","ATHT","ATSH","ATHH","ATF1","ATGN","ATAT","ATAM","ATCA","ATC0","ATVD",
	                                  "ATVV","ATHV","ATGV","ATUH","ATBT"};
	// Members of the wildcard entries.
	inline bool WildMatch(const char *l, const char *t)
	{
		if (memcmp(l, t, 3)) return false;
		const char c = t[3];
		if (!memcmp(l, "AFD", 3)) return (c >= '1' && c <= '9') || c == 'L';
		if (!memcmp(l, "AFF", 3)) return c == '1' || c == '2' || c == 'L';
		if (!memcmp(l, "ASV", 3)) return c == '0' || c == 'X';
		if (!memcmp(l, "ASS", 3)) return c == '1' || c == '2';
		if (!memcmp(l, "ATS", 3)) return c >= '1' && c <= '6';
		return false;
	}
	template<size_t N>
	inline int Index(const char *const (&list)[N], const void *tag)
	{
		const char *t = (const char*)tag;
		for (size_t i = 0; i < N; ++i) {
			const char *l = list[i];
			if (l[3] == '*' ? WildMatch(l, t) : !memcmp(l, t, 4))
				return (int)i;
		}
		return -1;
	}
}

struct Ha6FrameEnc
{
	static constexpr int kMaxBoxes = 33;   // 0..24 hurt/collision, 25..32 attack
	static constexpr int kMaxSlots = 16;
	static constexpr int kMaxExtra = 6;

	bool     valid = false;               // frame came from an HA6 file
	bool     hadAT = false;               // frame had an ATST block
	int8_t   fsn[4] = {-1,-1,-1,-1};      // FSNH FSNA FSNE FSNI values as loaded (-1 = absent)
	int16_t  asPool = -1;                 // own ASST block: its index in the pattern's AS pool
	int16_t  asRef = -1;                  // ASSM index as loaded
	int16_t  boxPool[kMaxBoxes];          // full box (HRNM/HRAT): its index in the pattern's box pool
	int16_t  boxRef[kMaxBoxes];           // reference (HRNS/HRAS): the pool index it pointed at
	int32_t  boxXY[kMaxBoxes][4];         // box coordinates as loaded
	uint64_t boxMask = 0;                 // boxes present at load (bit = HA6 location)
	uint8_t  nEF = 0, nIF = 0;            // EF/IF count at load (slots valid while unchanged)
	int8_t   efSlot[kMaxSlots];           // EFST index per EF, in load order
	int8_t   ifSlot[kMaxSlots];           // IFST index per IF, in load order
	uint8_t  nExtra = 0;
	Ha6ExtraTag extra[kMaxExtra]{};
	// Tags present at load, as bits over the ha6order lists. A present tag is
	// written back even when its value equals the "absent" default.
	uint32_t afTags = 0;
	uint16_t layerTags[5] = {0,0,0,0,0};
	uint32_t asTags = 0;
	uint32_t atTags = 0;

	void markAF(const void *tag, int layer)
	{
		if (layer >= 0) {
			int i = ha6order::Index(ha6order::kLayer, tag);
			if (i >= 0 && layer < 5) { layerTags[layer] |= (uint16_t)(1u << i); return; }
		}
		int i = ha6order::Index(ha6order::kAF, tag);
		if (i >= 0) afTags |= 1u << i;
	}
	void markAS(const void *tag) { int i = ha6order::Index(ha6order::kAS, tag); if (i >= 0) asTags |= 1u << i; }
	void markAT(const void *tag) { int i = ha6order::Index(ha6order::kAT, tag); if (i >= 0) atTags |= 1u << i; }

	Ha6FrameEnc()
	{
		for (int i = 0; i < kMaxBoxes; ++i) { boxPool[i] = -1; boxRef[i] = -1; }
		memset(boxXY, 0, sizeof(boxXY));
		for (int i = 0; i < kMaxSlots; ++i) { efSlot[i] = (int8_t)i; ifSlot[i] = (int8_t)i; }
	}

	bool addExtra(const char *tag, uint8_t block, const int32_t *words, int n)
	{
		if (nExtra >= kMaxExtra || n < 0 || n > 7) return false;
		Ha6ExtraTag &x = extra[nExtra++];
		memcpy(x.tag, tag, 4);
		x.block = block;
		x.nwords = (uint8_t)n;
		for (int i = 0; i < n; ++i) x.w[i] = words[i];
		return true;
	}
};

struct Ha6SeqEnc
{
	bool     valid = false;               // pattern came from an HA6 file
	bool     hasPTT2 = false;
	bool     hasPTCN = false;
	bool     hadPTIT = false;
	uint32_t ptt2Len = 0;
	uint8_t  ptt2[64]{};                  // raw PTT2 name buffer (MBTL/UNI2 keep stale bytes after the NUL)
	uint32_t ptcnLen = 0;
	uint8_t  ptcn[64]{};                  // raw PTCN buffer
	uint32_t pds2Unused = 0;              // PDS2 word 5 (always 0 in shipped files)
	bool     utf8Names = false;           // names were UTF-8 (old Hantei-chan header flag): raw buffers not reusable
};

#endif /* HA6_ENC_H_GUARD */
