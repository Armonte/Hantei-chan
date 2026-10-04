#ifndef HAN2_MB_FORMATS_H_GUARD
#define HAN2_MB_FORMATS_H_GUARD

// Melty Blood (2002, mb.exe) satellite files typed by the IDB structs (docs/formats/mb.md, docs/formats/ida/mb_types.h, generated twins in mb_types_gen.h):
// <CHAR>_C.CT command table, the older 42/44-byte-command CT variants that this build never loads, <CHAR>.WMT, <CHAR>.CPF, CHARSEL.CT (enciphered).
#include "mb_types_gen.h"
#include <string>
#include <vector>

namespace han2 { namespace mb {

struct CtFile { MbCtFile f; };                     // 4496 bytes: u32 count | MbCtCommand[100] | MbCtHeader
bool ParseCt(const uint8_t *p, size_t n, CtFile &out, std::string *err);
void SerializeCt(const CtFile &c, std::vector<uint8_t> &out);

// Older table variants shipped as ARC_C.CT2 / HIS_KOH_C.CT / MIYKO_C.CT (4232 B: 42-byte commands) and HISKOH_C.CT (4432 B: the same commands zero padded to 44):
// u32 count | 100 commands | 28-byte header (MbCtHeader without the evade table). Not loaded by mb.exe.
#pragma pack(push, 1)
struct OldCtCommand {                              // 42 bytes: MbCtCommand without usageLimit and the trailing byte
	uint8_t commandId, unused_01; uint8_t sequence[32]; uint8_t targetPattern, moveClass; uint16_t meterCost; uint8_t requirePartner, counterRequirement, flags, flags2;
};
struct OldCtHeader {                               // 28 bytes
	uint8_t maxAirJumps, unused_01, recoveryStyle, unused_03, unused_04, flags, unused_06[2]; float knockbackScale; float damageScale[4];
};
#pragma pack(pop)
static_assert(sizeof(OldCtCommand) == 42 && sizeof(OldCtHeader) == 28, "old CT layout");
struct OldCtFile { uint32_t count = 0; bool padded44 = false; std::vector<OldCtCommand> commands; std::vector<uint8_t> pad; OldCtHeader header; };
bool ParseOldCt(const uint8_t *p, size_t n, OldCtFile &out, std::string *err);
void SerializeOldCt(const OldCtFile &c, std::vector<uint8_t> &out);

struct WmtFile { uint32_t count = 0; std::vector<MbWmtRecord> records; };   // u32 count | count x 156
bool ParseWmt(const uint8_t *p, size_t n, WmtFile &out, std::string *err);
void SerializeWmt(const WmtFile &w, std::vector<uint8_t> &out);

bool ParseCpf(const uint8_t *p, size_t n, MbCpfFile &out, std::string *err);   // 193388 bytes
void SerializeCpf(const MbCpfFile &f, std::vector<uint8_t> &out);

struct CharSelFile { uint32_t count = 0; std::vector<MbCharSelEntry> entries; };   // u32 count | count x 136, block enciphered (block[p] ^= p + key[p % 24])
bool ParseCharSel(const uint8_t *p, size_t n, CharSelFile &out, std::string *err);
void SerializeCharSel(const CharSelFile &c, std::vector<uint8_t> &out);

}} // namespace han2::mb
#endif
