#ifndef HAN2_FOB_FILE_H_GUARD
#define HAN2_FOB_FILE_H_GUARD

// French-Bread .FOB script bytecode (RBO and GOF2). Layout and instruction forms: docs/formats/fob_vm.md sections 1 and 3
// (Script_LoadIntoBank 0x4248B0, Script_RunInterpreter 0x428AA0). This is a structured model: file header (named entry points, index tables),
// then the code block as a sequence of DECODED instructions (class, sub, typed operands) plus raw spans for code bytes that no instruction
// reaches (data addressed through PUSH_CODE_ADDR). Serialize rebuilds every byte from the fields.
#include <cstdint>
#include <string>
#include <vector>

namespace han2 { namespace fob {

struct FuncEntry { uint8_t name[32]; uint32_t pc = 0; };   // name slot kept verbatim (bytes after the NUL included)

enum : uint16_t { kClsData = 0, kClsStk = 1, kClsFlow = 2, kClsSys = 3, kClsEnd = 0x18, kClsHalt = 0x19 };

struct Insn {
	uint32_t pc = 0;
	uint16_t cls = 0, sub = 0;
	uint16_t flags = 0, kind = 0;      // STK.STORE/CMP/arith, FLOW.JCC/SWITCH
	uint32_t imm = 0;                  // PUSH_IMM / code offset / jump target / switch table pc / DATA count / line number
	std::vector<uint32_t> data;        // DATA.0/1 payload (n dwords)
};

struct Item { bool isInsn = true; bool reached = true; Insn insn;   // reached=false: decoded linearly from a gap no control flow enters (dead code / data that decodes)
	 std::vector<uint8_t> raw; uint32_t pc = 0; };   // raw: bytes not reached by any instruction

struct File {
	std::vector<FuncEntry> funcs;
	std::vector<std::vector<int32_t>> indexTables;
	uint32_t codeSize = 0;
	std::vector<Item> items;           // covers the whole code block, ordered by pc, no gaps, no overlaps
	std::vector<uint8_t> tail;         // bytes after the code block (none in any shipped file)
	// statistics from Parse
	size_t nInsns = 0, nUnreachedInsns = 0, rawBytes = 0, overlapsDropped = 0, decodeErrors = 0;
};

// Length and operands of the instruction at `pc`; false for an undecodable word (never a crash).
bool DecodeInsn(const uint8_t *code, size_t size, uint32_t pc, Insn &out, uint32_t &len);
void EncodeInsn(const Insn &in, std::vector<uint8_t> &out);

bool Parse(const uint8_t *p, size_t n, File &out, std::string *err);
bool Serialize(const File &f, std::vector<uint8_t> &out, std::string *err);

}} // namespace
#endif
