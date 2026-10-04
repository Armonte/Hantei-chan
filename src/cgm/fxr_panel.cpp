#include "fxr_panel.h"
#include "../cg.h"
#include "../character_instance.h"
#include "../i18n.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <imgui.h>

#define TXT(s) ::i18n::Tr(s)
#define LBL(s) ::i18n::Label(s)

namespace cgm {

static std::string Fmt(const char *fmt, ...) {
	char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); return buf;
}
static bool ReadText(const std::string &p, std::string &out) {
	std::ifstream f(std::filesystem::u8path(p), std::ios::binary); if (!f) return false;
	out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); return true;
}
static ImVec4 V4(fx::Rgb c) { return ImVec4(c.r, c.g, c.b, 1.f); }

FxPanel::~FxPanel() { freeTex(); }
void FxPanel::freeTex() { for (auto &t : tex) if (t.second.id) glDeleteTextures(1, &t.second.id); tex.clear(); }

std::string FxPanel::defaultPath(CharacterInstance &ch) const {
	namespace fs = std::filesystem;
	std::string pp = ch.cg.palettePath(0);
	std::string stem = ch.getName();
	for (char &c : stem) c = (char)tolower((unsigned char)c);
	fs::path dir = pp.empty() ? fs::path(".") : fs::u8path(pp).parent_path();
	if (!pp.empty()) { stem = fs::u8path(pp).stem().string(); for (char &c : stem) c = (char)tolower((unsigned char)c); }
	if (dir.filename() == "data") dir = dir.parent_path();   // <game>\data\<char>.pal -> <game>\fxrecolor\<char>.ini
	return (dir / "fxrecolor" / (stem + ".ini")).u8string();
}

const unsigned *FxPanel::slotPal(CharacterInstance &ch, int slot) const { return ch.cg.paletteAt(slot, 0); }

void FxPanel::reset(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage) {
	owner = &ch; gen = ch.cg.generation(); usageVersion = usage.version;
	freeTex();
	cls = FxClassification();
	if (bank) ClassifyEffects(*bank, usage, ch.frameData, cls);
	nSlots = 0; while (nSlots < 64 && ch.cg.paletteAt(nSlots, 0)) nSlots++;
	if (path.empty()) path = defaultPath(ch);
}

FxPanel::Tex &FxPanel::preview(CharacterInstance &ch, const Bank &bank, int image, const fx::Rule *rule, int slot) {
	std::string key = std::to_string(image) + "|" + std::to_string(slot) + "|" + std::to_string(gen) + "|" + (rule ? SaveRules([&] { fx::CharRules c; c.rules.push_back(*rule); return c; }()) : "-");
	auto it = tex.find(key);
	if (it != tex.end()) return it->second;
	if (tex.size() > 400) freeTex();
	Tex &t = tex[key];
	Rgba px;
	if (RecolorImage(bank, image, rule, slot, slotPal(ch, slot), px) && px.w > 0 && px.h > 0) {
		glGenTextures(1, &t.id);
		glBindTexture(GL_TEXTURE_2D, t.id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, px.w, px.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.px.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		t.w = px.w; t.h = px.h;
	}
	return t;
}

static void Checker(ImDrawList *dl, ImVec2 a, ImVec2 b) {
	dl->AddRectFilled(a, b, IM_COL32(58, 58, 62, 255));
	const float s = 8.f;
	for (float y = a.y, r = 0; y < b.y; y += s, r++) for (float x = a.x + (((int)r & 1) ? s : 0); x < b.x; x += 2 * s)
		dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + s, b.x), std::min(y + s, b.y)), IM_COL32(78, 78, 84, 255));
}
static void DrawTex(const FxPanel *, unsigned id, int w, int h, float box) {
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(box, box));
	ImDrawList *dl = ImGui::GetWindowDrawList();
	Checker(dl, p0, ImVec2(p0.x + box, p0.y + box));
	if (!id || w <= 0) return;
	const float s = std::min(box / w, box / h); const float dw = w * s, dh = h * s;
	const ImVec2 a(p0.x + (box - dw) / 2, p0.y + (box - dh) / 2);
	dl->AddImage((ImTextureID)(intptr_t)id, a, ImVec2(a.x + dw, a.y + dh));
}

