#include "../ui/ui_style.h"
#include "../ui/ui_layout2.h"
#include "fxr_panel.h"
#include "../cg.h"
#include "../app_log.h"
#include "../character_instance.h"
#include "../i18n.h"
#include "../game_table.h"
#include "../png_writer.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// The MBAACC game folder the ini belongs in: the install the character's files came from (a folder holding MBAA.exe, up to 3 levels above
// the .pal / .cg / .HA6), else the configured folder (File > Open game folder remembers it), else the detected install (Steam / common paths).
// "" when none is known: the panel never falls back to the exe's working directory.
static bool IsMbaaccRoot(const std::filesystem::path &d) { std::error_code ec; return !d.empty() && std::filesystem::is_regular_file(d / "MBAA.exe", ec); }
std::string FxPanel::mbaaccGameDir(const CharacterInstance &ch) {
	namespace fs = std::filesystem;
	std::vector<std::string> files = { ch.cg.palettePath(0), ch.getCGPath(), ch.getTopHA6Path() };
	for (const std::string &h : ch.getHA6Paths()) files.push_back(h);
	for (const std::string &f : files) {
		if (f.empty()) continue;
		fs::path d = fs::u8path(f).parent_path();
		for (int k = 0; k < 4 && !d.empty(); k++, d = d.parent_path()) { if (IsMbaaccRoot(d)) return d.u8string(); if (d == d.parent_path()) break; }
	}
	const std::string rem = abrowser::RememberedDir(abrowser::Game::MBAACC);
	if (!rem.empty() && IsMbaaccRoot(fs::u8path(rem))) return rem;
	const abrowser::Install &in = abrowser::InstallOf(abrowser::Game::MBAACC);
	if (!in.path.empty()) return in.path;
	if (const abrowser::GameDef *g = abrowser::FindGame(abrowser::Game::MBAACC))
		for (const char *c : g->commonPaths) if (IsMbaaccRoot(fs::u8path(c))) return c;
	return "";
}

std::string FxPanel::defaultPath(const CharacterInstance &ch) {
	namespace fs = std::filesystem;
	std::string stem = ch.getName();
	const std::string pp = ch.cg.palettePath(0), cgp = ch.getCGPath();
	if (!pp.empty()) stem = fs::u8path(pp).stem().string(); else if (!cgp.empty()) stem = fs::u8path(cgp).stem().string();
	for (char &c : stem) c = (char)tolower((unsigned char)c);
	charStem = stem;
	const std::string dir = mbaaccGameDir(ch);
	return dir.empty() ? std::string() : (fs::u8path(dir) / "fxrecolor" / (stem + ".ini")).u8string();   // <game>\fxrecolor\<char>.ini
}

const unsigned *FxPanel::slotPal(CharacterInstance &ch, int slot) const { return ch.cg.paletteAt(slot, 0); }

void FxPanel::reset(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage) {
	owner = &ch; gen = ch.cg.generation(); usageVersion = usage.version;
	freeTex();
	cls = FxClassification();
	if (bank) ClassifyEffects(*bank, usage, ch.frameData, cls);
	nSlots = 0; while (nSlots < 64 && ch.cg.paletteAt(nSlots, 0)) nSlots++;
	bodyHist.clear(); accentCands.clear();
	if (bank) {
		BodyIndexHistogram(*bank, bodyHist);
		std::vector<const uint32_t *> pals; for (int s = 0; s < nSlots; s++) pals.push_back(ch.cg.paletteAt(s, 0));
		accentCands = SuggestAccentIndices(bodyHist, pals, 6);
	}
	if (pathAuto) path = defaultPath(ch);   // follows the character / bank until the user types or picks another file
	if (const char *st = std::getenv("HANTEI_FXR_SELFTEST")) { static bool done = false; if (!done) { done = true; selfTest(st); return; } }
}

bool FxPanel::selfTestPending() const {
	static bool want = std::getenv("HANTEI_FXR_SELFTEST") != nullptr;
	return want && owner == nullptr;
}

void FxPanel::importDgv(bool accentFollow) {
	parserMsgs.clear();
	const int n = LoadRules(DgvAkihaIni(), rules, parserMsgs);
	if (accentFollow) {
		// accent index: of the suggested body indices, the one whose slot-0 colour is closest in hue to DGV's red, so slot 0 keeps his look
		// and the other slots follow that same palette entry
		std::vector<int> idx;
		const uint32_t *p0 = owner ? owner->cg.paletteAt(0, 0) : nullptr;
		float best = 1e9f;
		const fx::Lab red = fx::srgbToOklab({ 0xeb / 255.f, 0.f, 0.f });
		for (const AccentCandidate &c : accentCands) {
			if (!p0) { idx = { c.index }; break; }
			uint32_t rp[256]; ToRuntimePalette(p0, rp); const fx::Lab l = fx::srgbToOklab(fx::autoAccent(rp, { c.index }));
			const float C = std::sqrt(l.a * l.a + l.b * l.b); if (C < 0.05f) continue;
			float d = std::fabs(std::atan2(l.b, l.a) - std::atan2(red.b, red.a)); if (d > 3.14159265f) d = 6.2831853f - d;
			if (d < best) { best = d; idx = { c.index }; }
		}
		if (idx.empty() && !accentCands.empty()) idx = { accentCands[0].index };
		rules = DgvAccentFollow(rules, idx);
		status = Fmt(TXT("Imported %d DGV groups, palette-following (accent tokens; Akiha only: sprite ids are Akiha's)"), n);
	} else status = Fmt(TXT("Imported %d DGV groups (Akiha only: sprite ids are Akiha's)"), n);
	ruleSel = rules.rules.empty() ? -1 : 0; freeTex();
}

