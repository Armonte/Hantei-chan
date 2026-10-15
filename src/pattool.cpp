// pattool: part set renumbering from the command line (src/parts/pat_renumber.h, Hantei_Docs/pat_renumber.md).
//   pattool refs <pat>                                  users of the .pat and the part set ids their frames draw
//   pattool renumber <pat> [ops] [--dry-run] [--no-backup]
//       ops, applied in order: --move a-b:to  --place a-b:to  --insert at:n  --delete a-b  --clear a-b  --swap a:b
//       Writes the .pat and every user HA6 (backups next to them, backup/<name>.<time>.bak); refused (exit 3) when a
//       reference cannot be remapped safely. --dry-run prints the report only.
//   pattool check <pat> --tmp <dir> [ops] [--expect-refused]   on a copy of the .pat folder: a no-op save is byte-identical, the
//       ops applied and saved draw the same part set at every reference, and undoing them restores identical bytes.
// Paths are UTF-8 (MSYS / wslpath -w). CLI tool: plain stdout, like cgmtool.
#include "parts/parts.h"
#include "parts/parts_undo.h"
#include "parts/pat_renumber.h"
#include "cgm/cgm_project.h"
#include "file_backup.h"
#include "framedata.h"
#include "png_writer.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string Ansi(const std::string &s)
{
	const std::wstring w = Utf8ToWide(s);
	const int n = WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string o(n > 0 ? n : 0, '\0'); if (n > 0) WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), o.data(), n, nullptr, nullptr);
	return o;
}
static bool Slurp(const std::string &p, std::string &out)
{
	std::ifstream f(fs::path(Utf8ToWide(p)), std::ios::binary); if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}

static bool ParseOps(int argc, char **argv, int from, std::vector<patrn::Op> &ops, std::map<std::string, std::string> &flags)
{
	static const std::pair<const char *, patrn::Op::Kind> kinds[] = {{"--move", patrn::Op::Move}, {"--place", patrn::Op::Place},
		{"--insert", patrn::Op::Insert}, {"--delete", patrn::Op::Delete}, {"--clear", patrn::Op::Clear}, {"--swap", patrn::Op::Swap}};
	for (int i = from; i < argc; i++) {
		bool done = false;
		for (auto &k : kinds) if (!strcmp(argv[i], k.first)) {
			if (i + 1 >= argc) { printf("%s needs an argument\n", argv[i]); return false; }
			patrn::Op op; std::string err;
			if (!patrn::ParseOp(k.second, argv[++i], op, &err)) { printf("%s\n", err.c_str()); return false; }
			ops.push_back(op); done = true; break;
		}
		if (done) continue;
		if (!strcmp(argv[i], "--tmp") && i + 1 < argc) { flags["tmp"] = argv[++i]; continue; }
		if (!strncmp(argv[i], "--", 2)) { flags[argv[i] + 2] = "1"; continue; }
		printf("unknown argument %s\n", argv[i]); return false;
	}
	return true;
}

// Parts owns GL objects (Vao) whose destructors need a context: without one they are never destroyed (pat_roundtrip
// exits with _exit for the same reason).
static Parts &NewParts() { return *new Parts(nullptr); }

static void PrintPlan(const patrn::Plan &plan)
{
	for (auto &f : plan.files)
		printf("  %-60s %5d refs, %5d change%s\n", f.path.c_str(), f.refs, f.changed, f.shared ? " (shared: never rewritten)" : "");
	for (auto &r : plan.refusals) printf("  REFUSED: %s\n", r.c_str());
	printf("  %d references change\n", plan.changed);
}

// Applies the ops; prints the plan. False when an op fails or the plan refuses.
static bool Apply(Parts &parts, const patrn::Users &users, const std::vector<patrn::Op> &ops, std::vector<int> &diskRemap, patrn::Plan &plan)
{
	const int extent = patrn::MaxRef(users) + 1;
	for (auto &op : ops) {
		std::string err;
		if (!patrn::ApplyOp(parts, op, extent, &err)) { printf("  %s: %s\n", patrn::Describe(op).c_str(), err.c_str()); return false; }
		printf("  %s\n", patrn::Describe(op).c_str());
	}
	diskRemap = patrn::PendingDiskRemap(parts, extent);
	plan = patrn::MakePlan(users, diskRemap);
	PrintPlan(plan);
	return plan.ok();
}

static void BackupCb(const std::string &utf8, void *) { BackupOptions o; std::string e; if (!BackupBeforeSave(Ansi(utf8), o, nullptr, &e)) printf("  backup failed: %s\n", e.c_str()); }

