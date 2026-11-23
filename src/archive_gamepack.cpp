// MBAACC game archives (0000.p ... 0008.p): unpack into the game folder (what dump.exe did), restore the packed archives, pack loose files back into a .p.
//
// Game behaviour (MBAA.exe, IDA): FilePac_MountGameArchives_InPriorityOrder (0x41E470) mounts ./0008, 0003, 0005, 0002, 0007, 0000, 0004, 0006, 0001.p
// and every file open asks FilePac_FindEntry (0x4DB8E0) first: the FIRST mounted archive that holds the path wins, and the loose file on disk is only
// read when no archive has it. So packed files win over loose files: unpacking does nothing for the game unless the .p files leave the game folder.
// Unpack therefore writes every file (the version of the archive the game would pick) and then moves the .p files into <game>\_packed_backup.
#include "app_log.h"
#include "archive_browser_state.h"
#include "game_table.h"
#include "filedialog.h"
#include "png_writer.h"
#include "i18n.h"

#include <windows.h>
#include <imgui.h>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace abrowser {

namespace {
const char *const kBackupDir = "_packed_backup";
// Mount order of MBAA.exe: index 0 has the highest priority.
const char *const kGameOrder[] = { "0008.p", "0003.p", "0005.p", "0002.p", "0007.p", "0000.p", "0004.p", "0006.p", "0001.p" };

std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
fs::path P8(const std::string &u8) { return fs::u8path(u8); }
bool IsDigitP(const std::string &n) { return n.size() == 6 && isdigit((unsigned char)n[0]) && isdigit((unsigned char)n[1]) && isdigit((unsigned char)n[2]) && isdigit((unsigned char)n[3]) && Lower(n.substr(4)) == ".p"; }
int Rank(const std::string &fileName)
{
	const std::string l = Lower(fileName);
	for (int i = 0; i < (int)(sizeof kGameOrder / sizeof *kGameOrder); i++) if (l == kGameOrder[i]) return i;
	return 100;
}
std::string Fmt(const char *fmt, ...)
{
	char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); return b;
}
std::string SizeMB(uint64_t n) { return Fmt("%.1f MB", n / (1024.0 * 1024.0)); }

struct ArcRef { std::string name, path; };   // file name ("0002.p"), UTF-8 path
// The NNNN.p FilePacHeaderA archives of a folder, highest game priority first.
std::vector<ArcRef> ListArchives(const fs::path &dir)
{
	std::vector<ArcRef> out;
	std::error_code ec;
	if (!fs::is_directory(dir, ec)) return out;
	for (auto &e : fs::directory_iterator(dir, ec)) {
		std::error_code e2;
		if (!e.is_regular_file(e2)) continue;
		const std::string n = e.path().filename().u8string();
		if (!IsDigitP(n)) continue;
		std::ifstream f(e.path(), std::ios::binary); char h[16]{}; f.read(h, 14);
		if (memcmp(h, "FilePacHeaderA", 14) != 0) continue;
		out.push_back({ n, e.path().u8string() });
	}
	std::sort(out.begin(), out.end(), [](const ArcRef &a, const ArcRef &b) { int ra = Rank(a.name), rb = Rank(b.name); return ra != rb ? ra < rb : Lower(a.name) < Lower(b.name); });
	return out;
}
// Entry path as a safe relative file system path (UTF-8, '/' separators, no "..", no leading '/').
std::string SafeRel(const fbarc::Archive &a, size_t i)
{
	std::string rel = fbarc::NameToUtf8(a.relativePath(i));
	for (auto &c : rel) if (c == '\\') c = '/';
	while (rel.find("../") != std::string::npos) rel.erase(rel.find("../"), 3);
	while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
	return rel;
}
bool ReadFileBytes(const fs::path &p, std::vector<uint8_t> &out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}
bool MoveFileU8(const fs::path &from, const fs::path &to, bool replace)
{
	return MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != 0;
}
bool UnderProtected(const std::string &dir)
{
	const std::string l = Lower(dir);
	return l.rfind("c:\\program files", 0) == 0 || l.rfind("c:/program files", 0) == 0 || l.rfind("c:\\windows", 0) == 0;
}

