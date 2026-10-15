#ifndef S3D_BGLIST_H_GUARD
#define S3D_BGLIST_H_GUARD
// bg\BgList.txt (UNI2 / UNI / DFCI / MBTL): Squirrel-style table, Shift-JIS, `//` comments.
//   BgList <- { BgSelectList = [1, 2, ...], Bg_001 = { Name = "...", DataFile = "bg001", Scale = [2.0, 2.0, 2.0], FOV = 45.0, ... } }
// Every key is kept (numbers and number lists in `nums`, strings in `strs`, nested tables / arrays of tables as raw text in
// `raw`), so new keys need no parser change. Typed accessors give the values the 3D stage view uses, with the game's defaults
// (UNI2 BG record defaults, docs/stage3d/SCOPE.md §3).
#include <map>
#include <string>
#include <vector>

namespace s3d {

struct BgEntry {
	int number = 0;   // Bg_NNN
	std::map<std::string, std::vector<float>> nums;
	std::map<std::string, std::string> strs;   // DataFile, Name (Shift-JIS bytes as in the file)
	std::map<std::string, std::string> raw;    // ShadowLightStatus = [ {...}, ... ] etc.

	float num(const std::string& k, float def, int i = 0) const;
	std::string str(const std::string& k) const;
	bool has(const std::string& k) const { return nums.count(k) || strs.count(k) || raw.count(k); }
};

struct BgList {
	std::vector<int> selectList;
	std::vector<BgEntry> entries;
	const BgEntry* byDataFile(const std::string& dataFile) const;   // case-insensitive "bg001"
	const BgEntry* byNumber(int n) const;
};

bool ParseBgList(const std::string& text, BgList& out, std::string& err);
bool LoadBgListFile(const std::string& utf8Path, BgList& out, std::string& err);

// The parameters the viewer applies (UNI2 field names; DFCI / MBTL use the same keys).
struct StageParams {
	float scale[3] = {1, 1, 1};
	float position[3] = {0, 0, 0};
	float fov = 45.f;              // degrees, fed to PerspectiveFovLH with aspect 1.0 (vertical; the stage world squeezes X by 9/16)
	float vanishingPoint = 0.f;
	float viewRotationX = 0.f, viewRotationY = 0.f, viewTranslationY = 0.f;   // degrees (stage world rotation)
	bool fog = false;              // IsFog (0 zeroes FogColor; the shader keys fog on FogColor.a >= 0.001)
	float fogStart = 0.f, fogEnd = 0.f;
	float fogColor[4] = {0, 0, 0, 0};
	int stageW = 2560;
	bool bloom = false;            // BGBloomEnable
	float bloomBrightness = 0.f, bloomPower = 0.f, bloomBias[3] = {1, 1, 1}, bloomBlurRadius = 0.f, bloomAlpha = 0.5f;
	int bloomTexSize = 256;
	float shadowScale = 0.6f, shadowAlpha = 0.7f;
	int targetW = 1280, targetH = 720;
};
StageParams ParamsFrom(const BgEntry& e);

}  // namespace s3d
#endif
