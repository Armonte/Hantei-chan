// Archive browser: the ImGui window (sources, listing with thumbnails, inline preview) and the welcome screen.
#include "archive_browser_state.h"
#include "game_table.h"
#include "cgm/cgm_bank.h"
#include "filedialog.h"
#include "han2_browser.h"
#include "han2_pac_window.h"
#include "i18n.h"
#include "png_writer.h"
#include "misc.h"

#include <windows.h>
#include <shellapi.h>
#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace abrowser {

namespace {
const char *T(const char *en, const char *jp) { return han2ui::Tr(en, jp); }
std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

unsigned MakeTex(const uint8_t *rgba, int w, int h)
{
	GLuint t = 0;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	return t;
}
void FreeTex(unsigned &t) { if (t) { GLuint g = t; glDeleteTextures(1, &g); t = 0; } }

bool g_pinned = false;          // keep the window open after opening something
bool g_gridMode = false;
bool g_iconLarge = false;   // list rows: 20 px icons (default) or 40 px
bool g_focusFilter = false;
bool g_listFocused = false;
std::vector<int> g_injKeys;     // scripted key presses (ImGuiKey values), consumed by the list handler
std::string g_injType;          // scripted type-to-search text
OpenRequest g_pending; bool g_havePending = false;
const char *kTypeNames[] = { "Characters", "Character data", "Images", "Sprite banks", "Parts", "Scripts / data", "Audio", "Text", "Palettes", "3D models", "Archives", "Other", "Shared / effect data" };
}

void ApplyPreview(std::shared_ptr<Preview> pv)
{
	State &st = S();
	if (pv->serial != st.prevSerial) return;   // the user moved on
	FreeTex(st.prevTex);
	st.prev = std::move(*pv);
	if (st.prev.kind == Preview::Image && !st.prev.rgba.empty()) { st.prevTex = MakeTex(st.prev.rgba.data(), st.prev.w, st.prev.h); st.prevTexW = st.prev.w; st.prevTexH = st.prev.h; st.view2.fit = true; }
	st.bankTexFrame = -2;
	if (st.prev.kind == Preview::Bank && st.prev.bank) {   // first drawable image
		st.bankFrame = -1;
		for (size_t i = 0; i < st.prev.bank->images.size(); i++) if (st.prev.bank->images[i].drawable()) { st.bankFrame = (int)i; break; }
	}
}

void ApplyThumb(const std::string &key, std::shared_ptr<std::vector<uint8_t>> rgba, int w, int h)
{
	State &st = S();
	Thumb &t = st.thumbCache[key];
	st.thumbsPending = std::max(0, st.thumbsPending - 1);
	if (t.tex) FreeTex(t.tex);
	t.tex = MakeTex(rgba->data(), w, h); t.w = w; t.h = h; t.state = 2; t.used = ++st.useClock;
	if (st.thumbCache.size() > 700) {   // evict the least recently used quarter
		std::vector<std::pair<uint64_t, std::string>> v;
		for (auto &kv : st.thumbCache) if (kv.second.state == 2) v.push_back({ kv.second.used, kv.first });
		std::sort(v.begin(), v.end());
		for (size_t i = 0; i < v.size() / 4; i++) { Thumb &o = st.thumbCache[v[i].second]; FreeTex(o.tex); o.state = 0; }
	}
}

Thumb *ThumbFor(int sourceIdx, int itemIdx, bool request)
{
	State &st = S();
	SourceP s = FindSource(sourceIdx);
	if (!s || itemIdx < 0 || itemIdx >= (int)s->items.size()) return nullptr;
	auto it = st.thumbCache.find(ThumbKey(*s, s->items[itemIdx]));
	if (it == st.thumbCache.end() || it->second.state == 0) { if (request && st.thumbsPending < 48) RequestThumb(sourceIdx, itemIdx); return nullptr; }
	if (it->second.state == 1 && it->second.wanted) it->second.wanted->store(g_uiFrame.load());
	if (it->second.state != 2) return nullptr;
	it->second.used = ++st.useClock;
	return &it->second;
}

// ---- view building ---------------------------------------------------------------------------------------------------------------------------------
static void RebuildView()
{
	State &st = S();
	st.view.clear(); st.viewDirty = false;
	SourceP s = FindSource(st.selSource);
	if (!s) return;
	const std::string flt = Lower(st.filter);
	for (uint32_t i = 0; i < s->items.size(); i++) {
		const Item &it = s->items[i];
		if (st.typeFilter >= 0 && (int)it.type != st.typeFilter) continue;
		if (!flt.empty() && Lower(it.name).find(flt) == std::string::npos) continue;
		st.view.push_back(i);
	}
	const auto &items = s->items;
	auto sortKey = [&](uint32_t i) { std::string k = Lower(items[i].name); for (auto &c : k) if (c == '_') c = '~'; return k; };   // _talk / _temp / _csel sort after the plain names
	auto cmpName = [&](uint32_t a, uint32_t b) { return sortKey(a) < sortKey(b); };
	if (st.sortCol == 0) std::stable_sort(st.view.begin(), st.view.end(), [&](uint32_t a, uint32_t b) { return st.sortDesc ? cmpName(b, a) : cmpName(a, b); });
	else if (st.sortCol == 1) std::stable_sort(st.view.begin(), st.view.end(), [&](uint32_t a, uint32_t b) { return st.sortDesc ? (int)items[a].type > (int)items[b].type : (int)items[a].type < (int)items[b].type; });
	else std::stable_sort(st.view.begin(), st.view.end(), [&](uint32_t a, uint32_t b) { return st.sortDesc ? items[a].size > items[b].size : items[a].size < items[b].size; });
	// keep the cursor on the same item
	if (st.cursor >= 0) { st.cursor = -1; for (size_t p = 0; p < st.view.size(); p++) if (st.sel.count(st.view[p])) { st.cursor = (int)p; break; } }
}

static void SelectOnly(int pos)
{
	State &st = S();
	if (pos < 0 || pos >= (int)st.view.size()) return;
	st.sel.clear(); st.sel.insert(st.view[pos]); st.anchor = st.cursor = pos; st.scrollToCursor = true;
	RequestPreview(st.selSource, (int)st.view[pos]);
}
static void ExtendTo(int pos)
{
	State &st = S();
	if (pos < 0 || pos >= (int)st.view.size()) return;
	if (st.anchor < 0) st.anchor = pos;
	st.sel.clear();
	for (int p = std::min(st.anchor, pos); p <= std::max(st.anchor, pos); p++) st.sel.insert(st.view[p]);
	st.cursor = pos; st.scrollToCursor = true;
	RequestPreview(st.selSource, (int)st.view[pos]);
}
static void ToggleAt(int pos)
{
	State &st = S();
	if (pos < 0 || pos >= (int)st.view.size()) return;
	const uint32_t i = st.view[pos];
	if (st.sel.count(i)) st.sel.erase(i); else st.sel.insert(i);
	st.anchor = st.cursor = pos;
	if (st.sel.count(i)) RequestPreview(st.selSource, (int)i);
}

static bool OpenAt(int pos, OpenRequest &req)
{
	State &st = S();
	if (pos < 0 || pos >= (int)st.view.size()) return false;
	SourceP s = FindSource(st.selSource);
	if (!s) return false;
	const Item &it = s->items[st.view[pos]];
	std::string msg;
	// formats with their own viewer window (IMG, FOB, typed tables, hex): open it straight from the archive, no extraction
	const bool ownViewer = it.type == Type::Image || it.type == Type::Script || it.type == Type::Other || it.type == Type::Audio || it.type == Type::Text || it.type == Type::Model || it.type == Type::Palette;
	if (ownViewer) {
		std::vector<uint8_t> b; std::string err;
		if (!ReadItem(*s, it, b, &err)) { st.status = err; return false; }
		uint32_t oi; const Source &o = Owner(*s, it, oi);
		const std::string cp932 = o.kind == Source::Pac || o.kind == Source::Gof1 ? it.key : utf82sj(it.name);
		han2ui::OpenFileViewer(cp932, std::move(b), o.label, o.IsArchive() ? o.path : std::string(), it.key);
		return false;
	}
	if (BuildOpenRequest(st.selSource, (int)st.view[pos], req, msg)) return true;
	if (!msg.empty()) st.status = msg;
	return false;
}

