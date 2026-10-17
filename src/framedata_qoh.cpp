#include "framedata_qoh.h"
#include "framedata_ha4.h"
#include "han2/qoh_dat.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace qoh {

using han2::qoh::Layout;
namespace q = han2::qoh;

static inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
static inline void wr32(uint8_t *p, int32_t v) { memcpy(p, &v, 4); }

constexpr int kPatterns = 256;
// raw record layout: [0, frameSize) the frame record, then copies of the boxes it references (kept in han2::kMaxFrameBytes = 404)
constexpr int kHurtOff = 64, kHurtMax = 8, kGuardOff = 128, kGuardMax = 4, kBodyOff = 160, kAtkOff = 168, kAtkMax = 8;
constexpr int kBiasX = 128, kBiasY = 224;   // frame canvas origin of the editor (offsets and boxes are stored relative to the actor)
// QohTileXform (0 identity, 1 rot90cw, 2 rot180, 3 rot90ccw, 4 flipH, 5 transpose, 6 flipV, 7 antitranspose) <-> HA4 flip mode
static const int kXformToMode[8] = { 0, 3, 4, 5, 1, 6, 2, 7 };
static const int kModeToXform[8] = { 0, 4, 6, 1, 2, 3, 5, 7 };

static bool BodyPresent(int version, uint8_t mode) { return version == q::V98 ? mode != 0xFF : (mode != 0 && mode != 0xFF); }

static void Decode(Frame &F, const Han2FrameRaw &R, int version)
{
	const uint8_t *r = R.rec;
	F.AF.layers.clear(); F.AF.layers.push_back({});
	Layer_Type &L = F.AF.layers[0];
	L.spriteId = rd32(r); L.usePat = false;
	L.offset_x = rd16(r + 4) + kBiasX; L.offset_y = rd16(r + 6) + kBiasY;
	F.AF.duration = rd32(r + 8);
	const unsigned next = (uint16_t)rd16(r + 0xC);
	if (next == 0xFFFF) { F.AF.aniType = 1; F.AF.jump = 0; } else if (next == 0xFFFE) { F.AF.aniType = 0; F.AF.jump = 0; } else { F.AF.aniType = 2; F.AF.jump = (int)next; }
	F.AF.aniFlag = 0; F.AF.landJump = (r[0x10] & 4) ? r[0x16] : 0;
	ha4::DecodeFlip(kXformToMode[r[0x15] & 7], 0, L.rotation);
	Frame_AS &S = F.AS;
	S.movementFlags = (r[0x11] ? 0x10u : 0u) | (r[0x12] ? 0x01u : 0u);
	S.speed[0] = S.speed[1] = 0; S.accel[0] = rd16(r + 0x22); S.accel[1] = rd16(r + 0x24);
	S.stanceState = r[0x21] & 7;
	F.AT = Frame_AT{};
	if (R.hadAT) { const uint8_t *a = R.at; F.AT.damage = rd16(a + 8); F.AT.meter_gain = rd16(a + 14); F.AT.hitEffect = a[0x12]; }
	F.IF.clear(); F.EF.clear(); F.hitboxes.clear();
	auto put = [&](int key, const uint8_t *rc) { Hitbox hb{}; hb.xy[0] = rd16(rc) + kBiasX; hb.xy[1] = rd16(rc + 2) + kBiasY; hb.xy[2] = rd16(rc + 4) + kBiasX; hb.xy[3] = rd16(rc + 6) + kBiasY; F.hitboxes[key] = hb; };
	if (R.boxMask & 1) put(0, r + kBodyOff);
	for (int j = 0; j < kHurtMax; j++) if (R.boxMask >> (1 + j) & 1) put(1 + j, r + kHurtOff + 8 * j);
	for (int j = 0; j < kGuardMax; j++) if (R.boxMask >> (9 + j) & 1) put(9 + j, r + kGuardOff + 8 * j);
	for (int j = 0; j < kAtkMax; j++) if (R.boxMask >> (16 + j) & 1) put(25 + j, r + kAtkOff + 24 * j);
	(void)version;
}

void RedecodeFrame(Frame &F, int version) { if (F.han2.valid) Decode(F, F.han2, version); }

