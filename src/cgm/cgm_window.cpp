#include "cgm_window.h"
#include "../ui/ui_style.h"
#include "../ui/ui_layout2.h"
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
#include "cgm_texsrc.h"
#include "../parts/pat_texconv.h"
#include "../han2_pat.h"
#include "../texture.h"
#include <memory>
#include "../character_instance.h"
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
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
bool CgmButton(const char *label, const ImVec2 &size = ImVec2(0, 0)) {
	bool r = ImGui::Button(label, size);
	if (!g_scriptPress.empty() && LabelIs(label, g_scriptPress)) { g_scriptPress.clear(); r = true; }
	return r;
}
// A CgmButton in the next cell of an equal-width ui::ButtonGrid (rows wrap evenly).
bool GridButton(ui::ButtonGrid &g, const char *label) { g.Next(); return CgmButton(label, ImVec2(g.cellW, 0)); }
// A CgmButton that follows the previous item on the same line while it fits, otherwise starts a new line.
bool FitButton(const char *label, float spacing = -1.f) { ui::SameLineButton(label, spacing); return CgmButton(label); }
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
	pp.shownId = -1; idxUse.owner = nullptr;   // pixels or image count may have changed (palette pushes keep cgGen in step and skip this)
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
	if (redo ? !hist.canRedo() : !hist.canUndo()) return;
	if (!bank && (redo ? hist.topRedo().hasBank : hist.top().hasBank)) return;
	HistoryEntry e = redo ? hist.redo() : hist.undo();
	if (e.patBefore) restorePatTex(ch, redo ? e.patAfter : e.patBefore, host);
	if (e.rawBefore) {   // a CG bank format the Bank model does not hold: its stored bytes
		const std::vector<uint8_t> &b = redo ? *e.rawAfter : *e.rawBefore;
		if (b.size() == ch.cg.bank_size()) ch.cg.restore_bank((const char *)b.data(), (unsigned)b.size());
		else ch.cg.replaceBank(b.data(), (unsigned)b.size());
		clearThumbs(); previewId = -1; filterKey = ~0ull;
		if (host.markEdited) host.markEdited(&ch);
	}
	if (!e.modDst.empty()) {   // a mod-folder write: put the file back the way it was (removed when there was none)
		std::string er; bool ok;
		if (redo) ok = WriteModFile(e.modDst, *e.modAfter, &er);
		else if (e.modBefore) ok = WriteModFile(e.modDst, *e.modBefore, &er);
		else ok = RestoreModFile(e.modDst, &er);
		if (!ok) status = er;
	}
	if (e.hasBank && bank) {
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

// A slider whose value text stays readable: the theme's orange grab used to sit on top of the centred value ("0 deg" under the
// grab at the default). The value is drawn at the end of the frame away from the grab instead; Ctrl+click typing is untouched.
static bool ReadableSliderFloat(const char *label, float *v, float mn, float mx, const char *fmt) {
	const ImGuiID id = ImGui::GetID(label);
	const bool typing = ImGui::TempInputIsActive(id);
	if (!typing) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
	const bool changed = ImGui::SliderFloat(label, v, mn, mx, fmt);
	if (typing) return changed;
	ImGui::PopStyleColor();
	char buf[64]; std::snprintf(buf, sizeof buf, fmt, *v);
	const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax(), ts = ImGui::CalcTextSize(buf);
	const float t = mx > mn ? std::clamp((*v - mn) / (mx - mn), 0.f, 1.f) : 0.f, pad = ImGui::GetStyle().FramePadding.x;
	const float x = t >= 0.5f ? a.x + pad : b.x - pad - ts.x;   // the half the grab is not in
	ImGui::GetWindowDrawList()->AddText(ImVec2(x, a.y + (b.y - a.y - ts.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), buf);
	return changed;
}

// Palette indices drawn by the bank's 8-bit images (a few dozen images per frame so a big UNI2 bank does not stall the UI), plus the
// PPPA colour slots the .pat parts tint from (part_render_math.h PartTintPalette). Index 0 is the transparent entry: counted as used.
void Window::countIndexUse(CharacterInstance &ch) {
	const int count = ch.cg.get_image_count();
	if (idxUse.owner != &ch || idxUse.images != count) {
		idxUse = IndexUse(); idxUse.owner = &ch; idxUse.images = count; idxUse.used[0] = true;
		for (const auto &c : ch.parts.cutOuts) if (c.colorSlot >= 0 && c.colorSlot < 256) idxUse.tint[c.colorSlot] = true;
	}
	for (int budget = 48; !idxUse.done && budget > 0; budget--) {
		if (idxUse.next >= count) { idxUse.done = true; break; }
		const int n = idxUse.next++;
		if (!ch.cg.image_is_8bpp((unsigned)n)) continue;
		std::unique_ptr<ImageData> im(ch.cg.draw_texture((unsigned)n, false, true));
		if (!im || !im->is8bpp || !im->pixels) continue;
		const size_t px = (size_t)im->width * im->height;
		for (size_t i = 0; i < px; i++) idxUse.used[im->pixels[i]] = true;
	}
}

// The preview pane: the active frame's palette-following sprite (or any 8-bit CG image) drawn with `colors`, the palette as the grid
// shows it. Hovering a grid cell (hoverIndex) dims every other index; clicking a pixel selects its index in the grid.
void Window::drawPalPreview(CharacterInstance &ch, const WindowHost &host, const uint32_t *colors, int hoverIndex, float boxW, float boxH) {
	auto &P = pal;
	ImGui::PushID("##palpreview");
	const int count = ch.cg.get_image_count();
	ImGui::SetNextItemWidth(boxW);
	const char *modes[] = {TXT("Sprite of the current frame"), TXT("CG image")};
	ImGui::Combo("##ppmode", &pp.mode, modes, 2);
	int id = -1;
	if (pp.mode == 0) {
		int pat = -1, fr = 0;
		std::vector<std::pair<int, int>> lay;   // layer, image
		if (host.current && host.current(pat, fr)) {
			if (Sequence *seq = ch.frameData.get_sequence(pat); seq && fr >= 0 && fr < (int)seq->frames.size()) {
				const auto &L = seq->frames[fr].AF.layers;
				for (int l = 0; l < (int)L.size(); l++) if (!L[l].usePat && L[l].spriteId >= 0 && L[l].spriteId < count && ch.cg.image_is_8bpp((unsigned)L[l].spriteId)) lay.push_back({l, L[l].spriteId});
			}
		}
		if (lay.empty()) ImGui::TextDisabled(TXT("Pattern %d frame %d draws no palette-following sprite."), pat, fr);
		else {
			int k = 0; for (size_t i = 0; i < lay.size(); i++) if (lay[i].first == pp.layer) k = (int)i;
			if (lay.size() > 1) {
				ImGui::SetNextItemWidth(boxW);
				if (ImGui::BeginCombo("##pplayer", Fmt(TXT("Layer %d: image %d"), lay[k].first, lay[k].second).c_str())) {
					for (size_t i = 0; i < lay.size(); i++) if (ImGui::Selectable(Fmt(TXT("Layer %d: image %d"), lay[i].first, lay[i].second).c_str(), (int)i == k)) k = (int)i;
					ImGui::EndCombo();
				}
			} else ImGui::TextDisabled(TXT("Pattern %d frame %d, image %d"), pat, fr, lay[k].second);
			pp.layer = lay[k].first; id = lay[k].second;
		}
	} else {
		ImGui::SetNextItemWidth(boxW);
		if (ImGui::InputInt("##ppimage", &pp.image)) pp.image = std::clamp(pp.image, 0, std::max(0, count - 1));
		if (pp.image >= 0 && pp.image < count && ch.cg.image_is_8bpp((unsigned)pp.image)) id = pp.image;
		else ImGui::TextDisabled("%s", TXT("Not a palette-following (8-bit) image."));
	}
	if (id >= 0 && id != pp.shownId) {
		pp.shownId = id; pp.texKey = 0; pp.idx.clear(); pp.w = pp.h = 0;
		std::unique_ptr<ImageData> im(ch.cg.draw_texture((unsigned)id, false, true));
		if (im && im->is8bpp && im->pixels) { pp.w = im->width; pp.h = im->height; pp.idx.assign(im->pixels, im->pixels + (size_t)pp.w * pp.h); }
	}
	pp.hover = -1;
	if (id < 0 || pp.idx.empty()) { ImGui::Dummy(ImVec2(boxW, boxH * 0.5f)); ImGui::PopID(); return; }
	// recolour only when the palette or the highlighted index changed
	uint64_t key = 1469598103934665603ull;
	auto mix = [&](uint64_t v) { key = (key ^ v) * 1099511628211ull; };
	for (int i = 0; i < 256; i++) mix(colors[i]);
	mix((uint64_t)(hoverIndex + 2)); mix((uint64_t)id);
	if (key != pp.texKey || !pp.tex.tex) {
		std::vector<uint8_t> rgba((size_t)pp.w * pp.h * 4, 0);
		for (size_t i = 0; i < pp.idx.size(); i++) {
			const int k = pp.idx[i]; if (k == 0) continue;
			uint8_t *o = &rgba[i * 4]; const uint32_t c = colors[k];
			o[0] = cgm::R(c); o[1] = cgm::G(c); o[2] = cgm::B(c); o[3] = 255;
			if (hoverIndex >= 0 && k != hoverIndex) { const uint8_t g = (uint8_t)((o[0] + o[1] + o[2]) / 9 + 24); o[0] = o[1] = o[2] = g; o[3] = 70; }   // greyed: the hovered index stands out
		}
		upload(pp.tex, rgba, pp.w, pp.h, 0, false); pp.texKey = key;
	}
	// fit the box, whole-number zoom when the sprite is smaller than the box
	float z = std::min(boxW / pp.w, boxH / pp.h); if (z >= 1.f) z = std::floor(z);
	const ImVec2 sz(pp.w * z, pp.h * z), c0 = ImGui::GetCursorScreenPos();
	const ImVec2 o(c0.x + std::floor((boxW - sz.x) * 0.5f), c0.y);
	Checker(ImGui::GetWindowDrawList(), c0, ImVec2(c0.x + boxW, c0.y + std::max(sz.y, ui::Em(4.f))));
	ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)pp.tex.tex, o, ImVec2(o.x + sz.x, o.y + sz.y));
	ImGui::InvisibleButton("##pppick", ImVec2(boxW, std::max(sz.y, ui::Em(4.f))));
	if (ImGui::IsItemHovered()) {
		const ImVec2 m = ImGui::GetMousePos(); const int x = (int)((m.x - o.x) / z), y = (int)((m.y - o.y) / z);
		if (x >= 0 && y >= 0 && x < pp.w && y < pp.h) {
			const int k = pp.idx[(size_t)y * pp.w + x];
			pp.hover = k;
			ImGui::SetTooltip(TXT("Index %d  #%02X%02X%02X\nClick: select it in the grid (Shift: extend)"), k, cgm::R(colors[k]), cgm::G(colors[k]), cgm::B(colors[k]));
			if (ImGui::IsItemClicked(0)) { if (ImGui::GetIO().KeyShift) P.selB = k; else P.selA = P.selB = k; }
		}
	}
	ImGui::TextDisabled(TXT("Image %d  %dx%d  x%.2g"), id, pp.w, pp.h, z);
	ImGui::PopID();
}

