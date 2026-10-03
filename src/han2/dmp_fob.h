#ifndef HAN2_DMP_FOB_H_GUARD
#define HAN2_DMP_FOB_H_GUARD

// Drill Milky Punch (DMP.EXE, 2003) .FOB script bank. Spec: docs/formats/dmp.md section 1 (Script_LoadBank 0x412D40, Script_RunThread 0x415D80).
//   u32 nFuncs | { char name[32]; u32 pc }[nFuncs] | u32 codeSize | u8 code[codeSize]      (no index tables, unlike RBO / GOF2)
// The code block is decoded by recursive descent from every named entry point and pc 0 (the LINE-marker preamble); bytes no instruction reaches (string and
// table data addressed through PUSH_CODE_ADDR) stay raw spans. Serialize rebuilds every byte from the fields.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 { namespace dmpfob {

struct FuncEntry { uint8_t name[32]; uint32_t pc = 0; };   // the whole 32-byte slot kept verbatim (bytes after the NUL are stale writer junk)

struct Insn {
	uint32_t pc = 0; uint16_t op = 0;
	uint16_t flags = 0, kind = 0;       // STORE/CMP/JCC: flags + kind; arithmetic and LOAD: flags; SWITCH: flags
	uint32_t imm = 0;                   // PUSH_IMM / PUSH_CODE_ADDR / JMP / CALL target / LINE / DEBUG_PRINT / FORMAT_PRINT / JCC target / SWITCH table pc / DATA count
	std::vector<uint32_t> data;         // DATA_DWORDS / DATA_STRING payload
};
struct Item { bool isInsn = false; Insn insn; std::vector<uint8_t> raw; uint32_t pc = 0; };

struct File {
	std::vector<FuncEntry> funcs;
	uint32_t codeSize = 0;
	std::vector<Item> items;            // tiles the whole code block in pc order
	std::vector<uint8_t> tail;          // bytes after the code block (none in any shipped file)
	size_t nInsns = 0, rawBytes = 0;
};

const char *OpName(uint16_t op);        // DMPOP_* name or "" when unknown
bool Parse(const uint8_t *p, size_t n, File &out, std::string *err);
void Serialize(const File &f, std::vector<uint8_t> &out);
std::string Disassemble(const File &f); // one line per instruction with function labels

}} // namespace
#endif