// ---- drawing helpers -------------------------------------------------------------------------------------------------------------------------------
static void DrawImageView(unsigned tex, int w, int h, ViewState &v, const char *id)
{
	ImGui::BeginChild(id, ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 p0 = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##imgdrag", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	const ImVec2 center(p0.x + avail.x * 0.5f, p0.y + avail.y * 0.5f);
	if (v.fit && w > 0 && h > 0) { v.zoom = std::min(avail.x / (float)w, avail.y / (float)h); v.zoom = std::clamp(v.zoom, 0.05f, 6.f); if (v.zoom > 1.f) v.zoom = std::floor(v.zoom); v.panX = v.panY = 0; }
	if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
		const float old = v.zoom; v.fit = false;
		v.zoom = std::clamp(v.zoom * (ImGui::GetIO().MouseWheel > 0 ? 1.25f : 0.8f), 0.05f, 32.f);
		const ImVec2 m = ImGui::GetIO().MousePos;   // zoom about the cursor
		const float k = v.zoom / old;
		v.panX = (m.x - center.x) - ((m.x - center.x) - v.panX) * k; v.panY = (m.y - center.y) - ((m.y - center.y) - v.panY) * k;
	}
	if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f))) { v.panX += ImGui::GetIO().MouseDelta.x; v.panY += ImGui::GetIO().MouseDelta.y; v.fit = false; }
	const ImVec2 sz(w * v.zoom, h * v.zoom);
	const ImVec2 a(center.x + v.panX - sz.x * 0.5f, center.y + v.panY - sz.y * 0.5f), b(a.x + sz.x, a.y + sz.y);
	dl->PushClipRect(p0, ImVec2(p0.x + avail.x, p0.y + avail.y), true);
	if (v.checker) {
		const float tile = 12.f;
		const ImVec2 c0(std::max(a.x, p0.x), std::max(a.y, p0.y)), c1(std::min(b.x, p0.x + avail.x), std::min(b.y, p0.y + avail.y));
		for (float y = c0.y; y < c1.y; y += tile)
			for (float x = c0.x; x < c1.x; x += tile) {
				const int ix = (int)((x - a.x) / tile), iy = (int)((y - a.y) / tile);
				dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + tile, c1.x), std::min(y + tile, c1.y)), ((ix + iy) & 1) ? IM_COL32(86, 86, 92, 255) : IM_COL32(64, 64, 70, 255));
			}
	}
	if (tex) dl->AddImage((ImTextureID)(intptr_t)tex, a, b);
	dl->AddRect(a, b, IM_COL32(120, 120, 130, 160));
	dl->PopClipRect();
	ImGui::EndChild();
}

static void ImageToolbar(ViewState &v, int w, int h)
{
	if (ImGui::SmallButton(T("Fit", "\xe3\x83\x95\xe3\x82\xa3\xe3\x83\x83\xe3\x83\x88"))) v.fit = true;
	ImGui::SameLine(); if (ImGui::SmallButton("1:1")) { v.fit = false; v.zoom = 1.f; v.panX = v.panY = 0; }
	ImGui::SameLine(); if (ImGui::SmallButton("2x")) { v.fit = false; v.zoom = 2.f; v.panX = v.panY = 0; }
	ImGui::SameLine(); if (ImGui::SmallButton("4x")) { v.fit = false; v.zoom = 4.f; v.panX = v.panY = 0; }
	ImGui::SameLine(); ImGui::Checkbox(T("checkerboard", "\xe5\xb8\x82\xe6\x9d\xbe"), &v.checker);
	ImGui::SameLine(); ImGui::TextDisabled("%.0f%%", v.zoom * 100.f);
}

static void DrawFacts(const Preview &pv)
{
	if (pv.facts.empty()) return;
	if (ImGui::BeginTable("##facts", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings)) {
		ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthFixed, 130.f); ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);
		for (auto &f : pv.facts) {
			ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", f.first.c_str());
			ImGui::TableSetColumnIndex(1); ImGui::PushTextWrapPos(0.f); ImGui::TextUnformatted(f.second.c_str()); ImGui::PopTextWrapPos();
		}
		ImGui::EndTable();
	}
}

static void DrawSwatches(const std::vector<uint32_t> &pal)
{
	if (pal.empty()) return;
	ImGui::TextDisabled("%s (%zu)", T("Palette", "\xe3\x83\x91\xe3\x83\xac\xe3\x83\x83\xe3\x83\x88"), pal.size());
	ImDrawList *dl = ImGui::GetWindowDrawList();
	const ImVec2 p = ImGui::GetCursorScreenPos();
	const float sz = pal.size() > 64 ? 5.f : 10.f; const int cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / sz));
	for (size_t i = 0; i < pal.size() && i < 256; i++) dl->AddRectFilled(ImVec2(p.x + (i % cols) * sz, p.y + (i / cols) * sz), ImVec2(p.x + (i % cols) * sz + sz - 1, p.y + (i / cols) * sz + sz - 1), (ImU32)pal[i] | 0xFF000000u);
	ImGui::Dummy(ImVec2(cols * sz, (float)(((std::min<size_t>(pal.size(), 256) + cols - 1) / cols) * sz + 4)));
}

static void DrawBankPreview(State &st)
{
	Preview &pv = st.prev;
	if (!pv.bank) return;
	cgm::Bank &bank = *pv.bank;
	static std::vector<int> present; static uint64_t presentSerial = 0;
	if (presentSerial != pv.serial) { present.clear(); for (size_t i = 0; i < bank.images.size(); i++) if (bank.images[i].present) present.push_back((int)i); presentSerial = pv.serial; }
	ImGui::SetNextItemWidth(120);
	char lbl[32]; snprintf(lbl, sizeof lbl, "%s %d", T("Palette", "\xe3\x83\x91\xe3\x83\xac\xe3\x83\x83\xe3\x83\x88"), st.bankPalette + 1);
	if (ImGui::BeginCombo("##bankpal", lbl)) {
		for (int i = 0; i < 8; i++) { char l2[32]; snprintf(l2, sizeof l2, "%s %d", T("Palette", "\xe3\x83\x91\xe3\x83\xac\xe3\x83\x83\xe3\x83\x88"), i + 1); if (ImGui::Selectable(l2, st.bankPalette == i)) st.bankPalette = i; }
		ImGui::EndCombo();
	}
	ImGui::SameLine(); ImageToolbar(st.view2, st.prevTexW, st.prevTexH);
	// decode the selected frame
	if (st.bankFrame >= 0 && (st.bankTexFrame != st.bankFrame || st.bankTexPalette != st.bankPalette)) {
		cgm::Rgba r; uint32_t pal[256];
		bank.palette(st.bankPalette, pal);
		FreeTex(st.prevTex); st.prevTexW = st.prevTexH = 0;
		if (bank.decode(st.bankFrame, r, pal) && r.w > 0 && r.h > 0) { st.prevTex = MakeTex(r.px.data(), r.w, r.h); st.prevTexW = r.w; st.prevTexH = r.h; st.view2.fit = true; }
		st.bankTexFrame = st.bankFrame; st.bankTexPalette = st.bankPalette;
	}
	ImGui::BeginChild("##frames", ImVec2(190, 0), true);
	ImGui::TextDisabled("%s (%zu)", T("Frames", "\xe3\x83\x95\xe3\x83\xac\xe3\x83\xbc\xe3\x83\xa0"), present.size());
	ImGuiListClipper clip; clip.Begin((int)present.size());
	while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
		const int n = present[r]; const cgm::Image &im = bank.images[n];
		char l[96]; snprintf(l, sizeof l, "%4d  %s  %dx%d", n, im.name, im.boundsW(), im.boundsH());
		if (ImGui::Selectable(l, st.bankFrame == n)) st.bankFrame = n;
	}
	ImGui::EndChild();
	ImGui::SameLine();
	DrawImageView(st.prevTex, st.prevTexW, st.prevTexH, st.view2, "##bankview");
}