// ---- shared job state ---------------------------------------------------------------------------------------------------------------------------------
std::atomic<bool> g_busy{false};
std::mutex g_mx;
std::string g_result;          // last result (English, one paragraph)
bool g_resultOk = true;
void SetResult(const std::string &r, bool ok)
{
	{ std::lock_guard<std::mutex> lk(g_mx); g_result = r; g_resultOk = ok; }
	if (ok) HC_LOG_INFO("game archives: {}", r); else HC_LOG_WARN("game archives: {}", r);
}
bool BeginJob(const char *op, int total, const std::string &dest)
{
	State &st = S();
	if (st.prog.running || g_busy) return false;
	g_busy = true;
	st.prog.done = 0; st.prog.failed = 0; st.prog.total = total; st.prog.cancel = false; st.prog.running = true;
	st.prog.op = op; st.prog.dest = dest; st.prog.what.clear();
	return true;
}
void EndJob()   // worker thread
{
	PostMain([] { S().prog.running = false; });
	g_busy = false;
}

// After the .p files moved or changed: re-read the browser's view of that game folder (mounted archives that left are dropped).
void RefreshGameInBrowser(const std::string &gameDir)
{
	State &st = S();
	std::error_code ec;
	const std::string want = Lower(fs::absolute(P8(gameDir), ec).lexically_normal().u8string());
	for (auto &g : st.groups) {
		if (!g || g->info.root.empty()) continue;
		if (Lower(fs::absolute(P8(g->info.root), ec).lexically_normal().u8string()) != want) continue;
		for (int i = 0; i < (int)st.sources.size(); i++) if (st.sources[i] == g->folder) {
			const bool wasShown = show;
			CloseSource(i);
			std::string e = OpenPath(gameDir); (void)e;
			show = wasShown;
			return;
		}
	}
}

// ---- unpack ---------------------------------------------------------------------------------------------------------------------------------------------
struct UnpackPlan {
	std::string dir, error;
	std::vector<ArcRef> archives;
	std::vector<std::shared_ptr<fbarc::Archive>> open;
	struct File { std::string rel; uint32_t arc = 0, idx = 0; uint64_t size = 0; bool exists = false; };
	std::vector<File> files;              // one per path: the copy the game reads (highest-priority archive)
	size_t entries = 0, shadowed = 0, existing = 0;
	uint64_t bytes = 0;
	bool backupHasArchives = false;
	std::vector<std::string> existingSample;
};

std::shared_ptr<UnpackPlan> MakeUnpackPlan(const std::string &dir)
{
	auto p = std::make_shared<UnpackPlan>(); p->dir = dir;
	std::error_code ec;
	if (!fs::is_directory(P8(dir), ec)) { p->error = "Folder not found: " + dir; return p; }
	p->archives = ListArchives(P8(dir));
	p->backupHasArchives = !ListArchives(P8(dir) / kBackupDir).empty();
	if (p->archives.empty()) { p->error = p->backupHasArchives ? "No .p archives in this folder (they are in " + std::string(kBackupDir) + ": already unpacked?)" : "No MBAACC .p archives (0000.p ... 0008.p) in " + dir; return p; }
	std::map<std::string, size_t> seen;   // lower rel -> index in files
	for (uint32_t k = 0; k < p->archives.size(); k++) {
		std::string err;
		std::shared_ptr<fbarc::Archive> a(fbarc::Open(p->archives[k].path, &err).release());
		if (!a) { p->error = p->archives[k].name + ": " + err; return p; }
		p->open.push_back(a);
		for (uint32_t i = 0; i < a->entries().size(); i++) {
			p->entries++;
			std::string rel = SafeRel(*a, i);
			if (rel.empty()) continue;
			if (!seen.emplace(Lower(rel), p->files.size()).second) { p->shadowed++; continue; }   // a higher-priority archive already gives this path
			UnpackPlan::File f; f.rel = rel; f.arc = k; f.idx = i; f.size = a->entries()[i].size;
			f.exists = fs::exists(P8(dir) / P8(rel), ec);
			if (f.exists) { p->existing++; if (p->existingSample.size() < 8) p->existingSample.push_back(rel); }
			p->bytes += f.size;
			p->files.push_back(std::move(f));
		}
	}
	return p;
}

