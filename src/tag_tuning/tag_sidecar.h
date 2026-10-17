#ifndef TAG_SIDECAR_H_GUARD
#define TAG_SIDECAR_H_GUARD
// [authoring] The TAG tuning sidecar tree: the single source of truth that Hantei-chan edits and pchost.dll hot-reloads.
// docs/HANTEI_AUTHORING_MODE.md §2 as changed by §8.1 / §8.4 and pinned down in §9.1:
//
//   <gamedir>\povertycaster\tag\            SHIPPED defaults     (Layer::Shipped)
//     global.ini  chars\<f>.ini  chars\<f>_crescent|_full|_half.ini  hud.ini
//   <gamedir>\povertycaster\tag\local\      the user's OVERLAY   (Layer::Local) — the only tree the editor writes,
//                                           except "Promote to defaults"
//
// Resolution (§9.1):
//   G      = Tuning{} <- style <- shipped [tuning] <- local [tuning]            (<- env, game side only)
//   S(f,m) = G <- shipped/local global [char.f] <- shipped/local chars\f.ini <- shipped/local chars\f_<moon>.ini
//              (<- CSS assist choices, game side only)
//
// Documents are byte-preserving TagIni editors (tag_ini.h). Saves are atomic (tmp + FlushFileBuffers + MoveFileExW),
// the previous bytes go to <f>.bak once per editor session, character files are written before global.ini, and a save
// that would add a warning is refused (the panel's NewWarnings rule, across every file).
#include "tag_ini.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tagtune {

enum class Layer : uint8_t { Shipped = 0, Local = 1 };
enum class DocKind : uint8_t { Global, CharShared, CharMoon, Hud };
constexpr int kMoonCount = 3;
inline const char* MoonName(int m) { return m == 0 ? "crescent" : m == 1 ? "full" : m == 2 ? "half" : "?"; }
inline const char* MoonShort(int m) { return m == 0 ? "C" : m == 1 ? "F" : m == 2 ? "H" : "?"; }

struct DocId {
	Layer layer = Layer::Local;
	DocKind kind = DocKind::Global;
	std::string file;   // CharShared / CharMoon: the lower-case data-file name ("shiki")
	int moon = -1;      // CharMoon: 0..2
	bool operator<(const DocId& o) const;
	bool operator==(const DocId& o) const { return layer == o.layer && kind == o.kind && file == o.file && moon == o.moon; }
	// "global.ini", "chars\\shiki.ini", "chars\\shiki_full.ini", "hud.ini" (relative to the layer's root)
	std::string RelPath() const;
	// "local global.ini", "shipped chars\\shiki_full.ini"
	std::string Label() const;
};
DocId GlobalDoc(Layer l);
DocId CharDoc(Layer l, const std::string& file, int moon = -1);   // moon -1 = the shared (all-moons) file
DocId HudDoc(Layer l);

// §2.1 name rule: [a-z0-9_], 1..27 characters.
bool ValidCharFileName(const std::string& file);
// "shiki_full.ini" -> file "shiki", moon 1; "shiki.ini" -> "shiki", -1. False for names that break §2.1 / not *.ini.
bool ParseCharFileName(const std::string& leaf, std::string& file, int& moon);

// ---- resolution over a set of documents (the workspace's, or the migrator's) ----
enum class Src : uint8_t { Default, Style, Tuning, Env, Char, Moon, Css };
const char* SrcName(Src s);   // "default", "style", "global", "env", "char", "moon", "CSS"
struct Provenance { Src src = Src::Default; Layer layer = Layer::Shipped; };

struct SidecarSet {
	const TagIni* global[2] = { nullptr, nullptr };                          // [Layer]
	std::map<std::string, const TagIni*> shared[2];                          // file -> chars\<f>.ini
	std::map<std::string, const TagIni*> moon[2][kMoonCount];                 // file -> chars\<f>_<moon>.ini
	bool Empty() const;
	std::vector<std::string> CharFiles() const;   // every file with a character document or a legacy [char.f]
};

struct GlobalResolution {
	Values values{};
	std::array<Provenance, kLeverCount> from{};
	std::string style;          // the active style actually used ("" = defaults)
	std::string styleRequested; // what active_style says
	bool styleFound = true;
	std::vector<Warning> warnings;   // "<doc label>: <message>"
};
struct SlotResolution {
	Values values{};
	std::array<Provenance, kLeverCount> from{};
	bool hasCharFile = false, hasMoonFile = false;
	std::vector<Warning> warnings;
};
GlobalResolution ResolveGlobal(const SidecarSet& s);
SlotResolution ResolveSlot(const SidecarSet& s, const GlobalResolution& g, const std::string& file, int moon);
// Every warning the game would log for this set: parse warnings of every document, the global resolution, and every
// character's resolution in every moon (each character file warns once).
std::vector<Warning> AllSidecarWarnings(const SidecarSet& s);
// Every style name offered: built-ins, then shipped / local ini styles.
std::vector<std::string> SidecarStyleNames(const SidecarSet& s);

