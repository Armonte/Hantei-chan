#ifndef PATPARTSETPANEASDASD_H_GUARD
#define PATPARTSETPANEASDASD_H_GUARD
#include "../draw_window.h"

class PatPartSetPane : public DrawWindow
{
	using DrawWindow::DrawWindow;


public:
    PatPartSetPane(Render* render, StateReference *curInstance);
    bool isVisible = true;
    float highlightOpacity = 0.3f;
	void Draw();
    void DrawPartSet();
    void DrawPartProperty(PartSet<>* partSet);
    void DrawPopUp();
    void RegeneratePartSetNames();
    void RegeneratePartPropNames();
    void UpdateRenderFrame(int effectId, bool dontChangeProp = false);
    void UpdateHighligth(bool state);
    bool activeHighligth = false;

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *partSetDecoratedNames;
    std::string *partPropsDecoratedNames;

};

#endif /* PATPARTSETPANEASDASD_H_GUARD */
