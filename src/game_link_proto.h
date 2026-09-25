#ifndef GAME_LINK_PROTO_H_GUARD
#define GAME_LINK_PROTO_H_GUARD
// The PovertyCaster dev-link wire format, as seen from the editor side.
//
// DUPLICATE OF PovertyCaster pc-proto/include/pc/proto/Proto.hpp (IpcHeader, IpcKind::Link*, LinkOp, LinkStatus,
// kLinkFlag*, LinkCommand, LinkReply, LinkActor, LinkState, LinkStage) — Hantei-chan does not build against PovertyCaster,
// so the structs are mirrored here. Every struct is fixed-width and pointer-free on purpose, and the
// static_asserts pin the sizes to PovertyCaster's; a size change there must change here too. Protocol doc:
// docs/HANTEI_GAME_LINK.md; the stage ops: docs/HANTEI_STAGE_LINK.md.
#include <cstdint>

namespace gamelink::wire {

constexpr uint16_t kLinkVersion = 1;

enum class Kind : uint16_t { LinkCommand = 0x100, LinkReply = 0x101, LinkState = 0x102, LinkStage = 0x103 };

// SetStage carries the stage id in Command::slot. Stage ops are answered Unknown by a DLL that predates them.
enum class Op : uint16_t { Ping = 1, Reload = 2, SetChar = 3, QueryState = 4, SetStage = 5, ReloadStage = 6, QueryStage = 7 };

constexpr uint8_t kFlagReload    = 1u << 0;
constexpr uint8_t kFlagForce     = 1u << 1;
constexpr uint8_t kFlagResetPos  = 1u << 2;
constexpr uint8_t kFlagStageList = 1u << 3;   // stage ops: re-read Bg\BgList.ini first
constexpr uint8_t kFlagKeepBgm   = 1u << 4;   // SetStage: do not restart the BGM

enum class Status : int16_t {
	Ok = 0, Queued = 1, RefusedSession = -1, RefusedRecording = -2, RefusedScene = -3,
	RefusedVariant = -4, BadArgs = -5, MissingFile = -6, Busy = -7, Unknown = -8,
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
	}
	return "?";
}

} // namespace gamelink::wire

#endif
