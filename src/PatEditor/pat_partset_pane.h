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
    // Regenerate the name caches when the model no longer matches them (undo, reload, another view's edit).
    void SyncNames();

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *partSetDecoratedNames;
    std::string *partPropsDecoratedNames;
    int partSetNamesCount = -1;            // entries in partSetDecoratedNames
    int partPropNamesCount = -1;           // entries in partPropsDecoratedNames
    int partPropNamesSet = -1;             // part set they were made for
    void SaveHighlightSettings();

};

#endif /* PATPARTSETPANEASDASD_H_GUARD */
