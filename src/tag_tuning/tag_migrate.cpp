// [authoring] tag_tuning.ini -> sidecars — see tag_migrate.h.
#include "tag_migrate.h"
#include "tag_sidecar.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <set>

namespace tagtune {

namespace fs = std::filesystem;

namespace {

bool IsBlank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }

// One physical line with its terminator, plus its section header (if it is one), by TagIni's rules.
struct RawLine { std::string bytes; bool header = false; std::string section; };

std::vector<RawLine> SplitLines(const std::string& t)
{
	std::vector<RawLine> out;
	size_t p = 0;
	while (p < t.size()) {
		const size_t nl = t.find('\n', p);
		const size_t next = nl == std::string::npos ? t.size() : nl + 1;
		RawLine l;
		l.bytes = t.substr(p, next - p);
		size_t b = 0, e = l.bytes.size();
		for (size_t k = 0; k < e; ++k) {
			const char c = l.bytes[k];
			if ((c == ';' || c == '#') && (k == 0 || l.bytes[k - 1] == ' ' || l.bytes[k - 1] == '\t')) { e = k; break; }
		}
		while (b < e && IsBlank(l.bytes[b])) ++b;
		while (e > b && IsBlank(l.bytes[e - 1])) --e;
		if (b < e && l.bytes[b] == '[') {
			size_t close = l.bytes.find(']', b);
			size_t nb = b + 1, ne = (close == std::string::npos || close >= e) ? e : close;
			while (nb < ne && IsBlank(l.bytes[nb])) ++nb;
			while (ne > nb && IsBlank(l.bytes[ne - 1])) --ne;
			l.header = true;
			l.section = l.bytes.substr(nb, ne - nb);
		}
		out.push_back(std::move(l));
		p = next;
	}
	return out;
}

// "[char.<name>]" -> name (trimmed), else "".
bool CharSectionName(const std::string& section, std::string& name)
{
	if (section.size() <= 5 || !ieq(section.substr(0, 5), "char.")) return false;
	std::string n = section.substr(5);
	size_t a = 0, z = n.size();
	while (a < z && IsBlank(n[a])) ++a;
	while (z > a && IsBlank(n[z - 1])) --z;
	name = n.substr(a, z - a);
	return true;
}

std::multiset<std::string> NormalizedSet(const std::vector<Warning>& w, bool dropTolerance)
{
	std::multiset<std::string> s;
	for (const Warning& x : w) {
		if (dropTolerance && x.msg.find("per-character section in global.ini") != std::string::npos) continue;
		s.insert(NormalizeWarning(x.msg));
	}
	return s;
}

std::string ValuesDiff(const Values& a, const Values& b)
{
	for (size_t i = 0; i < kLeverCount; ++i)
		if (a[i] != b[i])
			return std::string(kLevers[i].key) + " " + FormatLeverValue(kLevers[i], a[i]) + " (legacy) vs " +
			       FormatLeverValue(kLevers[i], b[i]) + " (sidecars)";
	return {};
}

} // namespace

std::string NormalizeWarning(const std::string& msgIn)
{
	std::string m = msgIn;
	for (bool again = true; again;) {
		again = false;
		for (const char* pre : { "local ", "shipped " })
			if (m.compare(0, std::strlen(pre), pre) == 0) {
				const size_t c = m.find(": ");
				if (c != std::string::npos) { m = m.substr(c + 2); again = true; }
			}
		if (!m.empty() && m[0] == '[') {
			const size_t c = m.find("]: ");
			if (c != std::string::npos) { m = m.substr(c + 3); again = true; }
		}
		if (m.compare(0, 9, "built-in:") == 0) { m = m.substr(9); again = true; }
	}
	return m;
}

MigrationPlan PlanMigration(const std::string& legacy, const std::string& date)
{
	MigrationPlan p;
	const std::string nl = legacy.find("\r\n") != std::string::npos ? "\r\n" : "\n";
	const std::vector<RawLine> lines = SplitLines(legacy);
	std::string global = "; migrated from tag_tuning.ini by Hantei-chan " + date + "; per-character sections moved to chars\\" + nl;
	std::map<std::string, std::string> body;   // lower name -> lines
	std::vector<std::string> order;
	std::string cur;          // current [char.*] target ("" = global)
	bool inChar = false;
	for (const RawLine& l : lines) {
		if (l.header) {
			std::string name;
			inChar = false;
			cur.clear();
			if (CharSectionName(l.section, name)) {
				const std::string low = Lower(name);
				if (ValidCharFileName(low)) {
					inChar = true;
					cur = low;
					if (!body.count(low)) { body[low] = ""; order.push_back(low); }
					continue;   // the header becomes the file's own [char]
				}
				p.keptLegacy.push_back(name);
			}
		}
		if (inChar) body[cur] += l.bytes;
		else global += l.bytes;
	}
	// a legacy file without a final newline: keep the global text's last line as it was
	p.globalText = global;
	for (const std::string& n : order) {
		std::string t = "; chars\\" + n + ".ini - migrated from tag_tuning.ini [char." + n + "] (every moon)" + nl + "[char]" + nl;
		t += body[n];
		p.chars[n] = t;
	}

	// ---- verify: legacy rules vs the local tree alone ----
	TagIni leg;
	leg.LoadText(legacy);
	const Resolved lr = Resolve(leg);
	TagIni g;
	g.LoadText(p.globalText);
	std::map<std::string, std::unique_ptr<TagIni>> ch;
	SidecarSet s;
	s.global[(int)Layer::Local] = &g;
	for (const auto& kv : p.chars) {
		auto t = std::make_unique<TagIni>();
		t->SetCharFileMode(true);
		t->LoadText(kv.second);
		s.shared[(int)Layer::Local][kv.first] = t.get();
		ch[kv.first] = std::move(t);
	}
	const GlobalResolution gr = ResolveGlobal(s);
	auto fail = [&p](const std::string& why) { p.firstDifference = why; p.error = "verification failed: " + why; p.ok = false; return p; };
	if (lr.style != gr.style) return fail("style '" + lr.style + "' (legacy) vs '" + gr.style + "' (sidecars)");
	if (std::string d = ValuesDiff(lr.global, gr.values); !d.empty()) return fail("global " + d);
	std::set<std::string> names;
	for (const std::string& n : leg.SectionNames(SecKind::Char)) names.insert(Lower(n));
	for (const std::string& n : s.CharFiles()) names.insert(n);
	for (const std::string& n : names)
		for (int m = 0; m < kMoonCount; ++m) {
			const Values a = ForChar(leg, lr.global, n);
			const SlotResolution b = ResolveSlot(s, gr, n, m);
			if (std::string d = ValuesDiff(a, b.values); !d.empty()) return fail(n + " (" + MoonName(m) + "): " + d);
			++p.comparedSlots;
		}
	const std::multiset<std::string> wl = NormalizedSet(AllWarnings(leg), false);
	const std::multiset<std::string> ws = NormalizedSet(AllSidecarWarnings(s), true);
	if (wl != ws) {
		std::vector<std::string> onlyL, onlyS;
		std::set_difference(wl.begin(), wl.end(), ws.begin(), ws.end(), std::back_inserter(onlyL));
		std::set_difference(ws.begin(), ws.end(), wl.begin(), wl.end(), std::back_inserter(onlyS));
		return fail("warnings differ: " + (onlyL.empty() ? std::string() : "legacy only '" + onlyL.front() + "' ") +
		            (onlyS.empty() ? std::string() : "sidecars only '" + onlyS.front() + "'"));
	}
	p.verified = true;
	p.ok = true;
	return p;
}

