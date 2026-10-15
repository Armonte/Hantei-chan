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
    // French-Bread characters (parts->fbPartModel): PACNyx-style pose / part controls (pat_partset_fb.cpp)
    void DrawFbPoses();
    void DrawFbParts(PartSet<>& ps);
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
    // French-Bread pose / part editing state
    std::string fbNewPoseName, fbMessage;
    int fbMoveTarget = 0, fbLayerValue = 0, fbAddSheet = 0, fbAddRect[4]{0, 0, 32, 32};
    bool fbRemapFrames = true, fbAddOpen = false;
    int partSetNamesCount = -1;            // entries in partSetDecoratedNames
    int partPropNamesCount = -1;           // entries in partPropsDecoratedNames
    int partPropNamesSet = -1;             // part set they were made for
    void SaveHighlightSettings();
    // Part set renumbering (pat_renumber.cpp / pat_partset_renumber.cpp)
    void DrawRenumber();
    void DrawRenumberPopup();
    int rnFrom = 0, rnTo = 0, rnTarget = 0, rnCount = 1, rnKind = 0;
    bool rnScanned = false, rnPreviewOpen = false;
    std::string rnScannedPath, rnMessage, rnPreview;
    bool rnPreviewOk = false;
    void *rnUsers = nullptr;   // patrn::Users (owned)

};

#endif /* PATPARTSETPANEASDASD_H_GUARD */
