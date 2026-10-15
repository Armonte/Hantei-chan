// See hmod_ui.h.
#include "hmod_ui.h"
#include "hmod_export.h"
#include "hmod_import.h"

#include "../app_log.h"
#include "../filedialog.h"
#include "../i18n.h"
#include "../pcmod/pcmod_vanilla.h"
#include "../png_writer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace hmodui {
namespace {

using namespace hmod;

struct State {
	bool open = false;
	Project proj;
	bool loaded = false;
	std::string projPath, newId = "my-mod", status;
	int newGame = 0;
	std::string gameDir, outDir, knownText;
	bool imPacks = true, writeZip = true;
	VanillaIndex van;
	bool vanReady = false;
	std::string vanMsg;
	// background index build
	std::atomic<bool> building{false};
	std::atomic<size_t> bDone{0}, bTotal{0};
	std::atomic<bool> cancel{false};
	std::thread worker;
	std::mutex mu;
	std::string workerMsg;
	bool workerFinished = false;
	std::unique_ptr<VanillaIndex> built;
	std::vector<TrackedFile> tracked;
	bool trackedDirty = true;
	std::string report;
	bool reportOk = false;
	std::vector<Conflict> conflicts;
	// editing
	std::string newAssetId, newPkgId;
	int newAssetCat = 0;
	int selAsset = -1;
};
State g;

const char* const kGames[] = {"mbaacc", "uni2", "mbtl"};

bool LightTheme() { const ImVec4 b = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg); return b.x + b.y + b.z > 1.5f; }
ImVec4 ColBad() { return LightTheme() ? ImVec4(.75f, .05f, .05f, 1) : ImVec4(1, .4f, .4f, 1); }
ImVec4 ColWarn() { return LightTheme() ? ImVec4(.6f, .3f, 0, 1) : ImVec4(1, .8f, .3f, 1); }
ImVec4 ColGood() { return LightTheme() ? ImVec4(.05f, .45f, .1f, 1) : ImVec4(.5f, 1, .5f, 1); }

ImVec4 StatusColour(Status s)
{
	switch (s) {
	case Status::Changed: return ColWarn();
	case Status::Added: return ColGood();
	case Status::Orphan: return ColBad();
	default: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
	}
}

void JoinWorker() { if (g.worker.joinable()) g.worker.join(); }

void Retrack()
{
	if (!g.loaded) return;
	g.tracked = TrackProject(g.proj, g.vanReady ? &g.van : nullptr);
	g.trackedDirty = false;
}

void TryLoadIndex()
{
	g.vanReady = false;
	if (!g.loaded || g.gameDir.empty()) return;
	std::string err;
	g.vanReady = g.van.LoadCached(g.proj.game, g.gameDir, err);
	g.vanMsg = g.vanReady ? std::to_string(g.van.size()) + " files, packset " + g.van.packset8() : err;
	g.trackedDirty = true;
}

bool LoadProject(const std::string& path)
{
	std::string err;
	Project p;
	if (!Load(path, p, err)) { g.status = std::string(TXT("Cannot open project: ")) + err; HC_LOG_WARN("hmod: {}", err); return false; }
	g.proj = std::move(p);
	g.loaded = true;
	g.projPath = g.proj.dir;
	g.status = std::string(TXT("Opened ")) + g.proj.dir;
	if (g.gameDir.empty() && g.proj.game == "mbaacc") g.gameDir = pcmod::DefaultGameDir();
	g.report.clear(); g.conflicts.clear();
	TryLoadIndex();
	g.trackedDirty = true;
	return true;
}

void StartIndexBuild()
{
	if (g.building || !g.loaded || g.gameDir.empty()) return;
	JoinWorker();
	g.building = true; g.cancel = false; g.bDone = 0; g.bTotal = 0; g.workerFinished = false;
	const std::string game = g.proj.game, dir = g.gameDir;
	g.worker = std::thread([game, dir] {
		auto v = std::make_unique<VanillaIndex>();
		std::string cache, err;
		const bool ok = v->BuildCached(game, dir, cache, err, [](size_t d, size_t t, const std::string&) { g.bDone = d; g.bTotal = t; return !g.cancel.load(); });
		std::lock_guard<std::mutex> lk(g.mu);
		g.workerMsg = ok ? "Indexed " + std::to_string(v->size()) + " files -> " + cache : "Index failed: " + err;
		if (ok) g.built = std::move(v);
		g.workerFinished = true;
	});
}

