// [game-view] The Game panel — see game_view.h and docs/HANTEI_AUTHORING_MODE.md §12.2.
#include "game_view.h"
#include "authoring_state.h"
#include "game_view_input.h"
#include "../framedata.h"
#include "../game_frame_ring.h"

#include <glad/glad.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace authoring {

bool showGameView = false;

namespace {

namespace wire = gamelink::wire;
using namespace framering;

uint64_t NowMs() { return GetTickCount64(); }

struct Tex {
	GLuint id = 0;
	int w = 0, h = 0;
	// Upload `h` rows of `w` BGRA / BGRX pixels, `pitch` bytes apart.
	void Upload(const uint8_t* px, int width, int height, uint32_t pitch)
	{
		if (!id) {
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch / 4));
		if (width != w || height != h) {
			w = width; h = height;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, px);
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px);
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	ImTextureID Im() const { return (ImTextureID)(intptr_t)id; }
};

enum class StageSource { Hantei = 0, TestPattern = 1, Black = 2 };

struct GameViewState {
	Reader reader;
	std::string openedName, openWhy;
	uint64_t lastQueryMs = 0;
	Tex full, chars, hud, testStage;
	bool haveFrame = false;
	bool layeredView = false;            // what the panel composes (the game's mode is SetEmbedded)
	StageSource stageSource = StageSource::Hantei;
	bool overlayBoxes = true, overlayLabels = true, overlayHurt = true, overlayAttack = true, overlayOther = false;
	bool forwardInput = true;
	int player = 0;
	bool ownWindow = false;
	bool capturing = false;
	GameViewKeys lastKeys;
	uint64_t lastSendMs = 0;
	uint32_t serial = 0;
	bool released = true;
	std::vector<uint8_t> testStagePx;
	uint32_t uploads = 0;
	std::string status;
};

GameViewState& G() { static GameViewState s; return s; }

// ---- the premultiplied blend around an image (ImGui draw callbacks) ----
void BlendPremul(const ImDrawList*, const ImDrawCmd*) { glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); }

void UploadFrame(GameViewState& g)
{
	const FrameSlotHeader& f = g.reader.Frame();
	for (uint32_t k = 0; k < f.layerCount && k < kMaxLayers; ++k) {
		const FrameLayer& l = f.layers[k];
		Tex* t = l.kind == kLayerFull ? &g.full : l.kind == kLayerChars ? &g.chars : l.kind == kLayerHud ? &g.hud : nullptr;
		if (t) t->Upload(g.reader.LayerPixels(l), l.width, l.height, l.pitch);
	}
	g.haveFrame = true;
	++g.uploads;
}

// Screen position (panel pixels) of a world point (1/128 px) in this frame.
ImVec2 WorldToPanel(const FrameSlotHeader& f, ImVec2 origin, float scale, int32_t wx, int32_t wy)
{
	const FrameCamera& c = f.camera;
	const float zoom = c.zoomX1000 ? c.zoomX1000 / 1000.0f : 1.0f;
	const float vx = (float)c.viewX, vy = (float)c.viewY;
	const float sx = vx + ((wx - c.cameraX) / 128.0f) * zoom + 320.0f + c.shakeX / 1000.0f;
	const float sy = vy + ((wy - c.cameraY) / 128.0f) * zoom + 432.0f + c.shakeY / 1000.0f;
	return ImVec2(origin.x + sx * scale, origin.y + sy * scale);
}

