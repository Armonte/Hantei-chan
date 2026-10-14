#include "cgm_window.h"
#include "../cg.h"
#include "../character_instance.h"
#include "../i18n.h"
#include "../filedialog.h"
#include "../png_writer.h"
#include "cgm_ops.h"
#include "cgm_export.h"
#include "cgm_struct.h"
#include "cgm_io.h"
#include "cgm_folder.h"
#include "cgm_ccpal.h"
#include "cgm_project.h"
#include "../character_instance.h"
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TXT(s) ::i18n::Tr(s)
#define LBL(s) ::i18n::Label(s)

// --ui-script driving (startup_args.cpp "cgm ..." lines, used by tools/ui shots): press a button by its English label, select a tab,
// and answer the next file / folder dialogs from a queue. Nothing reads the real mouse or keyboard.
namespace {
std::string g_scriptPress, g_scriptTab;
std::vector<std::string> g_scriptPaths;
bool LabelIs(const char *label, const std::string &want) {
	if (want == label) return true;
	const char *id = strstr(label, "###");
	return id && want == id + 3;
}
bool CgmButton(const char *label) {
	bool r = ImGui::Button(label);
	if (!g_scriptPress.empty() && LabelIs(label, g_scriptPress)) { g_scriptPress.clear(); r = true; }
	return r;
}
ImGuiTabItemFlags CgmTab(const char *name) {
	if (!g_scriptTab.empty() && g_scriptTab == name) { g_scriptTab.clear(); return ImGuiTabItemFlags_SetSelected; }
	return 0;
}
std::string ScriptPath() { std::string p = g_scriptPaths.front(); g_scriptPaths.erase(g_scriptPaths.begin()); return p; }
}
#define FileDialog(...) (g_scriptPaths.empty() ? FileDialog(__VA_ARGS__) : ScriptPath())
#define BrowseForFolderUtf8(...) (g_scriptPaths.empty() ? BrowseForFolderUtf8(__VA_ARGS__) : ScriptPath())

namespace cgm {

static std::string Fmt(const char *fmt, ...) {
	char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); return buf;
}

static const char *TypeName(int t) {
	switch (t) {
	case 0: return TXT("0: 8-bit, bank palette");
	case 1: return TXT("1: 32-bit BGRA");
	case 2: return TXT("2: own palette, binary alpha");
	case 3: return TXT("3: one colour + alpha plane");
	case 4: return TXT("4: own palette + alpha plane");
	case 5: return TXT("5: bank palette + alpha plane");
	case -1: return TXT("-1: no pixels (not drawn)");
	}
	return TXT("unknown");
}

Window::~Window() { clearThumbs(); if (preview.tex) glDeleteTextures(1, &preview.tex); }

void Window::clearThumbs() {
	for (auto &t : thumbs) if (t.second.tex) glDeleteTextures(1, &t.second.tex);
	thumbs.clear();
}

void Window::rebuild(CharacterInstance &ch) {
	owner = &ch; cgGen = ch.cg.generation();
	clearThumbs();
	bank.reset(); atlas.clear();
	if (!ch.cg.foreign() && ch.cg.bank_data()) {
		auto b = std::make_unique<Bank>(); std::string err;
		if (Bank::parse((const uint8_t *)ch.cg.bank_data(), ch.cg.bank_size(), *b, &err)) { bank = std::move(b); atlas = bank->buildAtlas(); }
	}
	filterKey = ~0ull;
	diskUids = bank ? ImageUids(*bank) : std::vector<uint32_t>();
	diskUsageValid = false;
	scanProjects(ch);
}

// Projects on disk that draw this bank, and which stack files of the loaded character are rewritten with it.
void Window::scanProjects(CharacterInstance &ch) {
	const std::string cg = ch.getCGPath().empty() ? std::string() : AnsiToUtf8(ch.getCGPath());
	skipMark.clear();
	if (cg.empty() || !bank) { users = BankUsers(); usersFor.clear(); return; }
	if (usersFor != cg) { users = FindBankUsers(cg); usersFor = cg; }
	ProjectTxt t;
	if (!ch.getTxtPath().empty() && ParseProjectTxt(AnsiToUtf8(ch.getTxtPath()), t)) {
		skipMark.assign(t.dataPaths.size(), 0);
		for (size_t i = 0; i < t.dataPaths.size(); i++) {
			const int fi = users.fileIndex(t.dataPaths[i]);
			skipMark[i] = fi >= 0 && !users.files[fi].shared && (int)i != ch.frameData.ownFile();
		}
	}
}

void Window::countProjectUsage() {
	if (!bank) return;
	diskUsage = CountProjectUsage(users, (int)diskUids.size()); diskUsageValid = true; diskUsageMap.clear();
}
int Window::projectCount(int id) {
	if (!diskUsageValid || !bank) return -1;
	if (diskUsageMap.size() != bank->images.size()) diskUsageMap = InvertRemap(RemapSince(diskUids, *bank), bank->images.size());
	const int d = id >= 0 && id < (int)diskUsageMap.size() ? diskUsageMap[id] : -1;
	return d >= 0 ? diskUsage.count(d) : 0;
}

bool Window::pendingIdChanges(const CharacterInstance *ch) const {
	if (!ch || ch != owner || !bank || diskUids.empty()) return false;
	return bank->images.size() != diskUids.size() || !IsIdentityRemap(RemapSince(diskUids, *bank));
}

bool Window::saveBankFiles(CharacterInstance &ch, std::string &report) {
	if (!bank || ch.getCGPath().empty() || &ch != owner) { report = TXT("No bank to save."); return false; }
	const std::string cg = AnsiToUtf8(ch.getCGPath());
	const std::filesystem::path target(Utf8ToWide(cg));
	std::error_code ec; std::vector<uint8_t> bytes; bank->serialize(bytes);
	std::filesystem::path bak = target; bak += L".bak";
	if (!std::filesystem::exists(bak, ec)) std::filesystem::copy_file(target, bak, ec);   // the first save keeps the original beside it
	std::filesystem::path tmp = target; tmp += L".tmp";
	{ std::ofstream f(tmp, std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); }
	std::filesystem::rename(tmp, target, ec);
	if (ec) { report = TXT("Could not write the file."); return false; }
	report = Fmt(TXT("Saved %s (the original is kept as .bak)"), ch.getCGPath().c_str());
	const std::vector<int> remap = RemapSince(diskUids, *bank);
	bool ok = true;
	if (!IsIdentityRemap(remap) && !users.files.empty()) {
		std::vector<std::string> skip; if (!ch.getTopHA6Path().empty()) skip.push_back(AnsiToUtf8(ch.getTopHA6Path()));   // written by the character's own save
		DiskRemapResult r; ok = RemapUserFiles(users, remap, true, r, skip);
		report += "\n" + Fmt(TXT("References: %d rewritten (%d cleared) in %d other project file(s); %d file(s) already matched."), r.refsChanged, r.refsCleared, (int)r.written.size(), (int)r.unchanged.size());
		for (auto &w : r.written) report += "\n  " + w;
		if (!r.sharedSkipped.empty()) report += "\n" + Fmt(TXT("%d shared file(s) (BaseData) are not rewritten: their patterns are saved into this character's own file."), (int)r.sharedSkipped.size());
		for (auto &e : r.errors) report += "\n" + std::string(TXT("Error: ")) + e;
	}
	diskUids = ImageUids(*bank); diskUsageValid = false; diskUsageMap.clear();
	return ok;
}

bool Window::applyModel(CharacterInstance &ch, const WindowHost &host) {
	if (!bank) return false;
	std::vector<uint8_t> bytes; bank->serialize(bytes);
	bool ok;
	if (bytes.size() == ch.cg.bank_size()) ok = ch.cg.restore_bank((const char *)bytes.data(), (unsigned)bytes.size());
	else ok = ch.cg.replaceBank(bytes.data(), (unsigned)bytes.size());
	if (!ok) { status = TXT("The edited bank could not be loaded."); return false; }
	cgGen = ch.cg.generation();   // our own change: keep the model, drop only the pictures
	clearThumbs(); previewId = -1; filterKey = ~0ull;
	atlas = bank->buildAtlas();
	if (host.markEdited) host.markEdited(&ch);
	return true;
}

bool Window::commit(CharacterInstance &ch, const std::string &label, Bank before, const WindowHost &host) {
	HistoryEntry e; e.label = label; e.hasBank = true; e.after = *bank;
	if (!applyModel(ch, host)) { *bank = std::move(before); return false; }
	e.before = std::move(before);
	hist.push(std::move(e));
	return true;
}

bool Window::commitStruct(CharacterInstance &ch, const std::string &label, Bank before, const std::vector<int> &remap, const WindowHost &host) {
	HistoryEntry e; e.label = label; e.hasBank = true; e.after = *bank; e.remap = remap;
	if (!remap.empty()) RemapSprites(ch.frameData, remap, &e.cleared, &skipMark);
	if (!applyModel(ch, host)) { *bank = std::move(before); if (!remap.empty()) { RemapSprites(ch.frameData, InvertRemap(remap, e.after.images.size()), nullptr, &skipMark); RestoreCleared(ch.frameData, e.cleared, &skipMark); } return false; }
	diskUsageMap.clear();
	usage.version = ~0ull;   // references changed
	e.before = std::move(before);
	hist.push(std::move(e));
	return true;
}

void Window::undoRedo(CharacterInstance &ch, bool redo, const WindowHost &host) {
	if (!bank || (redo ? !hist.canRedo() : !hist.canUndo())) return;
	HistoryEntry e = redo ? hist.redo() : hist.undo();
	if (e.hasBank) {
		*bank = redo ? e.after : e.before;
		if (!e.remap.empty()) {   // put the frame references back the way they were
			std::vector<ClearedRef> again;
			if (redo) RemapSprites(ch.frameData, e.remap, &again, &skipMark);
			else { RemapSprites(ch.frameData, InvertRemap(e.remap, e.after.images.size()), nullptr, &skipMark); RestoreCleared(ch.frameData, e.cleared, &skipMark); }
			diskUsageMap.clear();
		}
		applyModel(ch, host);
	}
	if (e.palBank >= 0) { pal.set[e.palBank] = redo ? e.palAfter : e.palBefore; pal.dirty[e.palBank] = true; pushPalette(ch, e.palBank); if (host.markEdited) host.markEdited(&ch); }
	status = (redo ? std::string(TXT("Redone: ")) : std::string(TXT("Undone: "))) + e.label;
}