static void DrawTextPreview(const Preview &pv)
{
	static std::vector<size_t> starts; static uint64_t serial = 0;
	if (serial != pv.serial) { starts.clear(); starts.push_back(0); for (size_t i = 0; i < pv.text.size(); i++) if (pv.text[i] == '\n') starts.push_back(i + 1); serial = pv.serial; }
	ImGui::BeginChild("##text", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	ImGuiListClipper clip; clip.Begin((int)starts.size());
	while (clip.Step()) for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++) {
		const size_t a = starts[i], b = i + 1 < (int)starts.size() ? starts[i + 1] : pv.text.size();
		size_t e = b; while (e > a && (pv.text[e - 1] == '\n' || pv.text[e - 1] == '\r')) e--;
		ImGui::TextUnformatted(pv.text.data() + a, pv.text.data() + e);
	}
	ImGui::EndChild();
}

static void DrawHexPreview(const Preview &pv)
{
	ImGui::BeginChild("##hex", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	const size_t n = pv.bytes.size();
	ImGuiListClipper clip; clip.Begin((int)((n + 15) / 16));
	while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
		const size_t o = (size_t)r * 16; char line[160]; int k = snprintf(line, sizeof line, "%08zX  ", o);
		for (size_t j = 0; j < 16; j++) k += (o + j < n) ? snprintf(line + k, sizeof line - k, "%02X ", pv.bytes[o + j]) : snprintf(line + k, sizeof line - k, "   ");
		line[k++] = ' ';
		for (size_t j = 0; j < 16 && o + j < n; j++) { uint8_t c = pv.bytes[o + j]; line[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.'; }
		line[k] = 0; ImGui::TextUnformatted(line);
	}
	if (pv.totalSize > n) ImGui::TextDisabled(T("... first %zu of %llu bytes shown", "... %zu / %llu \xe3\x83\x90\xe3\x82\xa4\xe3\x83\x88\xe3\x82\x92\xe8\xa1\xa8\xe7\xa4\xba"), n, (unsigned long long)pv.totalSize);
	ImGui::EndChild();
}

static void DrawPreviewPane(State &st, OpenRequest &req, bool &open)
{
	Preview &pv = st.prev;
	if (pv.kind == Preview::None) {
		ImGui::Dummy(ImVec2(0, 40));
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextDisabled("%s", T("Select an entry to preview it here. Nothing is extracted.\nDouble-click or Enter opens it in the editor.", "\xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe3\x82\x92\xe9\x81\xb8\xe3\x81\xb6\xe3\x81\xa8\xe3\x81\x93\xe3\x81\x93\xe3\x81\xab\xe3\x83\x97\xe3\x83\xac\xe3\x83\x93\xe3\x83\xa5\xe3\x83\xbc\xe3\x81\x97\xe3\x81\xbe\xe3\x81\x99\xef\xbc\x88\xe5\xb1\x95\xe9\x96\x8b\xe4\xb8\x8d\xe8\xa6\x81\xef\xbc\x89\xe3\x80\x82\n\xe3\x83\x80\xe3\x83\x96\xe3\x83\xab\xe3\x82\xaf\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf / Enter \xe3\x81\xa7\xe3\x82\xa8\xe3\x83\x87\xe3\x82\xa3\xe3\x82\xbf\xe3\x81\xab\xe9\x96\x8b\xe3\x81\x8f\xe3\x80\x82"));
		ImGui::PopTextWrapPos();
		return;
	}
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextUnformatted(pv.title.c_str());
	ImGui::PopTextWrapPos();
	if (pv.kind == Preview::Loading) { ImGui::TextDisabled("%s", T("Reading...", "\xe8\xaa\xad\xe3\x81\xbf\xe8\xbe\xbc\xe3\x81\xbf\xe4\xb8\xad...")); return; }
	if (pv.canOpen) {
		const bool isChar = pv.isCharacter || (st.prevSource >= 0 && st.prevItem >= 0 && FindSource(st.prevSource) && st.prevItem < (int)FindSource(st.prevSource)->items.size() &&
			(FindSource(st.prevSource)->items[st.prevItem].type == Type::Character || FindSource(st.prevSource)->items[st.prevItem].type == Type::CharData));
		if (ImGui::Button(isChar ? T("Open in editor  (Enter)", "\xe3\x82\xa8\xe3\x83\x87\xe3\x82\xa3\xe3\x82\xbf\xe3\x81\xa7\xe9\x96\x8b\xe3\x81\x8f  (Enter)") : T("Open  (Enter)", "\xe9\x96\x8b\xe3\x81\x8f  (Enter)"))) {
			SourceP s = FindSource(st.selSource);
			if (s && st.cursor >= 0) open = OpenAt(st.cursor, req);
		}
		ImGui::SameLine();
	}
	if (ImGui::Button(T("Extract...", "\xe6\x8a\xbd\xe5\x87\xba..."))) {
		if (!st.sel.empty()) { std::string d = BrowseForFolderUtf8(""); if (!d.empty()) StartExtract(st.selSource, std::vector<uint32_t>(st.sel.begin(), st.sel.end()), d); }
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Optional: copy the selected entries to a folder you choose", "\xe4\xbb\xbb\xe6\x84\x8f: \xe9\x81\xb8\xe6\x8a\x9e\xe3\x81\x97\xe3\x81\x9f\xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe3\x82\x92\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80\xe3\x81\xab\xe3\x82\xb3\xe3\x83\x94\xe3\x83\xbc"));
	switch (pv.kind) {
	case Preview::Image:
		if (pv.isCharacter) {
			// thumbnail on the left, facts on the right at narrow widths: stack them (the thumbnail first)
			const float th = std::min(ImGui::GetContentRegionAvail().y * 0.55f, 300.f);
			ImGui::BeginChild("##charthumb", ImVec2(0, th), true, ImGuiWindowFlags_NoScrollbar);
			const ImVec2 av = ImGui::GetContentRegionAvail();
			float z = std::min(av.x / pv.w, av.y / pv.h); if (z > 1.f) z = std::floor(z);
			const ImVec2 sz(pv.w * z, pv.h * z);
			ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (av.x - sz.x) * 0.5f, ImGui::GetCursorPosY() + (av.y - sz.y) * 0.5f));
			ImGui::Image((ImTextureID)(intptr_t)st.prevTex, sz);
			ImGui::EndChild();
			DrawFacts(pv);
		} else {
			DrawFacts(pv); DrawSwatches(pv.swatches);
			ImageToolbar(st.view2, pv.w, pv.h);
			DrawImageView(st.prevTex, pv.w, pv.h, st.view2, "##imgview");
		}
		break;
	case Preview::Bank: DrawFacts(pv); DrawBankPreview(st); break;
	case Preview::Text: DrawFacts(pv); DrawTextPreview(pv); break;
	case Preview::Hex: DrawFacts(pv); DrawHexPreview(pv); break;
	default: DrawFacts(pv); ImGui::PushTextWrapPos(0.f); ImGui::TextWrapped("%s", pv.text.c_str()); ImGui::PopTextWrapPos(); break;
	}
}

// ---- listing ---------------------------------------------------------------------------------------------------------------------------------------
static bool LightTheme() { const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg]; return (c.x + c.y + c.z) / 3.f > 0.55f; }
static ImU32 Adj(ImU32 c) { if (!LightTheme()) return c; return IM_COL32((int)((c & 255) * 0.5f), (int)(((c >> 8) & 255) * 0.5f), (int)(((c >> 16) & 255) * 0.5f), 255); }
static ImU32 TypeColorRaw(Type t)
{
	switch (t) {
	case Type::Character: return IM_COL32(255, 200, 90, 255);
	case Type::CharData: return IM_COL32(200, 160, 90, 255);
	case Type::Shared: return IM_COL32(170, 150, 210, 255);
	case Type::Image: return IM_COL32(120, 200, 255, 255);
	case Type::CgBank: return IM_COL32(150, 220, 160, 255);
	case Type::Parts: return IM_COL32(220, 160, 255, 255);
	case Type::Archive: return IM_COL32(255, 140, 120, 255);
	case Type::Audio: return IM_COL32(180, 180, 255, 255);
	default: return IM_COL32(190, 190, 195, 255);
	}
}
static ImU32 TypeColor(Type t);
static const char *TypeBadge(Type t)
{
	switch (t) {
	case Type::Character: return "CH"; case Type::CharData: return "cd"; case Type::Shared: return "SH"; case Type::Image: return "IM"; case Type::CgBank: return "CG";
	case Type::Parts: return "PT"; case Type::Script: return "FB"; case Type::Audio: return "AU"; case Type::Text: return "TX"; case Type::Palette: return "PL";
	case Type::Model: return "3D"; case Type::Archive: return "AR"; default: return "--";
	}
}
static void DrawBadge(ImDrawList *dl, ImVec2 p, float size, Type t)
{
	const ImU32 c = ImGui::GetColorU32(ImGuiCol_TextDisabled);
	dl->AddRect(p, ImVec2(p.x + size, p.y + size), c, 3.f);
	const ImVec2 ts = ImGui::CalcTextSize(TypeBadge(t));
	dl->AddText(ImVec2(p.x + (size - ts.x) * 0.5f, p.y + (size - ts.y) * 0.5f), TypeColor(t), TypeBadge(t));
}
static ImU32 TypeColor(Type t) { return t == Type::Other || t == Type::Text || t == Type::Script ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : Adj(TypeColorRaw(t)); }