// Worker: writes the files, then moves the archives into the backup folder (only when every file was written).
void RunUnpack(std::shared_ptr<UnpackPlan> p, bool overwrite, const std::string &prefix = std::string())
{
	State &st = S();
	size_t written = 0, skipped = 0, failed = 0;
	std::string firstErr;
	for (auto &f : p->files) {
		if (st.prog.cancel) break;
		if (f.exists && !overwrite) { skipped++; st.prog.done++; continue; }
		std::vector<uint8_t> b; std::string err;
		bool ok = p->open[f.arc]->read(f.idx, b, &err);
		if (ok) {
			std::error_code ec;
			const fs::path out = P8(p->dir) / P8(f.rel);
			fs::create_directories(out.parent_path(), ec);
			std::ofstream o(out, std::ios::binary | std::ios::trunc);
			if (!b.empty()) o.write((const char *)b.data(), (std::streamsize)b.size());
			ok = (bool)o;
			if (!ok) err = "could not write " + out.u8string();
		}
		if (ok) written++; else { failed++; st.prog.failed++; if (firstErr.empty()) firstErr = f.rel + ": " + err; }
		st.prog.done++;
	}
	p->open.clear();   // no reader holds the archives while they move
	std::string r = prefix + Fmt("Unpacked %zu files into %s (%zu skipped because they already existed, %zu failed).", written, p->dir.c_str(), skipped, failed);
	bool ok = true;
	if (st.prog.cancel) { r += " Cancelled: the .p files were NOT moved, the game still reads the packed files."; ok = false; }
	else if (failed) { r += " First error: " + firstErr + ". The .p files were NOT moved, the game still reads the packed files."; ok = false; }
	else {
		std::error_code ec;
		const fs::path bdir = P8(p->dir) / kBackupDir;
		fs::create_directories(bdir, ec);
		std::vector<std::string> moved, notMoved;
		for (auto &a : p->archives) {
			const fs::path to = bdir / P8(a.name);
			if (fs::exists(to, ec)) { notMoved.push_back(a.name + " (" + std::string(kBackupDir) + " already has one)"); continue; }
			if (MoveFileU8(P8(a.path), to, false)) moved.push_back(a.name);
			else notMoved.push_back(a.name + Fmt(" (error %lu: is the game running?)", GetLastError()));
		}
		r += Fmt(" Moved %zu .p files into %s.", moved.size(), bdir.u8string().c_str());
		if (!notMoved.empty()) {
			ok = false;
			r += " NOT moved (the game still reads these packed files first):";
			for (auto &n : notMoved) r += " " + n;
			r += ".";
		}
	}
	SetResult(r, ok);
	const std::string dir = p->dir;
	PostMain([dir] { RefreshGameInBrowser(dir); });
	EndJob();
}

// ---- restore --------------------------------------------------------------------------------------------------------------------------------------------
std::string RunRestore(const std::string &dir, bool *ok)
{
	std::error_code ec;
	const fs::path bdir = P8(dir) / kBackupDir;
	auto arcs = ListArchives(bdir);
	if (arcs.empty()) { *ok = false; return "Nothing to restore: no .p files in " + bdir.u8string(); }
	std::vector<std::string> moved, notMoved;
	for (auto &a : arcs) {
		const fs::path to = P8(dir) / P8(a.name);
		if (fs::exists(to, ec)) { notMoved.push_back(a.name + " (the game folder already has one)"); continue; }
		if (MoveFileU8(P8(a.path), to, false)) moved.push_back(a.name);
		else notMoved.push_back(a.name + Fmt(" (error %lu)", GetLastError()));
	}
	if (fs::is_empty(bdir, ec)) fs::remove(bdir, ec);
	std::string r = Fmt("Moved %zu .p files back into %s. Packed files win over loose files again: the game ignores loose files that an archive also holds (the loose files were left on disk).", moved.size(), dir.c_str());
	*ok = notMoved.empty();
	if (!notMoved.empty()) { r += " Not moved:"; for (auto &n : notMoved) r += " " + n; r += "."; }
	SetResult(r, *ok);
	RefreshGameInBrowser(dir);
	return r;
}

// ---- pack -----------------------------------------------------------------------------------------------------------------------------------------------
struct PackJob {
	std::vector<std::string> archives;    // UTF-8 .p paths
	std::string gameDir;                  // where the loose files are
	bool addNew = false, force = false;
};

// The game folder that belongs to an archive: the parent of _packed_backup, else the archive's own folder.
std::string GameDirOfArchive(const std::string &arc)
{
	fs::path d = P8(arc).parent_path();
	if (Lower(d.filename().u8string()) == Lower(kBackupDir)) d = d.parent_path();
	return d.u8string();
}

// Every archive the pack looks at (siblings decide which archive the game reads a path from): the game folder, its backup folder and the archive's own folder.
std::vector<ArcRef> Siblings(const std::string &gameDir, const std::string &arcPath)
{
	std::vector<ArcRef> all;
	std::set<std::string> names;
	auto addFrom = [&](const fs::path &d) { for (auto &a : ListArchives(d)) if (names.insert(Lower(a.name)).second) all.push_back(a); };
	addFrom(P8(arcPath).parent_path());
	addFrom(P8(gameDir));
	addFrom(P8(gameDir) / kBackupDir);
	return all;
}

