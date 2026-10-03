// French-Bread HAN2RBO (RBO .DAT/.DT2) <-> Hantei-chan model. See framedata_han2.h and docs/formats/frenchbread_rbo_gof.md.
#include "framedata_han2.h"
#include "framedata_ha4.h"   // flip-mode helpers shared with the MBAC loader
#include "han2/han2_container.h"
#include "han2/rbo_types_gen.h"
#include "han2/rbo_at_gen.h"
#include "misc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <filesystem>

namespace han2 {

static inline int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t rdu32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline void wr16(uint8_t *p, int v) { int16_t t = (int16_t)v; memcpy(p, &t, 2); }
static inline void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

bool IsHan2(const void *data, size_t size) { return han2::IsHan2((const uint8_t *)data, size); }

// ---------------------------------------------------------------------------
// RBO layout: frame box slots. Order = ascending frame offset = the order in which the game's editor numbers the
// shared box table (first use), which makes the rebuilt table byte-identical (checked on all 346 files).
// model key: Hantei-chan box numbering (0 collision, 1..8 hurt, 9..24 special/clash/projectile, 25+ attack).
// ---------------------------------------------------------------------------
static const BoxSlotInfo kRboSlots[] = {
	{11, 0xD4,  0xD0,  "sousai",  0}, {16, 0xD8,  0xD0,  "sousai",  1}, {17, 0xDC,  0xD0,  "sousai",  2},
	{12, 0xE4,  0xE0,  "tobi",    0}, {18, 0xE8,  0xE0,  "tobi",    1}, {19, 0xEC,  0xE0,  "tobi",    2}, {20, 0xF0, 0xE0, "tobi", 3},
	{ 9, 0xF8,  0xF4,  "effect",  0},
	{ 1, 0x100, 0xFC,  "hurt",    0}, { 2, 0x104, 0xFC,  "hurt",    1}, { 3, 0x108, 0xFC,  "hurt",    2},
	{ 0, 0x110, 0x10C, "kasanari",0}, {13, 0x114, 0x10C, "kasanari",1}, {14, 0x118, 0x10C, "kasanari",2}, {15, 0x11C, 0x10C, "kasanari", 3},
	{25, 0x124, 0x120, "attack",  0}, {26, 0x128, 0x120, "attack",  1},
};
static const int kRboSlotCount = (int)(sizeof(kRboSlots) / sizeof(kRboSlots[0]));
const BoxSlotInfo *RboBoxSlots(int &count) { count = kRboSlotCount; return kRboSlots; }

const char *BoxLabel(int k)
{
	switch (k) {
	case 0: return "Overlap box (Kasanari)";
	case 1: case 2: case 3: return "Hurt box (Yarare)";
	case 9: return "Hit-spark box (Etc)";
	case 11: case 16: case 17: return "Clash box (Sousai)";
	case 12: case 18: case 19: case 20: return "Projectile-hit box (Tobi)";
	case 13: case 14: case 15: return "Overlap box (extra)";
	case 25: case 26: return "Attack box (Kougeki)";
	default: return nullptr;
	}
}

// ---------------------------------------------------------------------------
// decoders (raw -> model). The writer reuses them to decide whether the original bytes still represent the model.
// ---------------------------------------------------------------------------
// ani flag (frame +0x0B) -> HA6 (aniType, aniFlag): 0 End, 1 Next, 2 Jump, 3 Next+land, 4 Jump+land, 5 loop. 6..9 have no HA6 equivalent.
static const int kAniMap[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };

static void DecodeAxis(int clear, int add, int speed, int accel, bool yAxis, unsigned &flags, int &spd, int &acc)
{
	unsigned setBit = yAxis ? 0x01 : 0x10, addBit = yAxis ? 0x02 : 0x20;
	flags = 0; spd = 0; acc = 0;
	if (add) { flags = clear ? setBit : addBit; spd = speed; acc = accel; }
	else if (clear) flags = setBit;
}

static void DecodeFrame(Frame &F, const Han2FrameRaw &R)
{
	RboFrameRecord r; memcpy(&r, R.rec, sizeof(r));

	F.AF.layers.clear();
	F.AF.layers.push_back({});
	Layer_Type &L = F.AF.layers[0];
	int spr = r.spriteId;
	if (spr >= 10000) { L.spriteId = spr - 10000; L.usePat = false; }
	else if (spr >= 0) { L.spriteId = spr; L.usePat = true; }
	else { L.spriteId = spr; L.usePat = false; }
	// CG images are placed at actor + offset + canvas position (Actor_DrawCgSprite 0x4470E0); the Hantei-chan renderer
	// shifts CG layers by (-128,-224), so CG frames carry that bias in the model (parts frames do not).
	const bool cgSprite = spr >= 10000;
	L.offset_x = r.offsetX + (cgSprite ? 128 : 0);
	L.offset_y = r.offsetY + (cgSprite ? 224 : 0);
	F.AF.duration = r.duration;
	ha4::DecodeFlip(r.flipMode, 0, L.rotation);
	L.blend_mode = r.blendMode;
	L.rgba[3] = r.blendMode ? r.alpha / 255.f : 1.f;
	int ani = r.aniFlag;
	if (ani < 6) { F.AF.aniType = kAniMap[ani][0]; F.AF.aniFlag = kAniMap[ani][1]; }
	else { F.AF.aniType = 1; F.AF.aniFlag = 0; }
	F.AF.jump = r.jumpTarget;
	F.AF.landJump = r.landJumpTarget;
	F.AF.priority = r.drawPriorityCode;
	float z = (r.fxFlags & FXF_USE_ZOOM) ? r.zoom / 256.f : 1.f;
	L.scale[0] = L.scale[1] = z;
	F.AF.interpolationType = r.interpolationMode;
	F.AF.loopCount = r.loopCount;
	F.AF.loopEnd = r.loopEndFrame;

	Frame_AS &S = F.AS;
	unsigned fx, fy; int sx, sy, ax, ay;
	DecodeAxis(r.moveFlags & MOVE_CLEAR_VEL_X, r.moveFlags & MOVE_ADD_X, r.speedX, r.accelX, false, fx, sx, ax);
	DecodeAxis(r.moveFlags & MOVE_CLEAR_VEL_Y, r.moveFlags & MOVE_ADD_Y, r.speedY, r.accelY, true, fy, sy, ay);
	S.movementFlags = fx | fy;
	S.speed[0] = sx; S.speed[1] = sy; S.accel[0] = ax; S.accel[1] = ay;
	S.stanceState = r.stanceClass;
	S.cancelNormal = r.normalCancel;
	S.cancelSpecial = r.specialCancel;
	S.hitsNumber = (int)r.hitLimitCount;

	if (R.hadAT) {
		RboAtRecord a; memcpy(&a, R.at, sizeof(a));
		F.AT.damage = a.base_power;
		F.AT.guard_damage = a.guard_damage;
		F.AT.hitEffect = a.hit_class;
	}

	F.IF.clear(); F.EF.clear();
	F.hitboxes.clear();
	for (int k = 0; k < kRboSlotCount; k++) {
		if (!(R.boxMask & (1u << k))) continue;
		Hitbox hb{};
		for (int j = 0; j < 4; j++) hb.xy[j] = R.box[k][j];
		F.hitboxes[kRboSlots[k].key] = hb;
	}
}

void RedecodeFrame(Frame &F) { if (F.han2.valid) DecodeFrame(F, F.han2); }

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------
bool Load(FrameData &fd, const uint8_t *b, size_t size, std::string *err, const uint8_t *names, size_t namesSize)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	Han2File f;
	std::string perr;
	if (!Parse(b, size, f, &perr)) return fail(perr);
	if (f.sub != 1) return fail("GOF2 (sub 2) layout is not supported yet");