bool FxPanel::loadFile() {
	std::string t;
	if (!ReadText(path, t)) { status = TXT("Cannot read that file."); return false; }
	parserMsgs.clear(); const int n = LoadRules(t, rules, parserMsgs); saved = rules; status = Fmt(TXT("Loaded %d rules"), n); ruleSel = rules.rules.empty() ? -1 : 0; freeTex();
	return true;
}
bool FxPanel::saveFile(bool askFolder) {
	if (path.empty()) {   // no MBAACC folder known: ask (never write next to the exe)
		if (!askFolder) { status = TXT("No MBAACC game folder is known: set the file path or pick the game folder."); return false; }
		const std::string dir = BrowseForFolderUtf8("");
		if (dir.empty()) { status = TXT("Save cancelled: no game folder picked."); return false; }
		path = (std::filesystem::u8path(dir) / "fxrecolor" / ((charStem.empty() ? std::string("char") : charStem) + ".ini")).u8string();
		pathAuto = false;
	}
	std::error_code ec; std::filesystem::create_directories(std::filesystem::u8path(path).parent_path(), ec);
	std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
	if (!f) { status = TXT("Cannot write that file."); return false; }
	const std::string t = SaveRules(rules); f.write(t.data(), (std::streamsize)t.size()); status = Fmt(TXT("Saved %zu rules to %s"), rules.rules.size(), path.c_str()); saved = rules;
	return true;
}
void FxPanel::newRuleFromTicked() {
	fx::Rule nr = DefaultRule(UniqueRuleId(rules, "rule")); if (!accentCands.empty()) nr.accentIdx = { accentCands[0].index };
	rules.rules.insert(rules.rules.begin(), nr); AssignPatterns(rules, 0, picked); ruleSel = 0; freeTex();
}

