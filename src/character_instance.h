#ifndef CHARACTER_INSTANCE_H_GUARD
#define CHARACTER_INSTANCE_H_GUARD

#include "framedata.h"
#include "framestate.h"
#include "cg.h"
#include "parts/parts.h"
#include "undo_manager.h"
#include <string>
#include <vector>

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

	// File paths
	const std::vector<std::string>& getHA6Paths() const;
	const std::string& getCGPath() const;
	const std::string& getPATPath() const;
	const std::string& getTxtPath() const;
	const std::string& getTopHA6Path() const; // Auto-save target

	// MBAACC folder detection
	std::string getBaseFolder() const;
	bool isInMBAACCDataFolder() const;

	// Data access
	FrameData frameData;
	FrameState state;
	CG cg;
	Parts parts;

	// Undo/Redo manager
	UndoManager undoManager;

	// Render state (per-character)
	int renderX = 0;
	int renderY = 150;
	float zoom = 3.0f;
	int palette = 0;

private:
	std::string m_name;
	std::string m_txtPath;         // Original .txt file path
	std::vector<std::string> m_ha6Paths; // All loaded .ha6 files
	std::string m_cgPath;
	std::string m_patPath;         // PAT (Parts) file path
	std::string m_topHA6Path;      // Highest-indexed .ha6 (auto-save target)
	bool m_isModified = false;
};

#endif /* CHARACTER_INSTANCE_H_GUARD */
