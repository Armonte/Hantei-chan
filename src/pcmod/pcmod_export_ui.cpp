// "Export as pcmod package..." window; see pcmod_export_ui.h and docs/modmanager/pcmod-manifest_schema1.md.
#include "pcmod_export_ui.h"
#include "pcmod_manifest.h"
#include "pcmod_package.h"
#include "pcmod_vanilla.h"

#include "../app_log.h"
#include "../filedialog.h"
#include "../i18n.h"
#include "../mbaacc_package.h"
#include "../png_writer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace pcmodui {
namespace {

struct Row {
	bool include = true;
	std::string abs;      // UTF-8
	std::string rel;      // game-relative, editable
	std::string sha256;
	uint64_t size = 0;
	bool readable = false;
};

struct State {
	bool open = false;
	std::vector<Row> rows;
	std::string id, version = "1.0.0", name, author, description;
	int priority = 0;
	int classMode = 0;   // 0 auto, 1 sim, 2 cosmetic
	std::string exe, requiresText, conflicts, overrides, modes;   // one entry per line / comma
	std::string outDir;
	bool writeZip = true;
	std::string gameDir;
	pcmod::VanillaIndex vanilla;
	bool vanillaTried = false;
	std::string result;    // last export report
	bool resultOk = false;
	std::string digest, simDigest;
	std::vector<std::string> skippedWritten;   // data\*.wmt files the add paths skipped (written by the game, never read)
};
State g;

// Status colours readable on both the dark and the light (parchment) themes.
bool LightTheme() { const ImVec4 b = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg); return b.x + b.y + b.z > 1.5f; }
ImVec4 ColBad() { return LightTheme() ? ImVec4(.75f, .05f, .05f, 1) : ImVec4(1, .4f, .4f, 1); }
ImVec4 ColWarn() { return LightTheme() ? ImVec4(.6f, .3f, 0, 1) : ImVec4(1, .8f, .3f, 1); }
ImVec4 ColGood() { return LightTheme() ? ImVec4(.05f, .45f, .1f, 1) : ImVec4(.5f, 1, .5f, 1); }
ImVec4 ColSim() { return LightTheme() ? ImVec4(.1f, .25f, .7f, 1) : ImVec4(.55f, .75f, 1, 1); }

std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return s; }

// The top-level game folders a package may ship under (§3; each is one MBAACC .p pack or a presentation folder), and the
// root files it may ship.
const char* const kRootDirs[] = {"data", "story", "bg", "bgm", "dat", "grp", "se", "shader", "movie", "palettes"};
const char* const kRootFiles[] = {"charaselect.txt", "hud_theme.json", "font.fnt", "_sysfont.ini"};

// Game-relative path for a file on disk: from the last game-root segment (data, story, bg, ...), or a known root file.
std::string GuessRel(const std::string& absUtf8)
{
	std::string p = absUtf8;
	std::replace(p.begin(), p.end(), '/', '\\');
	std::vector<std::string> segs;
	std::stringstream ss(p);
	for (std::string s; std::getline(ss, s, '\\');) segs.push_back(s);
	if (!segs.empty()) for (const char* f : kRootFiles) if (Lower(segs.back()) == f) return segs.back();
	for (size_t i = segs.size(); i-- > 1;) {   // the file name itself is never the root
		if (i + 1 >= segs.size()) continue;
		const std::string l = Lower(segs[i]);
		if (std::any_of(std::begin(kRootDirs), std::end(kRootDirs), [&](const char* d) { return l == d; })) {
			std::string r;
			for (size_t k = i; k < segs.size(); ++k) r += (k > i ? "\\" : "") + segs[k];
			return r;
		}
	}
	return segs.empty() ? p : "data\\" + segs.back();   // unknown: assume data\ (the user can edit it)
}

void Hash(Row& r)
{
	pcmod::FileEntry e;
	std::string why;
	r.readable = pcmod::HashFile(r.abs, e, why);
	r.sha256 = r.readable ? e.sha256 : std::string();
	r.size = r.readable ? e.size : 0;
}

