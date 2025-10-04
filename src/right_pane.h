#ifndef RIGHTPANEASDASD_H_GUARD
#define RIGHTPANEASDASD_H_GUARD
#include "draw_window.h"

//This is the main pane on the left
class RightPane : public DrawWindow
{
	using DrawWindow::DrawWindow;
	
public:
	void Draw();

private:
	// Helper to recursively display spawn tree
	void DisplaySpawnNode(int spawnIndex, int displayNumber);
	
};

#endif /* RIGHTPANEASDASD_H_GUARD */
