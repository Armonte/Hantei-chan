#include "s3d_blender.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace s3d {

static const char kFoundation[] = "C:/Program Files/Blender Foundation";

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

std::vector<std::string> BlenderCandidates(const std::vector<std::string>& foundationDirNames)
{
	struct V { int major, minor; std::string name; };
	std::vector<V> vs;
	for (const std::string& n : foundationDirNames) {
		int a = 0, b = 0;
		if (sscanf(n.c_str(), "Blender %d.%d", &a, &b) == 2) vs.push_back({a, b, n});
	}
	std::sort(vs.begin(), vs.end(), [](const V& x, const V& y) { return x.major != y.major ? x.major > y.major : x.minor > y.minor; });
	std::vector<std::string> out;
	for (const V& v : vs) out.push_back(std::string(kFoundation) + "/" + v.name + "/blender.exe");
	out.push_back("C:/dev/blender40/blender-4.0.2-windows-x64/blender.exe");
	return out;
}

std::string DetectBlender()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<std::string> names;
	for (fs::directory_iterator it(fs::u8path(kFoundation), ec), end; !ec && it != end; it.increment(ec))
		if (it->is_directory(ec)) names.push_back(it->path().filename().u8string());
	for (const std::string& c : BlenderCandidates(names))
		if (fs::exists(fs::u8path(c), ec)) return c;
	return {};
}

}  // namespace s3d