void AddFile(const std::string& abs, const std::string& rel, bool include)
{
	for (const Row& r : g.rows) if (Lower(r.abs) == Lower(abs)) return;
	Row r;
	r.abs = abs;
	r.rel = rel.empty() ? GuessRel(abs) : rel;
	{
		std::string n, why;
		if (pcmod::NormalisePath(r.rel, n, why) && pcmod::IsGameWrittenPath(n)) {
			if (std::find(g.skippedWritten.begin(), g.skippedWritten.end(), r.rel) == g.skippedWritten.end()) g.skippedWritten.push_back(r.rel);
			HC_LOG_INFO("pcmod export: skipped {} ({})", r.rel, pcmod::RefusedPathReason(n));
			return;
		}
	}
	r.include = include;
	Hash(r);
	g.rows.push_back(r);
}

void AddFolder(const std::string& dir)
{
	std::error_code ec;
	const fs::path root = fs::u8path(dir);
	// A folder laid out like the game (has data\ or grp\ ...): paths relative to it. Otherwise guess per file.
	bool gameLike = false;
	for (const char* r : kRootDirs) gameLike |= fs::is_directory(root / r, ec);
	std::vector<pcmod::SourceFile> files;
	std::string why;
	if (gameLike && pcmod::ScanSourceDir(dir, files, why)) {
		for (const auto& f : files) AddFile(f.absPath, f.rel, true);
		return;
	}
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
		if (it->is_regular_file(ec)) AddFile(it->path().u8string(), "", true);
}

// The character's descriptor, every loose file it references, and its _c.txt / .pal. Other files with the same stem
// are listed unticked (layered HA6 leftovers, backups ...).
void AddCharacter(const std::string& descriptor)
{
	if (descriptor.empty()) return;
	AddFile(descriptor, "", true);
	const auto v = mbpackage::ValidateCharacterPackage(descriptor);
	for (const auto& row : v.rows)
		if (row.status == mbpackage::RowStatus::loose && !row.where.empty()) AddFile(row.where, "", true);
	const fs::path d = fs::u8path(descriptor);
	const std::string stem = Lower(d.stem().u8string());
	std::error_code ec;
	for (const auto& it : fs::directory_iterator(d.parent_path(), ec)) {
		if (!it.is_regular_file(ec)) continue;
		const std::string n = Lower(it.path().filename().u8string());
		if (n.rfind(stem + "_", 0) != 0 && n.rfind(stem + ".", 0) != 0) continue;
		const std::string ext = Lower(it.path().extension().u8string());
		if (ext == ".notes" || ext == ".bak" || ext == ".hproj" || ext == ".tmp" || ext == ".json") continue;
		const bool core = n == stem + "_c.txt" || ext == ".pal";
		AddFile(it.path().u8string(), "", core);
	}
}

std::vector<std::string> SplitList(const std::string& s)
{
	std::vector<std::string> out;
	std::string cur;
	for (char c : s + "\n") {
		if (c == '\n' || c == '\r' || c == ',' || c == ' ' || c == '\t' || c == ';') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
		else cur += c;
	}
	return out;
}

// The class of one row's game path (Refused when the path itself is refused).
pcmod::PathClass RowClass(const Row& r)
{
	std::string n, why;
	return pcmod::NormalisePath(r.rel, n, why) ? pcmod::ClassifyPath(n) : pcmod::PathClass::Refused;
}

