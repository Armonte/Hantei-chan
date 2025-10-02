#ifndef MAINPANE_H_GUARD
#define MAINPANE_H_GUARD
#include "draw_window.h"
#include "framedata.h"
#include "render.h"
#include <string>
#include <list>

//This is the main pane on the left
class MainPane : public DrawWindow
{
public:
	MainPane(Render* render, FrameData *frameData, FrameState &fs);
	void Draw();

	void RegenerateNames();

private:
	bool copyThisFrame = true;
	std::string *decoratedNames;

	// UTF-8 buffers for text editing (ImGui expects UTF-8)
	std::string nameEditBuffer;
	std::string codeNameEditBuffer;

	struct SequenceWId {
		int id;
		Sequence seq;
	};
	std::list<SequenceWId> patCopyStack;
	void PopCopies();

	// Range paste window
	bool rangeWindow = false;
	int ranges[2] = {0, 0};

};

#endif /* MAINPANE_H_GUARD */
