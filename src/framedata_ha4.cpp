// MBAC Hantei4 (.DAT) <-> Hantei-chan in-memory model.
// Byte layout and HA4->HA6 field rules: docs/bg_research/HA4_SECTION.md (V-ed/V-rt/V-data).
// Design and the field mapping table: docs/HANTEI_MBAC_SUPPORT.md.
//
// Save strategy: every HA4 field is re-encoded from the model, but when the
// frame/pattern carries its original bytes (Ha4FrameRaw / Ha4SeqRaw) and the
// model value equals what those bytes decode to, the original bytes are kept.
// Unedited data therefore round-trips byte-identically (including heap/stack
// garbage the original writer left in unused slots), and edited fields are
// encoded canonically.

#include "framedata_ha4.h"
#include "framedata.h"
#include "misc.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdio>

namespace ha4 {

// ---------------------------------------------------------------------------
// little helpers
// ---------------------------------------------------------------------------
static inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t rdu32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
static inline void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline uint8_t u8c(int v) { return (uint8_t)v; }

bool IsHA4(const void *data, size_t size)
{
	return size >= 8 && memcmp(data, "Hantei4\0", 8) == 0;
}

bool IsHA4File(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return false;
	char sig[8]{};
	size_t n = fread(sig, 1, 8, f);
	fclose(f);
	return IsHA4(sig, n);
}

bool EfHasPositionBias(int type)
{
	// V-data + mbacPC EF03_SpawnPresetEffect -> Actor_FrameOffsetToWorld
	return type == 1 || type == 2 || type == 3 || type == 4 || type == 8 || type == 11;
}

void BoxToModel(const int16_t in[4], bool partsFrame, int out[4])
{
	if (partsFrame) {
		// Actor_BoxToWorldRect: x' = (x>>1) - 32, y' = y>>1, then the CG bias (128,224).
		out[0] = (in[0] >> 1) - 160; out[1] = (in[1] >> 1) - 224;
		out[2] = (in[2] >> 1) - 160; out[3] = (in[3] >> 1) - 224;
	} else {
		out[0] = in[0] - 128; out[1] = in[1] - 224;
		out[2] = in[2] - 128; out[3] = in[3] - 224;
	}
}

void BoxFromModel(const int in[4], bool partsFrame, int16_t out[4])
{
	if (partsFrame) {
		out[0] = (int16_t)((in[0] + 160) * 2); out[1] = (int16_t)((in[1] + 224) * 2);
		out[2] = (int16_t)((in[2] + 160) * 2); out[3] = (int16_t)((in[3] + 224) * 2);
	} else {
		out[0] = (int16_t)(in[0] + 128); out[1] = (int16_t)(in[1] + 224);
		out[2] = (int16_t)(in[2] + 128); out[3] = (int16_t)(in[3] + 224);
	}
}

// ---------------------------------------------------------------------------
// field decoders (raw -> model). The writer uses the same functions to decide
// whether the original bytes still represent the model value.
// ---------------------------------------------------------------------------

// AF +8 flip/rotate mode (ctl 1080: normal, flipH, flipV, 90, 180, 270, H+90,
// H+270, arbitrary, arbitrary-R) and +0x24 arbitrary angle (1/10000 turn).
// HA6: 1 -> AFAY 0.5, 3/4 -> AFAZ 0.25/0.5, 8 -> AFAZ rot/10000 (V-data);
// 2 -> AFAX 0.5, 5 -> AFAZ 0.75, 6/7 -> AFAY 0.5 + AFAZ, 9 -> AFAY 0.5 + AFAZ rot (I).
void DecodeFlip(int mode, int rot, float r[3])
{
	r[0] = r[1] = r[2] = 0.f;
	switch (mode) {
	case 1: r[1] = 0.5f; break;
	case 2: r[0] = 0.5f; break;
	case 3: r[2] = 0.25f; break;
	case 4: r[2] = 0.5f; break;
	case 5: r[2] = 0.75f; break;
	case 6: r[1] = 0.5f; r[2] = 0.25f; break;
	case 7: r[1] = 0.5f; r[2] = 0.75f; break;
	case 8: r[2] = rot / 10000.f; break;
	case 9: r[1] = 0.5f; r[2] = rot / 10000.f; break;
	default: break;
	}
}

void EncodeFlip(const float r[3], int &mode, int &rot)
{
	rot = 0;
	bool fy = r[1] != 0.f, fx = r[0] != 0.f;
	float z = r[2];
	if (fx && !fy && z == 0.f) { mode = 2; return; }
	if (!fy) {
		if (z == 0.f) mode = 0;
		else if (z == 0.25f) mode = 3;
		else if (z == 0.5f) mode = 4;
		else if (z == 0.75f) mode = 5;
		else { mode = 8; rot = (int)lroundf(z * 10000.f); }
	} else {
		if (z == 0.f) mode = 1;
		else if (z == 0.25f) mode = 6;
		else if (z == 0.75f) mode = 7;
		else { mode = 9; rot = (int)lroundf(z * 10000.f); }
	}
}

// AF +0xB AniFlag -> (HA6 AFF aniType, AFFE aniFlag). calibrated in compare_states.py.
static const int kAniMap[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };

static int EncodeAni(int type, unsigned flag)
{
	for (int i = 0; i < 6; i++)
		if (kAniMap[i][0] == type && (unsigned)kAniMap[i][1] == flag) return i;
	if (type == 1) return (flag & 1) ? 3 : 1;
	if (type == 2) return (flag & 2) ? 5 : ((flag & 1) ? 4 : 2);
	return 0;
}

static float DecodeZoom(int v) { return v == 0 ? 1.f : v / 256.f; }

// AS movement bytes (+0 clearX, +1 clearY, +2 addX, +3 addY; mbacPC 0x4335F0).
// clear+add = HA6 Set (0x10 X / 0x01 Y), add = Add (0x20/0x02), clear alone zeroes
// the axis (= Set with 0 speed/accel). Speeds on axes with no flag are dead values.
struct AxisMove { unsigned flags; int speed, accel; };
static AxisMove DecodeAxis(int clear, int add, int speed, int accel, bool yAxis)
{
	AxisMove m{0, 0, 0};
	unsigned setBit = yAxis ? 0x01 : 0x10, addBit = yAxis ? 0x02 : 0x20;
	if (add) { m.flags = clear ? setBit : addBit; m.speed = speed; m.accel = accel; }
	else if (clear) { m.flags = setBit; }
	return m;
}

// AT +0x3B proration: 0 in MBAC = no proration (HA6 ATHS absent = 100). V-data.
static int DecodeCorrection(int v) { return v == 0 ? 100 : v; }

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------
static void DecodeFrame(Frame &F, const Ha4FrameRaw &R)
{
	const uint8_t *af = R.rec;
	const uint8_t *as = R.rec + 44;

	// --- AF ---
	F.AF.layers.clear();
	F.AF.layers.push_back({});
	Layer_Type &L = F.AF.layers[0];
	int spr = rd16(af + 0);
	if (spr >= 10000) { L.spriteId = spr - 10000; L.usePat = false; }
	else if (spr >= 0) { L.spriteId = spr; L.usePat = true; }
	else { L.spriteId = spr; L.usePat = false; }
	L.offset_x = rd16(af + 2);
	L.offset_y = rd16(af + 4);
	F.AF.duration = rd16(af + 6);
	DecodeFlip(af[8], rd16(af + 0x24), L.rotation);
	L.blend_mode = af[9];
	L.rgba[3] = af[9] ? af[0xA] / 255.f : 1.f;
	int ani = af[0xB];
	if (ani < 6) { F.AF.aniType = kAniMap[ani][0]; F.AF.aniFlag = kAniMap[ani][1]; }
	else { F.AF.aniType = 0; F.AF.aniFlag = 0; }
	F.AF.jump = af[0xC];
	F.AF.landJump = af[0xD];
	F.AF.priority = af[0xE];
	L.scale[0] = DecodeZoom(rd16(af + 0x10));
	L.scale[1] = DecodeZoom(rd16(af + 0x12));
	F.AF.interpolationType = af[0x14];
	F.AF.loopEnd = af[0x15];
	F.AF.loopCount = af[0x16];

	// --- AS ---
	Frame_AS &S = F.AS;
	AxisMove mx = DecodeAxis(as[0], as[2], rd16(as + 8), rd16(as + 0x10), false);
	AxisMove my = DecodeAxis(as[1], as[3], rd16(as + 0xA), rd16(as + 0x12), true);
	S.movementFlags = mx.flags | my.flags;
	S.speed[0] = mx.speed; S.speed[1] = my.speed;
	S.accel[0] = mx.accel; S.accel[1] = my.accel;
	S.stanceState = as[0x18];
	S.cancelNormal = as[0x19];
	S.cancelSpecial = as[0x1A];
	S.hitsNumber = as[0x1C];
	S.canMove = as[0x1D] != 0;
	uint32_t f1 = rdu32(as + 0x24), f2 = rdu32(as + 0x28);
	S.statusFlags[0] = f1;
	S.statusFlags[1] = f2 & ~0x00FF0000u;
	S.invincibility = (f2 >> 16) & 0xF;
	S.counterType = (f2 >> 20) & 0xF;
	S.maxSpeedX = rd16(as + 0x30);

	// --- AT ---
	if (R.hadAT) {
		const uint8_t *a = R.at;
		Frame_AT &T = F.AT;
		T.guard_flags = rdu32(a + 0);
		for (int i = 0; i < 3; i++) {
			uint16_t hv = (uint16_t)rd16(a + 8 + 2*i), gv = (uint16_t)rd16(a + 0x18 + 2*i);
			T.hitVector[i] = hv & 0xFF; T.hVFlags[i] = hv >> 8;
			T.guardVector[i] = gv & 0xFF; T.gVFlags[i] = gv >> 8;
		}
		T.hitEffect = rd16(a + 0x28);
		T.soundEffect = rd16(a + 0x2A);
		T.hitStopTime = rd16(a + 0x30);
		T.untechTime = rd16(a + 0x32);
		T.addedEffect = a[0x38];
		T.hitgrab = a[0x39] != 0;
		T.hitStop = a[0x3A];
		T.correction = DecodeCorrection(a[0x3B]);
		T.correction_type = a[0x3C];
		T.otherFlags = rdu32(a + 0x40);
		T.damage = rd16(a + 0x48);
		T.meter_gain = rd16(a + 0x4A);
		T.red_damage = rd16(a + 0x4C);     // ガード減少値 (guard reduction) -> ATVV[0]
		T.guard_damage = rd16(a + 0x4E);   // 気絶値 (stun) -> ATVV[2]
		T.breakTime = rd16(a + 0x50);
		T.damageProration = 100;
	}

	// --- IF / EF ---
	F.IF.clear();
	for (int k = 0; k < 8; k++) {
		if (R.idx(8 + k) == -1) continue;
		const uint8_t *r = R.ifr[k];
		Frame_IF c{};
		c.type = rd16(r);
		for (int i = 0; i < 9; i++) c.parameters[i] = rd32(r + 4 + 4*i);
		F.IF.push_back(c);
	}
	F.EF.clear();
	for (int k = 0; k < 8; k++) {
		if (R.idx(16 + k) == -1) continue;
		const uint8_t *r = R.efr[k];
		Frame_EF e{};
		e.type = rd16(r);
		e.number = rd16(r + 2);
		for (int i = 0; i < 8; i++) e.parameters[i] = rd32(r + 4 + 4*i);
		for (int i = 0; i < 4; i++) e.parameters[8 + i] = rd16(r + 0x24 + 2*i);
		if (EfHasPositionBias(e.type)) { e.parameters[0] -= 128; e.parameters[1] -= 224; }
		F.EF.push_back(e);
	}

	// --- boxes ---
	bool parts = spr < 10000;
	F.hitboxes.clear();
	for (int k = 0; k < 33; k++) {
		if (!(R.boxMask & (1ull << k))) continue;
		Hitbox hb{};
		BoxToModel(R.box[k], parts, hb.xy);
		F.hitboxes[k] = hb;
	}
}

bool Load(FrameData &fd, const uint8_t *b, size_t size, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	if (!IsHA4(b, size) || size < 0x444) return fail("not a Hantei4 file");

	uint32_t partsOff = rdu32(b + 0x14), partsSize = rdu32(b + 0x18);
	uint32_t cgOff = rdu32(b + 0x1C), cgSize = rdu32(b + 0x20);
	if ((uint64_t)partsOff + partsSize > size || (uint64_t)cgOff + cgSize > size || partsOff < 0x444)
		return fail("header blob offsets out of range");
	uint64_t namesOff = (uint64_t)partsOff + partsSize + cgSize;
	if (namesOff + kNamesSize > size) return fail("pattern name table out of range");

	auto cont = std::make_shared<Ha4Container>();
	memcpy(cont->header, b, 0x44);
	cont->parts.assign(b + partsOff, b + partsOff + partsSize);
	cont->cg.assign(b + cgOff, b + cgOff + cgSize);

	fd.initEmpty(kPatterns);     // 256 slots, m_loaded
	fd.m_ha4 = cont;

	for (int p = 0; p < kPatterns; p++) {
		Sequence &seq = fd.m_sequences[p];
		seq = Sequence{};
		const uint8_t *nm = b + namesOff + 64 * p;
		memcpy(seq.ha4.name, nm, 64);
		seq.ha4.nameValid = true;
		char nbuf[65]{}; memcpy(nbuf, nm, 64);
		seq.name = sj2utf8(std::string(nbuf));

		int32_t off = rd32(b + 0x44 + 4 * p);
		if (off == -1) continue;
		if (off < 0x444 || (uint64_t)off + 0x44 > partsOff) return fail("pattern offset out of range");
		const uint8_t *ph = b + off;
		int nf = rd32(ph);
		if (nf < 1 || nf > kMaxFrames) return fail("bad frame count");
		if ((uint64_t)off + 0x44 + (uint64_t)kFrameSize * nf > partsOff) return fail("frames out of range");

		memcpy(seq.ha4.hdr, ph, 0x44);
		seq.ha4.valid = true;
		seq.psts = rd32(ph + 4) & 0xF;
		seq.level = rd32(ph + 8);
		seq.initialized = true;
		seq.empty = false;

		auto tbl = [&](int32_t rel) -> const uint8_t * { return rel == -1 ? nullptr : ph + rel; };
		const uint8_t *boxT = tbl(rd32(ph + 0x0C)), *atT = tbl(rd32(ph + 0x10));
		const uint8_t *ifT = tbl(rd32(ph + 0x14)), *efT = tbl(rd32(ph + 0x18));
		const uint8_t *end = b + partsOff;
		auto inRange = [&](const uint8_t *t, int idx, int sz) {
			return t && idx >= 0 && t + (size_t)(idx + 1) * sz <= end;
		};

		seq.frames.resize(nf);
		for (int f = 0; f < nf; f++) {
			Frame &F = seq.frames[f];
			Ha4FrameRaw &R = F.ha4;
			R.valid = true;
			memcpy(R.rec, ph + 0x44 + kFrameSize * f, kFrameSize);
			int ai = R.idx(0);
			if (ai != -1) {
				if (!inRange(atT, ai, 88)) return fail("AT index out of range");
				memcpy(R.at, atT + 88 * ai, 88);
				R.hadAT = true;
			}
			for (int k = 0; k < 8; k++) {
				int ii = R.idx(8 + k);
				if (ii != -1) {
					if (!inRange(ifT, ii, 52)) return fail("IF index out of range");
					memcpy(R.ifr[k], ifT + 52 * ii, 52);
				}
				int ei = R.idx(16 + k);
				if (ei != -1) {
					if (!inRange(efT, ei, 52)) return fail("EF index out of range");
					memcpy(R.efr[k], efT + 52 * ei, 52);
				}
			}
			for (int k = 0; k < 33; k++) {
				int bi = R.idx(k < 25 ? 24 + k : 49 + (k - 25));
				if (bi == -1) continue;
				if (!inRange(boxT, bi, 8)) return fail("box index out of range");
				for (int j = 0; j < 4; j++) R.box[k][j] = rd16(boxT + 8 * bi + 2 * j);
				R.boxMask |= 1ull << k;
			}
			DecodeFrame(F, R);
		}
	}

	for (auto &s : fd.m_sequences) s.modified = false;
	return true;
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------
namespace {

struct PatternTables {
	std::vector<uint8_t> boxes, at, ifs, efs;
	int nBox = 0, nAt = 0, nIf = 0, nEf = 0;
};

static bool BoxNonEmpty(const int16_t b[4]) { return b[0] != b[2] && b[1] != b[3]; } // IsBoxNonEmpty 0x40F730

struct Ctx {
	std::string *err;
	std::vector<std::string> *warn;
	int pat, frame;
	void w(const std::string &m) {
		if (warn) warn->push_back("pattern " + std::to_string(pat) + " frame " + std::to_string(frame) + ": " + m);
	}
};

static bool LayerHasTint(const Layer_Type &L) { return L.rgba[0] != 1.f || L.rgba[1] != 1.f || L.rgba[2] != 1.f; }

// Encode one frame record plus its table records.
static bool EncodeFrame(const Frame &F, PatternTables &T, uint8_t out[216], Ctx &cx)
{
	const Ha4FrameRaw &R = F.ha4;
	const bool raw = R.valid;
	if (raw) memcpy(out, R.rec, 216);
	else {
		memset(out, 0, 216);
		memset(out + 100, 0xFF, 116);   // every index -1 (the original writer leaves heap garbage in 1..7/57)
	}
	uint8_t *af = out, *as = out + 44;

	// ---------------- AF ----------------
	Layer_Type L{};
	if (!F.AF.layers.empty()) L = F.AF.layers[0];
	if (F.AF.layers.size() > 1) cx.w("only layer 0 is stored (HA4 has one sprite per frame)");
	if (LayerHasTint(L)) cx.w("AFRG colour tint has no HA4 field (dropped)");

	int spr = L.usePat ? L.spriteId : (L.spriteId < 0 ? L.spriteId : L.spriteId + 10000);
	if (F.AF.layers.empty()) spr = -1;
	wr16(af + 0, spr);
	wr16(af + 2, L.offset_x);
	wr16(af + 4, L.offset_y);
	wr16(af + 6, F.AF.duration);

	{   // flip mode + rotation
		float rawRot[3];
		DecodeFlip(raw ? R.rec[8] : 0, raw ? rd16(R.rec + 0x24) : 0, rawRot);
		if (!(raw && rawRot[0] == L.rotation[0] && rawRot[1] == L.rotation[1] && rawRot[2] == L.rotation[2])) {
			int mode, rot;
			EncodeFlip(L.rotation, mode, rot);
			if (L.rotation[0] != 0.f && mode != 2) cx.w("X rotation combined with other rotation is not representable");
			af[8] = u8c(mode);
			wr16(af + 0x24, rot);
		}
	}
	{   // blend + amount
		bool keep = false;
		if (raw) {
			int rb = R.rec[9];
			float ra = rb ? R.rec[0xA] / 255.f : 1.f;
			keep = (rb == L.blend_mode && ra == L.rgba[3]);
		}
		if (!keep) {
			int blend = L.blend_mode;
			if (blend == 0 && L.rgba[3] != 1.f) blend = 1;   // same rule as the HA6 writer
			af[9] = u8c(blend);
			af[0xA] = blend ? u8c((int)lroundf(std::clamp(L.rgba[3], 0.f, 1.f) * 255.f)) : (raw ? R.rec[0xA] : 0);
		}
	}
	{   // AniFlag
		int rawAni = raw ? R.rec[0xB] : -1;
		bool keep = rawAni >= 0 && rawAni < 6 && kAniMap[rawAni][0] == F.AF.aniType && (unsigned)kAniMap[rawAni][1] == F.AF.aniFlag;
		if (raw && rawAni >= 6 && F.AF.aniType == 0 && F.AF.aniFlag == 0) keep = true;
		if (!keep) af[0xB] = u8c(EncodeAni(F.AF.aniType, F.AF.aniFlag));
	}
	af[0xC] = u8c(F.AF.jump);
	af[0xD] = u8c(F.AF.landJump);
	af[0xE] = u8c(F.AF.priority);
	for (int i = 0; i < 2; i++) {
		int rz = raw ? rd16(R.rec + 0x10 + 2*i) : 0;
		if (!(raw && DecodeZoom(rz) == L.scale[i]))
			wr16(af + 0x10 + 2*i, L.scale[i] == 1.f ? 0 : (int)lroundf(L.scale[i] * 256.f));
	}
	af[0x14] = u8c(F.AF.interpolationType);
	af[0x15] = u8c(F.AF.loopEnd);
	af[0x16] = u8c(F.AF.loopCount);

	// ---------------- AS ----------------
	const Frame_AS &S = F.AS;
	for (int axis = 0; axis < 2; axis++) {
		bool y = axis == 1;
		unsigned setBit = y ? 0x01 : 0x10, addBit = y ? 0x02 : 0x20;
		AxisMove want{ S.movementFlags & (setBit | addBit), S.speed[axis], S.accel[axis] };
		if (!(want.flags & (setBit | addBit))) { want.speed = 0; want.accel = 0; }
		bool keep = false;
		if (raw) {
			const uint8_t *ras = R.rec + 44;
			AxisMove have = DecodeAxis(ras[axis], ras[2 + axis], rd16(ras + 8 + 2*axis), rd16(ras + 0x10 + 2*axis), y);
			// the model may carry speeds on a flag-less axis (HA6 files do); HA4 cannot express them
			keep = have.flags == want.flags && have.speed == want.speed && have.accel == want.accel;
		}
		if (!keep) {
			if (want.flags & setBit) { as[axis] = 1; as[2 + axis] = 1; }
			else if (want.flags & addBit) { as[axis] = 0; as[2 + axis] = 1; }
			else { as[axis] = 0; as[2 + axis] = 0; }
			wr16(as + 8 + 2*axis, want.speed);
			wr16(as + 0x10 + 2*axis, want.accel);
		}
		if ((S.movementFlags & (setBit | addBit)) == 0 && (S.speed[axis] || S.accel[axis]))
			cx.w("speed/accel on an axis without Set/Add flag is not stored");
	}
	as[0x18] = u8c(S.stanceState);
	as[0x19] = u8c(S.cancelNormal);
	as[0x1A] = u8c(S.cancelSpecial);
	as[0x1C] = u8c(S.hitsNumber);
	if (!(raw && (R.rec[44 + 0x1D] != 0) == S.canMove)) as[0x1D] = S.canMove ? 1 : 0;
	wr32(as + 0x24, S.statusFlags[0]);
	wr32(as + 0x28, (S.statusFlags[1] & ~0x00FF0000u) | ((S.invincibility & 0xF) << 16) | ((S.counterType & 0xF) << 20));
	wr16(as + 0x30, S.maxSpeedX);
	if (S.sineFlags) cx.w("AST0 sine motion has no HA4 field (dropped)");

	// ---------------- boxes ----------------
	const bool partsOut = spr < 10000;
	bool hasAttack = false;
	for (int k = 0; k < 33; k++) {
		int slot = k < 25 ? 24 + k : 49 + (k - 25);
		auto it = F.hitboxes.find(k);
		int16_t bx[4];
		bool have = false;
		if (it != F.hitboxes.end()) {
			bool keep = false;
			if (raw && (R.boxMask & (1ull << k))) {
				int dec[4];
				BoxToModel(R.box[k], partsOut, dec);
				keep = !memcmp(dec, it->second.xy, sizeof(dec));
			}
			if (keep) memcpy(bx, R.box[k], sizeof(bx));
			else BoxFromModel(it->second.xy, partsOut, bx);
			have = BoxNonEmpty(bx);
		}
		if (!have) { wr16(out + 100 + 2*slot, -1); continue; }
		wr16(out + 100 + 2*slot, T.nBox++);
		size_t o = T.boxes.size(); T.boxes.resize(o + 8);
		for (int j = 0; j < 4; j++) wr16(&T.boxes[o + 2*j], bx[j]);
		if (k >= 25) hasAttack = true;
	}
	for (auto &hb : F.hitboxes)
		if (hb.first < 0 || hb.first > 32) cx.w("box slot " + std::to_string(hb.first) + " out of HA4 range");

	// ---------------- AT ----------------
	if (hasAttack) {
		uint8_t a[88];
		if (raw && R.hadAT) memcpy(a, R.at, 88); else memset(a, 0, 88);
		const Frame_AT &A = F.AT;
		wr32(a + 0, A.guard_flags);
		for (int i = 0; i < 3; i++) {
			wr16(a + 8 + 2*i, (A.hitVector[i] & 0xFF) | (A.hVFlags[i] << 8));
			wr16(a + 0x18 + 2*i, (A.guardVector[i] & 0xFF) | (A.gVFlags[i] << 8));
		}
		wr16(a + 0x28, A.hitEffect);
		wr16(a + 0x2A, A.soundEffect);
		wr16(a + 0x30, A.hitStopTime);
		wr16(a + 0x32, A.untechTime);
		a[0x38] = u8c(A.addedEffect);
		if (!(raw && R.hadAT && (R.at[0x39] != 0) == A.hitgrab)) a[0x39] = A.hitgrab ? 1 : 0;
		a[0x3A] = u8c(A.hitStop);
		if (!(raw && R.hadAT && DecodeCorrection(R.at[0x3B]) == A.correction))
			a[0x3B] = u8c(A.correction == 100 ? 0 : A.correction);
		a[0x3C] = u8c(A.correction_type);
		wr32(a + 0x40, A.otherFlags);
		wr16(a + 0x48, A.damage);
		wr16(a + 0x4A, A.meter_gain);
		wr16(a + 0x4C, A.red_damage);
		wr16(a + 0x4E, A.guard_damage);
		wr16(a + 0x50, A.breakTime);
		if (A.extraGravity != 0.f) cx.w("ATUH extra gravity has no HA4 field (dropped)");
		if (A.blockStopTime) cx.w("ATGN block stop has no HA4 field (dropped)");
		wr16(out + 100, T.nAt++);
		T.at.insert(T.at.end(), a, a + 88);
	} else {
		wr16(out + 100, -1);
	}

	// ---------------- IF / EF ----------------
	// Slot positions: reuse the original ones when the number of records is unchanged,
	// otherwise pack from slot 0.
	auto slotsFor = [&](int base, size_t count, int slots[8]) {
		int rawSlots[8], n = 0;
		if (raw) for (int k = 0; k < 8; k++) if (R.idx(base + k) != -1) rawSlots[n++] = k;
		bool reuse = raw && (size_t)n == count;
		for (size_t i = 0; i < count && i < 8; i++) slots[i] = reuse ? rawSlots[i] : (int)i;
		return reuse;
	};

	if (F.IF.size() > 8) cx.w("more than 8 conditions; extra dropped");
	if (F.EF.size() > 8) cx.w("more than 8 effects; extra dropped");
	for (int k = 0; k < 8; k++) { wr16(out + 100 + 2*(8 + k), -1); wr16(out + 100 + 2*(16 + k), -1); }

	int ifSlots[8];
	bool ifReuse = slotsFor(8, F.IF.size(), ifSlots);
	std::vector<std::pair<int, std::array<uint8_t, 52>>> ifRecs;
	for (size_t i = 0; i < F.IF.size() && i < 8; i++) {
		const Frame_IF &c = F.IF[i];
		if (c.type == 0) continue;   // the editor only stores type != 0
		std::array<uint8_t, 52> r{};
		int slot = ifSlots[i];
		if (ifReuse) memcpy(r.data(), R.ifr[slot], 52);
		bool same = ifReuse && rd16(r.data()) == c.type;
		for (int j = 0; j < 9 && same; j++) same = rd32(r.data() + 4 + 4*j) == c.parameters[j];
		if (!same) {
			wr16(r.data(), c.type);
			for (int j = 0; j < 9; j++) wr32(r.data() + 4 + 4*j, (uint32_t)c.parameters[j]);
		}
		ifRecs.push_back({slot, r});
	}
	std::sort(ifRecs.begin(), ifRecs.end(), [](auto &a, auto &b) { return a.first < b.first; });
	for (auto &r : ifRecs) {
		wr16(out + 100 + 2*(8 + r.first), T.nIf++);
		T.ifs.insert(T.ifs.end(), r.second.begin(), r.second.end());
	}

	int efSlots[8];
	bool efReuse = slotsFor(16, F.EF.size(), efSlots);
	std::vector<std::pair<int, std::array<uint8_t, 52>>> efRecs;
	for (size_t i = 0; i < F.EF.size() && i < 8; i++) {
		const Frame_EF &e = F.EF[i];
		if (e.type == 0) continue;
		std::array<uint8_t, 52> r{};
		int slot = efSlots[i];
		if (efReuse) memcpy(r.data(), R.efr[slot], 52);
		int p[12];
		memcpy(p, e.parameters, sizeof(p));
		if (EfHasPositionBias(e.type)) { p[0] += 128; p[1] += 224; }
		bool same = efReuse && rd16(r.data()) == e.type && rd16(r.data() + 2) == e.number;
		for (int j = 0; j < 8 && same; j++) same = rd32(r.data() + 4 + 4*j) == p[j];
		for (int j = 0; j < 4 && same; j++) same = rd16(r.data() + 0x24 + 2*j) == p[8 + j];
		if (!same) {
			wr16(r.data(), e.type);
			wr16(r.data() + 2, e.number);
			for (int j = 0; j < 8; j++) wr32(r.data() + 4 + 4*j, (uint32_t)p[j]);
			for (int j = 0; j < 4; j++) {
				if (p[8 + j] < -32768 || p[8 + j] > 32767) cx.w("EF param " + std::to_string(8 + j) + " truncated to int16");
				wr16(r.data() + 0x24 + 2*j, p[8 + j]);
			}
		}
		efRecs.push_back({slot, r});
	}
	std::sort(efRecs.begin(), efRecs.end(), [](auto &a, auto &b) { return a.first < b.first; });
	for (auto &r : efRecs) {
		wr16(out + 100 + 2*(16 + r.first), T.nEf++);
		T.efs.insert(T.efs.end(), r.second.begin(), r.second.end());
	}

	if (T.nBox > 32767 || T.nAt > 32767 || T.nIf > 32767 || T.nEf > 32767) {
		if (cx.err) *cx.err = "table index overflow in pattern " + std::to_string(cx.pat);
		return false;
	}
	return true;
}

} // namespace

bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err, std::vector<std::string> *warnings)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const Ha4Container *cont = fd.m_ha4.get();

