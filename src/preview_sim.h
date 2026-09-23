#ifndef PREVIEW_SIM_H_GUARD
#define PREVIEW_SIM_H_GUARD

// Cached tick simulator for pattern + spawn previews.
//
// Simulates a root pattern and every actor it spawns (EF 1/101/1000/8/108/
// 11/111, EF 3 preset markers, MBTL move-script spawns) tick by tick, using
// the MBAA engine's frame-advance and spawn-init rules (see
// docs/HANTEI_PREVIEW_SIM.md for the IDA evidence behind each rule).
//
// Design:
//   * The "frontier" simulation runs forward once and stores a full snapshot
//     every Options::checkpointInterval ticks. Spawn records (lifetimes) and
//     events are recorded only while extending the frontier.
//   * getStateAt(t) restores the nearest checkpoint (or an LRU cursor that is
//     already <= t) and steps forward, so any query costs at most
//     checkpointInterval ticks of simulation. Sequential playback costs one
//     tick per query.
//   * Every query validates the cache: input identity, options, the
//     FrameData::dataVersion counters, and a fingerprint of every sequence the
//     simulation actually read (catches edits that bypass mark_modified, e.g.
//     undo restores). Any mismatch drops all checkpoints.
//
// The module has no GL / ImGui / Windows dependencies so headless tools can
// link it (see src/preview_sim_tool.cpp).

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

class FrameData;
struct Frame_EF;

namespace preview {

// How a runtime IF condition (input, hit, distance, ...) is resolved.
enum class IfAssume : uint8_t {
	Default = 0,   // use Options::defaultIfAssumption
	False = 1,     // condition never holds: follow the authored AF flow
	True = 2,      // condition holds on the first evaluation of each frame visit
};

// Identifies one IF record: which data file, pattern, frame and IF index.
struct IfKey {
	bool effectHa6 = false;
	int pattern = -1;
	int frame = -1;
	int index = -1;
	bool operator<(const IfKey& o) const;
	bool operator==(const IfKey& o) const;
};

// A spawn not backed by an ha6 EF record (MBTL move scripts). Scheduled at an
// absolute tick; parentIndex links to another entry (its actor becomes the
// parent) or -1 for the root actor.
struct ScheduledSpawn {
	int tick = 0;
	int patternId = -1;
	bool effectHa6 = false;
	int offsetX = 0, offsetY = 0; // own offset relative to the parent, EF units
	int parentIndex = -1;
	bool isMarker = false;        // impact marker: preset-style, no pattern
	bool operator==(const ScheduledSpawn& o) const;
};

struct Options {
	// Context. World units are "CG pixels" (EF offset unit on a CG-sprite
	// owner frame; the engine uses 256 fixed-point units per CG pixel).
	bool rootFacingLeft = false;
	bool ownerSlotOdd = false;       // player slot parity, for flagset2 bit 11
	float opponentX = 160.f;         // opponent world X (flagset2 bit 8)
	float cameraX = 0.f;             // camera centre (flagset1 bit 4, screen edges)
	float cameraY = 0.f;
	float cameraZoom = 1.f;
	// MBAA rule: EF offsets are half-scale when the spawner's current frame is
	// a PAT frame (AFGP[0] != 0). Disable for non-MBAACC data (UNI/MBTL).
	bool patOwnerHalfScale = true;

	// Runtime conditions.
	IfAssume defaultIfAssumption = IfAssume::False;
	std::map<IfKey, IfAssume> ifOverrides;

	uint32_t rngSeed = 0x6d2b79f5u;  // EF11 / IF8 randomness (deterministic)
	int horizonTicks = 1800;         // hard simulation limit
	int checkpointInterval = 32;
	int maxActors = 1024;            // runaway-spawn guard (alive at once)

	bool operator==(const Options& o) const;
	bool operator!=(const Options& o) const { return !(*this == o); }
};

// One simulated actor. Consumers treat this as read-only state.
struct SimActor {
	int id = -1;
	int parentId = -1;        // -1 for the root
	int depth = 0;            // 0 = root, 1 = spawned by the root, ...
	bool isRoot = false;
	bool isPreset = false;    // EF3 preset / script impact marker (no pattern)
	bool isScript = false;    // from a ScheduledSpawn
	bool rootEnded = false;   // root pattern finished; frame is held

	int pattern = -1;         // pattern (or preset number when isPreset)
	bool effectHa6 = false;   // pattern lives in effect.ha6
	int frame = 0;
	int frameTimer = 0;       // ticks elapsed in the current frame visit
	int frameEnterTick = 0;
	int spawnTick = 0;

	float x = 0.f, y = 0.f;   // world position (root starts at 0,0)
	bool facingLeft = false;
	int angle = 0;            // 10000 = 360 degrees
	int zPriority = 0;        // sticky: AF priority 0 keeps the previous value

	int effectType = 0;       // spawning EF type (0 for root/script)
	int flagset1 = 0, flagset2 = 0;
	int linkFlags = 0;        // engine link flags derived from the flagsets
	float offsetScale = 1.f;  // 0.5 or 1.0 (spawner frame AFGP usePat)

	// Authored source record (stable identity across ticks).
	bool srcEffectHa6 = false;
	int srcPattern = -1, srcFrame = -1, srcEffectIndex = -1, srcSubIndex = 0;
	int scheduleIndex = -1;

	// Engine state.
	uint8_t loopCounter = 0;
	int queuedPattern = -1;
	bool ifFiredThisVisit = false;
	uint32_t visitSerial = 0;

