#include "dmp_fob.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace han2 { namespace dmpfob {

static uint16_t R16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void W16(std::vector<uint8_t> &o, uint16_t v) { o.insert(o.end(), (uint8_t *)&v, (uint8_t *)&v + 2); }
static void W32(std::vector<uint8_t> &o, uint32_t v) { o.insert(o.end(), (uint8_t *)&v, (uint8_t *)&v + 4); }

enum Form : uint8_t { kNone = 0, kImm, kFl, kFlk, kJcc, kSw, kData, kUnknown };

// opcodes the dispatcher knows plus the three that occur in shipped scripts without a handler (docs/formats/dmp.md 1.3)
static bool IsKnownOp(uint16_t op)
{
	if (op >= 0x01 && op <= 0x51) return op != 0x1D;
	switch (op) {
	case 0x54: case 0x55: case 0x57: case 0x58: case 0x59: case 0x5B: case 0x5C: case 0x61: case 0x62: case 0x63: case 0x67: case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6E: case 0x6F: case 0x70:
	case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: case 0x7E: case 0x7F: case 0x9D: case 0x9F: case 0xA0: case 0xA1: case 0x90: case 0x91: return true;
	default: return (op >= 0x80 && op <= 0x8F) || (op >= 0x92 && op <= 0x99);
	}
}

static Form FormOfDmp(uint16_t op)
{
	switch (op) {
	case 0x01: case 0x02: return kData;
	case 0x03: case 0x04: case 0x1A: case 0x1B: case 0x26: case 0x2F: case 0x51: return kImm;
	case 0x09: case 0x18: return kFlk;
	case 0x19: return kJcc;
	case 0x25: return kSw;
	default: break;
	}
	if ((op >= 0x0A && op <= 0x17) || op == 0x27) return kFl;
	return IsKnownOp(op) ? kNone : kUnknown;   // every other known opcode (including the three the engine has no case for: 0x90, 0x91, 0xA1) is a bare 2-byte op
}

// QoH '99 (docs/formats/qoh99.md section 6): 0x01/0x02 data, 0x0A compound assign, 0x0B..0x13/0x16/0x17 binary (flags), 0x18 compare, 0x1A/0x1B push imm / code address, 0x28 switch,
// 0x2D conditional jump, 0x2E jump, 0x2F call, 0x3C line; the rest of the known opcodes are bare 2-byte ops
static Form FormOfQoh(uint16_t op)
{
	switch (op) {
	case 0x01: case 0x02: return kData;
	case 0x0A: case 0x18: return kFlk;
	case 0x1A: case 0x1B: case 0x2E: case 0x2F: case 0x3C: return kImm;
	case 0x2D: return kJcc;
	case 0x28: return kSw;
	default: break;
	}
	if ((op >= 0x0B && op <= 0x13) || op == 0x16 || op == 0x17) return kFl;
	if (op == 0x09 || (op >= 0x1C && op <= 0x1E) || op == 0x30 || op == 0x33 || op == 0x34 || op == 0x36 || (op >= 0x3D && op <= 0x6B)) return kNone;
	return kUnknown;
}

// Rosa: opcode N == dMp opcode N below 0x20, dMp opcode N + 1 from 0x20 on; natives 0x56..0x69 are bare ops and 0x69 is the highest
static Form FormOfRosa(uint16_t op)
{
	if (op == 0 || op == 0x1D || op > 0x69) return kUnknown;
	if (op < 0x20) return FormOfDmp(op);
	const Form f = FormOfDmp((uint16_t)(op + 1));
	return f == kUnknown ? kNone : f;
}

static Form FormOf(Dialect d, uint16_t op) { return d == Dialect::Qoh99 ? FormOfQoh(op) : d == Dialect::Rosa ? FormOfRosa(op) : FormOfDmp(op); }

static bool IsTerminatorDmp(uint16_t op) { return op == 0xA1 || op == 0x1A || op == 0x1E || op == 0x1F || op == 0x21 || op == 0x4B || op == 0x54 || op == 0x2D || op == 0x55; }

static bool IsTerminator(Dialect d, uint16_t op)
{
	if (d == Dialect::Rosa) return op == 0x1A || op == 0x1E || op == 0x1F || op == 0x20 || op == 0x2C || op == 0x4A || op == 0x53 || op == 0x54;
	if (d == Dialect::Qoh99) return op == 0x2E || op == 0x28 || op == 0x30 || op == 0x6A || op == 0x6B;
	return IsTerminatorDmp(op);
}

