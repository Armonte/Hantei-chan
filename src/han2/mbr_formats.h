#ifndef HAN2_MBR_FORMATS_H_GUARD
#define HAN2_MBR_FORMATS_H_GUARD

// Melty Blood Re-ACT (mbr.exe) satellite files, typed by the IDB structs (docs/formats/mbr.md, docs/formats/ida/mbr_types.h, generated twins in
// mbr_types_gen.h): <CHAR>_C.CT command table, <CHAR>.WMT win quotes, <CHAR>.CPF CPU script, CHARASELECT.CT (enciphered character table).
// Every Parse is exact (size and structure checked) and every Serialize rebuilds the file from the typed fields.
#include "mbr_types_gen.h"
#include <string>
#include <vector>

namespace han2 { namespace mbr {

// ---- <CHAR>_C.CT: u32 count | MbrCommandMove[200] | MbrCtParams (8896 bytes) ------------------------------------------------------------
struct CtFile { uint32_t count = 0; MbrCommandMove commands[200]; MbrCtParams params; };
constexpr size_t kCtSize = 4 + 200 * sizeof(MbrCommandMove) + sizeof(MbrCtParams);
bool ParseCt(const uint8_t *p, size_t n, CtFile &out, std::string *err);
void SerializeCt(const CtFile &c, std::vector<uint8_t> &out);

// ---- <CHAR>.WMT: u32 count | count x MbrWmtRecord (262 bytes) --------------------------------------------------------------------------------
struct WmtFile { uint32_t count = 0; std::vector<MbrWmtRecord> records; };
bool ParseWmt(const uint8_t *p, size_t n, WmtFile &out, std::string *err);
void SerializeWmt(const WmtFile &w, std::vector<uint8_t> &out);

// ---- <CHAR>.CPF: the whole MbrCpfFile (193388 bytes) -----------------------------------------------------------------------------------------
bool ParseCpf(const uint8_t *p, size_t n, MbrCpfFile &out, std::string *err);
void SerializeCpf(const MbrCpfFile &f, std::vector<uint8_t> &out);

// ---- CHARASELECT.CT: u32 count | u32 header[9] | count x MbrCharEntry (196 bytes), entry block enciphered with a 24-byte key ----------------------
struct CharaSelectFile { uint32_t count = 0; uint32_t header[9]{}; std::vector<MbrCharEntry> entries; };
bool ParseCharaSelect(const uint8_t *p, size_t n, CharaSelectFile &out, std::string *err);
void SerializeCharaSelect(const CharaSelectFile &c, std::vector<uint8_t> &out);

}} // namespace han2::mbr
#endif
