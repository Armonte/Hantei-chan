#include "framedata_gof1.h"
#include "framedata_ha4.h"
#include "han2/gof1_archive.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gof1 {

static inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
static inline void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }

constexpr int kFrame = 116, kPatterns = 256;
// box slot -> model box key (0 push/collision, 1..3 hurt, 4 -> special 1 (probe), 5 special 2, 6 clash/reflect, 7 projectile, 8..9 attack)
static const int kSlotKey[10] = { 0, 1, 2, 3, 9, 10, 11, 12, 25, 26 };

static const int kAniMap[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };

static void DecodeAxis(int clear, int add, int speed, int accel, bool yAxis, unsigned &flags, int &spd, int &acc)
{
	unsigned setBit = yAxis ? 0x01 : 0x10, addBit = yAxis ? 0x02 : 0x20;
	flags = 0; spd = 0; acc = 0;
	if (add) { flags = clear ? setBit : addBit; spd = speed; acc = accel; }
	else if (clear) flags = setBit;
}

static void Decode(Frame &F, const Han2FrameRaw &R)
{
	const uint8_t *r = R.rec;
	F.AF.layers.clear(); F.AF.layers.push_back({});
	Layer_Type &L = F.AF.layers[0];
	int spr = rd16(r);
	if (spr >= 10000) { L.spriteId = spr - 10000; L.usePat = false; } else if (spr >= 0) { L.spriteId = spr; L.usePat = true; } else { L.spriteId = spr; L.usePat = false; }
	L.offset_x = rd16(r + 2); L.offset_y = rd16(r + 4);
	F.AF.duration = (uint16_t)rd16(r + 6);
	ha4::DecodeFlip(r[8], 0, L.rotation);
	L.blend_mode = r[9]; L.rgba[3] = r[9] ? r[0xA] / 255.f : 1.f;
	int ani = r[0xB];
	if (ani < 6) { F.AF.aniType = kAniMap[ani][0]; F.AF.aniFlag = kAniMap[ani][1]; } else { F.AF.aniType = 1; F.AF.aniFlag = 0; }
	F.AF.jump = r[0xC]; F.AF.landJump = r[0xD]; F.AF.priority = r[0xE];
	int z = (uint16_t)rd16(r + 0x10); L.scale[0] = L.scale[1] = z ? z / 256.f : 1.f;
	F.AF.interpolationType = r[0x18]; F.AF.loopCount = r[0x12]; F.AF.loopEnd = r[0x13];
	const uint8_t *as = r + 0x28;
	unsigned fx, fy; int sx, sy, ax, ay;
	DecodeAxis(as[0], as[2], rd16(as + 8), rd16(as + 0xC), false, fx, sx, ax);
	DecodeAxis(as[1], as[3], rd16(as + 0xA), rd16(as + 0xE), true, fy, sy, ay);
	Frame_AS &S = F.AS;
	S.movementFlags = fx | fy; S.speed[0] = sx; S.speed[1] = sy; S.accel[0] = ax; S.accel[1] = ay;
	S.stanceState = as[0x10]; S.cancelNormal = as[0x11]; S.cancelSpecial = as[0x12]; S.hitsNumber = as[0x13]; S.canMove = as[0x14] != 0;
	S.maxSpeedX = rd16(as + 0x20);
	uint32_t f1, f2; memcpy(&f1, as + 0x18, 4); memcpy(&f2, as + 0x1C, 4); S.statusFlags[0] = f1; S.statusFlags[1] = f2;
	if (R.hadAT) { F.AT.damage = rd16(R.at + 6); F.AT.meter_gain = rd16(R.at + 8); F.AT.hitEffect = R.at[0]; F.AT.hitStop = R.at[1]; }
	F.IF.clear(); F.EF.clear();
	for (int k = 0; k < 3; k++) if (R.gofIfMask >> k & 1) { Frame_IF c{}; c.type = R.gofIf[k][0]; for (int i = 0; i < 4; i++) c.parameters[i] = rd32(R.gofIf[k] + 4 + 4 * i); F.IF.push_back(c); }
	for (int k = 0; k < 4; k++) if (R.gofEfMask >> k & 1) { Frame_EF e{}; e.type = R.gofEf[k][0]; e.number = R.gofEf[k][1]; for (int i = 0; i < 5; i++) e.parameters[i] = rd16(R.gofEf[k] + 2 + 2 * i); F.EF.push_back(e); }
	F.hitboxes.clear();
	for (int s = 0; s < 10; s++) if (R.boxMask >> s & 1) { Hitbox hb{}; for (int j = 0; j < 4; j++) hb.xy[j] = R.box[s][j]; F.hitboxes[kSlotKey[s]] = hb; }
}

