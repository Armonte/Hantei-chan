#include "replay_file.h"
#include <cstring>

namespace han2 { namespace rep {

static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void W32(std::vector<uint8_t> &o, uint32_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 4); }

// Rng_NextFromState (rbo_ex3.exe 0x46B0xx): two steps of x = (1021 - 354542487 x) & 0x7FFFFFFF
static uint32_t RngNext(uint32_t &s)
{
	uint32_t a = (1021u - 354542487u * s) & 0x7FFFFFFFu;
	s = (1021u - 354542487u * a) & 0x7FFFFFFFu;
	return (s & 0x7FFF0000u) | ((a >> 15) & 0xFFFFu);
}

// Sv_SumThenXorEncode: returns the sum of the PLAIN bytes/dwords; `dec` selects direction
static uint32_t Cipher(uint32_t &s, uint8_t *p, size_t n, bool dec)
{
	uint32_t sum = 0;
	for (size_t i = 0; i < n / 4; i++) {
		uint32_t v = R32(p + 4 * i), k = RngNext(s);
		if (dec) { v ^= k; sum += v; } else { sum += v; v ^= k; }
		memcpy(p + 4 * i, &v, 4);
	}
	for (size_t i = n & ~(size_t)3; i < n; i++) {
		uint8_t k = (uint8_t)RngNext(s);
		if (dec) { p[i] ^= k; sum += p[i]; } else { sum += p[i]; p[i] ^= k; }
	}
	return sum;
}

struct Layout { uint32_t H, E, inBytes; };
static bool LayoutFor(uint32_t v, Layout &l)
{
	if (v == 1 || v == 0) { l = { 4324, 48, 2 }; return true; }   // version 0: ETC.PAC demos, same size as v1; no shipped exe accepts it (loaders require 1), the checksum proves the layout
	if (v == 3) { l = { 4932, 52, 2 }; return true; }
	if (v == 10) { l = { 4932, 52, 1 }; return true; }
	return false;
}

bool Parse(const uint8_t *b, size_t n, Replay &r, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	r = Replay();
	if (n < 4) return fail("short file");
	r.version = R32(b);
	Layout l; if (!LayoutFor(r.version, l)) return fail("unknown replay version");
	const size_t inLen = (size_t)kMaxTicks * kPlayers * l.inBytes;
	const size_t total = 4 + l.H + l.E + 12 + inLen;
	if (n != total) return fail("size does not match the version's layout");
	size_t p = 4;
	r.header.assign(b + p, b + p + l.H); p += l.H;
	r.extra.assign(b + p, b + p + l.E); p += l.E;
	r.seed = R32(b + p); r.checksum = R32(b + p + 4); r.tickCount = R32(b + p + 8); p += 12;
	uint32_t s = r.seed;
	uint32_t sum = Cipher(s, r.header.data(), r.header.size(), true);
	sum += Cipher(s, r.extra.data(), r.extra.size(), true);
	r.checksumOk = ((sum ^ RngNext(s)) == r.checksum);
	r.inputs.resize((size_t)kMaxTicks * kPlayers);
	for (size_t i = 0; i < r.inputs.size(); i++) r.inputs[i] = l.inBytes == 1 ? b[p + i] : (uint16_t)(b[p + 2 * i] | (b[p + 2 * i + 1] << 8));
	if (r.header.size() == 4932) {
		const uint8_t *h = r.header.data();
		r.flags = R32(h); r.gameMode = (int32_t)R32(h + 4); r.stageId = (int32_t)R32(h + 8); r.operatorSeat = (int32_t)R32(h + 12);
		for (int i = 0; i < 3; i++) r.seatClass[i] = (int32_t)R32(h + 16 + 4 * i);
		r.rngStream0Seed = R32(h + 0x133C); r.sessionClearBracket = (int32_t)R32(h + 0x1340);
	}
	return true;
}

bool Serialize(const Replay &r, std::vector<uint8_t> &o, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	Layout l; if (!LayoutFor(r.version, l)) return fail("unknown replay version");
	if (r.header.size() != l.H || r.extra.size() != l.E || r.inputs.size() != (size_t)kMaxTicks * kPlayers) return fail("block sizes");
	o.clear(); W32(o, r.version);
	std::vector<uint8_t> h = r.header, e = r.extra;
	uint32_t s = r.seed;
	uint32_t sum = Cipher(s, h.data(), h.size(), false);
	sum += Cipher(s, e.data(), e.size(), false);
	const uint32_t checksum = sum ^ RngNext(s);   // recomputed, not copied
	o.insert(o.end(), h.begin(), h.end()); o.insert(o.end(), e.begin(), e.end());
	W32(o, r.seed); W32(o, checksum); W32(o, r.tickCount);
	for (uint16_t v : r.inputs) { o.push_back((uint8_t)v); if (l.inBytes == 2) o.push_back((uint8_t)(v >> 8)); }
	return true;
}

}} // namespace