void Window::drawPalettes(CharacterInstance &ch, const WindowHost &host) {
	if (palOwner != &ch) loadPalettes(ch);
	auto &P = pal;
	// ---- source ----
	std::vector<std::string> names; std::vector<int> ids;   // id -1 = bank slots, 0..7 = palette file bank
	if (bank) { names.push_back(TXT("Bank slots (inside the .cg)")); ids.push_back(-1); }
	for (int b = 0; b < 8; b++) if (P.loaded[b]) { names.push_back(std::filesystem::path(ch.cg.palettePath(b)).filename().string() + (P.dirty[b] ? " *" : "")); ids.push_back(b); }
	if (names.empty()) { ImGui::TextDisabled("%s", TXT("This character has no palette file and its bank is not editable.")); return; }
	int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == P.bank) cur = (int)i;
	const bool isFile = P.bank >= 0;
	const int total = isFile ? P.set[P.bank].count : 8;
	P.number = std::clamp(P.number, 0, std::max(0, total - 1));
	// label-first source cells: the source combo, then the number with its count and "Show in main view"
	if (ui::BeginFormGrid("##palsrc", 2, ui::Em(18.f))) {
		ui::FormCell(TXT("Palette source"));
		if (ImGui::BeginCombo("##palsource", names[cur].c_str())) { for (size_t i = 0; i < ids.size(); i++) if (ImGui::Selectable(names[i].c_str(), (int)i == cur)) { P.bank = ids[i]; P.number = 0; } ImGui::EndCombo(); }
		ui::FormCell(TXT("Palette number"));
		ImGui::SetNextItemWidth(ui::Px(110));
		if (ImGui::InputInt("##palnumber", &P.number, 1, 8)) { P.number = std::clamp(P.number, 0, total - 1); if (isFile) { previewPal = P.number; previewPups = P.bank; } clearThumbs(); previewId = -1; }
		ImGui::SameLine(); ImGui::TextDisabled(TXT("of %d"), total);
		if (isFile && FitButton(LBL("Show in main view"))) { ch.palette = P.number; ch.cg.changePaletteNumber(P.number); }
		ui::EndFormGrid();
	}

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
	// swatches (fixed column) | colour tools (the rest of the width) | sprite preview (wide windows; below the grid otherwise)
	countIndexUse(ch);
	const float cell = std::floor(ui::Px(20.f));
	const bool threeCols = ImGui::GetContentRegionAvail().x >= cell * 16 + ui::Em(46.f);
	const float prevW = threeCols ? std::max(ui::Em(15.f), std::floor(ImGui::GetContentRegionAvail().x * 0.24f)) : cell * 16;
	const bool palSplit = ImGui::BeginTable("##palmain", threeCols ? 3 : 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings);
	if (palSplit) {
		ImGui::TableSetupColumn("##sw", ImGuiTableColumnFlags_WidthFixed, cell * 16);
		ImGui::TableSetupColumn("##tools", ImGuiTableColumnFlags_WidthStretch);
		if (threeCols) ImGui::TableSetupColumn("##preview", ImGuiTableColumnFlags_WidthFixed, prevW);
		ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
	}
	// The colour each entry shows: the live slider preview of the selection on bank slots (palette files are adjusted in place).
	uint32_t shown[256];
	{
		const int lo = std::min(P.selA, P.selB), hi = std::max(P.selA, P.selB);
		const bool preview = !isFile && !P.adj.neutral();
		for (int i = 0; i < 256; i++) shown[i] = preview && i >= lo && i <= hi ? cgm::Adjust(colors[i], P.adj) : colors[i];
	}
	int gridHover = -1;
	ImVec2 g0 = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##swatches", ImVec2(cell * 16, cell * 16));
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();
		const int lo = std::min(P.selA, P.selB), hi = std::max(P.selA, P.selB);
		const ImU32 bg = ImGui::GetColorU32(ImGuiCol_WindowBg), hatch = IM_COL32(128, 128, 128, 200);
		for (int i = 0; i < 256; i++) {
			const ImVec2 a(g0.x + (i % 16) * cell, g0.y + (i / 16) * cell), b(a.x + cell - 1, a.y + cell - 1);
			const ImU32 col = ImGui::ColorConvertFloat4ToU32(Col4(shown[i]));
			if (idxUse.done && !idxUse.used[i] && !idxUse.tint[i]) {   // no sprite draws it: the stored colour, faded and hatched
				dl->AddRectFilled(a, b, bg);
				dl->AddRectFilled(a, b, (col & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 45));
				dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), hatch);
				dl->AddLine(ImVec2(a.x, a.y + (b.y - a.y) * 0.5f), ImVec2(a.x + (b.x - a.x) * 0.5f, a.y), hatch);
				dl->AddLine(ImVec2(a.x + (b.x - a.x) * 0.5f, b.y), ImVec2(b.x, a.y + (b.y - a.y) * 0.5f), hatch);
			} else dl->AddRectFilled(a, b, col);
			if (idxUse.tint[i]) dl->AddTriangleFilled(a, ImVec2(a.x + cell * 0.4f, a.y), ImVec2(a.x, a.y + cell * 0.4f), IM_COL32(40, 200, 255, 255));
			if (i >= lo && i <= hi) dl->AddRect(a, b, IM_COL32(255, 200, 60, 255), 0, 0, 2.f);
			if (i == pp.hover) dl->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), IM_COL32(255, 255, 255, 255), 0, 0, 2.f);
		}
		if (ImGui::IsItemHovered()) {
			const ImVec2 m = ImGui::GetMousePos(); const int cx = (int)((m.x - g0.x) / cell), cy = (int)((m.y - g0.y) / cell), i = std::clamp(cy, 0, 15) * 16 + std::clamp(cx, 0, 15);
			gridHover = i;
			if (ImGui::IsMouseClicked(0)) { if (ImGui::GetIO().KeyShift) P.selB = i; else P.selA = P.selB = i; }
			else if (ImGui::IsMouseDragging(0)) P.selB = i;
			const char *note = !idxUse.done ? "" : idxUse.tint[i] ? TXT("\nPPPA tint slot of a .pat part") : !idxUse.used[i] ? TXT("\nNo sprite draws this entry") : "";
			ImGui::SetTooltip("%d  #%02X%02X%02X%s", i, cgm::R(colors[i]), cgm::G(colors[i]), cgm::B(colors[i]), note);
		}
	}
	if (gridHover < 0) gridHover = pp.scriptHover;
	{   // legend
		const float s = std::floor(ImGui::GetTextLineHeight() * 0.8f);
		auto key = [&](bool hatched, const char *text) {
			const ImVec2 a = ImGui::GetCursorScreenPos(); const float dy = (ImGui::GetTextLineHeight() - s) * 0.5f;
			ImDrawList *dl = ImGui::GetWindowDrawList(); const ImVec2 p0(a.x, a.y + dy), p1(a.x + s, a.y + dy + s);
			if (hatched) { dl->AddRectFilled(p0, p1, IM_COL32(0, 255, 0, 45)); dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p0.y), IM_COL32(128, 128, 128, 200)); }
			else { dl->AddRectFilled(p0, p1, IM_COL32(120, 120, 120, 255)); dl->AddTriangleFilled(p0, ImVec2(p0.x + s * 0.5f, p0.y), ImVec2(p0.x, p0.y + s * 0.5f), IM_COL32(40, 200, 255, 255)); }
			ImGui::Dummy(ImVec2(s, ImGui::GetTextLineHeight())); ImGui::SameLine(); ImGui::TextDisabled("%s", text);
		};
		int unused = 0, tints = 0; for (int i = 0; i < 256; i++) { unused += idxUse.done && !idxUse.used[i] && !idxUse.tint[i]; tints += idxUse.tint[i]; }
		if (!idxUse.done) ImGui::TextDisabled(TXT("Counting the entries sprites use... %d / %d images"), idxUse.next, idxUse.images);
		else key(true, Fmt(TXT("No sprite draws it (%d entries; colours kept)"), unused).c_str());
		if (tints) key(false, Fmt(TXT("PPPA tint slot of a .pat part (%d)"), tints).c_str());
	}
	if (palSplit && !threeCols) {   // narrow: the preview under the grid
		ImGui::Spacing(); drawPalPreview(ch, host, shown, gridHover, cell * 16, cell * 12);
	}
	if (palSplit) ImGui::TableSetColumnIndex(1); else ImGui::SameLine(0, ui::Px(16));
	ImGui::BeginGroup();
	const int lo = std::min(P.selA, P.selB), hi = std::max(P.selA, P.selB);
	ImGui::Text(TXT("Selected: %d - %d"), lo, hi);
	{   // single colour
		float c[3] = {cgm::R(colors[lo]) / 255.f, cgm::G(colors[lo]) / 255.f, cgm::B(colors[lo]) / 255.f};
		static PalSet liveBefore; static Bank liveBankBefore; static bool live = false;
		ImGui::SetNextItemWidth(std::max(ui::Em(10.f), ImGui::GetContentRegionAvail().x - ui::LabelTextW(LBL("Colour")) - ImGui::GetStyle().ItemInnerSpacing.x));
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
		if (ImGui::ColorEdit3("##fill", f, ImGuiColorEditFlags_NoInputs)) P.fill = FromCol(f);
		ImGui::SameLine(); ImGui::Checkbox(LBL("via HSV"), &P.hsvGradient);
		ui::ButtonGrid g({LBL("Fill selection"), LBL("Gradient first -> last"), LBL("Reverse"), LBL("Copy range"), LBL("Paste at selection")});
		if (GridButton(g, LBL("Fill selection"))) edit(Fmt(TXT("Fill %d-%d"), lo, hi), [&](uint32_t *p) { FillRange(p, lo, hi, P.fill); });
		if (GridButton(g, LBL("Gradient first -> last"))) edit(Fmt(TXT("Gradient %d-%d"), lo, hi), [&](uint32_t *p) { Gradient(p, lo, hi, p[lo], p[hi], P.hsvGradient); });
		if (GridButton(g, LBL("Reverse"))) edit(Fmt(TXT("Reverse %d-%d"), lo, hi), [&](uint32_t *p) { ReverseRange(p, lo, hi); });
		if (GridButton(g, LBL("Copy range"))) { P.clipLen = hi - lo + 1; memcpy(P.clip, colors + lo, sizeof(uint32_t) * P.clipLen); }
		ImGui::BeginDisabled(P.clipLen == 0);
		if (GridButton(g, LBL("Paste at selection"))) edit(Fmt(TXT("Paste at %d"), lo), [&](uint32_t *p) { CopyRange(P.clip, 0, P.clipLen - 1, p, lo); });
		ImGui::EndDisabled();
	}
	{   // hue / saturation / value of the selection, live
		static PalSet baseSet; static Bank baseBank; static bool active = false; static uint32_t baseColors[256];
		bool changed = false, started = false, finished = false;
		auto sl = [&](const char *l, float *v, float mn, float mx, const char *fmt) { changed |= ReadableSliderFloat(ui::FormRow(l), v, mn, mx, fmt); started |= ImGui::IsItemActivated(); finished |= ImGui::IsItemDeactivated(); };
		if (ui::BeginLabelForm("##paladj", ui::LabelColW({LBL("Hue shift"), LBL("Saturation x"), LBL("Brightness x")}))) {
			sl(LBL("Hue shift"), &P.adj.hueDeg, -180.f, 180.f, "%.0f deg"); sl(LBL("Saturation x"), &P.adj.satMul, 0.f, 2.f, "%.2f"); sl(LBL("Brightness x"), &P.adj.valMul, 0.f, 2.f, "%.2f");
			ui::EndLabelForm();
		}
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
		ui::SameLineField(ui::Px(70), LBL("Tolerance")); ImGui::SetNextItemWidth(ui::Px(70)); ImGui::InputInt(LBL("Tolerance"), &P.tolerance, 0, 0); P.tolerance = std::clamp(P.tolerance, 0, 255);
		if (FitButton(LBL("Replace in this palette"))) edit(TXT("Replace colour"), [&](uint32_t *p) { ReplaceColor(p, P.findC, P.replaceC, P.tolerance); });
	}
	ImGui::EndGroup();
	if (palSplit && threeCols) {
		ImGui::TableSetColumnIndex(2);
		ImGui::TextUnformatted(TXT("Preview"));
		drawPalPreview(ch, host, shown, gridHover, prevW, cell * 16);
	}
	if (palSplit) ImGui::EndTable();

	// ---- palette files ----
	ImGui::SeparatorText(TXT("Palettes of this file"));
	if (isFile) {
		PalSet &ps = P.set[P.bank];
		auto fileEdit = [&](const std::string &label, auto &&fn) { PalSet before = ps; std::string e; if (fn(e) && ps.raw != before.raw) { commitPalette(ch, label, P.bank, std::move(before), host); status.clear(); } else if (!e.empty()) status = e; };
		if (ps.paired()) {
			const int c = ps.colourOf(P.number);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.f, 1.f));
			ImGui::TextWrapped(TXT("UNI2 / MBTL layout: %d colours x 2 sets. Palette %d is colour %d's %s set; palette %d is its %s set. Moves keep the two together."),
				ps.half(), P.number, c, P.number < ps.half() ? TXT("main") : TXT("alternate"), ps.alternateOf(P.number), P.number < ps.half() ? TXT("alternate") : TXT("main"));
			ImGui::PopStyleColor();
			if (ImGui::SmallButton(LBL("Go to its pair"))) { P.number = ps.alternateOf(P.number); clearThumbs(); previewId = -1; }
		}
		{
		ui::ButtonGrid g({LBL("Duplicate"), LBL("Delete"), LBL("Move up"), LBL("Move down")});
		if (GridButton(g, LBL("Duplicate"))) fileEdit(TXT("Duplicate palette"), [&](std::string &e) { if (!ps.addCopy(P.number, &e)) return false; P.number = ps.count - 1; return true; });
		if (GridButton(g, LBL("Delete"))) fileEdit(TXT("Delete palette"), [&](std::string &e) { if (!ps.remove(P.number, &e)) return false; P.number = std::min(P.number, ps.count - 1); return true; });
		if (GridButton(g, LBL("Move up")) && ps.colourOf(P.number) > 0) fileEdit(TXT("Move palette"), [&](std::string &) { ps.movePair(P.number, ps.colourOf(P.number) - 1); P.number--; return true; });
		if (GridButton(g, LBL("Move down")) && ps.colourOf(P.number) + 1 < ps.half()) fileEdit(TXT("Move palette"), [&](std::string &) { ps.movePair(P.number, ps.colourOf(P.number) + 1); P.number++; return true; });
		}
		// save row: equal cells, the mod-folder save only for the archive games
		bool modGame = false; modFolderDst(ch, "", &modGame);
		const char *saveAsLayout = ps.offsetDw == 4 ? LBL("Save as MBAACC layout...") : LBL("Save as UNI2/MBTL layout...");
		ui::ButtonGrid sg = modGame ? ui::ButtonGrid({LBL("Save .pal"), LBL("Save .pal to mod folder"), LBL("Save .pal as..."), saveAsLayout}) : ui::ButtonGrid({LBL("Save .pal"), LBL("Save .pal as..."), saveAsLayout});
		if (GridButton(sg, LBL("Save .pal"))) {
			std::string e; const std::string path = AnsiToUtf8(ch.cg.palettePath(P.bank));
			if (WritePalSet(path, ps, true, &e)) { P.dirty[P.bank] = false; status = Fmt(TXT("Saved %s (the original is kept as .bak)"), path.c_str()); } else status = e;
		}
		// UNI2 / MBTL read their .pal from the archives: an edited palette reaches the game only through the IM mod folder (the same
		// place the "CG + .pat textures" tab saves the .cg / .pat; its game and game folder settings apply here too)
		if (modGame) {
			if (GridButton(sg, LBL("Save .pal to mod folder"))) {
				const std::string dst = modFolderDst(ch, ch.cg.palettePath(P.bank)); std::string e;
				if (dst.empty()) status = Fmt(TXT("Cannot place %s in the mod folder: no game folder, or the file is not under a data folder."), ch.cg.palettePath(P.bank).c_str());
				else status = writeModStep(dst, ps.raw, Fmt(TXT("Save %s to mod folder"), std::filesystem::u8path(dst).filename().u8string().c_str()), &e) ? Fmt(TXT("Wrote %s (%zu bytes). The improvement mod / fpan-im serve it in place of the archived file."), dst.c_str(), ps.raw.size()) : e;
			}
			ImGui::SetItemTooltip("%s", TXT("Writes this .pal to <game folder>\\UNI2-IM\\Mods or MBTL-IM\\Mods (set in the \"CG + .pat textures\" tab). \"Restore mod folder\" there, or cgmtex modrestore, undoes it."));
		}
		if (GridButton(sg, LBL("Save .pal as..."))) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", std::filesystem::path(ch.cg.palettePath(P.bank)).filename().string().c_str());
			const std::string path = FileDialog(-1, true, nm);
			if (!path.empty()) { std::ofstream f(std::filesystem::u8path(path), std::ios::binary); f.write((const char *)ps.raw.data(), (std::streamsize)ps.raw.size()); status = f ? Fmt(TXT("Saved %s"), path.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		if (GridButton(sg, saveAsLayout)) {
			const PalSet conv = ps.converted(ps.offsetDw != 4);
			char nm[128]; snprintf(nm, sizeof(nm), "%s", std::filesystem::path(ch.cg.palettePath(P.bank)).filename().string().c_str());
			const std::string path = FileDialog(-1, true, nm); std::string e;
			if (!path.empty()) status = WritePalSet(path, conv, false, &e) ? Fmt(TXT("Saved %s: %d palettes"), path.c_str(), conv.count) : e;
		}
		ImGui::SetItemTooltip("%s", TXT("MBAACC: count + palettes. UNI2 / MBTL: header FFFF,1,0,130 = 65 colours, palette c + 65 being colour c's alternate set (an MBAACC file gives each colour itself as its alternate)."));
		// a new PUPS bank (<cg>_pN.pal): what a pattern's PUPS value selects in UNI2 / MBTL
		if (!ch.getCGPath().empty() && P.loaded[0]) {
			ImGui::SetNextItemWidth(ui::Px(90)); ImGui::InputInt("##newpups", &newPups, 1, 1); newPups = std::clamp(newPups, 1, 7);
			ImGui::BeginDisabled(P.loaded[newPups]);
			if (FitButton(LBL("New PUPS bank _pN.pal"))) {
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
			ui::HelpMarker(TXT("(a copy of the base .pal; MBAACC ignores PUPS)"));
		}
	}
	ImGui::SetNextItemWidth(ui::Px(90)); ImGui::InputInt(LBL("from palette"), &importFrom, 1, 10); importFrom = std::max(0, importFrom);
	ImGui::SetItemTooltip("%s", TXT("Which palette of a multi-palette .pal to import (e.g. 37 of chr010.pal). Other formats hold one palette."));
	if (FitButton(LBL("Import colours..."))) {
		const std::string path = FileDialog(-1, false);
		if (!path.empty()) { uint32_t in[256]; std::string e; if (!ReadPalFileColors(path, in, &e, importFrom)) status = e; else edit(Fmt(TXT("Import palette %d"), importFrom), [&](uint32_t *p) { for (int i = 0; i < 256; i++) p[i] = (p[i] & 0xFF000000u) | (in[i] & 0xFFFFFFu); }); }
	}
	if (FitButton(LBL("Export colours..."))) {
		char nm[128]; snprintf(nm, sizeof(nm), "palette_%d.act", P.number);
		const std::string path = FileDialog(-1, true, nm);
		if (!path.empty()) { std::string e; status = WritePalFileColors(path, colors, &e, palLayout ? PalLayout::Uni : PalLayout::Mbaacc) ? Fmt(TXT("Saved %s"), path.c_str()) : e; }
	}
	ImGui::SetItemTooltip("%s", TXT("The file extension picks the format: .act, .gpl (GIMP), .png (16x16 strip) or .pal."));
	{
		const char *lay[] = {".pal: MBAACC layout", ".pal: UNI2/MBTL layout"};
		const float lw = std::max(ImGui::CalcTextSize(TXT(lay[0])).x, ImGui::CalcTextSize(TXT(lay[1])).x) + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2;
		i18n::SameLineFit(lw); ImGui::SetNextItemWidth(lw); i18n::Combo("##pallayout", &palLayout, lay, 2);
	}

	// ---- MBAA:CC custom palettes for PovertyCaster / CCCaster ----
	if (!g_scriptPress.empty() && isFile && P.set[P.bank].count > 0) ImGui::SetNextItemOpen(true);   // a scripted press opens the section
	if (isFile && P.set[P.bank].count > 0 && ImGui::TreeNode(LBL("Export for PovertyCaster (MBAA:CC custom palettes)"))) {
		const PalSet &ps = P.set[P.bank];
		const std::string stem = std::filesystem::path(ch.cg.palettePath(P.bank)).stem().string();
		if (ccChara < 0) ccChara = CcCharaFromStem(stem);
		const float fw = ui::Px(70);
		ImGui::SetNextItemWidth(fw); ImGui::InputInt(LBL("Chara id"), &ccChara, 0, 0);
		ImGui::SameLine(); ImGui::TextDisabled(TXT("(%s: %d in PovertyCaster's table)"), stem.c_str(), CcCharaFromStem(stem));
		ImGui::SetNextItemWidth(fw); ImGui::InputInt(LBL("Palettes from"), &ccFrom, 0, 0); ui::SameLineField(fw, LBL("to##cc")); ImGui::SetNextItemWidth(fw); ImGui::InputInt(LBL("to##cc"), &ccTo, 0, 0);
		ccFrom = std::clamp(ccFrom, 0, ps.count - 1); ccTo = std::clamp(ccTo, ccFrom, ps.count - 1);
		ui::SameLineField(fw, LBL("as slots from")); ImGui::SetNextItemWidth(fw); ImGui::InputInt(LBL("as slots from"), &ccFirst, 0, 0); ccFirst = std::clamp(ccFirst, 1, kCcMaxPalettes);
		const int last = std::min(kCcMaxPalettes, ccFirst + ccTo - ccFrom);
		const CcSlotPlace a = CcPlaceOf(ccFirst), b = CcPlaceOf(last);
		ImGui::PushTextWrapPos(0.f); ImGui::TextDisabled(TXT("Slots %d..%d: selector tab %d entry %d to tab %d entry %d (36 per tab; tab 1 is the stock list)."), ccFirst, last, a.bank + 1, a.local + 1, b.bank + 1, b.local + 1); ImGui::PopTextWrapPos();
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
	ImGui::Checkbox(LBL("Only the current palette number"), &P.recolorThisOnly);
	ImGui::SameLine(); ImGui::TextDisabled("(?)"); ImGui::SetItemTooltip("%s", TXT("Uses the hue / saturation / brightness sliders above."));
	ui::SameLineCheckbox(LBL("Palette file(s)")); ImGui::Checkbox(LBL("Palette file(s)"), &P.recolorFiles);
	ui::SameLineCheckbox(LBL("Bank slots")); ImGui::Checkbox(LBL("Bank slots"), &P.recolorSlots);
	ui::SameLineCheckbox(LBL("Sprites' own palettes (types 2, 3, 4)")); ImGui::Checkbox(LBL("Sprites' own palettes (types 2, 3, 4)"), &P.recolorImages);
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
	ui::SameLineField(ui::Px(70), LBL("at id")); ImGui::SetNextItemWidth(ui::Px(70)); ImGui::InputInt(LBL("at id"), &insertAt, 0, 0); ImGui::SetItemTooltip("%s", TXT("Id the new image takes (-1: after the selected image). Later ids move up and every frame reference follows."));
	ImGui::BeginDisabled(!have);
	{
	ui::ButtonGrid g({LBL("Duplicate"), LBL("Make independent"), LBL("Clear (keep id)"), LBL("Delete (renumber)")});
	if (GridButton(g, LBL("Duplicate"))) {
		Bank before = *bank; std::vector<int> remap; int id = -1; std::string e;
		if (!DuplicateImage(*bank, selected, selected + 1, remap, &id, &e)) { *bank = std::move(before); status = e; }
		else if (commitStruct(ch, Fmt(TXT("Duplicate image %d"), selected), std::move(before), remap, host)) { selected = id; status = Fmt(TXT("Duplicated as image %d."), id); }
	}
	if (GridButton(g, LBL("Make independent"))) {
		Bank before = *bank; std::string e;
		if (!UnshareImage(*bank, selected, &e)) { *bank = std::move(before); status = e; } else commitStruct(ch, Fmt(TXT("Make image %d independent"), selected), std::move(before), {}, host);
	}
	ImGui::SetItemTooltip("%s", TXT("Gives the image its own copy of every cell it borrows from another image."));
	if (GridButton(g, LBL("Clear (keep id)"))) { pend = {2, selected, true}; }
	if (GridButton(g, LBL("Delete (renumber)"))) { pend = {1, selected, true}; }
	}
	ImGui::SetNextItemWidth(ui::Px(70)); ImGui::InputInt(LBL("Move to id"), &moveTarget, 0, 0);
	if (FitButton(LBL("Move"))) {
		Bank before = *bank; std::vector<int> remap; std::string e; moveTarget = std::clamp(moveTarget, 0, count - 1);
		if (!MoveImage(*bank, selected, moveTarget, remap, &e)) { *bank = std::move(before); status = e; }
		else if (commitStruct(ch, Fmt(TXT("Move image %d to %d"), selected, moveTarget), std::move(before), remap, host)) { selected = moveTarget; status = TXT("Moved. Every frame reference follows the images."); }
	}
	ImGui::EndDisabled();
	if (CgmButton(LBL("Delete all unused..."))) { pend = {3, -1, true}; if (!users.files.empty() && !diskUsageValid) countProjectUsage(); }
	if (FitButton(LBL("Trim empty atlas pages"))) { Bank before = *bank; if (TrimPages(*bank)) commitStruct(ch, TXT("Trim atlas pages"), std::move(before), {}, host); else { *bank = std::move(before); status = TXT("No empty pages at the end."); } }
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
	ImGui::SetNextItemWidth(std::min(ui::Px(180), std::max(ui::FieldW(), ImGui::GetContentRegionAvail().x - i18n::ButtonWidth(LBL("Rename")) - ui::LabelTextW(LBL("Name##eff")) - ImGui::GetStyle().ItemSpacing.x * 3)));
	ImGui::InputText(LBL("Name##eff"), renameBuf, sizeof renameBuf);
	ui::SameLineButton(LBL("Rename"));
	if (ImGui::Button(LBL("Rename"))) {
		Bank before = *bank; std::string e;
		if (RenameImage(*bank, selected, renameBuf, &e)) { if (commit(ch, Fmt(TXT("Rename image %d"), selected), std::move(before), host)) status = Fmt(TXT("image %d renamed"), selected); }
		else { status = e; *bank = std::move(before); }
	}
	if (!im.drawable()) return;
	// base point: the canvas origin is the anchor the engine places the sprite with; moving the sprite moves it against that point
	ImGui::AlignTextToFramePadding();
	ImGui::Text(TXT("Sprite at (%d,%d) from its base point (canvas %d x %d)"), im.x1, im.y1, im.w, im.h);
	i18n::SameLineFit(ImGui::GetFrameHeight() * 4 + ImGui::GetStyle().ItemSpacing.x * 3);
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
	ui::SameLineCheckbox(LBL("Draw blocks")); ImGui::Checkbox(LBL("Draw blocks"), &blk.show);
	ui::HelpMarker(TXT("Click a cell in the preview to switch it on / off, then Apply. A cell that is switched off is not stored (transparent); a cell without visible pixels is never stored."));
	ui::ButtonGrid bg({LBL("Enable all"), LBL("Disable all"), LBL("Recalculate from alpha"), LBL("Apply blocks")});
	bg.Next(); if (ImGui::Button(LBL("Enable all"), ImVec2(bg.cellW, 0))) { std::fill(blk.mask.begin(), blk.mask.end(), 1); blk.pending = true; }
	bg.Next(); if (ImGui::Button(LBL("Disable all"), ImVec2(bg.cellW, 0))) { std::fill(blk.mask.begin(), blk.mask.end(), 0); blk.pending = true; }
	bg.Next();
	if (ImGui::Button(LBL("Recalculate from alpha"), ImVec2(bg.cellW, 0))) {   // every cell that has a visible pixel, nothing else
		Bank before = *bank; std::string e;
		if (ReblockImage(*bank, selected, blk.px.data(), blk.w, blk.h, nullptr, &e)) commit(ch, Fmt(TXT("Recalculate blocks of image %d"), selected), std::move(before), host);
		else { status = e; *bank = std::move(before); }
		blk.id = -1;
	}
	ImGui::BeginDisabled(!blk.pending);
	bg.Next();
	if (ImGui::Button(LBL("Apply blocks"), ImVec2(bg.cellW, 0))) {
		Bank before = *bank; std::string e;
		if (ReblockImage(*bank, selected, blk.px.data(), blk.w, blk.h, &blk.mask, &e)) commit(ch, Fmt(TXT("Block edit of image %d"), selected), std::move(before), host);
		else { status = e; *bank = std::move(before); }
		blk.id = -1;
	}
	ImGui::EndDisabled();
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
	ui::FitNextWindow(1200, 820);
	if (!ImGui::Begin(LBL("CG manager"), &open)) { ImGui::End(); return; }
	if (!ch || !ch->cg.m_loaded) {
		ImGui::TextDisabled("%s", TXT("The active character has no CG bank."));
		if (ch && fxr.selfTestPending()) fxr.draw(*ch, nullptr, usage, host.navigate);   // --fxr-selftest on a CG-less fixture (ctest fxr_selftest)
		ImGui::End(); return;
	}
	if (owner != ch || cgGen != ch->cg.generation()) rebuild(*ch);
	if (usage.stale(ch->frameData) || (int)usage.byImage.size() < ch->cg.get_image_count()) { usage.build(ch->frameData, ch->cg.get_image_count()); filterKey = ~0ull; }
	const int count = ch->cg.get_image_count();

	const bool editable = bank != nullptr;
	if (editable) {
		const ImGuiIO &io = ImGui::GetIO();
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
		ImGui::BeginDisabled(!hist.canUndo()); if (CgmButton(LBL("Undo")) || (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift)) undoRedo(*ch, false, host); ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::BeginDisabled(!hist.canRedo()); if (CgmButton(LBL("Redo")) || (focused && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))))) undoRedo(*ch, true, host); ImGui::EndDisabled();
		if (hist.canUndo()) { ImGui::SameLine(); ImGui::TextDisabled("%s", hist.topLabel()); }
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
		ImGui::SetItemTooltip("%s", TXT("Writes manifest.json, rgba/*.png, indexed/*.png and palettes/*.pal. Edit the PNGs, then import the folder: only changed images are re-encoded."));
		ui::SameLineCheckbox(LBL("cgtool layout")); ImGui::Checkbox(LBL("cgtool layout"), &cgtoolLayout);
		ImGui::SetItemTooltip("%s", TXT("<name>_ID_<n>.png, canvas-sized, no manifest (what cgtool writes). Uses the preview palette, so any palette of the .pal can be extracted."));
		if (FitButton(LBL("Import folder..."))) {
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
		ImGui::SetItemTooltip("%s", TXT("A folder with manifest.json (Export all), or a plain cgtool export: <name>_ID_<n>.png files, canvas-sized or not. A picture of another size is re-cut and keeps its place on the canvas."));
		if (FitButton(LBL("Sync structure from folder..."))) {
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
		{
			const char *anchors[] = {"Keep bottom centre", "Keep top left"};
			const float aw = std::max(ui::Px(130), std::max(ImGui::CalcTextSize(TXT(anchors[0])).x, ImGui::CalcTextSize(TXT(anchors[1])).x) + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2);
			i18n::SameLineFit(aw); ImGui::SetNextItemWidth(aw); i18n::Combo("##cgmanchor", &resizeAnchor, anchors, 2);
		}
		ImGui::SetItemTooltip("%s", TXT("For a replacement of another size: which point of the old sprite the new one keeps, so the frame offsets stay right. A canvas-sized PNG keeps its own position."));
		if (!ch->getCGPath().empty()) {
			const bool multi = users.projects.size() > 1 || (users.files.size() > 1);
			if (FitButton(multi ? LBL("Save bank, its projects and this character") : LBL("Save to the character's .cg"))) {
				std::string rep; const bool ok = saveBankFiles(*ch, rep);
				if (ok && host.saveCharacter && pendingIdChanges(ch) == false && ch->isModified()) host.saveCharacter(ch);
				status = rep;
			}
			ImGui::SetItemTooltip("%s", TXT("Writes the .cg, rewrites the image ids of every other project file that draws it (other moons, b_ files, MBTL variants) and saves this character, so nothing ships with ids the bank does not have."));
		}
		if (FitButton(LBL("Save bank as..."))) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", "bank.cg");
			const std::string p = FileDialog(-1, true, nm);
			if (!p.empty()) { std::vector<uint8_t> bytes; bank->serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Saved %s"), p.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		if (FitButton(LBL("New bank from PNG folder..."))) {
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
			ui::SameLineButton(LBL("Count usage in all projects"));
			if (ImGui::SmallButton(LBL("Count usage in all projects"))) { countProjectUsage(); filterKey = ~0ull; }
			if (diskUsageValid) { ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("(counted)")); ImGui::SetItemTooltip("%s", TXT("(counted: the Unused filter and Delete all unused use every project)")); }
			if (pendingIdChanges(ch)) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.6f, 0.2f, 1.f)); ImGui::TextWrapped("%s", TXT("Image ids changed: saving the character also saves the bank and rewrites the other projects, so they never ship mismatched ids.")); ImGui::PopStyleColor(); }
		}
		if (bank->cutter2 && !cutter2Note) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.3f, 1.f)); ImGui::TextWrapped("%s", TXT("BMP Cutter2 bank (GAKIHA): pixels sit on pre-baked pages. MBAC itself skips this bank (its 'carries a palette' flag is 0), so edits show here but not in the game.")); ImGui::PopStyleColor(); }
		if (ch->frameData.usesUniFormat()) {   // the long explanation lives in the (?) tooltip
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), "%s", TXT("UNI2 / MBTL: saved files reach the game only through the IM mod folder."));
			ui::HelpMarker(TXT("UNI2 / MBTL read their data from archives (d/, .bin) that are not rewritten: saved .cg / .pat / .pal / .ha6 files reach the game only from the IM mod folder, with the same relative path (UNI2: <game>\\UNI2-IM\\Mods\\data\\...; MBTL: <game>\\MBTL-IM\\Mods\\data\\...), loaded through fpan-im / UNI2-IM / MBTL-IM. The \"Save to mod folder\" buttons write there."));
		}
		if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
		for (const std::string &w : warnings) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", w.c_str());
	} else {
		// every other bank format (strip / tile banks): batch export and import and single-image replace go through the CG object; no undo, no structural edits
		CgIO io(ch->cg);
		if (CgmButton(LBL("Export all..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) { ExportResult er; ExportOptions eo; status = ExportBank(io, d, ch->getName(), eo, er) ? Fmt(TXT("Exported %d images (%d files) to %s"), er.images, er.files, d.c_str()) : er.error; warnings.clear(); }
		}
		if (FitButton(LBL("Import folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				ImportResult ir; warnings.clear();
				if (!ImportBank(io, d, ir)) status = ir.error;
				else { warnings = ir.warnings; if (!ir.changed.empty() && host.markEdited) host.markEdited(ch); status = Fmt(TXT("Imported %zu changed image(s); %d unchanged. This format has no undo here: save the bank to keep it."), ir.changed.size(), ir.skipped); clearThumbs(); previewId = -1; filterKey = ~0ull; cgGen = ch->cg.generation(); }
			}
		}
		ImGui::BeginDisabled(selected < 0 || selected >= count);
		if (FitButton(LBL("Replace selected from PNG..."))) {
			const std::string p = FileDialog(-1, false);
			if (!p.empty()) {
				std::vector<uint8_t> px; int w = 0, h = 0; std::string e; ReplaceReport rep;
				if (!ReadImageRgba(p, px, w, h, e)) status = e;
				else if (!io.replace(selected, px.data(), w, h, &e, &rep, nullptr)) status = e;
				else { status = rep.changedPixels ? Fmt(TXT("Image %d replaced (%d pixels). Save the bank to keep it."), selected, rep.changedPixels) : std::string(TXT("The PNG renders exactly like the stored image: nothing changed.")); if (rep.changedPixels && host.markEdited) host.markEdited(ch); clearThumbs(); previewId = -1; cgGen = ch->cg.generation(); }
			}
		}
		ImGui::EndDisabled();
		if (FitButton(LBL("Save bank as...")) && ch->cg.foreign()) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", "bank.bin");
			const std::string p = FileDialog(-1, true, nm);
			if (!p.empty()) { std::vector<uint8_t> bytes; ch->cg.foreign()->serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Saved %s"), p.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		ImGui::PushTextWrapPos(0.f); ImGui::TextDisabled("%s", TXT("This bank format: browse, batch export / import and image replace. Structural edits and palette editing are for BMP Cutter banks.")); ImGui::PopTextWrapPos();
		if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
		for (const std::string &w : warnings) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", w.c_str());
	}

	if (ImGui::BeginTabBar("##cgmtabs")) {
	if (ImGui::BeginTabItem(LBL("Browser"), nullptr, CgmTab("Browser"))) {
	// ---- filters ----
	// label-first filter cells, as many per row as the window holds; the check boxes wrap on their own row
	static const char *kTypes[] = {"Any type", "Type 0", "Type 1", "Type 2", "Type 3", "Type 4", "Type 5", "Type -1"};
	int tsel = typeFilter == -2 ? 0 : typeFilter == -1 ? 7 : typeFilter + 1;
	if (ui::BeginFormGrid("##cgmfilters", 6, ui::Em(12.f))) {
		ui::FormCell(TXT("Filter")); ImGui::InputTextWithHint("##cgmname", TXT("Filter by id or name"), nameFilter, sizeof(nameFilter));
		ui::FormCell(TXT("Type")); if (i18n::Combo("##cgmtype", &tsel, kTypes, 8)) typeFilter = tsel == 0 ? -2 : tsel == 7 ? -1 : tsel - 1;
		ui::FormCell(TXT("Min side")); ImGui::InputInt("##cgmminside", &minSize, 0, 0);
		ui::FormCell(TXT("Max side")); ImGui::InputInt("##cgmmaxside", &maxSize, 0, 0);
		ui::FormCell(TXT("Used by pattern"));
		ImGui::SetNextItemWidth(std::max(ui::FieldW() * 0.6f, ImGui::GetContentRegionAvail().x - i18n::ButtonWidth(LBL("any##cgmpat")) - ImGui::GetStyle().ItemSpacing.x));
		ImGui::InputInt("##cgmusedpat", &usedByPattern, 0, 0);
		ImGui::SameLine(); if (ImGui::SmallButton(LBL("any##cgmpat"))) usedByPattern = -1;
		ui::FormCell(TXT("Thumbnail")); ImGui::SliderInt("##cgmthumb", &thumbSize, 48, 192);
		ui::EndFormGrid();
	}
	ImGui::Checkbox(LBL("Unused"), &onlyUnused); if (onlyUnused) onlyUsed = false;
	ui::SameLineCheckbox(LBL("Used")); ImGui::Checkbox(LBL("Used"), &onlyUsed); if (onlyUsed) onlyUnused = false;
	ui::SameLineCheckbox(LBL("Shares cells")); ImGui::Checkbox(LBL("Shares cells"), &onlyShared);

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
	ImGui::SameLine(); ImGui::TextDisabled(TXT("%d of %d images shown"), (int)shown.size(), count);

	// ---- grid (left) | inspector (right): a resizable split that fills the tab ----
	const float browseH = ImGui::GetContentRegionAvail().y;
	const bool split = ImGui::BeginTable("##cgmbrowse", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings, ImVec2(0, browseH));
	if (split) {
		ImGui::TableSetupColumn("##grid", ImGuiTableColumnFlags_WidthStretch, 0.58f);
		ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch, 0.42f);
		ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
	}
	const float paneH = std::max(ui::Em(4.f), browseH - ImGui::GetStyle().CellPadding.y * 2);
	ImGui::BeginChild("##cgmgrid", ImVec2(split ? 0.f : ImGui::GetContentRegionAvail().x * 0.58f, paneH), ImGuiChildFlags_Borders);
	{
		const float cell = (float)thumbSize + ui::Px(8.f), availW = ImGui::GetContentRegionAvail().x;
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

	// ---- inspector (right) ----
	if (split) ImGui::TableSetColumnIndex(1); else ImGui::SameLine();
	ImGui::BeginChild("##cgminfo", ImVec2(0, paneH));
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
		if (ui::BeginLabelForm("##cgmview", ui::LabelColW({LBL("Palette"), LBL("PUPS bank"), LBL("Zoom")}))) {
			if (nPal > 1 || ch->cg.pupsBankCount() > 1) {
				if (ImGui::InputInt(ui::FormRow(LBL("Palette")), &previewPal, 1, 8)) { previewPal = std::clamp(previewPal, 0, nPal - 1); clearThumbs(); previewId = -1; }
				if (ch->cg.pupsBankCount() > 1) { if (ImGui::InputInt(ui::FormRow(LBL("PUPS bank")), &previewPups, 1, 1)) { previewPups = std::clamp(previewPups, 0, ch->cg.pupsBankCount() - 1); clearThumbs(); previewId = -1; } }
			}
			ImGui::SliderFloat(ui::FormRow(LBL("Zoom")), &zoom, 0.25f, 8.f);
			ui::EndLabelForm();
		}
		if (previewId != selected || previewGen != cgGen) {
			previewId = selected; previewGen = cgGen;
			std::vector<uint8_t> px; int w = 0, h = 0;
			if (ok && fetch(*ch, selected, previewPal, previewPups, px, w, h)) upload(preview, px, w, h, 0, false); else { preview.w = preview.h = 0; }
		}
		if (bank) drawStructure(*ch, host);
		if (bank) drawEffect(*ch, host);
		ImGui::SeparatorText(TXT("Used by"));
		if (selected < (int)usage.byImage.size() && !usage.byImage[selected].empty()) {
			ImGui::BeginChild("##cgmuse", ImVec2(0, std::min(ui::Em(6.f), ImGui::GetTextLineHeightWithSpacing() * (usage.byImage[selected].size() + 0.6f))), ImGuiChildFlags_Borders);
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
	if (split) ImGui::EndTable();
	ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(LBL("CG + .pat textures"), nullptr, CgmTab("CG + .pat textures"))) { drawTextures(*ch, host); ImGui::EndTabItem(); }
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



// ---------------------------------------------------------------------------------------------------------------------------------------
// "CG + .pat textures" tab: one browser for the CG bank and the character's .pat textures (cgm_texsrc.h does every operation)
// ---------------------------------------------------------------------------------------------------------------------------------------
namespace {
struct GameRow { const char *id, *name, *steamDir; };
const GameRow kGames[] = {
	{"mbaacc", "MBAACC", ""}, {"mbac", "ActCadenza", ""}, {"unib", "UNIB", ""},
	{"uni2", "UNI2", "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes"},
	{"mbtl", "MBTL", "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA"},
	{"rbo", "RBO / GOF2", ""},
};
constexpr int kGameCount = (int)(sizeof(kGames) / sizeof(kGames[0]));
int DetectGame(CharacterInstance &ch) {
	std::string p = ch.getCGPath().empty() ? ch.getTopHA6Path() : ch.getCGPath();
	for (auto &c : p) c = (char)tolower((unsigned char)c);
	switch (ch.frameData.game()) {
	case Ha6Game::MBTL: return 4;
	case Ha6Game::UNIB: return 2;
	case Ha6Game::UNI: case Ha6Game::UNIST: return p.find("unib") != std::string::npos ? 2 : 3;
	default: break;
	}
	if (ch.frameData.isHA4()) return 1;
	if (p.find(".pac") != std::string::npos || p.find("rbo") != std::string::npos || p.find("gof") != std::string::npos) return 5;
	return 0;
}
// a .pat texture's GL copy follows its pixels (the PAT editor and the game view draw from it)
void UploadPatTexture(Parts &parts, int i) {
	PartGfx<> &g = parts.gfxMeta[i];
	std::vector<uint8_t> rgba; int w = 0, h = 0; std::string e;
	if (!pattex::Decode(g, rgba, w, h, &e)) return;
	for (size_t k = 0; k < rgba.size(); k += 4) std::swap(rgba[k], rgba[k + 2]);   // LoadDirect takes BGRA
	Texture *t = g.GetTextureFromId(parts.textures);
	if (!t) { t = new Texture; parts.textures.push_back(t); }
	t->Unapply();   // Apply() skips a texture that is already on the GPU (every loaded .pat texture): drop it so the new pixels upload
	t->LoadDirect((char *)rgba.data(), w, h, true);
	t->Apply(false, false);
	g.textureIndex = t->id;
}
}

// .pat textures / part sheets: the PAT editor's history owns the parts model when its channel is live (French-Bread characters, a
// .pat opened as its own document): the step goes there, so Ctrl+Z in the editor and the han2 pane undo it. Otherwise (UNI / MBAACC
// characters, whose .pat saves on its own) the step goes into this window's history.
static bool PatChannelLive(CharacterInstance &ch) { return ch.partsUndo.active() || (ch.isPatDocument() && ch.patDocumentUndo() && ch.patDocumentUndo()->active()); }

void Window::uploadPatSide(CharacterInstance &ch, const std::vector<int> &textures) {
	if (ch.parts.fbPartModel) han2::UploadPartsTextures(ch.parts);
	else for (int i : textures) if (i >= 0 && i < (int)ch.parts.gfxMeta.size()) UploadPatTexture(ch.parts, i);
	tb.patGen++;
}

void Window::restorePatTex(CharacterInstance &ch, const PatTexSnap &s, const WindowHost &host) {
	uploadPatSide(ch, RestorePatTex(ch.parts, s));
	if (host.markEdited) host.markEdited(&ch);
}

bool Window::texReplace(CharacterInstance &ch, int kind, int id, const std::string &file, bool pngOnly, const WindowHost &host) {
	std::string e; bool changed = false;
	if (pngOnly) { std::string low = file; for (auto &c : low) c = (char)tolower((unsigned char)c); if (low.size() < 4 || low.compare(low.size() - 4, 4, ".png")) { status = TXT("Import PNG takes a .png file."); return false; } }
	if (kind == 1 || kind == 2) {
		PatTexSource patSrc(ch.parts); SheetTexSource sheetSrc(ch.parts);
		TexSource &src = kind == 2 ? (TexSource &)sheetSrc : (TexSource &)patSrc;
		const bool live = PatChannelLive(ch);
		if (live) ch.undoManager.flush();   // an earlier pending edit stays its own step
		PatTexSnap before = CapturePatTex(ch.parts);
		if (!ImportPng(src, id, file, &e, &changed)) { RestorePatTex(ch.parts, before); status = e; return false; }
		if (!changed) { status = TXT("The PNG renders exactly like the stored texture: nothing changed."); return true; }
		const std::vector<int> &touched = kind == 2 ? sheetSrc.touchedParts : patSrc.touchedParts;
		uploadPatSide(ch, {id});
		const std::string label = Fmt(kind == 2 ? TXT("Replace sheet %d") : TXT("Replace .pat texture %d"), id);
		if (host.markEdited) host.markEdited(&ch);
		if (live) ch.undoManager.flush(label.c_str());
		else { HistoryEntry h; h.label = label; h.patBefore = std::move(before); h.patAfter = CapturePatTex(ch.parts); hist.push(std::move(h)); }
		status = Fmt(kind == 2 ? TXT("Sheet %d replaced; %d part(s) that draw from it follow it. Save the character to keep it.") : TXT(".pat texture %d replaced; %d part(s) that sample it follow it. Save the .pat to keep it."), id, (int)touched.size());
		return true;
	}
	if (bank) {
		Bank before = *bank; BmpCutterIO io(*bank); CgTexSource src(io);
		if (!ImportPng(src, id, file, &e, &changed)) { *bank = std::move(before); status = e; return false; }
		if (!changed) { status = TXT("The PNG renders exactly like the stored image: nothing changed."); return true; }
		warnings.clear();
		if (src.lastReport.lostPixels) warnings.push_back(Fmt(TXT("%d changed pixel(s) sit in cells borrowed from another image and were not written."), src.lastReport.lostPixels));
		if (!src.lastReport.alsoChanges.empty()) { std::string o; for (int x : src.lastReport.alsoChanges) o += " " + std::to_string(x); warnings.push_back(std::string(TXT("Shared cells: these images change too:")) + o); }
		if (!commit(ch, Fmt(TXT("Replace image %d"), id), std::move(before), host)) return false;
	} else {
		// a bank format the Bank model does not hold: the step keeps the CG's stored bytes before and after
		std::shared_ptr<std::vector<uint8_t>> raw0;
		if (ch.cg.bank_data() && ch.cg.bank_size()) raw0 = std::make_shared<std::vector<uint8_t>>((const uint8_t *)ch.cg.bank_data(), (const uint8_t *)ch.cg.bank_data() + ch.cg.bank_size());
		CgIO io(ch.cg); CgTexSource src(io);
		if (!ImportPng(src, id, file, &e, &changed)) { status = e; return false; }
		if (!changed) { status = TXT("The PNG renders exactly like the stored image: nothing changed."); return true; }
		if (raw0 && ch.cg.bank_data()) {
			HistoryEntry h; h.label = Fmt(TXT("Replace image %d"), id); h.rawBefore = raw0;
			h.rawAfter = std::make_shared<std::vector<uint8_t>>((const uint8_t *)ch.cg.bank_data(), (const uint8_t *)ch.cg.bank_data() + ch.cg.bank_size());
			hist.push(std::move(h));
		}
		if (host.markEdited) host.markEdited(&ch);
	}
	status = Fmt(TXT("Image %d replaced. Save the bank to keep it."), id);
	return true;
}

std::string Window::modFolderDst(CharacterInstance &ch, const std::string &srcPath, bool *usesModFolder) {
	const int gi = tb.game < 0 ? DetectGame(ch) : tb.game;
	const bool mod = UsesModFolder(kGames[gi].id);
	if (usesModFolder) *usesModFolder = mod;
	if (!mod) return "";
	if (tb.gameDir[gi].empty() && kGames[gi].steamDir[0]) tb.gameDir[gi] = kGames[gi].steamDir;
	return ModFolderPath(kGames[gi].id, tb.gameDir[gi], GameKeyOf(srcPath));
}

// One undo step per mod-folder write: the file as it was before (none = null) and the bytes written.
bool Window::writeModStep(const std::string &dst, const std::vector<uint8_t> &bytes, const std::string &label, std::string *err) {
	std::shared_ptr<std::vector<uint8_t>> before;
	{ std::ifstream f(std::filesystem::u8path(dst), std::ios::binary); if (f) before = std::make_shared<std::vector<uint8_t>>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); }
	if (!WriteModFile(dst, bytes, err)) return false;
	HistoryEntry h; h.label = label; h.modDst = dst; h.modBefore = before; h.modAfter = std::make_shared<std::vector<uint8_t>>(bytes);
	hist.push(std::move(h));
	return true;
}

bool Window::texSave(CharacterInstance &ch, int kind, const std::string &game, std::string &report, const WindowHost &host) {
	if (kind == 2) {   // RBO / GOF2 sheets live in the character's PAT block: the han2 writers save the .DAT / .DT2 companion with it
		if (!host.saveCharacter) { report = TXT("Save the character (File > Save) to write the sheets."); return false; }
		if (!host.saveCharacter(&ch)) { report = TXT("The character could not be saved."); return false; }
		report = Fmt(TXT("Saved %s (sheets written by the han2 writers)."), ch.getTopHA6Path().c_str());
		return true;
	}
	const int gi = tb.game < 0 ? DetectGame(ch) : tb.game;
	if (UsesModFolder(game)) {
		const std::string srcPath = kind == 0 ? ch.getCGPath() : ch.getPATPath();
		const std::string key = GameKeyOf(srcPath), dst = ModFolderPath(game, tb.gameDir[gi], key);
		if (dst.empty()) { report = Fmt(TXT("Cannot place %s in the mod folder: no game folder, or the file is not under a data folder."), srcPath.c_str()); return false; }
		std::vector<uint8_t> bytes; std::string e;
		if (kind == 0) {
			if (!bank) { report = TXT("This bank format cannot be written to the mod folder."); return false; }
			bank->serialize(bytes);
		} else {
			const std::string tmp = dst + ".hcpat";
			{ std::error_code ec; std::filesystem::create_directories(std::filesystem::u8path(dst).parent_path(), ec); }
			if (!ch.parts.Save(tmp.c_str())) { report = TXT("The .pat could not be written."); return false; }
			std::ifstream f(std::filesystem::u8path(tmp), std::ios::binary); bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); f.close();
			std::error_code ec; std::filesystem::remove(std::filesystem::u8path(tmp), ec);
		}
		if (!writeModStep(dst, bytes, Fmt(TXT("Save %s to mod folder"), std::filesystem::u8path(dst).filename().u8string().c_str()), &e)) { report = e; return false; }
		report = Fmt(TXT("Wrote %s (%zu bytes). The improvement mod / fpan-im serve it in place of the archived file."), dst.c_str(), bytes.size());
		return true;
	}
	if (kind == 0) return saveBankFiles(ch, report);
	if (!ch.savePAT()) { report = TXT("The .pat could not be written."); return false; }
	report = Fmt(TXT("Saved %s"), ch.getPATPath().c_str());
	return true;
}

void Window::drawTextures(CharacterInstance &ch, const WindowHost &host) {
	const int gi = tb.game < 0 ? DetectGame(ch) : tb.game;
	if (tb.gameDir[gi].empty() && kGames[gi].steamDir[0]) tb.gameDir[gi] = kGames[gi].steamDir;
	const char *names[kGameCount]; for (int i = 0; i < kGameCount; i++) names[i] = kGames[i].name;
	const std::string game = kGames[gi].id;
	const bool modGame = UsesModFolder(game);
	// ---- game / game folder / filter: label-first cells; the folder cell takes the most room, narrow windows stack them ----
	{
		const ImGuiStyle &st = ImGui::GetStyle();
		const int cols = ImGui::GetContentRegionAvail().x >= ui::Em(36.f) ? (modGame ? 3 : 2) : 1;
		if (ImGui::BeginTable("##cgmtbtop", cols, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
			const float wGame = 1.f, wDir = 3.2f, wFilter = 1.3f;
			if (cols > 1) {
				ImGui::TableSetupColumn("##g", ImGuiTableColumnFlags_WidthStretch, wGame);
				if (modGame) ImGui::TableSetupColumn("##d", ImGuiTableColumnFlags_WidthStretch, wDir);
				ImGui::TableSetupColumn("##f", ImGuiTableColumnFlags_WidthStretch, wFilter);
			}
			auto cell = [&](const char *label) {
				ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine(0, st.ItemInnerSpacing.x);
				ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x, ui::FieldW()));
			};
			int g = gi;
			cell(TXT("Game")); if (ImGui::Combo("##cgmtbgame", &g, names, kGameCount)) tb.game = g;
			if (modGame) {
				cell(TXT("Game folder"));
				char buf[512]; snprintf(buf, sizeof(buf), "%s", tb.gameDir[gi].c_str());
				if (ImGui::InputText("##cgmtbdir", buf, sizeof(buf))) tb.gameDir[gi] = buf;
				ImGui::SetItemTooltip("%s", TXT("The archives are read-only: saves go to <game folder>\\UNI2-IM\\Mods or MBTL-IM\\Mods, which the improvement mod and fpan-im's file layer load instead of the archived file."));
			}
			cell(TXT("Filter")); ImGui::InputTextWithHint("##cgmtbfilter", TXT("Filter by id or name"), tb.filter, sizeof(tb.filter));
			ImGui::EndTable();
		}
	}
	std::string f = tb.filter; for (auto &c : f) c = (char)tolower((unsigned char)c);
	auto pass = [&](int id, const std::string &nm) { if (f.empty()) return true; std::string n = nm; for (auto &c : n) c = (char)tolower((unsigned char)c); return n.find(f) != std::string::npos || std::to_string(id) == f; };

	const bool hasPat = ch.parts.loaded && !ch.parts.gfxMeta.empty();
	const bool fbSheets = ch.parts.fbPartModel;   // RBO / GOF2: the parts model's square sprite sheets (third source kind)
	const int patKind = fbSheets ? 2 : 1;
	PatTexSource patOnly(ch.parts); SheetTexSource sheetSrc(ch.parts);
	TexSource &patSrc = fbSheets ? (TexSource &)sheetSrc : (TexSource &)patOnly;
	if (tb.kind == 1 && fbSheets) tb.kind = 2;
	if (tb.kind == 2 && !fbSheets) tb.kind = 1;
	// the selected item (identical operations for both kinds)
	std::unique_ptr<BmpCutterIO> bioOwn; std::unique_ptr<CgIO> cio;
	std::unique_ptr<CgTexSource> cgSrc;
	if (bank) { bioOwn.reset(new BmpCutterIO(*bank)); cgSrc.reset(new CgTexSource(*bioOwn)); }
	else { cio.reset(new CgIO(ch.cg)); cgSrc.reset(new CgTexSource(*cio)); }
	if (tb.kind != 0 && !hasPat) tb.sel = -1;
	TexSource &src = tb.kind != 0 ? patSrc : (TexSource &)*cgSrc;
	TexEntry e;
	if (tb.sel >= 0 && !src.entry(tb.sel, e)) tb.sel = -1;
	const bool haveSel = tb.sel >= 0;

	// ---- lists (left: CG images over .pat textures over "Used by") | the selected item and its preview (right) ----
	// The .pat list is short (a handful of atlases), so it takes only the rows it needs; the CG list gets the rest of the column.
	const ImGuiStyle &st = ImGui::GetStyle();
	const float H = std::max(ui::Em(10.f), ImGui::GetContentRegionAvail().y);
	const float rowH = ImGui::GetTextLineHeightWithSpacing(), chrome = ImGui::GetFrameHeightWithSpacing() + st.WindowPadding.y * 2 + st.ItemSpacing.y;
	const int patRows = hasPat ? patSrc.count() : 1;
	// "Used by" sized to its rows (one row minimum: a single reference or the "not referenced" note; at most a quarter of
	// the column, then it scrolls), so the CG list keeps the rest of the height instead of a fixed share (1280x720 left
	// the CG list ~5 rows).
	std::vector<TexUse> users;
	if (haveSel) users = tb.kind != 0 ? PatTextureUsers(ch.parts, tb.sel, &ch.frameData) : CgImageUsers(ch.frameData, tb.sel);
	const int usedRows = std::max(1, std::min((int)users.size(), 401));
	const float usedMin = rowH + chrome;
	const float usedH = haveSel ? std::clamp(usedRows * rowH + chrome, usedMin, std::max(usedMin, std::min(ui::Em(12.f), H * 0.25f))) : 0.f;
	const float patH = std::clamp(patRows * rowH + chrome, ui::Em(3.f), H * 0.34f);
	const float cgH = std::max(ui::Em(4.f), H - patH - usedH - st.ItemSpacing.y * (haveSel ? 2.f : 1.f) - st.CellPadding.y * 2);
	const bool split = ImGui::BeginTable("##cgmtbsplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings, ImVec2(0, H));
	if (split) {
		ImGui::TableSetupColumn("##lists", ImGuiTableColumnFlags_WidthStretch, 0.46f);
		ImGui::TableSetupColumn("##item", ImGuiTableColumnFlags_WidthStretch, 0.54f);
		ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
	}
	const float colW = split ? 0.f : ImGui::GetContentRegionAvail().x * 0.46f;
	if (!split) ImGui::BeginGroup();
	// ---- CG images ----
	ImGui::BeginChild("##cgmtbcg", ImVec2(colW, cgH), ImGuiChildFlags_Borders);
	ImGui::SeparatorText(TXT("CG images"));
	{
		std::vector<int> ids; const int n = ch.cg.get_image_count();
		for (int i = 0; i < n; i++) { int bpp, ty, x1, y1, x2, y2; if (!ch.cg.image_info(i, bpp, ty, x1, y1, x2, y2)) continue; const char *nm = ch.cg.get_filename(i); if (pass(i, nm ? nm : "")) ids.push_back(i); }
		ImGuiListClipper clip; clip.Begin((int)ids.size());
		while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
			const int i = ids[r]; int bpp, ty, x1, y1, x2, y2; ch.cg.image_info(i, bpp, ty, x1, y1, x2, y2);
			const char *nm = ch.cg.get_filename(i);
			char l[128]; snprintf(l, sizeof(l), "%4d  %-24s %4dx%-4d  x%d##cgi%d", i, nm ? nm : "", x2 - x1 + 1, y2 - y1 + 1, usage.count(i), i);
			if (ImGui::Selectable(l, tb.kind == 0 && tb.sel == i)) { tb.kind = 0; tb.sel = i; }
		}
	}
	ImGui::EndChild();
	// ---- .pat textures ----
	ImGui::BeginChild("##cgmtbpat", ImVec2(colW, patH), ImGuiChildFlags_Borders);
	ImGui::SeparatorText(fbSheets ? TXT("Part sheets (RBO / GOF2)") : TXT(".pat textures"));
	if (!hasPat) ImGui::TextDisabled("%s", TXT("This character has no .pat textures loaded."));
	else for (int i = 0; i < patSrc.count(); i++) {
		TexEntry pe; if (!patSrc.entry(i, pe) || !pass(i, pe.name)) continue;
		const auto users = PatTextureUsers(ch.parts, i, nullptr);
		int parts = 0; for (auto &u : users) parts += u.partSet < 0;
		char l[160]; snprintf(l, sizeof(l), "%3d  %-24s %4dx%-4d %-11s %d part(s)##pti%d", i, pe.name.c_str(), pe.w, pe.h, pe.format.c_str(), parts, i);
		if (ImGui::Selectable(l, tb.kind == patKind && tb.sel == i)) { tb.kind = patKind; tb.sel = i; }
	}
	ImGui::EndChild();
	// ---- references of the selected item ----
	if (haveSel) {
		ImGui::BeginChild("##cgmtbrefs", ImVec2(colW, 0), ImGuiChildFlags_Borders);
		ImGui::SeparatorText(TXT("Used by"));
		if (users.empty()) ImGui::TextDisabled("%s", TXT("Not referenced by any frame or part of this character."));
		int k = 0;
		for (const TexUse &u : users) {
			if (++k > 400) { ImGui::TextDisabled(TXT("... %d more"), (int)users.size() - 400); break; }
			char l[128];
			if (u.pattern >= 0 && u.partSet >= 0) snprintf(l, sizeof(l), TXT("pattern %d, frame %d, layer %d (part set %d)"), u.pattern, u.frame, u.layer, u.partSet);
			else if (u.pattern >= 0) snprintf(l, sizeof(l), TXT("pattern %d, frame %d, layer %d"), u.pattern, u.frame, u.layer);
			else if (u.partSet >= 0) snprintf(l, sizeof(l), fbSheets ? TXT("pose %d places part %d") : TXT("part set %d places part %d"), u.partSet, u.part);
			else snprintf(l, sizeof(l), TXT("part %d samples it"), u.part);
			ImGui::PushID(k); const bool hit = ImGui::Selectable(l); ImGui::PopID();
			if (hit && u.pattern >= 0 && host.navigate) host.navigate(u.pattern, u.frame);
		}
		ImGui::EndChild();
	}
	if (split) ImGui::TableSetColumnIndex(1); else { ImGui::EndGroup(); ImGui::SameLine(); }

	// ---- the selected item: header, actions, zoom, preview filling the column ----
	ImGui::BeginGroup();
	{   // the tab's own undo / redo: every replace and import here is one step (the PAT editor's history for French-Bread parts)
		const bool live = PatChannelLive(ch);
		const bool canU = hist.canUndo() && (bank || !hist.top().hasBank), canR = hist.canRedo() && (bank || !hist.topRedo().hasBank);
		ImGui::BeginDisabled(!canU); if (CgmButton(LBL("Undo##cgmtb"))) undoRedo(ch, false, host); ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::BeginDisabled(!canR); if (CgmButton(LBL("Redo##cgmtb"))) undoRedo(ch, true, host); ImGui::EndDisabled();
		if (hist.canUndo()) { ImGui::SameLine(); ImGui::TextDisabled("%s", hist.topLabel()); }   // re-read: the press above may have emptied it
		if (live && tb.kind != 0) { ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("(sheet / .pat steps: Edit > Undo, Ctrl+Z in the editor)")); }
	}
	if (!haveSel) ImGui::TextDisabled("%s", TXT("Select a CG image or a .pat texture."));
	else {
		ImGui::PushTextWrapPos(0.f);
		ImGui::Text(tb.kind == 2 ? TXT("Part sheet %d: %s, %d x %d, %s") : tb.kind ? TXT(".pat texture %d: %s, %d x %d, %s") : TXT("CG image %d: %s, %d x %d, %s"), tb.sel, e.name.c_str(), e.w, e.h, e.format.c_str());
		ImGui::PopTextWrapPos();
		const std::string def = std::to_string(tb.sel) + "_" + (e.name.empty() ? std::string(tb.kind ? "texture" : "image") : std::filesystem::path(e.name).stem().string()) + ".png";
		const char *saveLbl = modGame ? LBL("Save to mod folder##cgmtb") : LBL("Save to game##cgmtb");
		ui::ButtonGrid g = modGame ? ui::ButtonGrid({LBL("Export PNG...##cgmtb"), LBL("Import PNG...##cgmtb"), LBL("Replace...##cgmtb"), saveLbl, LBL("Restore mod folder##cgmtb")})
		                           : ui::ButtonGrid({LBL("Export PNG...##cgmtb"), LBL("Import PNG...##cgmtb"), LBL("Replace...##cgmtb"), saveLbl});
		ImGui::BeginDisabled(!e.present);
		if (GridButton(g, LBL("Export PNG...##cgmtb"))) {
			std::string name = def; const std::string p = FileDialog(fileType::IMAGESAVE, true, name.data());
			if (!p.empty()) { std::string er; status = ExportPng(src, tb.sel, p, &er) ? Fmt(TXT("Exported %s"), p.c_str()) : er; if (status == er) {} else tb.lastPng = p; }
		}
		if (GridButton(g, LBL("Import PNG...##cgmtb"))) { const std::string p = FileDialog(fileType::IMAGE); if (!p.empty()) texReplace(ch, tb.kind, tb.sel, p, true, host); }
		if (GridButton(g, LBL("Replace...##cgmtb"))) { const std::string p = FileDialog(fileType::IMAGE); if (!p.empty()) texReplace(ch, tb.kind, tb.sel, p, false, host); }
		ImGui::SetItemTooltip("%s", TXT("Replace the pixels from any image file (PNG, BMP, JPEG, TIFF) of the same size. Pixels that already render identically keep their stored bytes."));
		ImGui::EndDisabled();
		if (GridButton(g, saveLbl)) { std::string rep; texSave(ch, tb.kind, game, rep, host); status = rep; }
		if (modGame) {
			if (GridButton(g, LBL("Restore mod folder##cgmtb"))) {
				const std::string dst = ModFolderPath(game, tb.gameDir[gi], GameKeyOf(tb.kind == 0 ? ch.getCGPath() : ch.getPATPath()));
				std::string er; status = !dst.empty() && RestoreModFile(dst, &er) ? Fmt(TXT("Restored %s"), dst.c_str()) : er;
			}
		}
		if (ui::BeginLabelForm("##cgmtbzoom", ui::LabelColW({LBL("Zoom##cgmtb")}))) { ImGui::SliderFloat(ui::FormRow(LBL("Zoom##cgmtb")), &tb.zoom, 0.125f, 8.f); ui::EndLabelForm(); }
		// view
		ImGui::BeginChild("##cgmtbview", ImVec2(0, std::max(ui::Em(6.f), ImGui::GetContentRegionAvail().y - (split ? st.CellPadding.y : 0.f))), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
		// .pat side: also keyed on the record's pixel pointers, so an undo made in the PAT editor / han2 pane refreshes the view
		unsigned long long gen = tb.kind ? tb.patGen : cgGen;
		if (tb.kind && tb.sel < (int)ch.parts.gfxMeta.size()) gen ^= (unsigned long long)(uintptr_t)ch.parts.gfxMeta[tb.sel].data * 31 ^ (unsigned long long)(uintptr_t)ch.parts.gfxMeta[tb.sel].imageData;
		if (tb.viewKind != tb.kind || tb.viewId != tb.sel || tb.viewGen != gen) {
			tb.viewKind = tb.kind; tb.viewId = tb.sel; tb.viewGen = gen;
			Rgba px; if (e.present && src.decode(tb.sel, px) && px.w > 0) upload(tb.view, px.px, px.w, px.h, 0, false); else tb.view.w = tb.view.h = 0;
		}
		if (tb.view.w > 0) {
			const ImVec2 p0 = ImGui::GetCursorScreenPos(), sz(tb.view.w * tb.zoom, tb.view.h * tb.zoom);
			ImGui::Dummy(sz);
			Checker(ImGui::GetWindowDrawList(), p0, ImVec2(p0.x + sz.x, p0.y + sz.y));
			ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tb.view.tex, p0, ImVec2(p0.x + sz.x, p0.y + sz.y));
		} else ImGui::TextDisabled("%s", TXT("Nothing to show."));
		ImGui::EndChild();
	}
	ImGui::EndGroup();
	if (split) ImGui::EndTable();
}

bool Window::script(const std::string &cmd, const std::string &arg, std::string *msg) {
	auto num = [&](int &v) { v = atoi(arg.c_str() + arg.find(' ') + 1); return true; };
	const std::string key = arg.substr(0, arg.find(' '));
	if (cmd == "open") { open = arg != "off"; return true; }
	if (cmd == "tab") { g_scriptTab = arg; return true; }
	if (cmd == "press") { g_scriptPress = arg; return true; }
	if (cmd == "fxr") return fxr.script(arg, msg);   // Effect recolor tab: import / rule / pattern / slot / accent / path / load / save / status
	if (cmd == "path") { g_scriptPaths.push_back(arg); return true; }
	if (cmd == "select") { selected = atoi(arg.c_str()); previewId = -1; return true; }
	if (cmd == "texsel") { tb.kind = atoi(arg.c_str()); tb.sel = atoi(arg.c_str() + arg.find(' ') + 1); return true; }   // "texsel <0 CG | 1 .pat> <id>"
	if (cmd == "texgame") { tb.game = atoi(arg.c_str()); return true; }
	if (cmd == "texdir") { const int gi = tb.game < 0 ? 0 : tb.game; tb.gameDir[gi] = arg; return true; }
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
		if (key == "palHover") return num(pp.scriptHover);
		if (key == "palSel") { const bool r = num(pal.selA); pal.selB = pal.selA; return r; }
		if (key == "palPreviewMode") return num(pp.mode);
		if (key == "palPreviewImage") return num(pp.image);
		if (key == "previewPal") return num(previewPal);
		if (msg) *msg = "unknown field " + key; return false;
	}
	if (cmd == "status") { if (msg) *msg = "status: " + status + (g_scriptPress.empty() ? "" : " (press '" + g_scriptPress + "' still pending)"); return false; }
	if (msg) *msg = "unknown cgm command " + cmd;
	return false;
}

} // namespace cgm
