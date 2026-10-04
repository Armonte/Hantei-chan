#include "preview_sim.h"

#include "framedata.h"
#include "mbaacc_transform.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

// Engine references (MBAA.exe, IDA session 212fe94c). Full evidence and
// decompiler excerpts: docs/HANTEI_PREVIEW_SIM.md.
//   Character_AdvanceFrameByAniFlag 0x461150  frame advance / loops / end
//   Character_InitializeFrame       0x45F450  every frame entry -> EF dispatch
//   Effect_InitializeComplex        0x454900  spawn position / facing / links
//   Effect1_SpawnPattern            0x454D90  EF 1/101/1000
//   Effect8_SpawnEffectHa6Actor     0x4551B0  EF 8/108
//   Effect11_SpawnRandomPatterns    0x454E30  EF 11/111
//   Cond_Dispatch                   0x469599  IF semantics
//   Actor_RunIf2DestroyOnHitPass    0x469100  IF2 p3 = destroy at pattern end
//   Camera_GetScreenBoundary        0x44C300  screen edges

namespace preview {

// ---------------------------------------------------------------------------
// Small value helpers
// ---------------------------------------------------------------------------

bool IfKey::operator<(const IfKey& o) const
{
	if (effectHa6 != o.effectHa6) return effectHa6 < o.effectHa6;
	if (pattern != o.pattern) return pattern < o.pattern;
	if (frame != o.frame) return frame < o.frame;
	return index < o.index;
}

bool IfKey::operator==(const IfKey& o) const
{
	return effectHa6 == o.effectHa6 && pattern == o.pattern && frame == o.frame && index == o.index;
}

bool ScheduledSpawn::operator==(const ScheduledSpawn& o) const
{
	return tick == o.tick && patternId == o.patternId && effectHa6 == o.effectHa6 &&
	       offsetX == o.offsetX && offsetY == o.offsetY && parentIndex == o.parentIndex &&
	       isMarker == o.isMarker;
}

bool Options::operator==(const Options& o) const
{
	return rootFacingLeft == o.rootFacingLeft && ownerSlotOdd == o.ownerSlotOdd &&
	       opponentX == o.opponentX && cameraX == o.cameraX && cameraY == o.cameraY &&
	       cameraZoom == o.cameraZoom && defaultIfAssumption == o.defaultIfAssumption &&
	       ifOverrides == o.ifOverrides && rngSeed == o.rngSeed &&
	       horizonTicks == o.horizonTicks && checkpointInterval == o.checkpointInterval &&
	       maxActors == o.maxActors && patOwnerHalfScale == o.patOwnerHalfScale;
}

glm::mat4 SimActor::transform() const
{
	glm::mat4 t = glm::translate(glm::mat4(1.f), glm::vec3(x, y, 0.f));
	return t * MbaaTransform::ActorMatrix(angleTurns(), facingLeft);
}

const SimActor* TickState::find(int id) const
{
	for (const auto& a : actors)
		if (a.id == id) return &a;
	return nullptr;
}

const SimActor* TickState::root() const
{
	for (const auto& a : actors)
		if (a.isRoot) return &a;
	return nullptr;
}

const char* EventKindName(EventKind k)
{
	switch (k) {
	case EventKind::Spawn: return "spawn";
	case EventKind::Despawn: return "despawn";
	case EventKind::RootEnded: return "root-ended";
	case EventKind::PatternChange: return "pattern";
	case EventKind::IfAssumedTrue: return "if-true";
	case EventKind::IfUnmodeled: return "if-unmodeled";
	case EventKind::Limit: return "limit";
	}
	return "?";
}

// ---------------------------------------------------------------------------
// IF classification (Cond_Dispatch 0x469599). Record = {type, p1..p9};
// Frame_IF::parameters[0] is p1.
// ---------------------------------------------------------------------------

namespace {

enum class IfAct : uint8_t { None, Frame, Queue, Destroy };

struct IfBranch {
	IfAct act = IfAct::None;
	int target = 0;
	bool runtime = true; // depends on state the preview does not model
};

// Branch taken when a *runtime* condition holds. Native (deterministic) IFs
// are handled separately in EvalNativeIf.
IfBranch ClassifyRuntimeIf(const Frame_IF& c, int curFrame)
{
	const int* p = c.parameters;
	IfBranch b;
	auto frameOrQueue = [&](int t) {
		// ">= 10000 queues pattern t-10000, otherwise jump to frame t"
		if (t >= 10000) { b.act = IfAct::Queue; b.target = t - 10000; }
		else { b.act = IfAct::Frame; b.target = t; }
	};
	switch (c.type) {
	case 1: {
		// IF1 is inverted: p2 < 10000 queues a pattern, >= 10000 jumps.
		int t = p[1];
		if (t >= 10000) { b.act = IfAct::Frame; b.target = t - 10000; }
		else { b.act = IfAct::Queue; b.target = t; }
		break;
	}
	case 2: // p1 bits 1/2 = off-screen (native), p2 = landed (runtime)
		if (p[1]) { b.act = IfAct::Destroy; }
		break;
	case 3: case 14: case 25:
		frameOrQueue(p[0]);
		break;
	case 4: case 5: case 15: case 17: case 20: case 21: case 22: case 24:
	case 28: case 29: case 30: case 33: case 36: case 39: case 40: case 41:
	case 42: case 50: case 52: case 54: case 60: case 150:
		if (p[0] >= 0) { b.act = IfAct::Frame; b.target = p[0]; }
		break;
	case 6:
		b.act = IfAct::Frame;
		b.target = p[2] + (p[4] ? curFrame : 0);
		break;
	case 7:
		b.act = IfAct::Queue; b.target = p[2];
		break;
	case 8:
		if (p[2] == 0) { b.act = IfAct::Frame; b.target = p[0]; }
		else { b.act = IfAct::Queue; b.target = p[0]; }
		break;
	case 12:
		if (p[2] & 2) { b.act = IfAct::Queue; b.target = p[0]; }
		else { b.act = IfAct::Frame; b.target = p[0]; }
		break;
	case 13:
		if (p[0] != -1) {
			if (p[1]) { b.act = IfAct::Queue; b.target = p[0]; }
			else { b.act = IfAct::Frame; b.target = p[0]; }
		}
		break;
	case 18: // parent pattern check; p2 == 256 uses a runtime flag
		if (p[1] == 256) { b.act = IfAct::Frame; b.target = p[0]; }
		break;
	case 27:
		if (p[1]) { b.act = IfAct::Frame; b.target = p[0]; }
		else { b.act = IfAct::Queue; b.target = p[0]; }
		break;
	case 53:
		b.act = IfAct::Destroy;
		break;
	default:
		// 11, 16, 19, 26, 31, 34, 35, 38, 70, 100, 151 ...: no simple branch
		// (commands, velocity, variables, reflection) -> not modeled.
		break;
	}
	return b;
}

bool IsNativeIf(const Frame_IF& c)
{
	switch (c.type) {
	case 9: case 10: case 37: case 55: return true;
	case 18: return c.parameters[1] != 256;
	default: return false;
	}
}

uint32_t NextRand(uint32_t& s)
{
	// xorshift32: deterministic, cheap, good enough for preview randomness.
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return s;
}

int RandMod(uint32_t& s, int m)
{
	if (m <= 1) return 0;
	return (int)(NextRand(s) % (uint32_t)m);
}

inline uint64_t Fnv(uint64_t h, const void* data, size_t n)
{
	const unsigned char* p = (const unsigned char*)data;
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
	return h;
}

} // namespace

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

struct PreviewSim::State {
	int tick = -1;
	std::vector<SimActor> actors;
	uint32_t rng = 1;
	int nextId = 0;
	std::vector<std::pair<int, int>> onceGuards; // EF1000 (spawner id, var)
};

struct PreviewSim::Impl {
	bool valid = false;
	State frontier;
	std::vector<State> checkpoints; // checkpoints[k].tick == k * interval
	static constexpr int kCursors = 4;
	State cursors[kCursors];
	uint64_t cursorUse[kCursors] = {};
	uint64_t useClock = 0;

