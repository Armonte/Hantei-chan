#ifndef PCMOD_MANIFEST_H_GUARD
#define PCMOD_MANIFEST_H_GUARD
// pcmod.json schema 1 (docs/modmanager/pcmod-manifest_schema1.md, PINNED 2026-10-14): the PovertyCaster MBAACC mod package
// manifest. Model, strict reader, deterministic writer, path rules (§3) and both digests (§5).
//
// Implemented from the spec document. PovertyCaster's pc-core pc/mods/Manifest.hpp was used only to cross-check results
// (tests/fixtures/pcmod pins the four digests its fixture produces). No filesystem and no Win32 here: the CLI, the GUI
// and the unit test all link this.
//
// Schema 2 (PovertyCaster docs/design/pcmod-manifest.md S2.1/S2.3, branch mods/pcmod-schema2): exe_patches/features/rules
// file refs and the pcmod-pkg-2/sim-2/set-2 digests. The writer never emits exe_layers/inject/ha6_patches/palettes.
#include "pcmod_sha256.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pcmod {

enum class PackageClass : uint8_t { Sim, Cosmetic };
const char* ClassName(PackageClass c);                 // "sim" / "cosmetic"
bool ParseClass(const std::string& s, PackageClass& out);

struct FileEntry {
	std::string path;     // as written in the manifest: '\' separators, the author's case
	std::string sha256;   // 64 lowercase hex
	uint64_t size = 0;
	std::string key() const;   // normalised '\' + ASCII lowercase (empty if the path is refused)
};
struct Requirement { std::string id, version; };
// Schema 2 (PC docs "Schema 2" S2.1): a package-relative data file (patches.toml / features.toml / rules.toml). Not in files[].
struct AuxFile {
	bool present = false;
	std::string path;     // bare lowercase "<id>.toml"
	std::string sha256;   // 64 lowercase hex of the file bytes
	uint64_t size = 0;
};
bool ValidAuxPath(const std::string& p);   // [a-z0-9][a-z0-9._-]{0,63}\.toml

struct Manifest {
	int schema = 1;
	std::string id, version, name, author, description, game = "mbaacc";
	PackageClass cls = PackageClass::Cosmetic;
	int priority = 0;
	std::vector<std::string> exe;
	std::vector<Requirement> requires_;
	std::vector<std::string> conflicts, overrides;
	// Session modes this package COUNTS for online (§2/§6, added 2026-10-15, still schema 1). Empty = every mode. NOT in any
	// digest: it only decides which per-mode sets the package joins.
	std::vector<std::string> modes;
	std::vector<FileEntry> files;
	// ---- schema 2 only ----
	AuxFile exePatches, features, rules;
	// pcmod_schema2.h Canonical() of the PARSED three files: enters the pcmod-sim-2 digest. Not serialised.
	std::string s2Canon;
	bool HasAux() const { return exePatches.present || features.present || rules.present; }
};

// ---- field rules (§2) ----
constexpr size_t kMaxModes = 8;
constexpr size_t kMaxName = 128, kMaxAuthor = 128, kMaxDescription = 2048, kMaxFiles = 4096, kMaxPath = 240;
constexpr uint64_t kMaxFileSize = 64ull << 20;
bool ValidId(const std::string& id);            // [a-z0-9][a-z0-9._-]{0,63}
bool ValidVersion(const std::string& v);        // MAJOR.MINOR.PATCH, digits, each 0..65535, no leading zeros
int CompareVersion(const std::string& a, const std::string& b);   // -1/0/1; both must be ValidVersion
bool IsLowerHex64(const std::string& s);

// ---- paths (§3) ----
// Normalises `raw` to '\' separators and ASCII lowercase into `out`. Returns false and a reason on refusal.
bool NormalisePath(const std::string& raw, std::string& out, std::string& why);
// Same, keeping the author's case (only the separators change). Used by the writer.
bool CanonicalPath(const std::string& raw, std::string& out, std::string& why);
// Every NORMALISED path has a class decided by the path alone (§3, revised 2026-10-14 to cover the whole MBAACC tree):
//   sim       data\** (except below), story\**, charaselect.txt
//   cosmetic  data\**.pal, data\_csel\**, bg\ bgm\ dat\ grp\ se\ shader\ movie\ palettes\, hud_theme.json, font.fnt, _sysfont.ini
//   refused   data\**.wmt (written by the game from story\battle\*_win.txt on every result screen, never read; IDA
//             ResultMenu_LoadWinQuoteData, PovertyCaster classifyPath 2026-10-15), and anything else (packdataversion\ (gates the update pack), system\, information\, other root files; sound\ is not a root)
enum class PathClass : uint8_t { Sim, Cosmetic, Refused };
PathClass ClassifyPath(const std::string& normPath);
const char* PathClassName(PathClass c);                // "sim" / "cosmetic" / "refused"
// A file the GAME writes and never reads (data\**.wmt): refused in a manifest; pack and the GUI export skip it with a note.
bool IsGameWrittenPath(const std::string& normPath);
// Why a normalised path is refused by ClassifyPath ("" when it is not refused).
std::string RefusedPathReason(const std::string& normPath);

// ---- validation: every rule of §2/§3 on a model (used by the writer before it writes, and after parsing) ----
// Returns the first reason, or "" when the manifest is valid.
std::string Validate(const Manifest& m);

// ---- JSON ----
// Strict schema-1 reader: unknown/duplicate/reserved keys, a wrong type or an out-of-range value refuse the package.
bool ParseManifest(const std::string& text, Manifest& out, std::string& error);
// Deterministic writer (2-space indent, keys in §2 order, UTF-8, '\n'). Optional keys with empty lists are still written
// (requires/conflicts/overrides: [] ) so a reader never has to guess, EXCEPT `modes`: written only when non-empty, so a
// manifest without modes stays byte-identical to one written before the key existed. Never writes a reserved key.
std::string WriteManifest(const Manifest& m);

// ---- digests (§5) ----
// Package digest: every file, plus the class. The identity of one package's content (catalogue key).
Digest PackageDigest(const Manifest& m);
std::string PackageDigestHex(const Manifest& m);
// Package sim digest: only the SIM-class files (ClassifyPath). This, not the package digest, enters the session digest, so
// two players may differ in a sim package's palettes or art. A cosmetic package's is the digest of the header alone.
Digest PackageSimDigest(const Manifest& m);
std::string PackageSimDigestHex(const Manifest& m);

struct SessionEntry {
	std::string id, version;
	int priority = 0;
	std::string simDigestHex;   // PackageSimDigestHex
	int schema = 1;             // any schema-2 entry switches the set to pcmod-set-2
};
// §6 per-mode set: does a package with these `modes` join the set of a session in `mode`? "" = no mode filter (always).
bool CountsForMode(const std::vector<std::string>& modes, const std::string& mode);
// Splits "tag,team" (commas and/or spaces) into ids, dropping empties and repeats. No validation (Validate does that).
std::vector<std::string> SplitModes(const std::string& text);
std::string JoinModes(const std::vector<std::string>& modes);   // "tag,team"

// Enabled, non-broken SIM packages only (callers drop cosmetic ones). Ordered priority desc, then id.
// pcmod-set-1, or pcmod-set-2 (+ "abi:<primitiveAbiId>\n") when any entry has schema >= 2.
std::string SessionDigestHex(const std::string& exeSha256Hex, std::vector<SessionEntry> pkgs, const std::string& primitiveAbiId = "");

} // namespace pcmod
#endif