void FxPanel::drawStops(std::vector<fx::Stop> &stops, fx::Rgb accent, const char *idp) {
	// gradient bar as the shader sees it (kRampN entries, linear between)
	float r[fx::kRampN][4]; fx::evalRamp(stops, accent, r);
	const ImVec2 p0 = ImGui::GetCursorScreenPos(); const float W = ImGui::GetContentRegionAvail().x - 8, H = 16;
	ImGui::Dummy(ImVec2(W, H));
	ImDrawList *dl = ImGui::GetWindowDrawList();
	for (int k = 0; k + 1 < fx::kRampN; k++) {
		const float x0 = p0.x + W * k / (fx::kRampN - 1), x1 = p0.x + W * (k + 1) / (fx::kRampN - 1);
		dl->AddRectFilledMultiColor(ImVec2(x0, p0.y), ImVec2(x1, p0.y + H),
			ImGui::ColorConvertFloat4ToU32(ImVec4(r[k][0], r[k][1], r[k][2], 1)), ImGui::ColorConvertFloat4ToU32(ImVec4(r[k + 1][0], r[k + 1][1], r[k + 1][2], 1)),
			ImGui::ColorConvertFloat4ToU32(ImVec4(r[k + 1][0], r[k + 1][1], r[k + 1][2], 1)), ImGui::ColorConvertFloat4ToU32(ImVec4(r[k][0], r[k][1], r[k][2], 1)));
	}
	int del = -1;
	static const char *kTok[] = { "colour", "accent", "accent.dark", "accent.light" };
	for (size_t i = 0; i < stops.size(); i++) {
		fx::Stop &s = stops[i];
		ImGui::PushID((int)i);
		int tk = (int)s.tok;
		ImGui::SetNextItemWidth(104); if (ImGui::Combo("##tok", &tk, kTok, 4)) s.tok = (fx::ColTok)tk;
		ImGui::SameLine();
		if (s.tok == fx::ColTok::Literal) { float c[3] = { s.c.r, s.c.g, s.c.b }; if (ImGui::ColorEdit3("##c", c, ImGuiColorEditFlags_NoInputs)) s.c = { c[0], c[1], c[2] }; }
		else { const fx::Rgb rc = fx::resolveStop(s, accent); ImGui::ColorButton("##a", V4(rc), ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20)); }
		ImGui::SameLine(); ImGui::Checkbox("@", &s.hasPos);
		if (s.hasPos) { ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::SliderFloat("##p", &s.pos, 0.f, 1.f, "%.2f"); }
		ImGui::SameLine(); if (ImGui::SmallButton("x") && stops.size() > 1) del = (int)i;
		ImGui::PopID();
	}
	if (del >= 0) stops.erase(stops.begin() + del);
	ImGui::PushID(idp);
	if (ImGui::SmallButton(LBL("+ stop"))) { fx::Stop s; s.tok = fx::ColTok::Accent; stops.insert(stops.end() - (stops.size() > 1 ? 1 : 0), s); }
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Preset: dark-accent-white"))) fx::parseStops("#000000 accent.dark accent accent.light #ffffff", stops);
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Preset: black-accent"))) fx::parseStops("#000000 accent", stops);
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Preset: rainbow"))) { stops.clear(); for (int k = 0; k < fx::kRampN; k++) { fx::Stop s; s.c = fx::hsv2rgb((float)k / fx::kRampN, 1, 1); stops.push_back(s); } }
	ImGui::PopID();
}

