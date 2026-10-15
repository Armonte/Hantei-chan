// pattool: part set renumbering from the command line (src/parts/pat_renumber.h, Hantei_Docs/pat_renumber.md).
//   pattool refs <pat>                                  users of the .pat and the part set ids their frames draw
//   pattool renumber <pat> [ops] [--dry-run] [--no-backup]
//       ops, applied in order: --move a-b:to  --place a-b:to  --insert at:n  --delete a-b  --clear a-b  --swap a:b
//       Writes the .pat and every user HA6 (backups next to them, backup/<name>.<time>.bak); refused (exit 3) when a
//       reference cannot be remapped safely. --dry-run prints the report only.
//   pattool check <pat> --tmp <dir> [ops] [--expect-refused]   on a copy of the .pat folder: a no-op save is byte-identical, the
//       ops applied and saved draw the same part set at every reference, and undoing them restores identical bytes.
//   pattool append-from <dst.pat> <src.pat> -o <out.pat> --map <map.tsv> [--sets 1,5,7-9]
//       appends source part sets (default: every non-empty one) with their cut-outs, shapes and textures to the
//       destination, at ids >= its part set count (src/parts/pat_append.h); map.tsv: src_partset_id<TAB>new_id.
//   pattool append-check <dst.pat> <src.pat> --tmp <dir> [--sets ...]   the destination's own records stay
//       byte-identical, and each appended part set draws the same parts / cut-outs / shapes / texture pixels as in the source.
// Paths are UTF-8 (MSYS / wslpath -w). CLI tool: plain stdout, like cgmtool.
#include "parts/parts.h"
#include "parts/parts_undo.h"
#include "parts/pat_renumber.h"
#include "parts/pat_append.h"
#include "parts/pat_texconv.h"
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

// --- append-from ---
struct AppendArgs { std::string out, map, sets, tmp; };
static bool ParseAppendArgs(int argc, char **argv, int from, AppendArgs &a)
{
	for (int i = from; i < argc; i++) {
		const std::string k = argv[i];
		if (i + 1 >= argc) { printf("%s needs an argument\n", argv[i]); return false; }
		if (k == "-o") a.out = argv[++i];
		else if (k == "--map") a.map = argv[++i];
		else if (k == "--sets") a.sets = argv[++i];
		else if (k == "--tmp") a.tmp = argv[++i];
		else { printf("unknown argument %s\n", argv[i]); return false; }
	}
	return true;
}

static void PrintAppend(const patrn::AppendResult &r, size_t dstCount)
{
	printf("  %zu part sets appended", r.setMap.size());
	if (!r.setMap.empty()) printf(" at %d-%d (destination had %zu slots)", r.setMap.front().second, r.setMap.back().second, dstCount);
	printf("\n  cut-outs: %d added, %d reused; shapes: %d added, %d reused; textures: %d added, %d reused\n",
		r.cutOutsAdded, r.cutOutsReused, r.shapesAdded, r.shapesReused, r.texturesAdded, r.texturesReused);
	if (!r.skippedEmpty.empty()) printf("  %zu requested source ids are empty slots (not appended)\n", r.skippedEmpty.size());
	for (auto &n : r.notes) printf("  NOTE %s\n", n.c_str());
}

