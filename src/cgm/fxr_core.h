#ifndef CGM_FXR_CORE_H_GUARD
#define CGM_FXR_CORE_H_GUARD
// Effect recolour authoring core (no GL / ImGui): classification of effect patterns/sprites, the per-pattern rule model, and the lossless
// preview that applies the PovertyCaster runtime's own shader maths (fxr_spec.hpp, vendored byte-identical from the runtime) to the
// ORIGINAL pixels of a sprite. The config format is the runtime's <game>\fxrecolor\<char>.ini (docs/cg/fxrecolor_config_spec.md).
// Credit: the manual recolour pipeline this automates is DGV's "Better Akiha"; its Akiha groups ship as the starter ruleset.
#include "cgm_bank.h"
#include "cgm_usage.h"
#include "fxr_spec.hpp"
#include <string>
#include <vector>

class FrameData;

namespace cgm {

namespace fx = mbaacc::fxr;

// One pattern that draws at least one fixed-colour (type != 0) sprite.
struct FxPatternRow {
	int pattern = -1;
	std::string name;
	int layers = 0, fixedLayers = 0;       // layer references (frames x layers) in total / drawing a type != 0 image
	std::vector<int> fixedSprites;          // distinct type != 0 images it draws (ascending)
	int followSprites = 0;                  // distinct type 0 images it draws (these follow the palette natively)
	int sharedSprites = 0;                  // of fixedSprites: also drawn by another pattern (per-pattern rules matter here)
	unsigned blendMask = 0;                 // bit b set = a fixed layer uses blend preset b (0 = unset, treated as normal)
	int effectScore = 0;                    // 0..100: share of its layers that are fixed-colour (a body pattern with one RGB hair scores low)
};

struct FxClassification {
	std::vector<int> imageType;             // per CG image id (-2 absent)
	std::vector<int> patternsUsing;         // per image: distinct patterns drawing it
	std::vector<FxPatternRow> patterns;     // only patterns with fixedLayers > 0, ascending id
	int fixedImages = 0, usedFixedImages = 0, sharedFixedImages = 0;   // images type != 0 / drawn by >= 1 pattern / by >= 2 patterns
	const FxPatternRow *find(int pattern) const;
};

void ClassifyEffects(const Bank &bank, const UsageIndex &usage, FrameData &fd, FxClassification &out);

// Palette hand-over to autoAccent(): both sides use the engine layout (0xAABBGGRR = memory R,G,B,A); kept as a function so the contract lives in one place.
void ToRuntimePalette(const uint32_t *rgbaMem, uint32_t out[256]);
fx::Rgb AccentFor(const fx::Rule &r, const uint32_t *slotPalMem, int slot);   // override if present, else auto from the palette

// Auto-suggest the accent: which palette indices of the BODY (type 0 sprites, the only entries the character's colours live in) are vivid in the
// slots. The runtime's own auto accent scans all 255 entries, which also sees the unused entries (often pure green); naming the index in the rule
// (`accent_idx = N`) makes every slot's accent "the colour that slot gives the outfit's accent index".
void BodyIndexHistogram(const Bank &bank, std::vector<uint32_t> &counts256);
struct AccentCandidate { int index = 0; float score = 0; };
std::vector<AccentCandidate> SuggestAccentIndices(const std::vector<uint32_t> &hist, const std::vector<const uint32_t *> &slotPalsMem, int maxN = 5);

// The rule the runtime would pick for `pattern` drawing `sprite` in `slot` (first match wins), null = untouched.
const fx::Rule *RuleFor(const fx::CharRules &c, int pattern, int sprite, int slot, int blend = 1);
// The rule that colours most of a pattern's fixed sprites, with how many of them it covers (for the grid column).
const fx::Rule *RuleOfPattern(const fx::CharRules &c, const FxPatternRow &row, int slot, int *covered = nullptr);

// Lossless preview: the sprite's original pixels with the rule applied for owner `slot` (palette = slotPalMem, 256 x 0xAABBGGRR).
// Type 0 images are never recoloured (they follow the palette: drawn with slotPalMem). rule == null leaves the pixels untouched.
bool RecolorImage(const Bank &bank, int image, const fx::Rule *rule, int slot, const uint32_t *slotPalMem, Rgba &out);

// Rule-set editing helpers (all keep the file order the runtime matches in).
int FindRuleIndex(const fx::CharRules &c, const std::string &id);
std::string UniqueRuleId(const fx::CharRules &c, const std::string &base);
// Moves `patterns` into rule `ruleIdx`: removed from the explicit pattern lists of every other rule, added to this one.
void AssignPatterns(fx::CharRules &c, int ruleIdx, const std::vector<int> &patterns);
fx::Rule DefaultRule(const std::string &id);   // lumramp by Oklab lightness (the runtime default), black accent.dark accent accent.light white

// Rule-set <-> text through the runtime's own parser/writer. load returns the number of rules kept; `err` collects parser messages.
int LoadRules(const std::string &text, fx::CharRules &out, std::string &err);
std::string SaveRules(const fx::CharRules &c);

// Diff report between two rule sets (what changed vs the shipped/loaded one), plain text for the UI and CLI.
std::string DiffRules(const fx::CharRules &before, const fx::CharRules &after);

// DGV's Akiha groups as a ruleset (embedded text, fxr_dgv_akiha.inc).
const char *DgvAkihaIni();

} // namespace cgm
#endif
