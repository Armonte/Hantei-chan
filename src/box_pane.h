#ifndef BOX_PANE_H_GUARD
#define BOX_PANE_H_GUARD

#include "draw_window.h"

//This is the main pane on the left
class BoxPane : public DrawWindow
{
public:
	BoxPane(Render* render, FrameData *frameData, FrameState &state);

	void Draw();

	void BoxStart(int x, int y);
	void BoxDrag(int x, int y);

	void AdvanceBox(int dir);

private:
	// Helper to draw spawn timeline
	void DrawSpawnTimeline();
	std::string boxNameList[33];
	int currentBox;
	bool highlight;
	bool showManualControls;

	float dragxy[2];
};


#endif /* BOX_PANE_H_GUARD */
