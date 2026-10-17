// DUPLICATE OF PovertyCaster pc-proto/include/pc/proto/game_frame_share.h — keep byte-identical; layout hash pinned in tests
#ifndef GAME_FRAME_SHARE_H_GUARD
#define GAME_FRAME_SHARE_H_GUARD
// [game-view] The embedded game view's shared-memory FRAME RING (docs/HANTEI_AUTHORING_MODE.md §12, §12.1, and the
// layout in §12.2 "ADDED by Hantei agent"). pchost.dll copies every presented frame into it; Hantei-chan's "Game" panel
// shows the newest one, either as the FULL frame or LAYERED: back stage (Hantei-chan's own renderer, at the frame's
// camera) -> L1 characters + effects -> front stage (Hantei-chan) -> L2 HUD, premultiplied alpha.
//
// THIS HEADER IS MEANT TO BE SHARED VERBATIM by both sides (PovertyCaster copies it next to Proto.hpp, the way
// game_link_proto.h mirrors Proto.hpp): pointer-free, little-endian, fixed-width, no 64-bit fields, no Win32. Sizes,
// offsets and the golden values below are static_asserted here and pinned in tests/game_view_test.cpp.
//
// MAPPING  Local\povertycaster-frames-<producer pid>, CreateFileMappingW(INVALID_HANDLE_VALUE, current-user DACL,
//          PAGE_READWRITE, RingBytes(maxW, maxH, layerCapacity, slots)). Readers open it read-only (FILE_MAP_READ). The
//          name is announced by the link (LinkFrameShare.name): a reader never guesses a pid.
//
//   +0                         FrameRingHeader  (256 B)
//   +256                       slot 0: FrameSlotHeader (384 B), then layerCapacity layer regions of layerStride B each
//   +256 + slotStride          slot 1 ...
//   maxPitch    = maxW * 4 rounded up to 64 B
//   layerStride = maxPitch * maxH
//   slotStride  = 384 + layerCapacity * layerStride
// A frame uses layerCount <= layerCapacity layers; layer k's pixels are at slot + layers[k].offset (inside region k).
//
// LAYER KINDS  0 = FULL: the finished frame (opaque, BGRX), what the player sees. The first version ships only this.
//              1 = CHARS: characters + effects + shadows, premultiplied BGRA over a transparent clear (stage NOT drawn).
//              2 = HUD:   the HUD, premultiplied BGRA.
//   A layered frame may also carry FULL (so a reader can toggle without a round trip); FrameSlotHeader.flags says which.
//
// THE SEQLOCK (one writer, any number of readers, no locks):
//   writer:  pick w = the slot after `latest` (never the latest one: a reader is most likely on it)
//            slot[w].seq += 1 (odd = being written)  -> store-release
//            write the slot header fields and the layers
//            slot[w].seq += 1 (even = complete)      -> store-release
//            header.latest = w; header.frameSeq = the slot's frameSeq   (store-release, in that order)
//   reader:  i = header.latest (acquire); s1 = slot[i].seq (acquire); odd -> retry
//            copy the slot header + the used layers out; acquire fence; s2 = slot[i].seq
//            s1 != s2 -> the writer lapped us: retry (bounded); else the copy is one complete frame.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace framering {

constexpr uint32_t kMagic = 0x53464350u;       // "PCFS" little-endian (bytes 'P','C','F','S')
constexpr uint16_t kVersion = 1;
constexpr uint32_t kHeaderBytes = 256;
constexpr uint32_t kSlotHeaderBytes = 384;
constexpr uint32_t kMaxLayers = 4;
constexpr uint32_t kMinSlots = 2, kMaxSlots = 3;
constexpr uint32_t kMaxWidth = 1920, kMaxHeight = 1200;   // the producer sizes the ring for its backbuffer, never above
constexpr uint32_t kNoSlot = 0xFFFFFFFFu;