void RedecodeFrame(Frame &F) { if (F.han2.valid) Decode(F, F.han2); }

bool Load(FrameData &fd, const uint8_t *b, size_t size, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (size < 0x444 + 0x4000 || rd32(b + 0x10) != 18) return fail("not a decrypted GOF1 character file (version 18)");
	uint32_t patEnd = (uint32_t)rd32(b + 0x14), partsSize = (uint32_t)rd32(b + 0x18), cgOff = (uint32_t)rd32(b + 0x1C), cgSize = (uint32_t)rd32(b + 0x20);
	if (patEnd < 0x444 || (uint64_t)patEnd + partsSize > size || (uint64_t)cgOff + cgSize + 0x4000 != size) return fail("section offsets out of range");
	auto cont = std::make_shared<Han2Container>();
	cont->sub = 3; cont->kind = 0;
	cont->extra.assign(b, b + 0x44);
	cont->parts.assign(b + patEnd, b + patEnd + partsSize);
	cont->cg.assign(b + cgOff, b + cgOff + cgSize);
	cont->names.assign(b + size - 0x4000, b + size);
	fd.initEmpty(kPatterns);
	fd.m_han2 = cont;
	for (int p = 0; p < kPatterns; p++) {
		Sequence &seq = fd.m_sequences[p];
		seq = Sequence{};
		memcpy(seq.han2.name, cont->names.data() + 64 * p, 64); seq.han2.nameValid = true;
		char nb[65]{}; memcpy(nb, seq.han2.name, 64); seq.name = sj2utf8(std::string(nb));
		int32_t off = rd32(b + 0x44 + 4 * p);
		if (off == -1) continue;
		if (off < 0x444 || (uint64_t)off + 20 > patEnd) return fail("pattern offset out of range");
		const uint8_t *ph = b + off;
		int nf = ph[0];
		if (nf < 1 || (uint64_t)off + 20 + (uint64_t)nf * kFrame > patEnd) return fail("bad frame count");
		memcpy(seq.han2.gofHdr, ph, 20); seq.han2.valid = true; seq.initialized = true; seq.empty = false;
		auto tbl = [&](int rel) -> const uint8_t * { return rel == -1 ? nullptr : ph + rel; };
		const uint8_t *boxT = tbl(rd32(ph + 4)), *atT = tbl(rd32(ph + 8)), *ifT = tbl(rd32(ph + 0xC)), *efT = tbl(rd32(ph + 0x10));
		const uint8_t *end = b + patEnd;
		auto inR = [&](const uint8_t *t, int idx, int sz) { return t && idx >= 0 && t + (size_t)(idx + 1) * sz <= end; };
		seq.frames.resize(nf);
		for (int k = 0; k < nf; k++) {
			Frame &F = seq.frames[k]; Han2FrameRaw &R = F.han2;
			R.valid = true; R.frameSize = kFrame;
			memcpy(R.rec, ph + 20 + (size_t)kFrame * k, kFrame);
			int ai = (signed char)R.rec[0x50];
			if (ai >= 0) { if (!inR(atT, ai, 26)) return fail("AT index out of range"); memcpy(R.at, atT + 26 * ai, 26); R.hadAT = true; }
			for (int s = 0; s < 3; s++) { int ii = rd16(R.rec + 0x52 + 2 * s); if (ii == -1) continue; if (!inR(ifT, ii, 28)) return fail("IF index out of range"); memcpy(R.gofIf[s], ifT + 28 * ii, 28); R.gofIfMask |= 1 << s; }
			for (int s = 0; s < 4; s++) { int ei = rd16(R.rec + 0x58 + 2 * s); if (ei == -1) continue; if (!inR(efT, ei, 20)) return fail("EF index out of range"); memcpy(R.gofEf[s], efT + 20 * ei, 20); R.gofEfMask |= 1 << s; }
			for (int s = 0; s < 10; s++) { int bi = rd16(R.rec + 0x60 + 2 * s); if (bi == -1) continue; if (!inR(boxT, bi, 8)) return fail("box index out of range"); for (int j = 0; j < 4; j++) R.box[s][j] = rd16(boxT + 8 * bi + 2 * j); R.boxMask |= 1 << s; }
			Decode(F, R);
		}
	}
	for (auto &s : fd.m_sequences) s.modified = false;
	return true;
}