static size_t InsnLen(Dialect d, const uint8_t *code, size_t size, uint32_t pc)
{
	if (pc + 2 > size) return 0;
	switch (FormOf(d, R16(code + pc))) {
	case kNone: return 2; case kImm: return 6; case kFl: return 4; case kFlk: return 6; case kJcc: return 10; case kSw: return 8;
	case kData: { if (pc + 6 > size) return 0; uint64_t n = 6 + 4ull * R32(code + pc + 2); return pc + n <= size ? (size_t)n : 0; }
	default: return 0;
	}
}

static Insn Decode(Dialect d, const uint8_t *c, uint32_t pc)
{
	Insn i; i.pc = pc; i.op = R16(c + pc);
	switch (FormOf(d, i.op)) {
	case kImm: i.imm = R32(c + pc + 2); break;
	case kFl: i.flags = R16(c + pc + 2); break;
	case kFlk: i.flags = R16(c + pc + 2); i.kind = R16(c + pc + 4); break;
	case kJcc: i.flags = R16(c + pc + 2); i.kind = R16(c + pc + 4); i.imm = R32(c + pc + 6); break;
	case kSw: i.flags = R16(c + pc + 2); i.imm = R32(c + pc + 4); break;
	case kData: { i.imm = R32(c + pc + 2); for (uint32_t k = 0; k < i.imm; k++) i.data.push_back(R32(c + pc + 6 + 4 * k)); break; }
	default: break;
	}
	return i;
}

static void Encode(Dialect d, const Insn &i, std::vector<uint8_t> &o)
{
	W16(o, i.op);
	switch (FormOf(d, i.op)) {
	case kImm: W32(o, i.imm); break;
	case kFl: W16(o, i.flags); break;
	case kFlk: W16(o, i.flags); W16(o, i.kind); break;
	case kJcc: W16(o, i.flags); W16(o, i.kind); W32(o, i.imm); break;
	case kSw: W16(o, i.flags); W32(o, i.imm); break;
	case kData: W32(o, (uint32_t)i.data.size()); for (uint32_t v : i.data) W32(o, v); break;
	default: break;
	}
}

bool Parse(const uint8_t *p, size_t n, File &f, std::string *err, Dialect d)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	f = File(); f.dialect = d;
	if (n < 8) return fail("too short");
	const uint32_t nf = R32(p);
	if (nf > 4096 || 4 + 36ull * nf + 4 > n) return fail("function table runs past the file");
	size_t q = 4;
	for (uint32_t i = 0; i < nf; i++) { FuncEntry e; memcpy(e.name, p + q, 32); e.pc = R32(p + q + 32); f.funcs.push_back(e); q += 36; }
	f.codeSize = R32(p + q); q += 4;
	if ((uint64_t)q + f.codeSize > n) return fail("code block runs past the file");
	const uint8_t *code = p + q; const size_t cs = f.codeSize;
	f.tail.assign(p + q + cs, p + n);
	const bool qoh = d == Dialect::Qoh99;
	// branch classes per dialect: conditional jump (target @+6), jump / call (target @+2), switch (table pc @+4)
	auto isJcc = [&](uint16_t op) { return qoh ? op == 0x2D : op == 0x19; };
	auto isJmpCall = [&](uint16_t op) { return qoh ? (op == 0x2E || op == 0x2F) : (op == 0x1A || op == 0x1B); };
	auto isSwitch = [&](uint16_t op) { return qoh ? op == 0x28 : d == Dialect::Rosa ? op == 0x24 : op == 0x25; };
	std::map<uint32_t, size_t> seen;
	std::vector<uint8_t> covered(cs, 0), table(cs, 0);
	std::vector<uint32_t> work; work.push_back(0); for (auto &e : f.funcs) work.push_back(e.pc);
	auto descend = [&]() {
		while (!work.empty()) {
			uint32_t pc = work.back(); work.pop_back();
			for (;;) {
				if (pc >= cs || seen.count(pc)) break;
				const size_t len = InsnLen(d, code, cs, pc);
				if (!len) break;
				bool overlap = false; for (size_t k = 0; k < len; k++) if (covered[pc + k] || table[pc + k]) { overlap = true; break; }
				if (overlap) break;
				seen[pc] = len; for (size_t k = 0; k < len; k++) covered[pc + k] = 1;
				const uint16_t op = R16(code + pc);
				if (isJcc(op)) work.push_back(R32(code + pc + 6));
				else if (isJmpCall(op)) work.push_back(R32(code + pc + 2));
				else if (isSwitch(op)) {
					const uint32_t t = R32(code + pc + 4);
					if ((uint64_t)t + 8 <= cs) {
						work.push_back(R32(code + t)); const uint32_t cnt = R32(code + t + 4);
						uint64_t end = (uint64_t)t + 8;
						for (uint32_t k = 0; k < cnt && (uint64_t)t + 8 + 8ull * k + 8 <= cs; k++) { work.push_back(R32(code + t + 8 + 8 * k + 4)); end = (uint64_t)t + 8 + 8ull * (k + 1); }
						for (uint64_t k = t; k < end && k < cs; k++) table[k] = 1;   // the table is data
					}
				}
				if (IsTerminator(d, op)) break;
				pc += (uint32_t)len;
			}
		}
	};
	descend();
	if (qoh) {   // dead code behind unconditional jumps: decode every gap linearly (the tables are already marked as data), then follow what it branches to
		for (int round = 0; round < 8; round++) {
			bool any = false;
			uint32_t pc = 0;
			while (pc < cs) {
				if (covered[pc] || table[pc]) { pc++; continue; }
				const size_t len = InsnLen(d, code, cs, pc);
				bool ok = len != 0; if (ok) for (size_t k = 0; k < len; k++) if (covered[pc + k] || table[pc + k]) { ok = false; break; }
				if (!ok) { pc++; continue; }
				work.push_back(pc); any = true; pc += (uint32_t)len;
			}
			if (!any) break;
			descend();
		}
	}
	uint32_t pos = 0;
	auto flushRaw = [&](uint32_t end) { if (end > pos) { Item it; it.pc = pos; it.raw.assign(code + pos, code + end); f.rawBytes += end - pos; f.items.push_back(std::move(it)); pos = end; } };
	for (auto &kv : seen) {
		flushRaw(kv.first);
		Item it; it.isInsn = true; it.pc = kv.first; it.insn = Decode(d, code, kv.first); f.items.push_back(std::move(it)); f.nInsns++; pos = (uint32_t)(kv.first + kv.second);
	}
	flushRaw((uint32_t)cs);
	return true;
}

