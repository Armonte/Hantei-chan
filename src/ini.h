#ifndef INI_H_GUARD
#define INI_H_GUARD
#include "framedata.h"
#include "cg.h"
#include <vector>
#include <string>

// Forward declaration
class Parts;

extern struct Settings
{
	//default
	float color[3]{0.324f, 0.409f, 0.185f};
	float zoomLevel = 3.0f;
	bool bilinear = 0;
	int theme = 0;
	float fontSize = 18.f;
	short posX = 100;
	short posY = 100;
	short winSizeX = 1280;
	short winSizeY = 800;
	bool maximized = false;
	std::vector<std::string> recentProjects; // Recent project paths (max 10)
} gSettings;

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path = nullptr, Parts* parts = nullptr, std::string* outPATPath = nullptr);
bool LoadChrHA6FromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path = nullptr, Parts* parts = nullptr, std::string* outPATPath = nullptr);
bool AddHA6ToTxt(const std::string& txtPath, const std::string& ha6Filename);
void InitIni();

#endif /* INI_H_GUARD */
