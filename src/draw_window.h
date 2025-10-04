#ifndef DRAWWINDOW_H_GUARD
#define DRAWWINDOW_H_GUARD
#include "framedata.h"
#include "render.h"
#include "hitbox.h"
#include "framestate.h"
#include <vector>
#include <functional>
//ImGui Windows that draw themselves. Just for utility.
class DrawWindow
{
public:
	DrawWindow(Render* render, FrameData *frameData, FrameState &state):
		render(render),
		frameData(frameData),
		effectFrameData(nullptr),
		currState(state){};

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
	FrameData *frameData;
	FrameData *effectFrameData;  // Effect.ha6 frame data (can be null)

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
