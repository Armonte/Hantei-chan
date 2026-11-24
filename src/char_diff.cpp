#include "char_diff.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace chardiff {

const char* KindName(Kind k)
{
	switch (k) {
	case Kind::Changed: return "changed";
	case Kind::Added: return "added";
	case Kind::Removed: return "removed";
	default: return "same";
	}
}

static const char* Id(const char* s) { return s; }

std::string AreaNames(uint32_t areas, const char* (*tr)(const char*))
{
	if (!tr) tr = Id;
	static const struct { uint32_t bit; const char* name; } kNames[] = {
		{aAF, "AF"}, {aAS, "AS"}, {aAT, "AT"}, {aEF, "EF"}, {aIF, "IF"}, {aBoxes, "boxes"},
		{aFrameCount, "frame count"}, {aHeader, "header"},
	};
	std::string s;
	for (const auto& n : kNames) {
		if (!(areas & n.bit)) continue;
		if (!s.empty()) s += ", ";
		s += tr(n.name);
	}
	return s;
}

bool PatternExists(const Sequence* s)
{
	return s && (!s->frames.empty() || !s->name.empty() || !s->codeName.empty());
}

const PatternDiff* Result::find(int pattern) const
{
	auto it = std::lower_bound(patterns.begin(), patterns.end(), pattern, [](const PatternDiff& d, int p) { return d.pattern < p; });
	return it != patterns.end() && it->pattern == pattern ? &*it : nullptr;
}

// ---- field visitor ---------------------------------------------------------------------------------------------------------------------------------
// One walk over a pair of structures serves both the fast check (out == null: only "any difference" per area) and the
// detail view (out: one row per differing field). Text is only built for fields that differ.
namespace {
struct Sink {
	std::vector<FieldDiff>* out = nullptr;
	uint32_t areas = 0;
	uint32_t area = 0;          // area of the fields being visited
	char prefix[32] = {};       // "L1 " for layer fields, "EF 2 " ...

