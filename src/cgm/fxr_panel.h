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
private:
	struct Tex { unsigned id = 0; int w = 0, h = 0; };
	const CharacterInstance *owner = nullptr;
	unsigned long long gen = ~0ull;
	uint64_t usageVersion = ~0ull;
	FxClassification cls;
	fx::CharRules rules, saved;      // saved = the last loaded/saved state (diff report)
	std::string path, status, parserMsgs;
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
	void reset(CharacterInstance &ch, const Bank *bank, const UsageIndex &usage);
	std::string defaultPath(CharacterInstance &ch) const;
	const unsigned *slotPal(CharacterInstance &ch, int slot) const;
	Tex &preview(CharacterInstance &ch, const Bank &bank, int image, const fx::Rule *rule, int slot);
	void drawRuleEditor(CharacterInstance &ch, const Bank *bank);
	void drawStops(std::vector<fx::Stop> &stops, fx::Rgb accent, const char *idp);
	void drawSlotStrip(CharacterInstance &ch, const Bank &bank);
};

} // namespace cgm
#endif
