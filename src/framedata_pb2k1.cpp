#include "framedata_pb2k1.h"
#include "framedata_ha4.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pb2k1 {

static inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
static inline void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }

constexpr int kFrame = 96, kPatterns = 256, kHeader = 0x41C, kBankTail = 0x4000;
constexpr int kBoxSz = 4, kAtSz = 26, kIfSz = 20, kEfSz = 12;
// box slot -> model box key (same slot roles as GOF1: 0 push, 1..3 hurt, 4 probe, 5 special, 6 clash/reflect, 7 projectile hurt, 8..9 attack)
static const int kSlotKey[10] = { 0, 1, 2, 3, 9, 10, 11, 12, 25, 26 };
static const int kAniMap[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };

// Section ciphers (docs/formats/pb2k1.md section 2): buf[p] ^= (p + key[p % len]) with p restarting at 0 for every range.
static const uint8_t K1[] = { 0x4d,0x65,0x6d,0x6f,0x72,0x79,0x82,0xa6,0x82,0xe7,0x81,0x5b,0x82,0xc1,0x82,0xc4,0x82,0xb1,0x82,0xc6,0x82,0xc9,0x83,0x56,0x83,0x65,0x83,0x49,0x83,0x4e };
static const uint8_t K2[] = { 0x4d,0x65,0x83,0x93,0x82,0xc7,0x82,0xa4,0x2d,0x2d,0x82,0xc8,0x8e,0x96,0x82,0xcd,0x59,0x41,0x82,0xe8,0x82,0xbd,0x82,0xad,0x4e,0x61,0x82,0xa2,0x82,0xf1,0x82,0xbe,0x82,0xaf,0x82,0xc7,0x82,0xcb };
static const uint8_t K3[] = { 0x68,0x69,0x82,0xdc,0xc5,0x6e,0x6f,0x83,0x4a,0x82,0xc9,0x82,0xe5,0x81,0x48,0x20,0x82,0xb2,0x8b,0xea,0x98,0x4a,0x83,0x69,0x82,0xb1,0x54,0x6f,0x82,0xbe,0x82,0xc9,0x82,0xe5 };
static void Xd(uint8_t *b, size_t n, const uint8_t *key, size_t klen) { for (size_t p = 0; p < n; p++) b[p] ^= (uint8_t)(p + key[p % klen]); }

static bool Sizes(const uint8_t *hdr, size_t size, uint32_t &h5, uint32_t &h6)
{
	h5 = (uint32_t)rd32(hdr + 0x14); h6 = (uint32_t)rd32(hdr + 0x18);
	return rd32(hdr + 0x10) == 15 && h5 >= (uint32_t)kHeader && (uint64_t)h5 + h6 + kBankTail == size;
}

bool LooksLikeCharacter(const uint8_t *stored, size_t size)
{
	if (size < (size_t)kHeader + kBankTail) return false;
	uint8_t hdr[kHeader]; memcpy(hdr, stored, kHeader); Xd(hdr, kHeader, K1, sizeof K1);
	uint32_t h5, h6; return Sizes(hdr, size, h5, h6);
}

bool Decrypt(std::vector<uint8_t> &f)
{
	if (f.size() < (size_t)kHeader + kBankTail) return false;
	Xd(f.data(), kHeader, K1, sizeof K1);
	uint32_t h5, h6;
	if (!Sizes(f.data(), f.size(), h5, h6)) { Xd(f.data(), kHeader, K1, sizeof K1); return false; }
	Xd(f.data() + kHeader, h5 - kHeader, K2, sizeof K2); Xd(f.data() + h5, h6, K3, sizeof K3); Xd(f.data() + h5 + h6, kBankTail, K3, sizeof K3);
	return true;
}

