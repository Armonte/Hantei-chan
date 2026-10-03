#include "fob_file.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace han2 { namespace fob {

static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t R16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static void W32(std::vector<uint8_t> &o, uint32_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 4); }
static void W16(std::vector<uint8_t> &o, uint16_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 2); }

bool DecodeInsn(const uint8_t *code, size_t size, uint32_t pc, Insn &o, uint32_t &len)
{
	o = Insn(); o.pc = pc;
	if ((uint64_t)pc + 2 > size) return false;
	const uint8_t *p = code + pc;
	o.cls = R16(p);
	if (o.cls == kClsEnd || o.cls == kClsHalt) { len = 2; return true; }
	if (o.cls > 0x17 || (uint64_t)pc + 4 > size) return false;
	o.sub = R16(p + 2);
	auto need = [&](uint32_t l) { len = l; return (uint64_t)pc + l <= size; };
	switch (o.cls) {
	case kClsData: {
		if (o.sub > 1 || !need(8)) return false;
		uint32_t n = R32(p + 4); o.imm = n;
		if (n > (size - pc) / 4) return false;
		if (!need(8 + 4 * n)) return false;
		o.data.resize(n); for (uint32_t i = 0; i < n; i++) o.data[i] = R32(p + 8 + 4 * i);
		return true; }
	case kClsStk:
		if (o.sub <= 1) { if (!need(8)) return false; o.imm = R32(p + 4); return true; }
		if ((o.sub >= 2 && o.sub <= 9) || o.sub == 0x1A) return need(4);
		if (o.sub == 0x0A || o.sub == 0x19) { if (!need(8)) return false; o.flags = R16(p + 4); o.kind = R16(p + 6); return true; }
		if (o.sub >= 0x0B && o.sub <= 0x1B) { if (!need(6)) return false; o.flags = R16(p + 4); return true; }
		return false;
	case kClsFlow:
		if (o.sub == 0) { if (!need(12)) return false; o.flags = R16(p + 4); o.kind = R16(p + 6); o.imm = R32(p + 8); return true; }
		if (o.sub == 1) { if (!need(10)) return false; o.flags = R16(p + 4); o.imm = R32(p + 6); return true; }
		if (o.sub == 2 || o.sub == 3) { if (!need(8)) return false; o.imm = R32(p + 4); return true; }
		if (o.sub >= 4 && o.sub <= 7) return need(4);
		return false;
	case kClsSys:
		if (o.sub == 0 || o.sub == 2 || o.sub == 5) { if (!need(8)) return false; o.imm = R32(p + 4); return true; }
		if (o.sub == 1 || o.sub == 3 || o.sub == 4) return need(4);
		return false;
	default: return need(4);   // every native (classes 4..0x17): class + sub, arguments come from the value stack
	}
}

void EncodeInsn(const Insn &i, std::vector<uint8_t> &o)
{
	W16(o, i.cls);
	if (i.cls == kClsEnd || i.cls == kClsHalt) return;
	W16(o, i.sub);
	switch (i.cls) {
	case kClsData: W32(o, (uint32_t)i.data.size()); for (uint32_t d : i.data) W32(o, d); break;
	case kClsStk:
		if (i.sub <= 1) W32(o, i.imm);
		else if (i.sub == 0x0A || i.sub == 0x19) { W16(o, i.flags); W16(o, i.kind); }
		else if (i.sub >= 0x0B && i.sub <= 0x1B && i.sub != 0x1A) W16(o, i.flags);
		break;
	case kClsFlow:
		if (i.sub == 0) { W16(o, i.flags); W16(o, i.kind); W32(o, i.imm); }
		else if (i.sub == 1) { W16(o, i.flags); W32(o, i.imm); }
		else if (i.sub == 2 || i.sub == 3) W32(o, i.imm);
		break;
	case kClsSys: if (i.sub == 0 || i.sub == 2 || i.sub == 5) W32(o, i.imm); break;
	default: break;
	}
}

// control flow of one instruction (docs/formats/fob_vm.md 3.3): successors, plus a call target
static void Successors(const uint8_t *code, size_t size, const Insn &i, uint32_t len, std::vector<uint32_t> &succ, int64_t &call)
{
	succ.clear(); call = -1;
	const uint32_t nxt = i.pc + len;
	if (i.cls == kClsEnd || i.cls == kClsHalt || (i.cls == 4 && i.sub == 1)) return;
	if (i.cls == kClsFlow) {
		switch (i.sub) {
		case 0: succ = { nxt, i.imm }; return;
		case 1: {
			uint32_t t = i.imm; if ((uint64_t)t + 8 > size) return;
			uint32_t df = R32(code + t), n = R32(code + t + 4);
			succ.push_back(df);
			for (uint32_t k = 0; k < n && (uint64_t)t + 8 + 8 * (uint64_t)k + 8 <= size; k++) succ.push_back(R32(code + t + 8 + 8 * k + 4));
			return; }
		case 2: succ = { i.imm }; return;
		case 3: succ = { nxt }; call = i.imm; return;
		case 5: case 6: case 7: return;
		default: break;
		}
	}
	succ = { nxt };
}