static std::string SizeText(uint64_t n)
{
	char b[32];
	if (n >= (1u << 20)) snprintf(b, sizeof b, "%.1f MB", n / 1048576.0); else if (n >= 1024) snprintf(b, sizeof b, "%.1f KB", n / 1024.0); else snprintf(b, sizeof b, "%llu B", (unsigned long long)n);
	return b;
}

static void ItemContextMenu(State &st, OpenRequest &req, bool &open)
{
	if (!ImGui::BeginPopup("##itemctx")) return;
	SourceP s = FindSource(st.selSource);
	if (ImGui::MenuItem(T("Open in editor", "\xe3\x82\xa8\xe3\x83\x87\xe3\x82\xa3\xe3\x82\xbf\xe3\x81\xa7\xe9\x96\x8b\xe3\x81\x8f"), "Enter") && st.cursor >= 0) open = OpenAt(st.cursor, req);
	ImGui::Separator();
	char l[96]; snprintf(l, sizeof l, "%s (%zu)...", T("Extract selected", "\xe9\x81\xb8\xe6\x8a\x9e\xe3\x82\x92\xe6\x8a\xbd\xe5\x87\xba"), st.sel.size());
	if (ImGui::MenuItem(l, nullptr, false, !st.sel.empty() && !st.prog.running)) { std::string d = BrowseForFolderUtf8(""); if (!d.empty()) StartExtract(st.selSource, std::vector<uint32_t>(st.sel.begin(), st.sel.end()), d); }
	if (ImGui::MenuItem(T("Extract all in this list...", "\xe3\x81\x93\xe3\x81\xae\xe4\xb8\x80\xe8\xa6\xa7\xe3\x82\x92\xe3\x81\xbe\xe3\x81\xa8\xe3\x82\x81\xe3\x81\xa6\xe6\x8a\xbd\xe5\x87\xba..."), nullptr, false, !st.view.empty() && !st.prog.running)) {
		std::string d = BrowseForFolderUtf8(""); if (!d.empty()) StartExtract(st.selSource, std::vector<uint32_t>(st.view.begin(), st.view.end()), d);
	}
	if (ImGui::MenuItem(T("Copy name", "\xe5\x90\x8d\xe5\x89\x8d\xe3\x82\x92\xe3\x82\xb3\xe3\x83\x94\xe3\x83\xbc")) && s && st.cursor >= 0) ImGui::SetClipboardText(s->items[st.view[st.cursor]].name.c_str());
	ImGui::EndPopup();
}

static bool HandleKeys(State &st, OpenRequest &req, int cols)
{
	bool open = false;
	ImGuiIO &io = ImGui::GetIO();
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
	auto pressed = [&](ImGuiKey k) {
		for (size_t i = 0; i < g_injKeys.size(); i++) if (g_injKeys[i] == (int)k) { g_injKeys.erase(g_injKeys.begin() + i); return true; }
		return focused && ImGui::IsKeyPressed(k, true);
	};
	const int n = (int)st.view.size();
	if (n == 0) return false;
	const bool shift = io.KeyShift;
	auto move = [&](int target) { target = std::clamp(target, 0, n - 1); if (shift) ExtendTo(target); else SelectOnly(target); };
	const int cur = st.cursor < 0 ? 0 : st.cursor;
	const int page = 12;
	if (pressed(ImGuiKey_DownArrow)) move(st.cursor < 0 ? 0 : cur + cols);
	if (pressed(ImGuiKey_UpArrow)) move(st.cursor < 0 ? 0 : cur - cols);
	if (cols > 1) { if (pressed(ImGuiKey_RightArrow)) move(cur + 1); if (pressed(ImGuiKey_LeftArrow)) move(cur - 1); }
	if (pressed(ImGuiKey_PageDown)) move(cur + page * cols);
	if (pressed(ImGuiKey_PageUp)) move(cur - page * cols);
	if (pressed(ImGuiKey_Home)) move(0);
	if (pressed(ImGuiKey_End)) move(n - 1);
	if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { if (st.cursor >= 0) open = OpenAt(st.cursor, req); }
	if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) { st.sel.clear(); for (int p = 0; p < n; p++) st.sel.insert(st.view[p]); }
	// type-to-search: letters jump to the next entry whose name starts with what was typed
	std::string typed = g_injType; g_injType.clear();
	if (focused) for (int i = 0; i < io.InputQueueCharacters.Size; i++) { ImWchar c = io.InputQueueCharacters[i]; if (c >= 32 && c < 127) typed += (char)c; }
	if (!typed.empty()) {
		if (ImGui::GetTime() - st.typeAt > 1.0) st.typeBuf.clear();
		st.typeAt = ImGui::GetTime(); st.typeBuf += typed;
		SourceP s = FindSource(st.selSource);
		if (s) {
			const std::string q = Lower(st.typeBuf);
			auto baseName = [&](const Item &it) { std::string b = Lower(it.name); size_t sl = b.find_last_of('/'); return sl == std::string::npos ? b : b.substr(sl + 1); };
			for (int k = 0; k < n; k++) {
				const int p = (cur + (st.typeBuf.size() == 1 ? 1 : 0) + k) % n;
				const std::string b = baseName(s->items[st.view[p]]);
				if (b.compare(0, q.size(), q) == 0 || (q.size() > 0 && Lower(s->items[st.view[p]].name).compare(0, q.size(), q) == 0)) { SelectOnly(p); break; }
			}
		}
	}
	return open;
}

static void HandleClick(State &st, int pos, OpenRequest &req, bool &open, bool doubleClick, bool rightClick)
{
	ImGuiIO &io = ImGui::GetIO();
	if (rightClick) { if (!st.sel.count(st.view[pos])) SelectOnly(pos); ImGui::OpenPopup("##itemctx"); return; }
	if (io.KeyCtrl) ToggleAt(pos); else if (io.KeyShift) ExtendTo(pos); else SelectOnly(pos);
	if (doubleClick) open = OpenAt(pos, req);
}

