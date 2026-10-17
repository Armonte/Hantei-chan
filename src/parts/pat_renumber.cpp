// Part set renumbering. See pat_renumber.h.
#include "pat_renumber.h"
#include "../cgm/cgm_project.h"
#include "../framedata.h"
#include "../png_writer.h"
#include "parts.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace patrn {
namespace fs = std::filesystem;

static bool ParseRange(const std::string &s, int &a, int &b)
{
	const size_t d = s.find('-', 1);
	char *e = nullptr;
	a = (int)strtol(s.c_str(), &e, 10);
	if (e == s.c_str()) return false;
	if (d == std::string::npos) { b = a; return *e == 0; }
	if ((size_t)(e - s.c_str()) != d) return false;
	const char *p = s.c_str() + d + 1; b = (int)strtol(p, &e, 10);
	return e != p && *e == 0 && a >= 0 && b >= a;
}

bool ParseOp(Op::Kind kind, const std::string &arg, Op &op, std::string *err)
{
	op = Op(); op.kind = kind;
	const size_t c = arg.find(':');
	auto fail = [&](const char *usage) { if (err) *err = "bad argument '" + arg + "' (expected " + usage + ")"; return false; };
	switch (kind) {
	case Op::Move: case Op::Place:
		if (c == std::string::npos || !ParseRange(arg.substr(0, c), op.a, op.b)) return fail("a-b:to");
		op.to = atoi(arg.c_str() + c + 1); return op.to >= 0 && op.a >= 0 ? true : fail("a-b:to");
	case Op::Insert:
		if (c == std::string::npos) return fail("at:n");
		op.to = atoi(arg.substr(0, c).c_str()); op.n = atoi(arg.c_str() + c + 1);
		return op.to >= 0 && op.n > 0 ? true : fail("at:n");
	case Op::Delete: case Op::Clear:
		return ParseRange(arg, op.a, op.b) && op.a >= 0 ? true : fail("a-b");
	case Op::Swap:
		if (c == std::string::npos) return fail("a:b");
		op.a = atoi(arg.substr(0, c).c_str()); op.b = atoi(arg.c_str() + c + 1);
		return op.a >= 0 && op.b >= 0 && op.a != op.b ? true : fail("a:b");
	}
	return false;
}

std::string Describe(const Op &op)
{
	char b[96];
	switch (op.kind) {
	case Op::Move: snprintf(b, sizeof b, "move %d-%d to %d", op.a, op.b, op.to); break;
	case Op::Place: snprintf(b, sizeof b, "place %d-%d at %d", op.a, op.b, op.to); break;
	case Op::Insert: snprintf(b, sizeof b, "insert %d empty at %d", op.n, op.to); break;
	case Op::Delete: snprintf(b, sizeof b, "delete %d-%d (close up)", op.a, op.b); break;
	case Op::Clear: snprintf(b, sizeof b, "clear %d-%d (leave gaps)", op.a, op.b); break;
	case Op::Swap: snprintf(b, sizeof b, "swap %d and %d", op.a, op.b); break;
	}
	return b;
}

bool BuildRemap(int count, const Op &op, const std::vector<bool> &empty, std::vector<int> &remap, int *newCount, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	remap.resize((size_t)count);
	for (int i = 0; i < count; i++) remap[i] = i;
	int nc = count;
	const int len = op.b - op.a + 1;
	const bool blockOp = op.kind != Op::Insert && op.kind != Op::Swap;
	if (blockOp && (op.a < 0 || op.b < op.a || op.b >= count)) return fail("part sets " + std::to_string(op.a) + "-" + std::to_string(op.b) + " are not in the list (0-" + std::to_string(count - 1) + ")");
	switch (op.kind) {
	case Op::Move: {
		// the list without the block, then the block inserted at `to` (empty ids pad a target past the end)
		std::vector<int> rest;
		for (int i = 0; i < count; i++) if (i < op.a || i > op.b) rest.push_back(i);
		if (op.to > (int)rest.size()) { const int pad = op.to - (int)rest.size(); for (int k = 0; k < pad; k++) rest.push_back(-1); }
		std::vector<int> order(rest.begin(), rest.begin() + op.to);
		for (int i = op.a; i <= op.b; i++) order.push_back(i);
		order.insert(order.end(), rest.begin() + op.to, rest.end());
		for (size_t k = 0; k < order.size(); k++) if (order[k] >= 0) remap[order[k]] = (int)k;
		nc = std::max(count, (int)order.size());
		break;
	}
	case Op::Place: {
		for (int k = 0; k < len; k++) {
			const int t = op.to + k;
			if (t < count && (t < op.a || t > op.b) && !(t < (int)empty.size() && empty[t]))
				return fail("part set " + std::to_string(t) + " is not empty: Place only fills empty ids (use Move to shift)");
		}
		for (int k = 0; k < len; k++) remap[op.a + k] = op.to + k;
		// the target ids' empty slots go where the block was (so no id holds two slots)
		std::vector<int> freed;
		for (int k = 0; k < len; k++) { const int s = op.a + k; if (s < op.to || s >= op.to + len) freed.push_back(s); }
		size_t f = 0;
		for (int k = 0; k < len; k++) { const int t = op.to + k; if (t < count && (t < op.a || t > op.b)) remap[t] = freed[f++]; }
		nc = std::max(count, op.to + len);
		break;
	}
	case Op::Insert:
		if (op.n <= 0) return fail("nothing to insert");
		for (int i = 0; i < count; i++) if (i >= op.to) remap[i] = i + op.n;
		nc = std::max(count, op.to) + op.n;
		break;
	case Op::Delete:
		for (int i = 0; i < count; i++) remap[i] = i < op.a ? i : i > op.b ? i - len : -1;
		nc = count - len;
		break;
	case Op::Clear:
		for (int i = op.a; i <= op.b; i++) remap[i] = -1;
		break;
	case Op::Swap:
		if (op.a >= count || op.b >= count) return fail("part set " + std::to_string(std::max(op.a, op.b)) + " is not in the list");
		remap[op.a] = op.b; remap[op.b] = op.a;
		break;
	}
	if (newCount) *newCount = nc;
	return true;
}