// The model as the window would export it, and the first problem ("" when it can be written). `simFiles` = the ticked
// files whose class is sim: exactly the ones the online sim digest covers.
pcmod::Manifest Build(std::string& problem, pcmod::PackageClass& detected, size_t* simFiles = nullptr)
{
	pcmod::Manifest m;
	m.id = g.id; m.version = g.version; m.name = g.name; m.author = g.author; m.description = g.description;
	m.priority = g.priority;
	for (const std::string& e : SplitList(g.exe)) m.exe.push_back(Lower(e));
	for (const std::string& r : SplitList(g.requiresText)) {
		const size_t at = r.find('@');
		m.requires_.push_back({r.substr(0, at), at == std::string::npos ? std::string() : r.substr(at + 1)});
	}
	m.conflicts = SplitList(g.conflicts);
	m.overrides = SplitList(g.overrides);
	m.modes = pcmod::SplitModes(g.modes);   // empty = every mode; not in any digest
	size_t sim = 0;
	for (const Row& r : g.rows) {
		if (!r.include) continue;
		if (RowClass(r) == pcmod::PathClass::Sim) ++sim;
		if (!r.readable) { problem = r.abs + ": " + TXT("cannot read"); }
		m.files.push_back({r.rel, r.sha256.empty() ? std::string(64, '0') : r.sha256, r.size});
	}
	if (simFiles) *simFiles = sim;
	detected = sim ? pcmod::PackageClass::Sim : pcmod::PackageClass::Cosmetic;
	m.cls = g.classMode == 1 ? pcmod::PackageClass::Sim : g.classMode == 2 ? pcmod::PackageClass::Cosmetic : detected;
	if (problem.empty()) problem = pcmod::Validate(m);
	return m;
}

void EnsureDefaults()
{
	if (!g.vanillaTried) {
		g.vanillaTried = true;
		if (g.gameDir.empty()) g.gameDir = pcmod::DefaultGameDir();
		if (!g.gameDir.empty()) {
			g.vanilla.Load(g.gameDir);
			if (g.exe.empty()) g.exe = pcmod::DefaultExeSha256(g.gameDir);
		}
	}
	if (g.outDir.empty()) {
		std::error_code ec;
		g.outDir = (fs::current_path(ec) / "pcmod_out").u8string();
	}
}

void Export()
{
	std::string problem;
	pcmod::PackageClass detected;
	pcmod::Manifest m = Build(problem, detected);
	if (!problem.empty()) { g.result = problem; g.resultOk = false; return; }
	std::vector<pcmod::SourceFile> src;
	for (const Row& r : g.rows) if (r.include) src.push_back({r.rel, r.abs});
	const pcmod::PackResult r = pcmod::Pack(src, m, g.outDir, g.writeZip);
	g.resultOk = r.ok;
	if (!r.ok) {
		g.result = std::string(TXT("Export refused: ")) + r.error;
		HC_LOG_WARN("pcmod export refused: {}", r.error);
		return;
	}
	g.digest = r.digestHex;
	g.simDigest = r.simDigestHex;
	g.result = std::string(TXT("Wrote ")) + r.packageDir + "\\pcmod.json (" + std::to_string(r.manifest.files.size()) + ")";
	if (!r.zipPath.empty()) g.result += "\n" + std::string(TXT("Wrote ")) + r.zipPath;
	g.result += "\n" + std::string(TXT("Package digest: ")) + r.digestHex;
	g.result += "\n" + std::string(TXT("Sim digest (online): ")) + r.simDigestHex;
	HC_LOG_INFO("pcmod export {} {} ({}): {} files, digest {}, sim digest {}", r.manifest.id, r.manifest.version, pcmod::ClassName(r.manifest.cls), r.manifest.files.size(), r.digestHex, r.simDigestHex);
}

} // namespace

void Open(const std::string& descriptor)
{
	g.open = true;
	EnsureDefaults();
	if (!descriptor.empty()) {
		AddCharacter(descriptor);
		if (g.id.empty()) {
			std::string stem = Lower(fs::u8path(descriptor).stem().u8string());
			std::string id;
			for (char c : stem) id += pcmod::ValidId(id + c) ? c : (id.empty() ? 'x' : '-');
			g.id = id;
			if (g.name.empty()) g.name = fs::u8path(descriptor).stem().u8string();
		}
	}
}

void MenuItem(const std::string& descriptor)
{
	if (ImGui::MenuItem(LBL("Export as pcmod package..."))) Open(descriptor);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", TXT("PovertyCaster mod package (pcmod.json schema 1): the active character's files,\n"
			"or any files / folder you add. Writes <id>\\pcmod.json + files and a .zip."));
}

