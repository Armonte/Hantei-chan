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
	// §12.3 (CHANGED by PC agent)
	uint16_t layeredSeq = 0;             // the SetEmbedded 2 request waiting for its answer
	std::string layeredNote;             // why the panel shows the full frame although layered was asked
	bool cropSidebars = false;           // show only FrameCamera.view (the 4:3 picture between sidebars)
	uint16_t embedSeq = 0;
	int colorValX1000 = 0;               // SetStageLighting override (0..; a negative StageColorVal reads 0)
	uint16_t lightingSeq = 0;
	std::string lightingReply;
	bool lightingOverride = false;
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

// Screen position (panel pixels) of a world point in this frame (game_view_input.h WorldToFrame: §12.3 rules).
ImVec2 WorldToPanel(const FrameSlotHeader& f, ImVec2 origin, float scale, int32_t wx, int32_t wy)
{
	const FramePoint p = WorldToFrame(f, wx, wy);
	return ImVec2(origin.x + p.x * scale, origin.y + p.y * scale);
}

void DrawOverlay(HostContext& host, const gamelink::Snapshot& s, const FrameSlotHeader& f, ImVec2 origin, float scale)
{
	GameViewState& g = G();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float zoom = WorldToFrame(f, 0, 0).scale;   // HA6 px -> frame px (zoom x the picture's scale)
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

// §12.3: pchost owns only the P1 / P2 pad words (P3 / P4 answer Unsupported), and P2 is the Training dummy's while the
// game is in Training. "" = injectable.
std::string PlayerBlock(const gamelink::Snapshot& s, int player)
{
	const uint32_t kind = s.haveSetup ? s.setup.gameModeKind : s.haveState ? s.state.gameModeKind : 0;
	return PlayerBlockReason(player, kind);
}

void ForwardInput(bool focused, const gamelink::Snapshot& s)
{
	GameViewState& g = G();
	const bool can = s.connected && s.haveCaps && (s.caps.caps & wire::kCapInputInject) && !s.SessionLive() &&
	                 PlayerBlock(s, g.player).empty();
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
		// §12.3: a ring outgrown by the backbuffer is replaced (…-g<n>) and the old one's kFlagProducerAlive clears:
		// ask again where the ring is (1 Hz); the new name re-opens it above on the next frames.
		const FrameRingHeader* old = g.reader.Ring();
		if (old && !(LoadAcquire(&old->flags) & kFlagProducerAlive) && canShare && now - g.lastQueryMs > 1000) {
			g.lastQueryMs = now;
			c.QueryFrameShare();
			g.status = "the game replaced its frame ring (regrow / restart): asking for the new one";
		}
	}
	// §12.3: SetEmbedded 2 answers Unsupported in this pchost (no layered capture yet): fall back to the full frame
	if (g.layeredSeq) {
		wire::Reply r{};
		if (c.PeekReply(g.layeredSeq, r)) {
			g.layeredSeq = 0;
			if (r.status != 0) {
				g.layeredView = false;
				g.layeredNote = std::string("layered capture refused (") + wire::StatusName(r.status) + ": " + r.message +
				                ") - showing the full frame; this pchost.dll publishes only layer 0";
				g.embedSeq = c.SetEmbedded(1);
			} else {
				g.layeredNote.clear();
			}
		}
	}
	if (g.lightingSeq) {
		wire::Reply r{};
		if (c.PeekReply(g.lightingSeq, r)) { g.lightingSeq = 0; g.lightingReply = std::string(wire::StatusName(r.status)) + ": " + r.message; }
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
			ImGui::Separator();
			ImGui::MenuItem("Crop the sidebars (only the game's 4:3 picture)", nullptr, &g.cropSidebars);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game")) {
			const bool can = canShare && !s.SessionLive();
			if (ImGui::MenuItem("Embed: hide the real window, full frame", nullptr, false, can)) c.SetEmbedded(1);
			if (ImGui::MenuItem("Embed: layered capture (stage drawn here)", nullptr, false, can)) { g.layeredSeq = c.SetEmbedded(2); g.layeredView = true; }
			if (ImGui::MenuItem("Undock to real window (show the game's own window)", nullptr, false, canShare)) { c.SetEmbedded(0); g.layeredView = false; }
			ImGui::Separator();
			ImGui::MenuItem("This panel in its own OS window", nullptr, &g.ownWindow);
			ImGui::Separator();
			// §12.3: StageColorVal is applied (the BgList entry); the stage light colour is report-only
			ImGui::TextDisabled("StageColorVal override (BgPointBlur fColorHosei)");
			ImGui::SetNextItemWidth(160);
			ImGui::SliderInt("x1000##colorval", &g.colorValX1000, 0, 2000);
			if (ImGui::MenuItem("Apply StageColorVal", nullptr, false, can)) {
				wire::StageLighting l{};
				l.stageId = -1;
				l.stageColorValX1000 = (uint32_t)g.colorValX1000;
				g.lightingSeq = c.SetStageLighting(l);
				g.lightingOverride = true;
			}
			if (ImGui::MenuItem("Reset stage lighting (the game's own values)", nullptr, false, canShare)) {
				wire::StageLighting l{};
				l.stageId = -1;
				l.flags = 1;
				g.lightingSeq = c.SetStageLighting(l);
				g.lightingOverride = false;
			}
			ImGui::TextDisabled("an override stays until Reset (or a stage change), even after Hantei-chan disconnects;");
			ImGui::TextDisabled("negative StageColorVal (stage 16 ships -0.05) cannot be sent and reads 0");
			if (!g.lightingReply.empty()) ImGui::TextDisabled("last: %s", g.lightingReply.c_str());
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Input")) {
			ImGui::MenuItem("Forward keyboard / pad while this panel is focused", nullptr, &g.forwardInput);
			for (int p = 0; p < 4; ++p) {
				char l[32];
				std::snprintf(l, sizeof l, "as player %d", p + 1);
				const std::string block = PlayerBlock(s, p);
				if (ImGui::MenuItem(l, nullptr, g.player == p, block.empty())) g.player = p;
				if (!block.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", block.c_str());
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
	if (g.haveFrame) {
		const FrameCamera& cam = g.reader.Frame().camera;
		ImGui::TextDisabled("stage %d  StageColorVal %.3f%s  light 0x%08X (report only)%s", cam.stageId, cam.stageColorValX1000 / 1000.0,
		                    g.lightingOverride ? " (override: until Reset)" : "", cam.stageLightArgb,
		                    cam.viewW ? "" : "  picture fills the frame");
	}
	if (!g.layeredNote.empty()) ImGui::TextColored(kColWarn, "%s", g.layeredNote.c_str());
	if (!g.status.empty() && g.reader.Ring() && !(g.reader.Ring()->flags & kFlagProducerAlive)) ImGui::TextColored(kColWarn, "%s", g.status.c_str());
	{
		const std::string block = PlayerBlock(s, g.player);
		if (!block.empty() && g.forwardInput) ImGui::TextColored(kColWarn, "input not forwarded: %s", block.c_str());
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
	// §12.3: FrameCamera.view = the scaler's picture rect inside the frame (e.g. 78,0 468x351 between sidebars)
	const bool crop = g.cropSidebars && f.camera.viewW && f.camera.viewH;
	const float cx0 = crop ? (float)f.camera.viewX : 0.0f, cy0 = crop ? (float)f.camera.viewY : 0.0f;
	const float fw = crop ? (float)f.camera.viewW : (float)f.width, fh = crop ? (float)f.camera.viewH : (float)f.height;
	const float scale = std::max(0.05f, std::min(avail.x / fw, avail.y / fh));
	const ImVec2 size(fw * scale, fh * scale);
	const ImVec2 shown(ImGui::GetCursorScreenPos().x + (avail.x - size.x) * 0.5f, ImGui::GetCursorScreenPos().y + (avail.y - size.y) * 0.5f);
	// origin = where frame pixel (0,0) lands (left of the shown area when cropping)
	const ImVec2 origin(shown.x - cx0 * scale, shown.y - cy0 * scale);
	const ImVec2 end(origin.x + f.width * scale, origin.y + f.height * scale);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->PushClipRect(shown, ImVec2(shown.x + size.x, shown.y + size.y), true);
	const bool layeredAvailable = (f.flags & kSlotLayered) && FindLayer(f, kLayerChars);
	if (!g.layeredView || !layeredAvailable) {
		dl->AddImage(g.full.Im(), origin, end);
		if (g.layeredView && !layeredAvailable && g.layeredNote.empty()) {
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
		const float camPx = cam.cameraX / 128.0f, camPy = cam.cameraY / 128.0f;   // as drawn (§12.3: no shake to add)
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
	dl->PopClipRect();
	// the picture area takes the clicks (focus) without moving the window
	ImGui::SetCursorScreenPos(shown);
	ImGui::InvisibleButton("##picture", size);
	ForwardInput(focused, s);
	ImGui::End();
}

} // namespace authoring
