#ifndef HAN2_RAW_H_GUARD
#define HAN2_RAW_H_GUARD

// Per-frame / per-pattern source bytes of a French-Bread HAN2RBO character (RBO .DAT/.DT2, GOF2 .DT2).
//
// Same idea as ha4_raw.h: the loader maps the frame onto the HA6 model so every view works, and keeps the original
// records here. The writer starts from these bytes and re-encodes a field only when the model value differs from what
// the original bytes decode to, so an unedited file saves byte-identically (including the 0xCDCD debug fill and every
// field Hantei-chan has no model for). Plain fixed-size POD: Frame_T is copied across allocators.
// Layout and evidence: docs/formats/frenchbread_rbo_gof.md, docs/formats/ida/*.h.

#include <cstdint>
#include <cstring>

namespace han2 {
constexpr int kMaxFrameBytes = 404;   // GOF2; RBO is 300
constexpr int kMaxAtBytes = 240;
constexpr int kEffectBytes = 96;
constexpr int kScriptListBytes = 20;
constexpr int kMaxBoxSlots = 24;
}

struct Han2FrameRaw
{
	bool     valid = false;
	uint16_t frameSize = 0;                       // 300 or 404
	uint8_t  rec[han2::kMaxFrameBytes]{};         // frame record; the table indices inside are stale (rebuilt on save)
	bool     hadAT = false;
	uint8_t  at[han2::kMaxAtBytes]{};             // referenced AT record
	uint8_t  scriptHad = 0;                       // bit k set = script list k existed
	uint8_t  script[3][han2::kScriptListBytes]{}; // referenced script-list records (A, B, C)
	bool     hadFx = false;                       // GOF2: effect-spawn record (section 8) referenced by frame +0x190
	uint8_t  fx[han2::kEffectBytes]{};
	uint8_t  gofIf[3][28]{};                      // GOF1: IF records (type + 4 ints), slots 0..2
	uint8_t  gofEf[4][20]{};                      // GOF1: EF records, slots 0..3
	uint8_t  gofIfMask = 0, gofEfMask = 0;
	int16_t  box[han2::kMaxBoxSlots][4]{};        // referenced rectangles by box slot (layout order)
	uint32_t boxMask = 0;                         // bit k set = box slot k was present
	uint32_t alignPad = 0;                        // keeps sizeof a multiple of 8 (undo_manager.cpp layout assert)
};

struct Han2SeqRaw
{
	bool     valid = false;                       // pattern existed in the source file
	bool     nameValid = false;
	uint32_t patFlags = 0;                        // pattern-table entry word +4
	uint32_t firstFrame = 0;                      // pattern-table entry word +8 (only kept for empty patterns)
	uint8_t  gofHdr[20]{};                        // GOF1: pattern header (frame count, move info/level, frame size tag, table offsets)
	uint8_t  name[64]{};                          // raw CP932 name slot
};

#endif /* HAN2_RAW_H_GUARD */