// --fxr-selftest <dir>: drives load / save / assign / new-rule through the same methods the buttons call (no input events), checks the
// results and writes <dir>/fxr_selftest.txt. The caller takes the picture with the app's own --capture.
void FxPanel::selfTest(const std::string &dir) {
	namespace fs = std::filesystem;
	std::string rep; int fails = 0;
	auto chk = [&](bool ok, const std::string &what) { rep += std::string(ok ? "PASS " : "FAIL ") + what + "\n"; fails += !ok; };
	std::error_code ec; fs::create_directories(fs::u8path(dir), ec);
	path = (fs::u8path(dir) / "selftest.ini").u8string(); pathAuto = false;
	rules = fx::CharRules(); picked.clear();
	LoadRules(DgvAkihaIni(), rules, parserMsgs);
	const size_t nDgv = rules.rules.size();
	chk(nDgv == 9, "DGV ruleset parses to 9 rules (got " + std::to_string(nDgv) + ")");
	bool ramps16 = true; for (auto &r : rules.rules) ramps16 &= r.ramp.size() == (size_t)fx::kRampN;
	chk(ramps16, "every DGV ramp has kRampN=16 stops");
	chk(parserMsgs.empty(), "no parser messages: " + parserMsgs);
	chk(saveFile(), "save");
	const std::string saved1 = SaveRules(rules);
	rules = fx::CharRules(); chk(loadFile(), "load"); chk(rules.rules.size() == nDgv && SaveRules(rules) == saved1, "load/save round trip is text-identical");
	// new rule from ticked patterns, with the new fields
	for (const FxPatternRow &p : cls.patterns) { if (picked.size() >= 3) break; picked.push_back(p.pattern); }
	for (int p = 0; picked.size() < 3 && p < 3; p++) if (std::find(picked.begin(), picked.end(), p) == picked.end()) picked.push_back(p);   // no CG bank (fixture): any pattern ids
	// accent-following DGV variant: every chromatic stop is a token, grey groups untouched, ids suffixed
	{
		fx::CharRules acc = DgvAccentFollow(rules, { 7 });
		bool tok = true, grey = true, ids = true;
		for (size_t i = 0; i < acc.rules.size(); i++) {
			const fx::Rule &a = acc.rules[i]; const fx::Rule &o = rules.rules[i];
			bool anyTok = false; for (auto &st : a.ramp) anyTok |= st.tok != fx::ColTok::Literal;
			if (o.id == "dgv_01" || o.id == "dgv_02") grey &= !anyTok && a.id == o.id; else { tok &= anyTok && a.accentIdx == std::vector<int>{ 7 }; ids &= a.id == o.id + "_acc"; }
		}
		chk(tok && ids, "accent-follow DGV: colour groups use accent tokens + accent_idx, ids get _acc");
		chk(grey, "accent-follow DGV: grey-only groups stay literal");
		fx::CharRules back2; std::string e2; LoadRules(SaveRules(acc), back2, e2);
		chk(e2.empty() && SaveRules(back2) == SaveRules(acc), "accent-follow DGV round-trips through the runtime parser");
		// the variant follows the slot: two different accents give different colours for the same mid-tone input
		const int ri = FindRuleIndex(acc, "dgv_00_acc");
		if (ri >= 0) {
			fx::Rule r2 = acc.rules[ri]; r2.accentIdx.clear();
			fx::SlotOv o0; o0.slot = 0; o0.hasAccent = true; o0.accent = { 0.9f, 0.1f, 0.1f };
			fx::SlotOv o1; o1.slot = 1; o1.hasAccent = true; o1.accent = { 0.1f, 0.3f, 0.95f };
			r2.ov = { o0, o1 };
			fx::Packed p0, p1; fx::pack(r2, 0, nullptr, p0); fx::pack(r2, 1, nullptr, p1);
			const fx::Rgb a = fx::applyCpu(p0, { 0.5f, 0.5f, 0.5f }), b = fx::applyCpu(p1, { 0.5f, 0.5f, 0.5f });
			chk(a.r > a.b && b.b > b.r, "accent-follow DGV: a red slot gives red, a blue slot gives blue");
		} else chk(false, "accent-follow DGV has dgv_00_acc");
	}
	chk(picked.size() == 3, "ticked 3 patterns");
	newRuleFromTicked();
	fx::Rule &nr = rules.rules[0];
	chk(nr.patterns.size() == 3 && nr.by == 0 && nr.kind == fx::Kind::LumRamp, "new rule: 3 patterns, lumramp by lightness");
	nr.by = 1; nr.kind = fx::Kind::Rainbow; nr.vmin = 0.1f; nr.vmax = 0.9f;
	fx::Rule hs = DefaultRule("selftest_hsl"); hs.kind = fx::Kind::Hsl; hs.hueDeg = 40; hs.sat = 1.2f; hs.val = 0.9f; hs.sprites = { 1, 2 };
	rules.rules.push_back(hs);
	chk(saveFile(), "save with new rules");
	fx::CharRules back; std::string err, txt; ReadText(path, txt); LoadRules(txt, back, err);
	chk(err.empty() && back.rules.size() == nDgv + 2, "reload: rules kept, no parser messages " + err);
	chk(back.rules[0].by == 1 && back.rules[0].kind == fx::Kind::Rainbow && back.rules[0].patterns == nr.patterns, "new rule fields survive (by=max, rainbow, patterns)");
	chk(back.rules[nDgv + 1].kind == fx::Kind::Hsl && std::fabs(back.rules[nDgv + 1].hueDeg - 40) < 1e-3f, "OkHSL rule survives");
	// assign: move one of the new rule's patterns into a DGV rule; it must leave rule 0
	const int moved = nr.patterns[0]; AssignPatterns(rules, 1, { moved });
	chk(std::find(rules.rules[0].patterns.begin(), rules.rules[0].patterns.end(), moved) == rules.rules[0].patterns.end()
		&& std::find(rules.rules[1].patterns.begin(), rules.rules[1].patterns.end(), moved) != rules.rules[1].patterns.end(), "assign moves a pattern between rules");
	chk(!DiffRules(back, rules).empty(), "diff reports the assignment");
	// shader maths: a black..white ramp by lightness maps mid grey to mid-ish grey, and RecolorImage keeps alpha
	fx::Packed pk; fx::pack(DefaultRule("t"), 0, nullptr, pk);
	const fx::Rgb lo = fx::applyCpu(pk, { 0, 0, 0 }), hi = fx::applyCpu(pk, { 1, 1, 1 });
	chk(lo.r < 0.02f && hi.r > 0.98f, "default ramp maps black to black and white to white");
	rep += "status: " + status + "\n";
	rules = back; ruleSel = 0; saved = back; freeTex();   // leave the saved state on screen for the capture
	if (!cls.patterns.empty()) { sel = cls.patterns[0].pattern; }
	rep += fails ? "SECTION fxr-selftest: FAIL\n" : "SECTION fxr-selftest: pass\n";
	std::ofstream(fs::u8path(dir) / "fxr_selftest.txt", std::ios::binary) << rep;
}