	const std::vector<uint8_t> &patT = f.sec[0], &frames = f.sec[1], &boxes = f.sec[2], &ats = f.sec[3];
	if (patT.size() != 256 * 12) return fail("pattern table is not 256 entries");
	if (frames.size() % 300) return fail("frame section is not a multiple of 300 bytes");
	if (ats.size() % 120) return fail("AT section is not a multiple of 120 bytes");
	if (boxes.size() % 8) return fail("box section is not a multiple of 8 bytes");
	if (f.sec[6].size() % 20 || f.sec[7].size() % 20) return fail("script list section is not a multiple of 20 bytes");
	const int nFrames = (int)(frames.size() / 300), nAt = (int)(ats.size() / 120), nBox = (int)(boxes.size() / 8);
	const int nSl[2] = { (int)(f.sec[6].size() / 20), (int)(f.sec[7].size() / 20) };

	auto cont = std::make_shared<Han2Container>();
	memcpy(cont->header, f.header, 0x40);
	cont->sub = f.sub; cont->kind = f.kind;
	cont->lead = f.lead; cont->tail = f.tail;
	for (int i = 4; i < 9 && i < (int)f.sec.size(); i++) cont->sec[i] = f.sec[i];
	cont->sec[6].assign(f.sec[6].begin(), f.sec[6].begin() + std::min<size_t>(20, f.sec[6].size()));   // record 0
	cont->sec[7].assign(f.sec[7].begin(), f.sec[7].begin() + std::min<size_t>(20, f.sec[7].size()));
	cont->parts = f.area[kAreaParts]; cont->cg = f.area[kAreaCg]; cont->names = f.area[kAreaNames];
	for (int i = 0; i < 4; i++) cont->areaOff[i] = f.areaOff[i];
	const uint8_t *nameBytes = nullptr;
	if (cont->names.size() >= (size_t)kNamesSize) nameBytes = cont->names.data();
	else if (names && namesSize >= (size_t)kNamesSize) nameBytes = names;