void Draw()
{
	if (!g.open) return;
	const ImVec2 mp = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mp.x + 100, mp.y + 70), ImGuiCond_Appearing);
	// tall enough for the file table, the status lines and the export report under them, but inside the main window
	ImGui::SetNextWindowSize(ImVec2(980, std::max(400.f, std::min(880.f, ImGui::GetMainViewport()->Size.y - 90.f))), ImGuiCond_Appearing);
	if (!ImGui::Begin(LBL("Export as pcmod package###pcmod_export"), &g.open, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }

	std::string problem;
	pcmod::PackageClass detected;
	size_t simFiles = 0;
	const pcmod::Manifest m = Build(problem, detected, &simFiles);

	// ---- identity ----
	ImGui::PushItemWidth(260);
	ImGui::InputText(LBL("Id"), &g.id);
	ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("[a-z0-9][a-z0-9._-], folder name and catalogue key"));
	ImGui::InputText(LBL("Version"), &g.version);
	ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("MAJOR.MINOR.PATCH"));
	ImGui::InputText(LBL("Name"), &g.name);
	ImGui::InputText(LBL("Author"), &g.author);
	ImGui::InputInt(LBL("Priority"), &g.priority);
	g.priority = std::clamp(g.priority, -1000, 1000);
	ImGui::PopItemWidth();
	ImGui::InputTextMultiline(LBL("Description"), &g.description, ImVec2(-160, 48));

	const char* classes[] = {TXT("Auto"), "sim", "cosmetic"};
	ImGui::SetNextItemWidth(160);
	ImGui::Combo(LBL("Class"), &g.classMode, classes, 3);
	ImGui::SameLine();
	ImGui::Text(TXT("-> %s (detected: %s)"), pcmod::ClassName(m.cls), pcmod::ClassName(detected));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", TXT("Each file has a class decided by its path (see the Class column):\n"
			"sim: data\\ (except *.pal and data\\_csel\\), story\\, charaselect.txt. Both players must have the same sim files online.\n"
			"cosmetic: data\\*.pal, data\\_csel\\, bg\\ bgm\\ dat\\ grp\\ se\\ shader\\ movie\\ palettes\\, hud_theme.json, font.fnt, _sysfont.ini. Never compared.\n"
			"Any other path is refused.\n"
			"Auto picks sim when at least one file is sim. A sim package needs a sim file; a cosmetic package may not ship one."));
	ImGui::SameLine();
	ImGui::TextDisabled(TXT("%zu of %zu file(s) go into the online sim digest"), simFiles, m.files.size());

	ImGui::InputTextMultiline(LBL("MBAA.exe SHA-256 (one per line)"), &g.exe, ImVec2(-260, 40));
	if (!g.gameDir.empty()) { ImGui::SameLine(); if (ImGui::SmallButton(LBL("Use installed MBAA.exe"))) g.exe = pcmod::DefaultExeSha256(g.gameDir); }
	ImGui::SetNextItemWidth(300); ImGui::InputText(LBL("Requires (id@x.y.z, ...)"), &g.requiresText);
	ImGui::SetNextItemWidth(300); ImGui::InputText(LBL("Conflicts (ids)"), &g.conflicts);
	ImGui::SetNextItemWidth(300); ImGui::InputText(LBL("Overrides (ids)"), &g.overrides);
	ImGui::SetNextItemWidth(300); ImGui::InputText(LBL("Modes (ids, e.g. tag,team)"), &g.modes);
	ImGui::TextDisabled("%s", TXT("Online modes this package counts for (MBAACC: vs, tag, ...). Empty = every mode. Not part of any digest."));

	// ---- files ----
	ImGui::Separator();
	if (ImGui::Button(LBL("Add file..."))) {
		const std::string f = FileDialog(-1, false);
		if (!f.empty()) AddFile(AnsiToUtf8(f), "", true);
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Add folder..."))) {
		const std::string d = BrowseForFolderUtf8("");
		if (!d.empty()) AddFolder(d);
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Rehash"))) for (Row& r : g.rows) Hash(r);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Clear list"))) { g.rows.clear(); g.skippedWritten.clear(); }
	ImGui::SameLine();
	if (g.vanilla.loaded) ImGui::TextDisabled(TXT("Vanilla check: %s (%zu packed files)"), g.vanilla.gameDir.c_str(), g.vanilla.packed.size());
	else ImGui::TextDisabled("%s", TXT("Vanilla check off: no MBAA.exe in C:\\games\\mbaacc"));
	if (!g.skippedWritten.empty()) {
		ImGui::TextColored(ColWarn(), TXT("Skipped %zu .wmt file(s): written by the game, never read (win quotes come from story\\battle\\*_win.txt)"), g.skippedWritten.size());
		if (ImGui::IsItemHovered()) {
			std::string list;
			for (const std::string& w : g.skippedWritten) list += w + "\n";
			ImGui::SetTooltip("%s", list.c_str());
		}
	}

	int replaces = 0;
	// below the table: the status lines, the buttons and (after an export) the report, which must stay in view
	const float reportH = g.result.empty() ? 0.f : ImGui::GetTextLineHeightWithSpacing() * (float)(1 + std::count(g.result.begin(), g.result.end(), '\n'));
	const float listH = std::max(120.f, ImGui::GetContentRegionAvail().y - 150.f - reportH);
	if (ImGui::BeginTable("##pcmodfiles", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0, listH))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(TXT("Use"), ImGuiTableColumnFlags_WidthFixed, 34);
		ImGui::TableSetupColumn(TXT("Game path"), ImGuiTableColumnFlags_WidthStretch, 2.f);
		ImGui::TableSetupColumn(TXT("Class"), ImGuiTableColumnFlags_WidthFixed, 110);
		ImGui::TableSetupColumn(TXT("Size"), ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("SHA-256", ImGuiTableColumnFlags_WidthFixed, 150);   // i18n-ok: hash name
		ImGui::TableSetupColumn(TXT("Notes"), ImGuiTableColumnFlags_WidthStretch, 2.f);
		ImGui::TableHeadersRow();
		int del = -1;
		for (size_t i = 0; i < g.rows.size(); ++i) {
			Row& r = g.rows[i];
			ImGui::PushID((int)i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Checkbox("##use", &r.include);
			ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1); ImGui::InputText("##rel", &r.rel);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.abs.c_str());
			if (ImGui::BeginPopupContextItem("##rowctx")) { if (ImGui::MenuItem(LBL("Remove from list"))) del = (int)i; ImGui::EndPopup(); }
			const pcmod::PathClass pc = RowClass(r);
			ImGui::TableNextColumn();
			if (pc == pcmod::PathClass::Sim) {
				ImGui::TextColored(ColSim(), "%s", r.include ? TXT("sim (online)") : "sim");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("In the online sim digest: the peer must have this exact file."));
			} else if (pc == pcmod::PathClass::Cosmetic) {
				ImGui::TextDisabled("%s", TXT("cosmetic"));
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Presentation only: in the package digest, not in the online sim digest."));
			} else ImGui::TextColored(ColBad(), "%s", TXT("refused"));
			ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)r.size);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(r.sha256.empty() ? "-" : (r.sha256.substr(0, 16) + "...").c_str());
			if (ImGui::IsItemHovered() && !r.sha256.empty()) ImGui::SetTooltip("%s", r.sha256.c_str());
			ImGui::TableNextColumn();
			std::string n, why;
			if (!r.readable) ImGui::TextColored(ColBad(), "%s", TXT("cannot read"));
			else if (!pcmod::NormalisePath(r.rel, n, why)) ImGui::TextColored(ColBad(), "%s %s", TXT("refused:"), why.c_str());
			else if (pc == pcmod::PathClass::Refused) ImGui::TextColored(ColBad(), "%s", TXT("not a game path a package may ship"));
			else if (pc == pcmod::PathClass::Sim && m.cls == pcmod::PackageClass::Cosmetic) ImGui::TextColored(ColBad(), "%s", TXT("sim file: needs class sim"));
			else if (const char* w = g.vanilla.Where(n); *w) {
				if (r.include) ++replaces;
				ImGui::TextColored(ColWarn(), TXT("REPLACES vanilla (%s): ship the whole file"), *w == 'p' ? TXT("in a .p archive") : TXT("loose file"));
			} else ImGui::TextDisabled("%s", TXT("new file"));
			ImGui::PopID();
		}
		ImGui::EndTable();
		if (del >= 0) g.rows.erase(g.rows.begin() + del);
	}
	if (replaces)
		ImGui::TextColored(ColWarn(), TXT("%d file(s) replace a file of the MBAACC install. That is allowed, but the game then reads ONLY the packaged file: it must be the whole file, not a patch."), replaces);

	// ---- output ----
	ImGui::SetNextItemWidth(-260);
	ImGui::InputText(LBL("Output folder"), &g.outDir);
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Browse..."))) { const std::string d = BrowseForFolderUtf8(g.outDir); if (!d.empty()) g.outDir = d; }
	ImGui::Checkbox(LBL("Also write the .zip (single top-level <id> folder)"), &g.writeZip);
	if (problem.empty()) {
		ImGui::TextColored(ColGood(), TXT("Ready: %zu files, package digest %s"), m.files.size(), pcmod::PackageDigestHex(m).c_str());
		ImGui::TextColored(ColGood(), TXT("Online sim digest %s (%zu sim file(s))"), pcmod::PackageSimDigestHex(m).c_str(), simFiles);
	} else ImGui::TextColored(ColBad(), "%s", problem.c_str());
	ImGui::BeginDisabled(!problem.empty());
	if (ImGui::Button(LBL("Export package"))) Export();
	ImGui::EndDisabled();
	if (!g.digest.empty()) { ImGui::SameLine(); if (ImGui::SmallButton(LBL("Copy digest"))) ImGui::SetClipboardText(g.digest.c_str()); }
	if (!g.simDigest.empty()) { ImGui::SameLine(); if (ImGui::SmallButton(LBL("Copy sim digest"))) ImGui::SetClipboardText(g.simDigest.c_str()); }
	if (!g.result.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, g.resultOk ? ColGood() : ColBad());
		ImGui::TextWrapped("%s", g.result.c_str());
		ImGui::PopStyleColor();
	}
	ImGui::End();
}