std::string PackOne(const std::string &arcPath, const PackJob &job, bool *ok)
{
	State &st = S();
	*ok = false;
	std::string err;
	auto a = fbarc::Open(arcPath, &err);
	if (!a) { return P8(arcPath).filename().u8string() + ": " + err; }
	if (a->kind() != fbarc::Kind::MbFilePacA) return P8(arcPath).filename().u8string() + ": not an MBAACC FilePacHeaderA archive";
	const std::string me = P8(arcPath).filename().u8string();
	const int myRank = Rank(me);
	// paths held by higher-priority archives (the game reads those copies; a loose edit of them belongs in that archive)
	std::set<std::string> higher, anyArchive;
	for (auto &s : Siblings(job.gameDir, arcPath)) {
		if (Lower(s.name) == Lower(me)) continue;
		auto o = fbarc::Open(s.path, &err);
		if (!o) continue;
		const bool isHigher = Rank(s.name) < myRank;
		for (size_t i = 0; i < o->entries().size(); i++) { std::string r = Lower(SafeRel(*o, i)); anyArchive.insert(r); if (isHigher) higher.insert(r); }
	}
	fbarc::Edit ed;
	size_t replaced = 0, unchanged = 0, shadowed = 0, noLoose = 0, added = 0, tooLong = 0, readFail = 0;
	std::set<std::string> mine, dirs;
	for (size_t i = 0; i < a->entries().size(); i++) {
		if (st.prog.cancel) break;
		st.prog.done++;
		const std::string rel = SafeRel(*a, i);
		mine.insert(Lower(rel));
		const size_t sl = rel.rfind('/');
		if (sl != std::string::npos) dirs.insert(rel.substr(0, sl));
		std::error_code ec;
		const fs::path lp = P8(job.gameDir) / P8(rel);
		if (!fs::is_regular_file(lp, ec)) { noLoose++; continue; }
		if (higher.count(Lower(rel))) { shadowed++; continue; }
		std::vector<uint8_t> loose;
		if (!ReadFileBytes(lp, loose)) { readFail++; continue; }
		bool differs = loose.size() != a->entries()[i].size;
		if (!differs) {
			std::vector<uint8_t> packed;
			if (!a->read(i, packed, &err)) { readFail++; continue; }
			differs = packed != loose;
		}
		if (differs) { ed.replace[i] = std::move(loose); replaced++; } else unchanged++;
	}
	if (st.prog.cancel) return me + ": cancelled, nothing written.";
	if (job.addNew) {
		// loose files in the archive's own folders (not the game root) that no archive holds
		for (auto &d : dirs) {
			std::error_code ec;
			for (auto &e : fs::directory_iterator(P8(job.gameDir) / P8(d), ec)) {
				std::error_code e2;
				if (!e.is_regular_file(e2)) continue;
				const std::string rel = d + "/" + e.path().filename().u8string();
				if (mine.count(Lower(rel)) || anyArchive.count(Lower(rel))) continue;
				const std::string cpName = fbarc::NameFromUtf8(e.path().filename().u8string());
				if (cpName.empty() || cpName.size() > 32) { tooLong++; continue; }
				std::vector<uint8_t> b;
				if (!ReadFileBytes(e.path(), b)) { readFail++; continue; }
				ed.add.emplace_back(fbarc::NameFromUtf8(rel), std::move(b));
				added++;
			}
		}
	}
	std::string r = me + Fmt(": %zu replaced, %zu added, %zu unchanged, %zu without a loose file", replaced, added, unchanged, noLoose);
	if (shadowed) r += Fmt(", %zu skipped (a higher-priority archive holds them: the game reads that copy)", shadowed);
	if (tooLong) r += Fmt(", %zu new files skipped (name longer than 32 bytes)", tooLong);
	if (readFail) r += Fmt(", %zu could not be read", readFail);
	if (ed.replace.empty() && ed.add.empty() && !job.force) { *ok = readFail == 0; return r + ". Nothing changed: the archive was left as it is."; }
	const fs::path arc = P8(arcPath);
	fs::path tmp = arc; tmp += ".hantei-new";
	if (!a->rebuild(tmp.u8string(), ed, &err)) { std::error_code ec; fs::remove(tmp, ec); return r + ". Rebuild FAILED: " + err; }
	a.reset();
	fs::path bak = arc; bak += ".bak";
	std::error_code ec;
	std::string bakNote;
	if (!fs::exists(bak, ec)) {
		if (!MoveFileU8(arc, bak, false)) { fs::remove(tmp, ec); return r + Fmt(". Could not keep the previous archive as %s (error %lu: is the game running?); nothing changed.", bak.u8string().c_str(), GetLastError()); }
		bakNote = " Previous archive kept as " + bak.filename().u8string() + ".";
	} else bakNote = " " + bak.filename().u8string() + " already existed and was kept (it is not overwritten).";
	if (!MoveFileU8(tmp, arc, true)) {
		const DWORD e = GetLastError();
		if (!fs::exists(arc, ec) && bakNote.rfind(" Previous", 0) == 0) MoveFileU8(bak, arc, false);   // put the original back
		return r + Fmt(". Could not replace %s (error %lu); the new archive is %s.", arc.u8string().c_str(), e, tmp.u8string().c_str());
	}
	*ok = readFail == 0;
	r += ". Wrote " + arc.u8string() + "." + bakNote;
	if (Lower(arc.parent_path().filename().u8string()) == Lower(kBackupDir)) r += " It is in the backup folder: Restore packed archives puts it back in use.";
	return r;
}