// --- users on disk ---
static std::string Lower(std::string s) { for (char &c : s) c = (char)tolower((unsigned char)c); return s; }
static std::string DirOf(const std::string &p) { const size_t k = p.find_last_of("\\/"); return k == std::string::npos ? std::string(".") : p.substr(0, k); }
static std::string Utf8ToAnsi(const std::string &s)
{
	const std::wstring w = Utf8ToWide(s);
	const int n = WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string o(n > 0 ? n : 0, '\0'); if (n > 0) WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), o.data(), n, nullptr, nullptr);
	return o;
}
static bool ReadAll(const std::string &utf8, std::vector<uint8_t> &out)
{
	std::ifstream f(fs::path(Utf8ToWide(utf8)), std::ios::binary); if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static void ScanRefs(UserFile &u)
{
	std::error_code ec;
	if (!fs::exists(fs::path(Utf8ToWide(u.path)), ec)) { u.missing = true; return; }
	std::vector<uint8_t> bytes;
	if (!ReadAll(u.path, bytes)) { u.error = "cannot read"; return; }
	Ha6SpriteRefSink sink; FrameData fd;
	g_ha6SpriteRefSink = &sink; const bool ok = fd.load(Utf8ToAnsi(u.path).c_str()); g_ha6SpriteRefSink = nullptr;
	if (!ok || !sink.base) { u.error = "not an HA6 file"; return; }
	for (const auto &r : sink.refs) {
		if (!r.second || r.first + 4 > bytes.size()) continue;   // CG image layers are the CG manager's business
		int32_t id; memcpy(&id, &bytes[r.first], 4);
		if (id >= 0) u.refs.push_back({r.first, id});
	}
}

Users FindUsers(const std::string &patPath)
{
	Users u; u.patPath = patPath;
	const std::string patN = cgm::NormPath(patPath), dir = DirOf(patPath), dirN = cgm::NormPath(dir);
	std::vector<cgm::ProjectTxt> all;
	std::error_code ec;
	for (auto &e : fs::directory_iterator(fs::path(Utf8ToWide(dir)), ec)) {
		if (!e.is_regular_file(ec) || Lower(e.path().extension().string()) != ".txt") continue;
		cgm::ProjectTxt t; if (cgm::ParseProjectTxt(WideToUtf8(e.path().wstring()), t)) all.push_back(std::move(t));
	}
	std::sort(all.begin(), all.end(), [](const cgm::ProjectTxt &a, const cgm::ProjectTxt &b) { return cgm::NormPath(a.path) < cgm::NormPath(b.path); });
	std::set<std::string> otherPatFiles;
	for (const auto &t : all) if (!t.patPath.empty() && cgm::NormPath(t.patPath) != patN) for (auto &p : t.dataPaths) if (!p.empty()) otherPatFiles.insert(cgm::NormPath(p));
	for (const auto &t : all) {
		if (t.patPath.empty() || cgm::NormPath(t.patPath) != patN) continue;
		for (const std::string &p : t.dataPaths) {
			if (p.empty()) continue;
			const std::string n = cgm::NormPath(p);
			auto it = std::find_if(u.files.begin(), u.files.end(), [&](const UserFile &f) { return cgm::NormPath(f.path) == n; });
			if (it == u.files.end()) {
				UserFile f; f.path = p;
				f.shared = otherPatFiles.count(n) || n.substr(0, n.find_last_of('/')) != dirN || n.find("basedata") != std::string::npos;
				u.files.push_back(f); it = u.files.end() - 1;
			}
			it->projects.push_back(t.path);
		}
	}
	for (auto &f : u.files) ScanRefs(f);
	return u;
}

Plan MakePlan(const Users &users, const std::vector<int> &diskRemap)
{
	Plan plan;
	for (const UserFile &f : users.files) {
		Plan::File pf; pf.path = f.path; pf.shared = f.shared; pf.refs = (int)f.refs.size();
		if (!f.error.empty()) plan.refusals.push_back(f.path + ": " + f.error + ", its references cannot be checked");
		std::set<int> gone, sharedMoved;
		for (const auto &r : f.refs) {
			const int id = r.second;
			const int now = id < (int)diskRemap.size() ? diskRemap[id] : id;
			if (now < 0) { gone.insert(id); continue; }
			if (now != id) { pf.changed++; if (f.shared) sharedMoved.insert(id); }
		}
		auto list = [](const std::set<int> &s) { std::string o; int k = 0; for (int v : s) { if (k++ == 8) { o += ", ..."; break; } o += (o.empty() ? "" : ", ") + std::to_string(v); } return o; };
		if (!gone.empty()) plan.refusals.push_back(f.path + ": frames draw part set(s) " + list(gone) + ", which this deletes");
		if (!sharedMoved.empty()) plan.refusals.push_back(f.path + " is shared with other .pat files (never rewritten) and draws part set(s) " + list(sharedMoved) + ", which this renumbers");
		plan.changed += f.shared ? 0 : pf.changed;
		plan.files.push_back(pf);
	}
	return plan;
}

int MaxRef(const Users &users)
{
	int m = -1;
	for (auto &f : users.files) for (auto &r : f.refs) m = std::max(m, r.second);
	return m;
}

WriteResult WriteUsers(const Users &users, const std::vector<int> &diskRemap, void (*backup)(const std::string &, void *), void *ctx)
{
	WriteResult res;
	for (const UserFile &f : users.files) {
		if (f.shared || f.missing || !f.error.empty()) continue;
		std::vector<uint8_t> bytes;
		if (!ReadAll(f.path, bytes)) { res.errors.push_back("cannot read " + f.path); continue; }
		int n = 0;
		for (const auto &r : f.refs) {
			if (r.first + 4 > bytes.size()) continue;
			int32_t id; memcpy(&id, &bytes[r.first], 4);
			if (id != r.second) { res.errors.push_back(f.path + " changed on disk since it was scanned"); n = -1; break; }
			const int now = id < (int)diskRemap.size() ? diskRemap[id] : id;
			if (now < 0 || now == id) continue;
			const int32_t v = now; memcpy(&bytes[r.first], &v, 4); n++;
		}
		if (n <= 0) continue;
		if (backup) backup(f.path, ctx);
		if (!WriteFileAtomic(Utf8ToAnsi(f.path).c_str(), bytes.data(), bytes.size())) { res.errors.push_back("cannot write " + f.path); continue; }
		res.written.push_back(f.path); res.refs += n;
	}
	return res;
}

bool IsIdentity(const std::vector<int> &r) { for (size_t i = 0; i < r.size(); i++) if (r[i] != (int)i) return false; return true; }

std::vector<int> PendingDiskRemap(const Parts &parts, int extent)
{
	const std::vector<int> cur = parts.CurrentIndexOfOrig();
	int n = std::max(extent, (int)parts.partSets.size());
	for (int d : parts.diskIdOfOrig) n = std::max(n, d + 1);
	std::vector<int> remap((size_t)n);
	for (int i = 0; i < n; i++) remap[i] = i;   // ids no slot came from (past the end on disk): unchanged
	for (size_t o = 0; o < parts.diskIdOfOrig.size(); o++) {
		const int d = parts.diskIdOfOrig[o];
		if (d >= 0 && d < n) remap[d] = o < cur.size() ? cur[o] : -1;
	}
	return remap;
}

bool ApplyOp(Parts &parts, const Op &op, int extent, std::string *err)
{
	auto &sets = parts.partSets;
	// pad to the extent with empty slots that stand for the ids on disk past the end (no record)
	if ((int)sets.size() < extent) {
		std::set<int> usedDisk(parts.diskIdOfOrig.begin(), parts.diskIdOfOrig.end());
		while ((int)sets.size() < extent) {
			PartSet<> ps; ps.partId = (int)sets.size();
			if (!usedDisk.count(ps.partId)) { ps.origId = (int)parts.diskIdOfOrig.size(); parts.diskIdOfOrig.push_back(ps.partId); }
			sets.push_back(std::move(ps));
		}
	}
	std::vector<bool> empty(sets.size());
	for (size_t i = 0; i < sets.size(); i++) empty[i] = sets[i].groups.empty() && sets[i].name.empty();
	std::vector<int> remap; int nc = 0;
	if (!BuildRemap((int)sets.size(), op, empty, remap, &nc, err)) return false;
	if (op.kind == Op::Clear)
		for (int i = op.a; i <= op.b; i++) { PartSet<> e; e.partId = i; sets[i] = std::move(e); }   // the id stays, as an empty gap
	else
		ApplyRemap(sets, remap, nc);
	return true;
}

} // namespace patrn
