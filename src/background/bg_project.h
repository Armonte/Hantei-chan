// Stage "project": a game's stage list and the metadata files around the
// bgmake stages, for the stage browser and the metadata editor.
//
// MBAACC (docs/bg_research/STAGE_AUDIT.md, "Metadata"):
//   bg/BgList.ini   [Bg_NNN] DataFile / InfoFile / IsSelectAble / IsGiantStage /
//                   StageColorVal (StageSelect_LoadStageData 0x4b4a80). The id is
//                   the section number; select order = id order.
//   Bgm/bgm.txt     [BGM_NNN] File / IsLoop / LoopPos; stage NN plays BGM NN
//                   (battle BGM id byte 0x76E653 = stage id).
//   GRP/BgSelect/   stsel_view/chr_stsel_viewNN.dds (256x96 preview),
//                   stsel_en/chr_stsel_enNN.dds, stsel_jp/... (256x32 names).
//   bg/bgNNInfo.txt lights and weather (edited through TextIni as well).
//   Hardcoded in MBAA.exe (shown, not editable): exclusion lists (training
//   {55,57,58,99}, other modes {3,4,18,...}), boss stage 18, arcade table.
// MBAC: the stage list is a table in mbacPC.exe (g_StageTable 0x491050),
//   stage files bg\BGnn.DAT, bgm.txt [BGM_nnn].
//
// All text files are edited through TextIni, which keeps every byte it does
// not change (Shift-JIS comments, spacing, CRLF), so an unedited save is
// byte-identical and an edit touches only the value it changes.
#ifndef BG_PROJECT_H_GUARD
#define BG_PROJECT_H_GUARD

#include "bg_rng.h"
#include <cstdint>
#include <string>
#include <vector>

namespace bg {

// Line-preserving INI/"key = value" text as the FB tools read it: sections
// "[Name]", keys "Key = value", "//" starts a comment. Raw bytes throughout.
class TextIni {
public:
	bool Load(const std::string& path);
	bool Save(const std::string& path);
	bool LoadText(const std::string& raw);
	const std::string& Text() const { return text; }
	const std::string& Path() const { return path; }
	void SetPath(const std::string& p) { path = p; }
	bool IsLoaded() const { return loaded; }
	bool IsDirty() const { return loaded && text != saved; }
	// Replace the whole text (undo/redo); the disk copy is untouched.
	void SetText(const std::string& raw) { text = raw; }

	std::vector<std::string> Sections() const;
	bool HasSection(const std::string& sec) const;
	// section "" = anywhere in the file (bgNNInfo.txt keys are read file-wide);
	// the first occurrence wins, as in the games.
	bool Get(const std::string& sec, const std::string& key, std::string& value) const;
	std::string GetOr(const std::string& sec, const std::string& key, const std::string& def) const;
	// Replace the value text in place (spacing and trailing comment kept), or
	// add "Key = value" at the end of the section (or of the file for "").
	void Set(const std::string& sec, const std::string& key, const std::string& value);
	void RemoveKey(const std::string& sec, const std::string& key);
	// Append a new section (with keys) before the first section numbered
	// higher, so the file stays sorted when names are [Prefix_NNN].
	void AddSection(const std::string& sec, const std::vector<std::pair<std::string, std::string>>& keys);
	void RemoveSection(const std::string& sec);
	void RenameSection(const std::string& from, const std::string& to);

private:
	struct Line { size_t begin, end; };   // [begin, end) excluding the line break
	std::vector<Line> Lines() const;
	std::string LineText(const Line& l) const { return text.substr(l.begin, l.end - l.begin); }
	// index range of lines belonging to section (header excluded); false if absent
	bool SectionRange(const std::vector<Line>& ls, const std::string& sec, size_t& first, size_t& last) const;
	std::string eol() const;
	std::string text, saved, path;   // saved = the bytes on disk
	bool loaded = false, dirty = false;
};

// One stage of the list (MBAACC BgList entry, or MBAC table slot).
struct StageEntry {
	int id = 0;                   // BgList section number / MBAC table id
	int order = 0;                // position in the select screen walk
	bool listed = false;          // has a [Bg_NNN] section (MBAACC) / enabled (MBAC)
	std::string dataFile;         // "bg41"
	std::string datPath;          // resolved .dat ("" if missing: the game drops the entry)
	int infoFile = 0, selectable = 0, giant = 0;
	float colorVal = 0.0f;
	bool excludedTraining = false; // g_StageExcludeList_Training
	bool excludedOther = false;    // g_StageExcludeList_Default (and the random pool)
	// BGM [BGM_NNN] with NNN = id
	bool hasBgm = false;
	std::string bgmFile, bgmComment;  // comment is Shift-JIS (the stage/track name)
	int bgmLoop = 0;
	std::string bgmLoopPos;
	std::string previewPath, nameEnPath, nameJpPath;
};

class StageProject {
public:
	// Open from any stage file path or a game directory.
	bool Open(const std::string& pathInGame, Game game);
	bool IsOpen() const { return open; }
	Game GetGame() const { return game; }
	const std::string& GameDir() const { return gameDir; }
	const std::string& BgDir() const { return bgDir; }
	const std::vector<StageEntry>& Entries() const { return entries; }
	const StageEntry* Find(int id) const;
	const StageEntry* FindByDat(const std::string& datPath) const;
	// Neighbour in select order (wraps); skips entries whose .dat is missing.
	const StageEntry* Step(int fromId, int dir) const;

	TextIni& BgList() { return bgList; }
	TextIni& Bgm() { return bgm; }
	bool IsDirty() const { return bgList.IsDirty() || bgm.IsDirty(); }
	bool SaveAll();

	// Edits (each is one undo step). Rebuild Entries() afterwards.
	void SetListValue(int id, const std::string& key, const std::string& value);
	void SetBgmValue(int id, const std::string& key, const std::string& value);
	void AddStage(int id, const std::string& dataFile);
	void RemoveStage(int id);
	void MoveStage(int fromId, int toId);   // renumber (also moves its BGM section)
	bool Undo();
	bool Redo();
	bool CanUndo() const { return !undoStack.empty(); }
	bool CanRedo() const { return !redoStack.empty(); }
	uint64_t UndoSeq() const { return undoStack.empty() ? 0 : undoStack.back().seq; }
	uint64_t RedoSeq() const { return redoStack.empty() ? 0 : redoStack.back().seq; }
	const std::string& UndoLabel() const;
	const std::string& RedoLabel() const;

	void Rebuild();

	// Hardcoded MBAA.exe exclusion lists (V-rt, see header comment).
	static bool ExcludedTraining(int id);
	static bool ExcludedOther(int id);

private:
	struct Snap { std::string bgList, bgm, label; uint64_t seq = 0; };
	void Commit(const std::string& label);
	bool open = false;
	Game game = Game::MBAACC;
	std::string gameDir, bgDir;
	TextIni bgList, bgm;
	std::vector<StageEntry> entries;
	std::vector<Snap> undoStack, redoStack;
	Snap pending;   // state before the current edit
};

// Global edit sequence shared by the stage histories (object edits in
// bg::File and metadata edits here) so Edit > Undo picks the latest step.
uint64_t NextEditSeq();

} // namespace bg

#endif
