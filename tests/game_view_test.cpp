// game_view_test: the embedded game view (docs/HANTEI_AUTHORING_MODE.md §12 / §12.1 / §12.2), HEADLESS (no window):
//   * the frame ring layout: sizes, offsets and golden ring sizes (the same values pchost must pin);
//   * the seqlock: write / read, Unchanged, NoFrame, a stuck writer (Torn), a bad header, and a concurrent writer that
//     laps the reader thousands of times without one torn frame reaching the caller (per-layer checksums);
//   * layered compositing: stage (at the frame's camera) + CHARS + HUD, premultiplied, rebuilds FULL byte for byte;
//   * the link: LinkFrameShare / LinkInputInject / LinkStageLighting layouts;
//   * END TO END against the mock DLL's fake producer: QueryFrameShare -> open the mapping by name -> decode frames
//     (checksum per frame, and each FULL frame recomputed from its frameSeq), SetEmbedded 2 -> layered frames that
//     rebuild FULL, InputInject -> slot 0 moves in the next frames, a session refuses the inject.
#include "game_frame_ring.h"
#include "game_link.h"
#include "game_link_mock.h"
#include "authoring/game_view_input.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

using namespace framering;
namespace wire = gamelink::wire;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)
#define CHECKM(c, m) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s  (%s)\n", __FILE__, __LINE__, #c, std::string(m).c_str()); ++g_fail; } } while (0)

static void TestLayout()
{
	CHECK(sizeof(FrameRingHeader) == 256 && sizeof(FrameSlotHeader) == 384 && sizeof(FrameLayer) == 32);
	CHECK(sizeof(FrameCamera) == 64 && sizeof(FrameActor) == 16);
	CHECK(RingBytes(640, 480, 1, 3) == 3687808u && RingBytes(640, 480, 3, 3) == 11060608u);
	CHECK(RingBytes(854, 480, 3, 3) == 14931328u && RingBytes(1920, 1200, 4, 3) == 110593408u);
	CHECK(LayerRegion(640, 480, 0) == 384 && LayerRegion(640, 480, 2) == 384 + 2 * 1228800);
	CHECK(kMagic == 0x53464350u && std::memcmp(&kMagic, "PCFS", 4) == 0);
	CHECK(sizeof(wire::FrameShare) == 96 && sizeof(wire::InputInject) == 16 && sizeof(wire::StageLighting) == 16);
	CHECK((int)wire::Op::QueryFrameShare == 16 && (int)wire::Op::SetEmbedded == 17 && (int)wire::Op::InputInject == 18 &&
	      (int)wire::Op::SetStageLighting == 19 && (int)wire::Kind::LinkFrameShare == 0x10A);
	CHECK(wire::kCapFrameShare == 0x100 && wire::kCapInputInject == 0x200);
	CHECK(MappingName(4242) == "Local\\povertycaster-frames-4242");
	// a golden 640x480 test frame checksum (both sides can pin it: the pattern is a pure function of frameSeq)
	std::vector<uint8_t> px(PitchFor(640) * 480), scratch(px.size());
	DrawTestFull(px.data(), 640, 480, PitchFor(640), 1, scratch.data());
	const uint32_t golden1 = FrameChecksum(px.data(), 640, 480, PitchFor(640));
	DrawTestFull(px.data(), 640, 480, PitchFor(640), 1, scratch.data());
	CHECK(FrameChecksum(px.data(), 640, 480, PitchFor(640)) == golden1);
	CHECK(golden1 == 0x1AF20C3Du);   // pinned: DrawTestFull(640x480, frameSeq 1)
	std::printf("golden test frame 1 checksum: 0x%08X\n", (unsigned)golden1);
}

