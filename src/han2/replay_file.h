#ifndef HAN2_REPLAY_FILE_H_GUARD
#define HAN2_REPLAY_FILE_H_GUARD

// RBO replay (.REP RBO v1, .RP2 Ex1 v3, .RP3 Ex2 / .RP4 Ex3 v10). Written by Replay_WriteFile (rbo_ex3.exe 0x438AD0; v3 sub_437E20 in rbo_ex1,
// v1 sub_4339D0 in rbo.exe), read back by the matching loaders (v3 sub_437CA0 / v1 sub_433850), which also validate the checksum:
//   u32 version | header[H] | extra[E] | u32 seed | u32 checksum | u32 tickCount | inputs[216000 ticks x 3 players]
//   H = 4324 (v1) / 4932 = sizeof(RboReplayHeader) (v3, v10); E = 48 (v1) / 52; inputs: v1,v3 = 3 x u16 per tick (0x13C680 bytes), v10 = 3 x u8 (0x9E340).
//   header and extra are enciphered with the stream seeded by `seed`: dword i ^= Rng_NextFromState(&s) (Sv_SumThenXorEncode 0x46B140), the PLAIN
//   dwords are summed; checksum = sum ^ Rng_NextFromState(&s) after both blocks.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 { namespace rep {

constexpr uint32_t kMaxTicks = 216000;   // 0x34BC0
constexpr int kPlayers = 3;

struct Replay {
	uint32_t version = 0;
	std::vector<uint8_t> header;          // decoded (plain) header, H bytes
	std::vector<uint8_t> extra;           // decoded (plain) extra block, E bytes
	uint32_t seed = 0, checksum = 0, tickCount = 0;
	std::vector<uint16_t> inputs;         // kMaxTicks * kPlayers held-button words (u8 values for v10), tick-major
	// typed view of RboReplayHeader (valid when header.size() == 4932)
	uint32_t flags = 0; int32_t gameMode = 0, stageId = 0, operatorSeat = 0, seatClass[3]{};
	uint32_t rngStream0Seed = 0; int32_t sessionClearBracket = 0;
	bool checksumOk = false;
};

bool Parse(const uint8_t *p, size_t n, Replay &out, std::string *err);
bool Serialize(const Replay &r, std::vector<uint8_t> &out, std::string *err);

}} // namespace
#endif