static int CmdRefs(const std::string &pat)
{
	const patrn::Users u = patrn::FindUsers(pat);
	printf("%s: %zu user files, %d references\n", pat.c_str(), u.files.size(), u.refCount());
	for (auto &f : u.files) {
		std::map<int, int> ids; for (auto &r : f.refs) ids[r.second]++;
		printf("  %s%s%s%s: %zu refs, %zu ids\n", f.path.c_str(), f.shared ? " [shared]" : "", f.missing ? " [missing]" : "", f.error.empty() ? "" : (" [" + f.error + "]").c_str(), f.refs.size(), ids.size());
	}
	return 0;
}

static int CmdRenumber(const std::string &pat, const std::vector<patrn::Op> &ops, bool dry, bool backup)
{
	Parts &parts = NewParts();
	if (!parts.Load(Ansi(pat).c_str())) { printf("cannot load %s\n", pat.c_str()); return 2; }
	const patrn::Users users = patrn::FindUsers(pat);
	printf("%s: %zu part set slots, %zu user files, %d references\n", pat.c_str(), parts.partSets.size(), users.files.size(), users.refCount());
	std::vector<int> remap; patrn::Plan plan;
	if (!Apply(parts, users, ops, remap, plan)) { printf("not applied\n"); return 3; }
	if (dry) { printf("dry run: nothing written\n"); return 0; }
	if (backup) BackupCb(pat, nullptr);
	if (!parts.Save(Ansi(pat).c_str())) { printf("cannot write %s\n", pat.c_str()); return 4; }
	const patrn::WriteResult w = patrn::WriteUsers(users, remap, backup ? BackupCb : nullptr, nullptr);
	for (auto &p : w.written) printf("  wrote %s\n", p.c_str());
	for (auto &e : w.errors) printf("  ERROR %s\n", e.c_str());
	printf("wrote the .pat and %zu HA6 files (%d references)\n", w.written.size(), w.refs);
	return w.errors.empty() ? 0 : 4;
}

// Fingerprint of what part set `id` draws (Parts::Fingerprint ignores the id itself).
// An id past the end and an empty slot both draw nothing: 0.
static uint64_t SetFp(const Parts &p, int id)
{
	if (id < 0 || id >= (int)p.partSets.size() || (p.partSets[id].groups.empty() && p.partSets[id].name.empty())) return 0;
	return p.Fingerprint('P', (uint32_t)id);
}

