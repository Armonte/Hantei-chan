#include "ini.h"
#include "parts/parts.h"
#include "misc.h"
#include "extension_profile.h"
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cctype>
#include <filesystem>
#include <windows.h>

#include <imgui.h>
#include <imgui_internal.h>

Settings gSettings;

static void* ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
	return &gSettings;
}

static void ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	float x, y, z;
	int i;
	float j;
	if (sscanf(line, "Color=%f,%f,%f", &x, &y, &z) == 3) {
		gSettings.color[0] = x;
		gSettings.color[1] = y;
		gSettings.color[2] = z;
	} else if (sscanf(line, "Zoom=%f", &j) == 1) {
		gSettings.zoomLevel = j;
	} else if (sscanf(line, "Bilinear=%i", &i) == 1){
		gSettings.bilinear = i; 
	} else if (sscanf(line, "Theme=%i", &i) == 1){
		gSettings.theme = i;
	} else if (sscanf(line, "FontSize=%f", &x) == 1){
		gSettings.fontSize = x;
	} else if (sscanf(line, "posX=%i", &i) == 1){
		gSettings.posX = i;
	} else if (sscanf(line, "posY=%i", &i) == 1){
		gSettings.posY = i;
	} else if (sscanf(line, "sizeX=%i", &i) == 1){
		gSettings.winSizeX = i;
	} else if (sscanf(line, "sizeY=%i", &i) == 1){
		gSettings.winSizeY = i;
	} else if (sscanf(line, "Maximized=%i", &i) == 1){
		gSettings.maximized = i;
	} else if (sscanf(line, "DetachableWindows=%i", &i) == 1){
		gSettings.detachableWindows = i != 0;
	} else if (sscanf(line, "InvertWheelZoom=%i", &i) == 1){
		gSettings.invertWheelZoom = i != 0;
	} else if (sscanf(line, "StagePatAuthoring=%i", &i) == 1){
		gSettings.stagePatAuthoring = i != 0;
	} else if (sscanf(line, "StageClampCamera=%i", &i) == 1){
		gSettings.stageClampCamera = i != 0;
	} else if (sscanf(line, "StageGame=%i", &i) == 1){
		gSettings.stageGame = i;
	} else if (strncmp(line, "StageGameDir=", 13) == 0){
		gSettings.stageGameDir = line + 13;
	} else if (strncmp(line, "Key=", 4) == 0){
		gSettings.keyBindings.push_back(line);
	} else if (strncmp(line, "RecentProject=", 14) == 0){
		// Normalize path when loading from INI for consistency
		std::string path = normalizePath(line + 14);
		if (!path.empty()) {
			gSettings.recentProjects.push_back(path);
		}
	}
}

static void Write(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
	buf->appendf("[%s][]\n", handler->TypeName);
	buf->appendf("Color=%f,%f,%f\n", gSettings.color[0], gSettings.color[1], gSettings.color[2]);
	buf->appendf("Zoom=%f\n", gSettings.zoomLevel);
	buf->appendf("Bilinear=%i\n", gSettings.bilinear);
	buf->appendf("Theme=%i\n", gSettings.theme);
	buf->appendf("FontSize=%f\n", gSettings.fontSize);
	buf->appendf("posX=%hi\n", gSettings.posX);
	buf->appendf("posY=%hi\n", gSettings.posY);
	buf->appendf("sizeX=%hi\n", gSettings.winSizeX);
	buf->appendf("sizeY=%hi\n", gSettings.winSizeY);
	buf->appendf("Maximized=%i\n", gSettings.maximized);
	buf->appendf("DetachableWindows=%i\n", gSettings.detachableWindows ? 1 : 0);

	buf->appendf("InvertWheelZoom=%i\n", gSettings.invertWheelZoom ? 1 : 0);
	if (!gSettings.stageGameDir.empty()) buf->appendf("StageGameDir=%s\n", gSettings.stageGameDir.c_str());
	buf->appendf("StageGame=%i\n", gSettings.stageGame);
	buf->appendf("StagePatAuthoring=%i\n", gSettings.stagePatAuthoring ? 1 : 0);
	buf->appendf("StageClampCamera=%i\n", gSettings.stageClampCamera ? 1 : 0);
	for (const auto& k : gSettings.keyBindings)
		buf->appendf("%s\n", k.c_str());

	// Write recent projects (max 10)
	size_t maxRecent = gSettings.recentProjects.size() > 10 ? 10 : gSettings.recentProjects.size();
	for (size_t i = 0; i < maxRecent; i++) {
		buf->appendf("RecentProject=%s\n", gSettings.recentProjects[i].c_str());
	}

	buf->append("\n");
}

