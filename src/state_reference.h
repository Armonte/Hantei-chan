#ifndef STATE_REFERENCE_H_GUARD
#define STATE_REFERENCE_H_GUARD

#include "enums.h"
#include "framedata.h"
#include "framestate.h"
#include "cg.h"
#include "parts/parts.h"
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
};

#endif /* STATE_REFERENCE_H_GUARD */