void RunPack(PackJob job)
{
	std::string all; bool allOk = true;
	for (auto &p : job.archives) {
		if (S().prog.cancel) break;
		bool ok = false;
		const std::string r = PackOne(p, job, &ok);
		allOk = allOk && ok;
		if (!all.empty()) all += "\n";
		all += r;
	}
	SetResult(all.empty() ? std::string("No archive packed.") : all, allOk && !S().prog.cancel);
	std::vector<std::string> arcs = job.archives;
	PostMain([arcs] { for (auto &a : arcs) ReloadArchive(a); });
	EndJob();
}

int CountEntries(const std::vector<std::string> &arcs)
{
	int n = 0;
	for (auto &p : arcs) { std::string e; auto a = fbarc::Open(p, &e); if (a) n += (int)a->entries().size(); }
	return n;
}

bool StartPack(const PackJob &job)
{
	const int total = CountEntries(job.archives);
	if (!BeginJob("Packing", total, job.gameDir)) return false;
	PostJob([job] { RunPack(job); });
	return true;
}

// ---- dialogs --------------------------------------------------------------------------------------------------------------------------------------------
enum class Dlg { None, Unpack, Restore, Pack };
Dlg g_dlg = Dlg::None;
bool g_openPopup = false;
std::string g_dir;
enum class Phase { Scanning, Confirm, Running, Done } g_phase = Phase::Confirm;
std::shared_ptr<UnpackPlan> g_plan;
uint64_t g_planSerial = 0;
struct PackPick { std::string path; bool on = true; };
std::vector<PackPick> g_packList;
bool g_packAdd = false;

std::string DefaultGameDir()
{
	State &st = S();
	auto groupOf = [&](int sid) -> std::shared_ptr<Group> { SourceP s = FindSource(sid); if (!s) return nullptr; for (auto &g : st.groups) if (g && g->id == s->group) return g; return nullptr; };
	if (auto g = groupOf(st.selSource)) if (g->info.game == Game::MBAACC && !g->info.root.empty()) return g->info.root;
	for (auto &g : st.groups) if (g && g->info.game == Game::MBAACC && !g->info.root.empty()) return g->info.root;
	if (ScanDone() && !InstallOf(Game::MBAACC).path.empty()) return InstallOf(Game::MBAACC).path;
	return RememberedDir(Game::MBAACC);
}

void ScanPlan()
{
	g_phase = Phase::Scanning; g_plan.reset();
	const uint64_t serial = ++g_planSerial;
	const std::string dir = g_dir;
	PostJob([dir, serial] {
		auto p = MakeUnpackPlan(dir);
		PostMain([p, serial] { if (serial != g_planSerial) return; g_plan = p; if (g_phase == Phase::Scanning) g_phase = Phase::Confirm; });
	});
}

void FillPackList()
{
	g_packList.clear();
	if (g_dir.empty()) return;
	for (auto &a : ListArchives(P8(g_dir) / kBackupDir)) g_packList.push_back({ a.path, true });
	for (auto &a : ListArchives(P8(g_dir))) g_packList.push_back({ a.path, true });
}

void Open(Dlg d, const std::string &dir)
{
	g_dlg = d; g_openPopup = true;
	g_dir = dir.empty() ? DefaultGameDir() : dir;
	g_phase = Phase::Confirm;
	if (d == Dlg::Unpack) { if (!g_dir.empty()) ScanPlan(); else g_plan.reset(); }
	if (d == Dlg::Pack) { FillPackList(); g_packAdd = false; }
}

const char *PopupId() { return "###gamepack"; }