static void DrawList(State &st, OpenRequest &req, bool &open)
{
	SourceP s = FindSource(st.selSource);
	if (!s) { ImGui::TextDisabled("%s", T("Open a game folder or an archive to start.", "\xe3\x82\xb2\xe3\x83\xbc\xe3\x83\xa0\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80\xe3\x81\xbe\xe3\x81\x9f\xe3\x81\xaf\xe3\x82\xa2\xe3\x83\xbc\xe3\x82\xab\xe3\x82\xa4\xe3\x83\x96\xe3\x82\x92\xe9\x96\x8b\xe3\x81\x84\xe3\x81\xa6\xe3\x81\x8f\xe3\x81\xa0\xe3\x81\x95\xe3\x81\x84\xe3\x80\x82")); return; }
	if (s->state == Source::Failed) { ImGui::TextColored(ImVec4(1, .5f, .3f, 1), "%s", s->error.c_str()); return; }
	if (s->state == Source::Loading) { ImGui::TextDisabled("%s", T("Reading the entry table...", "\xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe8\xa1\xa8\xe3\x82\x92\xe8\xaa\xad\xe3\x81\xbf\xe8\xbe\xbc\xe3\x81\xbf\xe4\xb8\xad...")); return; }
	if (st.viewDirty) RebuildView();
	const int n = (int)st.view.size();
	const bool thumbs = st.thumbs;
	const float rowH = thumbs ? (g_iconLarge ? 42.f : 24.f) : ImGui::GetTextLineHeightWithSpacing() + 2.f;
	const float icon = rowH - 4.f;
	int cols = 1;
	if (g_gridMode) {
		const float cw = 128.f, ch = 148.f;
		cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize) / cw));
		ImGui::BeginChild("##grid", ImVec2(0, 0), false);
		const int rows = (n + cols - 1) / cols;
		if (st.scrollToCursor && st.cursor >= 0) { const float y = (float)(st.cursor / cols) * ch; if (y < ImGui::GetScrollY()) ImGui::SetScrollY(y); else if (y + ch > ImGui::GetScrollY() + ImGui::GetWindowHeight()) ImGui::SetScrollY(y + ch - ImGui::GetWindowHeight()); st.scrollToCursor = false; }
		ImGuiListClipper clip; clip.Begin(rows, ch);
		while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
			const ImVec2 rowPos = ImGui::GetCursorScreenPos();   // every tile of the row is placed from here; the cursor moves down one row at the end (a tile's own button must not advance it)
			for (int c = 0; c < cols; c++) {
				const int pos = r * cols + c; if (pos >= n) break;
				const Item &it = s->items[st.view[pos]];
				ImGui::PushID(pos);
				const ImVec2 p = ImVec2(rowPos.x + c * cw, rowPos.y);
				ImGui::SetCursorScreenPos(p);
				const bool selected = st.sel.count(st.view[pos]) > 0;
				ImGui::InvisibleButton("##card", ImVec2(cw - 6, ch - 6));
				const bool hov = ImGui::IsItemHovered();
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) HandleClick(st, pos, req, open, ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left), false);
				if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { if (!open) open = OpenAt(pos, req); }
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) HandleClick(st, pos, req, open, false, true);
				ImDrawList *dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(p, ImVec2(p.x + cw - 6, p.y + ch - 6), ImGui::GetColorU32(selected ? ImGuiCol_Header : hov ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg), 4.f);
				dl->AddRect(p, ImVec2(p.x + cw - 6, p.y + ch - 6), ImGui::GetColorU32(selected ? ImGuiCol_HeaderActive : ImGuiCol_Border), 4.f);
				Thumb *t = ThumbFor(st.selSource, (int)st.view[pos], true);
				if (t) { const float z = std::min(100.f / t->w, 100.f / t->h); const ImVec2 sz(t->w * z, t->h * z); const ImVec2 q(p.x + (cw - 6 - sz.x) * 0.5f, p.y + 6 + (100 - sz.y) * 0.5f); dl->AddImage((ImTextureID)(intptr_t)t->tex, q, ImVec2(q.x + sz.x, q.y + sz.y)); }
				else DrawBadge(dl, ImVec2(p.x + (cw - 6 - 48) * 0.5f, p.y + 30), 48, it.type);
				std::string nm = it.name; const size_t sl = nm.find_last_of('/'); if (sl != std::string::npos) nm = nm.substr(sl + 1);
				dl->PushClipRect(p, ImVec2(p.x + cw - 8, p.y + ch - 6), true);
				dl->AddText(ImVec2(p.x + 6, p.y + 110), TypeColor(it.type), nm.c_str());
				dl->AddText(ImVec2(p.x + 6, p.y + 126), ImGui::GetColorU32(ImGuiCol_TextDisabled), (std::string(TypeName(it.type)) + "  " + SizeText(it.size)).c_str());
				dl->PopClipRect();
				if (hov) ImGui::SetTooltip("%s", ItemPathText(*s, it).c_str());
				ImGui::PopID();
			}
			ImGui::SetCursorScreenPos(rowPos);
			ImGui::Dummy(ImVec2(cols * cw, ch));
		}
		ImGui::EndChild();
		ItemContextMenu(st, req, open);
		return;
	}
	ImGuiTableFlags tf = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("##ents", 3, tf)) return;
	ImGui::TableSetupColumn(T("Name", "\xe5\x90\x8d\xe5\x89\x8d"), ImGuiTableColumnFlags_DefaultSort, 3.f);
	ImGui::TableSetupColumn(T("Type", "\xe7\xa8\xae\xe9\xa1\x9e"), 0, 1.1f);
	ImGui::TableSetupColumn(T("Size", "\xe3\x82\xb5\xe3\x82\xa4\xe3\x82\xba"), ImGuiTableColumnFlags_PreferSortDescending, 0.8f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();
	if (ImGuiTableSortSpecs *sp = ImGui::TableGetSortSpecs()) {
		if (sp->SpecsDirty && sp->SpecsCount > 0) { st.sortCol = sp->Specs[0].ColumnIndex; st.sortDesc = sp->Specs[0].SortDirection == ImGuiSortDirection_Descending; st.viewDirty = true; sp->SpecsDirty = false; }
	}
	if (st.scrollToCursor && st.cursor >= 0) {
		const float y = (float)st.cursor * rowH;
		const float top = ImGui::GetScrollY(), h = ImGui::GetWindowHeight() - rowH * 1.5f;
		if (y < top) ImGui::SetScrollY(y); else if (y + rowH > top + h) ImGui::SetScrollY(y + rowH - h);
		st.scrollToCursor = false;
	}
	ImGuiListClipper clip; clip.Begin(n, rowH);
	while (clip.Step()) for (int pos = clip.DisplayStart; pos < clip.DisplayEnd; pos++) {
		const uint32_t idx = st.view[pos]; const Item &it = s->items[idx];
		ImGui::TableNextRow(0, rowH);
		ImGui::TableSetColumnIndex(0);
		ImGui::PushID(pos);
		const bool selected = st.sel.count(idx) > 0;
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::Selectable("##row", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowH));
		const bool hov = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) HandleClick(st, pos, req, open, ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left), false);
		else if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !open) open = OpenAt(pos, req);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) HandleClick(st, pos, req, open, false, true);
		if (hov) ImGui::SetTooltip("%s", ItemPathText(*s, it).c_str());
		ImDrawList *dl = ImGui::GetWindowDrawList();
		float tx = p.x + 2;
		if (thumbs) {
			Thumb *t = ThumbFor(st.selSource, (int)idx, true);
			if (t) { const float z = std::min(icon / t->w, icon / t->h); const ImVec2 sz(t->w * z, t->h * z); const ImVec2 q(p.x + 2 + (icon - sz.x) * 0.5f, p.y + 2 + (icon - sz.y) * 0.5f); dl->AddImage((ImTextureID)(intptr_t)t->tex, q, ImVec2(q.x + sz.x, q.y + sz.y)); }
			else DrawBadge(dl, ImVec2(p.x + 2, p.y + 2), icon, it.type);
			tx += icon + 6;
		}
		dl->AddText(ImVec2(tx, p.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), it.name.c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowH - ImGui::GetTextLineHeight()) * 0.5f - 2);
		ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(it.type)); ImGui::TextUnformatted(TypeName(it.type)); ImGui::PopStyleColor();
		ImGui::TableSetColumnIndex(2);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowH - ImGui::GetTextLineHeight()) * 0.5f - 2);
		ImGui::TextUnformatted(SizeText(it.size).c_str());
		ImGui::PopID();
	}
	ImGui::EndTable();
	ItemContextMenu(st, req, open);
}