FxPanel::Tex &FxPanel::preview(CharacterInstance &ch, const Bank &bank, int image, const fx::Rule *rule, int slot) {
	std::string key = std::to_string(image) + "|" + std::to_string(slot) + "|" + std::to_string(gen) + "|" + (rule ? SaveRules([&] { fx::CharRules c; c.rules.push_back(*rule); return c; }()) : "-");
	auto it = tex.find(key);
	if (it != tex.end()) return it->second;
	if (tex.size() > 400) freeTex();
	Tex &t = tex[key];
	Rgba px;
	GLint maxTex = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
	if (RecolorImage(bank, image, rule, slot, slotPal(ch, slot), px) && px.w > 0 && px.h > 0) {
		// the same guards as Window::upload (issue #87): a bad size made glTexImage2D fail with GL_INVALID_VALUE
		if (px.px.size() < (size_t)px.w * px.h * 4 || (maxTex > 0 && std::max(px.w, px.h) > maxTex)) {
			HC_LOG_WARN("Effect recolour: preview of image {} has an unusable size {}x{} ({} bytes); shown empty", image, px.w, px.h, px.px.size());
			return t;
		}
		glGenTextures(1, &t.id);
		glBindTexture(GL_TEXTURE_2D, t.id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		while (glGetError() != GL_NO_ERROR) {}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, px.w, px.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.px.data());
		if (const GLenum e = glGetError(); e != GL_NO_ERROR) {
			HC_LOG_WARN("Effect recolour: preview upload {}x{} failed (GL 0x{:x}); shown empty", px.w, px.h, (unsigned)e);
			glDeleteTextures(1, &t.id); t.id = 0;
			return t;
		}
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
	const ImVec2 p0 = ImGui::GetCursorScreenPos(); const float W = ImGui::GetContentRegionAvail().x - ui::Px(8), H = ui::Px(16);
	ImGui::Dummy(ImVec2(W, H));
	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImU32 col[fx::kRampN];
	for (int k = 0; k < fx::kRampN; k++) { const fx::Rgb c = fx::oklabToSrgb({ r[k][0], r[k][1], r[k][2] }); col[k] = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1)); }   // entries are Oklab
	for (int k = 0; k + 1 < fx::kRampN; k++) {
		const float x0 = p0.x + W * k / (fx::kRampN - 1), x1 = p0.x + W * (k + 1) / (fx::kRampN - 1);
		dl->AddRectFilledMultiColor(ImVec2(x0, p0.y), ImVec2(x1, p0.y + H), col[k], col[k + 1], col[k + 1], col[k]);
	}
	int del = -1;
	static const char *kTok[] = { "colour", "accent", "accent.dark", "accent.light" };
	for (size_t i = 0; i < stops.size(); i++) {
		fx::Stop &s = stops[i];
		ImGui::PushID((int)i);
		int tk = (int)s.tok;
		ImGui::SetNextItemWidth(ui::Px(104)); if (ImGui::Combo("##tok", &tk, kTok, 4)) s.tok = (fx::ColTok)tk;
		ImGui::SameLine();
		if (s.tok == fx::ColTok::Literal) { float c[3] = { s.c.r, s.c.g, s.c.b }; if (ImGui::ColorEdit3("##c", c, ImGuiColorEditFlags_NoInputs)) s.c = { c[0], c[1], c[2] }; }
		else { const fx::Rgb rc = fx::oklabToSrgb(fx::resolveStop(s, accent)); ImGui::ColorButton("##a", V4(rc), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())); }
		ImGui::SameLine(); ImGui::Checkbox("@", &s.hasPos);
		if (s.hasPos) { ImGui::SameLine(); ImGui::SetNextItemWidth(ui::Px(90)); ImGui::SliderFloat("##p", &s.pos, 0.f, 1.f, "%.2f"); }
		ImGui::SameLine(); if (ImGui::SmallButton("x") && stops.size() > 1) del = (int)i;
		ImGui::PopID();
	}
	if (del >= 0) stops.erase(stops.begin() + del);
	ImGui::PushID(idp);
	if (ImGui::SmallButton(LBL("+ stop"))) { fx::Stop s; s.tok = fx::ColTok::Accent; stops.insert(stops.end() - (stops.size() > 1 ? 1 : 0), s); }
	ui::SameLineButton(LBL("Preset: dark-accent-white")); if (ImGui::SmallButton(LBL("Preset: dark-accent-white"))) fx::parseStops("#000000 accent.dark accent accent.light #ffffff", stops);
	ui::SameLineButton(LBL("Preset: black-accent")); if (ImGui::SmallButton(LBL("Preset: black-accent"))) fx::parseStops("#000000 accent", stops);
	ui::SameLineButton(LBL("Preset: rainbow")); if (ImGui::SmallButton(LBL("Preset: rainbow"))) { stops.clear(); for (int k = 0; k < fx::kRampN; k++) { fx::Stop s; s.c = fx::okhsvToSrgb({ (double)k / fx::kRampN, 0.9, 0.95 }); stops.push_back(s); } }
	ImGui::PopID();
}