	for (size_t p = kPatterns; p < fd.m_sequences.size(); p++)
		if (!fd.m_sequences[p].frames.empty())
			return fail("pattern " + std::to_string(p) + " has frames, but HA4 only has 256 pattern slots");

	out.assign(0x444, 0);
	if (cont) memcpy(out.data(), cont->header, 0x44);
	else { memcpy(out.data(), "Hantei4\0", 8); wr32(out.data() + 0x10, 1); }

	for (int p = 0; p < kPatterns; p++) {
		const Sequence *seq = p < (int)fd.m_sequences.size() ? &fd.m_sequences[p] : nullptr;
		if (!seq || seq->frames.empty()) { wr32(out.data() + 0x44 + 4*p, 0xFFFFFFFFu); continue; }
		int nf = (int)seq->frames.size();
		if (nf > kMaxFrames) return fail("pattern " + std::to_string(p) + " has " + std::to_string(nf) + " frames (HA4 max 100)");

		size_t off = out.size();
		wr32(out.data() + 0x44 + 4*p, (uint32_t)off);

		uint8_t hdr[0x44];
		if (seq->ha4.valid) memcpy(hdr, seq->ha4.hdr, 0x44); else memset(hdr, 0, 0x44);
		wr32(hdr + 0, nf);
		int32_t mi = seq->ha4.valid ? rd32(seq->ha4.hdr + 4) : 0;
		if ((mi & 0xF) != (seq->psts & 0xF)) mi = (mi & ~0xF) | (seq->psts & 0xF);
		wr32(hdr + 4, mi);
		wr32(hdr + 8, seq->level);

		PatternTables T;
		std::vector<uint8_t> frames((size_t)kFrameSize * nf);
		for (int f = 0; f < nf; f++) {
			Ctx cx{err, warnings, p, f};
			if (!EncodeFrame(seq->frames[f], T, &frames[(size_t)kFrameSize * f], cx)) return false;
		}
		int32_t pos = 0x44 + kFrameSize * nf;
		auto place = [&](int cnt, int sz) { int32_t r = cnt ? pos : -1; pos += cnt * sz; return r; };
		wr32(hdr + 0x0C, place(T.nBox, 8));
		wr32(hdr + 0x10, place(T.nAt, 88));
		wr32(hdr + 0x14, place(T.nIf, 52));
		wr32(hdr + 0x18, place(T.nEf, 52));

		out.insert(out.end(), hdr, hdr + 0x44);
		out.insert(out.end(), frames.begin(), frames.end());
		out.insert(out.end(), T.boxes.begin(), T.boxes.end());
		out.insert(out.end(), T.at.begin(), T.at.end());
		out.insert(out.end(), T.ifs.begin(), T.ifs.end());
		out.insert(out.end(), T.efs.begin(), T.efs.end());
	}