// ---- sources --------------------------------------------------------------------------------------------------------------------------------------
static void DrawSources(State &st)
{
	int closeIdx = -1;
	for (int i = 0; i < (int)st.sources.size(); i++) {
		const SourceP &s = st.sources[i];
		ImGui::PushID(i);
		std::string l = s->label;
		if (s->kind == Source::Merged) {
			const auto it = std::find_if(st.groups.begin(), st.groups.end(), [&](const std::shared_ptr<Group> &g) { return g->id == s->group; });
			if (it != st.groups.end() && (*it)->info.game != Game::None && (*it)->info.game != Game::Generic) l = std::string(GameName((*it)->info.game)) + " - " + s->label;
		}
		char buf[320];
		if (s->state == Source::Loading) snprintf(buf, sizeof buf, "%s  ...", l.c_str());
		else if (s->state == Source::Failed) snprintf(buf, sizeof buf, "%s  (!)", l.c_str());
		else snprintf(buf, sizeof buf, "%s  (%zu)", l.c_str(), s->items.size());
		const float indent = s->kind == Source::Merged || s->kind == Source::Folder ? 0.f : 12.f;
		if (indent > 0) ImGui::Indent(indent);
		if (ImGui::Selectable(buf, st.selSource == i)) { st.selSource = i; st.viewDirty = true; st.sel.clear(); st.cursor = st.anchor = -1; st.prev = Preview(); st.prevSource = st.prevItem = -1; }
		if (indent > 0) ImGui::Unindent(indent);
		if (ImGui::IsItemHovered()) { std::string tip = s->path.empty() ? s->label : s->path; if (!s->describe.empty()) tip += "\n" + s->describe; ImGui::SetTooltip("%s", tip.c_str()); }
		if (ImGui::BeginPopupContextItem("##srcctx")) {
			if (ImGui::MenuItem(T("Close", "\xe9\x96\x89\xe3\x81\x98\xe3\x82\x8b"))) closeIdx = i;
			if (!s->path.empty() && ImGui::MenuItem(T("Show in Explorer", "\xe3\x82\xa8\xe3\x82\xaf\xe3\x82\xb9\xe3\x83\x97\xe3\x83\xad\xe3\x83\xbc\xe3\x83\xa9\xe3\x83\xbc\xe3\x81\xa7\xe8\xa1\xa8\xe7\xa4\xba"))) { std::wstring a = L"/select,\"" + Utf8ToWide(s->path) + L"\""; if (s->kind == Source::Folder) a = L"\"" + Utf8ToWide(s->path) + L"\""; ShellExecuteW(nullptr, L"open", L"explorer.exe", a.c_str(), nullptr, SW_SHOWNORMAL); }
			if (s->IsArchive() && ImGui::MenuItem(T("Extract everything...", "\xe3\x81\x99\xe3\x81\xb9\xe3\x81\xa6\xe6\x8a\xbd\xe5\x87\xba..."), nullptr, false, !st.prog.running)) { std::string d = BrowseForFolderUtf8(""); if (!d.empty()) StartExtract(i, {}, d); }
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
	if (closeIdx >= 0) CloseSource(closeIdx);
}

static void FolderDialog(Game want)
{
	LoadSettings();
	char key[48]; snprintf(key, sizeof key, "Dir_%d", (int)want);
	char b[1024]{}; GetPrivateProfileStringA("browser", key, "", b, sizeof b, [] { static std::string p; char c[512]; DWORD n = GetCurrentDirectoryA(512, c); p = std::string(c, n) + "\\han2_settings.ini"; return p.c_str(); }());
	const std::string d = BrowseForFolderUtf8(b[0] ? std::string(b) : S().lastBrowsed);
	if (d.empty()) return;
	std::string err = OpenPath(d);
	if (!err.empty()) { S().status = err; return; }
	WritePrivateProfileStringA("browser", key, d.c_str(), [] { static std::string p; char c[512]; DWORD n = GetCurrentDirectoryA(512, c); p = std::string(c, n) + "\\han2_settings.ini"; return p.c_str(); }());
	const GameInfo gi = DetectGame(d);
	if (want != Game::None && gi.game != want) S().status = std::string(T("This folder does not look like ", "\xe3\x81\x93\xe3\x81\xae\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80\xe3\x81\xaf ")) + GameName(want) + " (" + (gi.game == Game::None ? "nothing recognised" : GameName(gi.game)) + ")";
}
void OpenGameFolderDialog(Game want) { FolderDialog(want); }
void OpenArchiveDialog()
{
	std::string p = FileDialog(fileType::HAN2, false);
	if (p.empty()) return;
	std::string err = OpenPath(p);
	if (!err.empty()) S().status = err;
}

// ---- the window ----------------------------------------------------------------------------------------------------------------------------------------
static void Toolbar(State &st)
{
	if (ImGui::Button(T("Open game folder...", "\xe3\x82\xb2\xe3\x83\xbc\xe3\x83\xa0\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80\xe3\x82\x92\xe9\x96\x8b\xe3\x81\x8f..."))) ImGui::OpenPopup("##gamemenu");
	if (ImGui::BeginPopup("##gamemenu")) {
		if (ImGui::MenuItem(T("Detect the game automatically...", "\xe8\x87\xaa\xe5\x8b\x95\xe5\x88\xa4\xe5\x88\xa5..."))) FolderDialog(Game::None);
		ImGui::Separator();
		for (const GameDef &d : Games()) if (ImGui::MenuItem((std::string(d.name) + "...").c_str())) FolderDialog(d.id);
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button(T("Open archive / file...", "\xe3\x82\xa2\xe3\x83\xbc\xe3\x82\xab\xe3\x82\xa4\xe3\x83\x96/\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab\xe3\x82\x92\xe9\x96\x8b\xe3\x81\x8f..."))) OpenArchiveDialog();
	ImGui::SameLine();
	if (ImGui::Button(T("Recent", "\xe6\x9c\x80\xe8\xbf\x91")) ) ImGui::OpenPopup("##recentmenu");
	if (ImGui::BeginPopup("##recentmenu")) {
		if (st.recent.empty()) ImGui::TextDisabled("%s", T("Nothing yet", "\xe3\x81\xbe\xe3\x81\xa0\xe3\x81\x82\xe3\x82\x8a\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93"));
		std::string pick;
		for (auto &r : st.recent) if (ImGui::MenuItem(r.c_str())) pick = r;
		ImGui::EndPopup();
		if (!pick.empty()) { std::string e = OpenPath(pick); if (!e.empty()) st.status = e; }
	}
	ImGui::SameLine(0, 18);
	if (g_focusFilter) { ImGui::SetKeyboardFocusHere(); g_focusFilter = false; }
	ImGui::SetNextItemWidth(220);
	if (ImGui::InputTextWithHint("##filter", T("Filter (Ctrl+F)", "\xe7\xb5\x9e\xe3\x82\x8a\xe8\xbe\xbc\xe3\x81\xbf (Ctrl+F)"), st.filter, sizeof st.filter)) st.viewDirty = true;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(130);
	const char *cur = st.typeFilter < 0 ? T("All types", "\xe3\x81\x99\xe3\x81\xb9\xe3\x81\xa6") : kTypeNames[st.typeFilter];
	if (ImGui::BeginCombo("##typefilter", cur)) {
		if (ImGui::Selectable(T("All types", "\xe3\x81\x99\xe3\x81\xb9\xe3\x81\xa6"), st.typeFilter < 0)) { st.typeFilter = -1; st.viewDirty = true; }
		for (int t = 0; t < 13; t++) if (ImGui::Selectable(kTypeNames[t], st.typeFilter == t)) { st.typeFilter = t; st.viewDirty = true; }
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Checkbox(T("Thumbnails", "\xe3\x82\xb5\xe3\x83\xa0\xe3\x83\x8d\xe3\x82\xa4\xe3\x83\xab"), &st.thumbs)) SaveSettings();
	ImGui::SameLine();
	if (!g_gridMode) { ImGui::SameLine(); if (ImGui::Button(g_iconLarge ? T("Icons: large", "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xb3\xe3\x83\xb3: \xe5\xa4\xa7") : T("Icons: small", "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xb3\xe3\x83\xb3: \xe5\xb0\x8f"))) g_iconLarge = !g_iconLarge; ImGui::SameLine(); }
	if (ImGui::Button(g_gridMode ? T("List", "\xe3\x83\xaa\xe3\x82\xb9\xe3\x83\x88") : T("Grid", "\xe3\x82\xb0\xe3\x83\xaa\xe3\x83\x83\xe3\x83\x89"))) g_gridMode = !g_gridMode;
	ImGui::SameLine();
	ImGui::Checkbox(T("Stay open", "\xe9\x96\x8b\xe3\x81\x84\xe3\x81\x9f\xe3\x81\xbe\xe3\x81\xbe"), &g_pinned);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Keep this window open after opening a character", "\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xa9\xe3\x82\x92\xe9\x96\x8b\xe3\x81\x84\xe3\x81\xa6\xe3\x82\x82\xe3\x81\x93\xe3\x81\xae\xe3\x82\xa6\xe3\x82\xa3\xe3\x83\xb3\xe3\x83\x89\xe3\x82\xa6\xe3\x82\x92\xe9\x96\x89\xe3\x81\x98\xe3\x81\xaa\xe3\x81\x84"));
	ImGui::SameLine();
	{ const char *lg = i18n::language == 1 ? "EN" : "JP"; if (ImGui::SmallButton(lg)) { i18n::language = 1 - i18n::language; han2ui::SaveHan2Settings(); } }
}

bool Draw(OpenRequest &req)
{
	g_uiFrame++;
	PumpMain();
	State &st = S();
	LoadSettings();
	bool open = false;
	if (g_havePending) { req = g_pending; g_havePending = false; return true; }
	if (!show) return false;
	if (st.sources.empty() && !st.lastBrowsed.empty()) { std::string e = OpenPath(st.lastBrowsed); (void)e; show = true; }   // restore the last browsed folder / archive
	{ const ImGuiViewport *vp = ImGui::GetMainViewport(); ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 24, vp->WorkPos.y + 30), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(std::min(1240.f, vp->WorkSize.x - 48.f), std::min(760.f, vp->WorkSize.y - 60.f)), ImGuiCond_FirstUseEver); }
	if (!ImGui::Begin((std::string(T("Archive Browser", "\xe3\x82\xa2\xe3\x83\xbc\xe3\x82\xab\xe3\x82\xa4\xe3\x83\x96\xe3\x83\x96\xe3\x83\xa9\xe3\x82\xa6\xe3\x82\xb6")) + "###abrowser").c_str(), &show)) { ImGui::End(); return false; }
	Toolbar(st);
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) g_focusFilter = true;
	ImGui::Separator();
	const float bottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y + 4.f;
	if (ImGui::BeginTable("##layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings, ImVec2(0, -bottom))) {
		ImGui::TableSetupColumn("src", ImGuiTableColumnFlags_WidthFixed, 230.f);
		ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 3.f);
		ImGui::TableSetupColumn("prev", ImGuiTableColumnFlags_WidthStretch, 2.4f);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("##sources", ImVec2(0, 0), false);
		ImGui::TextDisabled("%s", T("Sources", "\xe3\x82\xbd\xe3\x83\xbc\xe3\x82\xb9"));
		DrawSources(st);
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("##entries", ImVec2(0, 0), false);
		{
			int cols = 1; if (g_gridMode) cols = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 128.f));
			if (HandleKeys(st, req, cols)) open = true;
			DrawList(st, req, open);
		}
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(2);
		ImGui::BeginChild("##preview", ImVec2(0, 0), false);
		DrawPreviewPane(st, req, open);
		ImGui::EndChild();
		ImGui::EndTable();
	}
	// status bar
	if (st.prog.running) {
		ImGui::ProgressBar(st.prog.total ? (float)st.prog.done / (float)st.prog.total : 0.f, ImVec2(220, 0));
		ImGui::SameLine(); ImGui::Text(T("Extracting %d / %d to %s", "\xe6\x8a\xbd\xe5\x87\xba\xe4\xb8\xad %d / %d -> %s"), st.prog.done.load(), st.prog.total.load(), st.prog.dest.c_str());
		ImGui::SameLine(); if (ImGui::SmallButton(T("Cancel", "\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab"))) st.prog.cancel = true;
	} else if (st.prog.total > 0 && !st.prog.dest.empty()) {
		ImGui::Text(T("Extracted %d files to %s (%d failed)", "%d \xe5\x80\x8b\xe3\x82\x92 %s \xe3\x81\xab\xe6\x8a\xbd\xe5\x87\xba (\xe5\xa4\xb1\xe6\x95\x97 %d)"), st.prog.done.load() - st.prog.failed.load(), st.prog.dest.c_str(), st.prog.failed.load());
	} else {
		SourceP s = FindSource(st.selSource);
		ImGui::TextDisabled("%s", !st.status.empty() ? st.status.c_str() : (s ? s->describe.c_str() : ""));
		if (s && !st.status.empty()) { ImGui::SameLine(); ImGui::TextDisabled("  |  %s", s->describe.c_str()); }
	}
	ImGui::SameLine(ImGui::GetWindowWidth() - 360);
	ImGui::TextDisabled("%zu %s, %zu %s", st.view.size(), T("shown", "\xe8\xa1\xa8\xe7\xa4\xba"), st.sel.size(), T("selected", "\xe9\x81\xb8\xe6\x8a\x9e"));
	ImGui::End();
	if (open) {
		if (!g_pinned && req.kind != OpenRequest::None) show = false;
		return req.kind != OpenRequest::None;
	}
	return false;
}