	void row(const char* name, const std::string& a, const std::string& b)
	{
		areas |= area;
		if (!out) return;
		FieldDiff d; d.area = area; d.field = std::string(prefix) + name; d.mine = a; d.other = b;
		out->push_back(std::move(d));
	}
	static std::string num(long long v) { return std::to_string(v); }
	static std::string hex(unsigned long long v) { char b[32]; snprintf(b, sizeof b, "0x%llX", v); return b; }
	static std::string flt(double v) { char b[48]; snprintf(b, sizeof b, "%g", v); return b; }
	void I(const char* name, long long a, long long b) { if (a != b) row(name, num(a), num(b)); }
	void H(const char* name, unsigned long long a, unsigned long long b) { if (a != b) row(name, hex(a), hex(b)); }
	void F(const char* name, double a, double b) { if (a != b) row(name, flt(a), flt(b)); }
	void B(const char* name, bool a, bool b) { if (a != b) row(name, a ? "on" : "off", b ? "on" : "off"); }
	void S(const char* name, const std::string& a, const std::string& b) { if (a != b) row(name, "\"" + a + "\"", "\"" + b + "\""); }
	void Arr(const char* name, const int* a, const int* b, int n)
	{
		for (int i = 0; i < n; i++) { char nm[64]; snprintf(nm, sizeof nm, "%s[%d]", name, i); I(nm, a[i], b[i]); }
	}
	// Whether this sink only needs a yes / no for `bit` and already has it (skip the rest of the area).
	bool settled(uint32_t bit) const { return !out && (areas & bit); }
};

void VisitLayer(Sink& s, const Layer_Type& a, const Layer_Type& b)
{
	s.I("sprite", a.spriteId, b.spriteId);
	s.B("parts (usePat)", a.usePat, b.usePat);
	s.B("AFRT (layer)", a.afrt, b.afrt);
	s.I("offset X", a.offset_x, b.offset_x);
	s.I("offset Y", a.offset_y, b.offset_y);
	s.I("blend mode", a.blend_mode, b.blend_mode);
	static const char* rgba[] = {"colour R", "colour G", "colour B", "colour A"};
	for (int i = 0; i < 4; i++) s.F(rgba[i], a.rgba[i], b.rgba[i]);
	static const char* rot[] = {"rotation X", "rotation Y", "rotation Z"};
	for (int i = 0; i < 3; i++) s.F(rot[i], a.rotation[i], b.rotation[i]);
	s.F("scale X", a.scale[0], b.scale[0]);
	s.F("scale Y", a.scale[1], b.scale[1]);
	s.I("draw bucket (AFPL)", a.priority, b.priority);
}

void VisitAF(Sink& s, const Frame_AF& a, const Frame_AF& b)
{
	s.area = aAF;
	s.I("duration", a.duration, b.duration);
	s.I("animation type", a.aniType, b.aniType);
	s.H("animation flags", a.aniFlag, b.aniFlag);
	s.I("jump to", a.jump, b.jump);
	s.I("landing frame", a.landJump, b.landJump);
	s.I("interpolation", a.interpolationType, b.interpolationType);
	s.I("priority (AFPR)", a.priority, b.priority);
	s.I("loop count", a.loopCount, b.loopCount);
	s.I("loop end", a.loopEnd, b.loopEnd);
	s.B("AFRT", a.AFRT, b.AFRT);
	s.I("frame ID (AFID)", a.frameId, b.frameId);
	for (int i = 0; i < 4; i++) { char nm[32]; snprintf(nm, sizeof nm, "AFPA[%d]", i); s.I(nm, a.param[i], b.param[i]); }
	s.B("AFJH", a.afjh, b.afjh);
	s.I("layer count", (long long)a.layers.size(), (long long)b.layers.size());
	const size_t n = std::min(a.layers.size(), b.layers.size());
	for (size_t i = 0; i < n && !s.settled(aAF); i++) {
		snprintf(s.prefix, sizeof s.prefix, "L%zu ", i);
		VisitLayer(s, a.layers[i], b.layers[i]);
	}
	s.prefix[0] = 0;
}

void VisitAS(Sink& s, const Frame_AS& a, const Frame_AS& b)
{
	s.area = aAS;
	s.H("movement flags", a.movementFlags, b.movementFlags);
	s.I("speed X", a.speed[0], b.speed[0]);
	s.I("speed Y", a.speed[1], b.speed[1]);
	s.I("accel X", a.accel[0], b.accel[0]);
	s.I("accel Y", a.accel[1], b.accel[1]);
	s.I("max speed X", a.maxSpeedX, b.maxSpeedX);
	s.B("can move", a.canMove, b.canMove);
	s.I("stance", a.stanceState, b.stanceState);
	s.I("normal cancel", a.cancelNormal, b.cancelNormal);
	s.I("special cancel", a.cancelSpecial, b.cancelSpecial);
	s.I("counter hit", a.counterType, b.counterType);
	s.I("hits", a.hitsNumber, b.hitsNumber);
	s.I("invincibility", a.invincibility, b.invincibility);
	s.H("state flags", a.statusFlags[0], b.statusFlags[0]);
	s.H("state flags 2", a.statusFlags[1], b.statusFlags[1]);
	s.H("sine flags", a.sineFlags, b.sineFlags);
	s.Arr("sine", a.sineParameters, b.sineParameters, 4);
	s.F("sine phase X", a.sinePhases[0], b.sinePhases[0]);
	s.F("sine phase Y", a.sinePhases[1], b.sinePhases[1]);
	s.I("ASCF", a.ascf, b.ascf);
}

void VisitAT(Sink& s, const Frame_AT& a, const Frame_AT& b)
{
	s.area = aAT;
	s.H("guard flags", a.guard_flags, b.guard_flags);
	s.H("hit flags", a.otherFlags, b.otherFlags);
	s.I("correction", a.correction, b.correction);
	s.I("correction type", a.correction_type, b.correction_type);
	s.I("damage", a.damage, b.damage);
	s.I("red damage", a.red_damage, b.red_damage);
	s.I("guard damage", a.guard_damage, b.guard_damage);
	s.I("meter gain", a.meter_gain, b.meter_gain);
	static const char* sac[] = {"stand", "air", "crouch"};
	for (int i = 0; i < 3; i++) {
		char nm[64];
		snprintf(nm, sizeof nm, "guard vector (%s)", sac[i]); s.I(nm, a.guardVector[i], b.guardVector[i]);
		snprintf(nm, sizeof nm, "hit vector (%s)", sac[i]); s.I(nm, a.hitVector[i], b.hitVector[i]);
		snprintf(nm, sizeof nm, "guard vector flags (%s)", sac[i]); s.H(nm, a.gVFlags[i], b.gVFlags[i]);
		snprintf(nm, sizeof nm, "hit vector flags (%s)", sac[i]); s.H(nm, a.hVFlags[i], b.hVFlags[i]);
	}
	s.I("hit effect", a.hitEffect, b.hitEffect);
	s.I("sound effect", a.soundEffect, b.soundEffect);
	s.I("added effect", a.addedEffect, b.addedEffect);
	s.I("hit grab (ATNG)", a.hitgrab, b.hitgrab);
	s.F("extra gravity", a.extraGravity, b.extraGravity);
	s.I("circuit break time", a.breakTime, b.breakTime);
	s.I("untech time", a.untechTime, b.untechTime);
	s.I("hit stop time", a.hitStopTime, b.hitStopTime);
	s.I("hit stop", a.hitStop, b.hitStop);
	s.I("block stop time", a.blockStopTime, b.blockStopTime);
	s.I("damage proration (ATHH)", a.damageProration, b.damageProration);
	s.I("min damage (ATAM)", a.minDamage, b.minDamage);
	s.I("added hit stun (ATSA)", a.addHitStun, b.addHitStun);
	s.I("starter correction (ATSH)", a.starterCorrection, b.starterCorrection);
	s.Arr("ATC0", a.hitStunDecay, b.hitStunDecay, 3);
	s.I("ATS1..6", a.hitStopLegacy, b.hitStopLegacy);
	s.I("ATRF", a.atrf, b.atrf);
	s.I("ATBC", a.atbc, b.atbc);
	s.I("ATVD", a.atvd, b.atvd);
}

template<class V>
void VisitEF(Sink& s, const V& a, const V& b)
{
	s.area = aEF;
	s.I("EF count", (long long)a.size(), (long long)b.size());
	const size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n && !s.settled(aEF); i++) {
		snprintf(s.prefix, sizeof s.prefix, "EF %zu ", i);
		s.I("type", a[i].type, b[i].type);
		s.I("number", a[i].number, b[i].number);
		s.Arr("param", a[i].parameters, b[i].parameters, kMaxEfParams);
	}
	s.prefix[0] = 0;
}

