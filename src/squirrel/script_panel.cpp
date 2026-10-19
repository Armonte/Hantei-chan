#include "script_panel.h"
#include "../ui/ui_style.h"
#include "../ui/ui_layout2.h"

#include "script_document.h"
#include "script_index.h"
#include "ptcn_stack.h"
#include "script_vm.h"
#include "fpanim_link.h"
#include "script_layers.h"
#include "../fbarc/fb_archive.h"

#include "../app_log.h"
#include "../i18n.h"
#include "../ini.h"
#include "../png_writer.h" // BrowseForFolderUtf8

#include <ImGuiColorTextEdit/TextEditor.h>
#include "squirrel/script_outline.h"
#include <imgui.h>
#include <imgui_internal.h> // MarkIniSettingsDirty (hot reload settings)
#include <json/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <regex>
#include <set>
#include <vector>

#include <windows.h>

namespace fs = std::filesystem;

namespace sqedit {

namespace {

fs::path U8(const std::string& s) { return fs::u8path(s); }
std::string ToU8(const fs::path& p) { return p.generic_u8string(); }

bool ReadBytes(const fs::path& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return !f.bad();
}

// Temp file next to the target, then rename over it (MoveFileExW REPLACE_EXISTING in libstdc++).
bool WriteAtomic(const fs::path& p, const std::string& bytes, std::string* error)
{
	fs::path tmp = p;
	tmp += ".tmp" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f || !f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
			if (error) *error = "cannot write " + ToU8(tmp);
			return false;
		}
	}
	std::error_code ec;
	fs::rename(tmp, p, ec);
	if (ec) {
		fs::remove(tmp, ec);
		if (error) *error = "cannot replace " + ToU8(p);
		return false;
	}
	return true;
}

// <dir>/.hantei-backups/<name>.<YYYYmmdd-HHMMSS>[-n].bak, never overwriting (same scheme as the cmdfile editor).
bool WriteBackup(const fs::path& target, const std::string& bytes, std::string* backup)
{
	std::error_code ec;
	const fs::path dir = target.parent_path() / ".hantei-backups";
	fs::create_directories(dir, ec);
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &now);
	char stamp[32];
	std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
	for (int n = 0; n < 1000; ++n) {
		fs::path b = dir / (target.filename().u8string() + "." + stamp + (n ? "-" + std::to_string(n) : "") + ".bak");
		if (fs::exists(b)) continue;
		std::ofstream f(b, std::ios::binary);
		if (!f || !f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return false;
		if (backup) *backup = ToU8(b);
		return true;
	}
	return false;
}

// Squirrel 2.2.4 grammar for the widget: C-like, keywords from sqlexer.cpp, '#' line comments.
const TextEditor::Language* SquirrelLanguage()
{
	static TextEditor::Language lang = [] {
		TextEditor::Language l = *TextEditor::Language::C();
		l.name = "Squirrel 2.2.4";
		l.preprocess = 0;
		l.singleLineComment = "//";
		l.singleLineCommentAlt = "#";
		l.commentStart = "/*";
		l.commentEnd = "*/";
		l.hasSingleQuotedStrings = true;
		l.hasDoubleQuotedStrings = true;
		l.otherStringStart = "@\"";
		l.otherStringEnd = "\"";
		l.stringEscape = '\\';
		l.keywords.clear();
		l.declarations.clear();
		l.identifiers.clear();
		for (const char* k : {"while", "do", "if", "else", "break", "continue", "return", "null", "function", "local", "for",
				 "foreach", "in", "typeof", "delegate", "delete", "try", "catch", "throw", "clone", "yield", "resume",
				 "switch", "case", "default", "this", "parent", "class", "extends", "constructor", "instanceof", "vargc",
				 "vargv", "true", "false", "static", "enum", "const"})
			l.keywords.insert(k);
		for (const char* d : {"BMvTbl", "BMvEff", "BMvCore", "BtlMvStd", "Battle_Std", "BCMDTbl", "BCom", "BSound", "BMvConst",
				 "BMvCoreConst", "BCMDConst", "vConst", "debuglog", "__dofile__", "SQ_LoadString"})
			l.declarations.insert(d);
		for (const char* i : {"print", "assert", "array", "type", "getroottable", "getconsttable", "compilestring",
				 "newthread", "suspend", "collectgarbage", "seterrorhandler", "setdebughook", "format", "split", "strip",
				 "abs", "fabs", "sqrt", "rand", "srand", "floor", "ceil", "sin", "cos", "clock", "time", "date", "blob"})
			l.identifiers.insert(i);
		return l;
	}();
	return &lang;
}

} // namespace

std::string GuessGame(const std::string& rootUtf8)
{
	std::string low = rootUtf8;
	std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const fs::path r = U8(rootUtf8);
	std::error_code ec;
	if (fs::exists(r / "MBTL.exe", ec) || low.find("mbtl") != std::string::npos || low.find("melty blood type lumina") != std::string::npos) return "mbtl";
	if (fs::exists(r / "uni2.exe", ec) || low.find("uni2") != std::string::npos) return "uni2";
	// The arcade builds both run RingGame.exe; their JVS config tools tell them apart (JConfigFC = Fighting Climax Ignition,
	// JConfigUN = UNDER NIGHT IN-BIRTH [st]/[cl-r]).
	if (fs::exists(r / "JConfigFC.exe", ec) || low.find("dfci") != std::string::npos || low.find("fighting climax") != std::string::npos) return "dfci";
	if (fs::exists(r / "JConfigUN.exe", ec) || low.find("unib") != std::string::npos) return "unib";
	return "";
}

std::string FindApiProfile(const std::string& game)
{
	if (game.empty()) return "";
	const std::string name = game + "_api.json";
	std::vector<fs::path> bases{fs::current_path()};
	wchar_t exe[1024] = {};
	if (GetModuleFileNameW(nullptr, exe, 1024)) bases.push_back(fs::path(exe).parent_path());
	for (const auto& b : bases) {
		for (fs::path d = b; !d.empty(); d = d.parent_path()) {
			for (const char* sub : {"res/squirrel", "docs/squirrel/api"}) {
				std::error_code ec;
				const fs::path p = d / sub / name;
				if (fs::exists(p, ec)) return ToU8(p);
			}
			if (d == d.parent_path()) break;
		}
	}
	return "";
}

// One native from the game's API profile, for hover and completion.
struct ApiSym {
	std::string table, ret;
	std::vector<std::string> args;
};

std::string Signature(const std::string& name, const ApiSym& a)
{
	std::string s = (a.table.empty() || a.table[0] == '(' ? "" : a.table + ".") + name + "(";
	for (size_t i = 0; i < a.args.size(); i++) s += (i ? ", " : "") + a.args[i];
	return s + ") -> " + (a.ret.empty() ? "?" : a.ret);
}

using PatRef = ScriptPanel::PatRef;

// What each engine table is for (modder notes, BootySama 2026-10-15); script tables are labelled from the index.
const char* EngineTableNote(const std::string& t)
{
	if (t == "BMvCore") return "engine: core battle data (P1/P2, stages and other essentials)";
	if (t == "BMvTbl") return "engine: move data holder (edit, compare and check values for MVs, stage status)";
	if (t == "BMvEff") return "engine: effects and their special flags";
	if (t == "BCMDTbl") return "engine: command input checks";
	return "engine native (from the game executable)";
}

// Text colours that stay readable on both the dark and the light (cream) theme.
bool LightTheme()
{
	const ImVec4 b = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
	return 0.299f * b.x + 0.587f * b.y + 0.114f * b.z > 0.5f;
}
ImVec4 EngineCol() { return LightTheme() ? ImVec4(0.1f, 0.35f, 0.75f, 1) : ImVec4(0.45f, 0.7f, 1, 1); }
ImVec4 ScriptCol() { return LightTheme() ? ImVec4(0.1f, 0.5f, 0.15f, 1) : ImVec4(0.5f, 0.9f, 0.5f, 1); }
ImVec4 WarnCol() { return LightTheme() ? ImVec4(0.65f, 0.38f, 0, 1) : ImVec4(1, 0.8f, 0.4f, 1); }
ImVec4 ErrCol() { return LightTheme() ? ImVec4(0.75f, 0.1f, 0.05f, 1) : ImVec4(1, 0.45f, 0.4f, 1); }
ImVec4 ArgCol() { return LightTheme() ? ImVec4(0.75f, 0.35f, 0, 1) : ImVec4(1, 0.85f, 0.3f, 1); }

struct ScriptPanel::Impl {
	TextEditor editor;
	ScriptDocument doc;
	ScriptVM vm;
	std::string root, game, apiPath;   // root: the game install (script_layers.h ResolveScriptRoot)
	std::vector<std::string> files; // relative, generic (winning copies)
	// Merged list over fpan-im / IM mods and packs, loose files and the archive (script_layers.h).
	std::vector<ScriptLayer> layers;
	std::vector<ScriptFile> sfiles;
	std::map<std::string, size_t> sfileByKey;   // lower-case rel -> sfiles index
	std::string currentPath;                     // the file the editor holds (the copy that was opened)
	std::string editRoot, editTag;               // a pack / mod folder chosen as the game folder: every save goes there
	std::string profileArg;                      // the Profile choice ("" = auto)
	std::unique_ptr<fbarc::Archive> archive;
	std::string archiveCache;                    // %TEMP%/hantei_archive/<game>_scripts
	std::string watchSig;                        // folder signature of the last scan (rescan when it changes)
	double lastWatch = 0.0;
	bool wasFocused = false;