// One test frame into a ring (the mock producer's logic, in-process).
static void WriteFrame(void* base, uint32_t n, bool layered, std::vector<uint8_t>& scratch)
{
	const FrameRingHeader* h = Header(base);
	const uint32_t w = h->maxWidth, hh = h->maxHeight, pitch = PitchFor(w);
	uint32_t slot = 0;
	FrameSlotHeader* s = BeginWrite(base, slot);
	s->frameSeq = n;
	s->width = (uint16_t)w; s->height = (uint16_t)hh;
	TestCamera(n, s->camera);
	uint8_t* full = AddLayer(base, s, kLayerFull, kFormatBGRX8, 0, (uint16_t)w, (uint16_t)hh);
	DrawTestFull(full, w, hh, pitch, n, scratch.data());
	s->layers[0].checksum = FrameChecksum(full, w, hh, pitch);
	if (layered) {
		uint8_t* ch = AddLayer(base, s, kLayerChars, kFormatBGRA8, kLayerPremultiplied, (uint16_t)w, (uint16_t)hh);
		DrawTestChars(ch, w, hh, pitch, s->camera, n);
		s->layers[1].checksum = FrameChecksum(ch, w, hh, pitch);
		uint8_t* hud = AddLayer(base, s, kLayerHud, kFormatBGRA8, kLayerPremultiplied, (uint16_t)w, (uint16_t)hh);
		DrawTestHud(hud, w, hh, pitch, n);
		s->layers[2].checksum = FrameChecksum(hud, w, hh, pitch);
		s->flags = kSlotHasFull | kSlotLayered;
	}
	Publish(base, slot, 0);
}

static bool LayersIntact(const uint8_t* frame)
{
	const FrameSlotHeader& f = *(const FrameSlotHeader*)frame;
	for (uint32_t k = 0; k < f.layerCount; ++k) {
		const FrameLayer& l = f.layers[k];
		if (FrameChecksum(frame + l.offset, l.width, l.height, l.pitch) != l.checksum) return false;
	}
	return true;
}

// Rebuild FULL from the layers: stage at the frame's camera, CHARS over it, HUD over that.
static bool LayeredRebuildsFull(const uint8_t* frame)
{
	const FrameSlotHeader& f = *(const FrameSlotHeader*)frame;
	const FrameLayer* full = FindLayer(f, kLayerFull);
	const FrameLayer* ch = FindLayer(f, kLayerChars);
	const FrameLayer* hud = FindLayer(f, kLayerHud);
	if (!full || !ch || !hud) return false;
	std::vector<uint8_t> px((size_t)full->pitch * full->height);
	DrawTestStage(px.data(), full->width, full->height, full->pitch, f.camera);
	CompositePremulOver(px.data(), full->width, full->height, full->pitch, frame + ch->offset, ch->width, ch->height, ch->pitch, ch->x, ch->y);
	CompositePremulOver(px.data(), full->width, full->height, full->pitch, frame + hud->offset, hud->width, hud->height, hud->pitch, hud->x, hud->y);
	return FrameChecksum(px.data(), full->width, full->height, full->pitch) == full->checksum;
}

static void TestSeqlock()
{
	const uint32_t W = 160, H = 120;
	std::vector<uint8_t> mem(RingBytes(W, H, 3, 3)), frame(SlotStride(W, H, 3)), scratch(PitchFor(W) * H);
	void* base = mem.data();
	CHECK(ReadLatest(base, mem.size(), 0, frame.data(), frame.size()) == ReadResult::BadHeader);   // no magic yet
	CHECK(!InitRing(base, mem.size(), W, H, 5, 3, 1, "t"));                                         // too many layers
	CHECK(!InitRing(base, mem.size() - 1, W, H, 3, 3, 1, "t"));                                     // too small
	CHECK(InitRing(base, mem.size(), W, H, 3, 3, 1234, "test"));
	CHECK(HeaderValid(base, mem.size()) && !HeaderValid(base, mem.size() - 1));
	CHECK(ReadLatest(base, mem.size(), 0, frame.data(), frame.size()) == ReadResult::NoFrame);
	WriteFrame(base, 1, false, scratch);
	CHECK(ReadLatest(base, mem.size(), 0, frame.data(), frame.size()) == ReadResult::Ok);
	const FrameSlotHeader& f = *(const FrameSlotHeader*)frame.data();
	CHECK(f.frameSeq == 1 && f.layerCount == 1 && LayersIntact(frame.data()) && (f.seq & 1u) == 0);
	CHECK(ReadLatest(base, mem.size(), 1, frame.data(), frame.size()) == ReadResult::Unchanged);
	CHECK(ReadLatest(base, mem.size(), 0, frame.data(), frame.size() - 1) == ReadResult::BadHeader);   // buffer too small
	WriteFrame(base, 2, true, scratch);
	CHECK(ReadLatest(base, mem.size(), 1, frame.data(), frame.size()) == ReadResult::Ok);
	CHECK(f.frameSeq == 2 && f.layerCount == 3 && LayersIntact(frame.data()) && LayeredRebuildsFull(frame.data()));
	CHECK(Header(base)->latest == 1 && Header(base)->framesPublished == 2);
	// the writer never reuses the latest slot: three publishes walk 0 -> 1 -> 2 -> 0
	WriteFrame(base, 3, false, scratch);
	WriteFrame(base, 4, false, scratch);
	CHECK(Header(base)->latest == 0);
	// a writer that died mid-frame (seq stays odd): the reader gives up (Torn), never returns half a frame
	uint32_t slot = 0;
	FrameSlotHeader* s = BeginWrite(base, slot);   // slot 1, odd
	s->frameSeq = 5;
	StoreRelease(&Header(base)->latest, slot);    // a broken producer published it anyway
	CHECK(ReadLatest(base, mem.size(), 4, frame.data(), frame.size()) == ReadResult::Torn);
	// a corrupt layer table is refused
	StoreRelease(&s->seq, s->seq + 1);
	s->layerCount = 1;
	s->layers[0] = FrameLayer{};
	s->layers[0].offset = 10; s->layers[0].width = W; s->layers[0].height = H; s->layers[0].pitch = PitchFor(W); s->layers[0].format = kFormatBGRX8;
	CHECK(ReadLatest(base, mem.size(), 4, frame.data(), frame.size()) == ReadResult::BadHeader);
	Header(base)->version = 2;
	CHECK(ReadLatest(base, mem.size(), 0, frame.data(), frame.size()) == ReadResult::BadHeader);
}

