#ifndef PCMOD_SCHEMA2_H_GUARD
#define PCMOD_SCHEMA2_H_GUARD
// pcmod schema 2 data files: patches.toml / features.toml / rules.toml (PovertyCaster docs/design/pcmod-manifest.md
// "Schema 2", S2.1-S2.8, branch mods/pcmod-schema2). Pure: no filesystem, no Win32, no ImGui.
//
// The strict TOML reader, the typed parse of the three files and the canonical text MIRROR PovertyCaster's
// pc-core/include/pc/mods/Schema2.hpp (commit 1adbd3408) refusal-for-refusal, because what Hantei-chan exports must be
// exactly what PovertyCaster accepts, and the canonical text enters the pcmod-sim-2 digest bit for bit.
// Hantei-chan adds: a deterministic WRITER for the three files, a minimal PE32 reader, and author-time verification of the
// patches against a vanilla MBAA.exe image (orig bytes, section range, call/jmp target, overlaps), with named issues.
#include <cstdint>
#include <string>
#include <vector>

namespace pcmod::s2 {

// ---- the strict TOML subset (S2.2) ----
struct TVal { enum class K : uint8_t { Int, Str, Bool } k = K::Int; long long i = 0; std::string s; bool b = false; };
struct TKv { std::string key; TVal v; int line = 0; };
struct TTable { std::string name; bool array = false; int line = 0; std::vector<TKv> kv; };   // name "" = root table
struct TDoc { std::vector<TTable> tables; };
// "" on success, else "line N: reason".
std::string ParseToml(const std::string& text, TDoc& out);

// ---- typed files ----
enum class Kind : uint8_t { Raw, Call, Jmp };
struct Patch {
	uint32_t addr = 0;
	std::vector<uint8_t> orig;
	Kind kind = Kind::Raw;
	std::vector<uint8_t> raw;     // Raw: replacement bytes (same length as orig)
	bool toVanilla = false;       // Call/Jmp: vanilla:<target> when true, pc:<primitive> otherwise
	uint32_t target = 0;
	std::string primitive;
	int line = 0;
};
struct FeatureParam { std::string key; long long v = 0; bool isBool = false; };
struct Feature { std::string id; int abi = 0; std::vector<FeatureParam> params; };
struct Rule { std::string name; long long v = 0; bool isBool = false; };   // isBool only affects the writer (canon is 0/1)

struct Data {
	std::vector<Patch> patches;
	std::vector<Feature> features;
	std::vector<Rule> rules;
	bool any() const { return !patches.empty() || !features.empty() || !rules.empty(); }
};

// ---- the per-game profile ----
struct ReservedRange { uint32_t addr = 0, len = 0; std::string name; };
// A .data table a patch may write (S2.8 "allow-listed load-time table"). Patch must lie fully inside [begin, end).
struct DataTable { std::string id; uint32_t begin = 0, end = 0; };
struct ParamDef { std::string name; long long min = 0, max = 0; };
struct FeatureDef { std::string id; int abi = 0; std::vector<ParamDef> params; };
struct RuleDef { std::string name; long long min = 0, max = 0; };
struct Profile {
	std::string abiId;                        // primitive ABI id ("mbaacc-prim-1"); enters pcmod-set-2
	uint32_t imageBegin = 0, imageEnd = 0;    // patchable range [begin, end)
	uint32_t textBegin = 0, textEnd = 0;      // vanilla code for call/jmp targets [begin, end)
	std::vector<DataTable> dataTables;        // .data allow-list (outside imageBegin..imageEnd)
	std::vector<ReservedRange> reserved;      // PovertyCaster-owned sites (advisory copy; PC's table is authoritative)
	std::vector<FeatureDef> features;
	std::vector<RuleDef> rules;
	std::vector<std::string> callPrimitives;  // names usable as pc:<name>
};
// MBAACC 1.07 profile as PovertyCaster mods/pcmod-schema2 (a584ab631/49bcaa321) defines it, plus the css_roster table.
const Profile& MbaaccProfile();
constexpr const char* kMbaaccExeSha256 = "6d1415ca";   // prefix of the vanilla MBAA.exe 1.07 sha256 (display / sanity only)

// ---- byte strings ----
bool ParseHexBytes(const std::string& s, std::vector<uint8_t>& out);   // "8B 45 08" == "8b4508"
std::string HexBytes(const std::vector<uint8_t>& b);                   // canonical: "8b4508"
std::string HexBytesSpaced(const std::vector<uint8_t>& b);             // writer: "8B 45 08"
std::string H8(uint32_t v);                                            // "%08x"

// ---- parse (mirrors PC; "" = ok, else "<file> line N: reason" / "<file>: reason") ----
std::string ParsePatches(const std::string& text, const Profile& pf, Data& out);
std::string ParseFeatures(const std::string& text, const Profile& pf, Data& out);
std::string ParseRules(const std::string& text, const Profile& pf, Data& out);

// ---- canonical text (S2.3), absent section = header only. `d` must be sorted (the parsers sort; Normalise sorts). ----
std::string Canonical(const Data& d);
void Normalise(Data& d);   // sort patches by addr, features by id (+ params by key), rules by name

// ---- deterministic writers (strict subset; reparse(write(d)) has the same Canonical) ----
std::string WritePatchesToml(const Data& d);
std::string WriteFeaturesToml(const Data& d);
std::string WriteRulesToml(const Data& d);

// Final bytes a patch writes (call/jmp to vanilla: E8/E9 rel32 + 0x90 fill). false for pc:<primitive> (resolved by PC).
bool PatchBytes(const Patch& p, std::vector<uint8_t>& out);

// ---- PE32 image (vanilla exe) ----
struct PeSection { std::string name; uint32_t va = 0, vsize = 0, rawOff = 0, rawSize = 0, chars = 0; };
struct PeImage {
	std::vector<uint8_t> bytes;
	uint32_t imageBase = 0, sizeOfHeaders = 0;
	std::vector<PeSection> sections;
	const PeSection* SectionAt(uint32_t va) const;   // by virtual address
	// Copies `len` bytes at virtual address `va` (zero-fill past raw data inside vsize). false if not mapped.
	bool Read(uint32_t va, size_t len, std::vector<uint8_t>& out) const;
};
bool ParsePe(std::vector<uint8_t> bytes, PeImage& out, std::string& error);

// ---- author-time verification against the vanilla exe ----
enum class IssueCode : uint8_t { OrigMismatch, OutOfRange, PeHeader, Resources, CallTooShort, CallTargetNotCode, Overlap, Reserved, LengthMismatch, BadLength };
const char* IssueName(IssueCode c);   // "orig-mismatch", "out-of-range", ...
struct Issue { IssueCode code; uint32_t addr = 0; std::string message; };
// Every patch is checked (not first-fail) so the author sees all of them. Empty = ok.
std::vector<Issue> VerifyPatches(const Data& d, const PeImage& exe, const Profile& pf);

} // namespace pcmod::s2

namespace pcmod { struct Manifest; }
namespace pcmod::s2 {
// ---- package level (pcmod.json schema 2 <-> the three files) ----
struct Files { std::string patches, features, rules; };   // file TEXTS ("" = absent)
// Export: writes the present sections of `d` as TOML (patches only if d.patches non-empty, etc.), RE-PARSES them with the
// PC-mirrored parser (so PovertyCaster accepts exactly what we write), and sets m.schema = 2, the aux refs
// (patches.toml / features.toml / rules.toml with sha256 + size of the written bytes) and m.s2Canon. "" = ok.
std::string AttachToManifest(Manifest& m, const Data& d, const Profile& pf, Files& out);
// Load an existing schema-2 package's texts (PC loadSchema2): checks each present file's size/sha256 against the manifest,
// parses it, sets m.s2Canon. "" = ok, else the refusal sentence.
std::string LoadFromTexts(Manifest& m, const Files& texts, const Profile& pf, Data& out);
} // namespace pcmod::s2
#endif
