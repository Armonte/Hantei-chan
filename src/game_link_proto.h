#ifndef GAME_LINK_PROTO_H_GUARD
#define GAME_LINK_PROTO_H_GUARD
// The PovertyCaster dev-link wire format, as seen from the editor side.
//
// DUPLICATE OF PovertyCaster pc-proto/include/pc/proto/Proto.hpp (IpcHeader, IpcKind::Link*, LinkOp, LinkStatus,
// kLinkFlag*, LinkCommand, LinkReply, LinkActor, LinkState, LinkStage) — Hantei-chan does not build against PovertyCaster,
// so the structs are mirrored here. Every struct is fixed-width and pointer-free on purpose, and the
// static_asserts pin the sizes to PovertyCaster's; a size change there must change here too. Protocol doc:
// docs/HANTEI_GAME_LINK.md; the stage ops: docs/HANTEI_STAGE_LINK.md.
#include <cstddef>
#include <cstdint>

namespace gamelink::wire {

constexpr uint16_t kLinkVersion = 1;

enum class Kind : uint16_t { LinkCommand = 0x100, LinkReply = 0x101, LinkState = 0x102, LinkStage = 0x103,
	LinkTag = 0x104 /* [link-tag], see Tag below */,
	// [authoring] docs/HANTEI_AUTHORING_MODE.md §3.2 (0x10A-0x10F / 0x111-0x11F reserved; 0x120-0x13F Training/TAS)
	LinkCaps = 0x105, LinkRoster = 0x106, LinkSetupState = 0x107, LinkTuningGlobal = 0x108, LinkTuningSlot = 0x109,
	LinkCommandEx = 0x110,
	LinkFrameShare = 0x10A /* [game-view] §12.1, ADDED by Hantei agent */ };

// SetStage carries the stage id in Command::slot. Stage ops are answered Unknown by a DLL that predates them.
enum class Op : uint16_t { Ping = 1, Reload = 2, SetChar = 3, QueryState = 4, SetStage = 5, ReloadStage = 6, QueryStage = 7,
	QueryTag = 8 /* [link-tag], see Tag below */,
	// [authoring] §3.2. 16-31 reserved (authoring growth), 32-63 reserved (Training / TAS, §3.8)
	QueryCaps = 9, QueryRoster = 10, QueryMatchSetup = 11, SetMatchSetup = 12 /* LinkCommandEx only */, ApplyTuning = 13,
	QueryTuning = 14, EndAuthoring = 15,
	// [game-view] §12.1 (ADDED by Hantei agent)
	QueryFrameShare = 16,
	SetEmbedded = 17 /* LinkCommand.slot: 0 show the real window (export stays on), 1 embedded full frame, 2 embedded layered */,
	InputInject = 18 /* LinkCommandEx + LinkInputInject */,
	SetStageLighting = 19 /* LinkCommandEx + LinkStageLighting (§12.1: custom stages rendered by Hantei-chan) */ };

constexpr uint8_t kFlagReload    = 1u << 0;
constexpr uint8_t kFlagForce     = 1u << 1;
constexpr uint8_t kFlagResetPos  = 1u << 2;
constexpr uint8_t kFlagStageList = 1u << 3;   // stage ops: re-read Bg\BgList.ini first
constexpr uint8_t kFlagKeepBgm   = 1u << 4;   // SetStage: do not restart the BGM
constexpr uint8_t kFlagQueryAfter = 1u << 5;  // [authoring] ApplyTuning: follow the reply with the QueryTuning answer

enum class Status : int16_t {
	Ok = 0, Queued = 1, RefusedSession = -1, RefusedRecording = -2, RefusedScene = -3,
	RefusedVariant = -4, BadArgs = -5, MissingFile = -6, Busy = -7, Unknown = -8,
	// [authoring] §3.2: only the new ops return these
	NeedsRestart = -9, Unsupported = -10, Ineligible = -11, Timeout = -12,
};

struct Header { uint16_t kind; uint16_t version; uint32_t size; };
static_assert(sizeof(Header) == 8, "IpcHeader size");

struct Command {
	uint16_t op; uint16_t seq; uint8_t slotMask; uint8_t flags; uint16_t _pad;
	int32_t slot; int32_t chara; int32_t moon; int32_t palette;
};
static_assert(sizeof(Command) == 24, "LinkCommand size");

struct Reply {
	uint16_t op; uint16_t seq; int16_t status; uint16_t _pad; uint32_t reloadCount; char message[116];
};
static_assert(sizeof(Reply) == 128, "LinkReply size");

struct Actor {
	uint8_t exists, team, tagFlag, partnerSlot;
	int16_t chara, moon, palette, _pad;
	int32_t pattern, frame, frameTicks, patternTicks, x, y;
	char file[28];
};
static_assert(sizeof(Actor) == 64, "LinkActor size");

struct State {
	uint32_t worldTimer; uint32_t gameModeKind; uint32_t reloadCount;
	uint16_t scene; uint8_t reloadAllowed; uint8_t tagLive;
	int8_t teamActive[2]; int8_t teamTagRequest[2];
	Actor actors[4];
};
static_assert(sizeof(State) == 276, "LinkState size");

// The answer to QueryStage (PovertyCaster LinkStage).
struct Stage {
	int32_t selected;       // g_SelectedStageId
	int32_t loaded;         // on screen (-1 = none)
	uint32_t stageLoads;    // SetStage + ReloadStage runs so far
	int16_t bgmId;
	uint8_t allowed;        // a stage op would be accepted now
	uint8_t _pad;
	uint8_t valid[16];      // bit i = BgList.ini entry i exists (i < 100)
	char dataFile[32];      // the loaded entry's DataFile, e.g. "bg28"
	bool IsValid(int id) const { return id > 0 && id < 100 && (valid[id >> 3] & (1u << (id & 7))) != 0; }
};
static_assert(sizeof(Stage) == 64, "LinkStage size");

// ---- [link-tag] the answer to QueryTag (docs/HANTEI_TAG_PANEL.md §5) ----
// Mirror of PovertyCaster pc-proto Proto.hpp LinkTag / LinkTagTeam / LinkTagSlot (branch mbaacc/link-tag; built by
// pc-adapters/mbaacc/include/mbaacc/LinkTag.hpp, answered by MbaaccSim_EtmLink.cpp, read-only, in every mode). An
// older pchost.dll answers QueryTag with Unknown and the Tag / Team panel falls back to LinkState (point / reserve /
// tag flag / "tag in progress") and a gate probe for the session state. Sizes AND field offsets are pinned on both
// sides (PovertyCaster tests/mbaacc_link_tag, Hantei-chan tests/game_link_test), and the client drops a reply of the
// wrong size instead of misreading it.
constexpr uint8_t kTagSessNetplay = 1u << 0, kTagSessRollback = 1u << 1, kTagSessReplay = 1u << 2,
                  kTagSessStepped = 1u << 3, kTagSessNetMode = 1u << 4, kTagSessRecording = 1u << 5;
constexpr uint8_t kTagCfgTag = 1u << 0, kTagCfgFromHost = 1u << 1, kTagCfgPartner0 = 1u << 2, kTagCfgPartner1 = 1u << 3;
struct TagTeam {
	int8_t activeSlot;        // g_TeamAux[t].activeSlot (the point)
	uint8_t assistSlot;       // AssistTeam.slot: directional slot 0..4 (5,2,6,4,8) + mode << 4
	uint8_t assistPlacement;  // AssistTeam.placement: the resolved entry (0 behind, 1 edge, 2 drop, 3 arc)
	uint8_t assistFlags;      // AssistTeam.flags (kAssistFlag*)
	int32_t tagRequest;       // g_TeamAux[t].tagRequest raw: 0 idle, 200 22D accepted (the entry starts), 255/256
	                          //   swap, 254 S1, 100 exit, 101 cooldown, 150 forced tag-in pending, 300..303 assist
	int32_t counter;          // g_TeamAux[t] +8 (state tick)
	int32_t tagInTick;        // tag::tagInTick() for the point (-1 = not in a tag-in, 0x7FFF = past the window)
	int32_t cooldownLeft;     // state 101: cooldownTicks - counter (0 otherwise)
	int32_t assistTick;       // AssistTeam.tick
	int32_t assistCooldown;   // AssistTeam.cooldown
	int32_t assistPattern;    // AssistTeam.pattern (0 = none)
	int32_t assistCalls;      // AssistTeam.calls (this round)
	int32_t meterPaid;        // AssistTeam.meterPaid
};
static_assert(sizeof(TagTeam) == 40, "LinkTagTeam size");
struct TagSlot {
	int16_t tagIn, tagOut;    // CharaSystemData +0x28 / +0x2C in force for this slot
	int32_t health, red;      // actor +0xB8 / +0xBC
};
static_assert(sizeof(TagSlot) == 12, "LinkTagSlot size");
struct Tag {
	uint8_t sessionFlags;     // kTagSess* (the reload gate's inputs: any bit = tuning frozen)
	uint8_t frozen;           // tag_tuning.ini is frozen (tuningReloadVerdict != Allowed)
	uint8_t iniPresent;       // tag_tuning.ini exists next to MBAA.exe
	uint8_t koRule;           // the rule in force: 0 oneDown, 1 allDown (the host's in netplay)
	uint32_t tuningLoads;     // successful ini (re)loads in this process
	uint16_t warnings;        // ini warnings of the last load
	uint8_t assistEnabled;    // resolved assistEnabled
	uint8_t tagConfig;        // kTagCfg*: tag::tagSessionConfig() (PovertyCaster tag-session)
	char activeStyle[24];     // the resolved active style ("" = defaults)
	char sha[16];             // the first 15 hex digits of the resolved set's sha256 (as the log prints it)
	TagTeam team[2];
	TagSlot slot[4];
};
static_assert(sizeof(Tag) == 180, "LinkTag size");
static_assert(offsetof(Tag, tagConfig) == 11 && offsetof(Tag, activeStyle) == 12 && offsetof(Tag, sha) == 36 && offsetof(Tag, team) == 52 &&
              offsetof(Tag, slot) == 132, "LinkTag offsets (PovertyCaster tests/mbaacc_link_tag pins the same)");
static_assert(offsetof(TagTeam, tagRequest) == 4 && offsetof(TagTeam, cooldownLeft) == 16 &&
              offsetof(TagTeam, assistPattern) == 28 && offsetof(TagTeam, meterPaid) == 36, "LinkTagTeam offsets");


// ================== [authoring] docs/HANTEI_AUTHORING_MODE.md §3.3 (+ §9.2 / §9.3) ==================
// Mirror of PovertyCaster pc-proto Proto.hpp (branch mbaacc/authoring). Pointer-free, little-endian, no 64-bit fields.
// Sizes AND offsets are pinned here and in PovertyCaster tests/mbaacc_link_authoring.
constexpr uint16_t kLinkMaxPayload = 1024;        // pc-ipc PipeChannel kMaxPayload, both ways

// editor -> DLL: the extended command (Kind::LinkCommandEx). Header.size = 8 + size.
struct CommandEx {
	uint16_t op;       // +0 Op (only SetMatchSetup in revision 1)
	uint16_t seq;      // +2 echoed in the LinkReply
	uint16_t size;     // +4 payload bytes that follow
	uint16_t _pad;     // +6
};
static_assert(sizeof(CommandEx) == 8, "LinkCommandEx size");

struct Pick {
	int16_t chara;     // +0 g_CharaSelectDataTable index; -1 = empty
	uint8_t moon;      // +2 0 crescent, 1 full, 2 half
	uint8_t palette;   // +3 0-based
};
static_assert(sizeof(Pick) == 4, "LinkPick size");

constexpr uint8_t kMatchSetupVersion = 1;
enum class Mode : uint8_t { Versus = 0, Tag = 1, Team = 2 };
enum class Scene : uint8_t { Auto = 0, Training = 1, AuthoringVs = 2 };
constexpr uint8_t kSetupAssists = 1u << 0, kSetupKeepBgm = 1u << 1, kSetupForce = 1u << 2, kSetupResetPos = 1u << 3,
                  kSetupHotOnly = 1u << 4, kSetupTuningFirst = 1u << 5;
struct MatchSetup {
	uint8_t version;          // +0 kMatchSetupVersion
	uint8_t mode;             // +1 Mode
	uint8_t scene;            // +2 Scene
	uint8_t flags;            // +3 kSetup*
	int16_t stage;            // +4 1..99; 0 keep; -1 random
	uint8_t koRule;           // +6 0 oneDown, 1 allDown, 0xFF tuning
	uint8_t timer;            // +7 0 infinite, 1/2/4 speeds, 0xFF scene default
	Pick slot[4];             // +8 engine slots: 0 P1 point, 1 P2 point, 2 P1 partner, 3 P2 partner
	uint8_t assist[2][5];     // +24 per side per direction (5,2,6,4,8): 0 tuning, k = kAssistMotionChoices[k-1]
	uint8_t dummy[2];         // +34 reserved (Training), 0
	uint8_t _reserved[28];    // +36 0
};
static_assert(sizeof(MatchSetup) == 64, "LinkMatchSetup size");
static_assert(offsetof(MatchSetup, stage) == 4 && offsetof(MatchSetup, koRule) == 6 && offsetof(MatchSetup, slot) == 8 &&
              offsetof(MatchSetup, assist) == 24 && offsetof(MatchSetup, dummy) == 34 && offsetof(MatchSetup, _reserved) == 36,
              "LinkMatchSetup offsets");

constexpr uint32_t kCapStage = 1u << 0, kCapTag = 1u << 1, kCapRoster = 1u << 2, kCapSetup = 1u << 3, kCapTuning = 1u << 4,
                   kCapSidecars = 1u << 5, kCapTeam4P = 1u << 6, kCapTrainingScene = 1u << 7,
                   kCapFrameShare = 1u << 8 /* [game-view] QueryFrameShare / SetEmbedded */, kCapInputInject = 1u << 9;
struct Caps {
	uint16_t revision;         // +0 authoring revision (1)
	uint16_t _pad;             // +2
	uint32_t caps;             // +4 kCap*
	uint32_t leverTableHash;   // +8 FNV-1a of the lever table (§3.5)
	uint8_t leverCount;        // +12
	uint8_t perCharLeverCount; // +13
	uint8_t matchSetupVersion; // +14
	uint8_t tuningWireVersion; // +15
	char build[16];            // +16 pchost build id
	uint8_t tuningSource;      // +32 0 defaults, 1 legacy, 2 sidecars, 3 host/tape
	uint8_t tuningFlags;       // +33 kTunFlag*
	uint16_t charFiles;        // +34
	char gameId[8];            // +36 §9.2 "mbaacc" ("" = mbaacc, an early revision-1 DLL)
	uint8_t _reserved[4];      // +44
};
static_assert(sizeof(Caps) == 48, "LinkCaps size");
static_assert(offsetof(Caps, leverTableHash) == 8 && offsetof(Caps, build) == 16 && offsetof(Caps, tuningSource) == 32 &&
              offsetof(Caps, charFiles) == 34 && offsetof(Caps, gameId) == 36, "LinkCaps offsets");

constexpr uint8_t kRosterDuo = 1u << 0, kRosterTagOk = 1u << 1, kRosterTeamOk = 1u << 2, kRosterHasTcRow = 1u << 3,
                  kRosterNeedsMod = 1u << 4;
struct RosterEntry {
	int16_t chara;       // +0
	uint8_t selector;    // +2 CSS grid cell
	uint8_t flags;       // +3 kRoster*
	char file1[20];      // +4 lower case
	char file2[20];      // +24
	char name[12];       // +44
};
static_assert(sizeof(RosterEntry) == 56, "LinkRosterEntry size");
struct Roster {
	uint8_t page, pageCount, count, total;   // +0..+3
	uint8_t modDataLoaded;                    // +4
	uint8_t _pad[3];                          // +5
	RosterEntry e[16];                        // +8
};
static_assert(sizeof(Roster) == 904, "LinkRoster size");

enum class Phase : uint8_t { Unknown = 0, Boot = 1, Title = 2, MainMenu = 3, CharaSelect = 4, Loading = 5, Battle = 6,
                             RoundEnd = 7, Other = 8 };
enum class AuthState : uint8_t { Idle = 0, ApplyingHot = 1, Rebuilding = 2, Ready = 3, Failed = 4 };
enum class SessionRole : uint8_t { Offline = 0, Host = 1, Client = 2, Viewer = 3 };
struct SetupState {
	MatchSetup requested;      // +0
	MatchSetup inForce;        // +64 read back (valid in Battle)
	uint8_t phase;             // +128 Phase
	uint8_t authState;         // +129 AuthState
	uint8_t sessionFlags;      // +130 kTagSess*
	uint8_t editsAllowed;      // +131
	uint16_t lastSeq;          // +132
	int16_t lastStatus;        // +134
	uint32_t setupCount;       // +136
	uint32_t gameModeKind;     // +140
	uint32_t lastDurationMs;   // +144
	uint8_t lastPath;          // +148 0 none, 1 hot, 2 cold
	uint8_t sessionRole;       // +149 §9.3 SessionRole
	uint8_t betweenRounds;     // +150 §9.3
	uint8_t _pad;              // +151
	char message[40];          // +152
};
static_assert(sizeof(SetupState) == 192, "LinkSetupState size");
static_assert(offsetof(SetupState, inForce) == 64 && offsetof(SetupState, phase) == 128 && offsetof(SetupState, lastSeq) == 132 &&
              offsetof(SetupState, setupCount) == 136 && offsetof(SetupState, lastPath) == 148 &&
              offsetof(SetupState, sessionRole) == 149 && offsetof(SetupState, message) == 152, "LinkSetupState offsets");

constexpr uint8_t kTunFlagLegacyIgnored = 1u << 0, kTunFlagHotReloadPaused = 1u << 1, kTunFlagParkXRestart = 1u << 2,
                  kTunFlagGlobalCharSecs = 1u << 3, kTunFlagReadError = 1u << 4;
enum class TuningSource : uint8_t { Defaults = 0, Legacy = 1, Sidecars = 2, Adopted = 3 };
struct TuningGlobal {
	uint8_t revision;          // +0 1
	uint8_t source;            // +1 TuningSource
	uint8_t frozen;            // +2
	uint8_t leverCount;        // +3
	uint32_t leverTableHash;   // +4
	uint32_t tuningLoads;      // +8
	uint16_t warnings;         // +12
	uint8_t flags;             // +14 kTunFlag*
	uint8_t sessionFlags;      // +15
	uint16_t charFiles;        // +16
	uint16_t _pad;             // +18
	char activeStyle[24];      // +20
	char sha[16];              // +44
	uint8_t _pad2[4];          // +60
	uint32_t tuningMask[2];    // +64
	uint32_t styleMask[2];     // +72
	uint32_t envMask[2];       // +80
	int32_t values[64];        // +88
};
static_assert(sizeof(TuningGlobal) == 344, "LinkTuningGlobal size");
static_assert(offsetof(TuningGlobal, activeStyle) == 20 && offsetof(TuningGlobal, sha) == 44 && offsetof(TuningGlobal, tuningMask) == 64 &&
              offsetof(TuningGlobal, envMask) == 80 && offsetof(TuningGlobal, values) == 88, "LinkTuningGlobal offsets");
constexpr uint8_t kTunSlotHasCharFile = 1u << 0, kTunSlotHasMoonSec = 1u << 1, kTunSlotCssAssists = 1u << 2,
                  kTunSlotFromHost = 1u << 3;
struct TuningSlot {
	uint8_t slot;              // +0
	uint8_t exists;            // +1
	uint8_t moon;              // +2 0xFF unknown
	uint8_t flags;             // +3 kTunSlot*
	char file[28];             // +4
	uint32_t charMask[2];      // +32
	uint32_t moonMask[2];      // +40
	uint32_t cssMask[2];       // +48
	int32_t values[64];        // +56
};
static_assert(sizeof(TuningSlot) == 312, "LinkTuningSlot size");
static_assert(offsetof(TuningSlot, file) == 4 && offsetof(TuningSlot, charMask) == 32 && offsetof(TuningSlot, cssMask) == 48 &&
              offsetof(TuningSlot, values) == 56, "LinkTuningSlot offsets");

inline bool MaskBit(const uint32_t m[2], int i) { return i >= 0 && i < 64 && ((m[i >> 5] >> (i & 31)) & 1u); }
inline void SetMaskBit(uint32_t m[2], int i) { if (i >= 0 && i < 64) m[i >> 5] |= 1u << (i & 31); }

static_assert(sizeof(Roster) <= kLinkMaxPayload, "every message <= 1024 B payload");
static_assert(sizeof(CommandEx) + sizeof(MatchSetup) <= kLinkMaxPayload && sizeof(TuningGlobal) <= kLinkMaxPayload &&
              sizeof(SetupState) <= kLinkMaxPayload, "every message <= 1024 B payload");


// ================== [game-view] docs/HANTEI_AUTHORING_MODE.md §12.1 (ADDED by Hantei agent) ==================
// QueryFrameShare (LinkCommand) -> LinkFrameShare: where the frame ring (game_frame_share.h) lives. Never gated.
struct FrameShare {               // 96 B
	uint16_t version;             // +0  framering::kVersion (0 = no ring: the export is off / not started)
	uint8_t  slotCount;           // +2
	uint8_t  layerCapacity;       // +3  1 = full frame only; 3 = full + chars + HUD (§12.1)
	uint32_t maxWidth, maxHeight; // +4 +8
	uint32_t ringBytes;           // +12 the mapping size (framering::RingBytes)
	uint32_t flags;               // +16 framering::kFlag* (a copy of the ring header's flags)
	uint32_t width, height;       // +20 +24 the current backbuffer
	uint32_t framesPublished;     // +28
	char     name[64];            // +32 the file mapping name, NUL-terminated ("Local\povertycaster-frames-<pid>")
};
static_assert(sizeof(FrameShare) == 96, "LinkFrameShare size");
static_assert(offsetof(FrameShare, ringBytes) == 12 && offsetof(FrameShare, name) == 32, "LinkFrameShare offsets");

// InputInject (LinkCommandEx, payload = InputInject): the panel's keyboard / pad state for one player. It feeds the
// game's normal controller input path (the same place the binder writes), never a key poll. Gated like
// SetMatchSetup (offline / authoring only; netplay, replay, spectate and recording refuse it). A state holds for
// `holdFrames` game frames and then goes neutral, so a stalled editor never leaves a button stuck.
constexpr uint8_t kInjBtnA = 1u << 0, kInjBtnB = 1u << 1, kInjBtnC = 1u << 2, kInjBtnD = 1u << 3,
                  kInjBtnE = 1u << 4 /* FN1 / assist */, kInjBtnFN2 = 1u << 5, kInjBtnStart = 1u << 6;
constexpr uint8_t kInjFlagRelease = 1u << 0;   // neutral now (focus lost / panel closed), ignores the rest
constexpr uint8_t kInjectVersion = 1;
struct InputInject {              // 16 B
	uint8_t  version;             // +0  kInjectVersion
	uint8_t  player;              // +1  0..3 (engine input slot)
	uint8_t  flags;               // +2  kInjFlag*
	uint8_t  direction;           // +3  numpad notation 1..9 (5 = neutral)
	uint8_t  buttons;             // +4  kInjBtn*
	uint8_t  _pad[3];             // +5
	uint16_t holdFrames;          // +8  frames this state holds (1..600); refreshed by the next inject
	uint16_t _pad2;               // +10
	uint32_t serial;              // +12 editor counter (the reply echoes the op; the log shows the serial)
};
static_assert(sizeof(InputInject) == 16, "LinkInputInject size");
static_assert(offsetof(InputInject, buttons) == 4 && offsetof(InputInject, holdFrames) == 8 && offsetof(InputInject, serial) == 12,
              "LinkInputInject offsets");
// SetStageLighting (LinkCommandEx): in layered mode Hantei-chan draws the stage, but the game still applies the stage's
// light / StageColorVal to the characters (and its BgPointBlur); for a custom (scratch) stage those values come from
// here. Offline only. stageId -1 = "the stage on screen"; flags bit 0 = reset to the game's own values.
struct StageLighting {            // 16 B
	int16_t  stageId;             // +0
	uint16_t flags;               // +2
	uint32_t lightArgb;           // +4
	uint32_t stageColorValX1000;  // +8
	uint32_t _reserved;           // +12
};
static_assert(sizeof(StageLighting) == 16, "LinkStageLighting size");

inline const char* PhaseName(uint8_t p)
{
	static const char* n[] = { "unknown", "boot", "title", "main menu", "character select", "loading", "battle", "round end", "other" };
	return p < 9 ? n[p] : "?";
}
inline const char* AuthStateName(uint8_t a)
{
	static const char* n[] = { "idle", "applying (hot)", "rebuilding (cold)", "ready", "failed" };
	return a < 5 ? n[a] : "?";
}

inline const char* StatusName(int16_t s)
{
	switch ((Status)s) {
	case Status::Ok: return "ok";
	case Status::Queued: return "queued";
	case Status::RefusedSession: return "refused: session";
	case Status::RefusedRecording: return "refused: recording";
	case Status::RefusedScene: return "refused: not in battle";
	case Status::RefusedVariant: return "refused: variant";
	case Status::BadArgs: return "bad args";
	case Status::MissingFile: return "missing data file";
	case Status::Busy: return "busy";
	case Status::Unknown: return "unknown op";
	case Status::NeedsRestart: return "needs a game restart";
	case Status::Unsupported: return "unsupported";
	case Status::Ineligible: return "ineligible pick";
	case Status::Timeout: return "timeout";
	}
	return "?";
}

} // namespace gamelink::wire

#endif