bool ScriptCommand(const std::string& cmd, const std::string& argIn, std::string* msg)
{
	if (cmd != "pcmod") return false;
	const size_t sp = argIn.find(' ');
	const std::string sub = argIn.substr(0, sp), arg = sp == std::string::npos ? "" : argIn.substr(sp + 1);
	if (sub == "open") { Open(arg); return true; }
	if (sub == "add") { AddFile(arg, "", true); return true; }
	if (sub == "addfolder") { AddFolder(arg); return true; }
	if (sub == "set") {
		const size_t s2 = arg.find(' ');
		const std::string k = arg.substr(0, s2), v = s2 == std::string::npos ? "" : arg.substr(s2 + 1);
		if (k == "id") g.id = v; else if (k == "version") g.version = v; else if (k == "name") g.name = v;
		else if (k == "author") g.author = v; else if (k == "out") g.outDir = v; else if (k == "exe") g.exe = v;
		else if (k == "priority") g.priority = atoi(v.c_str()); else if (k == "class") g.classMode = v == "sim" ? 1 : v == "cosmetic" ? 2 : 0;
		else if (k == "modes") g.modes = v;
		else { if (msg) *msg = "unknown field " + k; return true; }
		return true;
	}
	if (sub == "export") { Export(); if (msg) *msg = g.result; return true; }
	if (sub == "status") {
		std::string problem; pcmod::PackageClass d;
		size_t sim = 0;
		const pcmod::Manifest m = Build(problem, d, &sim);
		if (msg) *msg = std::to_string(m.files.size()) + " files (" + std::to_string(sim) + " sim), class " + pcmod::ClassName(m.cls) +
			(problem.empty() ? ", digest " + pcmod::PackageDigestHex(m) + ", sim digest " + pcmod::PackageSimDigestHex(m) : ", problem: " + problem);
		return true;
	}
	if (msg) *msg = "pcmod: unknown subcommand " + sub;
	return true;
}

} // namespace pcmodui