// pixel formats (layer.format)
constexpr uint16_t kFormatBGRX8 = 1;           // B,G,R,X bytes; opaque (D3DFMT_X8R8G8B8 as locked)
constexpr uint16_t kFormatBGRA8 = 2;           // B,G,R,A bytes; see kLayerPremultiplied
// layer kinds (layer.kind)
constexpr uint8_t kLayerFull = 0, kLayerChars = 1, kLayerHud = 2;
// layer flags
constexpr uint8_t kLayerPremultiplied = 1u << 0;
// ring header flags
constexpr uint32_t kFlagProducerAlive = 1u << 0;   // set while pchost publishes (cleared on shutdown)
constexpr uint32_t kFlagEmbedded = 1u << 1;        // the real game window is hidden (SetEmbedded 1 / 2)
constexpr uint32_t kFlagInputInject = 1u << 2;     // the producer accepts InputInject right now (offline / authoring)
constexpr uint32_t kFlagChecksums = 1u << 3;       // layers carry a pixel checksum (tests / the mock; pchost may not)
constexpr uint32_t kFlagLayered = 1u << 4;         // layered capture is on (SetEmbedded 2)
// slot flags
constexpr uint32_t kSlotHasFull = 1u << 0;         // a kLayerFull layer is in this frame
constexpr uint32_t kSlotLayered = 1u << 1;         // CHARS (+ HUD) layers are in this frame; the stage was NOT drawn

struct FrameRingHeader {          // 256 B
	uint32_t magic;               // +0   kMagic (written last by the producer)
	uint16_t version;             // +4   kVersion
	uint16_t headerBytes;         // +6   kHeaderBytes
	uint32_t slotCount;           // +8   2..3
	uint32_t slotStride;          // +12  bytes from one slot header to the next
	uint32_t maxWidth;            // +16  capacity of every layer region
	uint32_t maxHeight;           // +20
	uint32_t maxPitch;            // +24
	uint32_t layerCapacity;       // +28  1..kMaxLayers regions per slot
	uint32_t layerStride;         // +32  bytes per layer region
	uint32_t producerPid;         // +36
	uint32_t latest;              // +40  newest COMPLETE slot, kNoSlot before the first frame
	uint32_t frameSeq;            // +44  frameSeq of `latest` (1, 2, 3, ... per published frame)
	uint32_t flags;               // +48  kFlag*
	uint32_t heartbeatMs;         // +52  producer GetTickCount (low 32 bits) at the last publish
	uint32_t framesPublished;     // +56
	uint32_t framesDropped;       // +60  frames the producer skipped (copy busy / too slow)
	uint32_t lastCopyUs;          // +64  cost of the last readback + copy, microseconds
	char     producer[32];        // +68  "pchost <build>" / "mock", NUL-terminated
	uint8_t  _reserved[156];      // +100
};
static_assert(sizeof(FrameRingHeader) == kHeaderBytes, "FrameRingHeader size");
static_assert(offsetof(FrameRingHeader, layerCapacity) == 28 && offsetof(FrameRingHeader, latest) == 40 &&
              offsetof(FrameRingHeader, frameSeq) == 44 && offsetof(FrameRingHeader, flags) == 48 &&
              offsetof(FrameRingHeader, producer) == 68 && offsetof(FrameRingHeader, _reserved) == 100, "FrameRingHeader offsets");

// The camera and stage-effect state the game APPLIED to this frame (what Hantei-chan needs to draw the stage behind L1
// exactly where the game would have). Game units: world x/y are 1/128 px (MBAA 0x55DEC4 / 0x55DEC8); the screen mapping
// is the game's Camera_UpdateMatrices: screen = (world - cam) / 128 * zoom + (320, 432) inside the 640x480 view.
struct FrameCamera {              // 64 B
	int32_t  cameraX, cameraY;    // +0 +4   camera, 1/128 px
	uint32_t zoomX1000;           // +8      1000 = 1.0 (0 = unknown: treat as 1.0)
	int32_t  shakeX, shakeY;      // +12 +16 shake, screen px x1000; MBAACC exports 0: cameraX/Y is already the camera as drawn (shake included)
	int32_t  stageId;             // +20     BgList id on screen (-1 none)
	uint32_t stageColorValX1000;  // +24     StageColorVal (BgPointBlur fColorHosei) x1000
	uint32_t stageLightArgb;      // +28     the stage light colour the game applied (0 = none / stock)
	uint32_t superFlashDarkX1000; // +32     super-flash darkening 0..1000 (0 = none)
	uint32_t heatBlurX1000;       // +36     HEAT BgPointBlur fValue 0..1000 (0 = off)
	uint16_t viewX, viewY;        // +40     the 640x480 game picture inside the frame (sidebars / widescreen)
	uint16_t viewW, viewH;        // +44     (0 = the whole frame)
	uint32_t _reserved[4];        // +48
};
static_assert(sizeof(FrameCamera) == 64, "FrameCamera size");
static_assert(offsetof(FrameCamera, superFlashDarkX1000) == 32 && offsetof(FrameCamera, viewX) == 40, "FrameCamera offsets");

