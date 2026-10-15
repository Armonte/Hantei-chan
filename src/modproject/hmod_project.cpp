// See hmod_project.h.
#include "hmod_project.h"
#include "hmod_vanilla.h"
#include "../../third_party/json/json.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;
using nlohmann::ordered_json;

namespace hmod {

const std::vector<CategoryInfo>& Categories()
{
	static const std::vector<CategoryInfo> c = {
		{"character.framedata", "sim", "HA6 frame data"},
		{"character.commands", "sim", "_c.txt command files"},
		{"character.descriptor", "sim", "character descriptors"},
		{"character.new", "sim", "new character (all files + charaselect entry)"},
		{"system.constants", "sim", "_type.txt, vector.txt"},
		{"sprites", "sim", ".cg sprite banks"},
		{"palettes", "", ".pal / .cpf palettes"},
		{"hud", "cosmetic", "HUD images"},
		{"ui.text", "cosmetic", "UI text, command lists, fonts"},
		{"audio.se", "cosmetic", "sound effects and voices"},
		{"music", "cosmetic", "BGM tracks and bgm.txt"},
		{"stage", "cosmetic", "stage data, name / preview images, BGM"},
		{"story", "sim", "story scripts and win quotes"},
		{"other", "", "anything else"},
	};
	return c;
}
const CategoryInfo* FindCategory(const std::string& id)
{
	for (const CategoryInfo& c : Categories()) if (id == c.id) return &c;
	return nullptr;
}

static const char* const kOriginKinds[] = {"vanilla-derived", "clone", "game-convert", "install-import", "authored"};
static const char* const kExportModes[] = {"whole-file", "ha6-patch", "ini-merge"};
template <size_t N> static bool In(const char* const (&arr)[N], const std::string& s) { return std::find(std::begin(arr), std::end(arr), s) != std::end(arr); }

bool ValidGame(const std::string& g) { return g == "mbaacc" || g == "uni2" || g == "mbtl"; }

const Asset* Project::FindAsset(const std::string& id) const { for (const Asset& a : assets) if (a.id == id) return &a; return nullptr; }
Asset* Project::FindAsset(const std::string& id) { for (Asset& a : assets) if (a.id == id) return &a; return nullptr; }
const Package* Project::FindPackage(const std::string& id) const { for (const Package& p : packages) if (p.id == id) return &p; return nullptr; }
std::string Project::AssetDir(const std::string& assetId) const { return (fs::u8path(dir) / "assets" / fs::u8path(assetId)).u8string(); }

bool GlobMatch(const std::string& patIn, const std::string& strIn)
{
	const std::string p = NormKey(patIn), s = NormKey(strIn);
	size_t pi = 0, si = 0, star = std::string::npos, mark = 0;
	while (si < s.size()) {
		if (pi < p.size() && (p[pi] == '?' || p[pi] == s[si])) { ++pi; ++si; }
		else if (pi < p.size() && p[pi] == '*') { star = pi++; mark = si; }
		else if (star != std::string::npos) { pi = star + 1; si = ++mark; }
		else return false;
	}
	while (pi < p.size() && p[pi] == '*') ++pi;
	return pi == p.size();
}

std::vector<std::string> ResolvePackageAssets(const Project& p, const Package& pkg)
{
	std::vector<std::string> out;
	for (const std::string& pat : pkg.assets) {
		for (const Asset& a : p.assets)
			if ((a.id == pat || GlobMatch(pat, a.id)) && std::find(out.begin(), out.end(), a.id) == out.end()) out.push_back(a.id);
	}
	return out;
}

std::string NowIso()
{
	const std::time_t t = std::time(nullptr);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	char b[32];
	std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
	return b;
}

std::string Validate(const Project& p)
{
	if (p.hmod != kHmodVersion) return "unsupported hmod version " + std::to_string(p.hmod);
	if (!pcmod::ValidId(p.id)) return "project id '" + p.id + "' must match [a-z0-9][a-z0-9._-]{0,63}";
	if (!ValidGame(p.game)) return "game must be mbaacc, uni2 or mbtl";
	std::set<std::string> ids;
	for (const Asset& a : p.assets) {
		if (!pcmod::ValidId(a.id)) return "asset id '" + a.id + "' must match [a-z0-9][a-z0-9._-]{0,63}";
		if (!ids.insert(a.id).second) return "duplicate asset id " + a.id;
		if (!a.category.empty() && !FindCategory(a.category)) return "asset " + a.id + ": unknown category " + a.category;
		if (!In(kOriginKinds, a.origin.kind)) return "asset " + a.id + ": unknown origin kind " + a.origin.kind;
		if (!In(kExportModes, a.exportMode)) return "asset " + a.id + ": unknown export mode " + a.exportMode;
		if (!a.cls.empty() && a.cls != "sim" && a.cls != "cosmetic") return "asset " + a.id + ": class must be sim or cosmetic";
		for (const std::string& t : a.targets) if (t.empty() || t.find("..") != std::string::npos || t.find(':') != std::string::npos) return "asset " + a.id + ": bad target '" + t + "'";
	}
	for (const Asset& a : p.assets)
		for (const std::string& d : a.depends) if (!ids.count(d)) return "asset " + a.id + " depends on unknown asset " + d;
	std::set<std::string> pk;
	for (const Package& k : p.packages) {
		if (!pcmod::ValidId(k.id)) return "package id '" + k.id + "' must match [a-z0-9][a-z0-9._-]{0,63}";
		if (!pk.insert(k.id).second) return "duplicate package id " + k.id;
		if (!pcmod::ValidVersion(k.version)) return "package " + k.id + ": bad version " + k.version;
		if (!k.cls.empty() && k.cls != "sim" && k.cls != "cosmetic") return "package " + k.id + ": class must be sim or cosmetic";
		if (ResolvePackageAssets(p, k).empty()) return "package " + k.id + " lists no existing asset";
		for (const pcmod::Requirement& r : k.requires_) if (!pcmod::ValidId(r.id) || !pcmod::ValidVersion(r.version)) return "package " + k.id + ": bad requires " + r.id;
	}
	return {};
}

// ---------------------------------------------------------------- JSON
static std::string Str(const json& j, const char* k, const std::string& d = {}) { return j.contains(k) && j[k].is_string() ? j[k].get<std::string>() : d; }
static std::vector<std::string> StrList(const json& j, const char* k)
{
	std::vector<std::string> v;
	if (j.contains(k) && j[k].is_array()) for (const json& e : j[k]) if (e.is_string()) v.push_back(e.get<std::string>());
	return v;
}

bool Parse(const std::string& text, Project& out, std::string& err)
{
	json j = json::parse(text, nullptr, false);
	if (j.is_discarded() || !j.is_object()) { err = "hmod.json is not valid JSON"; return false; }
	Project p;
	p.hmod = j.value("hmod", 0);
	p.id = Str(j, "id"); p.name = Str(j, "name"); p.author = Str(j, "author"); p.game = Str(j, "game", "mbaacc");
	if (j.contains("base") && j["base"].is_object()) { p.base.exeSha256 = Str(j["base"], "exe_sha256"); p.base.packset = Str(j["base"], "packset"); }
	if (j.contains("sources") && j["sources"].is_array())
		for (const json& s : j["sources"]) {
			SourceRef r;
			r.id = Str(s, "id"); r.kind = Str(s, "kind"); r.path = Str(s, "path"); r.tool = Str(s, "tool"); r.toolVersion = Str(s, "tool_version");
			r.note = Str(s, "note"); r.credit = Str(s, "credit"); r.imported = Str(s, "imported");
			p.sources.push_back(r);
		}
	if (j.contains("assets") && j["assets"].is_array())
		for (const json& a : j["assets"]) {
			Asset x;
			x.id = Str(a, "id"); x.category = Str(a, "category"); x.cls = Str(a, "class"); x.targets = StrList(a, "targets");
			x.exportMode = Str(a, "export", "whole-file"); x.depends = StrList(a, "depends");
			if (a.contains("origin") && a["origin"].is_object()) {
				const json& o = a["origin"];
				x.origin.kind = Str(o, "kind", "authored"); x.origin.of = Str(o, "of"); x.origin.files = Str(o, "files"); x.origin.note = Str(o, "note"); x.origin.source = Str(o, "source");
				if (o.contains("base") && o["base"].is_array())
					for (const json& b : o["base"]) x.origin.base.push_back({Str(b, "path"), Str(b, "pack"), Str(b, "sha256")});
			}
			if (a.contains("charaselect") && a["charaselect"].is_object()) {
				const json& c = a["charaselect"];
				x.chara.charaNo = c.value("chara_no", -1); x.chara.cell = c.value("cell", -1);
				x.chara.name = Str(c, "name"); x.chara.file1 = Str(c, "file1"); x.chara.file2 = Str(c, "file2");
			}
			p.assets.push_back(std::move(x));
		}
	if (j.contains("packages") && j["packages"].is_array())
		for (const json& k : j["packages"]) {
			Package x;
			x.id = Str(k, "id"); x.version = Str(k, "version", "1.0.0"); x.name = Str(k, "name"); x.description = Str(k, "description"); x.cls = Str(k, "class");
			x.assets = StrList(k, "assets"); x.conflicts = StrList(k, "conflicts"); x.overrides = StrList(k, "overrides"); x.modes = StrList(k, "modes");
			if (k.contains("requires") && k["requires"].is_array())
				for (const json& r : k["requires"]) {
					if (r.is_object()) x.requires_.push_back({Str(r, "id"), Str(r, "version")});
					else if (r.is_string()) { const std::string s = r.get<std::string>(); const size_t at = s.find('@'); x.requires_.push_back({s.substr(0, at), at == std::string::npos ? "" : s.substr(at + 1)}); }
				}
			p.packages.push_back(std::move(x));
		}
	if (j.contains("outputs") && j["outputs"].is_array())
		for (const json& o : j["outputs"]) {
			Output x;
			x.target = Str(o, "target"); x.package = Str(o, "package"); x.version = Str(o, "version"); x.path = Str(o, "path");
			x.digest = Str(o, "digest"); x.simDigest = Str(o, "sim_digest"); x.when = Str(o, "when");
			if (o.contains("files") && o["files"].is_array()) for (const json& f : o["files"]) x.files.push_back({Str(f, "path"), Str(f, "sha256")});
			p.outputs.push_back(std::move(x));
		}
	const std::string bad = Validate(p);
	if (!bad.empty()) { err = bad; return false; }
	p.dir = out.dir;
	out = std::move(p);
	return true;
}

std::string Write(const Project& p)
{
	ordered_json j;
	j["hmod"] = p.hmod; j["id"] = p.id; j["name"] = p.name; j["author"] = p.author; j["game"] = p.game;
	j["base"] = {{"exe_sha256", p.base.exeSha256}, {"packset", p.base.packset}};
	j["sources"] = ordered_json::array();
	for (const SourceRef& s : p.sources) {
		ordered_json o = {{"id", s.id}, {"kind", s.kind}};
		auto put = [&](const char* k, const std::string& v) { if (!v.empty()) o[k] = v; };
		put("path", s.path); put("imported", s.imported); put("tool", s.tool); put("tool_version", s.toolVersion); put("credit", s.credit); put("note", s.note);
		j["sources"].push_back(o);
	}
	j["assets"] = ordered_json::array();
	for (const Asset& a : p.assets) {
		ordered_json o = {{"id", a.id}, {"category", a.category}};
		if (!a.cls.empty()) o["class"] = a.cls;
		o["targets"] = a.targets;
		ordered_json org = {{"kind", a.origin.kind}};
		if (!a.origin.of.empty()) org["of"] = a.origin.of;
		if (!a.origin.files.empty()) org["files"] = a.origin.files;
		if (!a.origin.source.empty()) org["source"] = a.origin.source;
		if (!a.origin.note.empty()) org["note"] = a.origin.note;
		if (!a.origin.base.empty()) {
			org["base"] = ordered_json::array();
			for (const BaseRef& b : a.origin.base) org["base"].push_back({{"path", b.path}, {"pack", b.pack}, {"sha256", b.sha256}});
		}
		o["origin"] = org;
		o["export"] = a.exportMode;
		o["depends"] = a.depends;
		if (a.chara.any()) {
			ordered_json c;
			if (a.chara.charaNo >= 0) c["chara_no"] = a.chara.charaNo;
			if (a.chara.cell >= 0) c["cell"] = a.chara.cell;
			if (!a.chara.name.empty()) c["name"] = a.chara.name;
			if (!a.chara.file1.empty()) c["file1"] = a.chara.file1;
			if (!a.chara.file2.empty()) c["file2"] = a.chara.file2;
			o["charaselect"] = c;
		}
		j["assets"].push_back(o);
	}
	j["packages"] = ordered_json::array();
	for (const Package& k : p.packages) {
		ordered_json o = {{"id", k.id}, {"version", k.version}};
		if (!k.name.empty()) o["name"] = k.name;
		if (!k.description.empty()) o["description"] = k.description;
		if (!k.cls.empty()) o["class"] = k.cls;
		o["assets"] = k.assets;
		o["requires"] = ordered_json::array();
		for (const auto& r : k.requires_) o["requires"].push_back({{"id", r.id}, {"version", r.version}});
		o["conflicts"] = k.conflicts; o["overrides"] = k.overrides;
		if (!k.modes.empty()) o["modes"] = k.modes;
		j["packages"].push_back(o);
	}
	j["outputs"] = ordered_json::array();
	for (const Output& o : p.outputs) {
		ordered_json x = {{"target", o.target}, {"package", o.package}, {"version", o.version}, {"path", o.path}, {"digest", o.digest}};
		if (!o.simDigest.empty()) x["sim_digest"] = o.simDigest;
		x["when"] = o.when;
		x["files"] = ordered_json::array();
		for (const OutputFile& f : o.files) x["files"].push_back({{"path", f.path}, {"sha256", f.sha256}});
		j["outputs"].push_back(x);
	}
	return j.dump(2) + "\n";
}

bool Load(const std::string& pathOrDir, Project& out, std::string& err)
{
	std::error_code ec;
	fs::path f = fs::u8path(pathOrDir);
	if (fs::is_directory(f, ec)) f /= "hmod.json";
	std::ifstream in(f, std::ios::binary);
	if (!in) { err = "cannot read " + f.u8string(); return false; }
	const std::string text((std::istreambuf_iterator<char>(in)), {});
	Project p;
	p.dir = f.parent_path().u8string();
	if (!Parse(text, p, err)) return false;
	out = std::move(p);
	return true;
}

bool Save(const Project& p, std::string& err)
{
	const std::string bad = Validate(p);
	if (!bad.empty()) { err = bad; return false; }
	std::error_code ec;
	const fs::path d = fs::u8path(p.dir);
	fs::create_directories(d / "assets", ec); fs::create_directories(d / "sources", ec); fs::create_directories(d / "recipes", ec);
	const fs::path tmp = d / "hmod.json.tmp";
	{
		std::ofstream f(tmp, std::ios::binary);
		if (!f) { err = "cannot write " + tmp.u8string(); return false; }
		f << Write(p);
		if (!f) { err = "write failed"; return false; }
	}
	fs::rename(tmp, d / "hmod.json", ec);
	if (ec) { err = "rename failed: " + ec.message(); return false; }
	return true;
}

bool Create(const std::string& dir, const std::string& id, const std::string& game, Project& out, std::string& err)
{
	Project p;
	p.id = id; p.name = id; p.game = game; p.dir = dir;
	const std::string bad = Validate(p);
	if (!bad.empty()) { err = bad; return false; }
	std::error_code ec;
	if (fs::exists(fs::u8path(dir) / "hmod.json", ec)) { err = "hmod.json already exists in " + dir; return false; }
	if (!Save(p, err)) return false;
	out = std::move(p);
	return true;
}

} // namespace hmod
