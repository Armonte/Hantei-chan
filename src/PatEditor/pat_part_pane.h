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
    void BoxStart(int x, int y);
    void BoxDrag(int x, int y);
    void DrawPopUp();
    void RegeneratePartCutOutsNames();

private:
    void SetButtonCenterCalc(std::string label, int xy[], CutOut<>* cutOut);
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *cutOutsDecoratedNames;
    PatPartSetPane* partsetPane;
    float dragxy[2];


};

#endif /* PATPARTPANEASDASD_H_GUARD */