void Encrypt(std::vector<uint8_t> &f)
{
	uint32_t h5 = (uint32_t)rd32(f.data() + 0x14), h6 = (uint32_t)rd32(f.data() + 0x18);
	Xd(f.data() + kHeader, h5 - kHeader, K2, sizeof K2); Xd(f.data() + h5, h6, K3, sizeof K3); Xd(f.data() + h5 + h6, kBankTail, K3, sizeof K3);
	Xd(f.data(), kHeader, K1, sizeof K1);
}

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
	L.spriteId = (uint16_t)rd16(r); L.usePat = false;      // sprite GROUP of the character's sprite bank
	L.offset_x = rd16(r + 2); L.offset_y = rd16(r + 4);
	F.AF.duration = r[6];
	ha4::DecodeFlip(std::min<int>(r[7], 7), 0, L.rotation);
	L.blend_mode = r[8]; L.rgba[3] = r[8] ? r[9] / 255.f : 1.f;
	const int ani = r[0xA];
	if (ani < 6) { F.AF.aniType = kAniMap[ani][0]; F.AF.aniFlag = kAniMap[ani][1]; } else { F.AF.aniType = 1; F.AF.aniFlag = 0; }
	F.AF.jump = r[0xB]; F.AF.landJump = r[0xC]; F.AF.priority = r[0xD];
	const int z = (uint16_t)rd16(r + 0xE); L.scale[0] = L.scale[1] = z ? z / 256.f : 1.f;
	F.AF.interpolationType = 0; F.AF.loopCount = r[0x10]; F.AF.loopEnd = r[0x11];
	const uint8_t *as = r + 0x18;
	unsigned fx, fy; int sx, sy, ax, ay;
	DecodeAxis(as[0], as[2], rd16(as + 4), rd16(as + 8), false, fx, sx, ax);
	DecodeAxis(as[1], as[3], rd16(as + 6), rd16(as + 10), true, fy, sy, ay);
	Frame_AS &S = F.AS;
	S.movementFlags = fx | fy; S.speed[0] = sx; S.speed[1] = sy; S.accel[0] = ax; S.accel[1] = ay;
	S.stanceState = as[0xC]; S.cancelNormal = as[0xD]; S.cancelSpecial = as[0xE]; S.hitsNumber = as[0xF]; S.canMove = as[0x10] != 0;
	S.maxSpeedX = rd16(as + 0x1C);
	uint32_t f1, f2; memcpy(&f1, as + 0x14, 4); memcpy(&f2, as + 0x18, 4); S.statusFlags[0] = f1; S.statusFlags[1] = f2;
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
	uint32_t h5, h6;
	if (size < (size_t)kHeader + kBankTail || !Sizes(b, size, h5, h6)) return fail("not a decrypted Party Breakers character file (check value 15, sections add up to the file size)");
	auto cont = std::make_shared<Han2Container>();
	cont->sub = 4; cont->kind = 0;
	cont->extra.assign(b, b + kHeader);
	cont->cg.assign(b + h5, b + h5 + h6);                       // the sprite bank (shown through CgForeignBank)
	cont->names.assign(b + size - kBankTail, b + size);
	fd.initEmpty(kPatterns);
	fd.m_han2 = cont;
	// pattern extents: offsets ascend, each ends where the next starts (the last at h5)
	std::vector<uint32_t> offs; for (int p = 0; p < kPatterns; p++) { uint32_t o = (uint32_t)rd32(b + 0x1C + 4 * p); if (o != 0xFFFFFFFFu) offs.push_back(o); }
	std::sort(offs.begin(), offs.end());
	for (int p = 0; p < kPatterns; p++) {
		Sequence &seq = fd.m_sequences[p];
		seq = Sequence{};
		memcpy(seq.han2.name, cont->names.data() + 64 * p, 64); seq.han2.nameValid = true;
		char nb[65]{}; memcpy(nb, seq.han2.name, 64); seq.name = sj2utf8(std::string(nb));
		const uint32_t off = (uint32_t)rd32(b + 0x1C + 4 * p);
		if (off == 0xFFFFFFFFu) continue;
		if (off < (uint32_t)kHeader || (uint64_t)off + 20 > h5) return fail("pattern offset out of range");
		auto nx = std::upper_bound(offs.begin(), offs.end(), off);
		const uint32_t endOff = nx == offs.end() ? h5 : *nx;
		const uint8_t *ph = b + off;
		const int nf = ph[0];
		if (nf < 1 || (uint64_t)off + 20 + (uint64_t)nf * kFrame > endOff) return fail("bad frame count in pattern " + std::to_string(p));
		memcpy(seq.han2.gofHdr, ph, 20); seq.han2.valid = true; seq.initialized = true; seq.empty = false;
		auto tbl = [&](int rel) -> const uint8_t * { return rel == -1 ? nullptr : ph + rel; };
		const uint8_t *boxT = tbl(rd32(ph + 4)), *atT = tbl(rd32(ph + 8)), *ifT = tbl(rd32(ph + 0xC)), *efT = tbl(rd32(ph + 0x10));
		const uint8_t *end = b + endOff;
		auto inR = [&](const uint8_t *t, int idx, int sz) { return t && idx >= 0 && t + (size_t)(idx + 1) * sz <= end; };
		seq.frames.resize(nf);
		for (int k = 0; k < nf; k++) {
			Frame &F = seq.frames[k]; Han2FrameRaw &R = F.han2;
			R.valid = true; R.frameSize = kFrame;
			memcpy(R.rec, ph + 20 + (size_t)kFrame * k, kFrame);
			const int ai = R.rec[0x3C];
			if (ai != 0xFF) { if (!inR(atT, ai, kAtSz)) return fail("AT index out of range"); memcpy(R.at, atT + kAtSz * ai, kAtSz); R.hadAT = true; }
			for (int s = 0; s < 3; s++) { int ii = rd16(R.rec + 0x3E + 2 * s); if (ii == -1) continue; if (!inR(ifT, ii, kIfSz)) return fail("IF index out of range"); memcpy(R.gofIf[s], ifT + kIfSz * ii, kIfSz); R.gofIfMask |= 1 << s; }
			for (int s = 0; s < 4; s++) { int ei = rd16(R.rec + 0x44 + 2 * s); if (ei == -1) continue; if (!inR(efT, ei, kEfSz)) return fail("EF index out of range"); memcpy(R.gofEf[s], efT + kEfSz * ei, kEfSz); R.gofEfMask |= 1 << s; }
			for (int s = 0; s < 10; s++) { int bi = rd16(R.rec + 0x4C + 2 * s); if (bi == -1) continue; if (!inR(boxT, bi, kBoxSz)) return fail("box index out of range"); for (int j = 0; j < 4; j++) R.box[s][j] = boxT[kBoxSz * bi + j]; R.boxMask |= 1 << s; }
			Decode(F, R);
		}
	}
	for (auto &s : fd.m_sequences) s.modified = false;
	return true;
}