void FxPanel::drawRuleEditor(CharacterInstance &ch, const Bank *bank) {
	if (ruleSel < 0 || ruleSel >= (int)rules.rules.size()) { ImGui::TextDisabled("%s", TXT("Select or create a rule.")); return; }
	fx::Rule &r = rules.rules[ruleSel];
	const unsigned *sp = slotPal(ch, slotSel);
	const fx::Rgb acc = AccentFor(r, sp, slotSel);
	char idb[64]; snprintf(idb, sizeof idb, "%s", r.id.c_str());
	ImGui::SetNextItemWidth(ui::Px(160)); if (ImGui::InputText(LBL("Rule id"), idb, sizeof idb) && idb[0]) r.id = idb;
	ImGui::SameLine(); ImGui::Checkbox(LBL("Enabled"), &r.enabled);
	int kind = r.kind == fx::Kind::LumRamp ? 0 : r.kind == fx::Kind::Rainbow ? 1 : r.kind == fx::Kind::Hsl ? 2 : 3;
	static const char *kKind[] = { "Luminance -> ramp", "Rainbow (hue -> ramp)", "HSL shift (OkHSL, default)", "HSV shift (OkHSV)" };
	ImGui::SetNextItemWidth(ui::Px(200));
	if (ImGui::Combo(LBL("Mapping"), &kind, kKind, 4)) r.kind = kind == 0 ? fx::Kind::LumRamp : kind == 1 ? fx::Kind::Rainbow : kind == 2 ? fx::Kind::Hsl : fx::Kind::Hsv;
	int bank_ = r.bank; static const char *kBank[] = { "any", "character bank", "effect.ha6" };
	ImGui::SetNextItemWidth(ui::Px(140)); if (ImGui::Combo(LBL("Bank"), &bank_, kBank, 3)) r.bank = bank_;
	ImGui::SameLine(); int bl = r.blend; ImGui::SetNextItemWidth(ui::Px(70)); if (ImGui::InputInt(LBL("Blend (-1 any)"), &bl, 0, 0)) r.blend = std::max(-1, bl);
	if (r.kind == fx::Kind::Hsv || r.kind == fx::Kind::Hsl) {
		const bool hsl = r.kind == fx::Kind::Hsl;
		ImGui::TextDisabled("%s", hsl ? TXT("OkHSL edit: hue and saturation change, perceived lightness is kept (lightness x = 1).") : TXT("OkHSV edit (picker style): V is not perceptual lightness.")); ImGui::SliderFloat(LBL("Hue shift"), &r.hueDeg, -180.f, 180.f, "%.0f deg");
		ImGui::SliderFloat(LBL("Saturation x"), &r.sat, 0.f, 2.f); ImGui::SliderFloat(hsl ? LBL("Lightness x") : LBL("Value x"), &r.val, 0.f, 2.f);
	} else {
		static const char *kBy[] = { "Oklab lightness (default)", "Max channel", "Luma" };
		int by = std::clamp(r.by, 0, 2); ImGui::SetNextItemWidth(ui::Px(200));
		if (ImGui::Combo(LBL("Drive by"), &by, kBy, 3)) r.by = by;
		if (r.kind == fx::Kind::LumRamp) {
			ImGui::SetNextItemWidth(ui::Px(220)); float vr[2] = { r.vmin, r.vmax };
			if (ImGui::DragFloat2(LBL("Source range"), vr, 0.005f, 0.f, 1.f, "%.3f")) { r.vmin = std::clamp(vr[0], 0.f, 0.99f); r.vmax = std::clamp(vr[1], r.vmin + 0.01f, 1.f); }
		} else ImGui::TextDisabled("%s", TXT("Rainbow: entry k of the ramp (16 slots) colours OkLCh hue k/16; lightness and chroma are kept."));
		ImGui::SeparatorText(TXT("Ramp"));
		drawStops(r.ramp, acc, "rampp");
	}
	// accent
	ImGui::SeparatorText(TXT("Accent (per slot)"));
	std::string ai = fx::joinInts(r.accentIdx); char ab[64]; snprintf(ab, sizeof ab, "%s", ai.c_str());
	ImGui::SetNextItemWidth(ui::Px(160));
	if (ImGui::InputText(LBL("Accent palette indices"), ab, sizeof ab)) r.accentIdx = fx::parseInts(ab);
	ImGui::SetItemTooltip("%s", TXT("Empty = the most vivid colour of the slot's palette. Otherwise the first listed index is the accent."));
	if (ImGui::Button(LBL("Suggest accent index from the body")) && !accentCands.empty()) r.accentIdx = { accentCands[0].index };
	ImGui::SetItemTooltip("%s", TXT("Picks the body palette index (used by type 0 sprites) that is most vivid across the slots, so each slot's accent is the colour that slot gives that index."));
	for (const AccentCandidate &c : accentCands) {
		ImGui::SameLine(); const uint32_t *pp = slotPal(ch, slotSel); const uint32_t col = pp ? pp[c.index] : 0;
		ImGui::PushID(c.index);
		if (ImGui::ColorButton("##cand", ImVec4((col & 255) / 255.f, ((col >> 8) & 255) / 255.f, ((col >> 16) & 255) / 255.f, 1), 0, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()))) r.accentIdx = { c.index };
		ImGui::SetItemTooltip(TXT("index %d"), c.index);
		ImGui::PopID();
	}
	if (ImGui::Button(LBL("Write auto accents of all slots as overrides"))) {
		for (int s = 0; s < nSlots; s++) {
			fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == s) o = &x;
			if (!o) { r.ov.emplace_back(); o = &r.ov.back(); o->slot = s; }
			if (!o->hasAccent) { o->hasAccent = true; uint32_t rp[256]; ToRuntimePalette(slotPal(ch, s), rp); o->accent = fx::autoAccent(rp, r.accentIdx); }
		}
	}
	ImGui::SameLine(); if (ImGui::Button(LBL("Clear all overrides"))) r.ov.clear();
	if (ImGui::BeginTable("##ov", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, ui::Px(150)))) {
		ImGui::TableSetupColumn(TXT("slot")); ImGui::TableSetupColumn(TXT("auto")); ImGui::TableSetupColumn(TXT("override")); ImGui::TableSetupColumn(TXT("ramp"));
		ImGui::TableHeadersRow();
		for (int s = 0; s < nSlots; s++) {
			ImGui::TableNextRow(); ImGui::PushID(s);
			fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == s) o = &x;
			ImGui::TableNextColumn(); if (ImGui::Selectable(Fmt("%d", s).c_str(), slotSel == s, ImGuiSelectableFlags_SpanAllColumns)) slotSel = s;
			ImGui::TableNextColumn(); uint32_t rp[256]; ToRuntimePalette(slotPal(ch, s), rp); ImGui::ColorButton("##auto", V4(fx::autoAccent(rp, r.accentIdx)), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
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
			if (hr) { ImGui::SameLine(); if (ImGui::SmallButton(LBL("edit"))) rampSlotEdit = s; }
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
	const float box = ui::Px(150);
	if (showBefore) { Tex &t = preview(ch, bank, spriteSel, nullptr, slotSel); DrawTex(this, t.id, t.w, t.h, box); ImGui::SameLine(); }
	{ Tex &t = preview(ch, bank, spriteSel, rule, slotSel); DrawTex(this, t.id, t.w, t.h, box); }
	ImGui::SameLine(); ImGui::BeginGroup();
	ImGui::Text("%s: %s", TXT("rule"), rule ? rule->id.c_str() : TXT("(none, stays as authored)"));
	ImGui::Text("%s %d", TXT("slot"), slotSel);
	{ const uint32_t *pp = slotPal(ch, slotSel); if (rule) { const fx::Rgb a = AccentFor(*rule, pp, slotSel); ImGui::ColorButton("##acc", V4(a), 0, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())); ImGui::SameLine(); ImGui::TextUnformatted(fx::hexOf(a).c_str()); } }
	ImGui::EndGroup();
	// every slot
	const float tile = ui::Px(56), gap = ui::Px(6);
	const float W = ImGui::GetContentRegionAvail().x;
	const int perRow = std::max(1, (int)(W / (tile + gap)));
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
		if ((s + 1) % perRow) ImGui::SameLine(0, gap);
	}
	ImGui::NewLine();
}

