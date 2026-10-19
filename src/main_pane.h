#ifndef MAINPANE_H_GUARD
#define MAINPANE_H_GUARD
#include <functional>
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
	// Drawn under the pattern fields: the script lines that use this pattern (set by the main frame with the Script panel).
	static inline std::function<void(FrameData*, int pattern)> drawScriptRefs;
	void Draw();
	void DrawPatternList();
	void DrawPatternListBody();

	void RegenerateNames();

	// Pattern name search
	bool showPatternSearch = false;  // Popup window visibility
	bool showPatternSearchBar = true;  // Search bar visibility (always visible when enabled)
	int patternListShown = -1;         // left pane pattern list: the pattern its selection last followed (scrolls only on change)

private:
	bool copyThisFrame = true;
	std::string *decoratedNames;

	// UTF-8 buffers for text editing (ImGui expects UTF-8)
	std::string nameEditBuffer;
	std::string noteEditBuffer;
	std::string codeNameEditBuffer;

	struct SequenceWId {
		int id;
		Sequence seq;
	};
	std::list<SequenceWId> patCopyStack;
	void PopCopies();

	// Range paste
	int ranges[2] = {0, 0};
	bool rangeWindow = false;

	// Landing frame tools
	int landingFrameValue = 0;
};

#endif /* MAINPANE_H_GUARD */
