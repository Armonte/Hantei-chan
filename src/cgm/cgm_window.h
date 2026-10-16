#ifndef CGM_WINDOW_H_GUARD
#define CGM_WINDOW_H_GUARD
// Tools > CG manager: browse / search the whole sprite bank of the active character (thumbnail grid, usage, filters, preview).
#include "cgm_bank.h"
#include "cgm_usage.h"
#include "cgm_undo.h"
#include "cgm_palette.h"
#include "cgm_project.h"
#include "fxr_panel.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CharacterInstance;
struct ImVec2;

namespace cgm {

struct WindowHost {
	std::function<void(int pattern, int frame)> navigate;   // show a pattern / frame in the active view
	std::function<void(CharacterInstance *)> markEdited;    // the character changed (dirty flag + undo step)
	std::function<bool(CharacterInstance *)> saveCharacter; // File > Save of that character (its own HA6 file)
	std::function<bool(int &pattern, int &frame)> current;  // pattern / frame shown in the active view (Palettes tab preview)
};

class Window {
public:
	bool open = false;
	void draw(CharacterInstance *ch, const WindowHost &host);
	void select(int id) { selected = id; }   // scripted sessions (--ui-script cgmselect)
	~Window();
	// One save for the bank and the frame data (shared-bank safety): true when the bank in the manager renumbers images
	// against the .cg on disk, so saving the character alone would ship ids the bank on disk does not have.
	bool pendingIdChanges(const CharacterInstance *ch) const;
	// Writes the .cg (original kept as .bak) and rewrites the references of every other project file that draws it
	// (cgm_project.h). The loaded character's own file is left to its own save. Returns false on an error; `report` says what was written.
	bool saveBankFiles(CharacterInstance &ch, std::string &report);
	// --ui-script "cgm <cmd> <arg>": open | tab <name> | press <English button label> | path <file or folder for the next dialog> |
	// select <id> | set <field> <n> | status (logs the status line). False = logged (status, or an error in msg).
	bool script(const std::string &cmd, const std::string &arg, std::string *msg);

private:
	struct Thumb { unsigned tex = 0; int w = 0, h = 0; unsigned long long gen = 0; bool empty = false; };
	// view state
	char nameFilter[48] = {};
	bool onlyUnused = false, onlyUsed = false, onlyShared = false;
	int typeFilter = -2;            // -2 = any
	int minSize = 0, maxSize = 0;   // longest side, 0 = off
	int usedByPattern = -1;
	int thumbSize = 96;
	int selected = -1;
	float zoom = 2.f;
	int previewPal = 0, previewPups = 0;
	// per character caches
	const CharacterInstance *owner = nullptr;
	unsigned long long cgGen = ~0ull;
	std::unique_ptr<Bank> bank;     // parsed model (null for foreign banks / unparsable)
	std::unordered_map<uint32_t, int> atlas;
	UsageIndex usage;
	std::vector<int> shown;         // ids passing the filters
	uint64_t filterKey = 0;
	std::unordered_map<int, Thumb> thumbs;
	Thumb preview; int previewId = -1; unsigned long long previewGen = ~0ull;
	// palette tab
	struct PalState {
		PalSet set[8]; bool loaded[8] = {}; bool dirty[8] = {};
		int bank = 0, number = 0, selA = 0, selB = 0;
		uint32_t clip[256] = {}; int clipLen = 0;
		uint32_t fill = 0xFF808080u, gradA = 0xFF000000u, gradB = 0xFFFFFFFFu;
		ColorAdjust adj; bool hsvGradient = false;
		bool recolorThisOnly = true, recolorSlots = false, recolorImages = true, recolorFiles = true;
		uint32_t findC = 0xFF000000u, replaceC = 0xFF000000u; int tolerance = 0;
	} pal;
	const CharacterInstance *palOwner = nullptr;
	void loadPalettes(CharacterInstance &ch);
	void drawPalettes(CharacterInstance &ch, const WindowHost &host);
	// Palettes tab sprite preview: a palette-following (8-bit) CG image drawn with the palette being edited.
	struct PalPreview {
		int mode = 0;                        // 0 a layer of the active view's frame, 1 any CG image
		int layer = 0, image = 0;            // layer pick (mode 0) / image id (mode 1)
		int shownId = -1; unsigned long long idxGen = ~0ull;   // image whose indices are cached / CG generation they came from
		std::vector<uint8_t> idx; int w = 0, h = 0;            // 1 byte per pixel palette indices
		Thumb tex; uint64_t texKey = 0;      // RGBA upload, keyed on the colours + hover
		int scriptHover = -1;                // --ui-script "cgm set palHover": a grid cell held as hovered (screenshots)
		int hover = -1;                      // palette index under the mouse in the preview (last frame), outlined in the grid
	} pp;
	// Palette indices drawn by any 8-bit CG image (counted a few images per frame) and the PPPA tint slots of the .pat parts.
	struct IndexUse {
		const void *owner = nullptr; int images = -1, next = 0; bool done = false;
		bool used[256] = {}; bool tint[256] = {};
	} idxUse;
	void countIndexUse(CharacterInstance &ch);
	void drawPalPreview(CharacterInstance &ch, const WindowHost &host, const uint32_t *colors, int hoverIndex, float boxW, float boxH);
	void pushPalette(CharacterInstance &ch, int bank);   // show pal.set[bank] in the CG object (live preview everywhere)
	void commitPalette(CharacterInstance &ch, const std::string &label, int bank, PalSet before, const WindowHost &host);
	// every project that draws the bank on disk (MBAACC moons + b_ files, MBTL variants, ...)
	BankUsers users; std::string usersFor;
	std::vector<char> skipMark;            // per stack file of the loaded character: rewritten on disk with the bank (not baked into the own file)
	std::vector<uint32_t> diskUids;        // image identities of the .cg as on disk (RemapSince gives old -> current ids)
	ProjectUsage diskUsage; bool diskUsageValid = false; std::vector<int> diskUsageMap;   // diskUsageMap: current id -> disk id
	void scanProjects(CharacterInstance &ch);
	int projectCount(int currentId);       // layers drawing it in every project on disk, through the pending renumbering (-1 = not counted yet)
	void countProjectUsage();
	bool cutter2Note = false;
	// palette tab extras
	int importFrom = 0, palLayout = 0, ccChara = -1, ccFirst = 37, ccFrom = 0, ccTo = 0; bool ccFullPal = false; int newPups = 1;
	int resizeAnchor = 0;                  // replace with another size: 0 bottom-centre, 1 top-left
	FxPanel fxr;                    // "Effect recolor" tab (fxr_panel.cpp)
	// "CG + .pat textures" tab (cgm_texbrowser.cpp): CG images and the character's .pat textures side by side, one set of operations
	// for both through cgm_texsrc.h (view, replace, PNG export / import, references, save to the game or its mod folder)
	struct TexBrowser {
		int kind = 0, sel = -1;                       // kind: 0 CG image, 1 .pat texture
		char filter[48] = {};
		int viewKind = -1, viewId = -1; unsigned long long viewGen = ~0ull;
		Thumb view; float zoom = 1.f;
		int game = -1;                                // index into the browser's game list (-1 = detect)
		std::string gameDir[8];
		unsigned long long patGen = 0;                // bumped on every .pat texture replace
		std::string lastPng;
	} tb;
	void drawTextures(CharacterInstance &ch, const WindowHost &host);
	bool texReplace(CharacterInstance &ch, int kind, int id, const std::string &file, bool pngOnly, const WindowHost &host);
	bool texSave(CharacterInstance &ch, int kind, const std::string &game, std::string &report);
	// UNI2 / MBTL: where <srcPath> goes in <game folder>\<IM>\Mods ("" when the detected game has no mod folder or the file is not under data\)
	std::string modFolderDst(CharacterInstance &ch, const std::string &srcPath, bool *usesModFolder = nullptr);
	History hist;
	std::string status;
	std::vector<std::string> warnings;
	void rebuild(CharacterInstance &ch);
	// make the CG object / character show the edited model; records an undo step when `label` is not empty
	bool commit(CharacterInstance &ch, const std::string &label, Bank before, const WindowHost &host);
	bool applyModel(CharacterInstance &ch, const WindowHost &host);
	// structural edit: the model in `bank` already changed; rewrite the frame references through `remap` (empty = ids unchanged), reload the CG object, record one undo step
	bool commitStruct(CharacterInstance &ch, const std::string &label, Bank before, const std::vector<int> &remap, const WindowHost &host);
	int moveTarget = 0, insertAt = -1;
	struct Pending { int op = 0; int id = -1; bool open = false; } pend;   // op: 1 delete, 2 clear, 3 delete all unused
	void drawStructure(CharacterInstance &ch, const WindowHost &host);
	// PACNyx effect editing: rename, base point (sprite position on its canvas), block grid (click-toggle cells, all on / off, recalculate)
	struct BlockEdit { int id = -1; unsigned long long gen = ~0ull; std::vector<uint8_t> mask, px; int cols = 0, rows = 0, ox = 0, oy = 0, w = 0, h = 0; bool pending = false; bool show = true; } blk;
	char renameBuf[32] = {}; int renameFor = -1;
	void drawEffect(CharacterInstance &ch, const WindowHost &host);
	void drawBlockOverlay(const ImVec2 &p0, float zoom, int x1, int y1);
	void clickBlock(const ImVec2 &p0, float zoom, int x1, int y1);
	void undoRedo(CharacterInstance &ch, bool redo, const WindowHost &host);
	void clearThumbs();
	bool fetch(CharacterInstance &ch, int id, int pal, int pups, std::vector<uint8_t> &px, int &w, int &h);
	void upload(Thumb &t, const std::vector<uint8_t> &px, int w, int h, int maxSide, bool linear);
};

} // namespace cgm
#endif
