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

private:
    std::string openpopupWithId;
    std::string customPopUpMessage;
    std::string *textureDecoratedNames;
};

#endif /* PATTEXTUREPANEASDASD_H_GUARD */