bool Load(FrameData &fd, const uint8_t *b, size_t size, int version, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	Layout L;
	if (!(version == q::V98 ? q::ComputeLayout98(b, size, L, err) : q::ComputeLayout99(b, size, L, err))) return false;
	if (L.actions != 256) return fail("a QoH character has exactly 256 actions");
	auto cont = std::make_shared<Han2Container>();
	cont->sub = 5; cont->kind = (uint32_t)version;
	cont->cg.assign(b, b + size);            // the whole plaintext file: authoritative bytes, patched in place by Serialize
	fd.initEmpty(kPatterns);
	fd.m_han2 = cont;
	const uint8_t *acts = b + L.actionOff;
	for (int p = 0; p < kPatterns; p++) {
		Sequence &seq = fd.m_sequences[p];
		seq = Sequence{}; seq.name = "action " + std::to_string(p);
		const uint8_t *a = acts + 24 * p;
		const int32_t first = rd32(a), cnt = rd32(a + 4), boxBase = rd32(a + 8), atkBase = rd32(a + 16);
		if (first == -1 || cnt <= 0) continue;
		if ((uint64_t)first + cnt > L.frames) return fail("action " + std::to_string(p) + " runs past the frame table");
		seq.han2.valid = true; seq.initialized = true; seq.empty = false;
		seq.frames.resize((size_t)cnt);
		for (int k = 0; k < cnt; k++) {
			Frame &F = seq.frames[(size_t)k]; Han2FrameRaw &R = F.han2;
			R.valid = true; R.frameSize = (uint16_t)L.frameSize;
			const uint8_t *fr = b + L.frameOff + L.frameSize * (size_t)(first + k);
			memcpy(R.rec, fr, L.frameSize);
			auto boxAt = [&](int idx, uint8_t *dst) { const int64_t g = (int64_t)boxBase + idx; if (boxBase < 0 || idx < 0 || g >= (int64_t)L.boxes) return false; memcpy(dst, b + L.boxOff + 8 * g, 8); return true; };
			if (BodyPresent(version, fr[0x18]) && boxAt(fr[0x17], R.rec + kBodyOff)) R.boxMask |= 1;
			for (int j = 0; j < std::min<int>(fr[0x1A], kHurtMax); j++) if (boxAt(fr[0x19] + j, R.rec + kHurtOff + 8 * j)) R.boxMask |= 1u << (1 + j);
			for (int j = 0; j < std::min<int>(fr[0x1E], kGuardMax); j++) if (boxAt(fr[0x1D] + j, R.rec + kGuardOff + 8 * j)) R.boxMask |= 1u << (9 + j);
			if (fr[0x20] >= 1 && fr[0x20] <= kAtkMax)
				for (int j = 0; j < fr[0x20]; j++) {
					const int64_t g = (int64_t)atkBase + fr[0x1F] + j;
					if (atkBase < 0 || g >= (int64_t)L.attacks) continue;
					memcpy(R.rec + kAtkOff + 24 * j, b + L.attackOff + 24 * g, 24); R.boxMask |= 1u << (16 + j);
				}
			if (R.boxMask >> 16 & 1) { memcpy(R.at, R.rec + kAtkOff, 24); R.hadAT = true; }
			Decode(F, R, version);
		}
	}
	for (auto &s : fd.m_sequences) s.modified = false;
	return true;
}

static uint8_t u8c(int v) { return (uint8_t)std::clamp(v, 0, 255); }

// Re-encodes the fields the user changed (model value != what the raw record decodes to) into `rec`; boxes are returned in `rects` for the caller to write.
static bool Encode(const Frame &F, uint8_t *rec, int version, std::string *err, const char *where)
{
	const Han2FrameRaw &R = F.han2;
	if (!R.valid) { if (err) *err = std::string(where) + ": a new frame has no record yet (adding frames is not supported: the sprite table is allotted per action)"; return false; }
	Han2FrameRaw base = R; Frame ref; Decode(ref, base, version);
	Layer_Type L{}; if (!F.AF.layers.empty()) L = F.AF.layers[0];
	Layer_Type Lr = ref.AF.layers[0];
	if (L.spriteId != Lr.spriteId) wr32(rec, L.spriteId);
	if (L.offset_x != Lr.offset_x) wr16(rec + 4, L.offset_x - kBiasX);
	if (L.offset_y != Lr.offset_y) wr16(rec + 6, L.offset_y - kBiasY);
	if (F.AF.duration != ref.AF.duration) wr32(rec + 8, F.AF.duration);
	if (F.AF.aniType != ref.AF.aniType || F.AF.jump != ref.AF.jump) { const int nx = F.AF.aniType == 1 ? 0xFFFF : F.AF.aniType == 0 ? 0xFFFE : F.AF.jump; wr16(rec + 0xC, nx); }
	if (F.AF.landJump != ref.AF.landJump) { rec[0x16] = u8c(F.AF.landJump); if (F.AF.landJump) rec[0x10] |= 4; else rec[0x10] &= (uint8_t)~4; }
	if (!(L.rotation[0] == Lr.rotation[0] && L.rotation[1] == Lr.rotation[1] && L.rotation[2] == Lr.rotation[2])) { int m, rot; ha4::EncodeFlip(L.rotation, m, rot); rec[0x15] = (uint8_t)kModeToXform[std::min(m, 7)]; }
	const Frame_AS &S = F.AS, &Sr = ref.AS;
	if (S.movementFlags != Sr.movementFlags) { rec[0x11] = (S.movementFlags & 0x10) ? 1 : 0; rec[0x12] = (S.movementFlags & 0x01) ? 1 : 0; }
	if (S.accel[0] != Sr.accel[0]) wr16(rec + 0x22, S.accel[0]);
	if (S.accel[1] != Sr.accel[1]) wr16(rec + 0x24, S.accel[1]);
	if (S.stanceState != Sr.stanceState) rec[0x21] = (uint8_t)((rec[0x21] & ~7) | (S.stanceState & 7));
	// the first attack record: damage / meter / hit class
	if (R.hadAT) {
		uint8_t *a = rec + kAtkOff;
		if (F.AT.damage != ref.AT.damage) wr16(a + 8, F.AT.damage);
		if (F.AT.meter_gain != ref.AT.meter_gain) wr16(a + 14, F.AT.meter_gain);
		if (F.AT.hitEffect != ref.AT.hitEffect) a[0x12] = u8c(F.AT.hitEffect);
	}
	// boxes: model keys vs the present mask; moved/resized rectangles are written back, additions and holes are refused
	auto check = [&](int key, int slotBit, uint8_t *dst) {
		const bool had = (R.boxMask >> slotBit) & 1; auto it = F.hitboxes.find(key);
		if (it == F.hitboxes.end()) return !had ? true : false;
		if (!had) return false;
		wr16(dst, it->second.xy[0] - kBiasX); wr16(dst + 2, it->second.xy[1] - kBiasY); wr16(dst + 4, it->second.xy[2] - kBiasX); wr16(dst + 6, it->second.xy[3] - kBiasY);
		return true;
	};
	bool ok = check(0, 0, rec + kBodyOff);
	for (int j = 0; j < kHurtMax && ok; j++) ok = check(1 + j, 1 + j, rec + kHurtOff + 8 * j);
	for (int j = 0; j < kGuardMax && ok; j++) ok = check(9 + j, 9 + j, rec + kGuardOff + 8 * j);
	for (int j = 0; j < kAtkMax && ok; j++) ok = check(25 + j, 16 + j, rec + kAtkOff + 24 * j);
	if (!ok) { if (err) *err = std::string(where) + ": boxes cannot be added or removed in a QoH frame (the box tables are allotted per action); move or resize the existing ones"; return false; }
	return true;
}

bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const Han2Container *cont = fd.m_han2.get();
	if (!cont || cont->sub != 5) return fail("not a QoH character");
	const int version = (int)cont->kind;
	Layout L;
	if (!(version == q::V98 ? q::ComputeLayout98(cont->cg.data(), cont->cg.size(), L, nullptr) : q::ComputeLayout99(cont->cg.data(), cont->cg.size(), L, nullptr))) return fail("corrupt container");
	out = cont->cg;
	for (int p = 0; p < kPatterns && p < (int)fd.m_sequences.size(); p++) {
		const uint8_t *a = out.data() + L.actionOff + 24 * p;
		const int32_t first = rd32(a), cnt = rd32(a + 4), boxBase = rd32(a + 8), atkBase = rd32(a + 16);
		const Sequence &seq = fd.m_sequences[p];
		if (first == -1 || cnt <= 0) { if (!seq.frames.empty()) return fail("action " + std::to_string(p) + " is unused in the file: frames cannot be added to it"); continue; }
		if ((int)seq.frames.size() != cnt) return fail("action " + std::to_string(p) + ": adding or removing frames is not supported (the sprite table is allotted per action)");
		for (int k = 0; k < cnt; k++) {
			const Frame &F = seq.frames[(size_t)k]; const Han2FrameRaw &R = F.han2;
			uint8_t rec[han2::kMaxFrameBytes]; memcpy(rec, R.rec, sizeof rec);
			std::string e; const std::string where = "action " + std::to_string(p) + " frame " + std::to_string(k);
			if (!Encode(F, rec, version, &e, where.c_str())) return fail(e);
			memcpy(out.data() + L.frameOff + L.frameSize * (size_t)(first + k), rec, L.frameSize);
			const uint8_t *fr = rec;
			auto wb = [&](int bit, int idx, const uint8_t *src) { if (!(R.boxMask >> bit & 1)) return; const int64_t g = (int64_t)boxBase + idx; if (g >= 0 && g < (int64_t)L.boxes) memcpy(out.data() + L.boxOff + 8 * g, src, 8); };
			wb(0, fr[0x17], rec + kBodyOff);
			for (int j = 0; j < kHurtMax; j++) wb(1 + j, fr[0x19] + j, rec + kHurtOff + 8 * j);
			for (int j = 0; j < kGuardMax; j++) wb(9 + j, fr[0x1D] + j, rec + kGuardOff + 8 * j);
			for (int j = 0; j < kAtkMax; j++) if (R.boxMask >> (16 + j) & 1) { const int64_t g = (int64_t)atkBase + fr[0x1F] + j; if (g >= 0 && g < (int64_t)L.attacks) memcpy(out.data() + L.attackOff + 24 * g, rec + kAtkOff + 24 * j, 24); }
		}
	}
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
	if (fd.m_han2->kind == (uint32_t)q::V99) q::Encrypt99(plain, q::StemOfPath(filename ? filename : ""));
	if (!WriteFileAtomic(filename, plain.data(), plain.size())) { g_lastErr = std::string("could not write ") + filename; return done(false); }
	return done(true);
}

} // namespace qoh
