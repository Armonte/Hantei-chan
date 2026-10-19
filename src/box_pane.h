#ifndef BOX_PANE_H_GUARD
#define BOX_PANE_H_GUARD

#include "draw_window.h"
#include "timeline_navigation.h"

//This is the main pane on the left
class BoxPane : public DrawWindow
{
public:
	BoxPane(Render* render, FrameData *frameData, FrameState &state);

	void Draw();

	void BoxStart(int x, int y);
	void BoxDrag(int x, int y);
	// Drag by a world-space delta (the caller divides by its view's zoom).
	void BoxDragWorld(float dx, float dy);

	void AdvanceBox(int dir);
	int SelectedBoxId() const { return currentBox; }   // [game-view] the hitbox id selected in this pane
	void SetTimelineZoom(float value) { timelineZoom=std::clamp(value,.25f,16.f); }

	// Viewport position tool (handles drawn/driven by MainFrame).
	bool positionTool = false;
	timeline::ResizeBinding durationResizeBinding;
	std::function<bool(int,int,int)> onResizeDuration;
	bool BeginDurationResize(int state,float mouseX,float tickWidth);
	void UpdateDurationResize(float mouseX);
	bool EndDurationResize(bool cancel);
	bool DurationResizeActive() const { return durationDrag.active; }

private:
	// Helper to draw spawn timeline
	void DrawSpawnTimeline();
	std::string boxNameList[33];
	int currentBox;
	bool highlight;
	bool showManualControls;

	float dragxy[2];
	float dragScale = 1.f;   // FrameData::boxDisplayScale of the frame the drag started on

	// Timeline (issue #10)
	float timelineZoom = 1.0f;
	struct { bool active=false; int pattern=-1,state=-1,original=0,preview=0; float mouseX=0,width=1; uint64_t version=0; } durationDrag;
	struct { bool active = false; int srcFrame = -1; int efIndex = -1; bool copy = false; } spawnDrag;
};


#endif /* BOX_PANE_H_GUARD */