static uint8_t u8c(int v) { return (uint8_t)std::clamp(v, 0, 255); }

static void Encode(const Frame &F, uint8_t *out, std::vector<uint8_t> &boxT, int &nBox, std::vector<uint8_t> &atT, int &nAt, std::vector<uint8_t> &ifT, int &nIf, std::vector<uint8_t> &efT, int &nEf)
{
	const Han2FrameRaw &R = F.han2; const bool raw = R.valid;
	if (raw) memcpy(out, R.rec, kFrame);
	else { memset(out, 0, kFrame); out[0xA] = 1; out[0x3C] = 0xFF; for (int i = 0; i < 3 + 4 + 10; i++) wr16(out + 0x3E + 2 * i, -1); }
	Han2FrameRaw base = R;
	if (!raw) { memcpy(base.rec, out, kFrame); base.valid = true; base.frameSize = kFrame; base.hadAT = false; base.boxMask = 0; base.gofIfMask = base.gofEfMask = 0; }
	Frame ref; Decode(ref, base);
	Layer_Type L{}; if (!F.AF.layers.empty()) L = F.AF.layers[0];
	Layer_Type Lr = ref.AF.layers[0];
	if (L.spriteId != Lr.spriteId) wr16(out, L.spriteId);
	if (L.offset_x != Lr.offset_x) wr16(out + 2, L.offset_x);
	if (L.offset_y != Lr.offset_y) wr16(out + 4, L.offset_y);
	if (F.AF.duration != ref.AF.duration) out[6] = u8c(F.AF.duration);
	if (!(L.rotation[0] == Lr.rotation[0] && L.rotation[1] == Lr.rotation[1] && L.rotation[2] == Lr.rotation[2])) { int m, rot; ha4::EncodeFlip(L.rotation, m, rot); out[7] = u8c(std::min(m, 7)); }
	if (L.blend_mode != Lr.blend_mode) out[8] = u8c(L.blend_mode);
	if ((L.rgba[3] != Lr.rgba[3] || L.blend_mode != Lr.blend_mode) && L.blend_mode) out[9] = u8c((int)lroundf(std::clamp(L.rgba[3], 0.f, 1.f) * 255.f));
	if (F.AF.aniType != ref.AF.aniType || F.AF.aniFlag != ref.AF.aniFlag) { int a = 1; for (int i = 0; i < 6; i++) if (kAniMap[i][0] == F.AF.aniType && (unsigned)kAniMap[i][1] == F.AF.aniFlag) a = i; out[0xA] = u8c(a); }
	if (F.AF.jump != ref.AF.jump) out[0xB] = u8c(F.AF.jump);
	if (F.AF.landJump != ref.AF.landJump) out[0xC] = u8c(F.AF.landJump);
	if (F.AF.priority != ref.AF.priority) out[0xD] = u8c(F.AF.priority);
	if (L.scale[0] != Lr.scale[0]) wr16(out + 0xE, L.scale[0] == 1.f ? 0 : (int)lroundf(L.scale[0] * 256.f));
	if (F.AF.loopCount != ref.AF.loopCount) out[0x10] = u8c(F.AF.loopCount);
	if (F.AF.loopEnd != ref.AF.loopEnd) out[0x11] = u8c(F.AF.loopEnd);
	uint8_t *as = out + 0x18; const Frame_AS &S = F.AS, &Sr = ref.AS;
	if (S.movementFlags != Sr.movementFlags || S.speed[0] != Sr.speed[0] || S.speed[1] != Sr.speed[1] || S.accel[0] != Sr.accel[0] || S.accel[1] != Sr.accel[1]) {
		int cx = 0, ax = 0, cy = 0, ay = 0;
		if (S.movementFlags & 0x10) { cx = 1; ax = 1; } else if (S.movementFlags & 0x20) ax = 1;
		if (S.movementFlags & 0x01) { cy = 1; ay = 1; } else if (S.movementFlags & 0x02) ay = 1;
		if (cx && ax && !S.speed[0] && !S.accel[0]) ax = 0;
		if (cy && ay && !S.speed[1] && !S.accel[1]) ay = 0;
		as[0] = u8c(cx); as[1] = u8c(cy); as[2] = u8c(ax); as[3] = u8c(ay);
		wr16(as + 4, S.speed[0]); wr16(as + 6, S.speed[1]); wr16(as + 8, S.accel[0]); wr16(as + 10, S.accel[1]);
	}
	if (S.stanceState != Sr.stanceState) as[0xC] = u8c(S.stanceState);
	if (S.cancelNormal != Sr.cancelNormal) as[0xD] = u8c(S.cancelNormal);
	if (S.cancelSpecial != Sr.cancelSpecial) as[0xE] = u8c(S.cancelSpecial);
	if (S.hitsNumber != Sr.hitsNumber) as[0xF] = u8c(S.hitsNumber);
	if (S.canMove != Sr.canMove) as[0x10] = S.canMove ? 1 : 0;
	if (S.maxSpeedX != Sr.maxSpeedX) wr16(as + 0x1C, S.maxSpeedX);
	if (S.statusFlags[0] != Sr.statusFlags[0]) memcpy(as + 0x14, &S.statusFlags[0], 4);
	if (S.statusFlags[1] != Sr.statusFlags[1]) memcpy(as + 0x18, &S.statusFlags[1], 4);
	// boxes (per-pattern numbering: appended in frame order, slot order); 4-byte u8 rectangles
	bool attackBox = false;
	for (int s = 0; s < 10; s++) {
		auto it = F.hitboxes.find(kSlotKey[s]);
		if (it == F.hitboxes.end()) { wr16(out + 0x4C + 2 * s, -1); continue; }
		uint8_t rc[4]; for (int j = 0; j < 4; j++) rc[j] = u8c(it->second.xy[j]);
		wr16(out + 0x4C + 2 * s, nBox++);
		boxT.insert(boxT.end(), rc, rc + 4);
		if (kSlotKey[s] >= 25) attackBox = true;
	}
	if (attackBox || R.hadAT) {
		uint8_t a[kAtSz]; if (R.hadAT) memcpy(a, R.at, kAtSz); else memset(a, 0, kAtSz);
		if (F.AT.damage != ref.AT.damage || !R.hadAT) wr16(a + 6, F.AT.damage);
		if (F.AT.meter_gain != ref.AT.meter_gain) wr16(a + 8, F.AT.meter_gain);
		if (F.AT.hitEffect != ref.AT.hitEffect) a[0] = u8c(F.AT.hitEffect);
		if (F.AT.hitStop != ref.AT.hitStop) a[1] = u8c(F.AT.hitStop);
		if (attackBox) { out[0x3C] = u8c(nAt++); atT.insert(atT.end(), a, a + kAtSz); } else out[0x3C] = 0xFF;
	} else out[0x3C] = 0xFF;
	for (int s = 0; s < 3; s++) wr16(out + 0x3E + 2 * s, -1);
	for (int s = 0; s < 4; s++) wr16(out + 0x44 + 2 * s, -1);
	auto slotFor = [&](unsigned mask, int total, size_t n, unsigned &used) {
		int cnt = 0; for (int s = 0; s < total; s++) if (mask >> s & 1) { if ((size_t)cnt == n) { used |= 1u << s; return s; } cnt++; }
		for (int s = 0; s < total; s++) if (!(used >> s & 1) && !(mask >> s & 1)) { used |= 1u << s; return s; }
		for (int s = 0; s < total; s++) if (!(used >> s & 1)) { used |= 1u << s; return s; }
		return -1;
	};
	unsigned usedIf = 0, usedEf = 0;
	for (size_t i = 0; i < F.IF.size() && i < 3; i++) {
		const Frame_IF &c = F.IF[i]; uint8_t r[kIfSz]{};
		int slot = slotFor(raw ? R.gofIfMask : 0u, 3, i, usedIf); bool had = (R.gofIfMask >> slot & 1) != 0 && raw;
		if (had) memcpy(r, R.gofIf[slot], kIfSz);
		bool same = had && r[0] == c.type; for (int j = 0; j < 4 && same; j++) same = rd32(r + 4 + 4 * j) == c.parameters[j];
		if (!same) { r[0] = u8c(c.type); for (int j = 0; j < 4; j++) wr32(r + 4 + 4 * j, c.parameters[j]); }
		wr16(out + 0x3E + 2 * slot, nIf++); ifT.insert(ifT.end(), r, r + kIfSz);
	}
	for (size_t i = 0; i < F.EF.size() && i < 4; i++) {
		const Frame_EF &e = F.EF[i]; uint8_t r[kEfSz]{};
		int slot = slotFor(raw ? R.gofEfMask : 0u, 4, i, usedEf); bool had = (R.gofEfMask >> slot & 1) != 0 && raw;
		if (had) memcpy(r, R.gofEf[slot], kEfSz);
		bool same = had && r[0] == e.type && r[1] == e.number; for (int j = 0; j < 5 && same; j++) same = rd16(r + 2 + 2 * j) == e.parameters[j];
		if (!same) { r[0] = u8c(e.type); r[1] = u8c(e.number); for (int j = 0; j < 5; j++) wr16(r + 2 + 2 * j, e.parameters[j]); }
		wr16(out + 0x44 + 2 * slot, nEf++); efT.insert(efT.end(), r, r + kEfSz);
	}
}

bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err, std::vector<std::string> *warn)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const Han2Container *cont = fd.m_han2.get();
	if (!cont || cont->sub != 4) return fail("not a Party Breakers character");
	out.assign(cont->extra.begin(), cont->extra.end()); out.resize(kHeader);
	for (int p = 0; p < kPatterns; p++) wr32(out.data() + 0x1C + 4 * p, -1);
	for (int p = 0; p < kPatterns; p++) {
		const Sequence *seq = p < (int)fd.m_sequences.size() ? &fd.m_sequences[p] : nullptr;
		if (!seq || seq->frames.empty()) continue;
		const int nf = (int)seq->frames.size();
		if (nf > 100) return fail("pattern " + std::to_string(p) + " has more than 100 frames");
		std::vector<uint8_t> frames((size_t)kFrame * nf), boxT, atT, ifT, efT; int nBox = 0, nAt = 0, nIf = 0, nEf = 0;
		for (int k = 0; k < nf; k++) Encode(seq->frames[k], &frames[(size_t)kFrame * k], boxT, nBox, atT, nAt, ifT, nIf, efT, nEf);
		uint8_t hdr[20]; if (seq->han2.valid) memcpy(hdr, seq->han2.gofHdr, 20); else memset(hdr, 0, 20);
		hdr[0] = (uint8_t)nf;
		int pos = 20 + kFrame * nf;
		auto place = [&](size_t n) { int r = n ? pos : -1; pos += (int)n; return r; };
		wr32(hdr + 4, place(boxT.size())); wr32(hdr + 8, place(atT.size())); wr32(hdr + 0xC, place(ifT.size())); wr32(hdr + 0x10, place(efT.size()));
		wr32(out.data() + 0x1C + 4 * p, (int32_t)out.size());
		out.insert(out.end(), hdr, hdr + 20); out.insert(out.end(), frames.begin(), frames.end());
		out.insert(out.end(), boxT.begin(), boxT.end()); out.insert(out.end(), atT.begin(), atT.end());
		out.insert(out.end(), ifT.begin(), ifT.end()); out.insert(out.end(), efT.begin(), efT.end());
	}
	const uint32_t h5 = (uint32_t)out.size();
	out.insert(out.end(), cont->cg.begin(), cont->cg.end());
	std::vector<uint8_t> names = cont->names; if (names.size() != kBankTail) names.assign(kBankTail, 0);
	for (int p = 0; p < kPatterns && p < (int)fd.m_sequences.size(); p++) {
		const Sequence &sq = fd.m_sequences[p]; uint8_t *dst = &names[64 * p];
		if (sq.han2.nameValid) { char nb[65]{}; memcpy(nb, sq.han2.name, 64); if (sj2utf8(std::string(nb)) == sq.name) { memcpy(dst, sq.han2.name, 64); continue; } }
		std::string sj = utf82sj(sq.name); size_t n = std::min<size_t>(sj.size(), 63), i = 0;
		while (i < n) { unsigned char c = (unsigned char)sj[i]; size_t w = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1; if (i + w > n) break; i += w; }
		memset(dst, 0, 64); memcpy(dst, sj.data(), i);
	}
	out.insert(out.end(), names.begin(), names.end());
	wr32(out.data() + 0x14, (int32_t)h5); wr32(out.data() + 0x18, (int32_t)cont->cg.size());
	(void)warn;
	return true;
}