	float angleTurns() const { return angle / 10000.f; }
	// World transform: translate(x,y) * MbaaTransform::ActorMatrix(angle, facing).
	glm::mat4 transform() const;
};

struct TickState {
	int tick = -1;
	std::vector<SimActor> actors; // alive actors, spawn order (root first)
	const SimActor* find(int id) const;
	const SimActor* root() const;
};

enum class EventKind : uint8_t {
	Spawn,             // actor spawned (a = actor id)
	Despawn,           // actor destroyed (a = actor id), detail says why
	RootEnded,         // root pattern ended / would change pattern
	PatternChange,     // actor started a queued pattern (b = new pattern)
	IfAssumedTrue,     // an IF branch was taken by assumption
	IfUnmodeled,       // a runtime IF was seen and assumed false
	Limit,             // actor/iteration guard hit
};

struct SimEvent {
	int tick = 0;
	EventKind kind = EventKind::Spawn;
	int actorId = -1;
	IfKey ifKey;       // for IF events
	int ifType = 0;
	int a = 0, b = 0;
	std::string detail;
};

// Lifetime of every actor ever spawned (frontier-recorded).
struct SpawnRecord {
	int id = -1, parentId = -1, depth = 0;
	int pattern = -1;
	bool effectHa6 = false, isPreset = false, isScript = false;
	int effectType = 0;
	bool srcEffectHa6 = false;
	int srcPattern = -1, srcFrame = -1, srcEffectIndex = -1, srcSubIndex = 0;
	int scheduleIndex = -1;
	int spawnTick = 0;
	int deathTick = -1;  // -1 = still alive at the frontier
	float x = 0.f, y = 0.f;
	bool facingLeft = false;
};

struct SimStats {
	uint64_t ticksStepped = 0;       // total ticks simulated (all paths)
	uint64_t frontierTicks = 0;      // ticks simulated while extending
	uint64_t invalidations = 0;
	uint64_t queries = 0;
	int checkpoints = 0;
	int frontierTick = -1;
	int lastQuerySteps = 0;          // ticks stepped by the last query
};

class PreviewSim {
public:
	PreviewSim();
	~PreviewSim();
	PreviewSim(const PreviewSim&) = delete;
	PreviewSim& operator=(const PreviewSim&) = delete;

	// Bind inputs. Cheap when unchanged; invalidates the cache otherwise.
	void setInputs(FrameData* mainData, FrameData* effectData, int rootPattern,
	               const Options& options,
	               const std::vector<ScheduledSpawn>& scheduled = {});

	// Drop every checkpoint (next query re-simulates from tick 0).
	void invalidate();

	// Fill `out` with the alive actors at `tick` (clamped to [0, horizon]).
	// Returns false when there is no valid root pattern.
	bool getStateAt(int tick, TickState& out);

	// Make sure the frontier covers `tick` (records/events up to it).
	void ensureSimulatedTo(int tick);

	// Tick where nothing is alive except an ended root (or the horizon).
	// Simulates as needed.
	int settledTick();
	// First tick the root ended (-1 if it never ends within the horizon).
	int rootEndTick();
	// First tick at which the root enters `frame`, or -1 if never (within
	// the frontier after ensureSimulatedTo(settledTick())).
	int firstTickOfRootFrame(int frame);
	// Root frame per tick for [0, frontier].
	const std::vector<int>& rootFrameTrack();

	const std::vector<SpawnRecord>& spawnRecords() const { return m_records; }
	const std::vector<SimEvent>& events() const { return m_events; }
	const SimStats& stats() const { return m_stats; }
	const Options& options() const { return m_options; }
	int horizon() const { return m_options.horizonTicks; }
	int rootPattern() const { return m_rootPattern; }

	// Check the cache against the bound data (versions + fingerprint of every
	// sequence read); invalidates on mismatch. Called by every query.
	bool validate();

	// Opaque internals (public only so the stepping code in the .cpp can name
	// them).
	struct State;
	struct Impl;

private:
	Impl* m_impl;

	FrameData* m_main = nullptr;
	FrameData* m_effect = nullptr;
	int m_rootPattern = -1;
	Options m_options;
	std::vector<ScheduledSpawn> m_scheduled;

	std::vector<SpawnRecord> m_records;
	std::vector<SimEvent> m_events;
	SimStats m_stats;
};

// Readable name for an event kind.
const char* EventKindName(EventKind k);

// ---------------------------------------------------------------------------
// Spawn placement (Effect_InitializeComplex 0x454900), shared with editing
// tools so handles sit exactly where the simulator puts the spawned actor.
// ---------------------------------------------------------------------------

// Parameter slots holding an EF's X/Y offset, for position-type EFs
// (1/101/1000, 8/108, 11/111, 3). False for other EF types.
bool SpawnOffsetSlots(const Frame_EF& ef, int& xParam, int& yParam);

// Flagset slots of a spawn EF (0 when the type has none, e.g. EF3).
void SpawnFlagsets(const Frame_EF& ef, int& flagset1, int& flagset2);

// World position of a spawn is (baseX + kx * X, baseY + ky * Y) for authored
// offsets X/Y. kx carries the facing mirror and the 0.5/1.0 owner scale.
struct SpawnPlacement {
	float baseX = 0.f, baseY = 0.f;
	float kx = 1.f, ky = 1.f;
	bool facingLeft = false;  // resolved child facing
	float scale = 1.f;        // owner offset scale used
};

// `spawner` needs x, y, facingLeft and angle; `spawnerFrameUsesPat` is the
// spawner's current frame AFGP[0] (layer 0 usePat).
SpawnPlacement ResolveSpawnPlacement(const Options& o, const SimActor& spawner,
                                     bool spawnerFrameUsesPat, int flagset1, int flagset2);

} // namespace preview

#endif /* PREVIEW_SIM_H_GUARD */
