#include "project_entries.h"
#include <filesystem>

using json = nlohmann::json;

namespace projentry {

std::string MakeRelativePath(const std::string& filePath, const std::string& projectPath)
{
	try {
		std::filesystem::path file(filePath);
		std::filesystem::path projectDir = std::filesystem::path(projectPath).parent_path();
		// Same drive/root: relative to the project file, so the project folder can move.
		if (file.root_path() == projectDir.root_path())
			return std::filesystem::relative(file, projectDir).string();
	} catch (...) {
		// fall through: keep it absolute
	}
	return filePath;
}

std::string MakeAbsolutePath(const std::string& filePath, const std::string& projectPath)
{
	if (filePath.empty()) return filePath;
	std::filesystem::path file(filePath);
	if (file.is_absolute()) return filePath;
	return (std::filesystem::path(projectPath).parent_path() / file).string();
}

const char* PatFormatName(bool mbaacc) { return mbaacc ? "mbaacc" : "uni"; }

json MakeCharacterEntry(const CharacterInfo& c, const std::string& projectPath)
{
	json e;
	e["name"] = c.name;
	if (c.patDocument) {
		if (c.patPath.empty()) return json(); // New PAT never saved: nothing to reopen
		e["type"] = "pat";
		e["path"] = MakeRelativePath(c.patPath, projectPath);
		e["pat_format"] = PatFormatName(c.patMbaacc);
		return e;
	}
	if (!c.txtPath.empty()) {
		e["type"] = "txt";
		e["path"] = MakeRelativePath(c.txtPath, projectPath);
	} else if (!c.ha6Path.empty()) {
		e["type"] = "ha6";
		e["path"] = MakeRelativePath(c.ha6Path, projectPath);
	} else {
		return json();
	}
	if (!c.patPath.empty()) {
		e["pat_path"] = MakeRelativePath(c.patPath, projectPath);
		e["pat_format"] = PatFormatName(c.patMbaacc);
	}
	return e;
}

LoadPlan PlanCharacterLoad(const json& entry, const std::string& projectPath)
{
	LoadPlan p;
	if (!entry.is_object()) return p;
	const std::string type = entry.value("type", std::string());
	p.path = MakeAbsolutePath(entry.value("path", std::string()), projectPath);
	if (type == "txt") p.kind = LoadPlan::Kind::Txt;
	else if (type == "ha6") p.kind = LoadPlan::Kind::Ha6;
	else if (type == "pat") p.kind = LoadPlan::Kind::Pat;
	if (p.kind != LoadPlan::Kind::Pat)
		p.patPath = MakeAbsolutePath(entry.value("pat_path", std::string()), projectPath);
	auto fmt = entry.find("pat_format");
	if (fmt != entry.end() && fmt->is_string()) {
		const std::string f = fmt->get<std::string>();
		if (f == "mbaacc" || f == "uni") { p.hasPatFormat = true; p.patMbaacc = f == "mbaacc"; }
	}
	if (p.path.empty()) p.kind = LoadPlan::Kind::None;
	return p;
}

std::string DescribeFailed(const json& entry, const std::string& absolutePath)
{
	if (absolutePath.empty()) return entry.is_object() ? entry.value("name", std::string("(unnamed)")) : std::string("(unnamed)");
	std::error_code ec;
	if (!std::filesystem::exists(std::filesystem::u8path(absolutePath), ec))
		return absolutePath + " (file not found)";
	return absolutePath;
}

} // namespace projentry
