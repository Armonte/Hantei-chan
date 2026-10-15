// pcmod.json schema 1: see pcmod_manifest.h. Section numbers refer to docs/modmanager/pcmod-manifest_schema1.md.
#include "pcmod_manifest.h"

#include "../../third_party/json/json.hpp"

#include <algorithm>
#include <set>

namespace pcmod {

const char* ClassName(PackageClass c) { return c == PackageClass::Sim ? "sim" : "cosmetic"; }

bool ParseClass(const std::string& s, PackageClass& out)
{
	if (s == "sim") { out = PackageClass::Sim; return true; }
	if (s == "cosmetic") { out = PackageClass::Cosmetic; return true; }
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// §2 field rules
// ---------------------------------------------------------------------------------------------------------------------
static bool IsIdChar(char c, bool first)
{
	if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return true;
	return !first && (c == '.' || c == '_' || c == '-');
}

bool ValidId(const std::string& id)
{
	if (id.empty() || id.size() > 64) return false;
	for (size_t i = 0; i < id.size(); ++i) if (!IsIdChar(id[i], i == 0)) return false;
	return true;
}

// Splits "A.B.C" into three numbers. Leading zeros are refused so that one version has exactly one spelling.
static bool SplitVersion(const std::string& v, unsigned out[3])
{
	int part = 0;
	size_t i = 0;
	for (;;) {
		const size_t start = i;
		unsigned long n = 0;
		while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
			n = n * 10 + unsigned(v[i] - '0');
			if (n > 65535) return false;
			++i;
		}
		if (i == start) return false;
		if (i - start > 1 && v[start] == '0') return false;
		if (part > 2) return false;
		out[part++] = (unsigned)n;
		if (i == v.size()) break;
		if (v[i] != '.') return false;
		++i;
	}
	return part == 3;
}

bool ValidVersion(const std::string& v) { unsigned p[3]; return SplitVersion(v, p); }

int CompareVersion(const std::string& a, const std::string& b)
{
	unsigned x[3] = {}, y[3] = {};
	SplitVersion(a, x);
	SplitVersion(b, y);
	for (int k = 0; k < 3; ++k) if (x[k] != y[k]) return x[k] < y[k] ? -1 : 1;
	return 0;
}

bool IsLowerHex64(const std::string& s)
{
	if (s.size() != 64) return false;
	return std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// ---------------------------------------------------------------------------------------------------------------------
// §3 paths
// ---------------------------------------------------------------------------------------------------------------------
static bool IsReservedDosName(std::string seg)
{
	// The device check applies to the part before the first '.', case-insensitively ("con.txt" is CON on Windows).
	seg = seg.substr(0, seg.find('.'));
	for (char& c : seg) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	static const char* const kNames[] = {"con", "prn", "aux", "nul"};
	for (const char* n : kNames) if (seg == n) return true;
	if (seg.size() == 4 && (seg.compare(0, 3, "com") == 0 || seg.compare(0, 3, "lpt") == 0) && seg[3] >= '1' && seg[3] <= '9') return true;
	return false;
}

bool CanonicalPath(const std::string& raw, std::string& out, std::string& why)
{
	out.clear();
	if (raw.empty()) { why = "empty path"; return false; }
	if (raw.size() > kMaxPath) { why = "longer than 240 bytes"; return false; }
	std::string p;
	p.reserve(raw.size());
	for (char c : raw) {
		const unsigned char u = (unsigned char)c;
		if (u < 0x20 || u == 0x7F) { why = "control character"; return false; }
		if (c == ':') { why = "':' (drive or stream)"; return false; }
		p += c == '/' ? '\\' : c;
	}
	if (p[0] == '\\') { why = "leading separator (absolute path)"; return false; }
	size_t start = 0;
	for (;;) {
		const size_t end = std::min(p.find('\\', start), p.size());
		const std::string seg = p.substr(start, end - start);
		if (seg.empty()) { why = "empty segment"; return false; }
		if (seg == "." || seg == "..") { why = "'.' or '..' segment"; return false; }
		if (seg.back() == ' ' || seg.back() == '.') { why = "segment ends in a space or dot"; return false; }
		if (IsReservedDosName(seg)) { why = "reserved DOS device name"; return false; }
		if (end == p.size()) break;
		start = end + 1;
	}
	out = p;
	return true;
}

bool NormalisePath(const std::string& raw, std::string& out, std::string& why)
{
	if (!CanonicalPath(raw, out, why)) return false;
	for (char& c : out) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	return true;
}

std::string FileEntry::key() const
{
	std::string k, why;
	NormalisePath(path, k, why);
	return k;
}

static bool StartsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

static bool EndsWith(const std::string& s, const char* suffix)
{
	const size_t n = std::char_traits<char>::length(suffix);
	return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

PathClass ClassifyPath(const std::string& normPath)
{
	// MBAACC packs are one top-level folder each (0000.p bg ... 0007.p story); a loose folder of the same name overrides it.
	// data\ is the simulation, except palettes and charaselect art (rollback-inert, mbaacc_synctest 2025-09-25).
	if (StartsWith(normPath, "data\\"))
		return (EndsWith(normPath, ".pal") || StartsWith(normPath, "data\\_csel\\")) ? PathClass::Cosmetic : PathClass::Sim;
	// story\ (arcade routes) and the roster table: offline modes and replays must verify them.
	if (StartsWith(normPath, "story\\") || normPath == "charaselect.txt") return PathClass::Sim;
	// bg\ is off the battle RNG (stage randoms use their own stream); dat\ is title/VS screen presentation only.
	// packdataversion\ is NOT here: it gates the update pack (InitializePackSystem), so it stays refused.
	static const char* const kCosmeticDirs[] = {"bg\\", "bgm\\", "dat\\", "grp\\", "se\\", "shader\\", "movie\\", "palettes\\"};
	for (const char* d : kCosmeticDirs) if (StartsWith(normPath, d)) return PathClass::Cosmetic;
	if (normPath == "hud_theme.json" || normPath == "font.fnt" || normPath == "_sysfont.ini") return PathClass::Cosmetic;
	return PathClass::Refused;
}

const char* PathClassName(PathClass c)
{
	return c == PathClass::Sim ? "sim" : c == PathClass::Cosmetic ? "cosmetic" : "refused";
}

// ---------------------------------------------------------------------------------------------------------------------
// Validation of a model
// ---------------------------------------------------------------------------------------------------------------------
std::string Validate(const Manifest& m)
{
	if (m.schema != 1) return "schema: only schema 1 is written or understood";
	if (!ValidId(m.id)) return "id: must match [a-z0-9][a-z0-9._-]{0,63}";
	if (!ValidVersion(m.version)) return "version: must be MAJOR.MINOR.PATCH (digits, 0..65535, no leading zeros)";
	if (m.name.size() > kMaxName) return "name: longer than 128 bytes";
	if (m.author.size() > kMaxAuthor) return "author: longer than 128 bytes";
	if (m.description.size() > kMaxDescription) return "description: longer than 2048 bytes";
	if (!ValidId(m.game)) return "game: not a catalogue game id";
	if (m.priority < -1000 || m.priority > 1000) return "priority: must be -1000..1000";
	if (m.exe.size() > 16) return "exe: at most 16 hashes";
	for (const std::string& e : m.exe) if (!IsLowerHex64(e)) return "exe: '" + e + "' is not 64 lowercase hex";
	if (m.cls == PackageClass::Sim && m.exe.empty()) return "exe: a sim package must name the MBAA.exe hash(es) it is valid on";
	if (m.requires_.size() > 64 || m.conflicts.size() > 64 || m.overrides.size() > 64) return "requires/conflicts/overrides: at most 64 each";
	for (const Requirement& r : m.requires_) {
		if (!ValidId(r.id)) return "requires: bad id '" + r.id + "'";
		if (!ValidVersion(r.version)) return "requires: bad version '" + r.version + "' for " + r.id;
		if (r.id == m.id) return "requires: a package cannot require itself";
	}
	for (const std::string& c : m.conflicts) if (!ValidId(c)) return "conflicts: bad id '" + c + "'";
	for (const std::string& c : m.overrides) if (!ValidId(c)) return "overrides: bad id '" + c + "'";
	if (m.files.empty() || m.files.size() > kMaxFiles) return "files: 1..4096 entries";
	std::set<std::string> seen;
	bool anySim = false;
	for (size_t i = 0; i < m.files.size(); ++i) {
		const FileEntry& f = m.files[i];
		const std::string at = "files[" + std::to_string(i) + "] '" + f.path + "'";
		std::string norm, why;
		if (!NormalisePath(f.path, norm, why)) return at + ": " + why;
		const PathClass pc = ClassifyPath(norm);
		if (pc == PathClass::Refused) return at + ": not under a root a package may ship (unclassified game path)";
		if (pc == PathClass::Sim && m.cls == PackageClass::Cosmetic) return at + ": a sim path; a cosmetic package may not ship it (make it \"sim\")";
		if (pc == PathClass::Sim) anySim = true;
		if (!IsLowerHex64(f.sha256)) return at + ": sha256 must be 64 lowercase hex";
		if (f.size > kMaxFileSize) return at + ": larger than 64 MiB";
		if (!seen.insert(norm).second) return at + ": listed twice (paths compare case-insensitively)";
	}
	if (m.cls == PackageClass::Sim && !anySim) return "class: \"sim\" but no file is a sim path (make it \"cosmetic\")";
	return {};
}

// ---------------------------------------------------------------------------------------------------------------------
// Strict reader
// ---------------------------------------------------------------------------------------------------------------------
using nlohmann::json;

namespace {
struct Refuse { std::string why; };

const std::string& NeedString(const json& v, const char* key)
{
	if (!v.is_string()) throw Refuse{std::string(key) + ": not a string"};
	return v.get_ref<const std::string&>();
}
long long NeedInt(const json& v, const char* key)
{
	if (!v.is_number_integer()) throw Refuse{std::string(key) + ": not an integer"};
	if (v.is_number_unsigned()) {
		const uint64_t u = v.get<uint64_t>();
		if (u > (uint64_t)INT64_MAX) throw Refuse{std::string(key) + ": out of range"};
		return (long long)u;
	}
	return v.get<long long>();
}
std::vector<std::string> NeedIdList(const json& v, const char* key)
{
	if (!v.is_array()) throw Refuse{std::string(key) + ": not an array"};
	std::vector<std::string> out;
	for (const json& e : v) {
		const std::string& s = NeedString(e, key);
		if (!ValidId(s)) throw Refuse{std::string(key) + ": bad id '" + s + "'"};
		out.push_back(s);
	}
	return out;
}
}

bool ParseManifest(const std::string& text, Manifest& out, std::string& error)
{
	out = Manifest{};
	out.game.clear();
	// Duplicate keys (at any depth) refuse the package: a JSON library would silently keep one of them.
	std::vector<std::set<std::string>> keyStack;
	bool duplicate = false;
	std::string dupKey;
	json::parser_callback_t cb = [&](int, json::parse_event_t ev, json& parsed) {
		if (ev == json::parse_event_t::object_start) keyStack.emplace_back();
		else if (ev == json::parse_event_t::object_end) { if (!keyStack.empty()) keyStack.pop_back(); }
		else if (ev == json::parse_event_t::key && !keyStack.empty()) {
			const std::string k = parsed.get<std::string>();
			if (!keyStack.back().insert(k).second && !duplicate) { duplicate = true; dupKey = k; }
		}
		return true;
	};
	const json root = json::parse(text, cb, /*allow_exceptions*/ false);
	if (root.is_discarded()) { error = "not valid JSON"; return false; }
	if (duplicate) { error = "duplicate key '" + dupKey + "'"; return false; }
	if (!root.is_object()) { error = "top level is not an object"; return false; }

	try {
		bool haveSchema = false, haveId = false, haveVersion = false, haveGame = false, haveClass = false, haveFiles = false;
		for (auto it = root.begin(); it != root.end(); ++it) {
			const std::string& k = it.key();
			const json& v = it.value();
			if (k == "inject" || k == "palettes" || k == "exe_layers") throw Refuse{"'" + k + "' is reserved for schema 2"};
			if (k == "schema") {
				if (NeedInt(v, "schema") != 1) throw Refuse{"schema: only schema 1 is understood"};
				haveSchema = true;
			} else if (k == "id") { out.id = NeedString(v, "id"); haveId = true; }
			else if (k == "version") { out.version = NeedString(v, "version"); haveVersion = true; }
			else if (k == "name") out.name = NeedString(v, "name");
			else if (k == "author") out.author = NeedString(v, "author");
			else if (k == "description") out.description = NeedString(v, "description");
			else if (k == "game") { out.game = NeedString(v, "game"); haveGame = true; }
			else if (k == "class") {
				if (!ParseClass(NeedString(v, "class"), out.cls)) throw Refuse{"class: must be \"sim\" or \"cosmetic\""};
				haveClass = true;
			} else if (k == "priority") {
				const long long p = NeedInt(v, "priority");
				if (p < -1000 || p > 1000) throw Refuse{"priority: must be -1000..1000"};
				out.priority = (int)p;
			} else if (k == "exe") {
				if (!v.is_array()) throw Refuse{"exe: not an array"};
				for (const json& e : v) out.exe.push_back(NeedString(e, "exe"));
			} else if (k == "requires") {
				if (!v.is_array()) throw Refuse{"requires: not an array"};
				for (const json& e : v) {
					if (!e.is_object() || e.size() != 2 || !e.contains("id") || !e.contains("version"))
						throw Refuse{"requires: entries are exactly {id, version}"};
					out.requires_.push_back({NeedString(e["id"], "requires.id"), NeedString(e["version"], "requires.version")});
				}
			} else if (k == "conflicts") out.conflicts = NeedIdList(v, "conflicts");
			else if (k == "overrides") out.overrides = NeedIdList(v, "overrides");
			else if (k == "files") {
				if (!v.is_array()) throw Refuse{"files: not an array"};
				for (const json& e : v) {
					if (!e.is_object() || e.size() != 3 || !e.contains("path") || !e.contains("sha256") || !e.contains("size"))
						throw Refuse{"files: entries are exactly {path, sha256, size}"};
					FileEntry f;
					f.path = NeedString(e["path"], "files.path");
					f.sha256 = NeedString(e["sha256"], "files.sha256");
					const long long sz = NeedInt(e["size"], "files.size");
					if (sz < 0 || (uint64_t)sz > kMaxFileSize) throw Refuse{"files.size: 0..64 MiB"};
					f.size = (uint64_t)sz;
					// Keep the author's spelling but with '\' separators (the digest lowercases it).
					std::string canon, why;
					if (!CanonicalPath(f.path, canon, why)) throw Refuse{"files: '" + f.path + "': " + why};
					f.path = canon;
					out.files.push_back(f);
				}
				haveFiles = true;
			} else throw Refuse{"unknown key '" + k + "'"};
		}
		if (!haveSchema) throw Refuse{"schema: missing"};
		if (!haveId) throw Refuse{"id: missing"};
		if (!haveVersion) throw Refuse{"version: missing"};
		if (!haveGame) throw Refuse{"game: missing"};
		if (!haveClass) throw Refuse{"class: missing"};
		if (!haveFiles) throw Refuse{"files: missing"};
	} catch (const Refuse& r) {
		error = r.why;
		return false;
	}
	error = Validate(out);
	return error.empty();
}

// ---------------------------------------------------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------------------------------------------------
static std::string Quote(const std::string& s)
{
	std::string o = "\"";
	for (unsigned char c : s) {
		switch (c) {
		case '"': o += "\\\""; break;
		case '\\': o += "\\\\"; break;
		case '\n': o += "\\n"; break;
		case '\r': o += "\\r"; break;
		case '\t': o += "\\t"; break;
		default:
			if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
			else o += (char)c;   // UTF-8 passes through
		}
	}
	return o + "\"";
}

std::string WriteManifest(const Manifest& m)
{
	std::string o = "{\n";
	o += "  \"schema\": 1,\n";
	o += "  \"id\": " + Quote(m.id) + ",\n";
	o += "  \"version\": " + Quote(m.version) + ",\n";
	o += "  \"name\": " + Quote(m.name) + ",\n";
	o += "  \"author\": " + Quote(m.author) + ",\n";
	o += "  \"description\": " + Quote(m.description) + ",\n";
	o += "  \"game\": " + Quote(m.game) + ",\n";
	o += "  \"class\": " + Quote(ClassName(m.cls)) + ",\n";
	o += "  \"priority\": " + std::to_string(m.priority) + ",\n";
	auto list = [](const std::vector<std::string>& v) {
		std::string s = "[";
		for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + Quote(v[i]);
		return s + "]";
	};
	o += "  \"exe\": " + list(m.exe) + ",\n";
	o += "  \"requires\": [";
	for (size_t i = 0; i < m.requires_.size(); ++i)
		o += std::string(i ? ", " : "") + "{ \"id\": " + Quote(m.requires_[i].id) + ", \"version\": " + Quote(m.requires_[i].version) + " }";
	o += "],\n";
	o += "  \"conflicts\": " + list(m.conflicts) + ",\n";
	o += "  \"overrides\": " + list(m.overrides) + ",\n";
	o += "  \"files\": [\n";
	// Files are written in digest order so two exports of the same content are byte-identical.
	std::vector<const FileEntry*> fs;
	for (const FileEntry& f : m.files) fs.push_back(&f);
	std::stable_sort(fs.begin(), fs.end(), [](const FileEntry* a, const FileEntry* b) { return a->key() < b->key(); });
	for (size_t i = 0; i < fs.size(); ++i) {
		std::string canon, why;
		if (!CanonicalPath(fs[i]->path, canon, why)) canon = fs[i]->path;
		o += "    { \"path\": " + Quote(canon) + ", \"sha256\": " + Quote(fs[i]->sha256) + ", \"size\": " + std::to_string(fs[i]->size) + " }";
		o += i + 1 < fs.size() ? ",\n" : "\n";
	}
	o += "  ]\n}\n";
	return o;
}

// ---------------------------------------------------------------------------------------------------------------------
// §5 digests
// ---------------------------------------------------------------------------------------------------------------------
static Digest DigestFiles(const std::string& head, const Manifest& m, bool simOnly)
{
	std::vector<std::pair<std::string, const FileEntry*>> fs;
	for (const FileEntry& f : m.files) {
		std::string k = f.key();
		if (simOnly && ClassifyPath(k) != PathClass::Sim) continue;
		fs.emplace_back(std::move(k), &f);
	}
	std::sort(fs.begin(), fs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });   // byte order
	Sha256 h;
	h.update(head.data(), head.size());
	for (const auto& [key, f] : fs) {
		std::string line = key;
		line += '\0';
		line += f->sha256;
		line += '\0';
		line += std::to_string(f->size);
		line += '\n';
		h.update(line.data(), line.size());
	}
	return h.finish();
}

Digest PackageDigest(const Manifest& m)
{
	return DigestFiles("pcmod-pkg-1\n" + m.id + "\n" + m.version + "\n" + ClassName(m.cls) + "\n", m, false);
}
std::string PackageDigestHex(const Manifest& m) { return ToHex(PackageDigest(m)); }

Digest PackageSimDigest(const Manifest& m)
{
	return DigestFiles("pcmod-sim-1\n" + m.id + "\n" + m.version + "\n", m, true);
}
std::string PackageSimDigestHex(const Manifest& m) { return ToHex(PackageSimDigest(m)); }

std::string SessionDigestHex(const std::string& exeSha256Hex, std::vector<SessionEntry> pkgs)
{
	std::sort(pkgs.begin(), pkgs.end(), [](const SessionEntry& a, const SessionEntry& b) {
		if (a.priority != b.priority) return a.priority > b.priority;
		return a.id < b.id;
	});
	Sha256 h;
	const std::string head = "pcmod-set-1\n" + exeSha256Hex + "\n";
	h.update(head.data(), head.size());
	for (const SessionEntry& e : pkgs) {
		std::string line = e.id;
		line += '\0';
		line += e.version;
		line += '\0';
		line += e.simDigestHex;
		line += '\n';
		h.update(line.data(), line.size());
	}
	return ToHex(h.finish());
}

} // namespace pcmod
