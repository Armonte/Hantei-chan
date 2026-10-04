#ifndef MV_SCRIPT_H_GUARD
#define MV_SCRIPT_H_GUARD

// Parser for MBTL Squirrel move scripts (chrXXX_mv_N.txt).
// MBTL moves spawn effect/projectile patterns from these scripts (CreateFireBall,
// BMvEff.CreateObject, ThrowParam, ...) instead of ha6 EF effect data, so the
// spawn visualization misses them. This module does a best-effort line-level
// scan (no full Squirrel parse) to extract those spawns and associate them with
// the ha6 pattern that owns the move block.
//
// Association rules implemented:
//  - A move block "t.Mv_Skill_236B <- {...}" owns the ha6 pattern whose PTCN
//    codeName matches the block name after stripping up to two leading
//    underscore segments ("Mv_", category prefix) — longest suffix wins.
//  - "mvs.DataPattern == N" literals inside a block add pattern N as an owner.
//  - Factory templates: 'local f = function(param) {...}' blocks instantiated
//    via 't.Mv_X <- f({ type="B" })' copy the factory's spawns; spawns found
//    under 'case "B":' labels are only copied when "B" appears in the call args.
//  - Note: _cmd_ tables only map commands to move names (UpdateTable={name=..})
//    and _com_ files are COM AI recipes — neither contains pattern bindings in
//    any shipped character, so they are not parsed. SetPattern="CODE" is always
//    a transition target (button-release pattern jump), never an owner binding.
//
// Spawn targets extracted:
//  - pat="CODE" / start_pat="CODE" table slots (Battle_Std.CreateFireBall etc.)
//  - start_pat=pat_num_VAR where 'local pat_num_VAR = ...GetPatternNum({pat="CODE"})'
//  - start_pat=mvParam.field where the field was assigned a pat_num_ variable
//  - BMvEff.CreateObject({ mvname="Mv_X" }) — resolved by move-name suffix
//  - BMvEff.ThrowParam({ pattern=N }) — numeric pattern id
//  - Battle_Std.SetImpactHitEffect({frameid=..}) — marker entry (isImpactEffect)
//
// Spawn timing: each spawn records the frame-ID gate it fires on (frameIdRef,
// matching ha6 AFID): 'case N:' labels under switch(GetUpdateFrameID()),
// 'GetUpdateFrameID() == N' comparisons, frameid=/FrameID=/CheckFrameID=
// literals, or — for spawns declared in an mvParam field and fired later via
// CreateFireBall(mvParam.field)/CreateObject(mvParam.field) — the gate at the
// use site.

#include <string>
#include <vector>
#include <map>
#include <functional>

class FrameData;

struct MvScriptSpawn {
	std::string patternCode;   // Target pattern code name (PTCN), may be empty
	int patternId = -1;        // Resolved ha6 pattern id (-1 = unresolved)
	int offsetX = 0, offsetY = 0;
	std::string source;        // Human readable origin (file:line + snippet), UTF-8
	std::string mvName;        // Move name of the spawned object, if given (mv=/mvname=)
	std::string ownerMove;     // Move block this spawn was found in
	int frameIdRef = -1;       // AFID frame gate the spawn fires on (-1 = move start)
	bool isImpactEffect = false; // SetImpactHitEffect marker (no target pattern)
};

class MvScriptIndex {
public:
	// Finds and parses sibling chrXXX_mv_*.txt files next to the character txt
	// (e.g. ".../chr019_0.txt" -> ".../chr019_mv_0.txt"). Returns true if at
	// least one script file was parsed.
	bool loadForCharacter(const std::string& txtPath);

	// Parse a single mv script file (Shift-JIS). Never throws; returns false
	// on missing/oversized file.
	bool loadScriptFile(const std::string& path);

	// Resolve pattern code names and owner move names against the loaded
	// frame data (Sequence::codeName / PTCN). Rebuilds the per-pattern map.
	void resolveCodeNames(FrameData* fd);
	// Core resolver: lookup returns a pattern id for a code name, or -1.
	void resolveCodeNames(const std::function<int(const std::string&)>& lookup);

	// Spawns owned by the given ha6 pattern (includes unresolved entries with
	// patternId == -1 so the UI can list them). Null if none.
	const std::vector<MvScriptSpawn>* spawnsForPattern(int patternId) const;

	// Spawns declared by the template block of a spawned object (e.g. the
	// "Mv_FireBall_236B" block referenced by a spawn's mv=/mvname=). Used to
	// chain grandchildren under the spawned object instead of flattening them
	// onto the parent move. Null if the move name owns no spawns.
	const std::vector<MvScriptSpawn>* spawnsForTemplate(const std::string& mvName) const;

	bool loaded() const { return m_loaded; }
	const std::vector<MvScriptSpawn>& allSpawns() const { return m_spawns; }
	const std::vector<std::string>& loadedFiles() const { return m_files; }

	void clear();

	// Global registry so code that only has a FrameData* (framestate.cpp,
	// box_pane.cpp) can find the owning character's script index.
	static void Register(const FrameData* fd, const MvScriptIndex* index);
	static void Unregister(const FrameData* fd);
	static const MvScriptIndex* Lookup(const FrameData* fd);

private:
	bool m_loaded = false;
	std::vector<std::string> m_files;
	std::vector<MvScriptSpawn> m_spawns;                  // raw extracted spawns
	std::map<std::string, std::vector<int>> m_ownerExtra; // move -> DataPattern ids
	std::map<int, std::vector<MvScriptSpawn>> m_byPattern; // built by resolveCodeNames
	// template block name (referenced by some spawn's mv=/mvname=) -> its spawns
	std::map<std::string, std::vector<MvScriptSpawn>> m_byTemplate;
};

#endif /* MV_SCRIPT_H_GUARD */