void DrawOverlay(HostContext& host, const gamelink::Snapshot& s, const FrameSlotHeader& f, ImVec2 origin, float scale)
{
	GameViewState& g = G();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float zoom = f.camera.zoomX1000 ? f.camera.zoomX1000 / 1000.0f : 1.0f;
	for (int i = 0; i < 4; ++i) {
		const FrameActor& a = f.actors[i];
		if (!a.exists) continue;
		const ImVec2 p = WorldToPanel(f, origin, scale, a.x, a.y);
		// which character: the link state knows the file / moon of each engine slot
		std::string file;
		int moon = 0;
		if (s.haveState && s.state.actors[i].exists) {
			file = std::string(s.state.actors[i].file, strnlen(s.state.actors[i].file, sizeof s.state.actors[i].file));
			for (char& ch : file) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
			moon = s.state.actors[i].moon >= 0 && s.state.actors[i].moon <= 2 ? s.state.actors[i].moon : 0;
		}
		const std::string txt = file.empty() ? std::string() : DataTxt(file, moon);
		const FrameData* fd = !txt.empty() && host.frameDataFor ? host.frameDataFor(txt) : nullptr;
		int boxes = 0;
		if (g.overlayBoxes && fd) {
			auto* seq = const_cast<FrameData*>(fd)->get_sequence(a.pattern);
			if (seq && a.frame >= 0 && a.frame < (int)seq->frames.size()) {
				const int selected = host.selectedBoxFor ? host.selectedBoxFor(txt) : -1;
				for (const auto& kv : seq->frames[a.frame].hitboxes) {
					const int id = kv.first;
					const bool hurt = id >= 1 && id <= 8, attack = id >= 25;
					if ((hurt && !g.overlayHurt) || (attack && !g.overlayAttack) || (!hurt && !attack && !g.overlayOther)) continue;
					const int* xy = kv.second.xy;
					float x1 = (float)xy[0], x2 = (float)xy[2];
					if (a.facing) { x1 = -x1; x2 = -x2; }
					const ImVec2 r0(p.x + std::min(x1, x2) * zoom * scale, p.y + std::min(xy[1], xy[3]) * zoom * scale);
					const ImVec2 r1(p.x + std::max(x1, x2) * zoom * scale, p.y + std::max(xy[1], xy[3]) * zoom * scale);
					const ImU32 col = id == selected ? IM_COL32(255, 230, 40, 255)
					                : attack ? IM_COL32(255, 60, 60, 230) : hurt ? IM_COL32(60, 230, 90, 200) : IM_COL32(80, 140, 255, 200);
					dl->AddRectFilled(r0, r1, (col & 0x00FFFFFFu) | (id == selected ? 0x50000000u : 0x28000000u));
					dl->AddRect(r0, r1, col, 0, 0, id == selected ? 2.5f : 1.0f);
					++boxes;
				}
			}
		}
		dl->AddLine(ImVec2(p.x - 6, p.y), ImVec2(p.x + 6, p.y), IM_COL32(255, 255, 255, 200));
		dl->AddLine(ImVec2(p.x, p.y - 6), ImVec2(p.x, p.y + 6), IM_COL32(255, 255, 255, 200));
		if (g.overlayLabels) {
			char b[96];
			std::snprintf(b, sizeof b, "%s %s p%d f%d%s", SlotRoleName(i), file.empty() ? "?" : file.c_str(), a.pattern, a.frame,
			              g.overlayBoxes && !fd && !file.empty() ? " (open the tab for boxes)" : "");
			const ImVec2 tp(p.x - 40, p.y + 8);
			dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 220), b);
			dl->AddText(tp, a.team ? IM_COL32(255, 170, 120, 255) : IM_COL32(140, 190, 255, 255), b);
		}
		(void)boxes;
	}
}

// Keyboard (arrows / WASD, J K L U = A B C D, I = E (FN1), O = FN2, Enter = Start) and pad (XInput through ImGui).
GameViewKeys ReadKeys()
{
	GameViewKeys k;
	auto dn = [](ImGuiKey key) { return ImGui::IsKeyDown(key); };
	k.up = dn(ImGuiKey_UpArrow) || dn(ImGuiKey_W) || dn(ImGuiKey_GamepadDpadUp) || dn(ImGuiKey_GamepadLStickUp);
	k.down = dn(ImGuiKey_DownArrow) || dn(ImGuiKey_S) || dn(ImGuiKey_GamepadDpadDown) || dn(ImGuiKey_GamepadLStickDown);
	k.left = dn(ImGuiKey_LeftArrow) || dn(ImGuiKey_A) || dn(ImGuiKey_GamepadDpadLeft) || dn(ImGuiKey_GamepadLStickLeft);
	k.right = dn(ImGuiKey_RightArrow) || dn(ImGuiKey_D) || dn(ImGuiKey_GamepadDpadRight) || dn(ImGuiKey_GamepadLStickRight);
	k.a = dn(ImGuiKey_J) || dn(ImGuiKey_GamepadFaceLeft);
	k.b = dn(ImGuiKey_K) || dn(ImGuiKey_GamepadFaceDown);
	k.c = dn(ImGuiKey_L) || dn(ImGuiKey_GamepadFaceRight);
	k.d = dn(ImGuiKey_U) || dn(ImGuiKey_GamepadFaceUp);
	k.e = dn(ImGuiKey_I) || dn(ImGuiKey_GamepadR1);
	k.fn2 = dn(ImGuiKey_O) || dn(ImGuiKey_GamepadL1);
	k.start = dn(ImGuiKey_Enter) || dn(ImGuiKey_GamepadStart);
	return k;
}