void Serialize(const File &f, std::vector<uint8_t> &o)
{
	o.clear(); W32(o, (uint32_t)f.funcs.size());
	for (auto &e : f.funcs) { o.insert(o.end(), e.name, e.name + 32); W32(o, e.pc); }
	W32(o, f.codeSize);
	for (auto &it : f.items) { if (it.isInsn) Encode(f.dialect, it.insn, o); else o.insert(o.end(), it.raw.begin(), it.raw.end()); }
	o.insert(o.end(), f.tail.begin(), f.tail.end());
}

static const char *OpNameQoh(uint16_t op)
{
	switch (op) {
	case 0x01: return "DATA_DWORDS"; case 0x02: return "DATA_STRING"; case 0x09: return "SWAP"; case 0x0A: return "ASSIGN"; case 0x0B: return "ADD"; case 0x0C: return "SUB"; case 0x0D: return "MUL";
	case 0x0E: return "DIV"; case 0x0F: return "MOD"; case 0x10: return "SHL"; case 0x11: return "SHR"; case 0x12: return "AND"; case 0x13: return "OR"; case 0x16: return "LAND"; case 0x17: return "LOR";
	case 0x18: return "CMP"; case 0x1A: return "PUSH_IMM"; case 0x1B: return "PUSH_CODE_ADDR"; case 0x1C: return "PUSH_RETURN_VALUE_ADDR"; case 0x1D: return "PUSH_GLOBALS_ADDR"; case 0x1E: return "DROP";
	case 0x28: return "SWITCH"; case 0x2D: return "JCC"; case 0x2E: return "JMP"; case 0x2F: return "CALL"; case 0x30: return "RET"; case 0x33: return "IRAND"; case 0x34: return "YIELD"; case 0x36: return "YIELD2";
	case 0x3C: return "LINE"; case 0x3D: return "AI_TRY_BLOCK"; case 0x3E: return "AI_TRY_SPECIAL"; case 0x3F: return "AI_TRY_THROW"; case 0x40: return "AI_TRY_JUMP"; case 0x41: return "POP_RETURN_VALUE";
	case 0x42: return "AI_ARM_SLOT"; case 0x43: return "AI_WAIT_DIR_20000"; case 0x44: return "AI_WAIT_DIR_10000"; case 0x45: return "AI_WAIT_DIR_80000"; case 0x46: return "AI_DEFAULT_KEY_DATA";
	case 0x6A: case 0x6B: return "HALT";
	default: return op >= 0x47 && op <= 0x69 ? "AI_CHARACTER_HANDLER" : "";
	}
}