// RGBA of image id with palette `pal` / PUPS bank `pups` (type 0 images follow the palette, the others carry their own colours).
bool Window::fetch(CharacterInstance &ch, int id, int pal, int pups, std::vector<uint8_t> &px, int &w, int &h) {
	if (bank) {
		Rgba r; const unsigned *p = ch.cg.paletteAt(pal, pups);
		if (!bank->decode(id, r, p)) return false;
		px = std::move(r.px); w = r.w; h = r.h; return true;
	}
	ImageData *im = ch.cg.draw_texture((unsigned)id, false, false);
	if (!im) return false;
	w = im->width; h = im->height; px.assign(im->pixels, im->pixels + (size_t)w * h * 4);
	delete im; return true;
}

void Window::upload(Thumb &t, const std::vector<uint8_t> &src, int w, int h, int maxSide, bool linear) {
	std::vector<uint8_t> tmp; const uint8_t *px = src.data(); int tw = w, th = h;
	if (maxSide > 0 && std::max(w, h) > maxSide) {   // box filter down to the thumbnail size
		const float s = (float)maxSide / (float)std::max(w, h);
		tw = std::max(1, (int)(w * s)); th = std::max(1, (int)(h * s));
		tmp.assign((size_t)tw * th * 4, 0);
		for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
			const int x0 = x * w / tw, x1 = std::max(x0 + 1, (x + 1) * w / tw), y0 = y * h / th, y1 = std::max(y0 + 1, (y + 1) * h / th);
			unsigned r = 0, g = 0, b = 0, a = 0, n = 0;
			for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { const uint8_t *p = &src[((size_t)yy * w + xx) * 4]; r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3]; n++; }
			uint8_t *o = &tmp[((size_t)y * tw + x) * 4];
			if (a) { o[0] = (uint8_t)(r / a); o[1] = (uint8_t)(g / a); o[2] = (uint8_t)(b / a); }
			o[3] = (uint8_t)(a / std::max(1u, n));
		}
		px = tmp.data();
	}
	if (!t.tex) glGenTextures(1, &t.tex);
	glBindTexture(GL_TEXTURE_2D, t.tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	t.w = tw; t.h = th;
}

static void Checker(ImDrawList *dl, ImVec2 a, ImVec2 b) {
	dl->AddRectFilled(a, b, IM_COL32(58, 58, 62, 255));
	const float s = 8.f;
	for (float y = a.y, r = 0; y < b.y; y += s, r++) for (float x = a.x + (((int)r & 1) ? s : 0); x < b.x; x += 2 * s)
		dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + s, b.x), std::min(y + s, b.y)), IM_COL32(78, 78, 84, 255));
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Palettes tab
// ---------------------------------------------------------------------------------------------------------------------------------------
void Window::loadPalettes(CharacterInstance &ch) {
	palOwner = &ch; pal = PalState();
	for (int b = 0; b < 8; b++) {
		const std::string &path = ch.cg.palettePath(b);
		if (path.empty()) continue;
		std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
		if (!f) f.open(std::filesystem::path(Utf8ToWide(path)), std::ios::binary);
		if (!f) continue;
		std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		pal.loaded[b] = PalSet::parse(raw.data(), raw.size(), pal.set[b], nullptr);
	}
	pal.bank = pal.loaded[0] ? 0 : -1;   // -1: the bank's own slots
}

void Window::pushPalette(CharacterInstance &ch, int b) {
	if (b < 0 || b >= 8 || !pal.loaded[b]) return;
	ch.cg.setPaletteBytes(b, pal.set[b].raw.data(), (unsigned)pal.set[b].raw.size());
	cgGen = ch.cg.generation(); clearThumbs(); previewId = -1; filterKey = ~0ull;
}

void Window::commitPalette(CharacterInstance &ch, const std::string &label, int b, PalSet before, const WindowHost &) {
	HistoryEntry e; e.label = label; e.palBank = b; e.palBefore = std::move(before); e.palAfter = pal.set[b];
	hist.push(std::move(e)); pal.dirty[b] = true; pushPalette(ch, b);
}

static ImVec4 Col4(uint32_t c) { return ImVec4(cgm::R(c) / 255.f, cgm::G(c) / 255.f, cgm::B(c) / 255.f, 1.f); }
static uint32_t FromCol(const float c[3]) { return cgm::Rgb((uint8_t)std::lround(std::clamp(c[0], 0.f, 1.f) * 255), (uint8_t)std::lround(std::clamp(c[1], 0.f, 1.f) * 255), (uint8_t)std::lround(std::clamp(c[2], 0.f, 1.f) * 255)); }