MigrationResult MigrateGameDir(const std::string& gameDir, bool dryRun, const std::string& date)
{
	MigrationResult r;
	const std::string root = gameDir + "\\povertycaster\\tag";
	const std::string legacyPath = gameDir + "\\tag_tuning.ini";
	std::string legacy;
	bool exists = false;
	if (!ReadWholeFile(legacyPath, legacy, &exists) || !exists) {
		r.plan.error = "no " + legacyPath + " to migrate";
		r.log = r.plan.error + "\n";
		return r;
	}
	SidecarWorkspace ws;
	ws.Open(root);
	for (const SidecarDoc* d : ws.Docs())
		if (d->id.layer == Layer::Local && d->onDisk && d->id.kind != DocKind::Hud) {
			r.plan.error = "refused: " + d->path + " already exists (merging into existing sidecars is not supported)";
			r.log = r.plan.error + "\n";
			return r;
		}
	r.plan = PlanMigration(legacy, date);
	r.log += "legacy file: " + legacyPath + " (" + std::to_string(legacy.size()) + " bytes)\n";
	r.log += "planned: local\\global.ini (" + std::to_string(r.plan.globalText.size()) + " bytes)\n";
	for (const auto& kv : r.plan.chars)
		r.log += "planned: local\\chars\\" + kv.first + ".ini (" + std::to_string(kv.second.size()) + " bytes)\n";
	for (const std::string& k : r.plan.keptLegacy) r.log += "kept in global.ini (name breaks the file rule): [char." + k + "]\n";
	if (!r.plan.ok) { r.log += r.plan.error + "\n"; return r; }
	r.log += "verified: identical resolution (global, style, " + std::to_string(r.plan.comparedSlots) +
	         " character x moon slots, warnings)\n";
	// what the shipped defaults will add on top (information: the legacy file never saw them)
	{
		SidecarSet shipped = ws.Set();
		for (int m = 0; m < kMoonCount; ++m) shipped.moon[(int)Layer::Local][m].clear();
		shipped.global[(int)Layer::Local] = nullptr;
		shipped.shared[(int)Layer::Local].clear();
		if (!shipped.Empty()) {
			const GlobalResolution sg = ResolveGlobal(shipped);
			for (size_t i = 0; i < kLeverCount; ++i)
				if (sg.from[i].src != Src::Default) r.plan.shippedChanges.push_back(kLevers[i].key);
			for (const std::string& k : r.plan.shippedChanges) r.log += "note: the shipped defaults also set " + k + "\n";
			if (!shipped.CharFiles().empty()) r.log += "note: the shipped defaults have character files; they apply under the local ones\n";
		}
	}
	if (dryRun) { r.log += "dry run: nothing written\n"; return r; }
	std::string err;
	for (const auto& kv : r.plan.chars) {
		const std::string path = root + "\\local\\chars\\" + kv.first + ".ini";
		if (!WriteFileAtomic(path, kv.second, false, &err)) { r.log += "FAILED: " + err + "\n"; return r; }
		r.files.push_back(path);
	}
	const std::string gpath = root + "\\local\\global.ini";
	if (!WriteFileAtomic(gpath, r.plan.globalText, false, &err)) { r.log += "FAILED: " + err + "\n"; return r; }
	r.files.push_back(gpath);
	const std::wstring from = fs::u8path(legacyPath).wstring(), to = fs::u8path(legacyPath + ".migrated").wstring();
	if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) r.log += "warning: could not rename tag_tuning.ini\n";
	else r.log += "renamed tag_tuning.ini -> tag_tuning.ini.migrated\n";
	r.wrote = true;
	for (const std::string& f : r.files) r.log += "wrote " + f + "\n";
	return r;
}

} // namespace tagtune
