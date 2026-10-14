#ifndef PATPARTPANEASDASD_H_GUARD
#define PATPARTPANEASDASD_H_GUARD
#include "pat_partset_pane.h"
#include "../draw_window.h"
#include "../framedata.h"

//This is pat editor pane
class PatPartPane : public DrawWindow
{
	using DrawWindow::DrawWindow;


public:
    PatPartPane(Render* render, StateReference *curInstance, PatPartSetPane* partsetPaneRef);
    bool isVisible = true;
    bool isUnkParams = false;
	void Draw();
    void DrawPartCutOut();
    // UV view right-drag (sosfiro): the rectangle from the press point to the cursor becomes the selected cut-out's
    // UV rectangle. Sheet pixels (0..w, 0..h of the cut-out's texture). Returns false when there is nothing to edit.
    bool BoxStart(float px, float py);
    void BoxDrag(float px, float py);
    void DrawPopUp();
    void RegeneratePartCutOutsNames();

private:
    void SetButtonCenterCalc(const char* label, int x, int y, CutOut<>* cutOut);
    int cutOutsNamesCount = -1;
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *cutOutsDecoratedNames;
    PatPartSetPane* partsetPane;
    float dragxy[2]{};


};

#endif /* PATPARTPANEASDASD_H_GUARD */
