#ifndef PATTOOLPANEASDASD_H_GUARD
#define PATTOOLPANEASDASD_H_GUARD
#include "pat_partset_pane.h"
#include "../draw_window.h"
#include "../framedata.h"

//This is pat editor pane
class PatToolPane : public DrawWindow
{
	using DrawWindow::DrawWindow;


public:
    PatToolPane(Render* render, StateReference *curInstance);
    bool isVisible = true;
	void Draw();
    void DrawTool();
    void DrawPopUp();
    void RegeneratePartsetNames();

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *partSetDecoratedNames;
};

#endif /* PATTOOLPANEASDASD_H_GUARD */
