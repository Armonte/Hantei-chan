#ifndef CGM_WINDOW_H_GUARD
#define CGM_WINDOW_H_GUARD
// Tools > CG manager: browse / search the whole sprite bank of the active character (thumbnail grid, usage, filters, preview).
#include "cgm_bank.h"
#include "cgm_usage.h"
#include "cgm_undo.h"
#include "cgm_palette.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CharacterInstance;

namespace cgm {

struct WindowHost {
	std::function<void(int pattern, int frame)> navigate;   // show a pattern / frame in the active view
	std::function<void(CharacterInstance *)> markEdited;    // the character changed (dirty flag + undo step)
};

class Window {
public:
	bool open = false;
	void draw(CharacterInstance *ch, const WindowHost &host);
	~Window();

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
	void pushPalette(CharacterInstance &ch, int bank);   // show pal.set[bank] in the CG object (live preview everywhere)
	void commitPalette(CharacterInstance &ch, const std::string &label, int bank, PalSet before, const WindowHost &host);
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
	void undoRedo(CharacterInstance &ch, bool redo, const WindowHost &host);
	void clearThumbs();
	bool fetch(CharacterInstance &ch, int id, int pal, int pups, std::vector<uint8_t> &px, int &w, int &h);
	void upload(Thumb &t, const std::vector<uint8_t> &px, int w, int h, int maxSide, bool linear);
};

} // namespace cgm
#endif
