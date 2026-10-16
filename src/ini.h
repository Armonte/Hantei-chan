#ifndef INI_H_GUARD
#define INI_H_GUARD
#include "framedata.h"
#include "cg.h"
#include <functional>
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
	std::vector<std::string> recentFiles;    // File > Open Recent: files opened through the smart Open... (max 10)
	bool detachableWindows = true;  // ImGui multi-viewport (native detached windows); restart-bound
	std::vector<std::string> keyBindings;    // "Key=<action>#<n>=<vk>,<mods>" overrides (issue #9)
	bool invertWheelZoom = false;            // wheel up zooms out (issue #9)
	std::string stageGameDir;                // last stage list (game bg folder) for the stage browser/dropdown
	int stageGame = 0;                       // 0 MBAACC, 1 MBAC
	bool stagePatAuthoring = true;           // PAT placement: Authoring (default) / Game-exact
	bool stageClampCamera = true;            // keep the stage view's game camera inside the game's limits
	bool mbtlCgZoomFilter = true;            // View > Filter: MBTL CG sprites at zoom > 1 use the game's *Filter_V 3x3 palette blur

	// Edit > Preferences > Auto backup: copy the file a save is about to overwrite (file_backup.h)
	bool backupHa6 = true;                   // character files (HA6, .DAT, ...)
	bool backupPat = true;                   // .pat files
	int backupKeep = 10;                     // backups kept per file (0 = all)
	std::string backupFolder;                // empty = "backup" next to the file; relative = from the file's folder

	// Editor panels shown in new tabs (Window menu), saved on exit
	bool showAnimPane = true, showAttackPane = true, showBoxPane = true;
	bool showPatPartSetPane = true, showPatPartPane = true, showPatShapePane = true, showPatTexturePane = true, showPatToolPane = true;
	// PAT editor: highlight of the selected part (the others are drawn at highlightOpacity) and overlay colours
	bool patHighlight = false;
	float patHighlightOpacity = 0.3f;
	float patUvRectColor[4]{0.f, 1.f, 1.f, 1.f};     // UV / texture view: the selected cut-out's rectangle
	float patCenterColor[4]{1.f, 0.5f, 1.f, 1.f};    // part centre cross
	float patSelectedColor[4]{0.2f, 1.f, 0.4f, 1.f}; // outline of the selected part
	bool patOutlineSelected = true;

	// Mouse buttons of the view (issue #9): 0 left, 1 right, 2 middle
	int mousePanButton = 0;                  // drag pans the view
	int mouseDrawButton = 1;                 // drag draws a box (UV rectangle in a PAT tab)

	// Script panel hot reload (fbmod pipe): on/off and the game install folder per profile ("" = Steam default)
	bool sqHotReload = false;
	int sqHotReloadMode = 0;   // 0: engine rebuild (reloadchara <chr>), 1: live in-place (reloadchara <chr> live)
	std::string sqGameDirUni2, sqGameDirMbtl;
	// 3D stage view "Open in Blender": blender.exe, UTF-8 ("" = auto-detect at launch, s3d_blender.h)
	// (Edit > Preferences > 3D stage: Blender; ini key Stage3dBlenderPath=, the older BlenderPath= is still read and wins
	// only when the new key is absent)
	// Default: the Blender 5.2.2 portable build (s3d::DefaultBlenderPath); a saved pre-5 Blender is not migrated.
	std::string blenderPath = "C:/apps/blender-5.2.2-windows-x64/blender.exe";
	bool blenderPathFromNewKey = false;
} gSettings;

bool LoadFromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path = nullptr, Parts* parts = nullptr, std::string* outPATPath = nullptr);
// Thumbnail loader (archive browser): the HA6 stack of a project .txt and the sprite bank, nothing else (no .pat parts, no scripts, no effect library). With `readCg` the bank
// bytes come from the caller (an archive entry named like the txt's BmpcutFile File00, plain bytes) instead of from <folder>\<File00>; palettes (<cg stem>.pal, _p1..7) are read from disk.
bool LoadStackLight(FrameData *framedata, CG *cg, const std::string& iniPath, const std::function<bool(const std::string &cgName, std::vector<uint8_t> &bytes)> &readCg = nullptr);
bool LoadChrHA6FromIni(FrameData *framedata, CG *cg, const std::string& iniPath, std::string* outTopHA6Path = nullptr, Parts* parts = nullptr, std::string* outPATPath = nullptr);
bool AddHA6ToTxt(const std::string& txtPath, const std::string& ha6Filename);
void InitIni();

#endif /* INI_H_GUARD */