template<class V>
void VisitIF(Sink& s, const V& a, const V& b)
{
	s.area = aIF;
	s.I("IF count", (long long)a.size(), (long long)b.size());
	const size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n && !s.settled(aIF); i++) {
		snprintf(s.prefix, sizeof s.prefix, "IF %zu ", i);
		s.I("type", a[i].type, b[i].type);
		s.Arr("param", a[i].parameters, b[i].parameters, kMaxIfParams);
	}
	s.prefix[0] = 0;
}

std::string BoxName(int i)
{
	if (i == 0) return "collision box";
	if (i >= 1 && i <= 8) return "hurtbox " + std::to_string(i);
	if (i == 11) return "clash box";
	if (i == 12) return "projectile box";
	if (i >= 9 && i <= 24) return "special box " + std::to_string(i - 8);
	return "attack box " + std::to_string(i - 24);
}
std::string BoxText(const Hitbox& h)
{
	char b[96]; snprintf(b, sizeof b, "%d, %d, %d, %d", h.xy[0], h.xy[1], h.xy[2], h.xy[3]); return b;
}

void VisitBoxes(Sink& s, const BoxList& a, const BoxList& b)
{
	s.area = aBoxes;
	auto ia = a.begin(), ib = b.begin();
	while ((ia != a.end() || ib != b.end()) && !s.settled(aBoxes)) {
		if (ib == b.end() || (ia != a.end() && ia->first < ib->first)) { s.row(BoxName(ia->first).c_str(), BoxText(ia->second), "-"); ++ia; }
		else if (ia == a.end() || ib->first < ia->first) { s.row(BoxName(ib->first).c_str(), "-", BoxText(ib->second)); ++ib; }
		else {
			if (memcmp(ia->second.xy, ib->second.xy, sizeof ia->second.xy) != 0) s.row(BoxName(ia->first).c_str(), BoxText(ia->second), BoxText(ib->second));
			++ia; ++ib;
		}
	}
}

void VisitFrame(Sink& s, const Frame& a, const Frame& b)
{
	VisitAF(s, a.AF, b.AF);
	VisitAS(s, a.AS, b.AS);
	VisitAT(s, a.AT, b.AT);
	VisitEF(s, a.EF, b.EF);
	VisitIF(s, a.IF, b.IF);
	VisitBoxes(s, a.hitboxes, b.hitboxes);
}

void VisitHeader(Sink& s, const Sequence& a, const Sequence& b)
{
	s.area = aHeader;
	s.S("name", a.name, b.name);
	s.S("code name", a.codeName, b.codeName);
	s.I("PSTS", a.psts, b.psts);
	s.I("PLVL", a.level, b.level);
	s.I("PFLG", a.flag, b.flag);
	s.I("PUPS", a.pups, b.pups);
}

const Sequence& EmptySeq() { static const Sequence e; return e; }
const Frame& EmptyFrame() { static const Frame e; return e; }
} // namespace