static uint8_t u8c(int v) { return (uint8_t)v; }

static void Encode(const Frame &F, uint8_t *out, std::vector<uint8_t> &boxT, int &nBox, std::vector<uint8_t> &atT, int &nAt, std::vector<uint8_t> &ifT, int &nIf, std::vector<uint8_t> &efT, int &nEf)
{
	const Han2FrameRaw &R = F.han2; const bool raw = R.valid;
	if (raw) memcpy(out, R.rec, kFrame);
	else { memset(out, 0, kFrame); out[0x0B] = 1; out[0x50] = 0xFF; for (int i = 0; i < 3 + 4 + 10; i++) wr16(out + 0x52 + 2 * i, -1); }
	Han2FrameRaw base = R;
	if (!raw) { memcpy(base.rec, out, kFrame); base.valid = true; base.frameSize = kFrame; base.hadAT = false; base.boxMask = 0; base.gofIfMask = base.gofEfMask = 0; }
	Frame ref; Decode(ref, base);
	Layer_Type L{}; if (!F.AF.layers.empty()) L = F.AF.layers[0];
	Layer_Type Lr = ref.AF.layers[0];
	int spr = L.usePat ? L.spriteId : (L.spriteId < 0 ? L.spriteId : L.spriteId + 10000);
	int sprR = Lr.usePat ? Lr.spriteId : (Lr.spriteId < 0 ? Lr.spriteId : Lr.spriteId + 10000);
	if (spr != sprR) wr16(out, spr);
	if (L.offset_x != Lr.offset_x) wr16(out + 2, L.offset_x);
	if (L.offset_y != Lr.offset_y) wr16(out + 4, L.offset_y);
	if (F.AF.duration != ref.AF.duration) wr16(out + 6, F.AF.duration);
	if (!(L.rotation[0] == Lr.rotation[0] && L.rotation[1] == Lr.rotation[1] && L.rotation[2] == Lr.rotation[2])) { int m, rot; ha4::EncodeFlip(L.rotation, m, rot); out[8] = u8c(std::min(m, 9)); }
	if (L.blend_mode != Lr.blend_mode) out[9] = u8c(L.blend_mode);
	if ((L.rgba[3] != Lr.rgba[3] || L.blend_mode != Lr.blend_mode) && L.blend_mode) out[0xA] = u8c((int)lroundf(std::clamp(L.rgba[3], 0.f, 1.f) * 255.f));
	if (F.AF.aniType != ref.AF.aniType || F.AF.aniFlag != ref.AF.aniFlag) { int a = 1; for (int i = 0; i < 6; i++) if (kAniMap[i][0] == F.AF.aniType && (unsigned)kAniMap[i][1] == F.AF.aniFlag) a = i; out[0xB] = u8c(a); }
	if (F.AF.jump != ref.AF.jump) out[0xC] = u8c(F.AF.jump);
	if (F.AF.landJump != ref.AF.landJump) out[0xD] = u8c(F.AF.landJump);
	if (F.AF.priority != ref.AF.priority) out[0xE] = u8c(F.AF.priority);
	if (L.scale[0] != Lr.scale[0]) wr16(out + 0x10, L.scale[0] == 1.f ? 0 : (int)lroundf(L.scale[0] * 256.f));
	if (F.AF.interpolationType != ref.AF.interpolationType) out[0x18] = u8c(F.AF.interpolationType);
	if (F.AF.loopCount != ref.AF.loopCount) out[0x12] = u8c(F.AF.loopCount);
	if (F.AF.loopEnd != ref.AF.loopEnd) out[0x13] = u8c(F.AF.loopEnd);
	uint8_t *as = out + 0x28; const Frame_AS &S = F.AS, &Sr = ref.AS;
	if (S.movementFlags != Sr.movementFlags || S.speed[0] != Sr.speed[0] || S.speed[1] != Sr.speed[1] || S.accel[0] != Sr.accel[0] || S.accel[1] != Sr.accel[1]) {
		int cx = 0, ax = 0, cy = 0, ay = 0;
		if (S.movementFlags & 0x10) { cx = 1; ax = 1; } else if (S.movementFlags & 0x20) ax = 1;
		if (S.movementFlags & 0x01) { cy = 1; ay = 1; } else if (S.movementFlags & 0x02) ay = 1;
		if (cx && ax && !S.speed[0] && !S.accel[0]) ax = 0;
		if (cy && ay && !S.speed[1] && !S.accel[1]) ay = 0;
		as[0] = u8c(cx); as[1] = u8c(cy); as[2] = u8c(ax); as[3] = u8c(ay);
		wr16(as + 8, S.speed[0]); wr16(as + 0xA, S.speed[1]); wr16(as + 0xC, S.accel[0]); wr16(as + 0xE, S.accel[1]);
	}
	if (S.stanceState != Sr.stanceState) as[0x10] = u8c(S.stanceState);
	if (S.cancelNormal != Sr.cancelNormal) as[0x11] = u8c(S.cancelNormal);
	if (S.cancelSpecial != Sr.cancelSpecial) as[0x12] = u8c(S.cancelSpecial);
	if (S.hitsNumber != Sr.hitsNumber) as[0x13] = u8c(S.hitsNumber);
	if (S.canMove != Sr.canMove) as[0x14] = S.canMove ? 1 : 0;
	if (S.maxSpeedX != Sr.maxSpeedX) wr16(as + 0x20, S.maxSpeedX);
	if (S.statusFlags[0] != Sr.statusFlags[0]) memcpy(as + 0x18, &S.statusFlags[0], 4);
	if (S.statusFlags[1] != Sr.statusFlags[1]) memcpy(as + 0x1C, &S.statusFlags[1], 4);
	// boxes (per-pattern numbering: appended in frame order, slot order)
	bool attackBox = false;
	for (int s = 0; s < 10; s++) {
		auto it = F.hitboxes.find(kSlotKey[s]);
		if (it == F.hitboxes.end()) { wr16(out + 0x60 + 2 * s, -1); continue; }
		int16_t rc[4]; for (int j = 0; j < 4; j++) rc[j] = (int16_t)it->second.xy[j];
		wr16(out + 0x60 + 2 * s, nBox++);
		boxT.insert(boxT.end(), (uint8_t *)rc, (uint8_t *)rc + 8);
		if (kSlotKey[s] >= 25) attackBox = true;
	}
	if (attackBox || R.hadAT) {
		uint8_t a[26]; if (R.hadAT) memcpy(a, R.at, 26); else memset(a, 0, 26);
		if (F.AT.damage != ref.AT.damage || !R.hadAT) wr16(a + 6, F.AT.damage);
		if (F.AT.meter_gain != ref.AT.meter_gain) wr16(a + 8, F.AT.meter_gain);
		if (F.AT.hitEffect != ref.AT.hitEffect) a[0] = u8c(F.AT.hitEffect);
		if (F.AT.hitStop != ref.AT.hitStop) a[1] = u8c(F.AT.hitStop);
		if (attackBox) { out[0x50] = u8c(nAt++); atT.insert(atT.end(), a, a + 26); } else out[0x50] = 0xFF;
	} else out[0x50] = 0xFF;
	// IF / EF: model entries in slot order; unchanged records keep their bytes
	for (int s = 0; s < 3; s++) wr16(out + 0x52 + 2 * s, -1);
	for (int s = 0; s < 4; s++) wr16(out + 0x58 + 2 * s, -1);
	// model entries are compacted; give the n-th entry the n-th slot that was in use (sparse slots survive), extra entries take free slots
	auto slotFor = [&](unsigned mask, int total, size_t n, unsigned &used) {
		int cnt = 0; for (int s = 0; s < total; s++) if (mask >> s & 1) { if ((size_t)cnt == n) { used |= 1u << s; return s; } cnt++; }
		for (int s = 0; s < total; s++) if (!(used >> s & 1) && !(mask >> s & 1)) { used |= 1u << s; return s; }
		for (int s = 0; s < total; s++) if (!(used >> s & 1)) { used |= 1u << s; return s; }
		return -1;
	};
	unsigned usedIf = 0, usedEf = 0;
	for (size_t i = 0; i < F.IF.size() && i < 3; i++) {
		const Frame_IF &c = F.IF[i]; uint8_t r[28]{};
		int slot = slotFor(raw ? R.gofIfMask : 0u, 3, i, usedIf); bool had = (R.gofIfMask >> slot & 1) != 0 && raw;
		if (had) memcpy(r, R.gofIf[slot], 28);
		bool same = had && r[0] == c.type; for (int j = 0; j < 4 && same; j++) same = rd32(r + 4 + 4 * j) == c.parameters[j];
		if (!same) { r[0] = u8c(c.type); for (int j = 0; j < 4; j++) wr32(r + 4 + 4 * j, c.parameters[j]); }
		wr16(out + 0x52 + 2 * slot, nIf++); ifT.insert(ifT.end(), r, r + 28);
	}
	for (size_t i = 0; i < F.EF.size() && i < 4; i++) {
		const Frame_EF &e = F.EF[i]; uint8_t r[20]{};
		int slot = slotFor(raw ? R.gofEfMask : 0u, 4, i, usedEf); bool had = (R.gofEfMask >> slot & 1) != 0 && raw;
		if (had) memcpy(r, R.gofEf[slot], 20);
		bool same = had && r[0] == e.type && r[1] == e.number; for (int j = 0; j < 5 && same; j++) same = rd16(r + 2 + 2 * j) == e.parameters[j];
		if (!same) { r[0] = u8c(e.type); r[1] = u8c(e.number); for (int j = 0; j < 5; j++) wr16(r + 2 + 2 * j, e.parameters[j]); }
		wr16(out + 0x58 + 2 * slot, nEf++); efT.insert(efT.end(), r, r + 20);
	}
}

bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err, std::vector<std::string> *warn)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const Han2Container *cont = fd.m_han2.get();
	if (!cont || cont->sub != 3) return fail("not a GOF1 character");
	std::vector<uint8_t> pats; std::vector<int32_t> offs(kPatterns, -1);
	const uint32_t base = 0x444;
	for (int p = 0; p < kPatterns; p++) {
		const Sequence *seq = p < (int)fd.m_sequences.size() ? &fd.m_sequences[p] : nullptr;
		if (!seq || seq->frames.empty()) continue;
		int nf = (int)seq->frames.size();
		if (nf > 100) return fail("pattern " + std::to_string(p) + " has more than 100 frames");
		std::vector<uint8_t> frames((size_t)kFrame * nf), boxT, atT, ifT, efT; int nBox = 0, nAt = 0, nIf = 0, nEf = 0;
		for (int k = 0; k < nf; k++) Encode(seq->frames[k], &frames[(size_t)kFrame * k], boxT, nBox, atT, nAt, ifT, nIf, efT, nEf);
		uint8_t hdr[20]; if (seq->han2.valid) memcpy(hdr, seq->han2.gofHdr, 20); else memset(hdr, 0, 20);
		hdr[0] = (uint8_t)nf;
		int pos = 20 + kFrame * nf;
		auto place = [&](size_t n, int sz) { int r = n ? pos : -1; pos += (int)n; (void)sz; return r; };
		wr32(hdr + 4, place(boxT.size(), 8)); wr32(hdr + 8, place(atT.size(), 26)); wr32(hdr + 0xC, place(ifT.size(), 28)); wr32(hdr + 0x10, place(efT.size(), 20));
		offs[p] = (int32_t)(base + pats.size());
		pats.insert(pats.end(), hdr, hdr + 20); pats.insert(pats.end(), frames.begin(), frames.end());
		pats.insert(pats.end(), boxT.begin(), boxT.end()); pats.insert(pats.end(), atT.begin(), atT.end());
		pats.insert(pats.end(), ifT.begin(), ifT.end()); pats.insert(pats.end(), efT.begin(), efT.end());
	}
	out.assign(0x444, 0);
	memcpy(out.data(), cont->extra.data(), 0x44);
	for (int p = 0; p < kPatterns; p++) wr32(out.data() + 0x44 + 4 * p, offs[p]);
	out.insert(out.end(), pats.begin(), pats.end());
	uint32_t patEnd = (uint32_t)out.size();
	out.insert(out.end(), cont->parts.begin(), cont->parts.end());
	uint32_t cgOff = (uint32_t)out.size();
	out.insert(out.end(), cont->cg.begin(), cont->cg.end());
	std::vector<uint8_t> names = cont->names; if (names.size() != 0x4000) names.assign(0x4000, 0);
	for (int p = 0; p < kPatterns && p < (int)fd.m_sequences.size(); p++) {
		const Sequence &sq = fd.m_sequences[p]; uint8_t *dst = &names[64 * p];
		if (sq.han2.nameValid) { char nb[65]{}; memcpy(nb, sq.han2.name, 64); if (sj2utf8(std::string(nb)) == sq.name) { memcpy(dst, sq.han2.name, 64); continue; } }
		std::string sj = utf82sj(sq.name); size_t n = std::min<size_t>(sj.size(), 63), i = 0;
		while (i < n) { unsigned char c = (unsigned char)sj[i]; size_t w = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1; if (i + w > n) break; i += w; }
		memset(dst, 0, 64); memcpy(dst, sj.data(), i);
	}
	out.insert(out.end(), names.begin(), names.end());
	wr32(out.data() + 0x14, (int32_t)patEnd); wr32(out.data() + 0x18, (int32_t)cont->parts.size()); wr32(out.data() + 0x1C, (int32_t)cgOff); wr32(out.data() + 0x20, (int32_t)cont->cg.size());
	(void)warn;
	return true;
}

