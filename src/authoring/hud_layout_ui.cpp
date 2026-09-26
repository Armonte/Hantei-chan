// [authoring] The TAG HUD layout editor — see hud_layout.h (DrawHudLayoutEditor) and docs/HANTEI_AUTHORING_MODE.md §8.6 / §9.4.
//
// A 640x480 preview of both sides' TAG HUD, drawn with the game's own face plates (GRP/Gauge_AA/face/
// faceNN_<side>.png, the art HudLoadPortraitBundle loads). Every element is dragged in the preview (move; the
// bottom-right handle resizes) or typed in the list beside it. Edits go to local\hud.ini through the EditSink: one
// undo step per gesture, saved and live-applied (the game re-reads hud.ini with the tuning). UI thread only: the
// textures are GL objects of the editor's context.
#include "hud_layout.h"
#include "edit_sink.h"
#include "../tag_tuning/tag_sidecar.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>

unsigned LoadPngTexture(const std::string& path, int* w, int* h);   // hud_textures.cpp

namespace authoring {

namespace {

namespace fs = std::filesystem;

struct Tex { unsigned id = 0; int w = 0, h = 0; bool tried = false; };

struct EditorState {
	std::map<std::string, Tex> tex;   // by path; kept for the process (a few face plates)
	std::string selected = "reservePortrait";
	// the drag in progress
	std::string dragKey;
	bool dragResize = false;
	int dragSide = 0;
	ImVec2 dragStartMouse{};
	HudRect dragStartRect{};
	bool listEditing = false;
};
EditorState& S() { static EditorState s; return s; }

const Tex* Texture(const std::string& path)
{
	Tex& t = S().tex[path];
	if (!t.tried) {
		t.tried = true;
		std::error_code ec;
		if (fs::exists(fs::u8path(path), ec)) t.id = LoadPngTexture(path, &t.w, &t.h);
	}
	return t.id ? &t : nullptr;
}

std::string FacePath(const std::string& gameDir, int chara, int side)
{
	if (gameDir.empty() || chara < 0) return {};
	char b[64];
	std::snprintf(b, sizeof b, "\\GRP\\Gauge_AA\\face\\face%02d_%02d.png", chara, side ? 1 : 0);
	return gameDir + b;
}

// The editable elements, bottom to top (the draw order; hit tests walk it backwards).
struct Element { const char* key; bool isRect; ImU32 color; const char* label; };
const Element kElements[] = {
	{ "banner", true, IM_COL32(90, 140, 220, 255), "banner" },
	{ "reservePortrait", true, IM_COL32(170, 170, 170, 255), "reserve face" },
	{ "mainPortrait", true, IM_COL32(255, 255, 255, 255), "point face" },
	{ "partnerThumb", true, IM_COL32(200, 200, 200, 255), "partner thumb" },
	{ "partnerBar.rect", true, IM_COL32(80, 220, 110, 255), "partner bar" },
	{ "swapSliver", true, IM_COL32(240, 200, 60, 255), "swap cooldown" },
	{ "assistPip", true, IM_COL32(240, 192, 48, 255), "assist pip" },
	{ "assistLabel", false, IM_COL32(240, 192, 48, 255), "FN1 label" },
	{ "name", false, IM_COL32(240, 240, 240, 255), "team name" },
};

HudRect RectOf(const TagHudValues& v, const std::string& key)
{
	if (key == "banner") return v.banner;
	if (key == "reservePortrait") return v.reservePortrait;
	if (key == "mainPortrait") return v.mainPortrait;
	if (key == "partnerThumb") return v.partnerThumb;
	if (key == "partnerBar.rect") return v.partnerBarRect;
	if (key == "swapSliver") return v.swapSliver;
	if (key == "assistPip") return v.assistPip;
	if (key == "assistLabel") return { v.assistLabelX, v.assistLabelY, 24, 8 };        // "FN1" in 8 px cells
	if (key == "name") return { v.nameX, v.nameY, 8.0f * 12 * v.namePx, 8.0f * v.namePx };  // ~12 characters
	return {};
}

// The text a moved / resized element writes.
std::string TextFor(const std::string& key, const HudRect& r, const TagHudValues& v)
{
	char b[96];
	auto n = [](float f) { return (int)std::lround(f); };
	if (key == "assistLabel") std::snprintf(b, sizeof b, "%d,%d", n(r.x), n(r.y));
	else if (key == "name") {
		TagHudValues t = v;
		t.nameX = (float)n(r.x); t.nameY = (float)n(r.y);
		return HudValueText(t, "name");
	} else std::snprintf(b, sizeof b, "%d,%d,%d,%d", n(r.x), n(r.y), n(r.w), n(r.h));
	return b;
}

HudRect Mirror(HudRect r, int side) { if (side) r.x = 640.0f - r.x - r.w; return r; }
HudRect SideSrc(HudRect src, int side) { if (side) src.x = 256.0f - src.x - src.w; return src; }

const char* LayerOf(const tagtune::TagIni* shipped, const tagtune::TagIni* local, const char* key, std::string* localValue)
{
	std::string v;
	if (local && local->Get(tagtune::SecKind::Other, "taghud", key, v)) { if (localValue) *localValue = v; return "local"; }
	if (shipped && shipped->Get(tagtune::SecKind::Other, "taghud", key, v)) return "shipped";
	return "default";
}

void DrawFace(ImDrawList* dl, const Tex* t, ImVec2 o, float s, HudRect dst, HudRect src, ImU32 tint, ImU32 fallback, const char* label)
{
	const ImVec2 a(o.x + dst.x * s, o.y + dst.y * s), b(o.x + (dst.x + dst.w) * s, o.y + (dst.y + dst.h) * s);
	if (t) {
		const ImVec2 uv0(src.x / t->w, src.y / t->h), uv1((src.x + src.w) / t->w, (src.y + src.h) / t->h);
		dl->AddImage((ImTextureID)(uintptr_t)t->id, a, b, uv0, uv1, tint);
	} else {
		dl->AddRectFilled(a, b, (fallback & 0x00FFFFFFu) | 0x50000000u);
		dl->AddText(ImVec2(a.x + 2, a.y + 1), fallback, label);
	}
}

} // namespace

void DrawHudLayoutEditor(EditSink& sink, const std::string& gameDir, const int previewChara[4])
{
	EditorState& st = S();
	tagtune::SidecarWorkspace& ws = sink.Workspace();
	const bool open = ws.IsOpen();
	const bool canEdit = open && sink.CanEdit();
	const tagtune::SidecarDoc* shippedDoc = open ? ws.Find(tagtune::HudDoc(tagtune::Layer::Shipped)) : nullptr;
	const tagtune::SidecarDoc* localDoc = open ? ws.Find(tagtune::HudDoc(tagtune::Layer::Local)) : nullptr;
	const tagtune::TagIni* shipped = shippedDoc ? &shippedDoc->ini : nullptr;
	const tagtune::TagIni* local = localDoc ? &localDoc->ini : nullptr;
	std::vector<std::string> warnings;
	const TagHudValues v = ResolveHud(shipped, local, &warnings);

	auto localIni = [&ws]() -> tagtune::TagIni& { return ws.Doc(tagtune::HudDoc(tagtune::Layer::Local)).ini; };
	auto shippedIni = [&ws]() -> tagtune::TagIni& { return ws.Doc(tagtune::HudDoc(tagtune::Layer::Shipped)).ini; };

	if (!open) ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "No povertycaster\\tag\\ tree: showing the built-in layout (read-only).");
	else if (!canEdit) ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "Read-only right now (session lock / lever table / no tree).");
	ImGui::TextDisabled("Drag an element (Shift: 8 px grid); drag its corner to resize. Edits go to local\\hud.ini; the game follows.");

	const float listW = std::min(430.0f, ImGui::GetContentRegionAvail().x * 0.42f);
	const float previewW = std::max(200.0f, ImGui::GetContentRegionAvail().x - listW - 12.0f);
	const float s = previewW / 640.0f;

	// ================== the preview ==================
	ImGui::BeginChild("##hudpreview", ImVec2(previewW, 480.0f * s + 4), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 o = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##hudcanvas", ImVec2(640.0f * s, 480.0f * s));
	const bool canvasHovered = ImGui::IsItemHovered();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(o, ImVec2(o.x + 640 * s, o.y + 480 * s), IM_COL32(28, 30, 38, 255));
	dl->AddRect(o, ImVec2(o.x + 640 * s, o.y + 480 * s), IM_COL32(90, 90, 110, 255));
	auto R = [&](HudRect r) { return std::make_pair(ImVec2(o.x + r.x * s, o.y + r.y * s), ImVec2(o.x + (r.x + r.w) * s, o.y + (r.y + r.h) * s)); };
	// the native HUD the TAG layer sits on (IDA: face plate 256x96 at 0,0; health bar 212x10 right-anchored at 273,40;
	// gauge frame 320x96 at 0,18; moon icon 48x48 at 27,60): dim outlines, for reference only
	for (int side = 0; side < 2; ++side) {
		const HudRect native[] = { { 0, 18, 320, 96 }, { 61, 40, 212, 10 }, { 27, 60, 48, 48 } };
		for (const HudRect& n : native) { auto ab = R(Mirror(n, side)); dl->AddRect(ab.first, ab.second, IM_COL32(70, 70, 90, 255)); }
		auto hb = R(Mirror(HudRect{ 61, 40, 212, 10 }, side));
		dl->AddRectFilled(hb.first, hb.second, IM_COL32(200, 60, 50, 110));
	}
	const char* fallbackName[2] = { "P1", "P2" };
	for (int side = 0; side < 2; ++side) {
		const int point = previewChara ? previewChara[side] : -1, reserve = previewChara ? previewChara[side + 2] : -1;
		const Tex* pf = Texture(FacePath(gameDir, point, side));
		const Tex* rf = Texture(FacePath(gameDir, reserve, side));
		for (const Element& e : kElements) {
			const std::string key = e.key;
			const HudRect dst = Mirror(RectOf(v, key), side);
			auto ab = R(dst);
			if (key == "reservePortrait") {
				if (v.reserveFace) DrawFace(dl, rf, o, s, dst, SideSrc(v.reserveSrc, side), IM_COL32(184, 184, 184, 255), e.color, "reserve");
			} else if (key == "mainPortrait") {
				DrawFace(dl, pf, o, s, dst, SideSrc(v.mainSrc, side), IM_COL32_WHITE, e.color, fallbackName[side]);
			} else if (key == "partnerThumb") {
				if (v.partnerBar) DrawFace(dl, rf, o, s, dst, SideSrc(v.reserveSrc, side), IM_COL32(184, 184, 184, 255), e.color, "");
			} else if (key == "partnerBar.rect") {
				if (v.partnerBar) {
					dl->AddRectFilled(ab.first, ab.second, IM_COL32(20, 20, 20, 255));
					const float fill = 0.7f * (ab.second.x - ab.first.x);
					if (side == 0) dl->AddRectFilled(ImVec2(ab.second.x - fill, ab.first.y), ab.second, e.color);
					else dl->AddRectFilled(ab.first, ImVec2(ab.first.x + fill, ab.second.y), e.color);
				}
			} else if (key == "swapSliver") {
				if (v.swapCooldown) dl->AddRectFilled(ab.first, ImVec2(ab.first.x + 0.4f * (ab.second.x - ab.first.x), ab.second.y), e.color);
			} else if (key == "assistPip") {
				if (v.assist) { dl->AddRectFilled(ab.first, ab.second, e.color); }
			} else if (key == "assistLabel") {
				if (v.assist) dl->AddText(nullptr, 8.0f * s * 1.25f, ab.first, e.color, "FN1");
			} else if (key == "name") {
				if (v.teamName && !v.banners) dl->AddText(nullptr, 8.0f * v.namePx * s * 1.25f, ab.first, e.color, side ? "TEAM TWO" : "TEAM ONE");
			} else if (key == "banner") {
				if (v.teamName && v.banners) {
					dl->AddRectFilled(ab.first, ab.second, IM_COL32(40, 60, 110, 200));
					dl->AddText(nullptr, std::max(8.0f, (ab.second.y - ab.first.y) * 0.9f), ImVec2(ab.first.x + 2, ab.first.y), IM_COL32_WHITE, "tag_name banner");
				}
			}
			// selection / hover frames
			const bool sel = st.selected == key;
			if (sel) dl->AddRect(ab.first, ab.second, IM_COL32(255, 80, 200, 255), 0, 0, 2.0f);
			if (sel && e.isRect && side == 0) dl->AddRectFilled(ImVec2(ab.second.x - 5, ab.second.y - 5), ImVec2(ab.second.x + 2, ab.second.y + 2), IM_COL32(255, 80, 200, 255));
		}
	}
	// ---- interaction ----
	const ImVec2 m = ImGui::GetIO().MousePos;
	auto hit = [&](int& sideOut, bool& resizeOut) -> const Element* {
		for (int side = 0; side < 2; ++side)
			for (int i = (int)(sizeof kElements / sizeof kElements[0]) - 1; i >= 0; --i) {
				const Element& e = kElements[i];
				auto ab = R(Mirror(RectOf(v, e.key), side));
				const bool corner = e.isRect && side == 0 && std::fabs(m.x - ab.second.x) <= 5 && std::fabs(m.y - ab.second.y) <= 5;
				if (corner || (m.x >= ab.first.x && m.x <= ab.second.x && m.y >= ab.first.y - 1 && m.y <= ab.second.y + 1)) {
					sideOut = side;
					resizeOut = corner;
					return &e;
				}
			}
		return nullptr;
	};
	if (st.dragKey.empty() && canvasHovered) {
		int side = 0;
		bool resize = false;
		if (const Element* e = hit(side, resize)) {
			const std::string txt = HudValueText(v, e->key);
			ImGui::SetTooltip("%s (%s): %s%s", e->label, e->key, txt.c_str(), resize ? "  [resize]" : side ? "  [P2 side, mirrored]" : "");
			if (resize) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				st.selected = e->key;
				if (canEdit) {
					st.dragKey = e->key;
					st.dragResize = resize;
					st.dragSide = side;
					st.dragStartMouse = m;
					st.dragStartRect = RectOf(v, e->key);
					sink.Begin(std::string(e->key) + " (hud)");
				}
			}
		}
	}
	if (!st.dragKey.empty()) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			float dx = (m.x - st.dragStartMouse.x) / s, dy = (m.y - st.dragStartMouse.y) / s;
			if (st.dragSide) dx = -dx;   // P2 is the mirror image
			const float grid = ImGui::GetIO().KeyShift ? 8.0f : 1.0f;
			auto snap = [grid](float f) { return std::round(f / grid) * grid; };
			HudRect r = st.dragStartRect;
			if (st.dragResize) { r.w = std::max(1.0f, snap(r.w + dx)); r.h = std::max(1.0f, snap(r.h + dy)); }
			else { r.x = snap(r.x + dx); r.y = snap(r.y + dy); }
			const std::string txt = TextFor(st.dragKey, r, v);
			if (txt != HudValueText(v, st.dragKey)) {
				SetHudKey(localIni(), st.dragKey, txt);
				sink.Edited();
			}
			ImGui::SetTooltip("%s = %s", st.dragKey.c_str(), txt.c_str());
		} else {
			st.dragKey.clear();
			sink.End();
		}
	}
	ImGui::EndChild();

	// ================== the list ==================
	ImGui::SameLine();
	ImGui::BeginChild("##hudlist", ImVec2(listW, 480.0f * s + 4), true);
	if (!canEdit) ImGui::BeginDisabled();
	for (const HudKey& k : HudKeys()) {
		ImGui::PushID(k.key);
		std::string localValue;
		const char* layer = LayerOf(shipped, local, k.key, &localValue);
		const std::string kind = k.kind;
		const bool sel = st.selected == k.key;
		if (ImGui::Selectable("##sel", sel, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 0))) st.selected = k.key;
		ImGui::SameLine(0, 0);
		ImGui::TextColored(!std::strcmp(layer, "local") ? ImVec4(0.55f, 0.8f, 1, 1) : !std::strcmp(layer, "shipped") ? ImVec4(0.8f, 0.8f, 0.8f, 1)
		                                                                                                                  : ImVec4(0.55f, 0.55f, 0.55f, 1),
		                   "%-16s %-7s", k.key, layer);
		ImGui::SameLine(210);
		ImGui::SetNextItemWidth(std::max(60.0f, listW - 210 - 110));
		const std::string label = std::string(k.key) + " (hud)";
		bool changed = false;
		std::string text;
		if (kind == "bool") {
			bool b = HudValueText(v, k.key) == "1";
			if (ImGui::Checkbox("##v", &b)) { sink.Begin(label); SetHudKey(localIni(), k.key, b ? "1" : "0"); sink.Edited(); sink.End(); }
		} else {
			float f[4] = {};
			const HudRect r = RectOf(v, k.key);
			int n = 4;
			if (kind == "float") { n = 1; f[0] = v.tweenMs; }
			else if (kind == "xy") { n = 2; f[0] = v.assistLabelX; f[1] = v.assistLabelY; }
			else if (kind == "xyp") { n = 3; f[0] = v.nameX; f[1] = v.nameY; f[2] = v.namePx; }
			else { f[0] = r.x; f[1] = r.y; f[2] = r.w; f[3] = r.h; }
			changed = n == 1 ? ImGui::DragFloat("##v", f, 1.0f, 0, 10000, "%.0f") : n == 2 ? ImGui::DragFloat2("##v", f, 1.0f, -640, 1280, "%.0f")
			        : n == 3 ? ImGui::DragFloat3("##v", f, 1.0f, -640, 1280, "%.2f") : ImGui::DragFloat4("##v", f, 1.0f, -640, 1280, "%.0f");
			if (ImGui::IsItemActivated()) sink.Begin(label);
			if (changed) {
				char b[128];
				auto g = [](float x) { return std::fabs(x - std::round(x)) < 1e-4f ? (double)std::round(x) : (double)x; };
				if (n == 1) std::snprintf(b, sizeof b, "%g", g(std::max(0.0f, f[0])));
				else if (n == 2) std::snprintf(b, sizeof b, "%g,%g", g(f[0]), g(f[1]));
				else if (n == 3) std::snprintf(b, sizeof b, "%g,%g,%g", g(f[0]), g(f[1]), g(std::max(0.25f, f[2])));
				else std::snprintf(b, sizeof b, "%g,%g,%g,%g", g(f[0]), g(f[1]), g(std::max(0.0f, f[2])), g(std::max(0.0f, f[3])));
				text = b;
				SetHudKey(localIni(), k.key, text);
				sink.Edited();
			}
			if (ImGui::IsItemDeactivated()) sink.End();
			if (ImGui::IsItemActive()) st.selected = k.key;
		}
		ImGui::SameLine();
		if (!localValue.empty() || !std::strcmp(layer, "local")) {
			if (ImGui::SmallButton("reset")) { sink.Begin("reset " + std::string(k.key) + " (hud)"); ClearHudKey(localIni(), k.key); sink.Edited(); sink.End(); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("remove the local value: the shipped / default one applies again");
			ImGui::SameLine();
			if (ImGui::SmallButton("promote")) {
				sink.Begin("promote " + std::string(k.key) + " (hud)");
				SetHudKey(shippedIni(), k.key, localValue);
				ClearHudKey(localIni(), k.key);
				sink.Edited();
				sink.End();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Promote to defaults: write this value into the shipped hud.ini and drop the local one");
		}
		ImGui::PopID();
	}
	if (!canEdit) ImGui::EndDisabled();
	if (!warnings.empty()) {
		ImGui::Separator();
		for (const std::string& w : warnings) ImGui::TextColored(ImVec4(1, 0.62f, 0.25f, 1), "%s", w.c_str());
	}
	ImGui::EndChild();
}

} // namespace authoring
