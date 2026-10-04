#include "cgm_window.h"
#include "../cg.h"
#include "../character_instance.h"
#include "../i18n.h"
#include "../filedialog.h"
#include "../png_writer.h"
#include "cgm_ops.h"
#include "cgm_export.h"
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#define TXT(s) ::i18n::Tr(s)
#define LBL(s) ::i18n::Label(s)

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
	Bank after = *bank;
	if (!applyModel(ch, host)) { *bank = std::move(before); return false; }
	hist.push(label, std::move(before), std::move(after));
	return true;
}

void Window::undoRedo(CharacterInstance &ch, bool redo, const WindowHost &host) {
	if (!bank || (redo ? !hist.canRedo() : !hist.canUndo())) return;
	HistoryEntry e = redo ? hist.redo() : hist.undo();
	*bank = redo ? e.after : e.before;
	applyModel(ch, host);
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

void Window::draw(CharacterInstance *ch, const WindowHost &host) {
	if (!open) return;
	ImGui::SetNextWindowSize(ImVec2(1000, 640), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("CG manager"), &open)) { ImGui::End(); return; }
	if (!ch || !ch->cg.m_loaded) { ImGui::TextDisabled("%s", TXT("The active character has no CG bank.")); ImGui::End(); return; }
	if (owner != ch || cgGen != ch->cg.generation()) rebuild(*ch);
	if (usage.stale(ch->frameData) || (int)usage.byImage.size() < ch->cg.get_image_count()) { usage.build(ch->frameData, ch->cg.get_image_count()); filterKey = ~0ull; }
	const int count = ch->cg.get_image_count();

	// ---- filters ----
	ImGui::SetNextItemWidth(160); ImGui::InputTextWithHint("##cgmname", TXT("Filter by id or name"), nameFilter, sizeof(nameFilter));
	ImGui::SameLine(); ImGui::Checkbox(LBL("Unused"), &onlyUnused); if (onlyUnused) onlyUsed = false;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Used"), &onlyUsed); if (onlyUsed) onlyUnused = false;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Shares cells"), &onlyShared);
	static const char *kTypes[] = {"Any type", "Type 0", "Type 1", "Type 2", "Type 3", "Type 4", "Type -1"};
	int tsel = typeFilter == -2 ? 0 : typeFilter == -1 ? 6 : typeFilter + 1;
	ImGui::SetNextItemWidth(100); if (i18n::Combo("##cgmtype", &tsel, kTypes, 7)) typeFilter = tsel == 0 ? -2 : tsel == 6 ? -1 : tsel - 1;
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Min side"), &minSize, 0, 0);
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Max side"), &maxSize, 0, 0);
	ImGui::SetNextItemWidth(70); ImGui::InputInt(LBL("Used by pattern"), &usedByPattern, 0, 0);
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("any##cgmpat"))) usedByPattern = -1;
	ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::SliderInt(LBL("Thumbnail"), &thumbSize, 48, 192);

	const bool editable = bank != nullptr;
	if (editable) {
		const ImGuiIO &io = ImGui::GetIO();
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
		ImGui::BeginDisabled(!hist.canUndo()); if (ImGui::Button(LBL("Undo")) || (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift)) undoRedo(*ch, false, host); ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::BeginDisabled(!hist.canRedo()); if (ImGui::Button(LBL("Redo")) || (focused && io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))))) undoRedo(*ch, true, host); ImGui::EndDisabled();
		if (hist.canUndo()) { ImGui::SameLine(); ImGui::TextDisabled("%s", hist.top().label.c_str()); }
		if (ImGui::Button(LBL("Export all..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				ExportResult er; ExportOptions eo; const unsigned *pp = ch->cg.paletteAt(previewPal, previewPups);
				const std::string nm = ch->getName();
				status = ExportBank(*bank, d, nm, eo, er, (bank->images.size() && pp) ? pp : nullptr) ? Fmt(TXT("Exported %d images (%d files) to %s"), er.images, er.files, d.c_str()) : er.error;
				warnings.clear();
			}
		}
		ImGui::SameLine(); ImGui::SetItemTooltip("%s", TXT("Writes manifest.json, rgba/*.png, indexed/*.png and palettes/*.pal. Edit the PNGs, then import the folder: only changed images are re-encoded."));
		ImGui::SameLine();
		if (ImGui::Button(LBL("Import folder..."))) {
			const std::string d = BrowseForFolderUtf8("");
			if (!d.empty()) {
				Bank before = *bank; ImportResult ir; warnings.clear();
				if (!ImportBank(*bank, d, ir)) { *bank = std::move(before); status = ir.error; }
				else {
					warnings = ir.warnings;
					if (ir.changed.empty()) { *bank = std::move(before); status = Fmt(TXT("Nothing changed (%d images unchanged)."), ir.skipped); }
					else if (commit(*ch, Fmt(TXT("Import folder (%zu images)"), ir.changed.size()), std::move(before), host)) status = Fmt(TXT("Imported %zu changed image(s); %d unchanged. Save the bank to keep it."), ir.changed.size(), ir.skipped);
				}
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(selected < 0 || selected >= count);
		if (ImGui::Button(LBL("Replace selected from PNG..."))) {
			const std::string p = FileDialog(-1, false);
			if (!p.empty()) {
				std::vector<uint8_t> px; int w = 0, h = 0; std::string e; ReplaceReport rep; Bank before = *bank;
				if (!ReadImageRgba(p, px, w, h, e)) status = e;
				else if (!ReplaceImage(*bank, selected, px.data(), w, h, &e, &rep)) { *bank = std::move(before); status = e; }
				else if (!rep.changedPixels && !rep.paletteChanged) { *bank = std::move(before); status = TXT("The PNG renders exactly like the stored image: nothing changed."); }
				else {
					warnings.clear();
					if (rep.lostPixels) warnings.push_back(Fmt(TXT("%d changed pixel(s) sit in cells borrowed from another image and were not written."), rep.lostPixels));
					if (!rep.alsoChanges.empty()) { std::string o; for (int x : rep.alsoChanges) o += " " + std::to_string(x); warnings.push_back(std::string(TXT("Shared cells: these images change too:")) + o); }
					if (commit(*ch, Fmt(TXT("Replace image %d"), selected), std::move(before), host)) status = Fmt(TXT("Image %d replaced (%d pixels). Save the bank to keep it."), selected, rep.changedPixels);
				}
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(LBL("Save bank as..."))) {
			char nm[128]; snprintf(nm, sizeof(nm), "%s", "bank.cg");
			const std::string p = FileDialog(-1, true, nm);
			if (!p.empty()) { std::vector<uint8_t> bytes; bank->serialize(bytes); std::ofstream f(std::filesystem::u8path(p), std::ios::binary); f.write((const char *)bytes.data(), (std::streamsize)bytes.size()); status = f ? Fmt(TXT("Saved %s"), p.c_str()) : std::string(TXT("Could not write the file.")); }
		}
		if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
		for (const std::string &w : warnings) ImGui::TextColored(ImVec4(1.f, 0.75f, 0.3f, 1.f), "%s", w.c_str());
	} else ImGui::TextDisabled("%s", TXT("This bank format is browse-only for now."));

	uint64_t key = 1469598103934665603ull;
	auto mix = [&](uint64_t v) { key ^= v + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2); };
	for (const char *c = nameFilter; *c; c++) mix((uint8_t)*c);
	mix(onlyUnused); mix(onlyUsed); mix(onlyShared); mix((uint64_t)(typeFilter + 5)); mix((uint64_t)minSize); mix((uint64_t)maxSize); mix((uint64_t)(usedByPattern + 5)); mix(usage.version); mix(cgGen); mix((uint64_t)count);
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
			const int uc = usage.count(i);
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
		}
		ImGui::EndChild();
	} else ImGui::TextDisabled("%s", TXT("Click an image in the grid."));
	ImGui::EndChild();
	ImGui::End();
}

} // namespace cgm