// A writer publishing as fast as it can, a reader polling: every frame the reader gets must be intact.
static void TestConcurrent()
{
	const uint32_t W = 64, H = 48;
	std::vector<uint8_t> mem(RingBytes(W, H, 3, 3));
	void* base = mem.data();
	InitRing(base, mem.size(), W, H, 3, 3, 1, "stress");
	std::atomic<bool> done{false};
	std::thread writer([&] {
		std::vector<uint8_t> scratch(PitchFor(W) * H);
		for (uint32_t n = 1; n <= 20000; ++n) WriteFrame(base, n, (n % 3) == 0, scratch);
		done = true;
	});
	std::vector<uint8_t> frame(SlotStride(W, H, 3));
	uint32_t last = 0, ok = 0, torn = 0, bad = 0, backwards = 0, layered = 0, rebuilt = 0;
	while (!done) {
		const ReadResult r = ReadLatest(base, mem.size(), last, frame.data(), frame.size(), 2);
		if (r == ReadResult::Torn) { ++torn; continue; }
		if (r != ReadResult::Ok) continue;
		const FrameSlotHeader& f = *(const FrameSlotHeader*)frame.data();
		if (!LayersIntact(frame.data())) ++bad;
		if (f.frameSeq < last) ++backwards;
		if (f.layerCount == 3) { ++layered; if (LayeredRebuildsFull(frame.data())) ++rebuilt; }
		last = f.frameSeq;
		++ok;
	}
	writer.join();
	std::printf("concurrent: %u frames read intact, %u torn attempts retried, %u layered (%u rebuilt FULL)\n", ok, torn, layered, rebuilt);
	CHECK(ok > 50 && bad == 0 && backwards == 0 && layered > 0 && rebuilt == layered);
}

static void TestInputMapping()
{
	using authoring::GameViewKeys;
	GameViewKeys k;
	CHECK(authoring::NumpadDirection(k) == 5);
	k.right = true;
	CHECK(authoring::NumpadDirection(k) == 6);
	k.down = true;
	CHECK(authoring::NumpadDirection(k) == 3);
	k.left = true;                      // left + right cancel (SOCD neutral), down stays
	CHECK(authoring::NumpadDirection(k) == 2);
	k = GameViewKeys{};
	k.up = k.left = true;
	CHECK(authoring::NumpadDirection(k) == 7);
	k.up = k.down = true; k.left = false;   // up + down cancel
	CHECK(authoring::NumpadDirection(k) == 5);
	k = GameViewKeys{};
	k.a = k.c = k.e = true;
	CHECK(authoring::InjectButtons(k) == (wire::kInjBtnA | wire::kInjBtnC | wire::kInjBtnE));
	const wire::InputInject in = authoring::MakeInject(k, 2, 7, 12);
	CHECK(in.version == 1 && in.player == 2 && in.direction == 5 && in.buttons == 0x15 && in.holdFrames == 12 && in.serial == 7);
	const wire::InputInject rel = authoring::MakeRelease(1, 8);
	CHECK((rel.flags & wire::kInjFlagRelease) && rel.direction == 5 && rel.buttons == 0);
}