// ---- the atomic writer (§2.8) ----
// Writes <path>.tmp, flushes, MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH); creates the directory. With bakOnce, the
// previous bytes (when the file exists) are copied to <path>.bak before the first write of this path in this process.
bool WriteFileAtomic(const std::string& path, const std::string& bytes, bool bakOnce, std::string* err);
bool ReadWholeFile(const std::string& path, std::string& out, bool* exists = nullptr);
void ResetBakMemory();   // tests: forget which paths already got their .bak

// ---- the workspace ----
struct SidecarDoc {
	DocId id;
	std::string path;          // absolute
	TagIni ini;
	bool onDisk = false;       // the file existed at the last load / save
	uint64_t diskTime = 0, diskSize = 0;
	bool externalChange = false;   // changed on disk while dirty: ask Load theirs / Keep mine
	std::string theirs;            // what is on disk now (when externalChange)
};

class SidecarWorkspace {
public:
	// root = <gamedir>\povertycaster\tag. Loads every document of both layers that exists. False if root is empty.
	bool Open(const std::string& root);
	void Close();
	bool IsOpen() const { return !m_root.empty(); }
	const std::string& Root() const { return m_root; }
	std::string LayerRoot(Layer l) const;   // root, or root\local
	bool Exists() const;                    // either tree holds global.ini or chars\*.ini (sidecar mode, §2.6)

	// The document (created empty, not on disk, if it does not exist yet).
	SidecarDoc& Doc(const DocId& id);
	const SidecarDoc* Find(const DocId& id) const;
	std::vector<const SidecarDoc*> Docs() const;
	std::vector<std::string> CharFiles() const;   // every character with a document in either layer

	// Resolution of the CURRENT texts (or the saved texts with saved = true).
	SidecarSet Set(bool saved = false) const;
	GlobalResolution Global(bool saved = false) const;
	SlotResolution Slot(const std::string& file, int moon, bool saved = false) const;

	bool AnyDirty() const;
	std::vector<DocId> DirtyDocs() const;

	struct SaveResult {
		bool ok = true;
		bool nothing = false;              // nothing was dirty
		std::string message;
		std::vector<std::string> written;  // relative labels
		std::vector<Warning> blocked;      // the new warnings that refused it
	};
	// Save every dirty document (character files first, global.ini last). Refused whole when any file would gain a
	// warning, or a dirty document has an unresolved external change.
	SaveResult SaveDirty();
	// Revert one document to its disk bytes.
	void Revert(const DocId& id);

	// External changes (the 1 s stat): a clean document reloads silently; a dirty one gets externalChange = true.
	// Returns the documents that changed on disk.
	std::vector<DocId> PollExternal();
	void ResolveExternal(const DocId& id, bool loadTheirs);

	// Every document's current bytes (checkpoints / A-B snapshots). Key = DocId.
	using Snapshot = std::map<DocId, std::string>;
	Snapshot Take() const;
	// Put a snapshot's bytes back into the documents (documents not in the snapshot become empty). Marks them dirty;
	// SaveDirty writes them. Returns the documents that changed.
	std::vector<DocId> Restore(const Snapshot& s);

	// Promote a key of a local document to the shipped document of the same name (§9.1): the shipped file gets the
	// value, the local file loses the key. For levers: sec = the section kind (Tuning / Char), lever = index.
	bool PromoteLever(const DocId& localDoc, int lever, std::string* err);
	// Promote every lever key of a local document.
	int PromoteAll(const DocId& localDoc);

private:
	std::string pathOf(const DocId& id) const;
	void loadDoc(const DocId& id);
	std::string m_root;
	std::map<DocId, std::unique_ptr<SidecarDoc>> m_docs;
};

// ---- lever edits on sidecar documents (character files ignore the section name) ----
void SetDocLever(TagIni& ini, DocKind kind, int lever, int32_t v);
void ClearDocLever(TagIni& ini, DocKind kind, int lever);
bool DocHasLever(const TagIni& ini, DocKind kind, int lever, int32_t* v = nullptr);

} // namespace tagtune

#endif