void FxPanel::draw(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage, const std::function<void(int, int)> &navigate) {
	if (owner != &ch || gen != ch.cg.generation() || usageVersion != usage.version) reset(ch, bank, usage);
	if (!bank) { ImGui::TextDisabled("%s", TXT("This bank format has no effect classification (not a BMP Cutter bank).")); return; }
	// one line; the method note lives in the (?) tooltip
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(TXT("Authors the PovertyCaster runtime recolour (<game>\\fxrecolor\\<char>.ini)."));
	ui::HelpMarker(TXT("Authors the PovertyCaster runtime recolour (<game>\\fxrecolor\\<char>.ini). Previews apply the runtime's own shader maths to the original pixels: nothing in the game files changes. Method after DGV's \"Better Akiha\"."));
	// file bar
	{
		char pb[512]; snprintf(pb, sizeof pb, "%s", path.c_str());
		// the path takes what the four buttons leave; at a narrow width the buttons wrap under it
		const float sp = ImGui::GetStyle().ItemSpacing.x;
		const float btnW = i18n::ButtonWidth(LBL("Load")) + i18n::ButtonWidth(LBL("Save")) + i18n::ButtonWidth(LBL("Import DGV Akiha ruleset")) + i18n::ButtonWidth(LBL("Diff vs file")) + sp * 4;
		ImGui::SetNextItemWidth(std::max(ui::Em(14.f), ImGui::GetContentRegionAvail().x - btnW));
		if (ImGui::InputTextWithHint("##fxpath", TXT("no MBAACC game folder known: Save asks for it"), pb, sizeof pb)) { path = pb; pathAuto = false; }
		ui::SameLineButton(LBL("Load")); if (ImGui::Button(LBL("Load"))) loadFile();
		ui::SameLineButton(LBL("Save")); if (ImGui::Button(LBL("Save"))) saveFile(true);
		ui::SameLineButton(LBL("Import DGV Akiha ruleset")); if (ImGui::Button(LBL("Import DGV Akiha ruleset"))) ImGui::OpenPopup("##fximport");
		if (ImGui::BeginPopup("##fximport")) {
			if (ImGui::MenuItem(LBL("DGV Akiha: fixed red (original)"))) importDgv(false);
			ImGui::SetItemTooltip("%s", TXT("DGV's ramps as he painted them: the same red in every palette slot."));
			if (ImGui::MenuItem(LBL("DGV Akiha: follows the palette slot (accent)"))) importDgv(true);
			ImGui::SetItemTooltip("%s", TXT("DGV's groups with the red stops turned into accent tokens: each palette slot recolours the effects with its own accent colour."));
			ImGui::EndPopup();
		}
		ui::SameLineButton(LBL("Diff vs file")); if (ImGui::Button(LBL("Diff vs file"))) ImGui::OpenPopup("##fxdiff");
		if (ImGui::BeginPopup("##fxdiff")) { ImGui::TextUnformatted(DiffRules(saved, rules).c_str()); ImGui::EndPopup(); }
	}
	if (!status.empty()) { ImGui::PushTextWrapPos(0.f); ImGui::TextDisabled("%s", status.c_str()); ImGui::PopTextWrapPos(); }
	if (!parserMsgs.empty()) ImGui::TextColored(ImVec4(1, .6f, .3f, 1), "%s", parserMsgs.c_str());
	ImGui::Text("%s: %d | %s: %d | %s: %d | %s: %zu", TXT("fixed-colour sprites"), cls.fixedImages, TXT("drawn"), cls.usedFixedImages, TXT("shared by 2+ patterns"), cls.sharedFixedImages, TXT("effect patterns"), cls.patterns.size());

	// rules + effect patterns (left) | slot strip + rule editor (right): a resizable split that fills the tab
	const float paneH = std::max(ui::Em(10.f), ImGui::GetContentRegionAvail().y);
	const bool split = ImGui::BeginTable("##fxsplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings, ImVec2(0, paneH));
	if (split) {
		ImGui::TableSetupColumn("##fxleft", ImGuiTableColumnFlags_WidthStretch, 0.45f);
		ImGui::TableSetupColumn("##fxright", ImGuiTableColumnFlags_WidthStretch, 0.55f);
		ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
	}
	const float childH = paneH - ImGui::GetStyle().CellPadding.y * 2;
	ImGui::BeginChild("##fxl", ImVec2(split ? 0.f : ui::Px(520), childH), ImGuiChildFlags_Borders);
	// rules list
	ImGui::SeparatorText(TXT("Rules (first match wins, file order)"));
	if (ImGui::SmallButton(LBL("New"))) { { fx::Rule nr = DefaultRule(UniqueRuleId(rules, "rule")); if (!accentCands.empty()) nr.accentIdx = { accentCands[0].index }; rules.rules.insert(rules.rules.begin(), nr); } ruleSel = 0; freeTex(); }
	ImGui::BeginDisabled(ruleSel < 0);
	ui::SameLineButton(LBL("Duplicate")); if (ImGui::SmallButton(LBL("Duplicate")) && ruleSel >= 0) { fx::Rule d = rules.rules[ruleSel]; d.id = UniqueRuleId(rules, d.id + "_copy"); rules.rules.insert(rules.rules.begin() + ruleSel + 1, d); ruleSel++; }
	ui::SameLineButton(LBL("Delete")); if (ImGui::SmallButton(LBL("Delete")) && ruleSel >= 0) { rules.rules.erase(rules.rules.begin() + ruleSel); ruleSel = std::min(ruleSel, (int)rules.rules.size() - 1); freeTex(); }
	ui::SameLineButton(LBL("Up")); if (ImGui::SmallButton(LBL("Up")) && ruleSel > 0) { std::swap(rules.rules[ruleSel], rules.rules[ruleSel - 1]); ruleSel--; freeTex(); }
	ui::SameLineButton(LBL("Down")); if (ImGui::SmallButton(LBL("Down")) && ruleSel >= 0 && ruleSel + 1 < (int)rules.rules.size()) { std::swap(rules.rules[ruleSel], rules.rules[ruleSel + 1]); ruleSel++; freeTex(); }
	ImGui::EndDisabled();
	ImGui::BeginChild("##fxrules", ImVec2(0, ui::Px(110)), ImGuiChildFlags_Borders);
	for (int i = 0; i < (int)rules.rules.size(); i++) {
		const fx::Rule &r = rules.rules[i];
		if (ImGui::Selectable(Fmt("%s%s  [%zu patterns, %zu sprites]##r%d", r.enabled ? "" : "(off) ", r.id.c_str(), r.patterns.size(), r.sprites.size(), i).c_str(), ruleSel == i)) ruleSel = i;
	}
	ImGui::EndChild();
	// grid
	ImGui::SeparatorText(TXT("Effect patterns"));
	if (ui::BeginFormGrid("##fxfilter", 2, ui::Em(11.f))) {
		ui::FormCell(TXT("Name")); ImGui::InputText("##fxname", nameFilter, sizeof nameFilter);
		ui::FormCell(TXT("Min score")); ImGui::SliderInt("##fxminscore", &minScore, 0, 100);
		ImGui::SetItemTooltip("%s", TXT("Share of the pattern's layers that draw fixed-colour sprites. A body pattern with one RGB hair sprite scores low."));
		ui::EndFormGrid();
	}
	ImGui::Checkbox(LBL("Shared sprites only"), &onlyShared); ui::SameLineCheckbox(LBL("No rule yet")); ImGui::Checkbox(LBL("No rule yet"), &onlyUnruled);
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
	if (ImGui::BeginTable("##fxgrid", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0, std::max(ui::Em(6.f), ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2 - ImGui::GetStyle().WindowPadding.y)))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight()); ImGui::TableSetupColumn(LBL("pat"), ImGuiTableColumnFlags_WidthFixed, ui::Px(40)); ImGui::TableSetupColumn(LBL("name"));
		ImGui::TableSetupColumn(LBL("fx/all"), ImGuiTableColumnFlags_WidthFixed, ui::Px(56)); ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, ui::Px(30)); ImGui::TableSetupColumn(LBL("spr"), ImGuiTableColumnFlags_WidthFixed, ui::Px(32));
		ImGui::TableSetupColumn(LBL("shr"), ImGuiTableColumnFlags_WidthFixed, ui::Px(32)); ImGui::TableSetupColumn(LBL("rule"));
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
		ImGui::BeginDisabled(picked.empty());
		ui::SameLineButton(LBL("New rule from ticked")); if (ImGui::Button(LBL("New rule from ticked"))) newRuleFromTicked();
		ImGui::EndDisabled();
	}
	ImGui::EndChild();
	if (split) ImGui::TableSetColumnIndex(1); else ImGui::SameLine();
	ImGui::BeginChild("##fxr", ImVec2(0, childH), ImGuiChildFlags_Borders);
	drawSlotStrip(ch, *bank);
	ImGui::Separator();
	drawRuleEditor(ch, bank);
	ImGui::EndChild();
	if (split) ImGui::EndTable();
}

