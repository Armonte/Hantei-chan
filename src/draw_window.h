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
		currState(state){};

	FrameState &currState;
	bool isVisible = true;

	virtual void Draw() = 0;

	// Callback to notify when data is modified
	std::function<void()> onModified;
protected:
	Render *render;
	FrameData *frameData;

	// Helper to mark as modified
	void markModified() {
		if (onModified) {
			onModified();
		}
	}
};

#endif /* DRAWWINDOW_H_GUARD */
