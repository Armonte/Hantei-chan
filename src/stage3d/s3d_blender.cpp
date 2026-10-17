#include "s3d_blender.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace s3d {

static const char kFoundation[] = "C:/Program Files/Blender Foundation";
static const char kApps[] = "C:/apps";   // portable zips: C:/apps/blender-5.2.2-windows-x64

std::string BlenderImportExpr(const std::string& stagePath, std::string& err)
{
	if (stagePath.empty()) { err = "no stage path"; return {}; }
	std::string p;
	p.reserve(stagePath.size());
	for (char c : stagePath) {
		if (c == '\'' || c == '"') { err = "stage path contains a quote character"; return {}; }
		if (static_cast<unsigned char>(c) < 0x20) { err = "stage path contains a control character"; return {}; }
		p += c == '\\' ? '/' : c;
	}
	return "import addon_utils,bpy; addon_utils.enable('MBTL_fbx_json', default_set=True, persistent=True); "
	       "bpy.ops.mbtl.hantei_open(filepath=r'" + p + "')";
}

std::string BlenderCommandLine(const std::string& blenderExe, const std::string& stagePath, std::string& err)
{
	if (blenderExe.empty()) { err = "no Blender executable set"; return {}; }
	if (blenderExe.find('"') != std::string::npos) { err = "Blender path contains a quote character"; return {}; }
	const std::string expr = BlenderImportExpr(stagePath, err);
	if (expr.empty()) return {};
	return "\"" + blenderExe + "\" --python-expr \"" + expr + "\"";
}

// "Blender 4.5" -> {4,5}; "blender-5.2.2-windows-x64" -> {5,2,2}; empty when the name has no version.
static std::vector<int> ParseVersion(const std::string& name, const char* prefix)
{
	const size_t n = strlen(prefix);
	if (name.size() <= n || name.compare(0, n, prefix) != 0) return {};
	std::vector<int> v;
	size_t i = n;
	while (i < name.size() && isdigit(static_cast<unsigned char>(name[i]))) {
		int x = 0;
		while (i < name.size() && isdigit(static_cast<unsigned char>(name[i]))) x = x * 10 + (name[i++] - '0');
		v.push_back(x);
		if (i < name.size() && name[i] == '.') ++i; else break;
	}
	return v.size() >= 2 ? v : std::vector<int>{};
}

std::vector<std::string> BlenderCandidates(const std::vector<std::string>& foundationDirNames, const std::vector<std::string>& appsDirNames)
{
	struct V { std::vector<int> ver; bool portable; std::string path; };
	std::vector<V> vs;
	for (const std::string& n : foundationDirNames) {
		std::vector<int> v = ParseVersion(n, "Blender ");
		if (!v.empty()) vs.push_back({v, false, std::string(kFoundation) + "/" + n + "/blender.exe"});
	}
	for (const std::string& n : appsDirNames) {
		std::vector<int> v = ParseVersion(n, "blender-");
		if (!v.empty()) vs.push_back({v, true, std::string(kApps) + "/" + n + "/blender.exe"});
	}
	std::stable_sort(vs.begin(), vs.end(), [](const V& x, const V& y) {
		if (x.ver != y.ver) return x.ver > y.ver;   // lexicographic per component, longer wins on a shared prefix
		return x.portable && !y.portable;
	});
	std::vector<std::string> out;
	for (const V& v : vs) out.push_back(v.path);
	return out;
}

const char* DefaultBlenderPath() { return "C:/apps/blender-5.2.2-windows-x64/blender.exe"; }

bool BlenderPathIsPre5(const std::string& exePath)
{
	std::string p = exePath;
	std::replace(p.begin(), p.end(), '\\', '/');
	size_t i = 0;
	while (i <= p.size()) {   // any path component "Blender X.Y" / "blender-X.Y.Z-..." with X < 5
		const size_t j = std::min(p.find('/', i), p.size());
		const std::string part = p.substr(i, j - i);
		for (const char* prefix : {"Blender ", "blender-"}) {
			const std::vector<int> v = ParseVersion(part, prefix);
			if (!v.empty()) return v[0] < 5;
		}
		i = j + 1;
	}
	return false;
}

static std::vector<std::string> ChildDirs(const char* dir)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<std::string> names;
	for (fs::directory_iterator it(fs::u8path(dir), ec), end; !ec && it != end; it.increment(ec))
		if (it->is_directory(ec)) names.push_back(it->path().filename().u8string());
	return names;
}

std::string DetectBlender()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	for (const std::string& c : BlenderCandidates(ChildDirs(kFoundation), ChildDirs(kApps)))
		if (fs::exists(fs::u8path(c), ec)) return c;
	return {};
}

std::vector<std::string> BlenderWatchPaths(const std::string& modelPath)
{
	namespace fs = std::filesystem;
	std::string m = modelPath;
	std::replace(m.begin(), m.end(), '\\', '/');
	if (m.empty()) return {};
	const std::string bin = (fs::u8path(m).parent_path() / "bg.fbx.bin").generic_u8string();
	std::vector<std::string> out{bin};
	if (m != bin) out.push_back(m);
	return out;
}

std::vector<std::string> ScanStageFolders(const std::string& bgDir)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<std::string> out;
	for (fs::directory_iterator it(fs::u8path(bgDir), ec), end; !ec && it != end; it.increment(ec)) {
		if (!it->is_directory(ec)) continue;
		if (fs::is_regular_file(it->path() / "bg.fbx.bin", ec) || fs::is_regular_file(it->path() / "bg.fbx", ec))
			out.push_back(it->path().filename().u8string());
	}
	std::sort(out.begin(), out.end());
	return out;
}

}  // namespace s3d
