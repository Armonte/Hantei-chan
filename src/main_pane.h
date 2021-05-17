#ifndef MAINPANE_H_GUARD
#define MAINPANE_H_GUARD
#include "draw_window.h"
#include "framedata.h"
#include "render.h"
#include <string>

//This is the main pane on the left
class MainPane : DrawWindow
{
public:
	MainPane(Render* render, FrameData *frameData, FrameState &fs);
	void Draw();

	void RegenerateNames();

private:
	bool copyThisFrame = true;
	std::string *decoratedNames;

};

#endif /* MAINPANE_H_GUARD */