struct FrameLayer {               // 32 B
	uint32_t offset;              // +0  from the slot header's first byte to this layer's first pixel row
	uint16_t width, height;       // +4 +6
	uint32_t pitch;               // +8
	uint16_t format;              // +12 kFormat*
	uint8_t  kind;                // +14 kLayer*
	uint8_t  flags;               // +15 kLayerPremultiplied
	int16_t  x, y;                // +16 +18 placement inside the frame (width x height of the slot)
	uint32_t checksum;            // +20 FrameChecksum over the visible bytes when kFlagChecksums, else 0
	uint32_t _reserved[2];        // +24
};
static_assert(sizeof(FrameLayer) == 32, "FrameLayer size");
static_assert(offsetof(FrameLayer, format) == 12 && offsetof(FrameLayer, checksum) == 20, "FrameLayer offsets");

// One fighter as DRAWN in this frame (the overlay lines up with the picture, not with a later LinkState).
struct FrameActor {               // 16 B
	int32_t x, y;                 // +0 world position, 1/128 px
	int16_t pattern, frame;       // +8
	uint8_t exists;               // +12
	uint8_t facing;               // +13 0 = faces right, 1 = faces left (the box x mirror)
	uint8_t team;                 // +14
	uint8_t tagFlag;              // +15
};
static_assert(sizeof(FrameActor) == 16, "FrameActor size");

struct FrameSlotHeader {          // 384 B
	uint32_t seq;                 // +0   the seqlock (odd = being written)
	uint32_t frameSeq;            // +4   publish counter
	uint32_t gameFrame;           // +8   the game's frame counter (world timer) drawn
	uint32_t presentMs;           // +12  producer GetTickCount (low 32 bits) at Present: latency = now - presentMs
	uint16_t width, height;       // +16  the frame (the backbuffer); layers are placed inside it
	uint32_t flags;               // +20  kSlot*
	uint32_t layerCount;          // +24  0..layerCapacity
	uint32_t _pad;                // +28
	FrameCamera camera;           // +32
	FrameLayer layers[kMaxLayers];// +96
	FrameActor actors[4];         // +224 engine slots 0..3
	uint8_t  _reserved[96];       // +288
};
static_assert(sizeof(FrameSlotHeader) == kSlotHeaderBytes, "FrameSlotHeader size");
static_assert(offsetof(FrameSlotHeader, camera) == 32 && offsetof(FrameSlotHeader, layers) == 96 &&
              offsetof(FrameSlotHeader, actors) == 224 && offsetof(FrameSlotHeader, _reserved) == 288, "FrameSlotHeader offsets");

constexpr uint32_t PitchFor(uint32_t w) { return ((w * 4u) + 63u) & ~63u; }
constexpr uint32_t LayerStride(uint32_t maxW, uint32_t maxH) { return PitchFor(maxW) * maxH; }
constexpr uint32_t SlotStride(uint32_t maxW, uint32_t maxH, uint32_t layers) { return kSlotHeaderBytes + layers * LayerStride(maxW, maxH); }
constexpr uint32_t RingBytes(uint32_t maxW, uint32_t maxH, uint32_t layers, uint32_t slots)
{
	return kHeaderBytes + slots * SlotStride(maxW, maxH, layers);
}
// The offset of layer region k inside a slot.
constexpr uint32_t LayerRegion(uint32_t maxW, uint32_t maxH, uint32_t k) { return kSlotHeaderBytes + k * LayerStride(maxW, maxH); }

// GOLDEN VALUES (both sides pin them).
static_assert(PitchFor(640) == 2560 && LayerStride(640, 480) == 1228800, "golden 640 pitch / layer");
static_assert(RingBytes(640, 480, 1, 3) == 3687808, "golden 640x480, full frame only, 3 slots");
static_assert(RingBytes(640, 480, 3, 3) == 11060608, "golden 640x480, full + chars + hud, 3 slots");
static_assert(PitchFor(854) == 3456 && RingBytes(854, 480, 3, 3) == 14931328, "golden 854x480 (16:9 sidebars), 3 layers");
static_assert(RingBytes(kMaxWidth, kMaxHeight, kMaxLayers, kMaxSlots) == 110593408, "golden max ring");