static std::string g_lastErr;
const std::string &LastSaveError() { return g_lastErr; }

bool SaveFile(const FrameData &fd, const char *filename, std::string *err)
{
	g_lastErr.clear();
	auto done = [&](bool ok) { if (err) *err = g_lastErr; return ok; };
	std::vector<uint8_t> plain;
	if (!Serialize(fd, plain, &g_lastErr)) return done(false);
	std::string f = filename ? filename : "";
	std::string ext = f.size() >= 2 ? f.substr(f.size() - 2) : "";
	for (auto &c : ext) c = (char)tolower((unsigned char)c);
	if (ext == ".p") {
		const Han2Container &c = *fd.m_han2;
		Archive a;
		if (c.sourcePath.empty() || !Open(c.sourcePath, a, &g_lastErr)) { if (g_lastErr.empty()) g_lastErr = "no source archive to copy"; return done(false); }
		int idx = Find(a, c.gof1Name);
		if (idx < 0) { g_lastErr = "entry " + c.gof1Name + " not found in the source archive"; return done(false); }
		return done(WriteArchiveReplacing(a, idx, plain, f, &g_lastErr));
	}
	EncryptDat(plain);
	if (!WriteFileAtomic(filename, plain.data(), plain.size())) { g_lastErr = std::string("could not write ") + filename; return done(false); }
	return done(true);
}

} // namespace gof1