static int CmdAppend(const std::string &dstPat, const std::string &srcPat, const AppendArgs &a, std::string *outPath, patrn::AppendResult *outRes)
{
	if (a.out.empty()) { printf("append-from needs -o <out.pat>\n"); return 1; }
	std::vector<int> ids; std::string err;
	if (!a.sets.empty() && !patrn::ParseIdList(a.sets, ids, &err)) { printf("%s\n", err.c_str()); return 1; }
	Parts &dst = NewParts(), &src = NewParts();
	if (!dst.Load(Ansi(dstPat).c_str())) { printf("cannot load %s\n", dstPat.c_str()); return 2; }
	if (!src.Load(Ansi(srcPat).c_str())) { printf("cannot load %s\n", srcPat.c_str()); return 2; }
	const size_t dstCount = dst.partSets.size();
	printf("%s: %zu part set slots, %zu cut-outs, %zu textures (%s)\n", dstPat.c_str(), dstCount, dst.cutOuts.size(), dst.gfxMeta.size(), dst.useMBAACCFormat ? "MBAACC" : "UNI2 / MBTL");
	printf("%s: %zu part set slots, %zu cut-outs, %zu textures (%s)\n", srcPat.c_str(), src.partSets.size(), src.cutOuts.size(), src.gfxMeta.size(), src.useMBAACCFormat ? "MBAACC" : "UNI2 / MBTL");
	patrn::AppendResult r;
	if (!patrn::AppendPartSets(dst, src, ids, r, &err)) { printf("not appended: %s\n", err.c_str()); return 3; }
	PrintAppend(r, dstCount);
	if (r.setMap.empty()) { printf("nothing to append\n"); return 3; }
	if (!dst.Save(Ansi(a.out).c_str())) { printf("cannot write %s\n", a.out.c_str()); return 4; }
	std::error_code ec;
	printf("wrote %s (%llu bytes)\n", a.out.c_str(), (unsigned long long)fs::file_size(fs::path(Utf8ToWide(a.out)), ec));
	if (!a.map.empty()) {
		std::ofstream m(fs::path(Utf8ToWide(a.map)), std::ios::binary);
		for (auto &p : r.setMap) m << p.first << '\t' << p.second << '\n';
		if (!m) { printf("cannot write %s\n", a.map.c_str()); return 4; }
		printf("wrote %s (%zu ids)\n", a.map.c_str(), r.setMap.size());
	}
	if (outPath) *outPath = a.out;
	if (outRes) *outRes = r;
	return 0;
}

static bool TexEq(const Parts &a, int ta, const Parts &b, int tb)
{
	if (ta < 0 || tb < 0 || ta >= (int)a.gfxMeta.size() || tb >= (int)b.gfxMeta.size()) return false;
	std::vector<uint8_t> pa, pb; int wa, ha, wb, hb;
	return pattex::Decode(a.gfxMeta[ta], pa, wa, ha, nullptr) && pattex::Decode(b.gfxMeta[tb], pb, wb, hb, nullptr) && wa == wb && ha == hb && pa == pb;
}
static bool ShapeGeomEq(const Parts &a, int sa, const Parts &b, int sb)
{
	// a .pat without shapes (MBAACC) draws every cut-out as a plane
	auto get = [](const Parts &p, int s) { Shape<> d; if (s >= 0 && s < (int)p.shapes.size()) d = p.shapes[s]; return d; };
	const Shape<> x = get(a, sa), y = get(b, sb);
	if (x.type == ShapeType::PLANE && y.type == ShapeType::PLANE && (a.shapes.empty() || b.shapes.empty())) return true;
	return x.type == y.type && x.vertexCount == y.vertexCount && x.vertexCount2 == y.vertexCount2 && x.length == y.length && x.length2 == y.length2 &&
		x.radius == y.radius && x.dRadius == y.dRadius && x.width == y.width && x.dz == y.dz &&
		x.rectW == y.rectW && x.rectH == y.rectH && x.rectX == y.rectX && x.rectY == y.rectY;
}