	fd.initEmpty(kPatterns);
	fd.m_han2 = cont;

	int maxBox = -1, expectFirst = 0;
	for (int p = 0; p < kPatterns; p++) {
		Sequence &seq = fd.m_sequences[p];
		seq = Sequence{};
		if (nameBytes) {
			memcpy(seq.han2.name, nameBytes + 64 * p, 64);
			seq.han2.nameValid = true;
			char nbuf[65]{}; memcpy(nbuf, nameBytes + 64 * p, 64);
			seq.name = sj2utf8(std::string(nbuf));
		}
		uint32_t cnt = rdu32(patT.data() + 12 * p), flags = rdu32(patT.data() + 12 * p + 4), first = rdu32(patT.data() + 12 * p + 8);
		seq.han2.patFlags = flags; seq.han2.firstFrame = first;
		if (cnt == 0) continue;
		if (first != (uint32_t)expectFirst) return fail("frames are not packed in pattern order (pattern " + std::to_string(p) + ")");
		if ((uint64_t)first + cnt > (uint64_t)nFrames) return fail("pattern " + std::to_string(p) + " runs past the frame section");
		expectFirst += (int)cnt;
		seq.han2.valid = true;
		seq.initialized = true; seq.empty = false;
		seq.frames.resize(cnt);
		for (uint32_t k = 0; k < cnt; k++) {
			Frame &F = seq.frames[k];
			Han2FrameRaw &R = F.han2;
			R.valid = true; R.frameSize = 300;
			memcpy(R.rec, frames.data() + 300 * (size_t)(first + k), 300);
			RboFrameRecord rec; memcpy(&rec, R.rec, sizeof(rec));
			if (rec.hasAttack && rec.attackRecordIdx >= 0) {
				if (rec.attackRecordIdx >= nAt) return fail("AT index out of range");
				memcpy(R.at, ats.data() + 120 * (size_t)rec.attackRecordIdx, 120);
				R.hadAT = true;
			}
			const uint32_t sli[2] = { rec.scriptListIndexA, rec.scriptListIndexB };
			for (int s = 0; s < 2; s++) {
				if (!sli[s]) continue;
				if ((int)sli[s] >= nSl[s]) return fail("script list index out of range");
				memcpy(R.script[s], f.sec[6 + s].data() + 20 * (size_t)sli[s], 20);
				R.scriptHad |= 1 << s;
			}
			for (int s = 0; s < kRboSlotCount; s++) {
				int bi = rd32(R.rec + kRboSlots[s].idxOffset);
				if (bi < 0) continue;
				if (bi >= nBox) continue;   // dangling index (EMO / SYSTEMEFFECT): no rectangle; the raw value is kept on save
				for (int j = 0; j < 4; j++) R.box[s][j] = rd16(boxes.data() + 8 * (size_t)bi + 2 * j);
				R.boxMask |= 1u << s;
				maxBox = std::max(maxBox, bi);
			}
			DecodeFrame(F, R);
		}
	}
	if (expectFirst != nFrames) return fail("pattern table does not cover every frame record");
	if (maxBox + 1 < nBox) cont->boxTail.assign(boxes.begin() + 8 * (size_t)(maxBox + 1), boxes.end());

