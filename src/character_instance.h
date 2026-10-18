#ifndef CHARACTER_INSTANCE_H_GUARD
#define CHARACTER_INSTANCE_H_GUARD

#include "framedata.h"
#include "framestate.h"
#include "cg.h"
#include "parts/parts.h"
#include "undo_manager.h"
#include "parts/parts_undo.h"
#include "mv_script.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

class CharacterInstance
{
public:
	CharacterInstance();
	~CharacterInstance();

	// Load character from .txt file (loads multiple .ha6 files)
	bool loadFromTxt(const std::string& txtPath);

	// Load only character .ha6 from .txt file (File01 only, skips _temp.ha6 and BaseData.ha6)
	bool loadChrHA6FromTxt(const std::string& txtPath);

	// Load single .ha6 file
	bool loadHA6(const std::string& ha6Path, bool patch = false);

	// Load a French-Bread RBO / GOF2 character (frames + CG + parts) through a file reader (folder or PAC archives).
	// saveTarget: file Save writes to (empty = Save As only, e.g. when the files live inside an archive).
	bool loadHan2(const std::string& stem, const std::function<bool(const std::string&, std::vector<uint8_t>&)>& read,
	              const std::string& origin, const std::string& saveTarget, std::string& err);

	// GOF1 character from a gof_0N.p archive (Save As .DAT / new .p).
	bool loadGof1File(const std::string& path, std::string& err);
	bool loadPb2k1File(const std::string& path, std::string& err);
	bool loadQohFile(const std::string& path, int version, std::string& err);
	bool loadGof1(const std::string& archivePath, const std::string& entryName, std::string& err);

	// Load CG file
	bool loadCG(const std::string& cgPath);

	// Load PAT file (Parts)
	bool loadPAT(const std::string& patPath);

	// Save current character data
	bool save();
	bool saveAs(const std::string& ha6Path);

	// Save PAT file
	bool savePAT();
	bool savePATAs(const std::string& patPath);

	// Save only modified sequences
	bool saveModifiedOnly(const std::string& ha6Path);

	// Metadata
	void setName(const std::string& name);
	std::string getName() const;
	std::string getDisplayName() const; // Returns "Name*" if modified

	void markModified();
	void clearModified();
	bool isModified() const;
	// Annotations side file (<ha6>.notes.json, issue #58)
	void loadNotes();
	bool saveNotes(const std::string& ha6Path);
	const std::string& notesError() const { return m_notesError; }

	// File paths
	const std::vector<std::string>& getHA6Paths() const;
	const std::string& getCGPath() const;
	const std::string& getPATPath() const;
	const std::string& getTxtPath() const;
	const std::string& getTopHA6Path() const; // Auto-save target

	// MBAACC folder detection
	std::string getBaseFolder() const;
	bool isInMBAACCDataFolder() const;

	// Effect character support (per-character effect.ha6/effect.pat)
	CharacterInstance* getEffectCharacter() const;
	bool loadEffectCharacter();  // Load effect.txt/effect.ha6/effect.pat for this character

	// MBTL move-script spawn index (sibling chrXXX_mv_*.txt files), see mv_script.h
	const MvScriptIndex& getMvScripts() const { return m_mvScripts; }

	// Data access
	FrameData frameData;
	FrameState state;
	CG cg;
	Parts parts;

	// Archive this character was opened from in the archive browser (PAC / GOF1 .p path): Save writes back into it (see abrowser::SaveCharacterIntoArchive).
	std::string archiveHome;

	// Undo/Redo manager
	UndoManager undoManager;
	// PAT-editor edits of a French-Bread character join undoManager's history through this channel (parts/parts_undo.h).
	PartsUndoChannel partsUndo{&parts};

	// RBO / GOF2 CG window: whole-bank snapshots of sprite imports (max 8), per character.
	std::vector<std::vector<char>> cgImportUndo, cgImportRedo;

	// Why the last save()/saveAs() failed (empty when the reason is only "could not write the file").
	const std::string& lastSaveError() const { return m_lastSaveError; }

	// Render state (per-character)
	int renderX = 0;
	int renderY = 150;
	float zoom = 3.0f;
	int palette = 0;

	// Per-character effect (effect.ha6/effect.pat loaded from character's folder)
	std::unique_ptr<CharacterInstance> effectCharacter;

	// PAT document (a PAT editor tab opened on a .pat file, or File > New PAT): the character only carries the parts
	// model. Its one-frame pattern list is view state (the part set on screen, the test animation), so the undo manager
	// tracks the parts model instead (parts/parts_undo.h): PAT edits are undoable, set the tab '*' and prompt on close,
	// and Save writes the .pat. French-Bread characters keep their parts in the character file and use the
	// character's own parts channel instead.
	bool isPatDocument() const { return m_patDocument; }
	void makePatDocument();
	PartsUndoChannel* patDocumentUndo() const { return m_patDocUndo.get(); }
	// New PAT: an empty parts model in the UNI / MBTL layout or the MBAACC one (no VEST shapes, MBAACC tags). No file yet.
	void newPatDocument(bool mbaaccFormat);

private:
	// Load sibling chrXXX_mv_*.txt move scripts and register for spawn viz
	void loadMvScripts(const std::string& txtPath);

	std::string m_name;
	std::string m_txtPath;         // Original .txt file path
	std::vector<std::string> m_ha6Paths; // All loaded .ha6 files
	std::string m_cgPath;
	std::string m_patPath;         // PAT (Parts) file path
	std::string m_topHA6Path;      // Highest-indexed .ha6 (auto-save target)
	bool m_isModified = false;
	MvScriptIndex m_mvScripts;     // MBTL move-script spawns (may be empty)
	std::string m_notesError;
	std::string m_lastSaveError;
	bool m_patDocument = false;
	std::vector<Sequence> m_patUndoDoc;              // empty pattern list the undo manager diffs in a PAT document
	std::unique_ptr<PartsUndoChannel> m_patDocUndo;  // declared after undoManager: destroyed first
};

// The placeholder character of a PAT editor tab: one pattern with one frame whose layer 0 draws the
// selected part set. The caller loads or creates the parts and calls makePatDocument().
std::unique_ptr<CharacterInstance> MakePatPlaceholderCharacter(const std::string& name);

#endif /* CHARACTER_INSTANCE_H_GUARD */