void Window::drawPalettes(CharacterInstance &ch, const WindowHost &host) {
	if (palOwner != &ch) loadPalettes(ch);
	auto &P = pal;
	// ---- source ----
	std::vector<std::string> names; std::vector<int> ids;   // id -1 = bank slots, 0..7 = palette file bank
	if (bank) { names.push_back(TXT("Bank slots (inside the .cg)")); ids.push_back(-1); }
	for (int b = 0; b < 8; b++) if (P.loaded[b]) { names.push_back(std::filesystem::path(ch.cg.palettePath(b)).filename().string() + (P.dirty[b] ? " *" : "")); ids.push_back(b); }
	if (names.empty()) { ImGui::TextDisabled("%s", TXT("This character has no palette file and its bank is not editable.")); return; }
	int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == P.bank) cur = (int)i;
	ImGui::SetNextItemWidth(260);
	if (ImGui::BeginCombo(LBL("Palette source"), names[cur].c_str())) { for (size_t i = 0; i < ids.size(); i++) if (ImGui::Selectable(names[i].c_str(), (int)i == cur)) { P.bank = ids[i]; P.number = 0; } ImGui::EndCombo(); }
	const bool isFile = P.bank >= 0;
	const int total = isFile ? P.set[P.bank].count : 8;
	P.number = std::clamp(P.number, 0, std::max(0, total - 1));
	ImGui::SameLine(); ImGui::SetNextItemWidth(110);
	if (ImGui::InputInt(LBL("Palette number"), &P.number, 1, 8)) { P.number = std::clamp(P.number, 0, total - 1); if (isFile) { previewPal = P.number; previewPups = P.bank; } clearThumbs(); previewId = -1; }
	ImGui::SameLine(); ImGui::TextDisabled(TXT("of %d"), total);
	if (isFile) { ImGui::SameLine(); if (CgmButton(LBL("Show in main view"))) { ch.palette = P.number; ch.cg.changePaletteNumber(P.number); } }

	// Run an edit on the current palette: `fn` changes the 256 colours; the step is recorded for undo and shown everywhere at once.
	auto edit = [&](const std::string &label, auto &&fn, bool record = true) -> bool {
		if (isFile) {
			PalSet before = P.set[P.bank]; fn(P.set[P.bank].pal(P.number));
			if (P.set[P.bank].raw == before.raw) return false;
			if (record) commitPalette(ch, label, P.bank, std::move(before), host); else { P.dirty[P.bank] = true; pushPalette(ch, P.bank); }
			return true;
		}
		Bank before = *bank; uint32_t a[256]; memcpy(a, bank->palettes.data() + 0x400 * P.number, 1024); fn(a);
		if (!memcmp(a, bank->palettes.data() + 0x400 * P.number, 1024)) return false;
		bank->setPaletteSlot(P.number, a); commit(ch, label, std::move(before), host); return true;
	};
	uint32_t *colors = isFile ? P.set[P.bank].pal(P.number) : (uint32_t *)(bank->palettes.data() + 0x400 * P.number);

	// ---- swatches ----
	const float cell = 20.f; ImVec2 g0 = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##swatches", ImVec2(cell * 16, cell * 16));
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();
		const int lo = std::min(P.selA, P.selB), hi = std::max(P.selA, P.selB);
		const bool preview = !P.adj.neutral();
		for (int i = 0; i < 256; i++) {
			const ImVec2 a(g0.x + (i % 16) * cell, g0.y + (i / 16) * cell), b(a.x + cell - 1, a.y + cell - 1);
			uint32_t c = colors[i]; if (preview && i >= lo && i <= hi) c = cgm::Adjust(c, P.adj);
			dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(Col4(c)));
			if (i >= lo && i <= hi) dl->AddRect(a, b, IM_COL32(255, 200, 60, 255), 0, 0, 2.f);
		}
		if (ImGui::IsItemHovered()) {
			const ImVec2 m = ImGui::GetMousePos(); const int cx = (int)((m.x - g0.x) / cell), cy = (int)((m.y - g0.y) / cell), i = std::clamp(cy, 0, 15) * 16 + std::clamp(cx, 0, 15);
			if (ImGui::IsMouseClicked(0)) { if (ImGui::GetIO().KeyShift) P.selB = i; else P.selA = P.selB = i; }
			else if (ImGui::IsMouseDragging(0)) P.selB = i;
			ImGui::SetTooltip("%d  #%02X%02X%02X", i, cgm::R(colors[i]), cgm::G(colors[i]), cgm::B(colors[i]));
		}
	}
	ImGui::SameLine(0, 16);
	ImGui::BeginGroup();
	const int lo = std::min(P.selA, P.selB), hi = std::max(P.selA, P.selB);
	ImGui::Text(TXT("Selected: %d - %d"), lo, hi);
	{   // single colour
		float c[3] = {cgm::R(colors[lo]) / 255.f, cgm::G(colors[lo]) / 255.f, cgm::B(colors[lo]) / 255.f};
		static PalSet liveBefore; static Bank liveBankBefore; static bool live = false;
		if (ImGui::ColorEdit3(LBL("Colour"), c, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8)) {
			if (!live) { live = true; if (isFile) liveBefore = P.set[P.bank]; else liveBankBefore = *bank; }
			const uint32_t n = FromCol(c);
			if (isFile) { colors[lo] = (colors[lo] & 0xFF000000u) | (n & 0xFFFFFFu); P.dirty[P.bank] = true; pushPalette(ch, P.bank); }
			else colors[lo] = (colors[lo] & 0xFF000000u) | (n & 0xFFFFFFu);
		}
		if (live && ImGui::IsItemDeactivated()) {
			live = false;
			if (isFile) { if (P.set[P.bank].raw != liveBefore.raw) commitPalette(ch, Fmt(TXT("Edit colour %d"), lo), P.bank, std::move(liveBefore), host); }
			else if (memcmp(bank->palettes.data(), liveBankBefore.palettes.data(), 0x2000)) commit(ch, Fmt(TXT("Edit colour %d"), lo), std::move(liveBankBefore), host);
		}
	}
	ImGui::SeparatorText(TXT("Selection"));
	{   // fill / gradient
		float f[3] = {cgm::R(P.fill) / 255.f, cgm::G(P.fill) / 255.f, cgm::B(P.fill) / 255.f};
		ImGui::SetNextItemWidth(150); if (ImGui::ColorEdit3("##fill", f, ImGuiColorEditFlags_NoInputs)) P.fill = FromCol(f);
		ImGui::SameLine(); if (CgmButton(LBL("Fill selection"))) edit(Fmt(TXT("Fill %d-%d"), lo, hi), [&](uint32_t *p) { FillRange(p, lo, hi, P.fill); });
		ImGui::SameLine(); if (CgmButton(LBL("Gradient first -> last"))) edit(Fmt(TXT("Gradient %d-%d"), lo, hi), [&](uint32_t *p) { Gradient(p, lo, hi, p[lo], p[hi], P.hsvGradient); });
		ImGui::SameLine(); ImGui::Checkbox(LBL("via HSV"), &P.hsvGradient);
		if (CgmButton(LBL("Reverse"))) edit(Fmt(TXT("Reverse %d-%d"), lo, hi), [&](uint32_t *p) { ReverseRange(p, lo, hi); });
		ImGui::SameLine(); if (CgmButton(LBL("Copy range"))) { P.clipLen = hi - lo + 1; memcpy(P.clip, colors + lo, sizeof(uint32_t) * P.clipLen); }
		ImGui::SameLine(); ImGui::BeginDisabled(P.clipLen == 0);
		if (CgmButton(LBL("Paste at selection"))) edit(Fmt(TXT("Paste at %d"), lo), [&](uint32_t *p) { CopyRange(P.clip, 0, P.clipLen - 1, p, lo); });
		ImGui::EndDisabled();
	}
	{   // hue / saturation / value of the selection, live
		static PalSet baseSet; static Bank baseBank; static bool active = false; static uint32_t baseColors[256];
		bool changed = false, started = false, finished = false;
		auto sl = [&](const char *l, float *v, float mn, float mx, const char *fmt) { ImGui::SetNextItemWidth(220); changed |= ImGui::SliderFloat(l, v, mn, mx, fmt); started |= ImGui::IsItemActivated(); finished |= ImGui::IsItemDeactivated(); };
		sl(LBL("Hue shift"), &P.adj.hueDeg, -180.f, 180.f, "%.0f deg"); sl(LBL("Saturation x"), &P.adj.satMul, 0.f, 2.f, "%.2f"); sl(LBL("Brightness x"), &P.adj.valMul, 0.f, 2.f, "%.2f");
		if (started && !active) { active = true; memcpy(baseColors, colors, 1024); if (isFile) baseSet = P.set[P.bank]; else baseBank = *bank; }
		if (changed && active && isFile) { memcpy(colors, baseColors, 1024); AdjustRange(colors, lo, hi, P.adj); P.dirty[P.bank] = true; pushPalette(ch, P.bank); }
		if (finished && active) {
			active = false;
			if (isFile) { PalSet after = P.set[P.bank]; if (after.raw != baseSet.raw) commitPalette(ch, TXT("Adjust colours"), P.bank, baseSet, host); }
			else { uint32_t a[256]; memcpy(a, baseColors, 1024); AdjustRange(a, lo, hi, P.adj); *bank = baseBank; bank->setPaletteSlot(P.number, a); commit(ch, TXT("Adjust colours"), baseBank, host); }
			P.adj = ColorAdjust();
		}
		if (ImGui::SmallButton(LBL("Reset sliders"))) P.adj = ColorAdjust();
	}
	ImGui::SeparatorText(TXT("Find and replace colour"));
	{
		float f[3] = {cgm::R(P.findC) / 255.f, cgm::G(P.findC) / 255.f, cgm::B(P.findC) / 255.f}, r[3] = {cgm::R(P.replaceC) / 255.f, cgm::G(P.replaceC) / 255.f, cgm::B(P.replaceC) / 255.f};
		if (ImGui::ColorEdit3("##find", f, ImGuiColorEditFlags_NoInputs)) P.findC = FromCol(f);
		ImGui::SameLine(); ImGui::TextUnformatted("->"); ImGui::SameLine();
		if (ImGui::ColorEdit3("##repl", r, ImGuiColorEditFlags_NoInputs)) P.replaceC = FromCol(r);
		ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Tolerance"), &P.tolerance, 0, 0); P.tolerance = std::clamp(P.tolerance, 0, 255);
		ImGui::SameLine(); if (CgmButton(LBL("Replace in this palette"))) edit(TXT("Replace colour"), [&](uint32_t *p) { ReplaceColor(p, P.findC, P.replaceC, P.tolerance); });
	}
	ImGui::EndGroup();

	// ---- palette files ----
	ImGui::SeparatorText(TXT("Palettes of this file"));
	if (isFile) {
		PalSet &ps = P.set[P.bank];
		auto fileEdit = [&](const std::string &label, auto &&fn) { PalSet before = ps; std::string e; if (fn(e) && ps.raw != before.raw) { commitPalette(ch, label, P.bank, std::move(before), host); status.clear(); } else if (!e.empty()) status = e; };
		if (ps.paired()) {
			const int c = ps.colourOf(P.number);
			ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), TXT("UNI2 / MBTL layout: %d colours x 2 sets. Palette %d is colour %d's %s set; palette %d is its %s set. Moves keep the two together."),
				ps.half(), P.number, c, P.number < ps.half() ? TXT("main") : TXT("alternate"), ps.alternateOf(P.number), P.number < ps.half() ? TXT("alternate") : TXT("main"));
			ImGui::SameLine(); if (ImGui::SmallButton(LBL("Go to its pair"))) { P.number = ps.alternateOf(P.number); clearThumbs(); previewId = -1; }
		}
		if (CgmButton(LBL("Duplicate"))) fileEdit(TXT("Duplicate palette"), [&](std::string &e) { if (!ps.addCopy(P.number, &e)) return false; P.number = ps.count - 1; return true; });
		ImGui::SameLine(); if (CgmButton(LBL("Delete"))) fileEdit(TXT("Delete palette"), [&](std::string &e) { if (!ps.remove(P.number, &e)) return false; P.number = std::min(P.number, ps.count - 1); return true; });
		ImGui::SameLine(); if (CgmButton(LBL("Move up")) && ps.colourOf(P.number) > 0) fileEdit(TXT("Move palette"), [&](std::string &) { ps.movePair(P.number, ps.colourOf(P.number) - 1); P.number--; return true; });
		ImGui::SameLine(); if (CgmButton(LBL("Move down")) && ps.colourOf(P.number) + 1 < ps.half()) fileEdit(TXT("Move palette"), [&](std::string &) { ps.movePair(P.number, ps.colourOf(P.number) + 1); P.number++; return true; });
		ImGui::SameLine(); if (CgmButton(LBL("Save .pal"))) {
			std::string e; const std::string path = AnsiToUtf8(ch.cg.palettePath(P.bank));
			if (WritePalSet(path, ps, true, &e)) { P.dirty[P.bank] = false; status = Fmt(TXT("Saved %s (the original is kept as .bak)"), path.c_str()); } else status = e;
		}
		ImGui::SameLine(); if (CgmButton(LBL("Save .pal as..."))) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", std::filesystem::path(ch.cg.palettePath(P.bank)).filename().string().c_str());
			const std::string path = FileDialog(-1, true, nm);
			if (!path.empty()) { std::ofstream f(std::filesystem::u8path(path), std::ios::binary); f.write((const char *)ps.raw.data(), (std::streamsize)ps.raw.size()); status = f ? Fmt(TXT("Saved %s"), path.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		ImGui::SameLine(); if (CgmButton(ps.offsetDw == 4 ? LBL("Save as MBAACC layout...") : LBL("Save as UNI2/MBTL layout..."))) {
			const PalSet conv = ps.converted(ps.offsetDw != 4);
			char nm[128]; snprintf(nm, sizeof(nm), "%s", std::filesystem::path(ch.cg.palettePath(P.bank)).filename().string().c_str());
			const std::string path = FileDialog(-1, true, nm); std::string e;
			if (!path.empty()) status = WritePalSet(path, conv, false, &e) ? Fmt(TXT("Saved %s: %d palettes"), path.c_str(), conv.count) : e;
		}
		ImGui::SetItemTooltip("%s", TXT("MBAACC: count + palettes. UNI2 / MBTL: header FFFF,1,0,130 = 65 colours, palette c + 65 being colour c's alternate set (an MBAACC file gives each colour itself as its alternate)."));
		// a new PUPS bank (<cg>_pN.pal): what a pattern's PUPS value selects in UNI2 / MBTL
		if (!ch.getCGPath().empty() && P.loaded[0]) {
			ImGui::SetNextItemWidth(70); ImGui::InputInt("##newpups", &newPups, 1, 1); newPups = std::clamp(newPups, 1, 7);
			ImGui::SameLine(); ImGui::BeginDisabled(P.loaded[newPups]);
			if (CgmButton(LBL("New PUPS bank _pN.pal"))) {
				std::string e, outPath; const std::string cg = AnsiToUtf8(ch.getCGPath());
				if (!CreatePupsBank(cg, newPups, P.set[0], &outPath, &e)) status = e;
				else {
					// only the new bank is added: unsaved edits of the other banks stay
					const int nb = newPups; const PalSet &base = P.set[0];
					ch.cg.setPaletteBytes(nb, base.raw.data(), (unsigned)base.raw.size());
					ch.cg.setPalettePath(nb, ch.getCGPath().substr(0, ch.getCGPath().find_last_of('.')) + "_p" + std::to_string(nb) + ".pal");
					P.set[nb] = base; P.loaded[nb] = true; P.dirty[nb] = false; P.bank = nb; P.number = 0;
					cgGen = ch.cg.generation(); clearThumbs(); previewId = -1;
					status = Fmt(TXT("Created %s as a copy of the base palettes. Set a pattern's PUPS to %d to draw with it."), outPath.c_str(), nb);
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("(a copy of the base .pal; MBAACC ignores PUPS)"));
		}
	}
	ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("from palette"), &importFrom, 1, 10); importFrom = std::max(0, importFrom);
	ImGui::SetItemTooltip("%s", TXT("Which palette of a multi-palette .pal to import (e.g. 37 of chr010.pal). Other formats hold one palette."));
	ImGui::SameLine();
	if (CgmButton(LBL("Import colours..."))) {
		const std::string path = FileDialog(-1, false);
		if (!path.empty()) { uint32_t in[256]; std::string e; if (!ReadPalFileColors(path, in, &e, importFrom)) status = e; else edit(Fmt(TXT("Import palette %d"), importFrom), [&](uint32_t *p) { for (int i = 0; i < 256; i++) p[i] = (p[i] & 0xFF000000u) | (in[i] & 0xFFFFFFu); }); }
	}
	ImGui::SameLine(); if (CgmButton(LBL("Export colours..."))) {
		char nm[128]; snprintf(nm, sizeof(nm), "palette_%d.act", P.number);
		const std::string path = FileDialog(-1, true, nm);
		if (!path.empty()) { std::string e; status = WritePalFileColors(path, colors, &e, palLayout ? PalLayout::Uni : PalLayout::Mbaacc) ? Fmt(TXT("Saved %s"), path.c_str()) : e; }
	}
	ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("The file extension picks the format: .act, .gpl (GIMP), .png (16x16 strip) or .pal."));
	ImGui::SameLine(); ImGui::SetNextItemWidth(150); { const char *lay[] = {".pal: MBAACC layout", ".pal: UNI2/MBTL layout"}; i18n::Combo("##pallayout", &palLayout, lay, 2); }

	// ---- MBAA:CC custom palettes for PovertyCaster / CCCaster ----
	if (!g_scriptPress.empty() && isFile && P.set[P.bank].count > 0) ImGui::SetNextItemOpen(true);   // a scripted press opens the section
	if (isFile && P.set[P.bank].count > 0 && ImGui::TreeNode(LBL("Export for PovertyCaster (MBAA:CC custom palettes)"))) {
		const PalSet &ps = P.set[P.bank];
		const std::string stem = std::filesystem::path(ch.cg.palettePath(P.bank)).stem().string();
		if (ccChara < 0) ccChara = CcCharaFromStem(stem);
		ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Chara id"), &ccChara, 0, 0);
		ImGui::SameLine(); ImGui::TextDisabled(TXT("(%s: %d in PovertyCaster's table)"), stem.c_str(), CcCharaFromStem(stem));
		ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Palettes from"), &ccFrom, 0, 0); ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("to##cc"), &ccTo, 0, 0);
		ccFrom = std::clamp(ccFrom, 0, ps.count - 1); ccTo = std::clamp(ccTo, ccFrom, ps.count - 1);
		ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("as slots from"), &ccFirst, 0, 0); ccFirst = std::clamp(ccFirst, 1, kCcMaxPalettes);
		const int last = std::min(kCcMaxPalettes, ccFirst + ccTo - ccFrom);
		const CcSlotPlace a = CcPlaceOf(ccFirst), b = CcPlaceOf(last);
		ImGui::TextDisabled(TXT("Slots %d..%d: selector tab %d entry %d to tab %d entry %d (36 per tab; tab 1 is the stock list)."), ccFirst, last, a.bank + 1, a.local + 1, b.bank + 1, b.local + 1);
		ImGui::Checkbox(LBL("Also write the full .pal (data and data\\_csel)"), &ccFullPal);
		ImGui::SetItemTooltip("%s", TXT("For players without PovertyCaster: data\\<name>.pal and data\\_csel\\<name>.pal with the count grown to the last slot (new slots are palette 0 with entry 0 cleared). Originals are kept as .bak."));
		if (CgmButton(LBL("Export to <game>\\palettes"))) {
			std::vector<CcExportItem> items; for (int k = ccFrom; k <= ccTo && ccFirst + k - ccFrom <= kCcMaxPalettes; k++) items.push_back({k, ccFirst + k - ccFrom});
			const std::filesystem::path palPath(Utf8ToWide(AnsiToUtf8(ch.cg.palettePath(P.bank))));
			const std::filesystem::path game = palPath.parent_path().parent_path();   // <game>\data\x.pal -> <game>
			CcExportResult r; std::string e;
			if (!ExportCcPalettePngs(ps, ccChara, items, WideToUtf8((game / L"palettes").wstring()), r)) status = r.error;
			else {
				status = Fmt(TXT("Wrote %d PovertyCaster palette(s) to %s"), (int)r.written.size(), WideToUtf8((game / L"palettes").wstring()).c_str());
				if (ccFullPal) {
					PalSet grown;
					if (!BuildCcPalSet(ps, items, 0, grown, &e)) status += "\n" + e;
					else for (const std::filesystem::path &t : {palPath, palPath.parent_path() / L"_csel" / palPath.filename()}) {
						std::error_code ec; if (!std::filesystem::exists(t, ec)) continue;
						status += "\n" + (WritePalSet(WideToUtf8(t.wstring()), grown, true, &e) ? Fmt(TXT("Saved %s: %d palettes"), WideToUtf8(t.wstring()).c_str(), grown.count) : e);
					}
				}
			}
		}
		ImGui::TreePop();
	}

	// ---- whole-bank recolour ----
	ImGui::SeparatorText(TXT("Recolour the whole character"));
	ImGui::TextDisabled("%s", TXT("Uses the hue / saturation / brightness sliders above."));
	ImGui::Checkbox(LBL("Only the current palette number"), &P.recolorThisOnly); ImGui::SameLine();
	ImGui::Checkbox(LBL("Palette file(s)"), &P.recolorFiles);
	ImGui::Checkbox(LBL("Bank slots"), &P.recolorSlots); ImGui::SameLine();
	ImGui::Checkbox(LBL("Sprites' own palettes (types 2, 3, 4)"), &P.recolorImages);
	ImGui::BeginDisabled(P.adj.neutral());
	if (CgmButton(LBL("Apply recolour"))) {
		HistoryEntry e; e.label = TXT("Recolour"); const int fb = isFile ? P.bank : (P.loaded[0] ? 0 : -1); std::string msg;
		int touched = 0;
		if (P.recolorFiles && fb >= 0) { e.palBank = fb; e.palBefore = P.set[fb]; RecolorScope sc; sc.allPalettes = !P.recolorThisOnly; sc.onlyPalette = P.number; touched += RecolorSet(P.set[fb], P.adj, sc); e.palAfter = P.set[fb]; }
		if (bank && (P.recolorSlots || P.recolorImages)) { e.hasBank = true; e.before = *bank; touched += RecolorBank(*bank, P.adj, P.recolorSlots, P.recolorImages); e.after = *bank; }
		if (e.hasBank) { if (!applyModel(ch, host)) { *bank = e.before; e.hasBank = false; } }
		if (e.palBank >= 0) { P.dirty[e.palBank] = true; pushPalette(ch, e.palBank); }
		if (e.hasBank || e.palBank >= 0) { hist.push(std::move(e)); status = Fmt(TXT("Recoloured %d palette(s). Undo restores them."), touched); P.adj = ColorAdjust(); }
	}
	ImGui::EndDisabled();
	if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
}