void FxPanel::drawRuleEditor(CharacterInstance &ch, const Bank *bank) {
	if (ruleSel < 0 || ruleSel >= (int)rules.rules.size()) { ImGui::TextDisabled("%s", TXT("Select or create a rule.")); return; }
	fx::Rule &r = rules.rules[ruleSel];
	const unsigned *sp = slotPal(ch, slotSel);
	const fx::Rgb acc = AccentFor(r, sp, slotSel);
	char idb[64]; snprintf(idb, sizeof idb, "%s", r.id.c_str());
	ImGui::SetNextItemWidth(160); if (ImGui::InputText(LBL("Rule id"), idb, sizeof idb) && idb[0]) r.id = idb;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Enabled"), &r.enabled);
	int kind = r.kind == fx::Kind::LumRamp ? 0 : r.kind == fx::Kind::Rainbow ? 1 : 2;
	static const char *kKind[] = { "Luminance -> ramp", "Rainbow (hue -> ramp)", "HSV shift" };
	ImGui::SetNextItemWidth(200);
	if (ImGui::Combo(LBL("Mapping"), &kind, kKind, 3)) r.kind = kind == 0 ? fx::Kind::LumRamp : kind == 1 ? fx::Kind::Rainbow : fx::Kind::Hsv;
	int bank_ = r.bank; static const char *kBank[] = { "any", "character bank", "effect.ha6" };
	ImGui::SetNextItemWidth(140); if (ImGui::Combo(LBL("Bank"), &bank_, kBank, 3)) r.bank = bank_;
	ImGui::SameLine(); int bl = r.blend; ImGui::SetNextItemWidth(70); if (ImGui::InputInt(LBL("Blend (-1 any)"), &bl, 0, 0)) r.blend = std::max(-1, bl);
	if (r.kind == fx::Kind::Hsv) {
		ImGui::SliderFloat(LBL("Hue shift"), &r.hueDeg, -180.f, 180.f, "%.0f deg");
		ImGui::SliderFloat(LBL("Saturation x"), &r.sat, 0.f, 2.f); ImGui::SliderFloat(LBL("Value x"), &r.val, 0.f, 2.f);
	} else {
		bool luma = r.byLuma; if (ImGui::Checkbox(LBL("Drive by luma (default: max channel)"), &luma)) r.byLuma = luma;
		if (r.kind == fx::Kind::LumRamp) {
			ImGui::SetNextItemWidth(220); float vr[2] = { r.vmin, r.vmax };
			if (ImGui::DragFloat2(LBL("Source range"), vr, 0.005f, 0.f, 1.f, "%.3f")) { r.vmin = std::clamp(vr[0], 0.f, 0.99f); r.vmax = std::clamp(vr[1], r.vmin + 0.01f, 1.f); }
		} else ImGui::TextDisabled("%s", TXT("Rainbow: entry k of the ramp (8 slots) colours hue k/8; value is kept."));
		ImGui::SeparatorText(TXT("Ramp"));
		drawStops(r.ramp, acc, "rampp");
	}
	// accent
	ImGui::SeparatorText(TXT("Accent (per slot)"));
	std::string ai = fx::joinInts(r.accentIdx); char ab[64]; snprintf(ab, sizeof ab, "%s", ai.c_str());
	ImGui::SetNextItemWidth(160);
	if (ImGui::InputText(LBL("Accent palette indices"), ab, sizeof ab)) r.accentIdx = fx::parseInts(ab);
	ImGui::SetItemTooltip("%s", TXT("Empty = the most vivid colour of the slot's palette. Otherwise the first listed index is the accent."));
	if (ImGui::Button(LBL("Write auto accents of all slots as overrides"))) {
		for (int s = 0; s < nSlots; s++) {
			fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == s) o = &x;
			if (!o) { r.ov.emplace_back(); o = &r.ov.back(); o->slot = s; }
			if (!o->hasAccent) { o->hasAccent = true; uint32_t rp[256]; ToRuntimePalette(slotPal(ch, s), rp); o->accent = fx::autoAccent(rp, r.accentIdx); }
		}
	}
	ImGui::SameLine(); if (ImGui::Button(LBL("Clear all overrides"))) r.ov.clear();
	if (ImGui::BeginTable("##ov", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 150))) {
		ImGui::TableSetupColumn("slot"); ImGui::TableSetupColumn("auto"); ImGui::TableSetupColumn("override"); ImGui::TableSetupColumn("ramp");
		ImGui::TableHeadersRow();
		for (int s = 0; s < nSlots; s++) {
			ImGui::TableNextRow(); ImGui::PushID(s);
			fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == s) o = &x;
			ImGui::TableNextColumn(); if (ImGui::Selectable(Fmt("%d", s).c_str(), slotSel == s, ImGuiSelectableFlags_SpanAllColumns)) slotSel = s;
			ImGui::TableNextColumn(); uint32_t rp[256]; ToRuntimePalette(slotPal(ch, s), rp); ImGui::ColorButton("##auto", V4(fx::autoAccent(rp, r.accentIdx)), ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
			ImGui::TableNextColumn();
			bool has = o && o->hasAccent;
			if (ImGui::Checkbox("##has", &has)) {
				if (!o) { r.ov.emplace_back(); o = &r.ov.back(); o->slot = s; }
				o->hasAccent = has; if (has) { uint32_t q[256]; ToRuntimePalette(slotPal(ch, s), q); o->accent = fx::autoAccent(q, r.accentIdx); }
			}
			if (has) { ImGui::SameLine(); float c[3] = { o->accent.r, o->accent.g, o->accent.b }; if (ImGui::ColorEdit3("##ac", c, ImGuiColorEditFlags_NoInputs)) o->accent = { c[0], c[1], c[2] }; }
			ImGui::TableNextColumn();
			bool hr = o && o->hasRamp;
			if (ImGui::Checkbox("##hr", &hr)) {
				if (!o) { r.ov.emplace_back(); o = &r.ov.back(); o->slot = s; }
				o->hasRamp = hr; if (hr) o->ramp = r.ramp; rampSlotEdit = hr ? s : -1;
			}
			if (hr) { ImGui::SameLine(); if (ImGui::SmallButton("edit")) rampSlotEdit = s; }
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (rampSlotEdit >= 0) {
		fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == rampSlotEdit) o = &x;
		if (o && o->hasRamp) { ImGui::Text("%s %d", TXT("Whole-ramp override, slot"), rampSlotEdit); drawStops(o->ramp, AccentFor(r, slotPal(ch, rampSlotEdit), rampSlotEdit), "ovramp"); }
	}
	(void)bank;
}