bool FolderRow()
{
	ImGui::TextUnformatted(TXT("Game folder:"));
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(.55f, .8f, 1.f, 1.f), "%s", g_dir.empty() ? TXT("(none chosen)") : g_dir.c_str());
	ImGui::SameLine();
	if (g_phase != Phase::Running && ImGui::SmallButton(LBL("Change..."))) {
		std::string d = BrowseForFolderUtf8(g_dir);
		if (!d.empty()) { g_dir = d; return true; }
	}
	if (!g_dir.empty() && UnderProtected(g_dir))
		ImGui::TextColored(ImVec4(1.f, .65f, .3f, 1.f), "%s", TXT("This folder is under Program Files: Windows may refuse the writes. Copy the game somewhere else (e.g. C:\\Games) if it fails."));
	return false;
}

void ProgressRow()
{
	State &st = S();
	const int total = st.prog.total.load(), done = st.prog.done.load();
	ImGui::ProgressBar(total ? (float)done / (float)total : 0.f, ImVec2(ImGui::GetContentRegionAvail().x, 0), Fmt("%d / %d", done, total).c_str());
	if (ImGui::Button(LBL("Cancel"))) st.prog.cancel = true;
}

bool DoneRow()
{
	std::string r; bool ok;
	{ std::lock_guard<std::mutex> lk(g_mx); r = g_result; ok = g_resultOk; }
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.f);
	ImGui::TextColored(ok ? ImVec4(.5f, .9f, .5f, 1.f) : ImVec4(1.f, .55f, .4f, 1.f), "%s", ok ? TXT("Done.") : TXT("Finished with problems:"));
	ImGui::TextUnformatted(r.c_str());
	ImGui::PopTextWrapPos();
	return ImGui::Button(LBL("Close"));
}

void UnpackBody()
{
	if (FolderRow()) ScanPlan();
	ImGui::Separator();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.f);
	if (g_phase == Phase::Running) { ImGui::TextUnformatted(TXT("Unpacking...")); ImGui::PopTextWrapPos(); ProgressRow(); if (!g_busy) g_phase = Phase::Done; return; }
	if (g_phase == Phase::Done) { ImGui::PopTextWrapPos(); if (DoneRow()) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); } return; }
	if (g_dir.empty()) { ImGui::TextUnformatted(TXT("Choose the MBAACC folder (the one with MBAA.exe and 0000.p ... 0008.p).")); ImGui::PopTextWrapPos(); if (ImGui::Button(LBL("Cancel"))) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); } return; }
	if (g_phase == Phase::Scanning || !g_plan) {
		ImGui::TextUnformatted(TXT("Reading the archives..."));
		ImGui::PopTextWrapPos();
		if (ImGui::Button(LBL("Cancel"))) { g_planSerial++; g_dlg = Dlg::None; g_plan.reset(); ImGui::CloseCurrentPopup(); }
		return;
	}
	const UnpackPlan &p = *g_plan;
	if (!p.error.empty()) {
		ImGui::TextColored(ImVec4(1.f, .55f, .4f, 1.f), "%s", i18n::TrDetail(p.error).c_str());
		ImGui::PopTextWrapPos();
		if (ImGui::Button(LBL("Close"))) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); }
		return;
	}
	std::string names; for (auto &a : p.archives) names += (names.empty() ? "" : ", ") + a.name;
	ImGui::Text(TXT("This writes %zu files (%s) from %zu archives (%s) into the game folder, at their archive paths (data\\..., grp\\...)."), p.files.size(), SizeMB(p.bytes).c_str(), p.archives.size(), names.c_str());
	if (p.shadowed) ImGui::Text(TXT("%zu entries exist in more than one archive: the copy the game reads (0008 > 0003 > 0005 > 0002 > 0007 > 0000 > 0004 > 0006 > 0001) is written."), p.shadowed);
	ImGui::Text(TXT("Then the .p files are MOVED into %s\\%s, because the game always reads packed files before loose ones."), g_dir.c_str(), kBackupDir);
	ImGui::TextUnformatted(TXT("Undo: Game > Game archives (MBAACC .p) > Restore packed archives moves them back (the loose files stay on disk)."));
	ImGui::TextUnformatted(TXT("Needs about as much free disk space as the archives take."));
	if (p.backupHasArchives) ImGui::TextColored(ImVec4(1.f, .65f, .3f, 1.f), "%s", TXT("The backup folder already holds .p files: an archive with the same name there is not overwritten and stays in the game folder."));
	ImGui::PopTextWrapPos();
	ImGui::Separator();
	bool go = false, overwrite = false;
	if (p.existing) {
		ImGui::TextColored(ImVec4(1.f, .8f, .3f, 1.f), TXT("%zu of these files already exist as loose files, e.g.:"), p.existing);
		for (auto &s : p.existingSample) ImGui::BulletText("%s", s.c_str());
		if (ImGui::Button(LBL("Overwrite all"))) { go = true; overwrite = true; }
		ImGui::SameLine();
		if (ImGui::Button(LBL("Skip existing"))) go = true;
		ImGui::SameLine();
	} else {
		if (ImGui::Button(LBL("Unpack"))) go = true;
		ImGui::SameLine();
	}
	if (ImGui::Button(LBL("Cancel"))) { g_dlg = Dlg::None; g_plan.reset(); ImGui::CloseCurrentPopup(); return; }
	if (go) {
		auto plan = g_plan;
		if (BeginJob("Unpacking", (int)plan->files.size(), plan->dir)) {
			g_phase = Phase::Running;
			PostJob([plan, overwrite] { RunUnpack(plan, overwrite); });
		}
	}
}