// FNV-1a 32 over the visible bytes only (width * 4 per row, pitch padding excluded).
inline uint32_t FrameChecksum(const uint8_t* px, uint32_t w, uint32_t h, uint32_t pitch)
{
	uint32_t hash = 2166136261u;
	for (uint32_t y = 0; y < h; ++y) {
		const uint8_t* row = px + (size_t)y * pitch;
		for (uint32_t k = 0; k < w * 4; ++k) { hash ^= row[k]; hash *= 16777619u; }
	}
	return hash;
}

inline FrameRingHeader* Header(void* base) { return (FrameRingHeader*)base; }
inline const FrameRingHeader* Header(const void* base) { return (const FrameRingHeader*)base; }
inline uint8_t* SlotBase(void* base, uint32_t i) { return (uint8_t*)base + kHeaderBytes + (size_t)i * Header(base)->slotStride; }
inline const uint8_t* SlotBase(const void* base, uint32_t i) { return (const uint8_t*)base + kHeaderBytes + (size_t)i * Header(base)->slotStride; }
inline FrameSlotHeader* Slot(void* base, uint32_t i) { return (FrameSlotHeader*)SlotBase(base, i); }
inline const FrameSlotHeader* Slot(const void* base, uint32_t i) { return (const FrameSlotHeader*)SlotBase(base, i); }