	for (auto &s : fd.m_sequences) s.modified = false;
	return true;
}

// Follows the frame flow of one pattern the way Actor_AdvanceByAniFlag (RBO 0x43F640) does and returns the visited frames
// (one entry per frame entry, with the ticks spent). Stops at the end of the pattern, at a state-dependent ani flag, or at a cap.
void SimulateFlow(const Sequence &seq, std::vector<std::pair<int, int>> &visits, std::string &endNote)
{
	int frame = 0, counter = 0, total = 0;
	const int n = (int)seq.frames.size();
	for (int guard = 0; guard < 4000 && frame >= 0 && frame < n; guard++) {
		const Frame &f = seq.frames[frame];
		const uint8_t *r = f.han2.rec;
		int ani = f.han2.valid ? r[0x0B] : 1, jump = f.han2.valid ? r[0x0C] : 0, loopCnt = f.han2.valid ? r[0x12] : 0, loopEnd = f.han2.valid ? r[0x13] : 0;
		if (loopCnt) counter = loopCnt;
		int dur = f.AF.duration;
		visits.push_back({frame, dur});
		total += dur;
		if (total > 100000) { endNote = "cap"; return; }
		switch (ani) {
		case 0: endNote = "ends: jumps to pattern " + std::to_string(jump); return;
		case 1: case 3: frame++; break;
		case 2: case 4: frame = jump; break;
		case 5: if (counter) { counter--; frame = jump; } else frame = loopEnd; break;
		default: endNote = "ani flag " + std::to_string(ani) + " depends on runtime state"; return;
		}
		if (guard > 3000) { endNote = "does not terminate"; return; }
	}
	if (endNote.empty()) endNote = "runs off the end";
}


// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------
namespace {

struct Tables {
	std::vector<uint8_t> boxes, at, sl[2];
	int nBox = 0, nAt = 0, nSl[2] = { 1, 1 };   // script lists start with record 0
	std::map<int, int> boxMap, slMap[2];        // old identity -> new index
};

struct Ctx {
	std::vector<std::string> *warn;
	int pat, frame;
	void w(const std::string &m) { if (warn) warn->push_back("pattern " + std::to_string(pat) + " frame " + std::to_string(frame) + ": " + m); }
};

static inline uint8_t u8c(int v) { return (uint8_t)v; }

static void EncodeFrame(const Frame &F, Tables &T, uint8_t out[300], Ctx &cx)
{
	const Han2FrameRaw &R = F.han2;
	const bool raw = R.valid;
	if (raw) memcpy(out, R.rec, 300);
	else {
		memset(out, 0, 300);
		for (int s = 0; s < kRboSlotCount; s++) wr32(out + kRboSlots[s].idxOffset, 0xFFFFFFFFu);
		wr32(out + 0xC8, 0xFFFFFFFFu);
		out[0x0B] = ANI_NEXT; out[0x34] = STANCE_GROUND;
	}
	// what the original bytes decode to; only fields that differ from it are rewritten
	Han2FrameRaw base = R;
	if (!raw) { memcpy(base.rec, out, 300); base.valid = true; base.frameSize = 300; base.hadAT = false; base.boxMask = 0; }
	Frame ref; DecodeFrame(ref, base);
	RboFrameRecord *r = (RboFrameRecord *)out;

	Layer_Type L{}; if (!F.AF.layers.empty()) L = F.AF.layers[0];
	Layer_Type Lr = ref.AF.layers[0];
	if (F.AF.layers.size() > 1) cx.w("only layer 0 is stored (HAN2 has one sprite per frame)");
	int spr = L.usePat ? L.spriteId : (L.spriteId < 0 ? L.spriteId : L.spriteId + 10000);
	if (F.AF.layers.empty()) spr = -1;
	int sprRef = Lr.usePat ? Lr.spriteId : (Lr.spriteId < 0 ? Lr.spriteId : Lr.spriteId + 10000);
	if (spr != sprRef) r->spriteId = (int16_t)spr;
	const bool cgNew = spr >= 10000;
	if (L.offset_x != Lr.offset_x || cgNew != (sprRef >= 10000)) r->offsetX = (int16_t)(L.offset_x - (cgNew ? 128 : 0));
	if (L.offset_y != Lr.offset_y || cgNew != (sprRef >= 10000)) r->offsetY = (int16_t)(L.offset_y - (cgNew ? 224 : 0));
	if (F.AF.duration != ref.AF.duration) r->duration = (uint16_t)F.AF.duration;
	if (!(L.rotation[0] == Lr.rotation[0] && L.rotation[1] == Lr.rotation[1] && L.rotation[2] == Lr.rotation[2])) {
		int mode, rot; ha4::EncodeFlip(L.rotation, mode, rot);
		if (mode >= 8) cx.w("free rotation has no angle field in HAN2; written as flip mode 8/9 without angle");
		r->flipMode = (RboFlipMode)mode;
	}
	if (L.blend_mode != Lr.blend_mode) r->blendMode = (RboBlendMode)L.blend_mode;
	if (L.rgba[3] != Lr.rgba[3] || L.blend_mode != Lr.blend_mode)
		if (L.blend_mode) r->alpha = u8c((int)lroundf(std::clamp(L.rgba[3], 0.f, 1.f) * 255.f));
	if (F.AF.aniType != ref.AF.aniType || F.AF.aniFlag != ref.AF.aniFlag) {
		int a = 1;
		for (int i = 0; i < 6; i++) if (kAniMap[i][0] == F.AF.aniType && (unsigned)kAniMap[i][1] == F.AF.aniFlag) a = i;
		r->aniFlag = (RboAniFlag)a;
	}
	if (F.AF.jump != ref.AF.jump) r->jumpTarget = u8c(F.AF.jump);
	if (F.AF.landJump != ref.AF.landJump) r->landJumpTarget = u8c(F.AF.landJump);
	if (F.AF.priority != ref.AF.priority) r->drawPriorityCode = u8c(F.AF.priority);
	if (L.scale[0] != Lr.scale[0] || L.scale[1] != Lr.scale[1]) {
		if (L.scale[0] != L.scale[1]) cx.w("HAN2 has one zoom value; X scale is used");
		float z = L.scale[0];
		if (z == 1.f) r->fxFlags = (RboFrameFxFlags)(r->fxFlags & ~FXF_USE_ZOOM);
		else { r->fxFlags = (RboFrameFxFlags)(r->fxFlags | FXF_USE_ZOOM); r->zoom = (uint16_t)lroundf(z * 256.f); }
	}
	if (F.AF.interpolationType != ref.AF.interpolationType) r->interpolationMode = u8c(F.AF.interpolationType);
	if (F.AF.loopCount != ref.AF.loopCount) r->loopCount = u8c(F.AF.loopCount);
	if (F.AF.loopEnd != ref.AF.loopEnd) r->loopEndFrame = u8c(F.AF.loopEnd);

	const Frame_AS &S = F.AS, &Sr = ref.AS;
	if (S.movementFlags != Sr.movementFlags || S.speed[0] != Sr.speed[0] || S.speed[1] != Sr.speed[1] || S.accel[0] != Sr.accel[0] || S.accel[1] != Sr.accel[1]) {
		unsigned mf = 0;
		if (S.movementFlags & 0x10) mf |= MOVE_CLEAR_VEL_X | MOVE_ADD_X;
		else if (S.movementFlags & 0x20) mf |= MOVE_ADD_X;
		if (S.movementFlags & 0x01) mf |= MOVE_CLEAR_VEL_Y | MOVE_ADD_Y;
		else if (S.movementFlags & 0x02) mf |= MOVE_ADD_Y;
		// a Set with zero speed and accel is a plain clear
		if ((mf & MOVE_ADD_X) && (mf & MOVE_CLEAR_VEL_X) && !S.speed[0] && !S.accel[0]) mf &= ~MOVE_ADD_X;
		if ((mf & MOVE_ADD_Y) && (mf & MOVE_CLEAR_VEL_Y) && !S.speed[1] && !S.accel[1]) mf &= ~MOVE_ADD_Y;
		r->moveFlags = (RboMoveFlags)mf;
		r->speedX = (int16_t)S.speed[0]; r->speedY = (int16_t)S.speed[1];
		r->accelX = (int16_t)S.accel[0]; r->accelY = (int16_t)S.accel[1];
	}
	if (S.stanceState != Sr.stanceState) r->stanceClass = (RboStanceClass)S.stanceState;
	if (S.cancelNormal != Sr.cancelNormal) r->normalCancel = (RboCancelPermission)S.cancelNormal;
	if (S.cancelSpecial != Sr.cancelSpecial) r->specialCancel = (RboCancelPermission)S.cancelSpecial;
	if (S.hitsNumber != Sr.hitsNumber) r->hitLimitCount = (uint32_t)S.hitsNumber;

	// ---- boxes: slot k present when the model holds its key ----
	int present[16] = {}; // per group count
	bool attackPresent = false;
	int newIdx[24]; for (int s = 0; s < kRboSlotCount; s++) newIdx[s] = -1;
	for (int s = 0; s < kRboSlotCount; s++) {
		auto it = F.hitboxes.find(kRboSlots[s].key);
		if (it == F.hitboxes.end()) {
			// a dangling original index (points past the box table) has no rectangle in the model; keep it verbatim
			if (raw && !(R.boxMask & (1u << s))) { int o = rd32(R.rec + kRboSlots[s].idxOffset); if (o >= 0) newIdx[s] = o; }
			continue;
		}
		int16_t rc[4]; for (int j = 0; j < 4; j++) rc[j] = (int16_t)it->second.xy[j];
		int oldIdx = raw ? rd32(R.rec + kRboSlots[s].idxOffset) : -1;
		bool same = raw && (R.boxMask & (1u << s)) && oldIdx >= 0 && memcmp(rc, R.box[s], 8) == 0;
		int idx;
		if (same) {
			auto m = T.boxMap.find(oldIdx);
			if (m != T.boxMap.end()) idx = m->second;
			else { idx = T.nBox++; T.boxMap[oldIdx] = idx; T.boxes.insert(T.boxes.end(), (uint8_t *)rc, (uint8_t *)rc + 8); }
		} else { idx = T.nBox++; T.boxes.insert(T.boxes.end(), (uint8_t *)rc, (uint8_t *)rc + 8); }
		newIdx[s] = idx;
		if (kRboSlots[s].key >= 25) attackPresent = true;
	}
	for (int s = 0; s < kRboSlotCount; s++) if (newIdx[s] >= 0 && kRboSlots[s].key >= 25) attackPresent = true;
	for (int s = 0; s < kRboSlotCount; s++) wr32(out + kRboSlots[s].idxOffset, (uint32_t)newIdx[s]);
	{
		std::map<int, int> cnt;
		for (int s = 0; s < kRboSlotCount; s++) if (newIdx[s] >= 0) cnt[kRboSlots[s].groupOffset]++;
		static const int groups[] = { 0xD0, 0xE0, 0xF4, 0xFC, 0x10C, 0x120 };
		for (int g : groups) wr32(out + g, (uint32_t)cnt[g]);
	}

	// ---- AT: present iff an attack box exists (verified: hasAttack == attackBoxCount > 0 in every shipped frame) ----
	if (attackPresent) {
		uint8_t a[120];
		if (R.hadAT) memcpy(a, R.at, 120); else memset(a, 0, 120);
		RboAtRecord *ar = (RboAtRecord *)a;
		if (F.AT.damage != ref.AT.damage || !R.hadAT) ar->base_power = F.AT.damage;
		if (F.AT.guard_damage != ref.AT.guard_damage || !R.hadAT) ar->guard_damage = F.AT.guard_damage;
		if (F.AT.hitEffect != ref.AT.hitEffect) ar->hit_class = (RboHitClass)F.AT.hitEffect;
		r->hasAttack = 1;
		r->attackRecordIdx = T.nAt++;
		T.at.insert(T.at.end(), a, a + 120);
	} else { r->hasAttack = 0; r->attackRecordIdx = -1; }

	// ---- script lists (kept verbatim, shared by old identity) ----
	for (int s = 0; s < 2; s++) {
		uint32_t *dst = s == 0 ? &r->scriptListIndexA : &r->scriptListIndexB;
		int oldIdx = raw ? (int)rdu32(R.rec + (s == 0 ? 0xBC : 0xC0)) : 0;
		if (!(R.scriptHad & (1 << s)) || oldIdx == 0) { *dst = 0; continue; }
		auto m = T.slMap[s].find(oldIdx);
		if (m != T.slMap[s].end()) { *dst = (uint32_t)m->second; continue; }
		int idx = T.nSl[s]++;
		T.slMap[s][oldIdx] = idx;
		T.sl[s].insert(T.sl[s].end(), R.script[s], R.script[s] + 20);
		*dst = (uint32_t)idx;
	}
}

} // namespace

