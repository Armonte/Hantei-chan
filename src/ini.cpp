#include "ini.h"
#include "parts/parts.h"
#include "misc.h"
#include <sstream>
#include <iomanip>
#include <string>
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
	ImGui::LoadIniSettingsFromDisk(context.IO.IniFilename);
}

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path, Parts* parts, std::string* outPATPath)
{
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, iniPath.c_str());
	if(fileNum)
	{
		std::string folder = iniPath.substr(0, iniPath.find_last_of("\\/"));
		std::string topHA6File;

		// Load all files (sosfiro's approach - the patch system handles overlays correctly)
		for(int i = 0; i < fileNum; i++)
		{
			char ha6file[256]{};
			std::stringstream ss;
			ss << "File" << std::setfill('0') << std::setw(2) << i;
			GetPrivateProfileStringA("DataFile", ss.str().c_str(), nullptr, ha6file, 256, iniPath.c_str());

			std::string fullpath = folder + "\\" + ha6file;
			if(!framedata->load(fullpath.c_str(), i))
				return false;
		}

		// Determine the save target HA6 file (highest-indexed file)
		// For uni2/mbtl/dbfci/unist: File00=temp, File01=chrxxx, File02=BaseData (use File01)
		// For MBAACC: File00=base, File01=base_r, File02=variant, File03=variant_r (use highest)
		// Always use the highest-indexed file as the save target
		if(fileNum > 0)
		{
			// Use the highest-indexed file (FileNum - 1)
			char ha6file[256]{};
			std::stringstream ss;
			ss << "File" << std::setfill('0') << std::setw(2) << (fileNum - 1);
			GetPrivateProfileStringA("DataFile", ss.str().c_str(), nullptr, ha6file, 256, iniPath.c_str());
			if(ha6file[0] != '\0')
			{
				topHA6File = folder + "\\" + ha6file;
			}
		}

		// Return the top HA6 file path if caller wants it
		if(outTopHA6Path && !topHA6File.empty()) {
			*outTopHA6Path = topHA6File;
		}

		int cgNum = GetPrivateProfileIntA("BmpcutFile", "FileNum", 0, iniPath.c_str());
		if(cgNum == 1)
		{
			char cgFile[256]{};
			GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, iniPath.c_str());

			std::string fullpath = folder + "\\" + cgFile;
			auto extensionPos = fullpath.find_last_of(".");
			auto palPath = fullpath.substr(0, extensionPos) + ".pal";
			cg->load(fullpath.c_str());
			if(std::filesystem::exists(palPath))
			{
				cg->loadPalette(palPath.c_str());
			}
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
					// Return the PAT file path if caller wants it
					if(outPATPath)
					{
						*outPATPath = fullpath;
					}
				}
			}
		}

		return true;
	}
	else
		return false;
}

bool LoadChrHA6FromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path, Parts* parts, std::string* outPATPath)
{
	int fileNum = GetPrivateProfileIntA("DataFile", "FileNum", 0, iniPath.c_str());
	if(fileNum < 2)
		return false;  // Need at least File00 and File01

	std::string folder = iniPath.substr(0, iniPath.find_last_of("\\/"));

	// Load only File01 (the main character .ha6)
	// Skip File00 (_temp.ha6) and File02+ (BaseData.ha6, etc.)
	char ha6file[256]{};
	GetPrivateProfileStringA("DataFile", "File01", nullptr, ha6file, 256, iniPath.c_str());

	if(ha6file[0] == '\0')
		return false;

	std::string ha6fullpath = folder + "\\" + ha6file;
	if(!framedata->load(ha6fullpath.c_str(), 0))
		return false;

	// Return the HA6 file path if caller wants it
	if(outTopHA6Path) {
		*outTopHA6Path = ha6fullpath;
	}

	// Load .cg file
	int cgNum = GetPrivateProfileIntA("BmpcutFile", "FileNum", 0, iniPath.c_str());
	if(cgNum == 1)
	{
		char cgFile[256]{};
		GetPrivateProfileStringA("BmpcutFile", "File00", nullptr, cgFile, 256, iniPath.c_str());

		std::string fullpath = folder + "\\" + cgFile;
		auto extensionPos = fullpath.find_last_of(".");
		auto palPath = fullpath.substr(0, extensionPos) + ".pal";
		cg->load(fullpath.c_str());
		if(std::filesystem::exists(palPath))
		{
			cg->loadPalette(palPath.c_str());
		}
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
				// Return the PAT file path if caller wants it
				if(outPATPath)
				{
					*outPATPath = fullpath;
				}
			}
		}
	}

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
	fileKey << "File" << std::setfill('0') << std::setw(2) << fileNum;  // Use old fileNum as index

	if(!WritePrivateProfileStringA("DataFile", fileKey.str().c_str(), ha6Filename.c_str(), txtPath.c_str()))
		return false;

	return true;
}