void PollWorker()
{
	if (!g.workerFinished) return;
	JoinWorker();
	std::lock_guard<std::mutex> lk(g.mu);
	g.workerFinished = false;
	g.building = false;
	g.vanMsg = g.workerMsg;
	HC_LOG_INFO("hmod: {}", g.workerMsg);
	if (g.built) {
		g.van = std::move(*g.built);
		g.built.reset();
		g.vanReady = true;
		g.trackedDirty = true;
	}
}

ExportOptions Options(bool dry)
{
	ExportOptions o;
	o.gameDir = g.gameDir; o.outDir = g.outDir; o.imUsePacks = g.imPacks; o.writeZip = g.writeZip; o.dryRun = dry;
	std::string cur;
	for (char c : g.knownText + "\n") { if (c == '\n' || c == '\r') { if (!cur.empty()) o.knownDirs.push_back(cur); cur.clear(); } else cur += c; }
	return o;
}

void RunExport(const std::string& pkg, bool dry)
{
	if (!g.loaded) return;
	if (g.trackedDirty) Retrack();
	ExportOptions o = Options(dry);
	o.package = pkg;
	const ExportReport r = Export(g.proj, o, g.vanReady ? &g.van : nullptr);
	g.report = ReportText(r);
	g.reportOk = r.ok;
	g.conflicts = r.conflicts;
	HC_LOG_INFO("hmod export{} {}: {}", dry ? " (check)" : "", pkg.empty() ? "(all)" : pkg, r.ok ? "ok" : "refused");
	if (!dry && r.ok) g.trackedDirty = true;
}

void AddAssetFiles(const Asset& a)
{
	const std::string f = FileDialog(-1, false);
	if (f.empty()) return;
	const std::string abs = AnsiToUtf8(f);
	// keep the path below the first game root segment when there is one, else under data\ .
	std::string rel = fs::u8path(abs).filename().u8string();
	std::string p = abs;
	for (char& c : p) if (c == '/') c = '\\';
	for (const char* root : {"\\data\\", "\\grp\\", "\\story\\", "\\se\\", "\\bgm\\", "\\bg\\", "\\palettes\\", "\\script\\", "\\dat\\"}) {
		std::string low = p;
		for (char& c : low) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
		const size_t at = low.rfind(root);
		if (at != std::string::npos) { rel = p.substr(at + 1); break; }
	}
	std::error_code ec;
	const fs::path d = fs::u8path(g.proj.AssetDir(a.id)) / fs::u8path(rel);
	fs::create_directories(d.parent_path(), ec);
	fs::copy_file(fs::u8path(abs), d, fs::copy_options::overwrite_existing, ec);
	g.status = ec ? ec.message() : std::string(TXT("Added ")) + rel;
	g.trackedDirty = true;
}

} // namespace

void Open() { g.open = true; }
void MenuItem()
{
	if (ImGui::MenuItem(LBL("Mod Project..."))) Open();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", TXT("Keep a mod as a project (hmod.json): tracked against vanilla, checked for conflicts,\nexported as a pcmod package (MBAACC) or a UNI2-IM / MBTL-IM mod folder (UNI2 / MBTL)."));
}