	uint64_t mainVersion = 0, effectVersion = 0;
	std::set<std::pair<bool, int>> touched;
	uint64_t fingerprint = 0;

	std::vector<int> rootTrack;   // root frame at each frontier tick
	int rootEnd = -1;
	int settled = -1;             // first tick with only a finished root left
	bool rootValid = false;
	std::set<IfKey> reportedUnmodeled;

	// Per-step context
	bool recording = false;
	PreviewSim* owner = nullptr;
};

// The stepping engine. Kept as free functions over (sim, state) so the same
// code path serves frontier extension and checkpoint replay.
namespace {

struct Ctx {
	FrameData* main;
	FrameData* effect;
	const Options* opt;
	const std::vector<ScheduledSpawn>* sched;
	std::set<std::pair<bool, int>>* touched;
	bool recording;
	std::vector<SpawnRecord>* records;
	std::vector<SimEvent>* events;
	std::set<IfKey>* reportedUnmodeled;
};

constexpr size_t kMaxEvents = 20000;

Sequence* GetSeq(Ctx& c, bool effectHa6, int pattern)
{
	FrameData* d = effectHa6 ? c.effect : c.main;
	if (!d) return nullptr;
	Sequence* s = d->get_sequence(pattern);
	if (!s || s->frames.empty()) return nullptr;
	if (c.recording) c.touched->insert({effectHa6, pattern});
	return s;
}

void Event(Ctx& c, int tick, EventKind k, int actorId, int a = 0, int b = 0,
           const char* detail = nullptr, const IfKey* key = nullptr, int ifType = 0)
{
	if (!c.recording || c.events->size() >= kMaxEvents) return;
	SimEvent e;
	e.tick = tick; e.kind = k; e.actorId = actorId; e.a = a; e.b = b;
	if (detail) e.detail = detail;
	if (key) e.ifKey = *key;
	e.ifType = ifType;
	c.events->push_back(std::move(e));
}

SimActor* FindActor(std::vector<SimActor>& v, int id)
{
	for (auto& a : v) if (a.id == id) return &a;
	return nullptr;
}

void Kill(Ctx& c, std::vector<SimActor>& actors, int id, int tick, const char* why);

void EnterFrame(Ctx& c, PreviewSim::State& s, size_t idx, int frame, int tick, int depthGuard);

// Destroy children linked by `mask` (engine link flags) to `parentId`.
void KillLinkedChildren(Ctx& c, std::vector<SimActor>& actors, int parentId, int mask, int tick, const char* why)
{
	std::vector<int> ids;
	for (const auto& a : actors)
		if (a.parentId == parentId && a.id != parentId && (a.linkFlags & mask)) ids.push_back(a.id);
	for (int id : ids) Kill(c, actors, id, tick, why);
}

void Kill(Ctx& c, std::vector<SimActor>& actors, int id, int tick, const char* why)
{
	SimActor* a = FindActor(actors, id);
	if (!a || a->isRoot || a->frame < 0) return;
	a->frame = -1; // mark dead; swept at the end of the tick
	if (c.recording && id >= 0 && id < (int)c.records->size() && (*c.records)[id].deathTick < 0)
		(*c.records)[id].deathTick = tick;
	Event(c, tick, EventKind::Despawn, id, 0, 0, why);
	// Effect_DetachChildrenOnParentDestroy 0x453A00: link 0x21 children die.
	KillLinkedChildren(c, actors, id, 0x21, tick, "parent destroyed (link 0x21)");
}

float OwnerOffsetScale(Ctx& c, const SimActor& spawner)
{
	// Effect_InitializeComplex: offsets are <<7 when the spawner's current
	// frame record's first word (AFGP[0], "usePat") is non-zero, else <<8.
	// In CG-pixel units (256 per px) that is 0.5 vs 1.0.
	if (!c.opt->patOwnerHalfScale) return 1.f;
	Sequence* s = GetSeq(c, spawner.effectHa6, spawner.pattern);
	if (!s || spawner.frame < 0 || spawner.frame >= (int)s->frames.size()) return 1.f;
	const auto& af = s->frames[spawner.frame].AF;
	bool usePat = !af.layers.empty() && af.layers[0].usePat;
	return usePat ? 0.5f : 1.f;
}

// Position / facing / angle per Effect_InitializeComplex 0x454900.
void PlaceChild(const Options& o, const SimActor& parent, SimActor& ch,
                int ox, int oy, int efAngle, float scale)
{
	const int fs1 = ch.flagset1, fs2 = ch.flagset2;
	bool facing = parent.facingLeft;               // +784 copied from parent
	if (fs2 & 0x800) facing = o.ownerSlotOdd;      // +752 bit 0
	if (fs1 & 0x800) facing = !facing;             // toggle

	ch.angle = efAngle + ((fs1 & 0x100) ? parent.angle : 0);

	if (fs2 & 0x200) {                             // fixed: ((X-128)<<8, Y<<8)
		ch.x = (float)(ox - 128);
		ch.y = (float)oy;
		ch.facingLeft = false;
		return;
	}

	float dx = ox * scale, dy = oy * scale;
	if (facing) dx = -dx;                          // own X only
	if (fs2 & 0x100) dx += o.opponentX - parent.x; // signed distance to opponent

	const float zoom = o.cameraZoom > 0.f ? o.cameraZoom : 1.f;
	const float halfW = 160.f / zoom;              // 40960 / 256
	const float leftEdge = o.cameraX - halfW + 8.f;
	const float rightEdge = o.cameraX + halfW - 8.f;

	if (fs1 & 0x400) {
		ch.x = (facing ? leftEdge : rightEdge) + dx;
		ch.y = parent.y + dy;
	} else if (fs1 & 0x200) {
		ch.x = (!facing ? leftEdge : rightEdge) + dx;
		ch.y = parent.y + dy;
	} else if (!(fs1 & 0x10)) {
		ch.x = parent.x + dx;
		ch.y = parent.y + dy;
	} else if (fs2 >= 0) {                         // camera-relative
		ch.x = o.cameraX + dx;
		ch.y = o.cameraY + dy;
	} else {                                       // screen-absolute
		ch.x = dx;
		ch.y = dy;
	}
	if (fs1 < 0) ch.y = (float)oy;                 // absolute Y (<<8)
	if (fs1 & 0x2) facing = false;                 // always face right
	ch.facingLeft = facing;
}

int LinkFlagsFrom(int fs1, int fs2)
{
	int link = fs1 & (0x1 | 0x4 | 0x8 | 0x20 | 0x40);
	if (fs2 & 0x2) link |= 0x20; // "advance with parent" also sets link 0x20
	return link;
}

// Spawn one actor. Returns the new actor index or -1.
int SpawnActor(Ctx& c, PreviewSim::State& s, size_t parentIdx, int tick, int depthGuard,
               int efType, int pattern, bool effectHa6, int ox, int oy, int fs1, int fs2,
               int efAngle, int efIndex, int subIndex, bool preset, int scheduleIndex)
{
	if ((int)s.actors.size() >= c.opt->maxActors) {
		Event(c, tick, EventKind::Limit, -1, 0, 0, "maxActors reached; spawn dropped");
		return -1;
	}
	const SimActor parentCopy = s.actors[parentIdx];
	if (!preset && !GetSeq(c, effectHa6, pattern)) {
		Event(c, tick, EventKind::Limit, parentCopy.id, pattern, effectHa6,
		      "spawn target pattern missing or empty; skipped");
		return -1;
	}

	SimActor ch;
	ch.id = s.nextId++;
	ch.parentId = parentCopy.id;
	ch.depth = parentCopy.depth + 1;
	ch.isPreset = preset;
	ch.isScript = scheduleIndex >= 0;
	ch.pattern = pattern;
	ch.effectHa6 = effectHa6;
	ch.spawnTick = tick;
	ch.effectType = efType;
	ch.flagset1 = fs1;
	ch.flagset2 = fs2;
	ch.linkFlags = LinkFlagsFrom(fs1, fs2);
	ch.zPriority = (fs2 & 0x400) ? 460 : 0;
	ch.srcEffectHa6 = parentCopy.effectHa6;
	ch.srcPattern = parentCopy.pattern;
	ch.srcFrame = parentCopy.frame;
	ch.srcEffectIndex = efIndex;
	ch.srcSubIndex = subIndex;
	ch.scheduleIndex = scheduleIndex;
	ch.offsetScale = scheduleIndex >= 0 ? 1.f : OwnerOffsetScale(c, parentCopy);
	PlaceChild(*c.opt, parentCopy, ch, ox, oy, efAngle, ch.offsetScale);
	ch.frame = 0;
	ch.frameEnterTick = tick;

	s.actors.push_back(ch);
	size_t idx = s.actors.size() - 1;

	if (c.recording) {
		SpawnRecord r;
		r.id = ch.id; r.parentId = ch.parentId; r.depth = ch.depth;
		r.pattern = ch.pattern; r.effectHa6 = ch.effectHa6; r.isPreset = ch.isPreset;
		r.isScript = ch.isScript; r.effectType = efType;
		r.srcEffectHa6 = ch.srcEffectHa6; r.srcPattern = ch.srcPattern;
		r.srcFrame = ch.srcFrame; r.srcEffectIndex = efIndex; r.srcSubIndex = subIndex;
		r.scheduleIndex = scheduleIndex;
		r.spawnTick = tick; r.x = ch.x; r.y = ch.y; r.facingLeft = ch.facingLeft;
		if ((int)c.records->size() <= ch.id) c.records->resize(ch.id + 1);
		(*c.records)[ch.id] = r;
	}
	Event(c, tick, EventKind::Spawn, ch.id, pattern, effectHa6);

	if (!preset)
		EnterFrame(c, s, idx, 0, tick, depthGuard + 1); // fires frame-0 EFs now
	return (int)idx;
}

// Effect_DispatchFrameEFs for the spawn-type EFs of the actor's current frame.
void DispatchEFs(Ctx& c, PreviewSim::State& s, size_t idx, int tick, int depthGuard)
{
	if (depthGuard > 48) {
		Event(c, tick, EventKind::Limit, s.actors[idx].id, 0, 0, "spawn recursion depth guard");
		return;
	}
	const SimActor a = s.actors[idx];
	Sequence* seq = GetSeq(c, a.effectHa6, a.pattern);
	if (!seq || a.frame < 0 || a.frame >= (int)seq->frames.size()) return;
	// Copy: spawning may not touch frame data, but keep the loop robust.
	const std::vector<Frame_EF> efs(seq->frames[a.frame].EF.begin(), seq->frames[a.frame].EF.end());

	for (size_t i = 0; i < efs.size(); i++) {
		const Frame_EF& ef = efs[i];
		const int* p = ef.parameters;
		const int t = ef.type;
		const bool relative = t > 100 && t < 1000;
		switch (t) {
		case 1: case 101: case 1000: {
			if (t == 1000) {
				// Spawn once per spawner var (word +0x1C0 + 2*p6).
				auto key = std::make_pair(a.id, p[5]);
				if (std::find(s.onceGuards.begin(), s.onceGuards.end(), key) != s.onceGuards.end())
					break;
				s.onceGuards.push_back(key);
			}
			int pat = ef.number + (relative ? a.pattern : 0);
			SpawnActor(c, s, idx, tick, depthGuard, t, pat, a.effectHa6, p[0], p[1], p[2], p[3],
			           p[7], (int)i, 0, false, -1);
			break;
		}
		case 8: case 108: {
			int pat = ef.number + (relative ? a.pattern : 0);
			SpawnActor(c, s, idx, tick, depthGuard, t, pat, true, p[0], p[1], p[2], p[3],
			           p[7], (int)i, 0, false, -1);
			break;
		}
		case 11: case 111: {
			// p1..p4 rectangle (p4 == 30000: circle of radius rand(p3)),
			// p5 pattern range, p6 count (0 = each pattern of the range once),
			// p7/p8 flagsets, p9 angle + rand(p10), p11 projectile var.
			int range = p[4] ? p[4] : 1;
			int count = p[5] ? p[5] : range;
			if (count > 256) count = 256;
			int base = ef.number + (relative ? a.pattern : 0);
			for (int k = 0; k < count; k++) {
				int pat = p[5] == 0 ? base + k : base + RandMod(s.rng, range);
				int ox = p[0], oy = p[1];
				if (p[3] == 30000) {
					int r = RandMod(s.rng, std::max(p[2], 1));
					int deg = RandMod(s.rng, 360);
					double rad = deg * 3.14159265358979323846 / 180.0;
					ox += (int)std::lround(r * std::cos(rad));
					oy += (int)std::lround(r * std::sin(rad));
				} else {
					ox += RandMod(s.rng, std::max(p[2], 1));
					oy += RandMod(s.rng, std::max(p[3], 1));
				}
				int ang = p[8] + (p[9] ? RandMod(s.rng, p[9]) : 0);
				// SpawnActor may reallocate s.actors; idx stays valid.
				SpawnActor(c, s, idx, tick, depthGuard, t, pat, a.effectHa6, ox, oy, p[6], p[7],
				           ang, (int)i, k, false, -1);
			}
			break;
		}
		case 3: {
			// Preset procedural effect: a one-tick marker at (p1,p2).
			SpawnActor(c, s, idx, tick, depthGuard, t, ef.number, false, p[0], p[1], 0, 0,
			           0, (int)i, 0, true, -1);
			break;
		}
		default:
			break;
		}
	}
}

// Character_InitializeFrame: enter a frame (also on a self-jump) and fire EFs.
void EnterFrame(Ctx& c, PreviewSim::State& s, size_t idx, int frame, int tick, int depthGuard)
{
	SimActor& a = s.actors[idx];
	Sequence* seq = GetSeq(c, a.effectHa6, a.pattern);
	a.frame = frame;
	a.frameTimer = 0;
	a.frameEnterTick = tick;
	a.visitSerial++;
	a.ifFiredThisVisit = false;
	if (!seq || frame < 0 || frame >= (int)seq->frames.size()) return;
	const auto& af = seq->frames[frame].AF;
	if (af.loopCount) a.loopCounter = (uint8_t)af.loopCount;   // AF +20
	if (af.priority) a.zPriority = af.priority;               // AFPR only if non-zero
	DispatchEFs(c, s, idx, tick, depthGuard);
}

// Root finished or would leave its pattern: hold the last frame.
void EndRoot(Ctx& c, PreviewSim::State& s, size_t idx, int tick, int queued, const char* why)
{
	SimActor& a = s.actors[idx];
	if (a.rootEnded) return;
	a.rootEnded = true;
	Event(c, tick, EventKind::RootEnded, a.id, queued, 0, why);
	// The character really starts another pattern here: children linked with
	// 0x20 ("remove when parent changes pattern") die with it.
	KillLinkedChildren(c, s.actors, a.id, 0x20, tick, "parent changed pattern (link 0x20)");
}

// Character_StartQueuedPattern: new pattern, frame 0.
void StartPattern(Ctx& c, PreviewSim::State& s, size_t idx, int pattern, int tick, int depthGuard)
{
	SimActor& a = s.actors[idx];
	a.queuedPattern = -1;
	if (a.isRoot) {
		EndRoot(c, s, idx, tick, pattern, "root queued another pattern");
		return;
	}
	KillLinkedChildren(c, s.actors, a.id, 0x20, tick, "parent changed pattern (link 0x20)");
	if (pattern == 0 || !GetSeq(c, s.actors[idx].effectHa6, pattern)) {
		Kill(c, s.actors, s.actors[idx].id, tick,
		     pattern == 0 ? "pattern end -> pattern 0 (engine restarts pattern 0; preview despawns)"
		                  : "queued pattern missing");
		return;
	}
	s.actors[idx].pattern = pattern;
	Event(c, tick, EventKind::PatternChange, s.actors[idx].id, 0, pattern);
	EnterFrame(c, s, idx, 0, tick, depthGuard);
}

// Frame index out of range after an advance/jump (Actor_CheckFrameIndexValid
// -2 resets to pattern 0 frame 0).
void RanOffEnd(Ctx& c, PreviewSim::State& s, size_t idx, int tick)
{
	if (s.actors[idx].isRoot) {
		// Keep a displayable frame.
		Sequence* seq = GetSeq(c, false, s.actors[idx].pattern);
		if (seq) s.actors[idx].frame = std::clamp(s.actors[idx].frame, 0, (int)seq->frames.size() - 1);
		EndRoot(c, s, idx, tick, 0, "frame index past the end (engine resets to pattern 0)");
		return;
	}
	Kill(c, s.actors, s.actors[idx].id, tick, "frame index past the end (engine resets to pattern 0)");
}

void JumpToFrame(Ctx& c, PreviewSim::State& s, size_t idx, int frame, int tick)
{
	Sequence* seq = GetSeq(c, s.actors[idx].effectHa6, s.actors[idx].pattern);
	if (!seq || frame < 0 || frame >= (int)seq->frames.size()) {
		s.actors[idx].frame = frame;
		RanOffEnd(c, s, idx, tick);
		return;
	}
	EnterFrame(c, s, idx, frame, tick, 0);
}

IfAssume ResolveAssumption(const Options& o, const IfKey& k)
{
	auto it = o.ifOverrides.find(k);
	IfAssume v = it != o.ifOverrides.end() ? it->second : IfAssume::Default;
	if (v == IfAssume::Default) v = o.defaultIfAssumption;
	if (v == IfAssume::Default) v = IfAssume::False;
	return v;
}

// Phase A: condition pass (runs before the character update each tick).
void RunConditions(Ctx& c, PreviewSim::State& s, size_t idx, int tick)
{
	SimActor a = s.actors[idx];
	if (a.frame < 0 || a.isPreset || (a.isRoot && a.rootEnded)) return;
	Sequence* seq = GetSeq(c, a.effectHa6, a.pattern);
	if (!seq || a.frame >= (int)seq->frames.size()) return;
	const std::vector<Frame_IF> ifs(seq->frames[a.frame].IF.begin(), seq->frames[a.frame].IF.end());
	const uint32_t visit = a.visitSerial;

	for (size_t i = 0; i < ifs.size(); i++) {
		SimActor& cur = s.actors[idx];
		if (cur.frame < 0 || cur.visitSerial != visit) return; // destroyed / jumped
		const Frame_IF& cond = ifs[i];
		const int* p = cond.parameters;

		if (IsNativeIf(cond)) {
			switch (cond.type) {
			case 9:  cur.loopCounter = (uint8_t)p[0]; break;
			case 10: if (cur.loopCounter == 0) JumpToFrame(c, s, idx, p[0], tick); break;
			case 37: cur.queuedPattern = p[0]; break;
			case 55: JumpToFrame(c, s, idx, c.opt->ownerSlotOdd ? p[1] : p[0], tick); break;
			case 18: {
				const SimActor* par = FindActor(s.actors, cur.parentId);
				if (par && !cur.isRoot && par->pattern == p[1]) JumpToFrame(c, s, idx, p[0], tick);
				break;
			}
			}
			continue;
		}

		// IF2 off-screen bits are position tests we can evaluate natively
		// (static positions; camera from Options).
		if (cond.type == 2 && (p[0] & 3) && !cur.isRoot) {
			const float zoom = c.opt->cameraZoom > 0.f ? c.opt->cameraZoom : 1.f;
			const float halfW = 160.f / zoom;
			bool out = false;
			if ((p[0] & 1) && (cur.x > c.opt->cameraX + halfW - 8.f + 50.f ||
			                   cur.x < c.opt->cameraX - halfW + 8.f - 50.f)) out = true;
			if ((p[0] & 2) && (cur.y < c.opt->cameraY - 212.f / zoom - 50.f ||
			                   cur.y > c.opt->cameraY + 28.f / zoom + 50.f)) out = true;
			if (out) { Kill(c, s.actors, cur.id, tick, "IF2 off-screen"); return; }
		}

		IfBranch br = ClassifyRuntimeIf(cond, cur.frame);
		if (br.act == IfAct::None) continue;

		IfKey key{a.effectHa6, a.pattern, a.frame, (int)i};
		IfAssume as = ResolveAssumption(*c.opt, key);
		if (as != IfAssume::True) {
			if (c.recording && !c.reportedUnmodeled->count(key)) {
				c.reportedUnmodeled->insert(key);
				Event(c, tick, EventKind::IfUnmodeled, cur.id, (int)br.act, br.target,
				      "runtime condition assumed false", &key, cond.type);
			}
			continue;
		}
		// Assumed true: fire once per frame visit.
		if (cur.ifFiredThisVisit) continue;
		cur.ifFiredThisVisit = true;
		Event(c, tick, EventKind::IfAssumedTrue, cur.id, (int)br.act, br.target,
		      "runtime condition assumed true", &key, cond.type);
		switch (br.act) {
		case IfAct::Frame: JumpToFrame(c, s, idx, br.target, tick); break;
		case IfAct::Queue: s.actors[idx].queuedPattern = br.target; break;
		case IfAct::Destroy:
			if (!cur.isRoot) Kill(c, s.actors, cur.id, tick, "IF destroy assumed true");
			break;
		default: break;
		}
	}
}

// Phase B: Character_RunFrameTick / AdvanceFrameByAniFlag.
void Update(Ctx& c, PreviewSim::State& s, size_t idx, int tick)
{
	SimActor& a = s.actors[idx];
	if (a.frame < 0) return;
	if (a.isPreset) { Kill(c, s.actors, a.id, tick, "preset marker (one tick)"); return; }
	if (a.isRoot && a.rootEnded) return;

	if (a.queuedPattern >= 0) {
		StartPattern(c, s, idx, a.queuedPattern, tick, 0);
		return;
	}

	Sequence* seq = GetSeq(c, a.effectHa6, a.pattern);
	if (!seq || a.frame >= (int)seq->frames.size()) { RanOffEnd(c, s, idx, tick); return; }
	const auto& fr = seq->frames[a.frame];
	const auto& af = fr.AF;

	a.frameTimer++;
	if (a.frameTimer < af.duration) return; // frame lasts max(1, duration) ticks

	switch (af.aniType) {
	case 0: {
		// End: +0x1F set; IF2 p3 destroys, otherwise start pattern AF jump.
		bool destroy = false;
		for (const auto& cond : fr.IF)
			if (cond.type == 2 && cond.parameters[2]) destroy = true;
		int target = af.jump + ((af.aniFlag & 4) ? a.pattern : 0);
		if (a.isRoot) {
			EndRoot(c, s, idx, tick, target, "pattern end (aniType 0)");
			a.frameTimer = std::max(1, af.duration);
			return;
		}
		if (destroy) { Kill(c, s.actors, a.id, tick, "pattern end + IF2 p3"); return; }
		StartPattern(c, s, idx, target, tick, 0);
		return;
	}
	case 1:
		JumpToFrame(c, s, idx, a.frame + 1, tick);
		return;
	case 2: {
		if (a.loopCounter) a.loopCounter--;
		int next;
		if (!(af.aniFlag & 2) || a.loopCounter)
			next = (af.aniFlag & 4) ? a.frame + af.jump : af.jump;
		else
			next = (af.aniFlag & 8) ? a.frame + af.loopEnd : af.loopEnd;
		JumpToFrame(c, s, idx, next, tick); // re-enters (and re-fires) on self-jumps
		return;
	}
	default:
		return; // other aniTypes hold the frame (timer keeps counting)
	}
}

void RunScheduled(Ctx& c, PreviewSim::State& s, int tick)
{
	const auto& sched = *c.sched;
	for (size_t i = 0; i < sched.size(); i++) {
		const ScheduledSpawn& e = sched[i];
		if (e.tick != tick) continue;
		// Parent: the root, or the live actor spawned by entry parentIndex.
		int parentIdx = -1;
		for (size_t k = 0; k < s.actors.size(); k++) {
			const SimActor& a = s.actors[k];
			if (a.frame < 0) continue;
			if (e.parentIndex < 0 ? a.isRoot : a.scheduleIndex == e.parentIndex) { parentIdx = (int)k; break; }
		}
		if (parentIdx < 0) continue;
		SpawnActor(c, s, (size_t)parentIdx, tick, 0, 0, e.patternId, e.effectHa6, e.offsetX, e.offsetY,
		           0, 0, 0, -1, 0, e.isMarker, (int)i);
	}
}

void Sweep(PreviewSim::State& s)
{
	s.actors.erase(std::remove_if(s.actors.begin(), s.actors.end(),
		[](const SimActor& a) { return a.frame < 0 && !a.isRoot; }), s.actors.end());
}

void InitState(Ctx& c, PreviewSim::State& s, int rootPattern)
{
	s = PreviewSim::State();
	s.tick = 0;
	s.rng = c.opt->rngSeed ? c.opt->rngSeed : 1u;
	SimActor root;
	root.id = s.nextId++;
	root.isRoot = true;
	root.pattern = rootPattern;
	root.facingLeft = c.opt->rootFacingLeft;
	s.actors.push_back(root);
	if (c.recording) {
		SpawnRecord r;
		r.id = root.id; r.pattern = rootPattern; r.spawnTick = 0;
		if (c.records->empty()) c.records->resize(1);
		(*c.records)[0] = r;
	}
	EnterFrame(c, s, 0, 0, 0, 0);
	RunScheduled(c, s, 0);
	Sweep(s);
}

// True when stepping can only advance the tick counter (nothing can change).
bool IsQuiescent(const PreviewSim::State& s, const std::vector<ScheduledSpawn>& sched)
{
	if (s.actors.size() != 1 || !s.actors[0].isRoot || !s.actors[0].rootEnded) return false;
	for (const auto& e : sched) if (e.tick > s.tick) return false;
	return true;
}

void Step(Ctx& c, PreviewSim::State& s)
{
	const int tick = s.tick + 1;
	s.tick = tick;
	if (IsQuiescent(s, *c.sched)) return;

	// Actors spawned during this tick are not updated this tick (the engine
	// sets the "spawned during update" skip flag, 0x563576).
	size_t n = s.actors.size();
	for (size_t i = 0; i < n && i < s.actors.size(); i++) RunConditions(c, s, i, tick);
	n = std::min(n, s.actors.size());
	for (size_t i = 0; i < n && i < s.actors.size(); i++) {
		if (s.actors[i].spawnTick == tick && !s.actors[i].isRoot) continue;
		Update(c, s, i, tick);
	}
	RunScheduled(c, s, tick);
	Sweep(s);
}

uint64_t FingerprintSeq(Sequence* s)
{
	uint64_t h = 1469598103934665603ull;
	if (!s) return h;
	size_t n = s->frames.size();
	h = Fnv(h, &n, sizeof(n));
	for (const auto& f : s->frames) {
		const auto& af = f.AF;
		int v[9] = {af.duration, af.aniType, (int)af.aniFlag, af.jump, af.loopCount, af.loopEnd,
		            af.priority, (!af.layers.empty() && af.layers[0].usePat) ? 1 : 0, (int)f.IF.size()};
		h = Fnv(h, v, sizeof(v));
		if (!f.EF.empty()) h = Fnv(h, f.EF.data(), f.EF.size() * sizeof(Frame_EF));
		if (!f.IF.empty()) h = Fnv(h, f.IF.data(), f.IF.size() * sizeof(Frame_IF));
	}
	return h;
}

} // namespace

// ---------------------------------------------------------------------------
// Placement helpers for tools
// ---------------------------------------------------------------------------

bool SpawnOffsetSlots(const Frame_EF& ef, int& xParam, int& yParam)
{
	switch (ef.type) {
	case 1: case 101: case 1000: case 8: case 108: case 3:
	case 11: case 111: // base offset of the random rectangle
		xParam = 0; yParam = 1; return true;
	default:
		return false;
	}
}

void SpawnFlagsets(const Frame_EF& ef, int& flagset1, int& flagset2)
{
	switch (ef.type) {
	case 1: case 101: case 1000: case 8: case 108:
		flagset1 = ef.parameters[2]; flagset2 = ef.parameters[3]; return;
	case 11: case 111:
		flagset1 = ef.parameters[6]; flagset2 = ef.parameters[7]; return;
	default:
		flagset1 = flagset2 = 0; return;
	}
}

SpawnPlacement ResolveSpawnPlacement(const Options& o, const SimActor& spawner,
                                     bool spawnerFrameUsesPat, int flagset1, int flagset2)
{
	SpawnPlacement r;
	r.scale = (o.patOwnerHalfScale && spawnerFrameUsesPat) ? 0.5f : 1.f;
	// PlaceChild is affine in (ox, oy): sample the origin and unit offsets.
	SimActor c0, cx, cy;
	c0.flagset1 = cx.flagset1 = cy.flagset1 = flagset1;
	c0.flagset2 = cx.flagset2 = cy.flagset2 = flagset2;
	PlaceChild(o, spawner, c0, 0, 0, 0, r.scale);
	PlaceChild(o, spawner, cx, 1, 0, 0, r.scale);
	PlaceChild(o, spawner, cy, 0, 1, 0, r.scale);
	r.baseX = c0.x; r.baseY = c0.y;
	r.kx = cx.x - c0.x;
	r.ky = cy.y - c0.y;
	r.facingLeft = c0.facingLeft;
	return r;
}

// ---------------------------------------------------------------------------
// PreviewSim
// ---------------------------------------------------------------------------

PreviewSim::PreviewSim() : m_impl(new Impl) { m_impl->owner = this; }
PreviewSim::~PreviewSim() { delete m_impl; }

void PreviewSim::setInputs(FrameData* mainData, FrameData* effectData, int rootPattern,
                           const Options& options, const std::vector<ScheduledSpawn>& scheduled)
{
	if (mainData == m_main && effectData == m_effect && rootPattern == m_rootPattern &&
	    options == m_options && scheduled == m_scheduled)
		return;
	m_main = mainData;
	m_effect = effectData;
	m_rootPattern = rootPattern;
	m_options = options;
	if (m_options.checkpointInterval < 1) m_options.checkpointInterval = 1;
	if (m_options.horizonTicks < 1) m_options.horizonTicks = 1;
	m_scheduled = scheduled;
	invalidate();
}

void PreviewSim::invalidate()
{
	Impl& I = *m_impl;
	if (I.valid) m_stats.invalidations++;
	I.valid = false;
	I.checkpoints.clear();
	for (auto& cur : I.cursors) cur = State();
	I.frontier = State();
	I.touched.clear();
	I.rootTrack.clear();
	I.reportedUnmodeled.clear();
	I.rootEnd = -1;
	I.settled = -1;
	I.rootValid = false;
	m_records.clear();
	m_events.clear();
	m_stats.checkpoints = 0;
	m_stats.frontierTick = -1;
}

bool PreviewSim::validate()
{
	Impl& I = *m_impl;
	if (!I.valid) return false;
	uint64_t mv = m_main ? m_main->dataVersion : 0;
	uint64_t ev = m_effect ? m_effect->dataVersion : 0;
	if (mv != I.mainVersion || ev != I.effectVersion) { invalidate(); return false; }
	// Safety net for edits that bypass mark_modified (undo restores etc.).
	uint64_t h = 1469598103934665603ull;
	for (const auto& t : I.touched) {
		FrameData* d = t.first ? m_effect : m_main;
		Sequence* s = d ? d->get_sequence(t.second) : nullptr;
		uint64_t sh = FingerprintSeq(s);
		h = Fnv(h, &sh, sizeof(sh));
	}
	if (h != I.fingerprint) { invalidate(); return false; }
	return true;
}

namespace {
uint64_t ComputeFingerprint(FrameData* main, FrameData* effect, const std::set<std::pair<bool, int>>& touched)
{
	uint64_t h = 1469598103934665603ull;
	for (const auto& t : touched) {
		FrameData* d = t.first ? effect : main;
		Sequence* s = d ? d->get_sequence(t.second) : nullptr;
		uint64_t sh = FingerprintSeq(s);
		h = Fnv(h, &sh, sizeof(sh));
	}
	return h;
}
} // namespace

void PreviewSim::ensureSimulatedTo(int tick)
{
	Impl& I = *m_impl;
	validate();
	tick = std::clamp(tick, 0, m_options.horizonTicks);

	Ctx c{m_main, m_effect, &m_options, &m_scheduled, &I.touched, true, &m_records, &m_events,
	      &I.reportedUnmodeled};

	if (!I.valid) {
		if (!m_main || !m_main->get_sequence(m_rootPattern) ||
		    m_main->get_sequence(m_rootPattern)->frames.empty()) {
			I.rootValid = false;
			return;
		}
		I.rootValid = true;
		I.mainVersion = m_main->dataVersion;
		I.effectVersion = m_effect ? m_effect->dataVersion : 0;
		InitState(c, I.frontier, m_rootPattern);
		I.checkpoints.push_back(I.frontier);
		I.rootTrack.assign(1, I.frontier.actors.empty() ? 0 : I.frontier.actors[0].frame);
		I.valid = true;
		I.fingerprint = ComputeFingerprint(m_main, m_effect, I.touched);
		m_stats.frontierTick = 0;
		m_stats.checkpoints = 1;
	}
	if (tick <= I.frontier.tick) return;

	const size_t touchedBefore = I.touched.size();
	const int N = m_options.checkpointInterval;
	while (I.frontier.tick < tick) {
		Step(c, I.frontier);
		m_stats.ticksStepped++;
		m_stats.frontierTicks++;
		const SimActor& root = I.frontier.actors[0];
		I.rootTrack.push_back(root.frame);
		if (root.rootEnded && I.rootEnd < 0) I.rootEnd = I.frontier.tick;
		if (I.settled < 0 && IsQuiescent(I.frontier, m_scheduled)) I.settled = I.frontier.tick;
		if (I.frontier.tick % N == 0) I.checkpoints.push_back(I.frontier);
	}
	if (I.touched.size() != touchedBefore)
		I.fingerprint = ComputeFingerprint(m_main, m_effect, I.touched);
	m_stats.frontierTick = I.frontier.tick;
	m_stats.checkpoints = (int)I.checkpoints.size();
}

bool PreviewSim::getStateAt(int tick, TickState& out)
{
	Impl& I = *m_impl;
	m_stats.queries++;
	m_stats.lastQuerySteps = 0;
	tick = std::clamp(tick, 0, m_options.horizonTicks);
	ensureSimulatedTo(tick); // validates, builds or extends the frontier
	if (!I.valid || !I.rootValid) { out.tick = tick; out.actors.clear(); return false; }

	const State* base = nullptr;
	int slot = -1;
	// Best cursor at or before tick.
	for (int k = 0; k < Impl::kCursors; k++) {
		const State& cur = I.cursors[k];
		if (cur.tick >= 0 && cur.tick <= tick && (!base || cur.tick > base->tick)) { base = &cur; slot = k; }
	}
	if (tick == I.frontier.tick && (!base || base->tick < tick)) { base = &I.frontier; slot = -1; }
	const int N = m_options.checkpointInterval;
	size_t cpi = std::min((size_t)(tick / N), I.checkpoints.size() - 1);
	const State& cp = I.checkpoints[cpi];
	if (!base || cp.tick > base->tick) { base = &cp; slot = -1; }

	// Pick the cursor slot to write: reuse the one we start from, else LRU.
	if (slot < 0) {
		slot = 0;
		for (int k = 1; k < Impl::kCursors; k++)
			if (I.cursorUse[k] < I.cursorUse[slot]) slot = k;
		I.cursors[slot] = *base;
	}
	I.cursorUse[slot] = ++I.useClock;
	State& cur = I.cursors[slot];

	Ctx c{m_main, m_effect, &m_options, &m_scheduled, &I.touched, false, &m_records, &m_events,
	      &I.reportedUnmodeled};
	while (cur.tick < tick) {
		Step(c, cur);
		m_stats.ticksStepped++;
		m_stats.lastQuerySteps++;
	}
	out.tick = tick;
	out.actors = cur.actors;
	return true;
}

int PreviewSim::rootEndTick()
{
	ensureSimulatedTo(m_options.horizonTicks);
	return m_impl->rootEnd;
}

int PreviewSim::settledTick()
{
	Impl& I = *m_impl;
	// Extend in chunks until settled or at the horizon.
	int t = std::max(I.frontier.tick, 0);
	while (I.settled < 0 && t < m_options.horizonTicks) {
		t = std::min(m_options.horizonTicks, t + 256);
		ensureSimulatedTo(t);
		if (!I.valid) return -1;
	}
	return I.settled;
}

int PreviewSim::firstTickOfRootFrame(int frame)
{
	Impl& I = *m_impl;
	ensureSimulatedTo(0);
	const int limit = std::min(m_options.horizonTicks, 600);
	for (int t = 0;; t++) {
		if (t >= (int)I.rootTrack.size()) {
			if (t > limit || !I.valid) return -1;
			if (I.rootEnd >= 0 && t > I.rootEnd) return -1;
			ensureSimulatedTo(std::min(limit, t + 64));
			if (t >= (int)I.rootTrack.size()) return -1;
		}
		if (I.rootTrack[t] == frame) return t;
	}
}

const std::vector<int>& PreviewSim::rootFrameTrack()
{
	ensureSimulatedTo(0);
	return m_impl->rootTrack;
}

} // namespace preview