// The memory ordering primitives (GCC / Clang builtins; a 4-byte aligned u32 in shared memory).
inline uint32_t LoadAcquire(const uint32_t* p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
inline void StoreRelease(uint32_t* p, uint32_t v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
inline void FenceAcquire() { __atomic_thread_fence(__ATOMIC_ACQUIRE); }

// Initialise a ring in `base` (RingBytes(...) bytes).
inline bool InitRing(void* base, size_t bytes, uint32_t maxW, uint32_t maxH, uint32_t layers, uint32_t slots, uint32_t pid,
                     const char* producer)
{
	if (slots < kMinSlots || slots > kMaxSlots || layers < 1 || layers > kMaxLayers || maxW == 0 || maxH == 0 ||
	    maxW > kMaxWidth || maxH > kMaxHeight)
		return false;
	if (bytes < RingBytes(maxW, maxH, layers, slots)) return false;
	std::memset(base, 0, RingBytes(maxW, maxH, layers, slots));
	FrameRingHeader* h = Header(base);
	h->version = kVersion;
	h->headerBytes = (uint16_t)kHeaderBytes;
	h->slotCount = slots;
	h->maxWidth = maxW;
	h->maxHeight = maxH;
	h->maxPitch = PitchFor(maxW);
	h->layerCapacity = layers;
	h->layerStride = LayerStride(maxW, maxH);
	h->slotStride = SlotStride(maxW, maxH, layers);
	h->producerPid = pid;
	h->latest = kNoSlot;
	std::strncpy(h->producer, producer ? producer : "", sizeof h->producer - 1);
	StoreRelease(&h->magic, kMagic);   // last: a reader that sees the magic sees an initialised header
	return true;
}

// ---- the writer (one per ring) ----
// BeginWrite: the slot to fill (seq made odd). Fill the header fields (not seq), the layer table (AddLayer) and the
// pixels, then Publish.
inline FrameSlotHeader* BeginWrite(void* base, uint32_t& slotOut)
{
	FrameRingHeader* h = Header(base);
	const uint32_t latest = LoadAcquire(&h->latest);
	const uint32_t w = latest == kNoSlot ? 0 : (latest + 1) % h->slotCount;
	FrameSlotHeader* s = Slot(base, w);
	StoreRelease(&s->seq, s->seq + 1);   // odd: being written
	s->layerCount = 0;
	slotOut = w;
	return s;
}
// Reserve the next layer region; returns its first pixel row (nullptr when the capacity is used up or it does not fit).
inline uint8_t* AddLayer(void* base, FrameSlotHeader* s, uint8_t kind, uint16_t format, uint8_t flags, uint16_t w, uint16_t h,
                         int16_t x = 0, int16_t y = 0)
{
	const FrameRingHeader* rh = Header(base);
	if (s->layerCount >= rh->layerCapacity || w > rh->maxWidth || h > rh->maxHeight) return nullptr;
	FrameLayer& l = s->layers[s->layerCount];
	l = FrameLayer{};
	l.offset = LayerRegion(rh->maxWidth, rh->maxHeight, s->layerCount);
	l.width = w; l.height = h; l.pitch = PitchFor(w); l.format = format; l.kind = kind; l.flags = flags; l.x = x; l.y = y;
	++s->layerCount;
	return (uint8_t*)s + l.offset;
}
inline void Publish(void* base, uint32_t slot, uint32_t nowMs)
{
	FrameRingHeader* h = Header(base);
	FrameSlotHeader* s = Slot(base, slot);
	StoreRelease(&s->seq, s->seq + 1);   // even: complete
	StoreRelease(&h->latest, slot);
	StoreRelease(&h->frameSeq, s->frameSeq);
	h->heartbeatMs = nowMs;
	++h->framesPublished;
}

// ---- the reader ----
enum class ReadResult { Ok, NoRing, NoFrame, Unchanged, Torn, BadHeader };
inline const char* ReadResultName(ReadResult r)
{
	switch (r) {
	case ReadResult::Ok: return "ok";
	case ReadResult::NoRing: return "no ring";
	case ReadResult::NoFrame: return "no frame yet";
	case ReadResult::Unchanged: return "unchanged";
	case ReadResult::Torn: return "torn (writer lapped the reader)";
	case ReadResult::BadHeader: return "bad header (version / geometry)";
	}
	return "?";
}
// Validate a mapped ring (magic, version, geometry fits in `bytes`).
inline bool HeaderValid(const void* base, size_t bytes)
{
	if (!base || bytes < kHeaderBytes) return false;
	const FrameRingHeader* h = Header(base);
	if (LoadAcquire(&h->magic) != kMagic || h->version != kVersion || h->headerBytes != kHeaderBytes) return false;
	if (h->slotCount < kMinSlots || h->slotCount > kMaxSlots || h->layerCapacity < 1 || h->layerCapacity > kMaxLayers) return false;
	if (h->maxWidth == 0 || h->maxHeight == 0 || h->maxWidth > kMaxWidth || h->maxHeight > kMaxHeight) return false;
	if (h->maxPitch != PitchFor(h->maxWidth) || h->layerStride != LayerStride(h->maxWidth, h->maxHeight) ||
	    h->slotStride != SlotStride(h->maxWidth, h->maxHeight, h->layerCapacity))
		return false;
	return bytes >= RingBytes(h->maxWidth, h->maxHeight, h->layerCapacity, h->slotCount);
}
// A layer's geometry is inside its slot and its region.
inline bool LayerValid(const FrameRingHeader& h, const FrameLayer& l, uint32_t index)
{
	if (l.width > h.maxWidth || l.height > h.maxHeight || l.pitch > h.maxPitch || l.pitch < (uint32_t)l.width * 4u) return false;
	if (l.format != kFormatBGRX8 && l.format != kFormatBGRA8) return false;
	const uint32_t region = LayerRegion(h.maxWidth, h.maxHeight, index);
	return l.offset >= region && (uint64_t)l.offset + (uint64_t)l.pitch * l.height <= (uint64_t)region + h.layerStride;
}
// Copy the newest complete frame whose frameSeq != `lastSeq` into `out`: a buffer of at least slotStride bytes that
// receives the slot header and the used layers at their slot offsets (so out + layers[k].offset = layer k).
inline ReadResult ReadLatest(const void* base, size_t bytes, uint32_t lastSeq, uint8_t* out, size_t cap, int tries = 4)
{
	if (!base) return ReadResult::NoRing;
	if (!HeaderValid(base, bytes)) return ReadResult::BadHeader;
	const FrameRingHeader* h = Header(base);
	if (cap < h->slotStride) return ReadResult::BadHeader;
	for (int t = 0; t < tries; ++t) {
		const uint32_t i = LoadAcquire(&h->latest);
		if (i == kNoSlot) return ReadResult::NoFrame;
		if (i >= h->slotCount) return ReadResult::BadHeader;
		const uint8_t* sb = SlotBase(base, i);
		const FrameSlotHeader* s = (const FrameSlotHeader*)sb;
		const uint32_t s1 = LoadAcquire(&s->seq);
		if (s1 & 1u) continue;
		std::memcpy(out, sb, kSlotHeaderBytes);
		FrameSlotHeader* o = (FrameSlotHeader*)out;
		if (lastSeq != 0 && o->frameSeq == lastSeq) {
			FenceAcquire();
			if (LoadAcquire(&s->seq) == s1) return ReadResult::Unchanged;
			continue;
		}
		bool ok = o->layerCount <= h->layerCapacity;
		for (uint32_t k = 0; ok && k < o->layerCount; ++k) {
			const FrameLayer& l = o->layers[k];
			ok = LayerValid(*h, l, k);
			if (ok) std::memcpy(out + l.offset, sb + l.offset, (size_t)l.pitch * l.height);
		}
		FenceAcquire();
		if (LoadAcquire(&s->seq) != s1) continue;   // lapped: the copy may be torn
		if (!ok) return ReadResult::BadHeader;
		o->seq = s1;
		return ReadResult::Ok;
	}
	return ReadResult::Torn;
}
inline const FrameLayer* FindLayer(const FrameSlotHeader& s, uint8_t kind)
{
	for (uint32_t k = 0; k < s.layerCount && k < kMaxLayers; ++k) if (s.layers[k].kind == kind) return &s.layers[k];
	return nullptr;
}

// ---- compositing (CPU reference; the panel does the same on the GPU with glBlendFunc(ONE, ONE_MINUS_SRC_ALPHA)) ----
// dst (opaque BGRX, w x h) = src (premultiplied BGRA) OVER dst, src placed at (sx, sy).
inline void CompositePremulOver(uint8_t* dst, uint32_t dw, uint32_t dh, uint32_t dpitch, const uint8_t* src, uint32_t sw,
                                uint32_t sh, uint32_t spitch, int sx, int sy)
{
	for (uint32_t y = 0; y < sh; ++y) {
		const int ty = sy + (int)y;
		if (ty < 0 || ty >= (int)dh) continue;
		for (uint32_t x = 0; x < sw; ++x) {
			const int tx = sx + (int)x;
			if (tx < 0 || tx >= (int)dw) continue;
			const uint8_t* s = src + (size_t)y * spitch + x * 4;
			uint8_t* d = dst + (size_t)ty * dpitch + (size_t)tx * 4;
			const uint32_t ia = 255u - s[3];
			for (int c = 0; c < 3; ++c) {
				const uint32_t v = s[c] + (d[c] * ia + 127u) / 255u;
				d[c] = (uint8_t)(v > 255u ? 255u : v);
			}
			d[3] = 0xFF;
		}
	}
}

// ---- the deterministic test content (the mock producer; the headless check recomputes it) ----
// A camera that moves: a pure function of frameSeq (1/128 px, inside the game's camera range).
inline void TestCamera(uint32_t frameSeq, FrameCamera& c)
{
	c = FrameCamera{};
	const int32_t tri = (int32_t)(frameSeq % 400u);                 // a triangle wave 0..200..0 px
	c.cameraX = ((tri < 200 ? tri : 400 - tri) - 100) * 128;
	c.cameraY = -(int32_t)((frameSeq / 3u) % 60u) * 128;
	c.zoomX1000 = 1000;
	c.shakeX = (frameSeq % 90u) < 6u ? 2000 : 0;                    // a 6-frame shake every 1.5 s
	c.stageId = 16;
	c.stageColorValX1000 = 650;
	c.superFlashDarkX1000 = (frameSeq % 240u) < 30u ? 600u : 0u;
	c.viewW = 640; c.viewH = 480;
}
// The test STAGE: world-anchored stripes, drawn at the camera (what Hantei-chan's own renderer stands in for).
inline void DrawTestStage(uint8_t* px, uint32_t w, uint32_t h, uint32_t pitch, const FrameCamera& c)
{
	const int32_t cx = c.cameraX / 128, cy = c.cameraY / 128, shake = c.shakeX / 1000;
	for (uint32_t y = 0; y < h; ++y) {
		uint8_t* row = px + (size_t)y * pitch;
		const int32_t wy = (int32_t)y - 432 + cy;                   // world y of this row
		for (uint32_t x = 0; x < w; ++x) {
			const int32_t wx = (int32_t)x - 320 + cx - shake;       // world x of this column
			uint8_t* p = row + x * 4;
			const bool floor = wy >= 0;
			const uint32_t band = (uint32_t)(((wx + 100000) / 40) % 2);
			p[0] = floor ? (uint8_t)(60 + band * 30) : (uint8_t)(150 + band * 40);
			p[1] = floor ? (uint8_t)(90 + band * 30) : (uint8_t)110;
			p[2] = floor ? (uint8_t)60 : (uint8_t)(70 + ((uint32_t)(-wy) % 64u));
			p[3] = 0xFF;
		}
		for (uint32_t k = w * 4; k < pitch; ++k) row[k] = 0;
	}
}
// The test CHARS layer (premultiplied): two soft-edged fighters at world x = +-(80 + frameSeq % 60) px on the ground,
// placed with the camera, over transparent.
inline void DrawTestChars(uint8_t* px, uint32_t w, uint32_t h, uint32_t pitch, const FrameCamera& c, uint32_t frameSeq)
{
	std::memset(px, 0, (size_t)pitch * h);
	const int32_t cx = c.cameraX / 128, cy = c.cameraY / 128, shake = c.shakeX / 1000;
	const int32_t off = 80 + (int32_t)(frameSeq % 60u);
	const int32_t fx[2] = { -off, off };
	const uint8_t col[2][3] = { { 60, 60, 230 }, { 230, 90, 40 } };   // B,G,R
	for (int f = 0; f < 2; ++f) {
		const int32_t sx = fx[f] - cx + 320 + shake, sy = 0 - cy + 432;   // feet on screen
		for (int32_t y = sy - 120; y < sy; ++y)
			for (int32_t x = sx - 30; x < sx + 30; ++x) {
				if (x < 0 || y < 0 || x >= (int32_t)w || y >= (int32_t)h) continue;
				const int32_t edge = std::min(std::min(x - (sx - 30), (sx + 29) - x), std::min(y - (sy - 120), (sy - 1) - y));
				const uint32_t a = edge >= 8 ? 255u : (uint32_t)(32 + edge * 28);
				uint8_t* p = px + (size_t)y * pitch + (size_t)x * 4;
				for (int k = 0; k < 3; ++k) p[k] = (uint8_t)((col[f][k] * a + 127u) / 255u);
				p[3] = (uint8_t)a;
			}
	}
}
// The test HUD layer (premultiplied): two health bars + a frame counter strip.
inline void DrawTestHud(uint8_t* px, uint32_t w, uint32_t h, uint32_t pitch, uint32_t frameSeq)
{
	std::memset(px, 0, (size_t)pitch * h);
	auto fill = [&](uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
		for (uint32_t y = y0; y < y1 && y < h; ++y)
			for (uint32_t x = x0; x < x1 && x < w; ++x) {
				uint8_t* p = px + (size_t)y * pitch + (size_t)x * 4;
				p[0] = (uint8_t)((b * a + 127u) / 255u); p[1] = (uint8_t)((g * a + 127u) / 255u);
				p[2] = (uint8_t)((r * a + 127u) / 255u); p[3] = a;
			}
	};
	const uint32_t hp = 40 + frameSeq % 200u;
	fill(40, 30, 40 + 240, 44, 30, 30, 30, 200);
	fill(40 + 240 - hp, 30, 40 + 240, 44, 40, 200, 240, 255);
	fill(w - 40 - 240, 30, w - 40, 44, 30, 30, 30, 200);
	fill(w - 40 - 240, 30, w - 40 - 240 + hp, 44, 40, 200, 240, 255);
	for (uint32_t b = 0; b < 16; ++b)   // frameSeq in binary, MSB first, 12 px cells at the bottom
		fill(20 + b * 12, h - 16, 20 + b * 12 + 10, h - 6, 255, 255, 255, ((frameSeq >> (15 - b)) & 1u) ? 255 : 64);
}
// The FULL frame the mock publishes: test stage, then chars, then HUD (exactly what a layered reader must rebuild).
// `scratch` = a pitch * h buffer.
inline void DrawTestFull(uint8_t* px, uint32_t w, uint32_t h, uint32_t pitch, uint32_t frameSeq, uint8_t* scratch)
{
	FrameCamera c;
	TestCamera(frameSeq, c);
	DrawTestStage(px, w, h, pitch, c);
	DrawTestChars(scratch, w, h, pitch, c, frameSeq);
	CompositePremulOver(px, w, h, pitch, scratch, w, h, pitch, 0, 0);
	DrawTestHud(scratch, w, h, pitch, frameSeq);
	CompositePremulOver(px, w, h, pitch, scratch, w, h, pitch, 0, 0);
}

} // namespace framering

#endif