static std::string g_lastErr;
const std::string &LastSaveError() { return g_lastErr; }

static bool ArchiveTarget(const std::string &f)
{
	std::string n = f.substr(f.find_last_of("/\\") == std::string::npos ? 0 : f.find_last_of("/\\") + 1);
	for (auto &c : n) c = (char)tolower((unsigned char)c);
	if (n.size() > 2 && n.compare(n.size() - 2, 2, ".p") == 0) return true;
	if (n.size() < 5 || n.compare(n.size() - 4, 4, ".dat") != 0) return false;
	std::string stem = n.substr(0, n.size() - 4);
	if (!stem.empty() && stem.back() == 'p') stem.pop_back();
	return !stem.empty() && std::all_of(stem.begin(), stem.end(), [](char c) { return c >= '0' && c <= '9'; });   // 01.dat / 01p.dat / 02p.dat: the game's archives
}

bool SaveFile(const FrameData &fd, const char *filename, std::string *err)
{
	g_lastErr.clear();
	auto done = [&](bool ok) { if (err) *err = g_lastErr; return ok; };
	std::vector<uint8_t> plain;
	if (!Serialize(fd, plain, &g_lastErr)) return done(false);
	Encrypt(plain);
	const std::string f = filename ? filename : "";
	if (ArchiveTarget(f)) {
		int r = g_saveEntryIntoArchive && fd.m_han2 ? g_saveEntryIntoArchive(fd.m_han2->sourcePath, plain, f, &g_lastErr) : 0;
		if (r == 0) g_lastErr = "this character was not opened from an archive entry: nothing to replace";
		return done(r == 1);
	}
	if (!WriteFileAtomic(filename, plain.data(), plain.size())) { g_lastErr = std::string("could not write ") + filename; return done(false); }
	return done(true);
}

} // namespace pb2k1