void RestoreBody()
{
	FolderRow();
	ImGui::Separator();
	if (g_phase == Phase::Done) { if (DoneRow()) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); } return; }
	auto arcs = g_dir.empty() ? std::vector<ArcRef>() : ListArchives(P8(g_dir) / kBackupDir);
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.f);
	if (arcs.empty()) ImGui::TextUnformatted(TXT("No .p files in the backup folder of this game folder: nothing to restore."));
	else {
		std::string names; for (auto &a : arcs) names += (names.empty() ? "" : ", ") + a.name;
		ImGui::Text(TXT("Moves %zu archives (%s) from %s back into the game folder."), arcs.size(), names.c_str(), kBackupDir);
		ImGui::TextUnformatted(TXT("Packed files win over loose files again: the game ignores every loose file that an archive also holds. The loose files are left on disk; use Pack first to keep your edits."));
		ImGui::TextUnformatted(TXT("An archive whose name already exists in the game folder is not moved."));
	}
	ImGui::PopTextWrapPos();
	if (!arcs.empty() && !g_busy && ImGui::Button(LBL("Restore"))) { bool ok; RunRestore(g_dir, &ok); g_phase = Phase::Done; }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Cancel"))) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); }
}

void PackBody()
{
	if (FolderRow()) FillPackList();
	ImGui::Separator();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.f);
	if (g_phase == Phase::Running) { ImGui::TextUnformatted(TXT("Packing...")); ImGui::PopTextWrapPos(); ProgressRow(); if (!g_busy) g_phase = Phase::Done; return; }
	if (g_phase == Phase::Done) { ImGui::PopTextWrapPos(); if (DoneRow()) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); } return; }
	ImGui::TextUnformatted(TXT("For every entry of the checked archives: when the game folder has a loose file at that path and its bytes differ, the entry is replaced with the loose file. Archives with no change are not rewritten."));
	ImGui::TextUnformatted(TXT("The new archive is written next to the old one and swapped in; the previous one is kept as <name>.p.bak (an existing .bak is never overwritten)."));
	ImGui::PopTextWrapPos();
	ImGui::Separator();
	if (g_packList.empty()) ImGui::TextDisabled("%s", TXT("No 0000.p ... 0008.p in the game folder or its backup folder."));
	for (size_t i = 0; i < g_packList.size(); i++) {
		ImGui::PushID((int)i);
		ImGui::Checkbox(g_packList[i].path.c_str(), &g_packList[i].on);
		ImGui::PopID();
	}
	if (ImGui::SmallButton(LBL("Add another .p file..."))) {
		std::string f = AnsiToUtf8(FileDialog(fileType::HAN2, false));
		if (!f.empty()) { g_packList.push_back({ f, true }); if (g_dir.empty()) g_dir = GameDirOfArchive(f); }
	}
	ImGui::Checkbox(LBL("Also add loose files that are not in any archive"), &g_packAdd);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Only files in the folders the archive already has (e.g. data\\), never the game root. Names are limited to 32 bytes."));
	PackJob job; job.gameDir = g_dir; job.addNew = g_packAdd;
	for (auto &p : g_packList) if (p.on) job.archives.push_back(p.path);
	ImGui::BeginDisabled(job.archives.empty() || g_dir.empty() || g_busy || S().prog.running);
	if (ImGui::Button(LBL("Pack"))) { if (StartPack(job)) g_phase = Phase::Running; }
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(LBL("Cancel"))) { g_dlg = Dlg::None; ImGui::CloseCurrentPopup(); }
}
} // namespace