void Window::drawStructure(CharacterInstance &ch, const WindowHost &host) {
	const int count = (int)bank->images.size();
	const bool have = selected >= 0 && selected < count;
	if (!g_scriptPress.empty()) ImGui::SetNextItemOpen(true);   // a scripted press opens the section
	if (!ImGui::TreeNode(LBL("Add / remove / reorder"))) return;
	// add
	if (CgmButton(LBL("Add image from PNG..."))) {
		const std::string p = FileDialog(-1, false);
		if (!p.empty()) {
			std::vector<uint8_t> px; int w = 0, h = 0; std::string e; Bank before = *bank; std::vector<int> remap; int id = -1; NewImageSpec sp;
			sp.name = std::filesystem::path(Utf8ToWide(p)).stem().string() + ".bmp";
			if (!ReadImageRgba(p, px, w, h, e)) status = e;
			else if (!AddImage(*bank, insertAt >= 0 ? insertAt : (have ? selected + 1 : -1), sp, px.data(), w, h, remap, &id, &e)) { *bank = std::move(before); status = e; }
			else if (commitStruct(ch, Fmt(TXT("Add image %d"), id), std::move(before), remap, host)) { selected = id; status = Fmt(TXT("Added image %d (storage type %d). Ids after it moved up by one; frame references were updated."), id, bank->images[id].type); }
		}
	}
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("at id"), &insertAt, 0, 0); ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("Id the new image takes (-1: after the selected image). Later ids move up and every frame reference follows."));
	ImGui::BeginDisabled(!have);
	if (CgmButton(LBL("Duplicate"))) {
		Bank before = *bank; std::vector<int> remap; int id = -1; std::string e;
		if (!DuplicateImage(*bank, selected, selected + 1, remap, &id, &e)) { *bank = std::move(before); status = e; }
		else if (commitStruct(ch, Fmt(TXT("Duplicate image %d"), selected), std::move(before), remap, host)) { selected = id; status = Fmt(TXT("Duplicated as image %d."), id); }
	}
	ImGui::SameLine(); if (CgmButton(LBL("Make independent"))) {
		Bank before = *bank; std::string e;
		if (!UnshareImage(*bank, selected, &e)) { *bank = std::move(before); status = e; } else commitStruct(ch, Fmt(TXT("Make image %d independent"), selected), std::move(before), {}, host);
	}
	ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("Gives the image its own copy of every cell it borrows from another image."));
	ImGui::SameLine(); if (CgmButton(LBL("Clear (keep id)"))) { pend = {2, selected, true}; }
	ImGui::SameLine(); if (CgmButton(LBL("Delete (renumber)"))) { pend = {1, selected, true}; }
	ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Move to id"), &moveTarget, 0, 0); ImGui::SameLine();
	if (CgmButton(LBL("Move"))) {
		Bank before = *bank; std::vector<int> remap; std::string e; moveTarget = std::clamp(moveTarget, 0, count - 1);
		if (!MoveImage(*bank, selected, moveTarget, remap, &e)) { *bank = std::move(before); status = e; }
		else if (commitStruct(ch, Fmt(TXT("Move image %d to %d"), selected, moveTarget), std::move(before), remap, host)) { selected = moveTarget; status = TXT("Moved. Every frame reference follows the images."); }
	}
	ImGui::EndDisabled();
	if (CgmButton(LBL("Delete all unused..."))) { pend = {3, -1, true}; if (!users.files.empty() && !diskUsageValid) countProjectUsage(); }
	ImGui::SameLine(); if (CgmButton(LBL("Trim empty atlas pages"))) { Bank before = *bank; if (TrimPages(*bank)) commitStruct(ch, TXT("Trim atlas pages"), std::move(before), {}, host); else { *bank = std::move(before); status = TXT("No empty pages at the end."); } }
	ImGui::TreePop();

	// confirmation with the impact of the operation
	if (pend.open) ImGui::OpenPopup("##cgmconfirm");
	if (ImGui::BeginPopupModal("##cgmconfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		auto at = bank->buildAtlas();
		std::vector<int> targets; if (pend.op == 3) { for (int i = 0; i < count; i++) if (bank->images[i].present && usage.count(i) == 0 && projectCount(i) <= 0 && bank->images[i].drawable() && bank->dependants(i, at).empty()) targets.push_back(i); } else targets.push_back(pend.id);
		int refs = 0, others = 0; for (int t : targets) { refs += usage.count(t); others += std::max(0, projectCount(t)); }
		std::vector<int> deps; if (pend.op != 3 && !targets.empty()) deps = bank->dependants(targets[0], at);
		ImGui::Text(pend.op == 1 ? TXT("Delete image %d and renumber the ones after it?") : pend.op == 2 ? TXT("Replace image %d by an empty placeholder (ids stay)?") : TXT("Delete %d unused image(s) and renumber the rest?"), pend.op == 3 ? (int)targets.size() : pend.id);
		if (refs) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), TXT("%d layer(s) in the frames use it. Their reference will be cleared (set to no sprite)."), refs);
		if (!deps.empty()) { std::string s; for (int d : deps) s += " " + std::to_string(d); ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "%s%s", TXT("Images that borrow its cells:"), s.c_str()); ImGui::TextWrapped("%s", pend.op == 1 ? TXT("They are made independent first (pixels unchanged).") : TXT("Clear is refused while other images borrow its cells: use Delete, which makes them independent first.")); }
		if (diskUsageValid && others && pend.op != 3) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), TXT("%d layer(s) in the project files on disk draw it (other moons / variants included). Saving clears them there too."), others);
		if (pend.op == 3) ImGui::TextDisabled(diskUsageValid ? TXT("Usage counted over every project that draws this bank (%d file(s)).") : TXT("Usage counted in the loaded character only (%d other project file(s) not read)."), (int)users.files.size());
		if (refs == 0 && others == 0 && deps.empty()) ImGui::TextDisabled("%s", TXT("Nothing references it."));
		ImGui::TextDisabled("%s", TXT("Game code that is not in this character (menus, other characters) cannot be checked."));
		const bool refuse = pend.op == 2 && !deps.empty();
		ImGui::BeginDisabled(refuse);
		if (CgmButton(LBL("Do it"))) {
			Bank before = *bank; std::vector<int> remap; std::string e; bool ok = true;
			if (pend.op == 1) ok = DeleteImage(*bank, pend.id, true, remap, &e);
			else if (pend.op == 2) { ok = ClearImage(*bank, pend.id, &e); }
			else {   // delete from the back so earlier ids stay valid; one combined remap
				remap = IdentityRemap((size_t)count);
				std::vector<int> cur = IdentityRemap((size_t)count);
				for (int k = (int)targets.size() - 1; k >= 0 && ok; k--) { std::vector<int> r; const int idNow = cur[targets[k]]; ok = DeleteImage(*bank, idNow, true, r, &e); if (ok) for (int &c : cur) if (c >= 0) c = r[c]; }
				remap = cur;
			}
			if (!ok) { *bank = std::move(before); status = e; }
			else if (commitStruct(ch, pend.op == 1 ? Fmt(TXT("Delete image %d"), pend.id) : pend.op == 2 ? Fmt(TXT("Clear image %d"), pend.id) : Fmt(TXT("Delete %d unused images"), (int)targets.size()), std::move(before), remap, host)) { status = TXT("Done. Frame references were updated."); if (selected >= (int)bank->images.size()) selected = -1; }
			pend.open = false; ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine(); if (CgmButton(LBL("Cancel"))) { pend.open = false; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}

void Window::drawEffect(CharacterInstance &ch, const WindowHost &host) {
	if (!bank || selected < 0 || selected >= (int)bank->images.size()) return;
	Image &im = bank->images[selected];
	ImGui::SeparatorText(TXT("Effect (PACNyx)"));
	// rename
	if (renameFor != selected) { renameFor = selected; memset(renameBuf, 0, sizeof renameBuf); memcpy(renameBuf, im.name, strnlen(im.name, 31)); }
	ImGui::SetNextItemWidth(180); ImGui::InputText(LBL("Name##eff"), renameBuf, sizeof renameBuf);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Rename"))) {
		Bank before = *bank; std::string e;
		if (RenameImage(*bank, selected, renameBuf, &e)) { if (commit(ch, Fmt(TXT("Rename image %d"), selected), std::move(before), host)) status = Fmt(TXT("image %d renamed"), selected); }
		else { status = e; *bank = std::move(before); }
	}
	if (!im.drawable()) return;
	// base point: the canvas origin is the anchor the engine places the sprite with; moving the sprite moves it against that point
	ImGui::AlignTextToFramePadding();
	ImGui::Text(TXT("Sprite at (%d,%d) from its base point (canvas %d x %d)"), im.x1, im.y1, im.w, im.h);
	ImGui::SameLine();
	int d[2] = {0, 0};
	ImGui::PushButtonRepeat(true);
	if (ImGui::ArrowButton("##el", ImGuiDir_Left)) d[0] = -1;
	ImGui::SameLine(); if (ImGui::ArrowButton("##er", ImGuiDir_Right)) d[0] = 1;
	ImGui::SameLine(); if (ImGui::ArrowButton("##eu", ImGuiDir_Up)) d[1] = -1;
	ImGui::SameLine(); if (ImGui::ArrowButton("##ed", ImGuiDir_Down)) d[1] = 1;
	ImGui::PopButtonRepeat();
	if (d[0] || d[1]) {
		const int k = ImGui::GetIO().KeyShift ? bank->cellUnit() : 1;
		Bank before = *bank; std::string e;
		if (OffsetImage(*bank, selected, d[0] * k, d[1] * k, &e)) commit(ch, Fmt(TXT("Move image %d on its canvas"), selected), std::move(before), host);
		else { status = e; *bank = std::move(before); }
		blk.id = -1;
	}
	// block grid
	if (blk.id != selected || blk.gen != cgGen) {
		blk = BlockEdit{}; blk.show = true;
		Rgba r;
		if (bank->decode(selected, r)) { blk.id = selected; blk.gen = cgGen; blk.px = std::move(r.px); blk.w = r.w; blk.h = r.h; blk.mask = CellMask(*bank, selected, blk.cols, blk.rows, blk.ox, blk.oy); }
	}
	if (blk.id != selected) return;
	int on = 0; for (uint8_t m : blk.mask) on += m;
	ImGui::Text(TXT("Blocks: %d of %d cells stored (%d px cells)"), on, (int)blk.mask.size(), bank->cellUnit());
	ImGui::SameLine(); ImGui::Checkbox(LBL("Draw blocks"), &blk.show);
	if (ImGui::Button(LBL("Enable all"))) { std::fill(blk.mask.begin(), blk.mask.end(), 1); blk.pending = true; }
	ImGui::SameLine(); if (ImGui::Button(LBL("Disable all"))) { std::fill(blk.mask.begin(), blk.mask.end(), 0); blk.pending = true; }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Recalculate from alpha"))) {   // every cell that has a visible pixel, nothing else
		Bank before = *bank; std::string e;
		if (ReblockImage(*bank, selected, blk.px.data(), blk.w, blk.h, nullptr, &e)) commit(ch, Fmt(TXT("Recalculate blocks of image %d"), selected), std::move(before), host);
		else { status = e; *bank = std::move(before); }
		blk.id = -1;
	}
	ImGui::BeginDisabled(!blk.pending);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Apply blocks"))) {
		Bank before = *bank; std::string e;
		if (ReblockImage(*bank, selected, blk.px.data(), blk.w, blk.h, &blk.mask, &e)) commit(ch, Fmt(TXT("Block edit of image %d"), selected), std::move(before), host);
		else { status = e; *bank = std::move(before); }
		blk.id = -1;
	}
	ImGui::EndDisabled();
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextDisabled("%s", TXT("Click a cell in the preview to switch it on / off, then Apply. A cell that is switched off is not stored (transparent); a cell without visible pixels is never stored."));
	ImGui::PopTextWrapPos();
}