bool Parse(const uint8_t *b, size_t n, File &f, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	f = File();
	size_t p = 0;
	if (n < 4) return fail("short file");
	uint32_t nf = R32(b); p = 4;
	if ((uint64_t)nf * 36 + 8 > n) return fail("bad function count");
	for (uint32_t i = 0; i < nf; i++) { FuncEntry e; memcpy(e.name, b + p, 32); e.pc = R32(b + p + 32); f.funcs.push_back(e); p += 36; }
	uint32_t nt = R32(b + p); p += 4;
	for (uint32_t i = 0; i < nt; i++) {
		if (p + 4 > n) return fail("short index table");
		uint32_t c = R32(b + p); p += 4;
		if ((uint64_t)c * 4 > n - p) return fail("bad index table count");
		std::vector<int32_t> t(c); memcpy(t.data(), b + p, 4 * (size_t)c); p += 4 * (size_t)c; f.indexTables.push_back(std::move(t));
	}
	if (p + 4 > n) return fail("no code size");
	f.codeSize = R32(b + p); p += 4;
	if (f.codeSize > n - p) return fail("code block runs past the file");
	const uint8_t *code = b + p; const size_t size = f.codeSize;
	f.tail.assign(code + size, b + n);

	// recursive descent from the named functions and index tables (what the engine can reach), the same walk as tools/rbo/fobdis.py
	std::map<uint32_t, std::pair<Insn, uint32_t>> seen;
	std::vector<uint32_t> work;
	for (auto &e : f.funcs) work.push_back(e.pc);
	for (auto &t : f.indexTables) for (int32_t x : t) if (x >= 0) work.push_back((uint32_t)x);
	std::vector<uint32_t> succ; int64_t call;
	while (!work.empty()) {
		uint32_t pc = work.back(); work.pop_back();
		while (!seen.count(pc)) {
			Insn in; uint32_t len = 0;
			if (!DecodeInsn(code, size, pc, in, len)) { f.decodeErrors++; break; }
			seen[pc] = { in, len };
			Successors(code, size, in, len, succ, call);
			if (call >= 0) work.push_back((uint32_t)call);
			if (succ.size() == 1 && succ[0] == pc + len) { pc += len; continue; }
			for (uint32_t s : succ) work.push_back(s);
			break;
		}
	}
	// lay out: instructions in pc order, raw spans for everything else; an instruction overlapping an earlier one is dropped (counted)
	uint32_t cur = 0;
	auto raw = [&](uint32_t to) {
		// bytes no control flow reaches: decode them linearly as far as they decode cleanly (they are almost all instruction streams), keep the rest raw
		while (cur < to) {
			Insn in; uint32_t len = 0;
			if (!DecodeInsn(code, size, cur, in, len) || (uint64_t)cur + len > to) break;
			Item it; it.isInsn = true; it.reached = false; it.pc = cur; it.insn = std::move(in); f.items.push_back(std::move(it)); f.nUnreachedInsns++; cur += len;
		}
		if (to > cur) { Item it; it.isInsn = false; it.pc = cur; it.raw.assign(code + cur, code + to); f.rawBytes += to - cur; f.items.push_back(std::move(it)); cur = to; } };
	for (auto &kv : seen) {
		if (kv.first < cur) { f.overlapsDropped++; continue; }
		raw(kv.first);
		Item it; it.isInsn = true; it.pc = kv.first; it.insn = kv.second.first; f.items.push_back(std::move(it)); f.nInsns++;
		cur = kv.first + kv.second.second;
	}
	raw((uint32_t)size);
	return true;
}

bool Serialize(const File &f, std::vector<uint8_t> &o, std::string *err)
{
	(void)err;
	o.clear();
	W32(o, (uint32_t)f.funcs.size());
	for (auto &e : f.funcs) { o.insert(o.end(), e.name, e.name + 32); W32(o, e.pc); }
	W32(o, (uint32_t)f.indexTables.size());
	for (auto &t : f.indexTables) { W32(o, (uint32_t)t.size()); for (int32_t x : t) W32(o, (uint32_t)x); }
	W32(o, f.codeSize);
	for (auto &it : f.items) { if (it.isInsn) EncodeInsn(it.insn, o); else o.insert(o.end(), it.raw.begin(), it.raw.end()); }
	o.insert(o.end(), f.tail.begin(), f.tail.end());
	return true;
}

}} // namespace
