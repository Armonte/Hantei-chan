#include "ptcn_stack.h"

#include "../framedata.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#include <windows.h>

namespace sqedit {

std::map<std::string, int> LoadPatternCodes(const std::string& txtPathUtf8)
{
	namespace fs = std::filesystem;
	std::map<std::string, int> out;
	const fs::path txt = fs::u8path(txtPathUtf8);
	const std::string t = txt.string();
	const int n = GetPrivateProfileIntA("DataFile", "FileNum", 0, t.c_str());
	std::vector<std::string> names;
	for (int i = 0; i < n; ++i) {
		char key[16], val[260] = {};
		snprintf(key, sizeof key, i >= 100 ? "File%03d" : "File%02d", i);
		GetPrivateProfileStringA("DataFile", key, "", val, sizeof val, t.c_str());
		names.push_back(val);
	}
	if (names.empty()) return out;
	const int own = FrameData::StackSaveTarget(names);
	FrameData fd;
	bool any = false;
	for (int i = 0; i < (int)names.size(); ++i) {
		if (names[i].empty()) continue;
		const fs::path p = txt.parent_path() / names[i];
		std::error_code ec;
		if (!fs::exists(p, ec)) continue;
		if (fd.load(p.string().c_str(), any, own >= 0 && i > own)) any = true;
	}
	for (int i = 0; i < (int)fd.get_sequence_count(); ++i) {
		const Sequence* s = fd.get_sequence(i);
		if (s && !s->codeName.empty()) out.emplace(std::string(s->codeName.c_str()), i);
	}
	return out;
}

} // namespace sqedit
