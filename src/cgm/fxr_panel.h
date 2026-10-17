#ifndef CGM_FXR_PANEL_H_GUARD
#define CGM_FXR_PANEL_H_GUARD
// Tools > CG manager > "Effect recolor": authoring + live preview of the PovertyCaster runtime recolour config (<game>\fxrecolor\<char>.ini).
// Classification grid, per-pattern rule assignment, ramp / HSV / rainbow editor, per-slot accent overrides, DGV starter ruleset.
#include "fxr_core.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class CharacterInstance;

namespace cgm {

class FxPanel {
public:
	// Draws the tab body. bank may be null (foreign banks: nothing to classify). navigate(pattern, frame) shows a pattern in the main view.
	void draw(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage, const std::function<void(int, int)> &navigate);
	~FxPanel();
	// ui-script (`cgm fxr <sub> [arg]`, see Window::script): import dgv|dgv-accent, rule <id|index>, pattern <id|first>, slot <n>,
	// accent <index|auto|#rrggbb>, path <file>, load, save [file], status. The tab must have been drawn once (it binds the character).
	bool script(const std::string &arg, std::string *msg);
	bool selfTestPending() const;    // --fxr-selftest given and not yet run (the CG manager then draws the tab even without a CG bank)
private:
	struct Tex { unsigned id = 0; int w = 0, h = 0; };
	const CharacterInstance *owner = nullptr;
	unsigned long long gen = ~0ull;
	uint64_t usageVersion = ~0ull;
	FxClassification cls;
	fx::CharRules rules, saved;      // saved = the last loaded/saved state (diff report)
	std::string path, status, parserMsgs;
	bool pathAuto = true;            // path is the derived default (follows the character); false once typed / loaded / saved elsewhere
	std::string charStem;            // lowercase roster stem of the bound character (<char>.ini)
	int nSlots = 0;
	std::vector<uint32_t> bodyHist; std::vector<AccentCandidate> accentCands;   // body index usage / suggested accent indices (lazy)
	// view state
	char nameFilter[48] = {};
	int minScore = 0; bool onlyUnassigned = false, onlyShared = false, onlyUnruled = false;
	int sel = -1;                    // clicked pattern id
	std::vector<int> picked;         // ticked pattern ids
	int ruleSel = -1, spriteSel = -1, slotSel = 0, ovSlot = -1, rampSlotEdit = -1;
	int assignRule = 0;
	bool showBefore = true;
	// preview cache
	std::unordered_map<std::string, Tex> tex;
	void freeTex();
	bool loadFile(); bool saveFile(bool askFolder = false); void newRuleFromTicked(); void selfTest(const std::string &dir);
	void importDgv(bool accentFollow);
	void reset(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage);
	std::string defaultPath(const CharacterInstance &ch);
	static std::string mbaaccGameDir(const CharacterInstance &ch);   // "" = unknown (Save then asks with a folder picker)
	const unsigned *slotPal(CharacterInstance &ch, int slot) const;
	Tex &preview(CharacterInstance &ch, const Bank &bank, int image, const fx::Rule *rule, int slot);
	void drawRuleEditor(CharacterInstance &ch, const Bank *bank);
	void drawStops(std::vector<fx::Stop> &stops, fx::Rgb accent, const char *idp);
	void drawSlotStrip(CharacterInstance &ch, const Bank &bank);
};

} // namespace cgm
#endif