void InitIni()
{
	ImGuiContext &context = *ImGui::GetCurrentContext();
	ImGuiSettingsHandler ini_handler{};
	ini_handler.TypeName = "Other settings";
	ini_handler.TypeHash = ImHashStr("Other settings");
	ini_handler.ReadOpenFn = ReadOpen;
	ini_handler.ReadLineFn = ReadLine;
	ini_handler.WriteAllFn = Write;
	context.SettingsHandlers.push_back(ini_handler);
	RegisterExtensionProfileSettings(context);
	ImGui::LoadIniSettingsFromDisk(context.IO.IniFilename);
}


// [DataFile] file list of a project .txt, read the way the games do
// (Han6_ParseProjectTxt: uni2.exe 0x4C3BA0 / MBTL.exe 0x4A5A50): FileNum N and
// File%0*d with width 2, or 3 once N >= 100; without FileNum, a single
// [DataFile] File= entry.
static std::vector<std::string> ReadDataFileList(const std::string& iniPath)
{
	std::vector<std::string> names;
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, iniPath.c_str());
	if(fileNum > 0 && fileNum < 1000)
	{
		const int width = fileNum >= 100 ? 3 : 2;
		for(int i = 0; i < fileNum; i++)
		{
			char ha6file[256]{};
			std::stringstream ss;
			ss << "File" << std::setfill('0') << std::setw(width) << i;
			GetPrivateProfileStringA("DataFile", ss.str().c_str(), nullptr, ha6file, 256, iniPath.c_str());
			names.push_back(ha6file);
		}
	}
	else if(fileNum == 0)
	{
		char ha6file[256]{};
		GetPrivateProfileStringA("DataFile", "File", nullptr, ha6file, 256, iniPath.c_str());
		if(ha6file[0])
			names.push_back(ha6file);
	}
	return names;
}