// ---- public -----------------------------------------------------------------------------------------------------------------------------------------------
void UnpackGameDialog(const std::string &dir) { Open(Dlg::Unpack, dir); }
void RestorePackedDialog(const std::string &dir) { Open(Dlg::Restore, dir); }
void PackArchivesDialog(const std::string &dir) { Open(Dlg::Pack, dir); }
bool GamePackBusy() { return g_busy; }
std::string GamePackResult(bool *ok) { std::lock_guard<std::mutex> lk(g_mx); if (ok) *ok = g_resultOk; return g_result; }
bool HasPackedBackup(const std::string &dir) { return !dir.empty() && !ListArchives(P8(dir) / kBackupDir).empty(); }
bool HasGameArchives(const std::string &dir) { return !dir.empty() && !ListArchives(P8(dir)).empty(); }

void DrawGamePackDialogs()
{
	if (g_dlg == Dlg::None) return;
	const char *title = g_dlg == Dlg::Unpack ? TXT("Unpack game archives") : g_dlg == Dlg::Restore ? TXT("Restore packed archives") : TXT("Pack loose files into .p");
	const std::string id = std::string(title) + PopupId();
	if (g_openPopup) { ImGui::OpenPopup(id.c_str()); g_openPopup = false; }
	ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetFontSize() * 30.f, 0), ImVec2(ImGui::GetFontSize() * 60.f, ImGui::GetIO().DisplaySize.y * 0.9f));
	if (!ImGui::BeginPopupModal(id.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { if (!g_busy) g_dlg = Dlg::None; return; }
	switch (g_dlg) {
	case Dlg::Unpack: UnpackBody(); break;
	case Dlg::Restore: RestoreBody(); break;
	case Dlg::Pack: PackBody(); break;
	default: break;
	}
	ImGui::EndPopup();
}

// Script commands (no dialogs; gamepackdialog only shows one): unpackgame <dir> (skip existing, auto-confirm) | restorepacked <dir> | packgame [+add] [+force] <dir | archive.p>
// The job runs on the browser worker; GamePackBusy() / GamePackResult() tell the script runner when it finished and how.
bool GamePackScript(const std::string &cmd, const std::string &argIn, std::string *msg)
{
	auto say = [&](const std::string &m) { if (msg) *msg = m; };
	if (cmd == "unpackgame") {
		const std::string dir = argIn;
		if (!BeginJob("Unpacking", 0, dir)) { say("busy"); return false; }
		SetResult("running", true);
		PostJob([dir] {
			auto p = MakeUnpackPlan(dir);
			if (!p->error.empty()) { SetResult(p->error, false); EndJob(); return; }
			S().prog.total = (int)p->files.size();
			HC_LOG_INFO("unpackgame {}: {} archives, {} entries, {} unique files, {} shadowed, {} existing", dir, p->archives.size(), p->entries, p->files.size(), p->shadowed, p->existing);
			const std::string head = Fmt("[%zu archives, %zu entries, %zu unique paths, %zu shadowed duplicates, %zu already existed] ", p->archives.size(), p->entries, p->files.size(), p->shadowed, p->existing);
			RunUnpack(p, false, head);
		});
		return true;
	}
	if (cmd == "gamepackdialog") {   // gamepackdialog <unpack|restore|pack> <dir>: shows the dialog (screenshots of its text); "gamepackdialog close" closes it
		const size_t sp = argIn.find(' ');
		const std::string which = argIn.substr(0, sp), dir = sp == std::string::npos ? std::string() : argIn.substr(sp + 1);
		if (which == "close") { g_dlg = Dlg::None; return true; }
		Open(which == "restore" ? Dlg::Restore : which == "pack" ? Dlg::Pack : Dlg::Unpack, dir);
		return true;
	}
	if (cmd == "restorepacked") {
		if (g_busy || S().prog.running) { say("busy"); return false; }
		bool ok = false;
		say(RunRestore(argIn, &ok));
		return ok;
	}
	if (cmd == "packgame") {
		PackJob job;
		std::string a = argIn;
		for (;;) {
			if (a.rfind("+add ", 0) == 0) { job.addNew = true; a = a.substr(5); }
			else if (a.rfind("+force ", 0) == 0) { job.force = true; a = a.substr(7); }
			else break;
		}
		std::error_code ec;
		if (fs::is_directory(P8(a), ec)) {
			job.gameDir = a;
			for (auto &x : ListArchives(P8(a) / kBackupDir)) job.archives.push_back(x.path);
			for (auto &x : ListArchives(P8(a))) job.archives.push_back(x.path);
		} else if (fs::is_regular_file(P8(a), ec)) { job.archives.push_back(a); job.gameDir = GameDirOfArchive(a); }
		if (job.archives.empty()) { say("no .p archive at " + a); return false; }
		SetResult("running", true);
		if (!StartPack(job)) { say("busy"); return false; }
		return true;
	}
	return false;
}

} // namespace abrowser