void FxPanel::drawSlotStrip(CharacterInstance &ch, const Bank &bank) {
	const FxPatternRow *row = cls.find(sel);
	if (!row || row->fixedSprites.empty()) { ImGui::TextDisabled("%s", TXT("Click a pattern in the grid to preview it under every palette slot.")); return; }
	if (spriteSel < 0 || std::find(row->fixedSprites.begin(), row->fixedSprites.end(), spriteSel) == row->fixedSprites.end()) spriteSel = row->fixedSprites[0];
	ImGui::Text("%s %d %s", TXT("Pattern"), row->pattern, row->name.c_str());
	ImGui::SameLine(); ImGui::Checkbox(LBL("Show original too"), &showBefore);
	ImGui::TextUnformatted(TXT("Sprite:")); 
	int shown = 0;
	for (int s : row->fixedSprites) {
		if (shown++ >= 24) { ImGui::SameLine(); ImGui::TextDisabled("+%zu", row->fixedSprites.size() - 24); break; }
		ImGui::SameLine(); if (ImGui::RadioButton(Fmt("%d##sp", s).c_str(), spriteSel == s)) spriteSel = s;
	}
	// big: original vs recolour at the chosen slot
	const fx::Rule *rule = RuleFor(rules, row->pattern, spriteSel, slotSel, 1);
	if (!rule) rule = RuleFor(rules, row->pattern, spriteSel, slotSel, 2);
	const float box = 150.f;
	if (showBefore) { Tex &t = preview(ch, bank, spriteSel, nullptr, slotSel); DrawTex(this, t.id, t.w, t.h, box); ImGui::SameLine(); }
	{ Tex &t = preview(ch, bank, spriteSel, rule, slotSel); DrawTex(this, t.id, t.w, t.h, box); }
	ImGui::SameLine(); ImGui::BeginGroup();
	ImGui::Text("%s: %s", TXT("rule"), rule ? rule->id.c_str() : TXT("(none, stays as authored)"));
	ImGui::Text("%s %d", TXT("slot"), slotSel);
	{ const uint32_t *pp = slotPal(ch, slotSel); if (rule) { const fx::Rgb a = AccentFor(*rule, pp, slotSel); ImGui::ColorButton("##acc", V4(a), 0, ImVec2(20, 20)); ImGui::SameLine(); ImGui::TextUnformatted(fx::hexOf(a).c_str()); } }
	ImGui::EndGroup();
	// every slot
	const float tile = 56.f;
	const float W = ImGui::GetContentRegionAvail().x;
	const int perRow = std::max(1, (int)(W / (tile + 6)));
	for (int s = 0; s < nSlots; s++) {
		const fx::Rule *rs = RuleFor(rules, row->pattern, spriteSel, s, 1);
		if (!rs) rs = RuleFor(rules, row->pattern, spriteSel, s, 2);
		Tex &t = preview(ch, bank, spriteSel, rs, s);
		ImGui::PushID(s);
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		DrawTex(this, t.id, t.w, t.h, tile);
		if (ImGui::IsItemClicked()) slotSel = s;
		if (slotSel == s) ImGui::GetWindowDrawList()->AddRect(p0, ImVec2(p0.x + tile, p0.y + tile), IM_COL32(255, 200, 40, 255), 0, 0, 2.f);
		ImGui::SetItemTooltip("%s %d", TXT("slot"), s);
		ImGui::PopID();
		if ((s + 1) % perRow) ImGui::SameLine(0, 6);
	}
	ImGui::NewLine();
}

