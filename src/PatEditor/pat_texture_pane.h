#ifndef PATTEXTUREPANEASDASD_H_GUARD
#define PATTEXTUREPANEASDASD_H_GUARD
#include "../draw_window.h"
#include "../framedata.h"

//This is pat editor pane
class PatTexturePane : public DrawWindow
{
	using DrawWindow::DrawWindow;


public:
    PatTexturePane(Render* render, StateReference *curInstance);
    bool isVisible = true;
	void Draw();
    void DrawTexture();
    void DrawPopUp();
    void RegenerateTexturesNames();
    // RBO / GOF2 / GOF1 sprite sheets (32-bit square images inside the PAT): PNG import / export, add, remove.
    void DrawFbSheets();
    // A PNG dropped on the editor: added as a new sheet (or replaces the selected one). Returns the message to show.
    std::string ImportSheetFile(const std::string &path, bool replaceSelected);

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *textureDecoratedNames;
    std::string fbMessage;
    bool fbConfirmRemove = false;
    int textureNamesCount = -1;
};

#endif /* PATTEXTUREPANEASDASD_H_GUARD */