// ---- scripted UI ----------------------------------------------------------------------------------------------------------------------------------
bool ScriptCommand(const std::string &cmd, const std::string &arg, std::string *msg)
{
	State &st = S();
	auto say = [&](const std::string &m) { if (msg) *msg = m; };
	if (cmd == "browse") { show = true; std::string e = OpenPath(arg); if (!e.empty()) { say(e); return false; } return true; }
	if (cmd == "source") {
		const std::string q = Lower(arg);
		for (int i = 0; i < (int)st.sources.size(); i++) if (Lower(st.sources[i]->label).find(q) != std::string::npos) { st.selSource = i; st.viewDirty = true; st.sel.clear(); st.cursor = st.anchor = -1; st.prev = Preview(); return true; }
		say("no source matching " + arg); return false;
	}
	if (cmd == "filter") { snprintf(st.filter, sizeof st.filter, "%s", arg.c_str()); st.viewDirty = true; return true; }
	if (cmd == "type") {
		if (arg == "all") st.typeFilter = -1;
		else { st.typeFilter = -2; for (int t = 0; t < 13; t++) if (Lower(kTypeNames[t]).find(Lower(arg)) == 0) st.typeFilter = t; if (st.typeFilter == -2) { st.typeFilter = -1; say("unknown type"); return false; } }
		st.viewDirty = true; return true;
	}
	if (cmd == "select") {
		if (st.viewDirty) RebuildView();
		SourceP s = FindSource(st.selSource); if (!s) return false;
		const std::string q = Lower(arg);
		for (int p = 0; p < (int)st.view.size(); p++) if (Lower(s->items[st.view[p]].name) == q) { SelectOnly(p); return true; }   // an exact name wins over a substring
		for (int p = 0; p < (int)st.view.size(); p++) if (Lower(s->items[st.view[p]].name).find(q) != std::string::npos) { SelectOnly(p); return true; }
		say("no entry matching " + arg); return false;
	}
	if (cmd == "open") {
		if (st.cursor < 0) { say("nothing selected"); return false; }
		OpenRequest r; if (OpenAt(st.cursor, r)) { g_pending = r; g_havePending = true; if (!g_pinned) show = false; } return true;
	}
	if (cmd == "key") {
		static const std::pair<const char *, ImGuiKey> keys[] = { {"Up", ImGuiKey_UpArrow}, {"Down", ImGuiKey_DownArrow}, {"Left", ImGuiKey_LeftArrow}, {"Right", ImGuiKey_RightArrow}, {"Enter", ImGuiKey_Enter}, {"Home", ImGuiKey_Home}, {"End", ImGuiKey_End}, {"PageDown", ImGuiKey_PageDown}, {"PageUp", ImGuiKey_PageUp} };
		for (auto &k : keys) if (arg == k.first) { g_injKeys.push_back((int)k.second); return true; }
		say("unknown key"); return false;
	}
	if (cmd == "info") { char b[256]; snprintf(b, sizeof b, "sources=%zu sel=%d view=%zu cursor=%d selected=%zu type=%d status=%s", st.sources.size(), st.selSource, st.view.size(), st.cursor, st.sel.size(), st.typeFilter, st.status.c_str()); say(b); return false; }
	if (cmd == "typeahead") { g_injType += arg; return true; }
	if (cmd == "frame") { st.bankFrame = atoi(arg.c_str()); return true; }
	if (cmd == "palette") { st.bankPalette = std::clamp(atoi(arg.c_str()), 0, 7); return true; }
	if (cmd == "zoom") { if (arg == "fit") st.view2.fit = true; else { st.view2.fit = false; st.view2.zoom = (float)atof(arg.c_str()); st.view2.panX = st.view2.panY = 0; } return true; }
	if (cmd == "thumbs") { st.thumbs = arg != "off"; return true; }
	if (cmd == "icons") { g_iconLarge = arg == "large"; return true; }
	if (cmd == "grid") { g_gridMode = arg != "off"; return true; }
	if (cmd == "pin") { g_pinned = arg != "off"; return true; }
	if (cmd == "show") { show = arg != "off"; return true; }
	say("unknown command " + cmd); return false;
}