static int CmdCheck(const std::string &pat, const std::string &tmp, const std::vector<patrn::Op> &ops, bool expectRefused)
{
	int fail = 0;
	auto bad = [&](const std::string &m) { printf("FAIL %s\n", m.c_str()); fail++; };
	// copy the .pat's folder (project .txt files, the .pat, the HA6 files) into tmp
	std::error_code ec;
	const fs::path src = fs::path(Utf8ToWide(pat)).parent_path(), dst = fs::path(Utf8ToWide(tmp));
	fs::remove_all(dst, ec); fs::create_directories(dst, ec);
	const patrn::Users orig = patrn::FindUsers(pat);
	auto copyIn = [&](const fs::path &f) { fs::copy_file(f, dst / f.filename(), fs::copy_options::overwrite_existing, ec); };
	for (auto &e : fs::directory_iterator(src, ec)) { const std::string x = e.path().extension().string(); if (_stricmp(x.c_str(), ".txt") == 0) copyIn(e.path()); }
	copyIn(fs::path(Utf8ToWide(pat)));
	for (auto &f : orig.files) if (!f.shared && !f.missing) copyIn(fs::path(Utf8ToWide(f.path)));
	const std::string tpat = WideToUtf8((dst / fs::path(Utf8ToWide(pat)).filename()).wstring());
	std::string patBytes; Slurp(tpat, patBytes);
	std::map<std::string, std::string> before;
	const patrn::Users users = patrn::FindUsers(tpat);
	for (auto &f : users.files) Slurp(f.path, before[f.path]);

	// 1. no-op round trip
	{
		Parts &p = NewParts(); p.Load(Ansi(tpat).c_str());
		const std::string out = tpat + ".noop";
		std::string b; if (!p.Save(Ansi(out).c_str()) || !Slurp(out, b) || b != patBytes) bad("no-op save is not byte-identical");
		fs::remove(fs::path(Utf8ToWide(out)), ec);
	}
	// 2. the ops, through the undo channel: undo must restore identical bytes
	Parts &parts = NewParts(); parts.Load(Ansi(tpat).c_str());
	Parts &ref = NewParts(); ref.Load(Ansi(tpat).c_str());
	PartsUndoChannel ch(&parts);
	const UndoChannel::Snap snap = ch.capture(nullptr);
	std::vector<int> remap; patrn::Plan plan;
	if (!Apply(parts, users, ops, remap, plan)) {
		printf("SECTION pat-renumber-check: %s: refused%s\n", pat.c_str(), expectRefused ? " (expected)" : "");
		return !fail && expectRefused ? 0 : 3;
	}
	if (expectRefused) bad("the operations were expected to be refused");
	{
		Parts &u = NewParts(); u.Load(Ansi(tpat).c_str());
		PartsUndoChannel uch(&u);
		const UndoChannel::Snap s0 = uch.capture(nullptr);
		std::vector<int> r2; patrn::Plan p2;
		Apply(u, users, ops, r2, p2);
		uch.restore(s0);
		const std::string out = tpat + ".undo"; std::string b;
		if (!u.Save(Ansi(out).c_str()) || !Slurp(out, b) || b != patBytes) bad("undo did not restore a byte-identical .pat");
		else printf("  undo: byte-identical .pat\n");
		fs::remove(fs::path(Utf8ToWide(out)), ec);
	}
	(void)snap;
	// 3. save, write the users, reload: every reference draws the same part set
	if (!parts.Save(Ansi(tpat).c_str())) bad("cannot save the .pat");
	const patrn::WriteResult w = patrn::WriteUsers(users, remap, nullptr, nullptr);
	for (auto &e : w.errors) bad(e);
	Parts &after = NewParts(); after.Load(Ansi(tpat).c_str());
	const patrn::Users users2 = patrn::FindUsers(tpat);
	int refs = 0, same = 0;
	for (size_t fi = 0; fi < users.files.size() && fi < users2.files.size(); fi++) {
		const auto &a = users.files[fi], &b = users2.files[fi];
		if (a.shared) continue;
		if (a.refs.size() != b.refs.size()) { bad(a.path + ": reference count changed"); continue; }
		std::string now; Slurp(a.path, now);
		const std::string &old = before[a.path];
		// every byte but the remapped ids is identical
		std::vector<bool> idByte(old.size());
		for (auto &r : a.refs) for (int k = 0; k < 4; k++) if (r.first + k < idByte.size()) idByte[r.first + k] = true;
		if (now.size() != old.size()) bad(a.path + ": size changed");
		else for (size_t k = 0; k < old.size(); k++) if (old[k] != now[k] && !idByte[k]) { bad(a.path + ": a byte other than a part set id changed at " + std::to_string(k)); break; }
		for (size_t k = 0; k < a.refs.size(); k++) {
			refs++;
			if (SetFp(ref, a.refs[k].second) == SetFp(after, b.refs[k].second)) same++;
			else bad(a.path + ": reference at " + std::to_string(a.refs[k].first) + " drew " + std::to_string(a.refs[k].second) + ", now " + std::to_string(b.refs[k].second) + " (a different part set)");
		}
	}
	// the part set content itself: same multiset of non-empty sets, cut-outs / textures untouched by construction
	{
		std::map<uint64_t, int> c;
		for (size_t i = 0; i < ref.partSets.size(); i++) if (!ref.partSets[i].groups.empty() || !ref.partSets[i].name.empty()) c[SetFp(ref, (int)i)]++;
		for (size_t i = 0; i < after.partSets.size(); i++) if (!after.partSets[i].groups.empty() || !after.partSets[i].name.empty()) c[SetFp(after, (int)i)]--;
		bool del = false; for (auto &op : ops) del |= op.kind == patrn::Op::Delete || op.kind == patrn::Op::Clear;
		for (auto &kv : c) if (kv.second < 0 || (kv.second > 0 && !del)) { bad("the set of part sets changed beyond the operations"); break; }
	}
	printf("SECTION pat-renumber-check: %s: %zu users, %d refs, %d draw the same part set, %zu files rewritten, %d failures\n",
		pat.c_str(), users.files.size(), refs, same, w.written.size(), fail);
	return fail ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("usage: pattool refs <pat> | renumber <pat> [--move a-b:to] [--place a-b:to] [--insert at:n] [--delete a-b] [--clear a-b] [--swap a:b] [--dry-run] [--no-backup]\n"
		       "       pattool check <pat> --tmp <dir> [ops] [--expect-refused]\n");
		return 1;
	}
	setvbuf(stdout, nullptr, _IONBF, 0);
	const std::string cmd = argv[1], pat = argv[2];
	std::vector<patrn::Op> ops; std::map<std::string, std::string> flags;
	if (!ParseOps(argc, argv, 3, ops, flags)) return 1;
	if (cmd == "refs") return CmdRefs(pat);
	if (cmd == "renumber") return CmdRenumber(pat, ops, flags.count("dry-run"), !flags.count("no-backup"));
	if (cmd == "check") { if (!flags.count("tmp")) { printf("check needs --tmp <dir>\n"); return 1; } return CmdCheck(pat, flags["tmp"], ops, flags.count("expect-refused")); }
	printf("unknown command %s\n", cmd.c_str());
	return 1;
}