template <class P>
static bool Wait(P pred, int ms) { for (int k = 0; k < ms / 10; ++k) { if (pred()) return true; Sleep(10); } return pred(); }

static void TestMockEndToEnd()
{
	gamelink::MockDll::Options o;
	o.frames = true;
	o.fps = 60;
	gamelink::MockDll mock(o);
	CHECK(mock.Start());
	gamelink::Client c;
	c.SetTargetPid(mock.Pid());
	c.SetPollHz(10);
	c.Connect();
	CHECK(c.WaitCaps(3000));
	CHECK((c.Get().caps.caps & wire::kCapFrameShare) && (c.Get().caps.caps & wire::kCapInputInject));
	c.QueryFrameShare();
	CHECK(Wait([&] { return c.Get().haveFrameShare; }, 3000));
	const wire::FrameShare fs = c.Get().frameShare;
	CHECK(fs.version == kVersion && fs.slotCount == 3 && fs.layerCapacity == 3 && fs.maxWidth == 640 && fs.maxHeight == 480);
	CHECK(std::string(fs.name) == MappingName(mock.Pid()) && fs.ringBytes == RingBytes(640, 480, 3, 3));
	Reader r;
	std::string why;
	CHECKM(r.Open(fs.name, &why), why);
	// 1. full frames: each decodes, its checksum matches, and it equals the pattern recomputed from its frameSeq
	std::vector<uint8_t> expect(PitchFor(640) * 480), scratch(expect.size());
	uint32_t got = 0, recomputed = 0, lastSeq = 0, monotonic = 1;
	const uint64_t t0 = GetTickCount64();
	while (got < 60 && GetTickCount64() - t0 < 5000) {
		if (r.Poll() != ReadResult::Ok) { Sleep(2); continue; }
		const FrameSlotHeader& f = r.Frame();
		const FrameLayer* full = FindLayer(f, kLayerFull);
		if (!full) continue;
		const uint32_t sum = FrameChecksum(r.LayerPixels(*full), full->width, full->height, full->pitch);
		CHECKM(sum == full->checksum, "frame " + std::to_string(f.frameSeq));
		if (got % 10 == 0) {
			DrawTestFull(expect.data(), 640, 480, PitchFor(640), f.frameSeq, scratch.data());
			recomputed += FrameChecksum(expect.data(), 640, 480, PitchFor(640)) == sum;
		}
		if (f.frameSeq <= lastSeq) monotonic = 0;
		lastSeq = f.frameSeq;
		++got;
	}
	const uint32_t elapsed = (uint32_t)(GetTickCount64() - t0);
	std::printf("mock producer: %u frames decoded in %u ms (%.1f fps), %u skipped, %u torn retries, latency %u ms\n", got, elapsed,
	            got * 1000.0f / (elapsed ? elapsed : 1), r.Skipped(), r.Torn(), r.LatencyMs());
	CHECK(got == 60 && recomputed == 6 && monotonic);
	CHECK(r.Ring()->flags & kFlagProducerAlive);
	// 2. layered: SetEmbedded 2 -> frames with CHARS + HUD that rebuild FULL, the camera moving between them
	wire::Reply rep{};
	uint16_t seq = c.SetEmbedded(2);
	CHECK(c.WaitReply(seq, 2000, rep) && rep.status == 0);
	uint32_t layered = 0, rebuilt = 0;
	int32_t camMin = 1 << 30, camMax = -(1 << 30);
	const uint64_t t1 = GetTickCount64();
	while (layered < 40 && GetTickCount64() - t1 < 5000) {
		if (r.Poll() != ReadResult::Ok) { Sleep(2); continue; }
		const FrameSlotHeader& f = r.Frame();
		if (!(f.flags & kSlotLayered)) continue;
		++layered;
		// rebuild through the reader's own buffer (the slot header + layers at their offsets)
		rebuilt += LayeredRebuildsFull((const uint8_t*)&f);
		camMin = std::min(camMin, f.camera.cameraX);
		camMax = std::max(camMax, f.camera.cameraX);
	}
	std::printf("layered: %u frames, %u rebuilt FULL from stage + CHARS + HUD, camera x %d..%d\n", layered, rebuilt, camMin, camMax);
	CHECK(layered == 40 && rebuilt == 40 && camMax > camMin);
	CHECK(r.Ring()->flags & kFlagLayered);
	// 3. input: hold 6 (forward) on P1 for 30 frames: slot 0 moves right by 4 px a frame in the frames that follow
	r.Poll();
	const int32_t x0 = r.Frame().actors[0].x - (-(80 + (int32_t)(r.Frame().frameSeq % 60u))) * 128;   // the injected offset so far
	authoring::GameViewKeys k;
	k.right = true;
	seq = c.InputInject(authoring::MakeInject(k, 0, 1, 30));
	CHECK(seq && c.WaitReply(seq, 2000, rep) && rep.status == 0 && std::string(rep.message).find("dir 6") != std::string::npos);
	Sleep(700);
	for (int k2 = 0; k2 < 50 && r.Poll() != ReadResult::Ok; ++k2) Sleep(5);
	const int32_t x1 = r.Frame().actors[0].x - (-(80 + (int32_t)(r.Frame().frameSeq % 60u))) * 128;
	std::printf("input: slot 0 offset %d -> %d (1/128 px) after 30 frames of 6\n", x0, x1);
	CHECK(x1 - x0 == 30 * 4 * 128 && mock.Injects() == 1);
	seq = c.InputInject(authoring::MakeRelease(0, 2));
	CHECK(c.WaitReply(seq, 2000, rep) && rep.status == 0);
	// stage lighting reaches the next frames' camera block
	wire::StageLighting sl{};
	sl.stageId = -1; sl.lightArgb = 0xFF808040u;
	seq = c.SetStageLighting(sl);
	CHECK(seq && c.WaitReply(seq, 2000, rep) && rep.status == 0);
	CHECK(Wait([&] { r.Poll(); return r.Frame().camera.stageLightArgb == 0xFF808040u; }, 2000));
	seq = c.SetEmbedded(0);
	CHECK(c.WaitReply(seq, 2000, rep) && rep.status == 0);
	CHECK(Wait([&] { r.Poll(); return !(r.Ring()->flags & kFlagEmbedded); }, 2000));
	r.Close();
	c.Disconnect();
	mock.Stop();
	// 4. a session refuses input (and a DLL without the export answers Unknown: no frame share)
	gamelink::MockDll::Options so;
	so.frames = true;
	so.session = true;
	gamelink::MockDll sm(so);
	CHECK(sm.Start());
	gamelink::Client c2;
	c2.SetTargetPid(sm.Pid());
	c2.Connect();
	CHECK(c2.WaitCaps(3000));
	seq = c2.InputInject(authoring::MakeInject(authoring::GameViewKeys{}, 0, 1, 5));
	CHECK(seq && c2.WaitReply(seq, 2000, rep) && rep.status == (int16_t)wire::Status::RefusedSession);
	c2.Disconnect();
	sm.Stop();
	gamelink::MockDll::Options no;
	gamelink::MockDll nm(no);
	CHECK(nm.Start());
	gamelink::Client c3;
	c3.SetTargetPid(nm.Pid());
	c3.Connect();
	CHECK(c3.WaitCaps(3000) && !(c3.Get().caps.caps & wire::kCapFrameShare));
	CHECK(c3.InputInject(authoring::MakeInject(authoring::GameViewKeys{}, 0, 1, 5)) == 0);   // not even sent
	c3.QueryFrameShare();
	CHECK(Wait([&] { return c3.Get().frameShareUnknown; }, 2000));
	c3.Disconnect();
	nm.Stop();
}

int main()
{
	TestLayout();
	TestSeqlock();
	TestConcurrent();
	TestInputMapping();
	TestMockEndToEnd();
	std::printf(g_fail ? "game_view_test: %d of %d FAILED\n" : "game_view_test: all %d checks passed\n", g_fail ? g_fail : g_checks, g_checks);
	return g_fail ? 1 : 0;
}