static void LoadCgAndPat(CG *cg, const std::string& folder, const std::string& iniPath, Parts* parts, std::string* outPATPath)
{
	int cgNum = GetPrivateProfileIntA("BmpcutFile", "FileNum", 0, iniPath.c_str());
	if(cgNum >= 1 && cg)
	{
		char cgFile[256]{};
		GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, iniPath.c_str());

		std::string fullpath = folder + "\\" + cgFile;
		auto extensionPos = fullpath.find_last_of(".");
		auto stem = fullpath.substr(0, extensionPos);
		cg->load(fullpath.c_str());
		// <cg>.pal is PUPS palette file 0; <cg>_p1.._p7.pal are files 1..7
		// (CharaPalette_LoadPalAndPupsVariants, MBTL.exe 0x5934B0). A pattern's
		// PUPS value picks the file (issue #76).
		cg->loadPupsPalettes(stem);
	}

	// Load .pat file if available (for UNIST/DFCI/MBTL/UNI2 characters)
	int patNum = GetPrivateProfileIntA("PAniFile", "FileNum", 0, iniPath.c_str());
	if(patNum >= 1 && parts != nullptr)
	{
		char patFile[256]{};
		GetPrivateProfileStringA("PAniFile", "File00", nullptr, patFile, 256, iniPath.c_str());

		if(patFile[0] != '\0')
		{
			std::string fullpath = folder + "\\" + patFile;
			if(parts->Load(fullpath.c_str()))
			{
				if(outPATPath)
					*outPATPath = fullpath;
			}
		}
	}
}

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path, Parts* parts, std::string* outPATPath)
{
	std::vector<std::string> ha6Names = ReadDataFileList(iniPath);
	if(ha6Names.empty())
		return false;
	const int fileNum = (int)ha6Names.size();
	std::string folder = iniPath.substr(0, iniPath.find_last_of("\\/"));

	// Save target (issue #46): the file whose patterns win in the game.
	// The games load File(N-1) first and File00 last, and a pattern already
	// loaded is kept (Han6_LoadProjectDataFiles_Reverse + Han6_LoadPattern_PSTR),
	// so the highest-indexed file wins -- except shared data outside the
	// project folder (../BaseData.HA6), whose PFLG template patterns only lend
	// timing to the character's own. So the target is the highest-indexed
	// file that is not shared data:
	//   UNI2/MBTL base   _temp, chrNNN, ../BaseData          -> chrNNN.ha6
	//   MBTL variant     _temp, chrNNN, chrNNN_7, ../BaseData -> chrNNN_7.ha6
	//   effect.txt       effect_temp, effect                  -> effect.ha6
	//   MBAACC           base, base_r, moon, moon_r           -> last file
	const int target = FrameData::StackSaveTarget(ha6Names);
	std::string topHA6File;
	if(target >= 0 && !ha6Names[target].empty())
		topHA6File = folder + "\\" + ha6Names[target];

	// Load in order; later files overlay earlier ones, which gives the game's
	// "highest index wins" for every pattern with frames. Files after the
	// target (shared BaseData) only fill empty slots: its patterns are
	// templates for slots the character also defines.
	for(int i = 0; i < fileNum; i++)
	{
		std::string fullpath = folder + "\\" + ha6Names[i];
		const bool fallback = i > target;
		if(!framedata->load(fullpath.c_str(), i > 0, fallback))
			return false;
	}

	// With several files, saving writes only the target's own patterns
	// (plus edits), not the whole merged stack (issue #71).
	if(fileNum > 1)
		framedata->setOwnFile(target);

	if(outTopHA6Path && !topHA6File.empty())
		*outTopHA6Path = topHA6File;

	LoadCgAndPat(cg, folder, iniPath, parts, outPATPath);
	return true;
}

bool LoadChrHA6FromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path, Parts* parts, std::string* outPATPath)
{
	// Load only the project's own file (the save target above), e.g.
	// chrNNN_7.ha6 for an MBTL variant project.
	std::vector<std::string> ha6Names = ReadDataFileList(iniPath);
	if(ha6Names.size() < 2)
		return false;
	const int target = FrameData::StackSaveTarget(ha6Names);
	if(target < 0 || ha6Names[target].empty())
		return false;

	std::string folder = iniPath.substr(0, iniPath.find_last_of("\\/"));
	std::string ha6fullpath = folder + "\\" + ha6Names[target];
	if(!framedata->load(ha6fullpath.c_str(), 0))
		return false;

	if(outTopHA6Path)
		*outTopHA6Path = ha6fullpath;

	LoadCgAndPat(cg, folder, iniPath, parts, outPATPath);
	return true;
}

bool AddHA6ToTxt(const std::string& txtPath, const std::string& ha6Filename)
{
	// Read the current file count
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, txtPath.c_str());
	if(fileNum == 0)
		return false;  // Invalid .txt file

	// Increment file count
	int newFileNum = fileNum + 1;
	std::stringstream fileNumStr;
	fileNumStr << newFileNum;

	// Write new file count
	if(!WritePrivateProfileStringA("DataFile", "FileNum", fileNumStr.str().c_str(), txtPath.c_str()))
		return false;

	// Add new HA6 file entry
	std::stringstream fileKey;
	fileKey << "File" << std::setfill('0') << std::setw(newFileNum >= 100 ? 3 : 2) << fileNum;  // Use old fileNum as index

	if(!WritePrivateProfileStringA("DataFile", fileKey.str().c_str(), ha6Filename.c_str(), txtPath.c_str()))
		return false;

	return true;
}