const char *OpName(uint16_t op, Dialect d)
{
	if (d == Dialect::Qoh99) return OpNameQoh(op);
	if (d == Dialect::Rosa && op >= 0x20) op = (uint16_t)(op + 1);
	switch (op) {
	case 0x01: return "DATA_DWORDS"; case 0x02: return "DATA_STRING"; case 0x03: return "PUSH_IMM"; case 0x04: return "PUSH_CODE_ADDR"; case 0x05: return "PUSH_SCRATCH_ADDR";
	case 0x06: return "PUSH_ARGS_ADDR"; case 0x07: return "PUSH_CONTEXT"; case 0x08: return "POP"; case 0x09: return "STORE"; case 0x0A: return "ADD"; case 0x0B: return "SUB";
	case 0x0C: return "MUL"; case 0x0D: return "DIV"; case 0x0E: return "MOD"; case 0x0F: return "SHL"; case 0x10: return "SHR"; case 0x11: return "AND"; case 0x12: return "OR";
	case 0x13: return "NOT"; case 0x14: return "XOR"; case 0x15: return "LNOT"; case 0x16: return "LAND"; case 0x17: return "LOR"; case 0x18: return "CMP"; case 0x19: return "JCC";
	case 0x1A: return "JMP"; case 0x1B: return "CALL"; case 0x1C: return "CALL_NAMED"; case 0x1E: return "RET_FAR"; case 0x1F: return "RET"; case 0x20: return "YIELD"; case 0x21: return "EXIT_THREAD";
	case 0x22: return "SPAWN_THREAD"; case 0x23: return "KILL_THREAD"; case 0x24: return "SWAP"; case 0x25: return "SWITCH"; case 0x26: return "DEBUG_PRINT"; case 0x27: return "LOAD";
	case 0x28: return "RAND_RANGE"; case 0x29: return "RAND_LCG2"; case 0x2A: return "RNG_SET_STATE"; case 0x2B: return "RNG_GET_STATE"; case 0x2C: return "WAIT_FRAMES_REALTIME";
	case 0x2D: return "SPIN_NO_ADVANCE"; case 0x2E: return "YIELD_OR_END"; case 0x2F: return "LINE"; case 0x4B: return "RET_VALUE"; case 0x51: return "FORMAT_PRINT"; case 0x54: return "END";
	case 0x55: return "SPIN_NO_ADVANCE_55"; case 0x57: return "STAGE_ENEMY_LIST"; case 0x58: return "ENEMY_SPAWN"; case 0x5C: return "MATH_ANGLE"; case 0x61: return "ENTITY_MOTION";
	case 0x62: return "ENTITY_CALL_COLLIDE"; case 0x63: return "NEXT_UNIQUE_ID"; case 0x73: return "GFX_ALLOC_IMAGE_TABLE"; case 0x74: return "GFX_ALLOC_SPRITE_TABLE"; case 0x75: return "GFX_SET_IMAGE";
	case 0x76: return "GFX_DEFINE_SPRITE"; case 0x77: return "GFX_DRAW_SPRITE"; case 0x7E: return "PLAYER_SLOT_COMMAND"; case 0x7F: return "GAME_QUERY"; case 0x85: return "AUDIO_DEFINE_CHANNEL";
	case 0x86: return "AUDIO_SET_LEVELS"; case 0x87: return "AUDIO_SET_STATE"; case 0x9D: return "SOUND_CHANNEL_SET_UNIQUE"; case 0xA1: return "END_OF_CODE";
	default: return "";
	}
}

std::string Disassemble(const File &f)
{
	std::string out; char b[160];
	std::map<uint32_t, std::string> labels; for (auto &e : f.funcs) labels[e.pc] = std::string((const char *)e.name, strnlen((const char *)e.name, 32));
	for (auto &it : f.items) {
		auto lb = labels.find(it.pc); if (lb != labels.end()) out += lb->second + ":\n";
		if (!it.isInsn) { snprintf(b, sizeof b, "  %05x  .data %zu bytes\n", it.pc, it.raw.size()); out += b; continue; }
		const Insn &i = it.insn; const char *nm = OpName(i.op, f.dialect);
		snprintf(b, sizeof b, "  %05x  %-24s", i.pc, nm[0] ? nm : ("op" + std::to_string(i.op)).c_str()); out += b;
		switch (FormOf(f.dialect, i.op)) {
		case kImm: snprintf(b, sizeof b, " %#x", i.imm); out += b; break;
		case kFl: snprintf(b, sizeof b, " flags=%#x", i.flags); out += b; break;
		case kFlk: snprintf(b, sizeof b, " flags=%#x kind=%#x", i.flags, i.kind); out += b; break;
		case kJcc: snprintf(b, sizeof b, " flags=%#x kind=%#x -> %#x", i.flags, i.kind, i.imm); out += b; break;
		case kSw: snprintf(b, sizeof b, " flags=%#x table=%#x", i.flags, i.imm); out += b; break;
		case kData: snprintf(b, sizeof b, " n=%zu", i.data.size()); out += b; break;
		default: break;
		}
		out += "\n";
	}
	return out;
}

}} // namespace