void Window::drawBlockOverlay(const ImVec2 &p0, float z, int x1, int y1) {
	ImDrawList *dl = ImGui::GetWindowDrawList();
	const int cu = bank->cellUnit();
	for (int cy = 0; cy < blk.rows; cy++) for (int cx = 0; cx < blk.cols; cx++) {
		const float ax = p0.x + (blk.ox + cx * cu - x1) * z, ay = p0.y + (blk.oy + cy * cu - y1) * z;
		const ImVec2 a(ax, ay), b(ax + cu * z, ay + cu * z);
		if (blk.mask[(size_t)cy * blk.cols + cx]) dl->AddRect(a, b, IM_COL32(60, 220, 90, 200));
		else { dl->AddRectFilled(a, b, IM_COL32(220, 60, 60, 60)); dl->AddRect(a, b, IM_COL32(220, 60, 60, 160)); }
	}
}

void Window::clickBlock(const ImVec2 &p0, float z, int x1, int y1) {
	const ImVec2 m = ImGui::GetIO().MousePos;
	const int cu = bank->cellUnit();
	const int px = (int)std::floor((m.x - p0.x) / z) + x1 - blk.ox, py = (int)std::floor((m.y - p0.y) / z) + y1 - blk.oy;
	if (px < 0 || py < 0) return;
	const int cx = px / cu, cy = py / cu;
	if (cx >= blk.cols || cy >= blk.rows) return;
	uint8_t &c = blk.mask[(size_t)cy * blk.cols + cx];
	c = c ? 0 : 1; blk.pending = true;
}