void Draw()
{
	PollWorker();
	if (!g.open) return;
	const ImVec2 mp = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mp.x + 90, mp.y + 60), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(1040, 740), ImGuiCond_Appearing);
	if (!ImGui::Begin(LBL("Mod Project###hmod_panel"), &g.open, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }

	// ---- open / new ----
	ImGui::SetNextItemWidth(420);
	ImGui::InputText(LBL("Project folder"), &g.projPath);
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Browse..."))) { const std::string d = BrowseForFolderUtf8(g.projPath); if (!d.empty()) g.projPath = d; }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Open"))) LoadProject(g.projPath);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(130); ImGui::InputText(LBL("Id##newid"), &g.newId);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90); ImGui::Combo(LBL("Game##newgame"), &g.newGame, kGames, 3);
	ImGui::SameLine();
	if (ImGui::Button(LBL("Create"))) {
		Project p; std::string err;
		if (Create(g.projPath, g.newId, kGames[g.newGame], p, err)) LoadProject(g.projPath); else g.status = err;
	}
	if (!g.status.empty()) ImGui::TextDisabled("%s", g.status.c_str());
	if (!g.loaded) { ImGui::TextWrapped("%s", TXT("Open a project folder holding hmod.json, or enter a folder and an id and press Create.")); ImGui::End(); return; }

	Project& p = g.proj;
	ImGui::Separator();
	ImGui::SetNextItemWidth(260); if (ImGui::InputText(LBL("Name"), &p.name)) {}
	ImGui::SameLine(); ImGui::SetNextItemWidth(200); ImGui::InputText(LBL("Author"), &p.author);
	ImGui::SameLine(); ImGui::TextDisabled("%s: %s", TXT("Game"), p.game.c_str());
	ImGui::SameLine();
	if (ImGui::Button(LBL("Save hmod.json"))) { std::string err; g.status = Save(p, err) ? std::string(TXT("Saved")) : err; }

	// ---- game install + vanilla index ----
	ImGui::SetNextItemWidth(420);
	if (ImGui::InputText(LBL("Game install"), &g.gameDir)) TryLoadIndex();
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Browse...##game"))) { const std::string d = BrowseForFolderUtf8(g.gameDir); if (!d.empty()) { g.gameDir = d; TryLoadIndex(); } }
	ImGui::SameLine();
	ImGui::BeginDisabled(g.building || g.gameDir.empty());
	if (ImGui::Button(LBL("Build vanilla index"))) StartIndexBuild();
	ImGui::EndDisabled();
	if (g.building) {
		ImGui::SameLine();
		const size_t t = g.bTotal;
		ImGui::ProgressBar(t ? float(g.bDone) / float(t) : 0.f, ImVec2(180, 0));
		ImGui::SameLine(); if (ImGui::SmallButton(LBL("Cancel"))) g.cancel = true;
	}
	ImGui::TextColored(g.vanReady ? ColGood() : ColWarn(), "%s %s", TXT("Vanilla index:"), g.vanMsg.empty() ? TXT("not built (files are treated as changed)") : g.vanMsg.c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Hashes every file of the game's archives once (minutes on a slow disk). The index is cached and shared by all projects."));
	ImGui::BeginDisabled(!g.vanReady || p.game != "mbaacc");
	if (ImGui::Button(LBL("Import a modded install..."))) {
		// The vanilla index above is the baseline: its loose files that differ (or are new) become install-import assets.
		const std::string d = BrowseForFolderUtf8(g.gameDir);
		if (!d.empty()) {
			ImportInstallResult r;
			std::string err;
			if (ImportInstall(p, d, g.van, r, err) && Save(p, err)) {
				char buf[256];
				snprintf(buf, sizeof buf, TXT("Imported %zu file(s) into %zu asset(s); %zu unchanged, %zu re-encoded, %zu skipped."),
					r.imported, r.perAsset.size(), r.unchanged, r.reencoded, r.skipped);
				g.status = buf;
				for (const auto& n : r.notes) HC_LOG_INFO("[hmod] import: {}", n);
				g.trackedDirty = true;
			} else g.status = err;
		}
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", TXT("Pick a modded MBAACC install: every loose file that differs from the vanilla index (or is new) is copied into this project, one asset per character plus system, roster, story, sound, music, stage and graphics assets. The install is only read."));

	// ---- assets ----
	ImGui::Separator();
	if (g.trackedDirty) Retrack();
	const StatusCounts c = Count(g.tracked);
	ImGui::Text(TXT("Changes: %zu changed, %zu added, %zu unchanged, %zu re-encoded, %zu orphan, %zu clutter"), c.changed, c.added, c.unchanged, c.reencoded, c.orphan, c.clutter);
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Refresh"))) g.trackedDirty = true;
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Record vanilla bases")) ) { FillBases(p, g.tracked); std::string err; g.status = Save(p, err) ? std::string(TXT("Saved")) : err; }

	const float h = 230.f;
	if (ImGui::BeginChild("##assets", ImVec2(300, h), true)) {
		for (size_t i = 0; i < p.assets.size(); ++i) {
			const Asset& a = p.assets[i];
			size_t ship = 0;
			for (const TrackedFile& t : g.tracked) if (t.asset == a.id && Ships(t.status)) ++ship;
			const std::string label = a.id + "  (" + std::to_string(ship) + ")###asset" + std::to_string(i);
			if (ImGui::Selectable(label.c_str(), g.selAsset == (int)i)) g.selAsset = (int)i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", a.category.c_str(), a.origin.kind.c_str());
		}
		ImGui::Separator();
		ImGui::SetNextItemWidth(120); ImGui::InputText("##newasset", &g.newAssetId);
		ImGui::SameLine();
		std::vector<const char*> cats;
		for (const CategoryInfo& ci : Categories()) cats.push_back(ci.id);
		ImGui::SetNextItemWidth(140); ImGui::Combo("##newcat", &g.newAssetCat, cats.data(), (int)cats.size());
		if (ImGui::SmallButton(LBL("Add asset"))) {
			Asset a; a.id = g.newAssetId; a.category = cats[g.newAssetCat]; a.origin.kind = "vanilla-derived";
			p.assets.push_back(a);
			std::string err;
			if (!Validate(p).empty()) { g.status = Validate(p); p.assets.pop_back(); }
			else { std::error_code ec; fs::create_directories(fs::u8path(p.AssetDir(a.id)), ec); Save(p, err); g.newAssetId.clear(); g.trackedDirty = true; }
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("##files", ImVec2(0, h), true)) {
		if (g.selAsset >= 0 && g.selAsset < (int)p.assets.size()) {
			const Asset& a = p.assets[g.selAsset];
			if (ImGui::SmallButton(LBL("Add file to asset..."))) AddAssetFiles(a);
			ImGui::SameLine();
			if (ImGui::SmallButton(LBL("Open asset folder"))) ShellExecuteW(nullptr, L"open", fs::u8path(p.AssetDir(a.id)).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			if (ImGui::BeginTable("##tf", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn(TXT("Status"), ImGuiTableColumnFlags_WidthFixed, 80);
				ImGui::TableSetupColumn(TXT("Game path"), ImGuiTableColumnFlags_WidthStretch, 2.f);
				ImGui::TableSetupColumn(TXT("Size"), ImGuiTableColumnFlags_WidthFixed, 80);
				ImGui::TableSetupColumn(TXT("Notes"), ImGuiTableColumnFlags_WidthStretch, 2.f);
				ImGui::TableHeadersRow();
				for (const TrackedFile& t : g.tracked) {
					if (t.asset != a.id) continue;
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextColored(StatusColour(t.status), "%s", StatusName(t.status));
					ImGui::TableNextColumn(); ImGui::TextUnformatted(t.rel.c_str());
					ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)t.size);
					ImGui::TableNextColumn(); ImGui::TextUnformatted(t.note.c_str());
				}
				ImGui::EndTable();
			}
		} else ImGui::TextDisabled("%s", TXT("Select an asset to see its files and their change status."));
	}
	ImGui::EndChild();

	// ---- packages + export ----
	ImGui::Separator();
	ImGui::Text("%s", TXT("Packages"));
	for (const Package& k : p.packages) {
		ImGui::PushID(k.id.c_str());
		ImGui::BulletText("%s %s", k.id.c_str(), k.version.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton(LBL("Check"))) RunExport(k.id, true);
		ImGui::SameLine();
		const bool pc = p.game == "mbaacc";
		if (ImGui::SmallButton(pc ? LBL("Export pcmod") : LBL("Save to mod folder"))) RunExport(k.id, false);
		ImGui::PopID();
	}
	ImGui::SetNextItemWidth(130); ImGui::InputText(LBL("New package id"), &g.newPkgId);
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Add package")) && !g.newPkgId.empty()) {
		Package k; k.id = g.newPkgId;
		for (const Asset& a : p.assets) k.assets.push_back(a.id);
		p.packages.push_back(k);
		if (!Validate(p).empty()) { g.status = Validate(p); p.packages.pop_back(); } else { std::string err; Save(p, err); g.newPkgId.clear(); }
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Check all"))) RunExport("", true);
	ImGui::SameLine();
	if (ImGui::Button(p.game == "mbaacc" ? LBL("Export all (pcmod)") : LBL("Save all to mod folder"))) RunExport("", false);

	if (p.game == "mbaacc") {
		ImGui::SetNextItemWidth(420); ImGui::InputText(LBL("Output folder"), &g.outDir);
		ImGui::SameLine(); ImGui::Checkbox(LBL("Also write the .zip"), &g.writeZip);
	} else {
		ImGui::Checkbox(LBL("Switchable pack (Packs\\<id>) instead of Mods"), &g.imPacks);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("A pack appears in the IM overlay's Mods list with a switch; Mods\\ is always on."));
	}
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextMultiline("##known", &g.knownText, ImVec2(-1, 36));
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Extra package folders to check for conflicts (one per line), besides the installed mods."));

	if (!g.report.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, g.reportOk ? ColGood() : ColBad());
		ImGui::TextUnformatted(g.reportOk ? TXT("Result: ok") : TXT("Result: REFUSED (nothing was written)"));
		ImGui::PopStyleColor();
		ImGui::BeginChild("##report", ImVec2(0, 0), true);
		for (const Conflict& cf : g.conflicts) ImGui::TextColored(cf.blocks ? ColBad() : ColWarn(), "%s", DescribeConflict(cf).c_str());
		ImGui::TextWrapped("%s", g.report.c_str());
		ImGui::EndChild();
	}
	ImGui::End();
}