void FxPanel::draw(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage, const std::function<void(int, int)> &navigate) {
	if (owner != &ch || gen != ch.cg.generation() || usageVersion != usage.version) reset(ch, bank, usage);
	if (!bank) { ImGui::TextDisabled("%s", TXT("This bank format has no effect classification (not a BMP Cutter bank).")); return; }
	ImGui::TextWrapped("%s", TXT("Authors the PovertyCaster runtime recolour (<game>\\fxrecolor\\<char>.ini). Previews apply the runtime's own shader maths to the original pixels: nothing in the game files changes. Method after DGV's \"Better Akiha\"."));
	// file bar
	{
		char pb[512]; snprintf(pb, sizeof pb, "%s", path.c_str());
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 420);
		if (ImGui::InputText("##fxpath", pb, sizeof pb)) path = pb;
		ImGui::SameLine(); if (ImGui::Button(LBL("Load"))) {
			std::string t;
			if (ReadText(path, t)) { parserMsgs.clear(); const int n = LoadRules(t, rules, parserMsgs); saved = rules; status = Fmt(TXT("Loaded %d rules"), n); ruleSel = rules.rules.empty() ? -1 : 0; freeTex(); }
			else status = TXT("Cannot read that file.");
		}
		ImGui::SameLine(); if (ImGui::Button(LBL("Save"))) {
			std::error_code ec; std::filesystem::create_directories(std::filesystem::u8path(path).parent_path(), ec);
			std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
			if (f) { const std::string t = SaveRules(rules); f.write(t.data(), (std::streamsize)t.size()); status = Fmt(TXT("Saved %zu rules to %s"), rules.rules.size(), path.c_str()); saved = rules; }
			else status = TXT("Cannot write that file.");
		}
		ImGui::SameLine(); if (ImGui::Button(LBL("Import DGV Akiha ruleset"))) {
			parserMsgs.clear(); const int n = LoadRules(DgvAkihaIni(), rules, parserMsgs); status = Fmt(TXT("Imported %d DGV groups (Akiha only: sprite ids are Akiha's)"), n); ruleSel = rules.rules.empty() ? -1 : 0; freeTex();
		}
		ImGui::SameLine(); if (ImGui::Button(LBL("Diff vs file"))) ImGui::OpenPopup("##fxdiff");
		if (ImGui::BeginPopup("##fxdiff")) { ImGui::TextUnformatted(DiffRules(saved, rules).c_str()); ImGui::EndPopup(); }
	}
	if (!status.empty()) ImGui::TextDisabled("%s", status.c_str());
	if (!parserMsgs.empty()) ImGui::TextColored(ImVec4(1, .6f, .3f, 1), "%s", parserMsgs.c_str());
	ImGui::Text("%s: %d | %s: %d | %s: %d | %s: %zu", TXT("fixed-colour sprites"), cls.fixedImages, TXT("drawn"), cls.usedFixedImages, TXT("shared by 2+ patterns"), cls.sharedFixedImages, TXT("effect patterns"), cls.patterns.size());

	const float leftW = 520.f;
	ImGui::BeginChild("##fxl", ImVec2(leftW, 0), ImGuiChildFlags_Borders);
	// rules list
	ImGui::SeparatorText(TXT("Rules (first match wins, file order)"));
	if (ImGui::SmallButton(LBL("New"))) { rules.rules.insert(rules.rules.begin(), DefaultRule(UniqueRuleId(rules, "rule"))); ruleSel = 0; freeTex(); }
	ImGui::SameLine(); ImGui::BeginDisabled(ruleSel < 0);
	if (ImGui::SmallButton(LBL("Duplicate")) && ruleSel >= 0) { fx::Rule d = rules.rules[ruleSel]; d.id = UniqueRuleId(rules, d.id + "_copy"); rules.rules.insert(rules.rules.begin() + ruleSel + 1, d); ruleSel++; }
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Delete")) && ruleSel >= 0) { rules.rules.erase(rules.rules.begin() + ruleSel); ruleSel = std::min(ruleSel, (int)rules.rules.size() - 1); freeTex(); }
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Up")) && ruleSel > 0) { std::swap(rules.rules[ruleSel], rules.rules[ruleSel - 1]); ruleSel--; freeTex(); }
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Down")) && ruleSel >= 0 && ruleSel + 1 < (int)rules.rules.size()) { std::swap(rules.rules[ruleSel], rules.rules[ruleSel + 1]); ruleSel++; freeTex(); }
	ImGui::EndDisabled();
	ImGui::BeginChild("##fxrules", ImVec2(0, 110), ImGuiChildFlags_Borders);
	for (int i = 0; i < (int)rules.rules.size(); i++) {
		const fx::Rule &r = rules.rules[i];
		if (ImGui::Selectable(Fmt("%s%s  [%zu patterns, %zu sprites]##r%d", r.enabled ? "" : "(off) ", r.id.c_str(), r.patterns.size(), r.sprites.size(), i).c_str(), ruleSel == i)) ruleSel = i;
	}
	ImGui::EndChild();
	// grid
	ImGui::SeparatorText(TXT("Effect patterns"));
	ImGui::SetNextItemWidth(110); ImGui::InputText(LBL("Name"), nameFilter, sizeof nameFilter);
	ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::SliderInt(LBL("Min score"), &minScore, 0, 100);
	ImGui::SetItemTooltip("%s", TXT("Share of the pattern's layers that draw fixed-colour sprites. A body pattern with one RGB hair sprite scores low."));
	ImGui::Checkbox(LBL("Shared sprites only"), &onlyShared); ImGui::SameLine(); ImGui::Checkbox(LBL("No rule yet"), &onlyUnruled);
	std::vector<int> shown;
	for (size_t i = 0; i < cls.patterns.size(); i++) {
		const FxPatternRow &p = cls.patterns[i];
		if (p.effectScore < minScore) continue;
		if (nameFilter[0] && p.name.find(nameFilter) == std::string::npos && std::to_string(p.pattern) != nameFilter) continue;
		if (onlyShared && !p.sharedSprites) continue;
		if (onlyUnruled && RuleOfPattern(rules, p, slotSel)) continue;
		shown.push_back((int)i);
	}
	ImGui::Text("%zu / %zu", shown.size(), cls.patterns.size());
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Tick all shown"))) for (int i : shown) if (std::find(picked.begin(), picked.end(), cls.patterns[i].pattern) == picked.end()) picked.push_back(cls.patterns[i].pattern);
	ImGui::SameLine(); if (ImGui::SmallButton(LBL("Untick"))) picked.clear();
	ImGui::SameLine(); ImGui::Text("(%zu)", picked.size());
	if (ImGui::BeginTable("##fxgrid", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y - 62))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22); ImGui::TableSetupColumn("pat", ImGuiTableColumnFlags_WidthFixed, 40); ImGui::TableSetupColumn("name");
		ImGui::TableSetupColumn("fx/all", ImGuiTableColumnFlags_WidthFixed, 56); ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 30); ImGui::TableSetupColumn("spr", ImGuiTableColumnFlags_WidthFixed, 32);
		ImGui::TableSetupColumn("shr", ImGuiTableColumnFlags_WidthFixed, 32); ImGui::TableSetupColumn("rule");
		ImGui::TableHeadersRow();
		ImGuiListClipper clip; clip.Begin((int)shown.size());
		while (clip.Step()) for (int k = clip.DisplayStart; k < clip.DisplayEnd; k++) {
			const FxPatternRow &p = cls.patterns[shown[k]];
			ImGui::TableNextRow(); ImGui::PushID(p.pattern);
			ImGui::TableNextColumn();
			bool on = std::find(picked.begin(), picked.end(), p.pattern) != picked.end();
			if (ImGui::Checkbox("##pk", &on)) { if (on) picked.push_back(p.pattern); else picked.erase(std::remove(picked.begin(), picked.end(), p.pattern), picked.end()); }
			ImGui::TableNextColumn();
			if (ImGui::Selectable(Fmt("%d", p.pattern).c_str(), sel == p.pattern, ImGuiSelectableFlags_SpanAllColumns)) { sel = p.pattern; spriteSel = -1; if (navigate) navigate(p.pattern, 0); }
			ImGui::TableNextColumn(); ImGui::TextUnformatted(p.name.c_str());
			ImGui::TableNextColumn(); ImGui::Text("%d/%d", p.fixedLayers, p.layers);
			ImGui::TableNextColumn(); ImGui::Text("%d", p.effectScore);
			ImGui::TableNextColumn(); ImGui::Text("%zu", p.fixedSprites.size());
			ImGui::TableNextColumn(); if (p.sharedSprites) ImGui::Text("%d", p.sharedSprites);
			ImGui::TableNextColumn();
			int cov = 0; const fx::Rule *rr = RuleOfPattern(rules, p, slotSel, &cov);
			if (rr) ImGui::Text("%s %d/%zu", rr->id.c_str(), cov, p.fixedSprites.size()); else ImGui::TextDisabled("-");
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	// assignment
	{
		ImGui::BeginDisabled(picked.empty() || rules.rules.empty() || ruleSel < 0);
		if (ImGui::Button(LBL("Assign ticked patterns to the selected rule"))) { AssignPatterns(rules, ruleSel, picked); freeTex(); status = Fmt(TXT("Assigned %zu patterns to %s"), picked.size(), rules.rules[ruleSel].id.c_str()); }
		ImGui::EndDisabled();
		ImGui::SameLine(); ImGui::BeginDisabled(picked.empty());
		if (ImGui::Button(LBL("New rule from ticked"))) {
			fx::Rule nr = DefaultRule(UniqueRuleId(rules, "rule")); rules.rules.insert(rules.rules.begin(), nr); AssignPatterns(rules, 0, picked); ruleSel = 0; freeTex();
		}
		ImGui::EndDisabled();
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##fxr", ImVec2(0, 0), ImGuiChildFlags_Borders);
	drawSlotStrip(ch, *bank);
	ImGui::Separator();
	drawRuleEditor(ch, bank);
	ImGui::EndChild();
}

} // namespace cgm