bool Serialize(const FrameData &fd, std::vector<uint8_t> &out, std::string *err, std::vector<std::string> *warnings, bool asDt2)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const Han2Container *cont = fd.m_han2.get();
	if (!cont) return fail("not a HAN2RBO character");
	for (size_t p = kPatterns; p < fd.m_sequences.size(); p++)
		if (!fd.m_sequences[p].frames.empty()) return fail("pattern " + std::to_string(p) + " has frames, but HAN2RBO has 256 pattern slots");

	Tables T;
	std::vector<uint8_t> patT(256 * 12, 0), frames;
	int total = 0;
	for (int p = 0; p < kPatterns; p++) {
		const Sequence *seq = p < (int)fd.m_sequences.size() ? &fd.m_sequences[p] : nullptr;
		uint32_t flags = seq ? seq->han2.patFlags : 0;
		uint32_t cnt = 0, first = seq ? seq->han2.firstFrame : 0;
		if (seq && !seq->frames.empty()) {
			cnt = (uint32_t)seq->frames.size();
			if (cnt > 255) return fail("pattern " + std::to_string(p) + " has more than 255 frames (frame numbers are bytes)");
			first = (uint32_t)total;
			size_t at = frames.size();
			frames.resize(at + 300 * (size_t)cnt);
			for (uint32_t k = 0; k < cnt; k++) {
				Ctx cx{warnings, p, (int)k};
				EncodeFrame(seq->frames[k], T, &frames[at + 300 * (size_t)k], cx);
			}
			total += (int)cnt;
		}
		wr32(patT.data() + 12 * p, cnt); wr32(patT.data() + 12 * p + 4, flags); wr32(patT.data() + 12 * p + 8, first);
	}

	Han2File f;
	memcpy(f.header, cont->header, 0x40);
	if (!asDt2 && cont->kind == 3 && cont->parts.empty() && cont->cg.empty())
		return fail("this character was loaded without its .DAT (no parts / CG): save it as .DT2, or open it together with its .DAT");
	f.sub = cont->sub; f.kind = asDt2 ? 3 : 0; f.xorFlag = 0;
	f.lead = cont->lead; f.tail = cont->tail;
	f.sec.assign(8, {});
	f.sec[0] = patT; f.sec[1] = frames;
	f.sec[2] = T.boxes; f.sec[2].insert(f.sec[2].end(), cont->boxTail.begin(), cont->boxTail.end());
	f.sec[3] = T.at;
	f.sec[4] = cont->sec[4]; f.sec[5] = cont->sec[5];
	for (int s = 0; s < 2; s++) {
		f.sec[6 + s] = cont->sec[6 + s];
		if (f.sec[6 + s].size() < 20) f.sec[6 + s].assign(20, 0);   // record 0
		f.sec[6 + s].insert(f.sec[6 + s].end(), T.sl[s].begin(), T.sl[s].end());
	}
	if (!asDt2) { f.area[kAreaParts] = cont->parts; f.area[kAreaCg] = cont->cg; f.area[kAreaNames] = cont->names; }
	for (int i = 0; i < 4; i++) f.areaOff[i] = cont->areaOff[i];
	// names: refresh edited pattern names
	if (!asDt2 && f.area[kAreaNames].size() >= (size_t)kNamesSize) {
		for (int p = 0; p < kPatterns && p < (int)fd.m_sequences.size(); p++) {
			const Sequence &seq = fd.m_sequences[p];
			uint8_t *dst = &f.area[kAreaNames][64 * p];
			if (seq.han2.nameValid) {
				char nbuf[65]{}; memcpy(nbuf, seq.han2.name, 64);
				if (sj2utf8(std::string(nbuf)) == seq.name) { memcpy(dst, seq.han2.name, 64); continue; }
			}
			std::string sj = utf82sj(seq.name);
			size_t n = std::min<size_t>(sj.size(), 63), i = 0;
			while (i < n) {
				unsigned char c = (unsigned char)sj[i];
				size_t w = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1;
				if (i + w > n) break;
				i += w;
			}
			memset(dst, 0, 64); memcpy(dst, sj.data(), i);
		}
	}
	std::string serr;
	if (!han2::Serialize(f, out, &serr)) return fail(serr);
	return true;
}