	uint32_t partsOff = (uint32_t)out.size();
	uint32_t partsSize = cont ? (uint32_t)cont->parts.size() : 0;
	uint32_t cgSize = cont ? (uint32_t)cont->cg.size() : 0;
	wr32(out.data() + 0x14, partsOff);
	wr32(out.data() + 0x18, partsSize);
	wr32(out.data() + 0x1C, partsOff + partsSize);
	wr32(out.data() + 0x20, cgSize);
	if (cont) {
		out.insert(out.end(), cont->parts.begin(), cont->parts.end());
		out.insert(out.end(), cont->cg.begin(), cont->cg.end());
	}

	// names
	size_t nOff = out.size();
	out.resize(nOff + kNamesSize, 0);
	for (int p = 0; p < kPatterns && p < (int)fd.m_sequences.size(); p++) {
		const Sequence &seq = fd.m_sequences[p];
		uint8_t *dst = &out[nOff + 64 * p];
		if (seq.ha4.nameValid) {
			char nbuf[65]{}; memcpy(nbuf, seq.ha4.name, 64);
			if (sj2utf8(std::string(nbuf)) == seq.name) { memcpy(dst, seq.ha4.name, 64); continue; }
		}
		std::string sj = utf82sj(seq.name);
		size_t n = std::min<size_t>(sj.size(), 63);
		// don't cut a double-byte character in half
		size_t i = 0;
		while (i < n) {
			unsigned char c = (unsigned char)sj[i];
			size_t w = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1;
			if (i + w > n) break;
			i += w;
		}
		memcpy(dst, sj.data(), i);
	}
	return true;
}

static std::string g_lastErr;
static std::vector<std::string> g_lastWarn;
const std::string &LastSaveError() { return g_lastErr; }
const std::vector<std::string> &LastSaveWarnings() { return g_lastWarn; }

bool SaveFile(const FrameData &fd, const char *filename, std::string *err, std::vector<std::string> *warnings)
{
	g_lastErr.clear();
	g_lastWarn.clear();
	std::vector<uint8_t> bytes;
	bool ok = Serialize(fd, bytes, &g_lastErr, &g_lastWarn);
	if (ok && !WriteFileAtomic(filename, bytes.data(), bytes.size())) {
		g_lastErr = std::string("could not write ") + filename;
		ok = false;
	}
	if (err) *err = g_lastErr;
	if (warnings) *warnings = g_lastWarn;
	return ok;
}

bool LoadFile(FrameData &fd, const char *filename, std::string *err)
{
	char *data; unsigned int size;
	if (!ReadInMem(filename, data, size)) { if (err) *err = "could not read file"; return false; }
	bool ok = Load(fd, (const uint8_t *)data, size, err);
	delete[] data;
	if (ok && fd.m_ha4) fd.m_ha4->sourcePath = filename;
	return ok;
}

} // namespace ha4
