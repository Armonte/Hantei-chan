#ifndef STATE_REFERENCE_H_GUARD
#define STATE_REFERENCE_H_GUARD

#include "enums.h"
#include "framedata.h"
#include "framestate.h"
#include "cg.h"
#include "parts/parts.h"
#include <functional>
#include <string>

// StateReference holds pointers to all data needed for PatEditor panes
// This is a compatibility structure to match sosfiro's architecture
class StateReference
{
public:
	// Instance structure used by PatEditor
	FrameData* framedata;
	FrameState* currState;
	CG* cg;
	int* curPalette;
	std::string* currentFilePath;
	std::string* currentPatFilePath;
	Parts* parts;
	RenderMode* renderMode;
	// Set by CharacterView: marks the view's pattern in the character's undo manager so the next commit runs even when no
	// mouse gesture ends it (drops, scripts). Parts edits themselves are diffed by the parts undo channel.
	std::function<void()> onEdited;
	// The view edits a PAT document (its own undo history, Save writes the .pat): part set renumbering is offered there.
	std::function<bool()> isPatDocument;
	void markEdited() const { if (onEdited) onEdited(); }
};

#endif /* STATE_REFERENCE_H_GUARD */