// ---- welcome screen ---------------------------------------------------------------------------------------------------------------------------------
// Driven by the game table (game_table.cpp): one row per title, grouped by family. A row's folder comes from the install scan (remembered folder, Steam
// libraries, common paths); "Locate..." picks one by hand and remembers it.
static void OpenGameRow(const GameDef &d)
{
	State &st = S();
	const Install &in = InstallOf(d.id);
	if (!in.path.empty()) { std::string e = OpenPath(in.path); if (!e.empty()) st.status = e; else RememberDir(d.id, in.path); }
	else FolderDialog(d.id);
}

std::string DrawWelcome(const std::vector<std::string> &recentFiles)
{
	std::string openFile;
	State &st = S();
	LoadSettings();
	RescanInstalls();
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const ImVec2 sz(std::min(1300.f, vp->WorkSize.x - 40.f), std::min(980.f, vp->WorkSize.y - 12.f));
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (vp->WorkSize.x - sz.x) * 0.5f, vp->WorkPos.y + (vp->WorkSize.y - sz.y) * 0.5f));
	ImGui::SetNextWindowSize(sz);
	ImGui::SetNextWindowBgAlpha(0.97f);
	if (!ImGui::Begin("##welcome", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return {}; }
	ImGui::SetWindowFontScale(1.5f); ImGui::TextUnformatted("Hantei-chan"); ImGui::SetWindowFontScale(1.f);
	ImGui::TextDisabled("%s", T("Every French-Bread game this editor reads. Open one: its archives are mounted, characters are listed with thumbnails, a double click opens a character.",
	                            "対応しているフレンチ・ブレッド作品の一覧です。開くとアーカイブをマウントしてキャラクターをサムネイル付きで表示し、ダブルクリックで編集できます。"));
	ImGui::Separator();
	const bool scanned = ScanDone();
	const float footer = 5 * ImGui::GetFrameHeightWithSpacing() + 6;
	ImGui::BeginChild("##games", ImVec2(0, -footer), false);
	static const Family order[] = { Family::Hantei6Modern, Family::Hantei6, Family::Hantei4, Family::Han2, Family::Gof1Pb, Family::QoH };
	int rowId = 0;
	for (Family fam : order) {
		bool any = false;
		for (const GameDef &d : Games()) if (d.family == fam) any = true;
		if (!any) continue;
		ImGui::SeparatorText(FamilyName(fam, false));
		if (ImGui::BeginTable((std::string("##t") + std::to_string((int)fam)).c_str(), 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings)) {
			ImGui::TableSetupColumn("a", ImGuiTableColumnFlags_WidthFixed, 170); ImGui::TableSetupColumn("b", ImGuiTableColumnFlags_WidthFixed, 400); ImGui::TableSetupColumn("c", ImGuiTableColumnFlags_WidthStretch);
			for (const GameDef &d : Games()) {
				if (d.family != fam) continue;
				ImGui::TableNextRow();
				ImGui::PushID(rowId++);
				ImGui::TableSetColumnIndex(0);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 3));
				if (ImGui::Button((std::string(T("Open ", "開く ")) + d.shortName).c_str(), ImVec2(160, 0))) OpenGameRow(d);
				ImGui::PopStyleVar();
				ImGui::TableSetColumnIndex(1);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(T(d.name, d.nameJa));
				if (d.support == Support::FilesOnly) { ImGui::SameLine(); ImGui::TextColored(ImVec4(.75f, .45f, 0, 1), "%s", T("[files only]", "[ファイル閲覧のみ]")); if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", d.note); }
				else if (d.support == Support::CharactersLoose) { ImGui::SameLine(); ImGui::TextDisabled("%s", T("[loose files]", "[ルーズファイル]")); }
				ImGui::TableSetColumnIndex(2);
				ImGui::AlignTextToFramePadding();
				const Install &in = InstallOf(d.id);
				if (!in.path.empty()) {
					ImGui::TextColored(ImVec4(.05f, .45f, .1f, 1), "%s", in.path.c_str());
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %s", T("found via", "検出方法"), in.how.c_str());
					ImGui::SameLine();
					if (ImGui::SmallButton(T("change...", "変更..."))) FolderDialog(d.id);
				} else if (!scanned) ImGui::TextDisabled("%s", T("looking for the install...", "インストール先を検索中..."));
				else {
					ImGui::TextColored(ImVec4(.8f, .35f, 0, 1), "%s", T("not found", "見つかりません"));
					ImGui::SameLine();
					if (ImGui::SmallButton(T("Locate...", "場所を指定..."))) FolderDialog(d.id);
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();
	ImGui::Separator();
	if (ImGui::Button(T("Open any game folder (auto-detect)...", "ゲームフォルダを開く (自動判別)..."))) FolderDialog(Game::None);
	ImGui::SameLine();
	if (ImGui::Button(T("Open archive (.PAC .p .dat)...", "アーカイブを開く..."))) OpenArchiveDialog();
	ImGui::SameLine();
	if (ImGui::Button(T("Open a character file...", "キャラクターファイルを開く..."))) { std::string p = FileDialog(fileType::OPENANY, false); if (!p.empty()) openFile = p; }
	ImGui::TextDisabled("%s", T("Recent game folders and archives", "最近のフォルダ/アーカイブ"));
	if (st.recent.empty()) ImGui::TextDisabled("  -");
	for (size_t i = 0; i < st.recent.size() && i < 2; i++) if (ImGui::Selectable((st.recent[i] + "##rb" + std::to_string(i)).c_str())) { std::string e = OpenPath(st.recent[i]); if (!e.empty()) st.status = e; }
	ImGui::TextDisabled("%s", T("Recent files", "最近のファイル"));
	if (recentFiles.empty()) ImGui::TextDisabled("  -");
	for (size_t i = 0; i < recentFiles.size() && i < 1; i++) if (ImGui::Selectable((recentFiles[i] + "##rf" + std::to_string(i)).c_str())) openFile = recentFiles[i];
	if (!st.status.empty()) ImGui::TextColored(ImVec4(1, .7f, .3f, 1), "%s", st.status.c_str());
	ImGui::End();
	return openFile;
}

} // namespace abrowser