void Window::draw(CharacterInstance *ch, const WindowHost &host) {
	if (!open) return;
	ImGui::SetNextWindowSize(ImVec2(1200, 820), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("CG manager"), &open)) { ImGui::End(); return; }
	if (!ch || !ch->cg.m_loaded) { ImGui::TextDisabled("%s", TXT("The active character has no CG bank.")); ImGui::End(); return; }
	if (owner != ch || cgGen != ch->cg.generation()) rebuild(*ch);
	if (usage.stale(ch->frameData) || (int)usage.byImage.size() < ch->cg.get_image_count()) { usage.build(ch->frameData, ch->cg.get_image_count()); filterKey = ~0ull; }
	const int count = ch->cg.get_image_count();

	const bool editable = bank != nullptr;
	if (editable) {
		const ImGuiIO &io = ImGui::GetIO();
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
		ImGui::BeginDisabled(!hist.canUndo()); if (CgmButton(LBL("Undo")) || (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift)) undoRedo(*ch, false, host); ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::BeginDisabled(!hist.canRedo()); if (CgmButton(LBL("Redo")) || (focused && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))))) undoRedo(*ch, true, host); ImGui::EndDisabled();
		if (hist.canUndo()) { ImGui::SameLine(); ImGui::TextDisabled("%s", hist.top().label.c_str()); }
		static bool cgtoolLayout = false;
		if (CgmButton(LBL("Export all..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				ExportResult er; ExportOptions eo; eo.cgtoolLayout = cgtoolLayout; const unsigned *pp = ch->cg.paletteAt(previewPal, previewPups);
				const std::string nm = ch->getName();
				status = ExportBank(*bank, d, nm, eo, er, (bank->images.size() && pp) ? pp : nullptr) ? Fmt(TXT("Exported %d images (%d files) to %s"), er.images, er.files, d.c_str()) : er.error;
				warnings.clear();
			}
		}
		ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("Writes manifest.json, rgba/*.png, indexed/*.png and palettes/*.pal. Edit the PNGs, then import the folder: only changed images are re-encoded."));
		ImGui::SameLine(); ImGui::Checkbox(LBL("cgtool layout"), &cgtoolLayout);
		ImGui::SetItemTooltip("%s", TXT("<name>_ID_<n>.png, canvas-sized, no manifest (what cgtool writes). Uses the preview palette, so any palette of the .pal can be extracted."));
		ImGui::SameLine();
		if (CgmButton(LBL("Import folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				Bank before = *bank; warnings.clear();
				std::error_code ec;
				if (!std::filesystem::exists(std::filesystem::path(Utf8ToWide(d)) / "manifest.json", ec)) {   // a plain cgtool folder: images by their _ID_<n> / leading id
					FolderImportReport fr;
					if (!ImportPlainFolder(*bank, d, fr)) { *bank = std::move(before); status = fr.error; }
					else {
						warnings = fr.warnings;
						const size_t n = fr.replaced.size() + fr.resized.size();
						if (!n) { *bank = std::move(before); status = Fmt(TXT("Nothing changed (%d images unchanged)."), fr.unchanged); }
						else if (commit(*ch, Fmt(TXT("Import folder (%zu images)"), n), std::move(before), host)) status = Fmt(TXT("Imported a folder without manifest: %d replaced in place, %d re-cut to a new size; %d unchanged. Ids are unchanged."), (int)fr.replaced.size(), (int)fr.resized.size(), fr.unchanged);
					}
				} else {
					ImportResult ir;
					if (!ImportBank(*bank, d, ir)) { *bank = std::move(before); status = ir.error; }
					else {
						warnings = ir.warnings;
						if (ir.changed.empty()) { *bank = std::move(before); status = Fmt(TXT("Nothing changed (%d images unchanged)."), ir.skipped); }
						else if (commit(*ch, Fmt(TXT("Import folder (%zu images)"), ir.changed.size()), std::move(before), host)) status = Fmt(TXT("Imported %zu changed image(s); %d unchanged. Save the bank to keep it."), ir.changed.size(), ir.skipped);
					}
				}
			}
		}
		ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("A folder with manifest.json (Export all), or a plain cgtool export: <name>_ID_<n>.png files, canvas-sized or not. A picture of another size is re-cut and keeps its place on the canvas."));
		ImGui::SameLine();
		if (CgmButton(LBL("Sync structure from folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				Bank before = *bank; std::vector<int> remap; FolderSyncReport fr; warnings.clear();
				if (!SyncBankToFolder(*bank, d, remap, fr)) { *bank = std::move(before); status = fr.error; }
				else if (!fr.added && !fr.removed && !fr.moved && fr.pixels.replaced.empty() && fr.pixels.resized.empty()) { *bank = std::move(before); status = TXT("The folder matches the bank: nothing changed."); }
				else if (commitStruct(*ch, TXT("Sync structure from folder"), std::move(before), remap, host)) {
					warnings = fr.pixels.warnings;
					status = Fmt(TXT("Folder sync: %d added, %d removed, %d moved, %d pictures changed. Every frame reference follows; Save writes the other projects too."), fr.added, fr.removed, fr.moved, (int)(fr.pixels.replaced.size() + fr.pixels.resized.size()));
				}
			}
		}
		ImGui::SetItemTooltip("%s", TXT("The folder becomes the bank: a deleted file deletes its image, a new file adds one, and renaming a file with an order prefix moves it (\"12.5#name_ID_40.png\" goes between 12 and 13). An order.txt listing the files also sets the order."));
		ImGui::BeginDisabled(selected < 0 || selected >= count);
		if (CgmButton(LBL("Replace selected from PNG..."))) {
			const std::string p = FileDialog(-1, false);
			if (!p.empty()) {
				std::vector<uint8_t> px, idx; uint32_t ppal[256]; int w = 0, h = 0; std::string e; ReplaceReport rep; Bank before = *bank;
				const Image &cur = bank->images[selected];
				if (!ReadFolderPng(p, px, w, h, &idx, ppal, &e)) status = e;
				else if (cur.drawable() && w == cur.boundsW() && h == cur.boundsH()) {
					if (!ReplaceImage(*bank, selected, px.data(), w, h, &e, &rep)) { *bank = std::move(before); status = e; }
					else if (!rep.changedPixels && !rep.paletteChanged) { *bank = std::move(before); status = TXT("The PNG renders exactly like the stored image: nothing changed."); }
					else {
						warnings.clear();
						if (rep.lostPixels) warnings.push_back(Fmt(TXT("%d changed pixel(s) sit in cells borrowed from another image and were not written."), rep.lostPixels));
						if (!rep.alsoChanges.empty()) { std::string o; for (int x : rep.alsoChanges) o += " " + std::to_string(x); warnings.push_back(std::string(TXT("Shared cells: these images change too:")) + o); }
						if (commit(*ch, Fmt(TXT("Replace image %d"), selected), std::move(before), host)) status = Fmt(TXT("Image %d replaced (%d pixels). Save the bank to keep it."), selected, rep.changedPixels);
					}
				} else {   // another size: re-cut; a canvas-sized PNG keeps its own position, otherwise the anchor decides
					int x1 = cur.x1, y1 = cur.y1; std::vector<uint8_t> use = px; int uw = w, uh = h;
					if (w == cur.w && h == cur.h) {
						int a, b2, c2, d2;
						if (!TrimToContent(px.data(), w, h, a, b2, c2, d2)) { status = TXT("The PNG is empty."); goto replaceDone; }
						uw = c2 - a + 1; uh = d2 - b2 + 1; use.assign((size_t)uw * uh * 4, 0);
						for (int y = 0; y < uh; y++) memcpy(&use[(size_t)y * uw * 4], &px[((size_t)(y + b2) * w + a) * 4], (size_t)uw * 4);
						x1 = a; y1 = b2;
					} else if (resizeAnchor == 0 && cur.drawable()) { x1 = cur.x1 + cur.boundsW() / 2 - w / 2; y1 = cur.y2 + 1 - h; }
					{
						std::vector<int> un;
						if (x1 < 0 || y1 < 0) status = Fmt(TXT("The new sprite would start at %d,%d, outside its canvas."), x1, y1);
						else if (!ReplaceImageResized(*bank, selected, use.data(), uw, uh, x1, y1, &un, &e)) { *bank = std::move(before); status = e; }
						else if (commitStruct(*ch, Fmt(TXT("Replace image %d (new size)"), selected), std::move(before), {}, host))
							status = Fmt(TXT("Image %d re-cut to %d x %d at %d,%d on its canvas; the id and the frame offsets stay. Save the bank to keep it."), selected, uw, uh, x1, y1);
					}
				}
			replaceDone:;
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::SetNextItemWidth(130);
		{ const char *anchors[] = {"Keep bottom centre", "Keep top left"}; i18n::Combo("##cgmanchor", &resizeAnchor, anchors, 2); }
		ImGui::SetItemTooltip("%s", TXT("For a replacement of another size: which point of the old sprite the new one keeps, so the frame offsets stay right. A canvas-sized PNG keeps its own position."));
		if (!ch->getCGPath().empty()) {
			ImGui::SameLine();
			const bool multi = users.projects.size() > 1 || (users.files.size() > 1);
			if (CgmButton(multi ? LBL("Save bank, its projects and this character") : LBL("Save to the character's .cg"))) {
				std::string rep; const bool ok = saveBankFiles(*ch, rep);
				if (ok && host.saveCharacter && pendingIdChanges(ch) == false && ch->isModified()) host.saveCharacter(ch);
				status = rep;
			}
			ImGui::SetItemTooltip("%s", TXT("Writes the .cg, rewrites the image ids of every other project file that draws it (other moons, b_ files, MBTL variants) and saves this character, so nothing ships with ids the bank does not have."));
		}
		ImGui::SameLine();
		if (CgmButton(LBL("Save bank as..."))) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", "bank.cg");
			const std::string p = FileDialog(-1, true, nm);
			if (!p.empty()) { std::vector<uint8_t> bytes; bank->serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Saved %s"), p.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		ImGui::SameLine();
		if (CgmButton(LBL("New bank from PNG folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				BuildOptions bo; Bank nb; BuildReport br;
				if (!BuildBankFromFolder(d, bo, nb, br)) status = br.error;
				else {
					char nm[128]; snprintf(nm, sizeof(nm), "%s", "new.cg");
					const std::string p = FileDialog(-1, true, nm);
					if (!p.empty()) { std::vector<uint8_t> bytes; nb.serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Built %s: %d image(s), %d of them 8-bit on the bank palette, %d empty placeholder(s)."), p.c_str(), br.images, br.indexed, br.placeholders) : std::string(TXT("Could not write the file.")); }
				}
			}
		}
		ImGui::SetItemTooltip("%s", TXT("cgtool's \"make a bank\": every PNG of the folder becomes an image (ids from _ID_<n> or a leading number, else the file order). The first indexed PNG gives the bank palette; indexed PNGs on that palette are stored 8-bit with their exact indices."));
		// who else draws this bank
		if (!users.projects.empty()) {
			const int shared = (int)users.files.size() - users.ownFileCount();
			ImGui::TextDisabled(TXT("Drawn by %d project(s), %d HA6 file(s)%s"), (int)users.projects.size(), (int)users.files.size(), shared ? TXT(" (shared files are never rewritten)") : "");
			if (ImGui::IsItemHovered()) {
				std::string t; for (auto &pj : users.projects) { t += std::filesystem::path(Utf8ToWide(pj.path)).filename().string() + ":"; for (auto &d : pj.dataFiles) t += " " + d; t += "\n"; }
				ImGui::SetTooltip("%s", t.c_str());
			}
			ImGui::SameLine();
			if (ImGui::SmallButton(LBL("Count usage in all projects"))) { countProjectUsage(); filterKey = ~0ull; }
			if (diskUsageValid) { ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("(counted: the Unused filter and Delete all unused use every project)")); }
			if (pendingIdChanges(ch)) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "%s", TXT("Image ids changed: saving the character also saves the bank and rewrites the other projects, so they never ship mismatched ids."));
		}
		if (bank->cutter2 && !cutter2Note) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", TXT("BMP Cutter2 bank (GAKIHA): pixels sit on pre-baked pages. MBAC itself skips this bank (its 'carries a palette' flag is 0), so edits show here but not in the game."));
		if (ch->frameData.usesUniFormat()) ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), "%s", TXT("UNI2 / MBTL read their data from archives (d/, .bin) that are not rewritten: saved .cg / .pal / .ha6 files reach the game only from a loose override folder (UNI2: the nuuni mod/ tree; MBTL: __Mods or fu/data), with the same relative path."));
		if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
		for (const std::string &w : warnings) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", w.c_str());
	} else {
		// every other bank format (strip / tile banks): batch export and import and single-image replace go through the CG object; no undo, no structural edits
		CgIO io(ch->cg);
		if (CgmButton(LBL("Export all..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) { ExportResult er; ExportOptions eo; status = ExportBank(io, d, ch->getName(), eo, er) ? Fmt(TXT("Exported %d images (%d files) to %s"), er.images, er.files, d.c_str()) : er.error; warnings.clear(); }
		}
		ImGui::SameLine();
		if (CgmButton(LBL("Import folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				ImportResult ir; warnings.clear();
				if (!ImportBank(io, d, ir)) status = ir.error;
				else { warnings = ir.warnings; if (!ir.changed.empty() && host.markEdited) host.markEdited(ch); status = Fmt(TXT("Imported %zu changed image(s); %d unchanged. This format has no undo here: save the bank to keep it."), ir.changed.size(), ir.skipped); clearThumbs(); previewId = -1; filterKey = ~0ull; cgGen = ch->cg.generation(); }
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(selected < 0 || selected >= count);
		if (CgmButton(LBL("Replace selected from PNG..."))) {
			const std::string p = FileDialog(-1, false);
			if (!p.empty()) {
				std::vector<uint8_t> px; int w = 0, h = 0; std::string e; ReplaceReport rep;
				if (!ReadImageRgba(p, px, w, h, e)) status = e;
				else if (!io.replace(selected, px.data(), w, h, &e, &rep, nullptr)) status = e;
				else { status = rep.changedPixels ? Fmt(TXT("Image %d replaced (%d pixels). Save the bank to keep it."), selected, rep.changedPixels) : std::string(TXT("The PNG renders exactly like the stored image: nothing changed.")); if (rep.changedPixels && host.markEdited) host.markEdited(ch); clearThumbs(); previewId = -1; cgGen = ch->cg.generation(); }
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (CgmButton(LBL("Save bank as...")) && ch->cg.foreign()) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", "bank.bin");
			const std::string p = FileDialog(-1, true, nm);
			if (!p.empty()) { std::vector<uint8_t> bytes; ch->cg.foreign()->serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Saved %s"), p.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		ImGui::TextDisabled("%s", TXT("This bank format: browse, batch export / import and image replace. Structural edits and palette editing are for BMP Cutter banks."));
		if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
		for (const std::string &w : warnings) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", w.c_str());
	}

	if (ImGui::BeginTabBar("##cgmtabs")) {
	if (ImGui::BeginTabItem(LBL("Browser"), nullptr, CgmTab("Browser"))) {
	// ---- filters ----
	ImGui::SetNextItemWidth(160); ImGui::InputTextWithHint("##cgmname", TXT("Filter by id or name"), nameFilter, sizeof(nameFilter));
	ImGui::SameLine(); ImGui::Checkbox(LBL("Unused"), &onlyUnused); if (onlyUnused) onlyUsed = false;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Used"), &onlyUsed); if (onlyUsed) onlyUnused = false;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Shares cells"), &onlyShared);
	static const char *kTypes[] = {"Any type", "Type 0", "Type 1", "Type 2", "Type 3", "Type 4", "Type 5", "Type -1"};
	int tsel = typeFilter == -2 ? 0 : typeFilter == -1 ? 7 : typeFilter + 1;
	ImGui::SetNextItemWidth(100); if (i18n::Combo("##cgmtype", &tsel, kTypes, 8)) typeFilter = tsel == 0 ? -2 : tsel == 7 ? -1 : tsel - 1;
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Min side"), &minSize, 0, 0);
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Max side"), &maxSize, 0, 0);
	ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Used by pattern"), &usedByPattern, 0, 0);
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("any##cgmpat"))) usedByPattern = -1;
	ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::SliderInt(LBL("Thumbnail"), &thumbSize, 48, 192);

	uint64_t key = 1469598103934665603ull;
	auto mix = [&](uint64_t v) { key ^= v + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2); };
	for (const char *c = nameFilter; *c; c++) mix((uint8_t)*c);
	mix(onlyUnused); mix(onlyUsed); mix(onlyShared); mix((uint64_t)(typeFilter + 5)); mix((uint64_t)minSize); mix((uint64_t)maxSize); mix((uint64_t)(usedByPattern + 5)); mix(usage.version); mix(cgGen); mix(diskUsageValid); mix((uint64_t)count);
	if (key != filterKey) {
		filterKey = key; shown.clear();
		std::string f = nameFilter; for (auto &c : f) c = (char)tolower((unsigned char)c);
		for (int i = 0; i < count; i++) {
			int bpp, ty, x1, y1, x2, y2;
			const bool ok = ch->cg.image_info(i, bpp, ty, x1, y1, x2, y2);
			if (!ok && typeFilter != -1 && typeFilter != -2) continue;
			if (typeFilter != -2 && (!ok ? typeFilter != -1 : ty != typeFilter)) continue;
			const int side = ok ? std::max(x2 - x1 + 1, y2 - y1 + 1) : 0;
			if (minSize && side < minSize) continue;
			if (maxSize && side > maxSize) continue;
			int uc = usage.count(i);
			if (diskUsageValid) uc += std::max(0, projectCount(i));   // every project on disk, not just the loaded stack
			if (onlyUnused && uc) continue;
			if (onlyUsed && !uc) continue;
			if (usedByPattern >= 0) { bool hit = false; if (i < (int)usage.byImage.size()) for (auto &r : usage.byImage[i]) if (r.pattern == usedByPattern) { hit = true; break; } if (!hit) continue; }
			if (onlyShared && !(bank && (!bank->owners(i, atlas).empty() || !bank->dependants(i, atlas).empty()))) continue;
			if (!f.empty()) {
				std::string nm = ch->cg.get_filename(i) ? ch->cg.get_filename(i) : ""; for (auto &c : nm) c = (char)tolower((unsigned char)c);
				if (nm.find(f) == std::string::npos && std::to_string(i) != f) continue;
			}
			shown.push_back(i);
		}
	}
	ImGui::TextDisabled(TXT("%d of %d images shown"), (int)shown.size(), count);

	// ---- grid (left) ----
	ImGui::BeginChild("##cgmgrid", ImVec2(ImGui::GetContentRegionAvail().x * 0.58f, 0), ImGuiChildFlags_Borders);
	{
		const float cell = (float)thumbSize + 8.f, availW = ImGui::GetContentRegionAvail().x;
		const int cols = std::max(1, (int)(availW / cell));
		const float rowH = cell + ImGui::GetTextLineHeight() + 4.f;
		const int rows = ((int)shown.size() + cols - 1) / cols;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGuiListClipper clip; clip.Begin(rows, rowH);
		int budget = 10;
		while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) for (int c = 0; c < cols; c++) {
			const int k = r * cols + c;
			if (c > 0) ImGui::SameLine();
			if (k >= (int)shown.size()) { ImGui::Dummy(ImVec2(cell, rowH)); continue; }
			const int id = shown[k];
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImGui::PushID(id);
			if (ImGui::InvisibleButton("##t", ImVec2(cell, rowH))) selected = id;
			const bool hov = ImGui::IsItemHovered();
			ImDrawList *dl = ImGui::GetWindowDrawList();
			const ImVec2 a(p0.x + 2, p0.y + 2), b(a.x + thumbSize, a.y + thumbSize);
			Checker(dl, a, b);
			Thumb &t = thumbs[id];
			if (!t.tex && !t.empty && budget > 0) {
				std::vector<uint8_t> px; int w = 0, h = 0; budget--;
				if (fetch(*ch, id, previewPal, previewPups, px, w, h)) upload(t, px, w, h, thumbSize, true); else t.empty = true;
			}
			if (t.tex) {
				const float s = std::min((float)thumbSize / t.w, (float)thumbSize / t.h);
				const ImVec2 sz(t.w * s, t.h * s), o(a.x + (thumbSize - sz.x) * 0.5f, a.y + (thumbSize - sz.y) * 0.5f);
				dl->AddImage((ImTextureID)(intptr_t)t.tex, o, ImVec2(o.x + sz.x, o.y + sz.y));
			} else if (t.empty) dl->AddText(ImVec2(a.x + 6, a.y + 6), ImGui::GetColorU32(ImGuiCol_TextDisabled), "-");
			const bool unused = usage.count(id) == 0;
			dl->AddRect(a, b, id == selected ? IM_COL32(255, 200, 60, 255) : hov ? IM_COL32(180, 200, 255, 255) : unused ? IM_COL32(200, 90, 90, 200) : IM_COL32(110, 110, 120, 255), 0, 0, id == selected ? 2.f : 1.f);
			char lab[48]; snprintf(lab, sizeof(lab), "%d  x%d", id, usage.count(id));
			dl->AddText(ImVec2(a.x, b.y + 2), ImGui::GetColorU32(ImGuiCol_Text), lab);
			if (hov) {
				int bpp, ty, x1, y1, x2, y2; ch->cg.image_info(id, bpp, ty, x1, y1, x2, y2);
				ImGui::SetTooltip("%s\n%dx%d  %s\n%s %d", ch->cg.get_filename(id) ? ch->cg.get_filename(id) : "", x2 - x1 + 1, y2 - y1 + 1, TypeName(ty), TXT("used by layers:"), usage.count(id));
			}
			ImGui::PopID();
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	// ---- inspector (right) ----
	ImGui::BeginChild("##cgminfo");
	if (selected >= 0 && selected < count) {
		int bpp = 0, ty = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
		const bool ok = ch->cg.image_info(selected, bpp, ty, x1, y1, x2, y2);
		ImGui::Text(TXT("Image %d: %s"), selected, ch->cg.get_filename(selected) ? ch->cg.get_filename(selected) : "");
		if (ok) {
			ImGui::Text(TXT("%d x %d, %d-bit, canvas (%d,%d)-(%d,%d)"), x2 - x1 + 1, y2 - y1 + 1, bpp, x1, y1, x2, y2);
			ImGui::TextUnformatted(TypeName(ty));
		} else ImGui::TextDisabled("%s", TXT("This image has no pixels."));
		if (bank && selected < (int)bank->images.size()) {
			const Image &im = bank->images[selected];
			int copyBlocks = 0; for (const Block &b : im.blocks) copyBlocks += b.copy != 0;
			ImGui::Text(TXT("%zu blocks (%d borrow cells from another image), %zu data bytes"), im.blocks.size(), copyBlocks, im.blob ? im.blob->size() : (size_t)0);
			auto own = bank->owners(selected, atlas), dep = bank->dependants(selected, atlas);
			if (!own.empty()) { std::string s; for (int o : own) s += std::to_string(o) + " "; ImGui::TextWrapped(TXT("Draws cells owned by image(s): %s"), s.c_str()); }
			if (!dep.empty()) { std::string s; for (int o : dep) s += std::to_string(o) + " "; ImGui::TextWrapped(TXT("Its cells are drawn by image(s): %s"), s.c_str()); }
		}
		const int nPal = std::max(1, ch->cg.getPalNumber());
		if (nPal > 1 || ch->cg.pupsBankCount() > 1) {
			ImGui::SetNextItemWidth(90); if (ImGui::InputInt(LBL("Palette"), &previewPal, 1, 8)) { previewPal = std::clamp(previewPal, 0, nPal - 1); clearThumbs(); previewId = -1; }
			if (ch->cg.pupsBankCount() > 1) { ImGui::SameLine(); ImGui::SetNextItemWidth(70); if (ImGui::InputInt(LBL("PUPS bank"), &previewPups, 1, 1)) { previewPups = std::clamp(previewPups, 0, ch->cg.pupsBankCount() - 1); clearThumbs(); previewId = -1; } }
		}
		ImGui::SetNextItemWidth(120); ImGui::SliderFloat(LBL("Zoom"), &zoom, 0.25f, 8.f);
		if (previewId != selected || previewGen != cgGen) {
			previewId = selected; previewGen = cgGen;
			std::vector<uint8_t> px; int w = 0, h = 0;
			if (ok && fetch(*ch, selected, previewPal, previewPups, px, w, h)) upload(preview, px, w, h, 0, false); else { preview.w = preview.h = 0; }
		}
		if (bank) drawStructure(*ch, host);
		if (bank) drawEffect(*ch, host);
		ImGui::SeparatorText(TXT("Used by"));
		if (selected < (int)usage.byImage.size() && !usage.byImage[selected].empty()) {
			ImGui::BeginChild("##cgmuse", ImVec2(0, 110), ImGuiChildFlags_Borders);
			for (const UsageRef &r : usage.byImage[selected]) {
				char l[96]; snprintf(l, sizeof(l), TXT("pattern %d, frame %d, layer %d"), r.pattern, r.frame, r.layer);
				if (ImGui::Selectable(l) && host.navigate) host.navigate(r.pattern, r.frame);
			}
			ImGui::EndChild();
		} else ImGui::TextDisabled("%s", TXT("Not referenced by any frame of this character."));
		ImGui::BeginChild("##cgmprev", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
		if (preview.w > 0 && ok) {
			const ImVec2 p0 = ImGui::GetCursorScreenPos(), sz(preview.w * zoom, preview.h * zoom);
			ImGui::Dummy(sz);
			Checker(ImGui::GetWindowDrawList(), p0, ImVec2(p0.x + sz.x, p0.y + sz.y));
			ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)preview.tex, p0, ImVec2(p0.x + sz.x, p0.y + sz.y));
			if (bank && blk.id == selected && blk.show) {
				drawBlockOverlay(p0, zoom, x1, y1);
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clickBlock(p0, zoom, x1, y1);
			}
		}
		ImGui::EndChild();
	} else ImGui::TextDisabled("%s", TXT("Click an image in the grid."));
	ImGui::EndChild();
	ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(LBL("Palettes"), nullptr, CgmTab("Palettes"))) {
		drawPalettes(*ch, host);
		ImGui::EndTabItem();
	}
	static const bool fxrFirst = std::getenv("HANTEI_CGM_TAB") && !strcmp(std::getenv("HANTEI_CGM_TAB"), "fxr");   // screenshot / test hook
	static bool fxrSelectOnce = fxrFirst;
	if (ImGui::BeginTabItem(LBL("Effect recolor"), nullptr, (fxrSelectOnce ? ImGuiTabItemFlags_SetSelected : 0) | CgmTab("Effect recolor"))) {
		fxrSelectOnce = false;
		fxr.draw(*ch, bank.get(), usage, host.navigate);
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	}
	ImGui::End();
}


bool Window::script(const std::string &cmd, const std::string &arg, std::string *msg) {
	auto num = [&](int &v) { v = atoi(arg.c_str() + arg.find(' ') + 1); return true; };
	const std::string key = arg.substr(0, arg.find(' '));
	if (cmd == "open") { open = arg != "off"; return true; }
	if (cmd == "tab") { g_scriptTab = arg; return true; }
	if (cmd == "press") { g_scriptPress = arg; return true; }
	if (cmd == "path") { g_scriptPaths.push_back(arg); return true; }
	if (cmd == "select") { selected = atoi(arg.c_str()); previewId = -1; return true; }
	if (cmd == "set") {
		if (key == "moveTarget") return num(moveTarget);
		if (key == "insertAt") return num(insertAt);
		if (key == "newPups") return num(newPups);
		if (key == "ccChara") return num(ccChara);
		if (key == "ccFirst") return num(ccFirst);
		if (key == "ccFrom") return num(ccFrom);
		if (key == "ccTo") return num(ccTo);
		if (key == "palBank") return num(pal.bank);
		if (key == "palNumber") return num(pal.number);
		if (key == "previewPal") return num(previewPal);
		if (msg) *msg = "unknown field " + key; return false;
	}
	if (cmd == "status") { if (msg) *msg = "status: " + status + (g_scriptPress.empty() ? "" : " (press '" + g_scriptPress + "' still pending)"); return false; }
	if (msg) *msg = "unknown cgm command " + cmd;
	return false;
}

} // namespace cgm