bool FxPanel::script(const std::string &arg, std::string *msg) {
	const size_t sp = arg.find(' ');
	const std::string sub = arg.substr(0, sp), val = sp == std::string::npos ? std::string() : arg.substr(sp + 1);
	auto fail = [&](const std::string &m) { if (msg) *msg = "fxr " + sub + ": " + m; return false; };
	if (!owner) return fail("draw the Effect recolor tab first (cgm tab Effect recolor + wait)");
	if (sub == "import") {
		if (val == "dgv") importDgv(false); else if (val == "dgv-accent") importDgv(true); else return fail("want dgv | dgv-accent");
		return true;
	}
	if (sub == "rule") {
		int i = FindRuleIndex(rules, val);
		if (i < 0 && !val.empty() && isdigit((unsigned char)val[0])) i = atoi(val.c_str());
		if (i < 0 || i >= (int)rules.rules.size()) return fail("no rule " + val);
		ruleSel = i; return true;
	}
	if (sub == "pattern") {
		if (val == "first") {   // first effect pattern the current rules colour
			for (const FxPatternRow &p : cls.patterns) if (RuleOfPattern(rules, p, slotSel)) { sel = p.pattern; spriteSel = -1; return true; }
			return fail("no ruled effect pattern");
		}
		if (!cls.find(atoi(val.c_str()))) return fail("pattern " + val + " is not an effect pattern");
		sel = atoi(val.c_str()); spriteSel = -1; return true;
	}
	if (sub == "sprite") { spriteSel = atoi(val.c_str()); return true; }
	if (sub == "slot") { const int s = atoi(val.c_str()); if (s < 0 || s >= std::max(1, nSlots)) return fail("slot out of range"); slotSel = s; return true; }
	if (sub == "accent") {   // on the selected rule: palette index, auto (the suggested body index), or a fixed override for the selected slot
		if (ruleSel < 0 || ruleSel >= (int)rules.rules.size()) return fail("no rule selected");
		fx::Rule &r = rules.rules[ruleSel];
		if (val == "auto") { if (accentCands.empty()) return fail("no accent candidates"); r.accentIdx = { accentCands[0].index }; }
		else if (!val.empty() && val[0] == '#') {
			fx::Rgb c; if (!fx::parseHex(val, c)) return fail("bad colour " + val);
			fx::SlotOv *o = nullptr; for (auto &x : r.ov) if (x.slot == slotSel) o = &x;
			if (!o) { r.ov.emplace_back(); o = &r.ov.back(); o->slot = slotSel; }
			o->hasAccent = true; o->accent = c;
		} else if (!val.empty() && isdigit((unsigned char)val[0])) r.accentIdx = fx::parseInts(val);
		else return fail("want <index> | auto | #rrggbb");
		freeTex(); return true;
	}
	if (sub == "path") { pathAuto = val.empty(); path = pathAuto ? defaultPath(*owner) : val; return true; }   // empty = back to <game>\fxrecolor\<char>.ini
	if (sub == "load") { if (!val.empty()) { path = val; pathAuto = false; } return loadFile() || fail(status); }
	if (sub == "save") { if (!val.empty()) { path = val; pathAuto = false; } return saveFile(false) || fail(status); }   // scripts never open the folder picker
	if (sub == "status") {
		if (msg) *msg = Fmt("fxr status: path='%s' auto=%d rules=%zu rule=%d pattern=%d slot=%d | %s", path.c_str(), (int)pathAuto, rules.rules.size(), ruleSel, sel, slotSel, status.c_str());
		return false;   // like `cgm status`: the message lands in the script log
	}
	return fail("unknown (import|rule|pattern|sprite|slot|accent|path|load|save|status)");
}

} // namespace cgm