static int CmdAppendCheck(const std::string &dstPat, const std::string &srcPat, AppendArgs a)
{
	if (a.tmp.empty()) { printf("append-check needs --tmp <dir>\n"); return 1; }
	std::error_code ec;
	fs::remove_all(fs::path(Utf8ToWide(a.tmp)), ec); fs::create_directories(fs::path(Utf8ToWide(a.tmp)), ec);
	a.out = WideToUtf8((fs::path(Utf8ToWide(a.tmp)) / "out.pat").wstring());
	a.map = WideToUtf8((fs::path(Utf8ToWide(a.tmp)) / "map.tsv").wstring());
	patrn::AppendResult r; std::string out;
	if (int rc = CmdAppend(dstPat, srcPat, a, &out, &r)) return rc;
	int fail = 0;
	auto bad = [&](const std::string &m) { if (fail < 30) printf("FAIL %s\n", m.c_str()); fail++; };
	Parts &dst = NewParts(), &src = NewParts(), &o = NewParts();
	dst.Load(Ansi(dstPat).c_str()); src.Load(Ansi(srcPat).c_str());
	if (!o.Load(Ansi(out).c_str())) { printf("FAIL cannot reload %s\n", out.c_str()); return 1; }
	// 1. the destination's own records: byte-identical, same ids
	std::map<std::pair<char, uint32_t>, const std::string *> outRaw;
	for (auto &rr : o.rawRecords) outRaw.emplace(std::make_pair(rr.kind, rr.id), &rr.raw);
	int own = 0;
	for (auto &rr : dst.rawRecords) {
		auto it = outRaw.find({rr.kind, rr.id});
		if (it == outRaw.end()) { bad(std::string("record ") + rr.kind + " " + std::to_string(rr.id) + " is missing"); continue; }
		if (*it->second != rr.raw && rr.kind != 'V') bad(std::string("record ") + rr.kind + " " + std::to_string(rr.id) + " changed");
		own++;
	}
	for (size_t i = 0; i < dst.partSets.size(); i++)
		if (i >= o.partSets.size() || dst.Fingerprint('P', (uint32_t)i) != o.Fingerprint('P', (uint32_t)i)) bad("part set " + std::to_string(i) + " of the destination changed");
	for (size_t i = 0; i < dst.cutOuts.size(); i++)
		if (i >= o.cutOuts.size() || dst.Fingerprint('C', (uint32_t)i) != o.Fingerprint('C', (uint32_t)i)) bad("cut-out " + std::to_string(i) + " of the destination changed");
	for (size_t i = 0; i < dst.gfxMeta.size(); i++)
		if (!TexEq(dst, (int)i, o, (int)i)) bad("texture " + std::to_string(i) + " of the destination changed");
	for (size_t i = 0; i < dst.shapes.size(); i++)
		if (i >= o.shapes.size() || !ShapeGeomEq(dst, (int)i, o, (int)i)) bad("shape " + std::to_string(i) + " of the destination changed");
	// 2. every appended part set draws what it drew in the source
	int parts = 0, texChecked = 0;
	std::map<std::pair<int, int>, bool> texCache;
	for (auto &m : r.setMap) {
		const std::string tag = "part set " + std::to_string(m.first) + " -> " + std::to_string(m.second) + ": ";
		if (m.second < (int)dst.partSets.size()) { bad(tag + "new id is below the destination's count"); continue; }
		if (m.second >= (int)o.partSets.size()) { bad(tag + "missing from the output"); continue; }
		const PartSet<> &s = src.partSets[m.first], &n = o.partSets[m.second];
		if (std::string(s.name.data(), s.name.size()) != std::string(n.name.data(), n.name.size())) bad(tag + "name differs");
		if (s.groups.size() != n.groups.size()) { bad(tag + "part count differs"); continue; }
		for (size_t k = 0; k < s.groups.size(); k++) {
			PartProperty x = s.groups[k], y = n.groups[k];
			const int cx = x.ppId, cy = y.ppId; x.ppId = y.ppId = 0;
			parts++;
			if (memcmp(&x, &y, sizeof x)) {
				// compare field by field (padding bytes)
				const bool same = x.propId == y.propId && x.priority == y.priority && !memcmp(x.rotation, y.rotation, sizeof x.rotation) && !memcmp(x.pras, y.pras, sizeof x.pras) &&
					x.x == y.x && x.y == y.y && x.scaleX == y.scaleX && x.scaleY == y.scaleY && x.additive == y.additive && x.filter == y.filter && x.pral == y.pral &&
					x.prfl == y.prfl && x.prpa == y.prpa && x.hasPrpa == y.hasPrpa && x.flip == y.flip && !memcmp(x.bgra, y.bgra, 4) && !memcmp(x.addColor, y.addColor, sizeof x.addColor);
				if (!same) { bad(tag + "part " + std::to_string(k) + " transform / blend differs"); continue; }
			}
			const bool hx = cx >= 0 && cx < (int)src.cutOuts.size(), hy = cy >= 0 && cy < (int)o.cutOuts.size();
			if (hx != hy) { bad(tag + "part " + std::to_string(k) + " cut-out presence differs"); continue; }
			if (!hx) continue;
			const CutOut<> &a1 = src.cutOuts[cx], &b1 = o.cutOuts[cy];
			if (memcmp(a1.uv, b1.uv, sizeof a1.uv) || memcmp(a1.xy, b1.xy, sizeof a1.xy) || memcmp(a1.wh, b1.wh, sizeof a1.wh) || a1.colorSlot != b1.colorSlot ||
				memcmp(a1.ppjp, b1.ppjp, sizeof a1.ppjp) || memcmp(a1.ppte, b1.ppte, sizeof a1.ppte) || a1.pptx != b1.pptx)
				bad(tag + "part " + std::to_string(k) + " cut-out " + std::to_string(cx) + " -> " + std::to_string(cy) + " differs");
			if (!ShapeGeomEq(src, a1.shapeIndex, o, b1.shapeIndex)) bad(tag + "part " + std::to_string(k) + " shape differs");
			auto key = std::make_pair(a1.texture, b1.texture);
			auto t = texCache.find(key);
			if (t == texCache.end()) { t = texCache.emplace(key, TexEq(src, a1.texture, o, b1.texture)).first; texChecked++; }
			if (!t->second) bad(tag + "part " + std::to_string(k) + " texture " + std::to_string(a1.texture) + " -> " + std::to_string(b1.texture) + " pixels differ");
		}
	}
	printf("SECTION pat-append-check: %s + %s: %d own records byte-identical, %zu sets appended, %d parts, %d texture pairs compared, %d failures\n",
		dstPat.c_str(), srcPat.c_str(), own, r.setMap.size(), parts, texChecked, fail);
	return fail ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("usage: pattool refs <pat> | renumber <pat> [--move a-b:to] [--place a-b:to] [--insert at:n] [--delete a-b] [--clear a-b] [--swap a:b] [--dry-run] [--no-backup]\n"
		       "       pattool check <pat> --tmp <dir> [ops] [--expect-refused]\n"
		       "       pattool append-from <dst.pat> <src.pat> -o <out.pat> --map <map.tsv> [--sets 1,5,7-9]\n"
		       "       pattool append-check <dst.pat> <src.pat> --tmp <dir> [--sets 1,5,7-9]\n");
		return 1;
	}
	setvbuf(stdout, nullptr, _IONBF, 0);
	const std::string cmd = argv[1], pat = argv[2];
	if (cmd == "append-from" || cmd == "append-check") {
		AppendArgs a;
		if (argc < 4 || !ParseAppendArgs(argc, argv, 4, a)) { printf("usage: pattool %s <dst.pat> <src.pat> %s [--sets 1,5,7-9]\n", cmd.c_str(), cmd == "append-from" ? "-o <out.pat> --map <map.tsv>" : "--tmp <dir>"); return 1; }
		return cmd == "append-from" ? CmdAppend(pat, argv[3], a, nullptr, nullptr) : CmdAppendCheck(pat, argv[3], a);
	}
	std::vector<patrn::Op> ops; std::map<std::string, std::string> flags;
	if (!ParseOps(argc, argv, 3, ops, flags)) return 1;
	if (cmd == "refs") return CmdRefs(pat);
	if (cmd == "renumber") return CmdRenumber(pat, ops, flags.count("dry-run"), !flags.count("no-backup"));
	if (cmd == "check") { if (!flags.count("tmp")) { printf("check needs --tmp <dir>\n"); return 1; } return CmdCheck(pat, flags["tmp"], ops, flags.count("expect-refused")); }
	printf("unknown command %s\n", cmd.c_str());
	return 1;
}
