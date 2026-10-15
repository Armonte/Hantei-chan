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

#include <cstddef>
#include <cstdint>
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

// ActCadenza reference version (decision D1, docs/actcadenza_parity/SPEC.md section 0). 1.03A is the
// English v1.1 patch: its 06.p overrides 12 <CHR>_C.CT files (guts), VECTOR.TXT and G_CHAOS.DAT.
enum class AcVersion { V100, V103A };
bool ParseAcVersion(const std::string &s, AcVersion &out);   // "1.00" / "1.03A" (case-insensitive)
const char *AcVersionName(AcVersion v);

// The status block of a compiled <CHR>_C.CT (what ActCadenza actually reads; it never opens _C.TXT).
// mbacPC Slot_LoadCommandCT (1.00 0x435B40, 1.03A 0x4362C0) copies CT+0x2264..0x22D4 to slot+0x304.
// Damage_ComputeProrated (1.00 0x451660, 1.03A Damage_Calculate 0x452C00) reads the guts float for HP
// quarter q (0 = HP >= 3/4 .. 3 = HP < 1/4) at slot+0x310+4q = CT+0x2270+4q, i.e. the _C.TXT status
// floats f1..f4. f0 (CT+0x226C) is never read. 0.0 means 1.0.
struct CtStatus {
	static constexpr size_t kSize = 8916;
	static constexpr size_t kStatusOff = 0x2264, kFloatOff = 0x226C, kTeamOff = 0x2280;
	bool ok = false;
	uint8_t head[8] = {};        // CT+0x2264: [0] air jumps, [1] combo max, [5] status bits (_C.TXT "00111001")
	float floats[5] = {};        // CT+0x226C: _C.TXT status f0..f4
	int team[2] = {};            // CT+0x2280: [TeamChangeData]
	int airJumps() const { return head[0]; }
	unsigned bits() const { return head[5]; }
	float guts(int q) const { return floats[q + 1]; }
};
bool ParseCtStatus(const uint8_t *d, size_t n, CtStatus &out, std::string *err = nullptr);
bool ReadCtStatus(const std::string &path, CtStatus &out, std::string *err = nullptr);

// MBAACC-syntax command file from an ActCadenza _C.TXT (commands verbatim, positional trailer -> INI
// sections, as gamedata/tools/scripts/mbac_moon_cmd_audit.py emit) with [Status] taken from the CT:
// AirJumpNum, Guard = the 4 guts floats f1..f4, Flags from status bit 7. `ct` may be null (TXT only,
// then Guard = TXT f1..f4). Mismatches between the TXT trailer and the CT are logged.
bool EmitCommandFile(const std::string &txtPath, const CtStatus *ct, const std::string &ctLabel,
                     const std::string &outPath, std::vector<std::string> *log);

// The MBAACC common EX-shield window for moon 3 ([Type_3], decision S8): stand / crouch / air.
constexpr int kExShieldCommon[3] = {6, 6, 8};

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

	// --- ActCadenza parity data (S8b) ---
	AcVersion acVersion = AcVersion::V103A;
	std::string acPatchDir;    // 1.03A: folder with the extracted 06.p (<CHR>_C.CT overrides). A CT missing
	                           // there is unchanged in 1.03A and is read from next to the .DAT.
	std::string acBaseDir;     // the 1.00 data folder (02.p) for _C.TXT / _C.CT / .PAL when the .DAT itself
	                           // comes from the 06.p overlay (G_CHAOS.DAT in 1.03A) and has no siblings
	bool writeCmd = true;      // <base>_cc_c.txt from <STEM>_C.TXT + the CT of acVersion
	// Spawn leaks: an EF1 whose target pattern is empty in the ActCadenza data (spawns nothing there) but
	// exists in the MBAACC stack the moon-3 set is layered on. Needs mbaaccDataDir (<ccName>.HA6, _r, _0, _0_r).
	std::string mbaaccDataDir;
	bool leakNoop = true;      // retarget them to a reserved no-op pattern
	int noopPattern = 990;     // first choice; the next lower slot empty in both is used if taken
	// Giants: MBAACC's own giant reaction layout. GiantFlag 2 (G_AKIHA #16): KO 44/45 forced by the
	// engine on a KO hit, otherwise the vector table's HitAni (900-908 / 351 / 354) plus the old 23-30 set.
	// GiantFlag 1 (HERMES #32, G_CHAOS #27): only 44/45 (MBAA Victim_ApplyPendingHits 0x470FC0 @0x471F09).
	int giantFlag = 0;
	std::string giantDonorHa6; // MBAACC giant HA6 the missing reaction slots are copied from (G_AKIHA / Hermes)
	bool writeShieldSidecar = true;   // <base>_actcadenza_moon.ini when the EX window differs from kExShieldCommon
	std::string sidecarSection;       // MBAACC File1 for the sidecar section (default upper-case ccName/baseName)
};

struct Report {
	std::vector<std::string> lines;
	std::string ha6Path, cgPath, patPath, palPath, txtPath, cmdPath, sidecarPath;
	int exShield[3] = {-1, -1, -1};    // ActCadenza EX window stand / crouch / air (-1 = no shield data)
	std::vector<std::pair<int, int>> leaks;   // (source pattern, leaked EF1 target) retargeted to noopUsed
	int noopUsed = -1;
	std::vector<int> giantAdded;       // reaction slots added from the giant donor
	std::string gutsSource;            // "1.03A CT <path>" etc.
	float guts[4] = {};
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
                   const FrameData *cc = nullptr, int *exShieldOut = nullptr);

// Spawn leaks (see Options::leakNoop): retargets every EF1 whose target is empty in `fd` but has frames in
// `mbaacc` to one reserved 1-frame invisible pattern (aniType 0, as MBAACC's own empty effects 216/217).
// Returns the no-op slot used (-1 when there was no leak).
int RewriteSpawnLeaks(FrameData &fd, const FrameData &mbaacc, int preferredNoop,
                      std::vector<std::pair<int, int>> *leaks, std::vector<std::string> *log);

// Giant reaction slots from an MBAACC giant (Options::giantFlag). Copies each donor slot the data lacks,
// keeping the donor's timing / flags / AS / IF, with the sprite layers and hurt boxes of the character's
// own hit pose (pattern 24, else 23, else 0); EF spawns that do not resolve in `fd` are dropped. Then,
// for GiantFlag 2, the 900-908 / 351 / 354 copies from the (now complete) 23-26. Returns the slots added.
std::vector<int> AddGiantReactions(FrameData &fd, const FrameData &donor, int giantFlag, std::vector<std::string> *log);

// Convert already-loaded HA4 data (fd.m_ha4 must be set).
bool Convert(const FrameData &fd, const std::string &datPath, const Options &opt, Report &rep);

// Load + convert a .DAT file.
bool ConvertFile(const std::string &datPath, const Options &opt, Report &rep);

} // namespace ha4conv

#endif
