#ifndef PATSHAPEPANEASDASD_H_GUARD
#define PATSHAPEPANEASDASD_H_GUARD
#include "pat_partset_pane.h"
#include "../draw_window.h"
#include "../framedata.h"

//This is pat editor pane
class PatShapePane : public DrawWindow
{
	using DrawWindow::DrawWindow;


public:
    PatShapePane(Render* render, StateReference *curInstance);
    bool isVisible = true;
	void Draw();
    void DrawShape();
    void DrawPopUp();
    void RegenerateShapesNames();

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *shapesDecoratedNames;
};

#endif /* PATSHAPEPANEASDASD_H_GUARD */
