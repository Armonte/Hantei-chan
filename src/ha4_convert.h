#ifndef HA4_CONVERT_H_GUARD
#define HA4_CONVERT_H_GUARD

// MBAC (Hantei4 .DAT) -> MBAACC-style HA6 character export.
// Output for <name>: <name>.ha6, <name>.cg (the embedded CG, same BMP Cutter3
// format MBAACC uses), <name>.pat (embedded parts converted to PAniDataFile,
// only if the .DAT has parts), <name>.pal (copied from the MBAC .PAL next to
// the .DAT, same 64 x 256 format) and <name>.txt (Hantei-chan/MBAACC character
// list: [DataFile] / [BmpcutFile] / [PAniFile]).
// Conversion rules: Hantei_Docs/HANTEI_MBAC_SUPPORT.md section 4; behaviour
// audit (M1..M12, IDA evidence): docs/mbac_moon/CONVERTER_FIDELITY.md.

#include <string>
#include <vector>

class FrameData;

namespace ha4conv {

// AT +0x4E (MBAC "stun", never read by mbacPC Character_ApplyPendingHit 0x4548B0)
// lands in ATVV[2], which MBAA reads as guard-gauge damage (Hit_BuildHitResultParams
// 0x4706FF -> GuardGauge_ApplyBlockDamage). Fidelity M3.
enum class GuardDamage {
	Zero,      // 0: MBAC attacks do no guard-gauge damage (what mbacPC does)
	FromCC,    // take ATVV[2] from the same CC pattern/frame (ccDataDir), else 0
	Raw,       // the old verbatim copy of +0x4E (not behaviour-faithful)
};

struct Options {
	std::string outDir;        // output folder (created if missing)
	std::string baseName;      // defaults to the lower-case .DAT stem
	std::string palPath;       // defaults to <stem>.PAL next to the .DAT (any case)
	bool writeCG = true;
	bool writePat = true;
	bool writePal = true;
	bool writeTxt = true;

	// --- behaviour fixes (CONVERTER_FIDELITY.md) ---
	// M7: add CC's victim reaction patterns 900-908 / 351 / 354 (global vector.txt
	// HitAni targets MBAC data lacks) as copies of the MBAC reactions they replace.
	// Turn off when the result is layered over CC's own crescent files, which carry them.
	bool systemPatterns = true;
	// M8: rewrite IF 51 (shield) records to MBAA's meaning.
	bool shieldRemap = true;
	// M3 / F4.
	GuardDamage guardDamage = GuardDamage::Zero;
	std::string ccDataDir;     // GuardDamage::FromCC: folder with <ccName>.HA6 (+_r, _0, _0_r)
	std::string ccName;        // defaults to baseName
	// M12: clear EF 1/8/11 p2 bits >= 0x100 (mbacPC reads only the low byte; MBAA
	// gives 0x100/0x200/0x400/0x800 meanings).
	bool efClearCcOnlyBits = true;
};

struct Report {
	std::vector<std::string> lines;
	std::string ha6Path, cgPath, patPath, palPath, txtPath;
};

// The victim reaction patterns CC's global data\vector.txt sends a hit character
// to that MBAC (256 slots, MBAC VECTOR.TXT) never had, and the MBAC pattern each
// one is copied from. MBAC's VECTOR.TXT used 23/24/25 for every strength.
struct SystemPatternRule { int target, source; const char *why; };
const std::vector<SystemPatternRule> &SystemPatternRules();

// Apply the HA4 -> HA6 rules that are not already implied by the model
// mapping (in place; call it on a copy of the loaded data). `log` gets one
// line per applied fix (counts), may be null. `cc` is the CC character's
// merged data for GuardDamage::FromCC (may be null).
void PrepareForHA6(FrameData &fd, const Options &opt = Options(), std::vector<std::string> *log = nullptr,
                   const FrameData *cc = nullptr);

// Convert already-loaded HA4 data (fd.m_ha4 must be set).
bool Convert(const FrameData &fd, const std::string &datPath, const Options &opt, Report &rep);

// Load + convert a .DAT file.
bool ConvertFile(const std::string &datPath, const Options &opt, Report &rep);

} // namespace ha4conv

#endif