static std::string g_lastErr;
static std::vector<std::string> g_lastWarn;
const std::string &LastSaveError() { return g_lastErr; }
const std::vector<std::string> &LastSaveWarnings() { return g_lastWarn; }

bool SaveFile(const FrameData &fd, const char *filename, std::string *err, std::vector<std::string> *warnings, bool asDt2)
{
	g_lastErr.clear(); g_lastWarn.clear();
	std::vector<uint8_t> bytes;
	bool ok = Serialize(fd, bytes, &g_lastErr, &g_lastWarn, asDt2);
	if (warnings) *warnings = g_lastWarn;
	if (ok) {   // keep the first version of a file we replace
		std::error_code ec;
		std::filesystem::path p = std::filesystem::u8path(filename), bak = p; bak += ".bak";
		if (std::filesystem::exists(p, ec) && !std::filesystem::exists(bak, ec)) std::filesystem::copy_file(p, bak, ec);
	}
	if (ok && !WriteFileAtomic(filename, bytes.data(), bytes.size())) { g_lastErr = std::string("could not write ") + filename; ok = false; }
	if (err) *err = g_lastErr;
	return ok;
}

bool LoadFile(FrameData &fd, const char *filename, std::string *err)
{
	char *data; unsigned int size;
	if (!ReadInMem(filename, data, size)) { if (err) *err = "could not read file"; return false; }
	bool ok = Load(fd, (const uint8_t *)data, size, err);
	delete[] data;
	if (ok && fd.m_han2) fd.m_han2->sourcePath = filename;
	return ok;
}

} // namespace han2