bool SameKeys(const GameViewKeys& a, const GameViewKeys& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

void ForwardInput(bool focused, const gamelink::Snapshot& s)
{
	GameViewState& g = G();
	const bool can = s.connected && s.haveCaps && (s.caps.caps & wire::kCapInputInject) && !s.SessionLive();
	g.capturing = focused && g.forwardInput && can;
	const uint64_t now = NowMs();
	if (!g.capturing) {
		if (!g.released && can) { Link().InputInject(MakeRelease((uint8_t)g.player, ++g.serial)); }
		g.released = true;
		g.lastKeys = GameViewKeys{};
		return;
	}
	const GameViewKeys k = ReadKeys();
	// send on a change, and every 100 ms as a keep-alive (the state holds 12 frames = 200 ms, then goes neutral)
	if (!SameKeys(k, g.lastKeys) || now - g.lastSendMs >= 100) {
		Link().InputInject(MakeInject(k, (uint8_t)g.player, ++g.serial, 12));
		g.lastKeys = k;
		g.lastSendMs = now;
		g.released = false;
	}
}

} // namespace

void OpenGameView() { showGameView = true; }
bool GameViewCapturesInput() { return showGameView && G().capturing; }

void DrawGameView(HostContext& host)
{
	GameViewState& g = G();
	gamelink::Client& c = Link();
	const gamelink::Snapshot s = c.Get();
	if (!showGameView) {
		if (!g.released && s.connected) c.InputInject(MakeRelease((uint8_t)g.player, ++g.serial));
		g.released = true;
		g.capturing = false;
		return;
	}
	// ---- the ring: ask where it is, open it, pull the newest frame ----
	const bool canShare = s.connected && s.haveCaps && (s.caps.caps & wire::kCapFrameShare);
	const uint64_t now = NowMs();
	if (canShare && (!s.haveFrameShare || !s.frameShare.version) && now - g.lastQueryMs > 1000) { g.lastQueryMs = now; c.QueryFrameShare(); }
	if (!s.connected && g.reader.IsOpen()) { g.reader.Close(); g.openedName.clear(); g.haveFrame = false; }
	if (s.haveFrameShare && s.frameShare.version && g.openedName != s.frameShare.name) {
		std::string why;
		if (g.reader.Open(s.frameShare.name, &why)) { g.openedName = s.frameShare.name; g.openWhy.clear(); }
		else if (now - g.lastQueryMs > 1000) { g.openWhy = why; g.lastQueryMs = now; }
	}
	ReadResult rr = ReadResult::NoRing;
	if (g.reader.IsOpen()) {
		rr = g.reader.Poll();
		if (rr == ReadResult::Ok) UploadFrame(g);
	}
	// ---- the window ----
	if (g.ownWindow) {
		ImGuiWindowClass wc;
		wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;   // its own OS window, even over the main one
		ImGui::SetNextWindowClass(&wc);
	}
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 80, vp->WorkPos.y + 80), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(std::min(700.0f, vp->WorkSize.x - 100), std::min(640.0f, vp->WorkSize.y - 100)), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Game (MBAACC)###gameview", &showGameView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) {
		ForwardInput(false, s);
		ImGui::End();
		return;
	}
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Full frame (the game's picture)", nullptr, !g.layeredView)) g.layeredView = false;
			if (ImGui::MenuItem("Layered: Hantei-chan's stage + the game's characters / HUD", nullptr, g.layeredView)) g.layeredView = true;
			ImGui::Separator();
			ImGui::TextDisabled("stage behind the layers");
			if (ImGui::MenuItem("Hantei-chan's open stage", nullptr, g.stageSource == StageSource::Hantei)) g.stageSource = StageSource::Hantei;
			if (ImGui::MenuItem("Test pattern stage (mock)", nullptr, g.stageSource == StageSource::TestPattern)) g.stageSource = StageSource::TestPattern;
			if (ImGui::MenuItem("Black", nullptr, g.stageSource == StageSource::Black)) g.stageSource = StageSource::Black;
			ImGui::Separator();
			ImGui::MenuItem("Overlay: hitboxes", nullptr, &g.overlayBoxes);
			ImGui::MenuItem("   hurt boxes", nullptr, &g.overlayHurt);
			ImGui::MenuItem("   attack boxes", nullptr, &g.overlayAttack);
			ImGui::MenuItem("   other boxes", nullptr, &g.overlayOther);
			ImGui::MenuItem("Overlay: pattern / frame labels", nullptr, &g.overlayLabels);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game")) {
			const bool can = canShare && !s.SessionLive();
			if (ImGui::MenuItem("Embed: hide the real window, full frame", nullptr, false, can)) c.SetEmbedded(1);
			if (ImGui::MenuItem("Embed: layered capture (stage drawn here)", nullptr, false, can)) { c.SetEmbedded(2); g.layeredView = true; }
			if (ImGui::MenuItem("Undock to real window (show the game's own window)", nullptr, false, canShare)) { c.SetEmbedded(0); g.layeredView = false; }
			ImGui::Separator();
			ImGui::MenuItem("This panel in its own OS window", nullptr, &g.ownWindow);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Input")) {
			ImGui::MenuItem("Forward keyboard / pad while this panel is focused", nullptr, &g.forwardInput);
			for (int p = 0; p < 4; ++p) {
				char l[32];
				std::snprintf(l, sizeof l, "as player %d", p + 1);
				if (ImGui::MenuItem(l, nullptr, g.player == p)) g.player = p;
			}
			ImGui::Separator();
			ImGui::TextDisabled("arrows / WASD, J K L U = A B C D, I = FN1, O = FN2, Enter = Start");
			ImGui::TextDisabled("pad: d-pad / stick, face buttons = A B C D, RB = FN1, LB = FN2");
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	// ---- the status line ----
	const FrameRingHeader* rh = g.reader.Ring();
	if (!s.connected) ImGui::TextDisabled("not linked: launch or attach in the Authoring window");
	else if (s.haveCaps && !(s.caps.caps & wire::kCapFrameShare))
		ImGui::TextColored(kColWarn, "this pchost.dll has no frame export (no kCapFrameShare): use the real game window");
	else if (!g.reader.IsOpen()) ImGui::TextColored(kColWarn, "waiting for the frame ring%s%s", g.openWhy.empty() ? "" : ": ", g.openWhy.c_str());
	else {
		ImGui::Text("%.0f fps  latency %u ms  frame %u  game %u  skipped %u", g.reader.Fps(), g.reader.LatencyMs(),
		            g.haveFrame ? g.reader.Frame().frameSeq : 0, g.haveFrame ? g.reader.Frame().gameFrame : 0, g.reader.Skipped());
		ImGui::SameLine();
		ImGui::TextDisabled("| copy %u us  %s%s%s | %s", rh ? rh->lastCopyUs : 0, rh && (rh->flags & kFlagEmbedded) ? "embedded" : "real window shown",
		                    rh && (rh->flags & kFlagLayered) ? " + layered" : "", rh && !(rh->flags & kFlagProducerAlive) ? " (producer stopped)" : "",
		                    rh ? rh->producer : "");
	}
	if (g.capturing) { ImGui::SameLine(); ImGui::TextColored(kColOk, "  INPUT -> P%d", g.player + 1); }
	else if (focused && g.forwardInput && s.SessionLive()) { ImGui::SameLine(); ImGui::TextColored(kColBad, "  input refused: session"); }
	if (!s.lastInjectReply.empty() && s.lastInjectReply.find("ok") == std::string::npos) ImGui::TextColored(kColBad, "%s", s.lastInjectReply.c_str());
	// ---- the picture, aspect-correct ----
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	if (!g.haveFrame) {
		ImGui::Dummy(avail);
		ForwardInput(focused, s);
		ImGui::End();
		return;
	}
	const FrameSlotHeader& f = g.reader.Frame();
	const float fw = (float)f.width, fh = (float)f.height;
	const float scale = std::max(0.05f, std::min(avail.x / fw, avail.y / fh));
	const ImVec2 size(fw * scale, fh * scale);
	const ImVec2 origin(ImGui::GetCursorScreenPos().x + (avail.x - size.x) * 0.5f, ImGui::GetCursorScreenPos().y + (avail.y - size.y) * 0.5f);
	const ImVec2 end(origin.x + size.x, origin.y + size.y);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const bool layeredAvailable = (f.flags & kSlotLayered) && FindLayer(f, kLayerChars);
	if (!g.layeredView || !layeredAvailable) {
		dl->AddImage(g.full.Im(), origin, end);
		if (g.layeredView && !layeredAvailable) {
			ImGui::SetCursorScreenPos(ImVec2(origin.x + 6, origin.y + 6));
			ImGui::TextColored(kColWarn, "no layers in this frame (Game > Embed: layered capture): showing the full frame");
		}
	} else {
		// back stage -> [super-flash darkening] -> CHARS -> front stage -> HUD, premultiplied
		const FrameCamera& cam = f.camera;
		const float zoom = cam.zoomX1000 ? cam.zoomX1000 / 1000.0f : 1.0f;
		const int vw = cam.viewW ? cam.viewW : f.width, vh = cam.viewH ? cam.viewH : f.height;
		const ImVec2 v0(origin.x + cam.viewX * scale, origin.y + cam.viewY * scale), v1(v0.x + vw * scale, v0.y + vh * scale);
		dl->AddRectFilled(origin, end, IM_COL32(0, 0, 0, 255));
		unsigned back = 0, front = 0;
		const float camPx = cam.cameraX / 128.0f - cam.shakeX / 1000.0f / zoom, camPy = cam.cameraY / 128.0f - cam.shakeY / 1000.0f / zoom;
		if (g.stageSource == StageSource::Hantei && host.renderStage) {
			back = host.renderStage(camPx, camPy, zoom, vw, vh, 1, cam.heatBlurX1000 / 1000.0f);
			front = host.renderStage(camPx, camPy, zoom, vw, vh, 2, 0.0f);
		}
		if (back) dl->AddImage((ImTextureID)(intptr_t)back, v0, v1, ImVec2(0, 1), ImVec2(1, 0));   // FBO: bottom-up
		else if (g.stageSource == StageSource::TestPattern || (g.stageSource == StageSource::Hantei && !back)) {
			const uint32_t pitch = PitchFor((uint32_t)vw);
			g.testStagePx.resize((size_t)pitch * vh);
			DrawTestStage(g.testStagePx.data(), (uint32_t)vw, (uint32_t)vh, pitch, cam);
			g.testStage.Upload(g.testStagePx.data(), vw, vh, pitch);
			dl->AddImage(g.testStage.Im(), v0, v1);
		}
		if (cam.superFlashDarkX1000) dl->AddRectFilled(v0, v1, IM_COL32(0, 0, 0, (int)std::min(255u, cam.superFlashDarkX1000 * 255u / 1000u)));
		dl->AddCallback(BlendPremul, nullptr);
		const FrameLayer* ch = FindLayer(f, kLayerChars);
		dl->AddImage(g.chars.Im(), ImVec2(origin.x + ch->x * scale, origin.y + ch->y * scale),
		             ImVec2(origin.x + (ch->x + ch->width) * scale, origin.y + (ch->y + ch->height) * scale));
		if (front) dl->AddImage((ImTextureID)(intptr_t)front, v0, v1, ImVec2(0, 1), ImVec2(1, 0));
		if (const FrameLayer* hud = FindLayer(f, kLayerHud))
			dl->AddImage(g.hud.Im(), ImVec2(origin.x + hud->x * scale, origin.y + hud->y * scale),
			             ImVec2(origin.x + (hud->x + hud->width) * scale, origin.y + (hud->y + hud->height) * scale));
		dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
		if (g.stageSource == StageSource::Hantei && !back) {
			ImGui::SetCursorScreenPos(ImVec2(origin.x + 6, origin.y + 6));
			ImGui::TextColored(kColWarn, "no stage open in Hantei-chan: test pattern behind the layers (open the stage tab)");
		}
	}
	DrawOverlay(host, s, f, origin, scale);
	// the picture area takes the clicks (focus) without moving the window
	ImGui::SetCursorScreenPos(origin);
	ImGui::InvisibleButton("##picture", size);
	ForwardInput(focused, s);
	ImGui::End();
}

} // namespace authoring
