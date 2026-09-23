#ifndef GAME_LINK_PROTO_H_GUARD
#define GAME_LINK_PROTO_H_GUARD
// The PovertyCaster dev-link wire format, as seen from the editor side.
//
// DUPLICATE OF PovertyCaster pc-proto/include/pc/proto/Proto.hpp (IpcHeader, IpcKind::Link*, LinkOp, LinkStatus,
// kLinkFlag*, LinkCommand, LinkReply, LinkActor, LinkState) — Hantei-chan does not build against PovertyCaster,
// so the structs are mirrored here. Every struct is fixed-width and pointer-free on purpose, and the
// static_asserts pin the sizes to PovertyCaster's; a size change there must change here too. Protocol doc:
// docs/HANTEI_GAME_LINK.md.
#include <cstdint>

namespace gamelink::wire {

constexpr uint16_t kLinkVersion = 1;

enum class Kind : uint16_t { LinkCommand = 0x100, LinkReply = 0x101, LinkState = 0x102 };

enum class Op : uint16_t { Ping = 1, Reload = 2, SetChar = 3, QueryState = 4 };

constexpr uint8_t kFlagReload   = 1u << 0;
constexpr uint8_t kFlagForce    = 1u << 1;
constexpr uint8_t kFlagResetPos = 1u << 2;

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