	static std::string LowerS(std::string v) { for (char& c : v) c = static_cast<char>(tolower(static_cast<unsigned char>(c))); return v; }
	// The file of one copy; an archive copy is extracted once into the cache folder.
	std::string pathOfCopy(const ScriptFile& f, const ScriptCopy& c)
	{
		if (!c.path.empty()) return c.path;
		if (!archive || archiveCache.empty()) return "";
		const fs::path dst = U8(archiveCache) / U8(f.rel);
		std::error_code ec;
		if (fs::exists(dst, ec)) return ToU8(dst);
		const auto& es = archive->entries();
		int idx = -1;
		for (size_t i = 0; i < es.size() && idx < 0; i++) {
			std::string r = fbarc::NameToUtf8(archive->relativePath(i));
			for (char& ch : r) if (ch == '\\') ch = '/';
			while (r.rfind("./", 0) == 0) r.erase(0, 2);
			if (LowerS(r) == LowerS(f.rel)) idx = static_cast<int>(i);
		}
		std::vector<uint8_t> plain;
		std::string err;
		if (idx < 0 || !archive->read(static_cast<size_t>(idx), plain, &err)) { say("archive: cannot read " + f.rel + (err.empty() ? "" : ": " + err)); return ""; }
		fs::create_directories(dst.parent_path(), ec);
		if (!WriteAtomic(dst, std::string(plain.begin(), plain.end()), &err)) { say("archive: " + err); return ""; }
		return ToU8(dst);
	}
	// The file the game reads for a game-relative path ("" = not listed).
	std::string pathOf(const std::string& rel)
	{
		const auto it = sfileByKey.find(LowerS(rel));
		if (it == sfileByKey.end()) return "";
		const ScriptFile& f = sfiles[it->second];
		return f.winner < 0 ? "" : pathOfCopy(f, f.copies[static_cast<size_t>(f.winner)]);
	}
	std::string pathOrRoot(const std::string& rel) { const std::string p = pathOf(rel); return p.empty() ? ToU8(U8(root) / U8(rel)) : p; }
	static bool Under(const std::string& path, const std::string& dir)
	{
		if (dir.empty()) return false;
		const std::string a = LowerS(ToU8(U8(path).lexically_normal())), b = LowerS(ToU8(U8(dir).lexically_normal()));
		return a.size() > b.size() && a.compare(0, b.size(), b) == 0 && a[b.size()] == '/';
	}
	const ScriptLayer* layerOfPath(const std::string& path) const
	{
		const ScriptLayer* best = nullptr;
		for (const ScriptLayer& l : layers)
			if (!l.root.empty() && Under(path, l.root) && (!best || l.root.size() > best->root.size())) best = &l;
		return best;
	}
	// Where a save of `rel` goes: the chosen pack / mod folder; else the opened copy, except an archive copy, which goes
	// to the IM mod folder (UNI2-IM\Mods / MBTL-IM\Mods) or, for other games, loose under the game folder.
	std::string saveTargetFor(const std::string& rel, const std::string& openedPath)
	{
		if (!editRoot.empty()) return ToU8(U8(editRoot) / U8(rel));
		if (!openedPath.empty() && !(archiveCache.size() && Under(openedPath, archiveCache))) return openedPath;
		const std::string im = FpanimImModsRoot(game, root);
		return ToU8(U8(im.empty() ? root : im) / U8(rel));
	}
	// Folder signature for the cheap watch: the write times of every layer folder and its data / script directories.
	std::string folderSignature() const
	{
		std::string sig;
		std::error_code ec;
		auto stamp = [&](const fs::path& d) {
			std::error_code es;
			const auto t = fs::last_write_time(d, es);
			if (!es) sig += std::to_string(static_cast<long long>(t.time_since_epoch().count())) + ";";
		};
		for (const char* d : {"fpan-im/mods", "fpan-im/packs", "UNI2-IM", "UNI2-IM/Packs", "UNI2-IM/Mods", "MBTL-IM", "MBTL-IM/Packs", "MBTL-IM/Mods"}) stamp(U8(root) / d);
		for (const ScriptLayer& l : layers) {
			if (l.root.empty()) continue;
			stamp(U8(l.root));
			for (const char* sub : {"script", "data"}) {
				const fs::path d = U8(l.root) / sub;
				if (!fs::is_directory(d, ec)) continue;
				for (auto it = fs::recursive_directory_iterator(d, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
					std::error_code e2;
					if (it->is_directory(e2)) {
						if (it->path().filename() == ".hantei-backups") { it.disable_recursion_pending(); continue; }
						stamp(it->path());
					}
				}
				stamp(d);
			}
		}
		return sig;
	}
	// Rebuilds the merged list and the index (new files appear; the open file and its unsaved text stay).
	void rescan(bool quiet)
	{
		layers = ScriptLayersFor(root, game);
		std::vector<std::string> arch;
		if (archive)
			for (size_t i = 0; i < archive->entries().size(); i++) arch.push_back(fbarc::NameToUtf8(archive->relativePath(i)));
		sfiles = ListScriptFiles(layers, arch);
		sfileByKey.clear();
		files.clear();
		for (size_t i = 0; i < sfiles.size(); i++) {
			sfileByKey[LowerS(sfiles[i].rel)] = i;
			if (sfiles[i].winner >= 0) files.push_back(sfiles[i].rel);
		}
		index.build(root, files, [this](const std::string& rel) { return pathOf(rel); });
		if (dirty && !currentRel.empty()) index.updateFile(currentRel, editor.GetText());
		indexGen++;
		watchSig = folderSignature();
		if (!quiet) say("rescanned: " + std::to_string(files.size()) + " Squirrel scripts in " + std::to_string(layers.size()) + " layer(s)");
	}
	void watchFolders(bool focused)
	{
		const double now = ImGui::GetTime();
		const bool gained = focused && !wasFocused;
		wasFocused = focused;
		if (root.empty() || (!gained && now - lastWatch < 2.0)) return;
		lastWatch = now;
		const std::string sig = folderSignature();
		if (sig != watchSig) rescan(false);
	}
	std::string filter;
	std::string currentRel;
	bool dirty = false;
	bool vmReady = false;
	std::string status;
	bool statusError = false;
	bool offerSaveAnyway = false;
	bool forceNextSave = false;
	std::deque<std::string> log;
	ScriptPanel* owner = nullptr;
	// Project index (search, definitions, usages, member completion, move checks) and the bottom panes.
	ScriptIndex index;
	int indexGen = 0;   // bumped whenever the index changes (reverse-link cache key)
	std::string refsKey;
	std::vector<ScriptPanel::ScriptRef> refsCache;
	std::map<std::string, std::vector<std::pair<std::string, const ApiSym*>>> apiByTable;   // engine natives per table
	std::vector<ScriptProblem> problems;
	std::vector<ScriptProblem> loadProblems;   // from the last character load check (save / Check), MBTL
	std::string loadProblemsDir;
	std::set<std::string> realMoves;   // keys of the <CHR>_MoveTable the last character load built (loadProblemsDir)
	double lastEditTime = -1.0;
	int bottomTab = 0;            // 0 log, 1 problems, 2 search
	bool selectBottomTab = false;
	// `sq hoverat <line> <col>` (ui-script): show the hover tooltip for that text position, as if the mouse rested there.
	bool scriptHover = false;
	TextEditor::DocPos scriptHoverPos;
	char searchBuf[256] = {};
	SearchOptions searchOpt;
	std::vector<ScriptHit> hits;
	std::vector<std::pair<std::string, PatRef>> ha6Hits;
	std::string hitsTitle;
	bool focusSearch = false;
	std::vector<const ScriptDef*> defChoices;   // go-to-definition with several targets
	bool openDefChoices = false;
	std::vector<ScriptProblem> pendingMissing;  // save prompt: referenced moves that do not exist
	bool askMissingMoves = false, skipMovePromptOnce = false;
	// Outline (P5): command entries of a _cmd_ table, t.Mv_* move blocks, functions. Rebuilt on change.
	std::vector<OutlineItem> outline;
	bool outlineDirty = true;
	std::string outlineFilter;
	std::set<std::string> outlineOpen;   // expanded groups, by group name + first line
	std::vector<OutlineRow> outlineRows;

	// Hot reload through the fpan-im in-game pipe (fpanim_link.h). Pipe calls run on a worker (a reload reply waits for the
	// next battle frame); the UI polls hotJob each frame. A save while a job runs replaces hotPending (latest wins).
	struct HotTask {
		bool reload = false;   // false: ping only
		bool live = false;     // reloadchara <chr> live (in-place, keeps state) instead of the engine rebuild
		bool shared = false;   // a shared battle file (script/*, data/VectorTable...): reloadstd [live]
		bool copy = true;      // false: the file already sits in a folder fpan-im serves (its mods / packs, the IM's Mods / Packs)
		std::string pipe, gameDir, srcFile, chr, fileName, rel;
	};
	struct HotResult {
		bool ok = false;
		std::string text;
		bool reload = false;      // a reloadchara reply (info is valid)
		FpanimReloadInfo info;
		std::string chr;
	};
	std::future<HotResult> hotJob;
	bool hotBusy = false, hasHotPending = false;
	HotTask hotPending;
	std::string hotStatus;
	bool hotStatusOk = false;
	bool hotHasInfo = false;            // hotInfo/hotChr describe the last reloadchara reply
	FpanimReloadInfo hotInfo;
	std::string hotChr;
	std::string hotPipe, hotPipeGame;   // pipe name; reset to the profile's default when the profile changes
	std::string hotGameDirOther;        // game folder for profiles without a saved setting

	static std::string DefaultGameDir(const std::string& g)
	{
		if (g == "mbtl") return "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA";
		return "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes";
	}
	std::string& hotGameDirSetting()
	{
		return game == "mbtl" ? gSettings.sqGameDirMbtl : game == "uni2" ? gSettings.sqGameDirUni2 : hotGameDirOther;
	}
	std::string hotGameDir() { const std::string& d = hotGameDirSetting(); return d.empty() ? DefaultGameDir(game) : d; }

	static HotResult RunHotTask(const HotTask& t)
	{
		const FpanimReply ping = FpanimRequest(t.pipe, "ping", 3000);
		if (!ping.connected) return {false, "game not running (no fpan-im pipe)"};
		if (!ping.ok) return {false, "ping: " + ping.text};
		if (!t.reload) return {true, ping.text};
		std::string mods = FpanimModsDir(ping.text);
		if (mods.empty()) mods = ToU8(U8(t.gameDir) / "fpan-im" / "mods");
		if (t.copy) {
		// The file goes to the same game-relative path under the mods folder (fpan-im's VFS serves it from there).
		const std::string dstU8 = FpanimModsTarget(mods, t.rel);
		if (dstU8.empty()) return {false, "not a game-relative path: " + t.rel};
		const fs::path dst = U8(dstU8);
		std::error_code ec;
		fs::create_directories(dst.parent_path(), ec);
		if (ec) return {false, "cannot create " + ToU8(dst.parent_path()) + ": " + ec.message()};
		fs::copy_file(U8(t.srcFile), dst, fs::copy_options::overwrite_existing, ec);
		if (ec) return {false, "cannot copy to " + ToU8(dst) + ": " + ec.message()};
		HC_LOG_INFO("[script] hot reload: copied {} to {}", t.srcFile, ToU8(dst));
		} else HC_LOG_INFO("[script] hot reload: {} is served by fpan-im from its own folder (no copy)", t.srcFile);
		// Engine rebuild: fpan-im waits up to 120 s for the training battle to tick again; live: up to 30 s.
		const std::string req = t.shared ? FpanimStdReloadRequest(t.live) : FpanimReloadRequest(t.chr, t.live);
		const FpanimReply r = FpanimRequest(t.pipe, req, t.live ? 35000 : 130000);
		if (!r.connected) return {false, "game not running (no fpan-im pipe)"};
		HotResult out{r.ok, r.text};
		out.reload = true;
		out.info = FpanimParseReloadReply(r.text);
		out.chr = t.chr;
		return out;
	}
	void startHot(const HotTask& t)
	{
		if (hotBusy) { hotPending = t; hasHotPending = true; return; }
		hotBusy = true;
		hotStatus = t.reload ? (t.live ? "reloading " : "rebuilding ") + (t.shared ? std::string("shared scripts") : t.chr) + "..." : "ping...";
		hotStatusOk = true;
		hotHasInfo = false;
		hotJob = std::async(std::launch::async, RunHotTask, t);
	}
	void pollHot()
	{
		if (!hotBusy || hotJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
		const HotResult r = hotJob.get();
		hotBusy = false;
		hotStatus = r.text;
		hotStatusOk = r.ok;
		hotHasInfo = r.reload;
		hotInfo = r.info;
		hotChr = r.chr;
		say("hot reload: " + r.text);
		if (hasHotPending) { hasHotPending = false; startHot(hotPending); }
	}
	// After a save: the file (relative to the opened game root) goes to the same path under <mods>, then
	//  - data/chrNNN/<file>: reloadchara chrNNN [live];
	//  - a shared battle file (every script/*.txt, data/_combase / VectorTable / BattleInfo / BoundEff.txt), which the
	//    game loads once per process: reloadstd [live] (re-runs them, then rebuilds every character against them).
	void hotReloadSaved()
	{
		if (!gSettings.sqHotReload) return;
		static const std::regex re(R"(^data/(chr\d+)/([^/]+)$)");
		std::smatch mm;
		HotTask t;
		t.shared = FpanimIsSharedFile(currentRel);
		if (!t.shared && !std::regex_match(currentRel, mm, re)) {
			hotStatus = "not a data/chrNNN or shared script/data file: not reloaded";
			hotStatusOk = false;
			return;
		}
		t.reload = true;
		t.live = gSettings.sqHotReloadMode == 1;
		t.pipe = hotPipe;
		t.gameDir = hotGameDir();
		t.srcFile = currentPath.empty() ? ToU8(U8(root) / U8(currentRel)) : currentPath;
		t.rel = currentRel;   // the data/... key: a pack / mod copy reloads like the archived file it replaces
		if (const ScriptLayer* l = layerOfPath(t.srcFile)) t.copy = !(l->served && l->enabled);
		if (!t.shared) {
			t.chr = mm[1].str();
			t.fileName = mm[2].str();
		}
		startHot(t);
	}
	void drawHotReload()
	{
		pollHot();
		if (hotPipeGame != game || hotPipe.empty()) { hotPipeGame = game; hotPipe = FpanimPipeForGame(game); }
		if (ImGui::Checkbox(LBL("Hot reload"), &gSettings.sqHotReload)) ImGui::MarkIniSettingsDirty();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("On save, copy the file into the game's fpan-im mods folder and reload it in the running game (fpan-im pipe). A data/chrNNN file reloads that character; a shared file (script/*.txt, data/_combase, VectorTable, BattleInfo, BoundEff) re-runs the shared battle scripts and tables, then reloads every character."));
		if (!gSettings.sqHotReload) return;
		// The row wraps before an item that no longer fits (1280, Japanese labels) instead of clipping it.
		ui::SameLineField(ui::Px(320), LBL("Game folder"));
		char buf[512];
		snprintf(buf, sizeof buf, "%s", hotGameDir().c_str());
		ImGui::SetNextItemWidth(ui::Px(320));
		if (ImGui::InputText(LBL("Game folder"), buf, sizeof buf)) {
			hotGameDirSetting() = buf == DefaultGameDir(game) ? std::string() : std::string(buf);
			ImGui::MarkIniSettingsDirty();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Install folder of the game. Used for fpan-im\\mods when the game's ping reply does not name the mods folder."));
		ui::SameLineField(ui::Px(130) + ImGui::GetFrameHeight(), LBL("Pipe"));
		char pbuf[128];
		snprintf(pbuf, sizeof pbuf, "%s", hotPipe.c_str());
		ImGui::SetNextItemWidth(ui::Px(130));
		if (ImGui::InputText("##hotpipe", pbuf, sizeof pbuf)) hotPipe = pbuf;
		ImGui::SameLine(0, 0);
		if (ImGui::BeginCombo(LBL("Pipe"), nullptr, ImGuiComboFlags_NoPreview)) {
			for (const char* p : {"fpan-im-uni2", "fpan-im-mbtl"})
				if (ImGui::Selectable(p, hotPipe == p)) hotPipe = p;
			ImGui::EndCombo();
		}
		ui::SameLineButton(LBL("Ping"));
		ImGui::BeginDisabled(hotBusy);
		if (ImGui::Button(LBL("Ping"))) { HotTask t; t.pipe = hotPipe; t.gameDir = hotGameDir(); startHot(t); }
		ImGui::EndDisabled();
		ui::SameLineField(ui::Px(150), LBL("Reload mode"));
		const char* modes[] = {TXT("Rebuild (engine)"), TXT("Live (keep state)")};
		int mode = gSettings.sqHotReloadMode == 1 ? 1 : 0;
		ImGui::SetNextItemWidth(ui::Px(150));
		if (ImGui::Combo(LBL("Reload mode"), &mode, modes, 2)) { gSettings.sqHotReloadMode = mode; ImGui::MarkIniSettingsDirty(); }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Rebuild (engine): the game's own training-menu character switch. A full battle rebuild of about 1-3 s that picks up every change (HA6, CG, PAT, palettes, effects, scripts) but resets positions and meter.\nLive (keep state): reloads the character in place, so positions, meter and the current move are kept; use it for mid-move tests. It keeps the old character data in memory until the game restarts.\nBoth need a running training battle."));
		if (!hotStatus.empty()) {
			ImGui::SameLine();
			drawHotStatus();
		}
	}

	// The last reply: a reloadchara reply is summarized (timing, HA6 root before/after, refusals); anything else as is.
	void drawHotStatus()
	{
		char line[512];
		bool ok = hotStatusOk;
		if (hotBusy || !hotHasInfo) snprintf(line, sizeof line, "%s", i18n::TrDetail(hotStatus).c_str());
		else if (hotInfo.refused) {
			ok = false;
			snprintf(line, sizeof line, "%s", TXT("Refused: no training battle is running. Start a training battle in the game, then save again."));
		} else if (hotInfo.stdReload && hotStatusOk) {
			if (hotInfo.engine)
				snprintf(line, sizeof line, hotInfo.backInBattle ? TXT("Reloaded shared scripts and rebuilt the battle in %ld ms") : TXT("Reloaded shared scripts; the rebuild did not return to the training battle (%ld ms)"), hotInfo.ms);
			else snprintf(line, sizeof line, "%s", TXT("Reloaded shared scripts and every character in place"));
		} else if (hotInfo.engine && hotInfo.hasRoots) {
			snprintf(line, sizeof line, hotInfo.backInBattle ? TXT("Rebuilt %s in %ld ms, HA6 root %08X -> %08X") : TXT("Rebuild of %s did not return to the training battle (%ld ms), HA6 root %08X -> %08X"),
				hotChr.c_str(), hotInfo.ms, static_cast<unsigned>(hotInfo.rootBefore), static_cast<unsigned>(hotInfo.rootAfter));
			if (hotInfo.backInBattle && hotInfo.rootBefore == hotInfo.rootAfter) {
				const size_t n = strlen(line);
				snprintf(line + n, sizeof line - n, " %s", TXT("(HA6 root unchanged)"));
			}
		} else snprintf(line, sizeof line, "%s", i18n::TrDetail(hotStatus).c_str());
		ImGui::TextColored(ok ? ScriptCol() : ErrCol(), "%s", line);
		if (hotHasInfo && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hotStatus.c_str());   // the raw fpan-im reply
	}

	void buildOutline()
	{
		outlineDirty = false;
		outline = BuildOutline(editor.GetLineCount(), [this](size_t i) { return editor.GetLineText(i); });
	}

	static std::string GroupKey(const std::string& group, size_t line) { return group + "@" + std::to_string(line); }

	void drawOutline()
	{
		if (outlineDirty) buildOutline();
		ImGui::SetNextItemWidth(-1);
		char buf[64];
		snprintf(buf, sizeof buf, "%s", outlineFilter.c_str());
		if (ImGui::InputTextWithHint("##ofilter", "outline filter", buf, sizeof buf)) outlineFilter = buf;
		outlineRows = OutlineRows(outline, outlineFilter, [this](const std::string& g, size_t first) { return outlineOpen.count(GroupKey(g, outline[first].line)) > 0; });
		// only the visible rows are submitted (a 100k-line table has thousands of entries)
		ImGuiListClipper clip;
		clip.Begin(static_cast<int>(outlineRows.size()));
		while (clip.Step()) {
			for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
				const OutlineRow& row = outlineRows[static_cast<size_t>(r)];
				const auto& o = outline[row.item];
				ImGui::PushID(r);
				if (row.count) {   // group header: click folds / unfolds, the line jumps to the first entry
					const std::string k = GroupKey(o.group, o.line);
					const bool isOpen = outlineOpen.count(k) > 0;
					char label[160];
					snprintf(label, sizeof label, "%s %s (%zu)", isOpen ? "v" : ">", o.group.c_str(), row.count);
					if (ImGui::Selectable(label)) { if (isOpen) outlineOpen.erase(k); else outlineOpen.insert(k); }
					ImGui::PopID();
					continue;
				}
				const float indent = 10.0f * o.depth + (row.child ? 12.0f : 0.0f);
				if (indent > 0) ImGui::Indent(indent);
				if (ImGui::Selectable(o.name.c_str())) {
					editor.SetCursor(TextEditor::DocPos(o.line, 0));
					editor.ScrollToLine(o.line, TextEditor::Scroll::alignTop);
				}
				if (!o.command.empty() || !o.move.empty()) {
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						if (!o.command.empty()) ImGui::Text(TXT("command = [%s]"), o.command.c_str());
						if (!o.move.empty()) ImGui::Text("-> %s", o.move.c_str());
						ImGui::EndTooltip();
					}
					if (!o.command.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", o.command.c_str()); }
				}
				if (indent > 0) ImGui::Unindent(indent);
				ImGui::PopID();
			}
		}
	}
	std::map<std::string, std::vector<ApiSym>> api; // by native name; one name can live in several tables
	std::map<std::string, int> consts;               // integer `const` from script/*.txt (def_PAT_* = pattern numbers)
	// Completion popup (Ctrl+Space). It is an ImGui popup so it owns the keyboard while open.
	bool acOpenRequest = false;
	char acFilter[64] = {};
	int acSel = 0;
	TextEditor::DocPos acStart, acEnd;
	std::vector<std::string> acWords;

	void loadApi(const std::string& path)
	{
		api.clear();
		if (path.empty()) return;
		std::ifstream f(U8(path));
		const nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
		if (!j.is_array()) return;
		for (const auto& e : j) {
			if (!e.is_object() || !e.contains("name") || !e["name"].is_string()) continue;
			ApiSym a;
			a.table = e.value("table", "");
			a.ret = e.value("ret", "");
			if (e.contains("args") && e["args"].is_array())
				for (const auto& x : e["args"]) if (x.is_string()) a.args.push_back(x.get<std::string>());
			api[e["name"].get<std::string>()].push_back(std::move(a));
		}
		apiByTable.clear();
		for (const auto& [n, v] : api)
			for (const auto& a : v) apiByTable[a.table].push_back({n, &a});
	}

	// Candidates after "Table.": the engine natives of that table plus the script members the index knows.
	// Otherwise: API natives, table names, project globals, Squirrel keywords and names this file declares.
	std::string acOwner;
	std::map<std::string, std::string> acTag;   // word -> "[engine] sig" / "[script] file:line"
	void collectWords()
	{
		std::set<std::string> w;
		acTag.clear();
		if (!acOwner.empty()) {
			const auto tb = apiByTable.find(acOwner);
			if (tb != apiByTable.end())
				for (const auto& [n, a] : tb->second) { w.insert(n); acTag[n] = "[engine] " + Signature(n, *a); }
			for (const ScriptDef* d : index.members(acOwner)) {
				if (!w.insert(d->name).second && acTag.count(d->name)) continue;
				acTag[d->name] = "[script] " + d->name + (d->kind == ScriptDef::Function ? "(" + d->params + ")" : "") + "  " + d->file + ":" + std::to_string(d->line + 1);
			}
			acWords.assign(w.begin(), w.end());
			return;
		}
		for (const auto& [k, v] : api) { w.insert(k); acTag[k] = "[engine] " + Signature(k, v.front()); }
		for (const auto& [t, v] : apiByTable) if (!t.empty() && t[0] != '(') { w.insert(t); acTag[t] = "[engine] " + std::string(EngineTableNote(t)); }
		for (const ScriptDef& d : index.defs()) {
			if (!d.owner.empty() && d.kind != ScriptDef::MoveTable) continue;
			if (w.insert(d.name).second) acTag[d.name] = "[script] " + d.file + ":" + std::to_string(d.line + 1);
		}
		for (const char* k : {"base", "break", "case", "catch", "class", "clone", "const", "continue", "default", "delegate", "delete", "else",
				 "enum", "extends", "for", "foreach", "function", "if", "in", "instanceof", "local", "null", "resume", "return", "static",
				 "switch", "this", "throw", "try", "typeof", "while", "yield", "true", "false"})
			w.insert(k);
		static const std::regex decl(R"((?:function|local|class|const|enum)\s+([A-Za-z_]\w*))");
		const std::string text = editor.GetText();
		for (std::sregex_iterator it(text.begin(), text.end(), decl), end; it != end; ++it) w.insert((*it)[1].str());
		acWords.assign(w.begin(), w.end());
	}

	void loadConsts()
	{
		consts.clear();
		static const std::regex c(R"(^\s*const\s+([A-Za-z_]\w*)\s*=\s*(-?\d+)\s*;)");
		// script/*.txt from the merged list (the copy the game reads: a pack, a mod folder, loose or the archive)
		for (const std::string& rel : files) {
			if (rel.rfind("script/", 0) != 0 || rel.find('/', 7) != std::string::npos) continue;
			std::ifstream f(U8(pathOrRoot(rel)), std::ios::binary);
			std::string line;
			std::smatch mm;
			while (std::getline(f, line))
				if (std::regex_search(line, mm, c)) consts[mm[1].str()] = std::stoi(mm[2].str());
		}
	}

	// The token under the mouse, read as a pattern reference. Columns are code points; the line is UTF-8.
	bool refAtMouse(ScriptPanel::PatRef& out)
	{
		const ImVec2 mp = ImGui::GetMousePos();
		if (!editor.IsMousePosOverTextArea(mp) || !editor.IsMousePosOverGlyph(mp)) return false;
		const TextEditor::DocPos pos = editor.GetDocPosAtMousePos(mp);
		const std::string line = editor.GetLineText(pos.line);
		size_t at = 0;
		for (size_t cp = 0; at < line.size() && cp < pos.index; cp++) {
			at++;
			while (at < line.size() && (static_cast<unsigned char>(line[at]) & 0xC0) == 0x80) at++;
		}
		if (at >= line.size()) return false;
		const auto isWord = [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_'; };
		// Inside a string literal? Quotes pair up left to right (scripts here have no escaped quotes in names).
		for (size_t open = line.find('"'); open != std::string::npos;) {
			const size_t close = line.find('"', open + 1);
			if (close == std::string::npos) break;
			if (open <= at && at <= close) {
				static const std::regex lead(R"((?:\bpat|\bstart_pat|\bDataPattern|SetPattern)\s*[=(]\s*$)");
				if (!std::regex_search(line.substr(0, open), lead)) return false;
				out.code = line.substr(open + 1, close - open - 1);
				return !out.code.empty();
			}
			open = line.find('"', close + 1);
		}
		if (!isWord(line[at])) return false;
		size_t b = at, e = at;
		while (b > 0 && isWord(line[b - 1])) b--;
		while (e < line.size() && isWord(line[e])) e++;
		const std::string word = line.substr(b, e - b);
		if (word.rfind("def_PAT_", 0) == 0) {
			const auto c = consts.find(word);
			if (c == consts.end()) return false;
			out.pattern = c->second;
			return true;
		}
		if (word.find_first_not_of("0123456789") != std::string::npos) return false;
		const std::string left = line.substr(0, b);
		static const std::regex patLead(R"((?:\bpat|\bstart_pat|\bDataPattern|SetPattern)\s*[=(]\s*$)");
		static const std::regex afidLead(R"((?:JumpFrameID\s*\(\s*|\bcase\s+)$)");
		if (std::regex_search(left, patLead)) out.pattern = std::stoi(word);
		else if (std::regex_search(left, afidLead)) out.afid = std::stoi(word);
		else return false;
		return true;
	}

	void drawHover()
	{
		ImVec2 mp;
		if (scriptHover) {
			const ImVec2 at = editor.DocPos2ScreenPos(scriptHoverPos);
			mp = ImVec2(at.x + 2.0f, at.y + editor.GetLineHeight() * 0.5f);
		} else {
			if (!ImGui::IsItemHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) return;
			mp = ImGui::GetMousePos();
			if (!editor.IsMousePosOverTextArea(mp) || !editor.IsMousePosOverGlyph(mp)) return;
		}
		ScriptPanel::PatRef ref;
		if (!scriptHover && owner && refAtMouse(ref)) {
			const std::string dir = ToU8((U8(root) / U8(currentRel)).parent_path());
			const std::string what = owner->describeRef ? owner->describeRef(dir, ref) : "";
			ImGui::BeginTooltip();
			if (!what.empty()) ImGui::TextUnformatted(what.c_str());
			else if (ref.afid >= 0) ImGui::Text(TXT("frame id (AFID) %d"), ref.afid);
			else if (!ref.code.empty()) ImGui::Text(TXT("pattern code name (PTCN) \"%s\""), ref.code.c_str());
			else ImGui::Text(TXT("pattern %d"), ref.pattern);
			if (owner->openRef) ImGui::TextDisabled("%s", TXT("Ctrl+click: open in the HA6 editor"));
			ImGui::EndTooltip();
			if (owner->openRef && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl) owner->openRef(dir, ref);
			return;
		}
		const TextEditor::DocPos pos = editor.GetDocPosAtMousePos(mp);
		const std::string line = editor.GetLineText(pos.line);
		std::string word, own;
		WordAt(line, ByteCol(line, pos.index), word, own);
		if (word.empty() || std::isdigit(static_cast<unsigned char>(word[0]))) return;
		const bool ctrlClick = !scriptHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyCtrl;
		// Engine side: natives of the table it is called through (or any table when unqualified), and the tables themselves.
		std::vector<std::string> engine;
		const auto it = api.find(word);
		if (it != api.end())
			for (const auto& a : it->second) if (own.empty() || a.table == own) engine.push_back(Signature(word, a));
		const bool isTable = apiByTable.count(word) && own.empty();
		const auto defs = defsFor(word, own);
		if (engine.empty() && !isTable && defs.empty()) return;
		if (scriptHover) ImGui::SetNextWindowPos(ImVec2(mp.x + 16.0f, mp.y + editor.GetLineHeight()));   // where a resting mouse puts it
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40);
		if (isTable) {
			ImGui::TextColored(EngineCol(), "[engine]"); ImGui::SameLine();
			ImGui::Text("%s: %s (%zu natives)", word.c_str(), EngineTableNote(word), apiByTable.at(word).size());
		}
		for (const auto& e : engine) {
			ImGui::TextColored(EngineCol(), "[engine]"); ImGui::SameLine();
			ImGui::TextUnformatted(e.c_str());
		}
		if (!engine.empty()) ImGui::TextDisabled("%s", (game + " native in the game executable (" + EngineTableNote(own.empty() ? it->second.front().table : own) + ")").c_str());
		int shown = 0;
		for (const ScriptDef* d : defs) {
			if (shown++ == 4) { ImGui::TextDisabled(TXT("... %zu more definitions"), defs.size() - 4); break; }
			ImGui::TextColored(ScriptCol(), "[script]"); ImGui::SameLine();
			std::string head = (d->owner.empty() ? "" : d->owner + ".") + d->name;
			if (d->kind == ScriptDef::Function) head += "(" + d->params + ")";
			if (d->kind == ScriptDef::Const) head += " = " + d->value;
			if (d->kind == ScriptDef::CmdEntry && !d->value.empty()) head += " -> " + d->value;
			ImGui::TextUnformatted(head.c_str());
			ImGui::TextDisabled("  %s:%d", d->file.c_str(), d->line + 1);
			if (!d->doc.empty()) ImGui::TextUnformatted(d->doc.c_str());
			if (d->kind == ScriptDef::Function) {   // script wrappers: which engine natives they build on
				const std::string calls = EngineCallsOf(*d);
				if (!calls.empty()) {
					ImGui::TextColored(EngineCol(), "Calls engine:"); ImGui::SameLine();
					ImGui::TextUnformatted(calls.c_str());
				}
			}
		}
		if (!defs.empty()) ImGui::TextDisabled("%s", TXT("Ctrl+click / F12: go to definition, Shift+F12: find usages"));
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
		if (ctrlClick && !defs.empty()) goToDefinition(word, own);
	}

	void drawCompletion(bool focused)
	{
		if (focused && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
			acEnd = editor.GetMainCursorPosition();
			acStart = editor.FindWordStart(acEnd);
			std::string prefix = editor.GetLineText(acEnd.line);
			prefix = acStart.line == acEnd.line && acStart.index <= acEnd.index && acEnd.index <= prefix.size()
				? prefix.substr(acStart.index, acEnd.index - acStart.index) : "";
			if (prefix.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
				prefix.clear();
				acStart = acEnd;
			}
			snprintf(acFilter, sizeof acFilter, "%s", prefix.c_str());
			acSel = 0;
			{
				const std::string line = editor.GetLineText(acStart.line);
				std::string word, own;
				const size_t b = ByteCol(line, acStart.index);
				acOwner.clear();
				if (b >= 2 && line[b - 1] == '.') { WordAt(line, b - 1, word, own); acOwner = word; }
			}
			collectWords();
			const ImVec2 at = editor.DocPos2ScreenPos(acEnd);
			ImGui::SetNextWindowPos(ImVec2(at.x, at.y + editor.GetLineHeight()));
			ImGui::OpenPopup("##sqcomplete");
		}
		ImGui::SetNextWindowSizeConstraints(ImVec2(ui::Px(420), 0), ImVec2(ui::Px(900), ui::Px(340)));
		if (!ImGui::BeginPopup("##sqcomplete")) return;
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputText("##acfilter", acFilter, sizeof acFilter)) acSel = 0;
		std::string low = acFilter;
		std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::vector<const std::string*> hits;
		for (const auto& w : acWords) {
			std::string lw = w;
			std::transform(lw.begin(), lw.end(), lw.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (lw.rfind(low, 0) == 0) hits.push_back(&w);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) acSel = std::min(acSel + 1, static_cast<int>(hits.size()) - 1);
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) acSel = std::max(acSel - 1, 0);
		const std::string* chosen = nullptr;
		if (!hits.empty() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) || ImGui::IsKeyPressed(ImGuiKey_Tab)))
			chosen = hits[std::clamp(acSel, 0, static_cast<int>(hits.size()) - 1)];
		ImGui::BeginChild("##aclist", ImVec2(0, std::min(10, static_cast<int>(hits.size())) * ImGui::GetTextLineHeightWithSpacing() + 4));
		for (int i = 0; i < static_cast<int>(hits.size()) && i < 400; i++) {
			const auto t = acTag.find(*hits[i]);
			if (ImGui::Selectable(hits[i]->c_str(), i == acSel)) chosen = hits[i];
			if (t != acTag.end()) {
				ImGui::SameLine(ui::Px(160));
				ImGui::TextColored(t->second.rfind("[engine]", 0) == 0 ? EngineCol() : ScriptCol(), "%s", t->second.c_str());
			}
			if (i == acSel && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) ImGui::SetScrollHereY();
		}
		ImGui::EndChild();
		if (chosen) {
			editor.SelectRegion(acStart, acEnd);
			editor.ReplaceTextInCurrentCursor(*chosen);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	static std::string CharDirOf(const std::string& rel)
	{
		if (rel.rfind("data/", 0) != 0) return "";
		const size_t sl = rel.find('/', 5);
		return sl == std::string::npos ? "" : rel.substr(0, sl);
	}
	std::string scriptDir() const { return ToU8((U8(root) / U8(currentRel)).parent_path()); }

	bool openAt(const std::string& rel, int line, int col)
	{
		if (rel != currentRel) {
			if (dirty) { setStatus("unsaved changes in " + currentRel + ": save first", true); return false; }
			std::string err;
			if (!owner->openFile(pathOrRoot(rel), &err)) { setStatus(err, true); return false; }
		}
		editor.SetCursor(TextEditor::DocPos(static_cast<size_t>(line), static_cast<size_t>(std::max(0, col))));
		editor.ScrollToLine(static_cast<size_t>(line), TextEditor::Scroll::alignMiddle);
		return true;
	}

	// Every pattern / code-name / frame-id reference in the current file that points into the character's own HA6, for
	// the HA6 checks. Skipped: objects created with datatype=N (they read the effect HA6), and `case N` labels unless the
	// switch is over the character's own frame id (GetUpdateFrameID() with no argument; with one it is a spawned object's).
	std::vector<std::pair<int, PatRef>> scanRefs() const
	{
		std::vector<std::string> lines(editor.GetLineCount());
		for (size_t i = 0; i < lines.size(); ++i) lines[i] = editor.GetLineText(i);
		return ScanRefs(lines, consts);
	}
	static std::vector<std::pair<int, PatRef>> ScanRefs(const std::vector<std::string>& lines, const std::map<std::string, int>& consts)
	{
		std::vector<std::pair<int, PatRef>> out;
		static const std::regex num(R"((?:\bpat|\bstart_pat|\bDataPattern|SetPattern)\s*[=(]\s*(\d+))");
		static const std::regex code(R"re((?:\bpat|\bstart_pat|\bDataPattern|SetPattern)\s*[=(]\s*"([^"]+)")re");
		static const std::regex jump(R"((?:^|[^.\w]|\bBMvTbl\.)JumpFrameID\s*\(\s*(\d+))");
		static const std::regex caseN(R"(\bcase\s+(\d+))");
		static const std::regex sw(R"(\bswitch\s*\((.*)\))");
		static const std::regex selfFrame(R"(GetUpdateFrameID\s*\(\s*\))");
		static const std::regex otherData(R"(\bdatatype\s*=\s*[1-9])");
		static const std::regex defpat(R"(\b(def_PAT_\w+))");
		const size_t n = lines.size();
		bool frameSwitch = false;
		for (size_t i = 0; i < n; ++i) {
			std::string l = lines[i];
			const size_t c = l.find("//");
			if (c != std::string::npos) l.resize(c);
			std::smatch m;
			if (std::regex_search(l, m, sw)) frameSwitch = std::regex_search(m[1].str(), selfFrame);
			const bool effectData = std::regex_search(l, otherData) || (i + 1 < n && std::regex_search(lines[i + 1], otherData));
			if (!effectData) {
				for (std::sregex_iterator it(l.begin(), l.end(), num), e; it != e; ++it) { PatRef r; r.pattern = std::stoi((*it)[1].str()); out.push_back({(int)i, r}); }
				for (std::sregex_iterator it(l.begin(), l.end(), code), e; it != e; ++it) { PatRef r; r.code = (*it)[1].str(); out.push_back({(int)i, r}); }
			}
			for (std::sregex_iterator it(l.begin(), l.end(), jump), e; it != e; ++it) { PatRef r; r.afid = std::stoi((*it)[1].str()); out.push_back({(int)i, r}); }
			if (frameSwitch)
				for (std::sregex_iterator it(l.begin(), l.end(), caseN), e; it != e; ++it) { PatRef r; r.afid = std::stoi((*it)[1].str()); out.push_back({(int)i, r}); }
			for (std::sregex_iterator it(l.begin(), l.end(), defpat), e; it != e; ++it) {
				const auto k = consts.find((*it)[1].str());
				if (k != consts.end()) { PatRef r; r.pattern = k->second; out.push_back({(int)i, r}); }
			}
		}
		return out;
	}

	// Re-index the open file from the editor and rebuild the problem list (move checks for its character folder,
	// unknown engine calls, HA6 reference checks against the loaded character).
	void recheck()
	{
		if (currentRel.empty()) return;
		index.updateFile(currentRel, editor.GetText());
		indexGen++;
		problems.clear();
		const std::string cdir = CharDirOf(currentRel);
		if (!cdir.empty()) problems = index.checkMoves(cdir);
		// After a character load, the game's own move table decides: Battle_Std also builds moves the mv files never define
		// (command templates, standard moves), and those exist in game.
		if (!realMoves.empty() && cdir == loadProblemsDir)
			problems.erase(std::remove_if(problems.begin(), problems.end(), [&](const ScriptProblem& p) {
				return p.fix == ScriptProblem::CreateMoveTable && realMoves.count(p.fixArg);
			}), problems.end());
		static const std::regex call(R"(\b([A-Z]\w*)\.([A-Za-z_]\w*)\s*\()");
		const size_t n = editor.GetLineCount();
		for (size_t i = 0; i < n; ++i) {
			std::string l = editor.GetLineText(i);
			const size_t c = l.find("//");
			if (c != std::string::npos) l.resize(c);
			for (std::sregex_iterator it(l.begin(), l.end(), call), e; it != e; ++it) {
				const std::string t = (*it)[1].str(), f = (*it)[2].str();
				const auto tb = apiByTable.find(t);
				if (tb == apiByTable.end()) continue;   // not an engine table
				bool known = std::any_of(tb->second.begin(), tb->second.end(), [&](const auto& x) { return x.first == f; });
				if (!known) for (const ScriptDef* d : index.members(t)) if (d->name == f) known = true;
				if (known) continue;
				ScriptProblem p;
				p.file = currentRel; p.line = (int)i; p.col = (int)it->position(2);
				p.message = t + " has no native " + f + " in the " + game + " API table (engine side): the call fails at run time.";
				problems.push_back(p);
			}
		}
		if (owner && owner->checkRef) {
			std::set<std::string> seen;
			for (const auto& [line, ref] : scanRefs()) {
				const std::string msg = owner->checkRef(scriptDir(), ref);
				if (msg.empty() || !seen.insert(std::to_string(line) + msg).second) continue;
				ScriptProblem p;
				p.file = currentRel; p.line = line; p.message = msg;
				problems.push_back(p);
			}
		}
		std::stable_sort(problems.begin(), problems.end(), [&](const ScriptProblem& a, const ScriptProblem& b) {
			const bool ah = a.file == currentRel, bh = b.file == currentRel;
			return ah != bh ? ah : (a.file != b.file ? a.file < b.file : a.line < b.line);
		});
		if (!loadProblemsDir.empty() && CharDirOf(currentRel) == loadProblemsDir)
			problems.insert(problems.end(), loadProblems.begin(), loadProblems.end());
		editor.ClearMarkers();
		for (const auto& p : problems)
			if (p.file == currentRel) editor.AddMarker(static_cast<size_t>(p.line), IM_COL32(230, 170, 40, 255), IM_COL32(110, 80, 0, 70), p.message, p.message);
	}

	// The game's character load for the open file's character (MBTL: traced order, docs/squirrel/RE_MBTL_LOAD.md), with the
	// editor text in place of the file on disk. False when it does not apply (not a character sc/cmd/mv/com file, or the load
	// order of this game is not traced yet). Fills loadProblems; the first runtime error in this file comes back in *here.
	bool characterCheck(const std::string& bytes, Diagnostic* here)
	{
		// UNI2 / MBTL: data/chrNNN/chrNNN_{sc,cmd,mv,com}_0.txt; DFCI / UNIB: data/<Name>_0/<Name>_0_{sc,cmd,mv}.txt, _com.txt
		static const std::regex reUni(R"(^data/(chr(\d+))/\1_(sc|cmd|mv|com)_0\.txt$)");
		static const std::regex reArc(R"(^data/(([A-Za-z]+(?:_s)?)_0)/(?:\1_(?:sc|cmd|mv)\.txt|_com\.txt)$)");
		std::smatch m;
		const bool uni = std::regex_match(currentRel, m, reUni);
		if (apiPath.empty() || game.empty() || (!uni && !std::regex_match(currentRel, m, reArc))) return false;
		const std::string dir = "data/" + m[1].str();
		const auto t0 = std::chrono::steady_clock::now();
		const auto codes = LoadPatternCodes(pathOrRoot(ToU8(U8(dir) / U8(m[1].str() + (uni ? "_0.txt" : ".txt")))));
		const auto effect = LoadPatternCodes(pathOrRoot("data/effect.txt"));
		LoadContext ctx;
		ctx.charaNo = ctx.vsCharaNo = uni ? std::stoi(m[2].str()) : -1;
		ctx.patternByCode = [&](int dt, const std::string& code) {
			const auto& t = dt == 1 ? effect : codes;
			const auto it = t.find(code);
			return it == t.end() ? -1 : it->second;
		};
		const CharaLoadResult r = vm.checkCharacter(dir, ctx, currentRel, bytes);
		loadProblems.clear();
		loadProblemsDir = dir;
		realMoves = r.moves;
		int dev = 0;
		for (const auto& i : r.issues) {
			if (i.kind == CharaLoadIssue::DevWarning) { dev++; say("developer warning (" + dir + "): " + i.message); continue; }
			if (i.kind == CharaLoadIssue::Note) { say(i.file + ": " + i.message); continue; }
			ScriptProblem p;
			p.severity = i.kind == CharaLoadIssue::MissingPattern ? ScriptProblem::Warning : ScriptProblem::Error;
			p.file = i.file.empty() ? currentRel : i.file;
			p.line = std::max(0, i.line - 1);
			if (i.kind == CharaLoadIssue::MissingPattern) {
				// point at the move's own table (last definition in this folder wins), else at the command that names it
				bool defined = false;
				for (const ScriptDef* def : index.find(i.move))
					if (def->kind == ScriptDef::MoveTable && def->file.rfind(dir + "/", 0) == 0) { p.file = def->file; p.line = def->line; defined = true; }
				if (!defined) {   // only a command names it: point there and suggest the closest defined move
					for (const ScriptHit& h : index.usages(i.move))
						if (h.file.rfind(dir + "/", 0) == 0 && h.file.find("_cmd") != std::string::npos) { p.file = h.file; p.line = h.line; p.col = h.col; break; }
					const std::string guess = index.nearestMove(i.move, dir);
					if (!guess.empty() && guess != i.move) p.message_suffix_hint = guess;
				}
			}
			p.message = (i.kind == CharaLoadIssue::RuntimeError ? "game load: " : "") + i.message +
				(p.message_suffix_hint.empty() ? "" : " Did you mean " + p.message_suffix_hint + "?");
			loadProblems.push_back(p);
			if (here && here->ok && i.kind == CharaLoadIssue::RuntimeError && p.file == currentRel) {
				here->ok = false;
				here->runtime = true;
				here->line = i.line;
				here->message = i.message;
				here->source = "./" + currentRel;
			}
		}
		const long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
		say("character load check " + dir + ": " + std::to_string(r.filesRun) + " files, " + std::to_string(r.commandMoves.size()) +
			" moves startable by commands, " + std::to_string(loadProblems.size()) + " problem(s), " + std::to_string(dev) +
			" developer warning(s), " + std::to_string(ms) + " ms");
		return true;
	}

	// Writes the blank move table `name` into `fixFile` (the open editor when it is that file, so it can be undone).
	bool addMoveStub(const std::string& name, const std::string& fixFile)
	{
		if (fixFile == currentRel) {
			int line = 0;
			const std::string text = editor.GetText();
			ScriptIndex::InsertMoveTableStub(text, name, &line);
			const std::string var = [&] {
				static const std::regex mk(R"(MakeMoveTable\s*\(\s*([A-Za-z_]\w*))");
				std::smatch m;
				return std::regex_search(text, m, mk) ? m[1].str() : std::string("t");
			}();
			editor.SetCursor(TextEditor::DocPos(static_cast<size_t>(line), 0));
			editor.ReplaceTextInCurrentCursor(var + "." + name + " <-\n{\n}\n\n");
			dirty = true;
			say("added a blank move table " + name + " to " + fixFile + " (unsaved)");
			return true;
		}
		const std::string src = pathOrRoot(fixFile);
		const fs::path path = U8(saveTargetFor(fixFile, src));
		std::string bytes;
		if (!ReadBytes(U8(src), bytes)) { setStatus("cannot read " + fixFile, true); return false; }
		{ std::error_code ec; fs::create_directories(path.parent_path(), ec); }
		ScriptDocument d;
		d.loadBytes(bytes);
		const std::string updated = ScriptIndex::InsertMoveTableStub(d.utf8Text(), name);
		std::string out, backup, err;
		std::vector<EncodeError> errs;
		if (!d.encode(SplitLines(updated), out, &errs)) { setStatus("cannot encode " + fixFile, true); return false; }
		std::error_code eex;
		if ((fs::exists(path, eex) && !WriteBackup(path, bytes, &backup)) || !WriteAtomic(path, out, &err)) { setStatus("cannot write " + fixFile, true); return false; }
		if (ToU8(path) != src) rescan(true);
		index.updateFile(fixFile, updated);
		indexGen++;
		say("added a blank move table " + name + " to " + fixFile + " (saved, backup kept)");
		return true;
	}

	// The engine natives a script function's body calls ("BMvTbl.GetPosition, BMvEff.GetCameraRect"), from its definition
	// line to the brace that closes it; cached per definition.
	std::map<std::string, std::string> engineCallsCache;
	std::string EngineCallsOf(const ScriptDef& d)
	{
		const std::string key = d.file + ":" + std::to_string(d.line) + ":" + std::to_string(indexGen);
		const auto hit = engineCallsCache.find(key);
		if (hit != engineCallsCache.end()) return hit->second;
		const auto* lines = index.lines(d.file);
		std::vector<std::string> found;
		if (lines) {
			static const std::regex call(R"(\b([A-Z]\w*)\.([A-Za-z_]\w*)\s*\()");
			int depth = 0;
			bool opened = false;
			for (size_t i = static_cast<size_t>(d.line); i < lines->size() && i < static_cast<size_t>(d.line) + 2000; ++i) {
				std::string l = (*lines)[i];
				const size_t c = l.find("//");
				if (c != std::string::npos) l.resize(c);
				if (i == static_cast<size_t>(d.line)) {   // skip the parameter list: default values like tbl={} are not the body
					const size_t close = l.rfind(')');
					if (close != std::string::npos) l = l.substr(close + 1);
				}
				for (std::sregex_iterator it(l.begin(), l.end(), call), e; it != e; ++it) {
					if (!apiByTable.count((*it)[1].str())) continue;   // only engine tables
					const std::string q = (*it)[1].str() + "." + (*it)[2].str();
					if (std::find(found.begin(), found.end(), q) == found.end()) found.push_back(q);
				}
				for (char ch : l) {
					if (ch == '{') { depth++; opened = true; }
					else if (ch == '}') depth--;
				}
				if (opened && depth <= 0) break;
			}
		}
		std::string out;
		for (size_t i = 0; i < found.size() && i < 12; ++i) out += (i ? ", " : "") + found[i];
		if (found.size() > 12) out += ", ... (" + std::to_string(found.size()) + ")";
		engineCallsCache[key] = out;
		return out;
	}

	// The identifier under the cursor / mouse, and the table it is reached through ("BMvTbl" in BMvTbl.GetPosition).
	static void WordAt(const std::string& line, size_t at, std::string& word, std::string& owner)
	{
		word.clear(); owner.clear();
		auto isw = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
		if (at > line.size()) return;
		size_t b = at, e = at;
		while (b > 0 && isw(line[b - 1])) b--;
		while (e < line.size() && isw(line[e])) e++;
		word = line.substr(b, e - b);
		if (b >= 2 && line[b - 1] == '.') {
			size_t ob = b - 1;
			while (ob > 0 && isw(line[ob - 1])) ob--;
			owner = line.substr(ob, b - 1 - ob);
		}
	}
	static size_t ByteCol(const std::string& line, size_t cp)
	{
		size_t at = 0;
		for (size_t c = 0; at < line.size() && c < cp; c++) {
			at++;
			while (at < line.size() && (static_cast<unsigned char>(line[at]) & 0xC0) == 0x80) at++;
		}
		return at;
	}

	std::vector<const ScriptDef*> defsFor(const std::string& word, const std::string& ownerName) const
	{
		std::vector<const ScriptDef*> all = index.find(word), pick;
		if (!ownerName.empty())
			for (const ScriptDef* d : all) if (d->owner == ownerName || d->owner.size() > ownerName.size() && d->owner.compare(d->owner.size() - ownerName.size(), ownerName.size(), ownerName) == 0) pick.push_back(d);
		return pick.empty() ? all : pick;
	}

	void goToDefinition(const std::string& word, const std::string& ownerName)
	{
		const auto defs = defsFor(word, ownerName);
		if (defs.empty()) {
			const bool engine = api.count(word) != 0;
			setStatus(engine ? word + " is an engine native (in the game executable, no script source)" : "no definition of " + word + " in the scripts", !engine);
			return;
		}
		if (defs.size() == 1) { openAt(defs[0]->file, defs[0]->line, defs[0]->col); return; }
		defChoices = defs;
		openDefChoices = true;
	}

	void findUsages(const std::string& word)
	{
		hits = index.usages(word);
		ha6Hits.clear();
		hitsTitle = std::to_string(hits.size()) + " usage(s) of " + word;
		bottomTab = 2;
		selectBottomTab = true;
	}

	void runSearch()
	{
		hits = index.search(searchBuf, searchOpt);
		ha6Hits.clear();
		if (owner && owner->searchHa6 && searchBuf[0] && !currentRel.empty()) ha6Hits = owner->searchHa6(scriptDir(), searchBuf);
		hitsTitle = std::to_string(hits.size()) + " script hit(s)" + (ha6Hits.empty() ? "" : ", " + std::to_string(ha6Hits.size()) + " pattern(s)") + " for \"" + searchBuf + "\"";
	}

	// Signature help: inside an unclosed call on the cursor line, show its parameters with the current one highlighted.
	void drawSignatureHelp(bool focused)
	{
		if (!focused || ImGui::IsPopupOpen("##sqcomplete")) return;
		const TextEditor::DocPos cur = editor.GetMainCursorPosition();
		const std::string line = editor.GetLineText(cur.line);
		size_t at = ByteCol(line, cur.index);
		int depth = 0, arg = 0;
		size_t open = std::string::npos;
		bool str = false;
		for (size_t i = at; i-- > 0;) {
			const char c = line[i];
			if (c == '"') { str = !str; continue; }
			if (str) continue;
			if (c == ')' || c == ']' || c == '}') depth++;
			else if (c == '(' || c == '[' || c == '{') {
				if (depth == 0) { if (c == '(') open = i; break; }
				depth--;
			} else if (c == ',' && depth == 0) arg++;
		}
		if (open == std::string::npos || open == 0) return;
		std::string word, own;
		size_t e = open;
		while (e > 0 && line[e - 1] == ' ') e--;
		if (e == 0) return;
		WordAt(line, e - 1, word, own);
		if (word.empty() || word == "if" || word == "while" || word == "for" || word == "foreach" || word == "switch" || word == "function") return;
		std::vector<std::string> params;
		std::string head, where;
		bool engine = false;
		const auto tb = apiByTable.find(own);
		const ApiSym* native = nullptr;
		if (tb != apiByTable.end()) for (const auto& [n, a] : tb->second) if (n == word) native = a;
		if (!native && own.empty() && api.count(word)) native = &api.at(word).front();
		if (native) {
			engine = true;
			params = native->args;
			head = (native->table.empty() || native->table[0] == '(' ? "" : native->table + ".") + word;
			where = "-> " + (native->ret.empty() ? std::string("?") : native->ret);
		} else {
			const auto defs = defsFor(word, own);
			const ScriptDef* d = nullptr;
			for (const ScriptDef* x : defs) if (x->kind == ScriptDef::Function) { d = x; break; }
			if (!d) return;
			head = (d->owner.empty() ? "" : d->owner + ".") + d->name;
			where = d->file + ":" + std::to_string(d->line + 1);
			std::string cur2;
			int dd = 0;
			for (char c : d->params) {
				if (c == '(' || c == '{' || c == '[') dd++;
				if (c == ')' || c == '}' || c == ']') dd--;
				if (c == ',' && dd == 0) { params.push_back(cur2); cur2.clear(); continue; }
				cur2 += c;
			}
			if (!cur2.empty() || !params.empty()) params.push_back(cur2);
			for (auto& p : params) { while (!p.empty() && p[0] == ' ') p.erase(0, 1); while (!p.empty() && p.back() == ' ') p.pop_back(); }
		}
		const ImVec2 pos = editor.DocPos2ScreenPos(cur);
		ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y - editor.GetLineHeight() * 2.2f));
		ImGui::BeginTooltip();
		ImGui::TextColored(engine ? EngineCol() : ScriptCol(), "%s", engine ? "[engine]" : "[script]");
		ImGui::SameLine();
		ImGui::TextUnformatted((head + "(").c_str());
		for (size_t i = 0; i < params.size(); ++i) {
			ImGui::SameLine(0, 0);
			const std::string t = params[i] + (i + 1 < params.size() ? ", " : "");
			if ((int)i == arg) ImGui::TextColored(ArgCol(), "%s", t.c_str());
			else ImGui::TextUnformatted(t.c_str());
		}
		ImGui::SameLine(0, 0);
		ImGui::TextUnformatted(")");
		ImGui::SameLine();
		ImGui::TextDisabled("%s", where.c_str());
		ImGui::EndTooltip();
	}

	void drawBottom()
	{
		ImGui::BeginChild("##bottom", ImVec2(0, 0), ImGuiChildFlags_Borders);
		const int want = selectBottomTab ? bottomTab : -1;
		if (ImGui::BeginTabBar("##btabs")) {
			auto tab = [&](const char* label, int id) {
				const bool sel = want == id;
				const bool open = ImGui::BeginTabItem(label, nullptr, sel ? ImGuiTabItemFlags_SetSelected : 0);
				if (open) bottomTab = id;
				return open;
			};
			if (tab("Log", 0)) {
				ImGui::BeginChild("##log");
				for (const auto& l : log) ImGui::TextUnformatted(l.c_str());
				if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			const std::string pl = "Problems (" + std::to_string(problems.size()) + ")###problems";
			if (tab(pl.c_str(), 1)) { drawProblems(); ImGui::EndTabItem(); }
			if (tab("Search###search", 2)) { drawSearch(); ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
		selectBottomTab = false;
		ImGui::EndChild();
	}

	void drawProblems()
	{
		if (problems.empty()) { ImGui::TextDisabled(currentRel.empty() ? "Open a script." : "No problems found in this character's scripts."); return; }
		ImGui::BeginChild("##plist");
		for (size_t i = 0; i < problems.size(); ++i) {
			const ScriptProblem& p = problems[i];
			ImGui::PushID(static_cast<int>(i));
			if (p.fix == ScriptProblem::CreateMoveTable) {
				if (ImGui::SmallButton(LBL("Add blank state"))) {
					const ScriptProblem copy = p;
					if (addMoveStub(copy.fixArg, copy.fixFile)) recheck();
					ImGui::PopID();
					break;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Writes an empty %s <- { } into %s, named exactly as referenced."), p.fixArg.c_str(), p.fixFile.c_str());
				ImGui::SameLine();
			}
			const std::string label = p.file + ":" + std::to_string(p.line + 1) + "  " + p.message;
			ImGui::PushStyleColor(ImGuiCol_Text, p.severity == ScriptProblem::Error ? ErrCol() : WarnCol());
			const bool go = ImGui::Selectable(label.c_str());
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40); ImGui::TextUnformatted(p.message.c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }
			if (go) { const ScriptProblem copy = p; openAt(copy.file, copy.line, copy.col); ImGui::PopID(); break; }
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	void drawSearch()
	{
		if (focusSearch) { ImGui::SetKeyboardFocusHere(); focusSearch = false; }
		ImGui::SetNextItemWidth(ui::Px(320));
		bool go = ImGui::InputTextWithHint("##q", "search all scripts + pattern names (Enter)", searchBuf, sizeof searchBuf, ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine(); go |= ImGui::Checkbox(LBL("Regex"), &searchOpt.regex);
		ImGui::SameLine(); go |= ImGui::Checkbox(LBL("Whole word"), &searchOpt.wholeWord);
		ImGui::SameLine(); go |= ImGui::Checkbox(LBL("Match case"), &searchOpt.matchCase);
		ImGui::SameLine(); go |= ImGui::Checkbox(LBL("Skip comments"), &searchOpt.codeOnly);
		if (go && searchBuf[0]) runSearch();
		if (!hitsTitle.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", hitsTitle.c_str()); }
		ImGui::BeginChild("##hits");
		for (size_t i = 0; i < ha6Hits.size(); ++i) {
			ImGui::PushID(static_cast<int>(100000 + i));
			if (ImGui::Selectable(("[HA6] " + ha6Hits[i].first).c_str()) && owner->openRef) owner->openRef(scriptDir(), ha6Hits[i].second);
			ImGui::PopID();
		}
		for (size_t i = 0; i < hits.size(); ++i) {
			const ScriptHit& h = hits[i];
			ImGui::PushID(static_cast<int>(i));
			std::string t = h.text;
			while (!t.empty() && (t[0] == ' ' || t[0] == '\t')) t.erase(0, 1);
			if (ImGui::Selectable((h.file + ":" + std::to_string(h.line + 1) + "  " + t).c_str())) openAt(h.file, h.line, h.col);
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	void drawPopups()
	{
		if (openDefChoices) { ImGui::OpenPopup("Definitions##sq"); openDefChoices = false; }
		if (ImGui::BeginPopup("Definitions##sq")) {
			for (size_t i = 0; i < defChoices.size(); ++i) {
				const ScriptDef* d = defChoices[i];
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable(((d->owner.empty() ? "" : d->owner + ".") + d->name + "   " + d->file + ":" + std::to_string(d->line + 1)).c_str())) {
					const ScriptDef copy = *d;
					ImGui::CloseCurrentPopup();
					openAt(copy.file, copy.line, copy.col);
				}
				ImGui::PopID();
			}
			ImGui::EndPopup();
		}
		if (askMissingMoves) { ImGui::OpenPopup(LBL("Missing moves###sqmissing")); askMissingMoves = false; }
		ImGui::SetNextWindowSizeConstraints(ImVec2(ui::Px(460), 0), ImVec2(ui::Px(900), ui::Px(600)));
		if (ImGui::BeginPopupModal(LBL("Missing moves###sqmissing"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted(TXT("These moves are referenced but not found in the character's mv file:"));
			for (const auto& p : pendingMissing) {
				ImGui::BulletText("%s  (line %d)", p.fixArg.c_str(), p.line + 1);
				const std::string guess = index.nearestMove(p.fixArg, CharDirOf(p.file));
				if (!guess.empty() && guess != p.fixArg) { ImGui::SameLine(); ImGui::TextDisabled(TXT("did you mean %s?"), guess.c_str()); }
			}
			ImGui::Spacing();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34);
			ImGui::TextDisabled("%s", TXT("Save them as blank states in the mv file? A missing move can reset the character to neutral, or spawn copies of the character on button presses."));
			ImGui::PopTextWrapPos();
			const std::string fixFile = pendingMissing.empty() ? std::string() : pendingMissing.front().fixFile;
			ImGui::BeginDisabled(fixFile.empty());
			if (ImGui::Button(LBL("Add blank states and save"))) {
				const auto todo = pendingMissing;
				for (const auto& p : todo) addMoveStub(p.fixArg, p.fixFile);
				skipMovePromptOnce = true;
				owner->requestSave();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button(LBL("Save without adding"))) { skipMovePromptOnce = true; owner->requestSave(); ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button(LBL("Cancel"))) { forceNextSave = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	void say(const std::string& s)
	{
		HC_LOG_INFO("[script] {}", s);
		log.push_back(s);
		while (log.size() > 200) log.pop_front();
	}

	void setStatus(const std::string& s, bool err)
	{
		status = s;
		statusError = err;
		say(s);
	}

	void showDiagnostic(const Diagnostic& d, const std::string& name)
	{
		editor.ClearMarkers();
		if (d.ok) return;
		const bool here = !d.runtime || d.source == name;
		if (here && d.line > 0) {
			const ImU32 col = d.runtime ? IM_COL32(200, 140, 0, 255) : IM_COL32(220, 50, 50, 255);
			const ImU32 bg = d.runtime ? IM_COL32(120, 80, 0, 90) : IM_COL32(140, 20, 20, 110);
			editor.AddMarker(static_cast<size_t>(d.line - 1), col, bg, d.message, d.message);
			editor.SetCursor(TextEditor::DocPos(static_cast<size_t>(d.line - 1), 0));
			editor.ScrollToLine(static_cast<size_t>(d.line - 1));
		}
	}
};

ScriptPanel::ScriptPanel() : m(std::make_unique<Impl>())
{
	m->editor.SetLanguage(SquirrelLanguage());
	m->editor.SetLineFoldingEnabled(true);
	m->owner = this;
	// SetText (opening a file) also reports a change on the next frame; only an undoable edit makes the file dirty,
	// so a freshly opened file no longer shows the unsaved marker or blocks switching files.
	m->editor.SetChangeCallback([this] { if (m->editor.CanUndo()) m->dirty = true; m->outlineDirty = true; m->lastEditTime = ImGui::GetTime(); });
}

ScriptPanel::~ScriptPanel() = default;

bool ScriptPanel::hasUnsavedChanges() const { return m->dirty; }

bool ScriptPanel::openGameRoot(const std::string& rootUtf8, const std::string& profile, std::string* error)
{
	// The chosen folder may be the install, its data/ or script/, a UNI2-IM / MBTL-IM pack or Mods folder (or their data/),
	// or an fpan-im mods / pack folder: the install above it is the game root, and a pack / mod is where saves go.
	const ScriptRootInfo ri = ResolveScriptRoot(rootUtf8);
	if (!ri.error.empty()) {
		if (error) *error = ri.error;
		return false;
	}
	const fs::path r = U8(ri.gameDir);
	std::error_code ec;
	m->root = ri.gameDir;
	m->editRoot = ri.editRoot;
	m->editTag = ri.editTag;
	m->profileArg = profile;
	m->game = profile.empty() ? GuessGame(ri.gameDir) : profile;
	m->apiPath = FindApiProfile(m->game);
	m->loadApi(m->apiPath);
	// The game archive (UNI2: <install>/d, MBTL: the table in MBTL.exe): its script / data names join the list; an entry
	// is extracted to %TEMP%/hantei_archive/<game>_scripts when it is opened or indexed.
	m->archive.reset();
	m->archiveCache.clear();
	{
		std::string aerr;
		if (m->game == "uni2" && fs::is_directory(r / "d", ec)) m->archive = fbarc::OpenUni2Data(ToU8(r / "d"), &aerr);
		else if (m->game == "mbtl" && fs::exists(r / "MBTL.exe", ec)) m->archive = fbarc::OpenMbtlExe(ToU8(r / "MBTL.exe"), &aerr);
		if (m->archive) m->archiveCache = ToU8(fs::temp_directory_path(ec) / "hantei_archive" / (m->game + "_scripts"));
		else if (!aerr.empty()) m->say("archive: " + aerr);
	}
	m->rescan(true);
	m->loadConsts();
	m->problems.clear();
	std::string vmErr;
	m->vm.setLoadProfile(LoadProfileFor(m->game));
	m->vm.setPathResolver([this](const std::string& rel) { return m->pathOf(rel); });
	m->vmReady = m->vm.open(ToU8(r), m->apiPath, [this](const std::string& s) { m->say(s); }, &vmErr);
	if (!m->vmReady) m->vm.open(ToU8(r), "", [this](const std::string& s) { m->say(s); }, &vmErr);
	m->setStatus(std::to_string(m->files.size()) + " Squirrel scripts; profile " + (m->game.empty() ? "none" : m->game) +
			(m->apiPath.empty() ? " (no API table found: compile-only)" : " (" + std::to_string(m->vm.nativeCount()) + " natives)") +
			(m->editTag.empty() ? "" : "; editing " + m->editTag),
		false);
	return true;
}

bool ScriptPanel::openFile(const std::string& pathUtf8, std::string* error)
{
	std::string bytes;
	if (!ReadBytes(U8(pathUtf8), bytes)) {
		if (error) *error = "cannot read " + pathUtf8;
		return false;
	}
	m->doc.loadBytes(bytes);
	m->editor.SetText(m->doc.utf8Text());
	m->outlineDirty = true;
	m->editor.ClearMarkers();
	m->dirty = false;
	m->offerSaveAnyway = false;
	std::error_code ec;
	(void)ec;
	// The game-relative key: below the install (loose), else from the data/ or script/ folder on (a pack, mod or archive copy).
	m->currentPath = ToU8(U8(pathUtf8).lexically_normal());
	m->currentRel = m->root.empty() ? pathUtf8 : U8(pathUtf8).lexically_normal().lexically_relative(U8(m->root).lexically_normal()).generic_u8string();
	{
		const std::string low = Impl::LowerS(m->currentRel);
		if (m->root.empty() || !(low.rfind("data/", 0) == 0 || low.rfind("script/", 0) == 0)) {
			const std::string key = FpanimGameKey(pathUtf8);
			if (!key.empty()) {
				const size_t sl = key.find('/');
				m->currentRel = Impl::LowerS(key.substr(0, sl)) + key.substr(sl);
			}
		}
	}
	m->lastEditTime = -1.0;
	m->recheck();
	m->setStatus("opened " + m->currentRel + " (" + std::to_string(bytes.size()) + " bytes, " + std::to_string(m->doc.lineCount()) + " lines)", false);
	return true;
}

std::vector<ScriptPanel::ScriptRef> ScriptPanel::refsToPattern(const std::string& charDirUtf8, int pattern, const std::string& code)
{
	std::vector<ScriptRef> out;
	if (m->root.empty()) return out;
	const std::string rel = U8(charDirUtf8).lexically_normal().lexically_relative(U8(m->root).lexically_normal()).generic_u8string();
	if (rel.empty() || rel.rfind("..", 0) == 0) return out;
	const std::string key = rel + "|" + std::to_string(pattern) + "|" + code + "|" + std::to_string(m->indexGen);
	if (key == m->refsKey) return m->refsCache;
	for (const std::string& f : m->index.files()) {
		if (f.rfind(rel + "/", 0) != 0) continue;
		const auto* lines = m->index.lines(f);
		if (!lines) continue;
		for (const auto& [line, r] : Impl::ScanRefs(*lines, m->consts))
			if ((r.pattern >= 0 && r.pattern == pattern) || (!r.code.empty() && r.code == code))
				out.push_back({f, line, (*lines)[static_cast<size_t>(line)]});
	}
	// The move table that owns the pattern: "Mv_<a>_<b>_..." whose name, minus "Mv_" and at most one more leading segment,
	// is the PTCN (mv_script.h binding rule) -- then every line in the folder that uses that move.
	if (!code.empty()) {
		for (const ScriptDef& d : m->index.defs()) {
			if (d.kind != ScriptDef::MoveTable || d.file.rfind(rel + "/", 0) != 0 || d.name.rfind("Mv_", 0) != 0) continue;
			std::string tail = d.name.substr(3);
			bool own = tail == code;
			const size_t us = tail.find('_');
			if (!own && us != std::string::npos) own = tail.substr(us + 1) == code;
			if (!own) continue;
			const auto* lines = m->index.lines(d.file);
			out.push_back({d.file, d.line, "[move] " + (lines ? (*lines)[static_cast<size_t>(d.line)] : d.name)});
			for (const ScriptHit& h : m->index.usages(d.name))
				if (h.file.rfind(rel + "/", 0) == 0 && !(h.file == d.file && h.line == d.line)) out.push_back({h.file, h.line, h.text});
		}
	}
	m->refsKey = key;
	m->refsCache = out;
	return out;
}

bool ScriptPanel::showAt(const std::string& rel, int line)
{
	m_visible = true;
	ImGui::SetWindowFocus("###SquirrelScript");
	return m->openAt(rel, line, 0);
}

bool ScriptPanel::scriptCommand(const std::string& sub, const std::string& arg, std::string* msg)
{
	std::string out, err;
	if (sub == "open") { if (!openGameRoot(arg, "", &err)) out = err; else out = m->status; }
	else if (sub == "rescan") { m->rescan(false); out = std::to_string(m->files.size()) + " Squirrel scripts"; }
	else if (sub == "file") { if (!openFile(m->pathOrRoot(arg), &err)) out = err; else out = std::to_string(m->problems.size()) + " problem(s)"; }
	else if (sub == "goto") {
		int line = 0, col = 0;
		sscanf(arg.c_str(), "%d %d", &line, &col);
		m->openAt(m->currentRel, line - 1, col);
		m->editor.SelectRegion(TextEditor::DocPos(line - 1, col), TextEditor::DocPos(line - 1, col));
	}
	else if (sub == "search") { snprintf(m->searchBuf, sizeof m->searchBuf, "%s", arg.c_str()); m->runSearch(); m->bottomTab = 2; m->selectBottomTab = true; out = m->hitsTitle; }
	else if (sub == "usages") { m->findUsages(arg); out = m->hitsTitle; }
	else if (sub == "state") out = std::to_string(m->problems.size()) + " problem(s), tab " + std::to_string(m->bottomTab) + ", lastEdit " + std::to_string(m->lastEditTime) + ", " + std::to_string(m->hits.size()) + " hit(s)";
	else if (sub == "refs") {   // refs <char dir rel> <pattern> [code]
		char dir[256] = {}, code[128] = {};
		int pat = -1;
		sscanf(arg.c_str(), "%255s %d %127s", dir, &pat, code);
		const auto r = refsToPattern(ToU8(U8(m->root) / U8(dir)), pat, code);
		out = std::to_string(r.size()) + " script line(s)";
		for (size_t i = 0; i < r.size() && i < 10; ++i) out += "\n  " + r[i].file + ":" + std::to_string(r[i].line + 1) + " " + r[i].text;
	}
	else if (sub == "hover") {   // hover <Owner.name | name>: what the hover shows for a script definition
		const size_t dot = arg.rfind('.');
		const std::string own = dot == std::string::npos ? "" : arg.substr(0, dot), name = dot == std::string::npos ? arg : arg.substr(dot + 1);
		for (const ScriptDef* d : m->defsFor(name, own)) {
			out += "[script] " + (d->owner.empty() ? "" : d->owner + ".") + d->name + "(" + d->params + ")  " + d->file + ":" + std::to_string(d->line + 1);
			if (!d->doc.empty()) out += "\n  doc: " + d->doc;
			const std::string calls = m->EngineCallsOf(*d);
			if (!calls.empty()) out += "\n  calls engine: " + calls;
			out += "\n";
		}
		if (m->apiByTable.count(arg)) out += "[engine] " + arg + ": " + EngineTableNote(arg);
	}
	else if (sub == "hoverat") {   // hoverat <line> <col> | off: pin the hover tooltip to a text position (1-based line), for screenshots
		int line = 0, col = 0;
		if (arg == "off" || sscanf(arg.c_str(), "%d %d", &line, &col) < 1) { m->scriptHover = false; out = "hover off"; }
		else { m->scriptHover = true; m->scriptHoverPos = TextEditor::DocPos(line - 1, col); out = "hover at " + std::to_string(line) + ":" + std::to_string(col); }
	}
	else if (sub == "hide") m_visible = false;
	else if (sub == "check") {   // the Check button: compile, then (MBTL character files) the game's character load
		std::string bytes;
		std::vector<EncodeError> errs;
		m->doc.encode(SplitLines(m->editor.GetText()), bytes, &errs);
		Diagnostic d = m->vm.compile(bytes, m->currentRel);
		const bool loaded = d.ok && m->characterCheck(bytes, &d);
		if (loaded) m->recheck();
		out = std::string(d.ok ? "ok" : "error line " + std::to_string(d.line) + ": " + d.message) + (loaded ? ", character load ran" : ", single file") +
			", " + std::to_string(m->loadProblems.size()) + " load problem(s)";
		for (const auto& p : m->loadProblems) out += "\n  " + p.file + ":" + std::to_string(p.line + 1) + " " + p.message;
	}
	else if (sub == "drawstats") {   // the last frame's draw lists with the most vertices (16-bit index budget = 65535 per draw cmd)
		const ImGuiIO& io = ImGui::GetIO();
		out = std::string("VtxOffset ") + ((io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) ? "on" : "OFF") + ", ImDrawIdx " + std::to_string(sizeof(ImDrawIdx) * 8) + "-bit, " +
			std::to_string(m->editor.GetLineCount()) + " lines, " + std::to_string(m->outline.size()) + " outline items, " +
			std::to_string(m->outlineRows.size()) + " outline rows, visible rows " + std::to_string(m->editor.GetFirstVisibleRow() + 1) + "-" + std::to_string(m->editor.GetLastVisibleRow() + 1);
		if (const ImDrawData* dd = ImGui::GetDrawData()) {
			std::vector<const ImDrawList*> ls(dd->CmdLists.begin(), dd->CmdLists.end());
			std::sort(ls.begin(), ls.end(), [](auto a, auto b) { return a->VtxBuffer.Size > b->VtxBuffer.Size; });
			for (size_t i = 0; i < ls.size() && i < 4; ++i)
				out += "\n  " + std::string(ls[i]->_OwnerName ? ls[i]->_OwnerName : "?") + ": " + std::to_string(ls[i]->VtxBuffer.Size) + " vtx, " + std::to_string(ls[i]->IdxBuffer.Size) + " idx, " + std::to_string(ls[i]->CmdBuffer.Size) + " cmds";
		}
	}
	else if (sub == "tab") { m->bottomTab = atoi(arg.c_str()); m->selectBottomTab = true; }
	else if (sub == "type") { m->editor.ReplaceTextInCurrentCursor(arg); }
	else if (sub == "save") { requestSave(); out = "save requested"; }   // the next frame runs the Ctrl+S path (checks, backup, hot reload)
	else if (sub == "hotreload") {   // hotreload on|off|ping|status|mode rebuild|live|pipe <name>|gamedir <dir>: the Hot reload row of the panel
		const size_t sp = arg.find(' ');
		const std::string verb = arg.substr(0, sp), rest = sp == std::string::npos ? "" : arg.substr(sp + 1);
		if (verb == "on" || verb == "off") { gSettings.sqHotReload = verb == "on"; out = std::string("hot reload ") + verb; }
		else if (verb == "pipe") { if (!rest.empty()) { m->hotPipe = rest; m->hotPipeGame = m->game; } out = "pipe " + m->hotPipe; }
		else if (verb == "mode") {   // mode rebuild|live
			if (rest == "rebuild" || rest == "live") { gSettings.sqHotReloadMode = rest == "live" ? 1 : 0; ImGui::MarkIniSettingsDirty(); }
			out = std::string("mode ") + (gSettings.sqHotReloadMode == 1 ? "live" : "rebuild");
		}
		else if (verb == "gamedir") { if (!rest.empty()) m->hotGameDirSetting() = rest; out = "game dir " + m->hotGameDir(); }
		else if (verb == "ping") { Impl::HotTask t; t.pipe = m->hotPipe.empty() ? FpanimPipeForGame(m->game) : m->hotPipe; m->startHot(t); out = "ping " + t.pipe; }
		else out = std::string(m->hotBusy ? "busy: " : "") + (m->hotStatusOk ? "ok: " : "error: ") + m->hotStatus;
	}
	else if (sub == "problems") {
		m->recheck();
		out = std::to_string(m->problems.size()) + " problem(s)";
		for (const auto& p : m->problems) out += "\n  " + p.file + ":" + std::to_string(p.line + 1) + " " + p.message;
	}
	else if (sub == "fix") {
		const size_t i = static_cast<size_t>(atoi(arg.c_str()));
		if (i < m->problems.size() && m->problems[i].fix == ScriptProblem::CreateMoveTable) {
			const ScriptProblem p = m->problems[i];
			out = m->addMoveStub(p.fixArg, p.fixFile) ? "added " + p.fixArg + " to " + p.fixFile : m->status;
			m->recheck();
		} else out = "no fix for problem " + arg;
	}
	else if (sub == "complete") {
		const std::string own = arg;
		m->acOwner = own;
		m->collectWords();
		out = std::to_string(m->acWords.size()) + " candidate(s)";
		for (size_t i = 0; i < m->acWords.size() && i < 12; ++i) out += "\n  " + m->acWords[i] + "  " + m->acTag[m->acWords[i]];
	}
	else return false;
	if (msg) *msg = out;
	return true;
}

void ScriptPanel::draw()
{
	m_focused = false;
	if (!m_visible) return;
	ui::FitNextWindow(1000, 700);
	const std::string title = std::string("Script") + (m->dirty ? " *" : "") + (m->currentRel.empty() ? "" : " - " + m->currentRel) + "###SquirrelScript";
	if (!ImGui::Begin(title.c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	m->watchFolders(m_focused);

	if (ImGui::Button(LBL("Open game folder..."))) {
		const std::string dir = BrowseForFolderUtf8(m->root);
		std::string err;
		if (!dir.empty() && !openGameRoot(dir, "", &err)) m->setStatus(err, true);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(m->root.empty());
	ImGui::SetNextItemWidth(ui::Px(110));
	const char* games[] = {"(auto)", "uni2", "mbtl", "dfci", "unib"};
	int gi = 0;
	for (int i = 1; i < 5; i++) if (m->game == games[i]) gi = i;
	if (ImGui::Combo(LBL("Profile"), &gi, games, 5)) {
		std::string err;
		openGameRoot(m->editRoot.empty() ? m->root : m->editRoot, gi ? games[gi] : "", &err);
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Rescan"))) m->rescan(false);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Read the script folders again (new files appear). The list also refreshes by itself when the panel gets focus or a folder changes."));
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m->currentRel.empty());
	if (ImGui::Button(LBL("Save (Ctrl+S)"))) m_saveRequested = true;
	ImGui::SameLine();
	if (ImGui::Button(LBL("Check"))) {
		std::string bytes;
		std::vector<EncodeError> errs;
		if (m->doc.encode(SplitLines(m->editor.GetText()), bytes, &errs)) {
			Diagnostic d = m->vm.compile(bytes, m->currentRel);
			const bool loaded = d.ok && m->characterCheck(bytes, &d);
			m->showDiagnostic(d, "./" + m->currentRel);
			if (loaded) m->recheck();
			m->setStatus(d.ok ? (m->loadProblems.empty() ? "compiles; the character loads clean" : "compiles; see Problems")
				: "line " + std::to_string(d.line) + ": " + d.message, !d.ok);
		}
	}
	ImGui::EndDisabled();
	m->drawHotReload();
	if (m->offerSaveAnyway) {
		ImGui::SameLine();
		if (ImGui::Button(LBL("Save anyway"))) { m->offerSaveAnyway = false; m->forceNextSave = true; m_saveRequested = true; }
	}
	if (!m->status.empty()) {   // after the toolbar while it fits, else on its own wrapped line (the hot-reload row filled it)
		i18n::SameLineFit(std::min(ImGui::CalcTextSize(m->status.c_str()).x, ui::Em(16.f)));
		ImGui::PushStyleColor(ImGuiCol_Text, m->statusError ? ErrCol() : ScriptCol());
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextUnformatted(m->status.c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}

	// File list | editor | outline as one resizable table (drag the borders): 18% / 64% / 18% by default, the editor takes
	// what the side columns leave. Either side column folds away with the toggles on the right of the toolbar.
	static bool showFiles = true, showOutline = true;
	{
		const float tw = i18n::FieldWidth(ImGui::GetFrameHeight(), LBL("Files")) + ImGui::GetStyle().ItemSpacing.x
			+ i18n::FieldWidth(ImGui::GetFrameHeight(), LBL("Outline"));
		const float right = ImGui::GetWindowContentRegionMax().x;   // window-local
		if (ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetStyle().ItemSpacing.x + tw <= right) ImGui::SameLine();
		ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - tw));
		ImGui::Checkbox(LBL("Files"), &showFiles);
		ImGui::SameLine();
		ImGui::Checkbox(LBL("Outline"), &showOutline);
	}
	const float logH = ui::Px(190.0f);
	const bool hasFile = !m->currentRel.empty();
	ImGui::BeginChild("##sqcols", ImVec2(0, -logH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	if (ImGui::BeginTable("##sqcolumns", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
		ImGui::TableSetupColumn("##files", ImGuiTableColumnFlags_WidthStretch, 0.18f);
		ImGui::TableSetupColumn("##editor", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide, 0.64f);
		ImGui::TableSetupColumn("##outline", ImGuiTableColumnFlags_WidthStretch, 0.18f);
		ImGui::TableSetColumnEnabled(0, showFiles);
		ImGui::TableSetColumnEnabled(2, showOutline && hasFile);
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetContentRegionAvail().y);
	if (ImGui::TableSetColumnIndex(0)) {
	ImGui::BeginChild("##files", ImVec2(0, 0), ImGuiChildFlags_Borders);
	ImGui::SetNextItemWidth(-1);
	char buf[128];
	snprintf(buf, sizeof buf, "%s", m->filter.c_str());
	if (ImGui::InputTextWithHint("##filter", "filter", buf, sizeof buf)) m->filter = buf;
	if (!m->editTag.empty()) {
		ImGui::TextColored(WarnCol(), TXT("Editing: %s"), m->editTag.c_str());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Saves go into %s"), m->editRoot.c_str());
	}
	// One row per file with the tag of the copy the game reads; the copies it overrides follow, indented and dimmed.
	for (const ScriptFile& f : m->sfiles) {
		if (!m->filter.empty() && f.rel.find(m->filter) == std::string::npos) continue;
		for (size_t ci = 0; ci < f.copies.size(); ci++) {
			const bool win = static_cast<int>(ci) == f.winner || (f.winner < 0 && ci == 0);
			const ScriptCopy& c = f.copies[ci];
			const std::string tag = "[" + m->layers[static_cast<size_t>(c.layer)].tag + "]";
			const std::string cp = c.path.empty() ? std::string() : ToU8(U8(c.path).lexically_normal());
			const bool sel = f.rel == m->currentRel && (cp.empty() ? Impl::Under(m->currentPath, m->archiveCache) : Impl::LowerS(cp) == Impl::LowerS(m->currentPath));
			const std::string label = (win ? f.rel : "    " + std::string(TXT("overridden"))) + "  " + tag + "###" + f.rel + "#" + std::to_string(ci);
			if (!win) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			const bool click = ImGui::Selectable(label.c_str(), sel);
			if (!win) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", (cp.empty() ? std::string(TXT("in the game archive")) : cp).c_str());
			if (!click) continue;
			if (m->dirty && !sel) m->setStatus("unsaved changes in " + m->currentRel + ": save first", true);
			else {
				std::string err;
				const std::string path = m->pathOfCopy(f, c);
				if (path.empty() || !openFile(path, &err)) m->setStatus(err.empty() ? "cannot read " + f.rel : err, true);
			}
		}
	}
	ImGui::EndChild();
	}
	ImGui::TableSetColumnIndex(1);
	ImGui::BeginChild("##edit", ImVec2(0, 0));
	if (!m->currentRel.empty()) {
		m->editor.Render("##sqtext", ImVec2(-1, -1));
		m->drawHover();
		m->drawCompletion(m_focused);
		m->drawSignatureHelp(m_focused);
		if (m_focused && !ImGui::IsPopupOpen("##sqcomplete") && (ImGui::IsKeyPressed(ImGuiKey_F12, false))) {
			const TextEditor::DocPos cur = m->editor.GetMainCursorPosition();
			const std::string line = m->editor.GetLineText(cur.line);
			std::string word, own;
			Impl::WordAt(line, Impl::ByteCol(line, cur.index), word, own);
			if (!word.empty()) {
				if (ImGui::GetIO().KeyShift) m->findUsages(word);
				else m->goToDefinition(word, own);
			}
		}
	} else ImGui::TextDisabled("%s", TXT("Open a game folder (the one holding script/ and data/), then pick a script."));
	ImGui::EndChild();
	if (hasFile && ImGui::TableSetColumnIndex(2)) {
		ImGui::BeginChild("##outline", ImVec2(0, 0), ImGuiChildFlags_Borders);
		m->drawOutline();
		ImGui::EndChild();
	}
		ImGui::EndTable();
	}
	ImGui::EndChild();
	m->drawBottom();
	m->drawPopups();
	if (m->lastEditTime >= 0 && ImGui::GetTime() - m->lastEditTime > 0.6) { m->lastEditTime = -1.0; m->recheck(); }

	if (m_focused && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S, false)) m_saveRequested = true;
	if (m_focused && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
		m->bottomTab = 2; m->selectBottomTab = true; m->focusSearch = true;
	}
	ImGui::End();

	if (!m_saveRequested || m->currentRel.empty()) { m_saveRequested = false; return; }
	const bool force = m->forceNextSave;
	m->forceNextSave = false;
	m_saveRequested = false;

	// 0. Moves this file switches to that do not exist: warn and offer blank states (never block the save).
	if (!m->skipMovePromptOnce) {
		m->recheck();
		m->pendingMissing.clear();
		for (const auto& p : m->problems)
			if (p.fix == ScriptProblem::CreateMoveTable && p.file == m->currentRel &&
				std::none_of(m->pendingMissing.begin(), m->pendingMissing.end(), [&](const ScriptProblem& q) { return q.fixArg == p.fixArg; }))
				m->pendingMissing.push_back(p);
		if (!m->pendingMissing.empty()) { m->askMissingMoves = true; m->forceNextSave = force; return; }
	}
	m->skipMovePromptOnce = false;

	// 1. CP932 encode (strict; untouched lines keep their original bytes).
	std::string bytes;
	std::vector<EncodeError> errs;
	if (!m->doc.encode(SplitLines(m->editor.GetText()), bytes, &errs)) {
		m->editor.ClearMarkers();
		for (const auto& e : errs) m->editor.AddMarker(static_cast<size_t>(e.line), IM_COL32(220, 50, 50, 255), IM_COL32(140, 20, 20, 110), e.message, e.message);
		m->setStatus("not saved: line " + std::to_string(errs.front().line + 1) + " has a character CP932 cannot represent", true);
		return;
	}
	// 2. Compile with the game's VM; with an API profile also run the top level after the bootstrap.
	Diagnostic d = m->vm.compile(bytes, "./" + m->currentRel);
	if (d.ok && m->characterCheck(bytes, &d)) {
		m->showDiagnostic(d, "./" + m->currentRel);
		m->recheck();
	} else if (!d.ok) {
		m->showDiagnostic(d, "./" + m->currentRel);
	} else if (!m->apiPath.empty()) {
		const Diagnostic boot = m->vm.bootstrap();
		if (!boot.ok) m->say("bootstrap: " + boot.source + ":" + std::to_string(boot.line) + " " + boot.message);
		d = m->vm.compileAndRun(bytes, "./" + m->currentRel);
		m->showDiagnostic(d, "./" + m->currentRel);
	} else {
		d = m->vm.compile(bytes, "./" + m->currentRel);
		m->showDiagnostic(d, "./" + m->currentRel);
	}
	if (!d.ok && !d.runtime && !force) {
		m->offerSaveAnyway = true;
		m->setStatus("not saved: compile error line " + std::to_string(d.line) + ": " + d.message, true);
		return;
	}
	m->offerSaveAnyway = false;
	if (bytes == m->doc.originalBytes()) {
		m->dirty = false;
		m->setStatus(d.ok ? "no changes (file is byte-identical); compiles" : "no changes; " + d.message, !d.ok);
		m->hotReloadSaved();   // unchanged file: still push it (first sync after turning hot reload on)
		return;
	}
	// 3. Backup + atomic replace. The target: the chosen pack / mod folder, else the copy that was opened (an archive copy
	//    goes to the IM mod folder). A new copy in a pack shadows the lower layers, like fpan-im's VFS.
	const fs::path target = U8(m->saveTargetFor(m->currentRel, m->currentPath));
	std::string backup, err;
	std::error_code tec;
	const bool newCopy = !fs::exists(target, tec);
	if (newCopy) fs::create_directories(target.parent_path(), tec);
	else if (!WriteBackup(target, m->doc.originalBytes(), &backup)) { m->setStatus("not saved: cannot write backup", true); return; }
	if (!WriteAtomic(target, bytes, &err)) { m->setStatus("not saved: " + err, true); return; }
	if (Impl::LowerS(ToU8(target.lexically_normal())) != Impl::LowerS(m->currentPath)) {
		m->say("saved a new copy of " + m->currentRel + " into " + ToU8(target));
		m->currentPath = ToU8(target.lexically_normal());
		m->rescan(true);
	}
	m->doc.commit(bytes);
	m->dirty = false;
	if (d.ok) m->setStatus("saved " + m->currentRel + " (" + std::to_string(bytes.size()) + " bytes); compiles" + (m->apiPath.empty() ? "" : " and runs against the " + m->game + " stubs"), false);
	else m->setStatus("saved with " + std::string(d.runtime ? "a runtime warning" : "a COMPILE ERROR") + " (" + d.source + ":" + std::to_string(d.line) + ") " + d.message, true);
	m->hotReloadSaved();
}

} // namespace sqedit
