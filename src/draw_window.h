#ifndef DRAWWINDOW_H_GUARD
#define DRAWWINDOW_H_GUARD
#include "framedata.h"
#include "render.h"
#include "hitbox.h"
#include "framestate.h"
#include "state_reference.h"
#include <vector>
#include <functional>

//ImGui Windows that draw themselves. Just for utility.
class DrawWindow
{
public:
	// Constructor for existing panes (MainPane, RightPane, BoxPane)
	DrawWindow(Render* render, FrameData *frameData, FrameState &state):
		render(render),
		frameData(frameData),
		effectFrameData(nullptr),
		currState(state),
		curInstance(nullptr){};

	// Constructor for PatEditor panes (uses StateReference)
	DrawWindow(Render* render, StateReference *curInstance):
		render(render),
		frameData(nullptr),
		effectFrameData(nullptr),
		currState(*curInstance->currState),
		curInstance(curInstance){};

	virtual ~DrawWindow() = default;

	FrameState &currState;
	bool isVisible = true;

	virtual void Draw() = 0;

	// Callback to notify when data is modified
	std::function<void()> onModified;
	// Callback to save undo state before modification
	std::function<void(int)> onSaveUndo;

	// Setter for effect.ha6 frame data
	void setEffectFrameData(FrameData* data) {
		effectFrameData = data;
	}

protected:
	Render *render;
	FrameData *frameData;           // For existing panes
	FrameData *effectFrameData;     // Effect.ha6 frame data (can be null)
	StateReference *curInstance;     // For PatEditor panes (can be null)

	// Helper to mark as modified
	void markModified() {
		if (onModified) {
			onModified();
		}
	}

	// Helper to save undo state before modifying
	void saveUndoState(int patternIndex) {
		if (onSaveUndo) {
			onSaveUndo(patternIndex);
		}
	}
};

#endif /* DRAWWINDOW_H_GUARD */