bool ScriptCommand(const std::string& cmd, const std::string& argIn, std::string* msg)
{
	if (cmd != "hmod") return false;
	const size_t sp = argIn.find(' ');
	const std::string sub = argIn.substr(0, sp), arg = sp == std::string::npos ? "" : argIn.substr(sp + 1);
	auto say = [&](const std::string& s) { if (msg) *msg = s; };
	if (sub == "open") { g.open = true; LoadProject(arg); say(g.status); return true; }
	if (sub == "new") {
		std::vector<std::string> w;
		for (size_t s = 0; s <= arg.size();) { const size_t e = arg.find(' ', s); w.push_back(arg.substr(s, e == std::string::npos ? std::string::npos : e - s)); if (e == std::string::npos) break; s = e + 1; }
		Project p; std::string err;
		if (w.size() != 3 || !Create(w[0], w[1], w[2], p, err)) { say(w.size() != 3 ? "hmod new <dir> <id> <game>" : err); return true; }
		g.open = true; LoadProject(w[0]); say(g.status);
		return true;
	}
	if (sub == "gamedir") { g.gameDir = arg; TryLoadIndex(); say(g.vanMsg); return true; }
	if (sub == "out") { g.outDir = arg; return true; }
	if (sub == "mods") { g.imPacks = arg != "1"; return true; }
	if (sub == "zip") { g.writeZip = arg != "0"; return true; }
	if (sub == "known") { g.knownText += arg + "\n"; return true; }
	if (sub == "index") { StartIndexBuild(); if (g.worker.joinable()) { g.worker.join(); g.workerFinished = true; } PollWorker(); say(g.vanMsg); return true; }
	if (sub == "status") {
		if (g.trackedDirty) Retrack();
		const StatusCounts c = Count(g.tracked);
		say(std::to_string(c.changed) + " changed, " + std::to_string(c.added) + " added, " + std::to_string(c.unchanged) + " unchanged, " + std::to_string(c.reencoded) + " re-encoded, " +
			std::to_string(c.orphan) + " orphan, " + std::to_string(c.clutter) + " clutter");
		return true;
	}
	if (sub == "check" || sub == "export") {
		RunExport(arg, sub == "check");
		say(std::string(g.reportOk ? "ok" : "refused") + ": " + g.report);
		return true;
	}
	say("hmod: unknown subcommand " + sub);
	return true;
}

} // namespace hmodui