bool ComparePattern(const Sequence* mine, const Sequence* other, int index, PatternDiff* out)
{
	const bool hasA = PatternExists(mine), hasB = PatternExists(other);
	PatternDiff d;
	d.pattern = index;
	d.framesMine = mine ? (int)mine->frames.size() : 0;
	d.framesOther = other ? (int)other->frames.size() : 0;
	if (!hasA && !hasB) { if (out) *out = d; return false; }
	const Sequence& a = hasA ? *mine : EmptySeq();
	const Sequence& b = hasB ? *other : EmptySeq();
	Sink hs;
	VisitHeader(hs, a, b);
	d.areas = hs.areas;
	const int common = std::min(d.framesMine, d.framesOther), most = std::max(d.framesMine, d.framesOther);
	for (int f = 0; f < most; f++) {
		FrameDiff fd; fd.frame = f;
		if (f >= common) fd.areas = aFrameCount;
		else {
			Sink s;
			VisitFrame(s, a.frames[f], b.frames[f]);
			fd.areas = s.areas;
		}
		if (fd.areas) { d.areas |= fd.areas; d.frames.push_back(fd); }
	}
	if (d.framesMine != d.framesOther) d.areas |= aFrameCount;
	if (!hasB) d.kind = Kind::Added;
	else if (!hasA) d.kind = Kind::Removed;
	else d.kind = d.areas ? Kind::Changed : Kind::Same;
	if (out) *out = std::move(d);
	return hasA != hasB || (out ? out->areas != 0 : d.areas != 0);
}

Result Compare(const std::vector<Sequence>& mine, const std::vector<Sequence>& other)
{
	const auto t0 = std::chrono::steady_clock::now();
	Result r;
	const size_t n = std::max(mine.size(), other.size());
	r.kindOf.assign(n, Kind::Same);
	for (size_t i = 0; i < n; i++) {
		PatternDiff d;
		if (!ComparePattern(i < mine.size() ? &mine[i] : nullptr, i < other.size() ? &other[i] : nullptr, (int)i, &d)) continue;
		r.kindOf[i] = d.kind;
		if (d.kind == Kind::Changed) r.changed++;
		else if (d.kind == Kind::Added) r.added++;
		else if (d.kind == Kind::Removed) r.removed++;
		r.framesDiffering += (int)d.frames.size();
		r.patterns.push_back(std::move(d));
	}
	r.valid = true;
	r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	return r;
}

std::string Describe(const PatternDiff& d, const char* (*tr)(const char*))
{
	if (!tr) tr = Id;
	char b[160];
	if (d.kind == Kind::Added) { snprintf(b, sizeof b, tr("only on this side (%d frames)"), d.framesMine); return b; }
	if (d.kind == Kind::Removed) { snprintf(b, sizeof b, tr("only on the other side (%d frames)"), d.framesOther); return b; }
	std::string s;
	uint32_t frameAreas = 0;
	int changedFrames = 0;
	for (const auto& f : d.frames) if (f.areas & ~aFrameCount) { frameAreas |= f.areas & ~aFrameCount; changedFrames++; }
	if (changedFrames) {
		snprintf(b, sizeof b, changedFrames == 1 ? tr("%d frame: ") : tr("%d frames: "), changedFrames);
		s = b + AreaNames(frameAreas, tr);
	}
	if (d.areas & aFrameCount) {
		if (!s.empty()) s += "; ";
		snprintf(b, sizeof b, tr("frame count %d -> %d"), d.framesOther, d.framesMine);
		s += b;
	}
	if (d.areas & aHeader) {
		if (!s.empty()) s += "; ";
		s += tr("header");
	}
	return s;
}

std::vector<FieldDiff> HeaderFields(const Sequence* mine, const Sequence* other)
{
	std::vector<FieldDiff> out;
	Sink s; s.out = &out;
	VisitHeader(s, mine ? *mine : EmptySeq(), other ? *other : EmptySeq());
	return out;
}

std::vector<FieldDiff> FrameFields(const Frame* mine, const Frame* other)
{
	std::vector<FieldDiff> out;
	Sink s; s.out = &out;
	VisitFrame(s, mine ? *mine : EmptyFrame(), other ? *other : EmptyFrame());
	return out;
}

bool KindColor(Kind k, bool light, float c[4])
{
	static const float dark[3][3] = {{1.0f, 0.78f, 0.25f}, {0.45f, 0.9f, 0.45f}, {1.0f, 0.45f, 0.45f}};
	static const float lite[3][3] = {{0.78f, 0.42f, 0.0f}, {0.08f, 0.5f, 0.08f}, {0.8f, 0.1f, 0.1f}};
	int i;
	switch (k) {
	case Kind::Changed: i = 0; break;
	case Kind::Added: i = 1; break;
	case Kind::Removed: i = 2; break;
	default: return false;
	}
	const float* s = light ? lite[i] : dark[i];
	c[0] = s[0]; c[1] = s[1]; c[2] = s[2]; c[3] = 1.0f;
	return true;
}

Kind ListMarks::kindFor(const FrameData* fd, int pattern) const
{
	if (!active(fd) || pattern < 0 || pattern >= (int)kinds->size()) return Kind::Same;
	const Kind k = (*kinds)[pattern];
	if (fd == other && fd != mine) {
		if (k == Kind::Added) return Kind::Removed;
		if (k == Kind::Removed) return Kind::Added;
	}
	return k;
}

ListMarks& Marks()
{
	static ListMarks m;
	return m;
}

} // namespace chardiff
