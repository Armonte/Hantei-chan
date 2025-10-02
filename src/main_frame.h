#ifndef MAINFRAME_H_GUARD
#define MAINFRAME_H_GUARD
#include "context_gl.h"
#include "render.h"
#include "main_pane.h"
#include "right_pane.h"
#include "box_pane.h"
#include "about.h"
#include "vectors.h"
#include "character_instance.h"
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>
#include <memory>

class MainFrame
{
public:
	MainFrame(ContextGl *context);
	~MainFrame();
	
	void Draw();
	void UpdateBackProj(float x, float y);
	void HandleMouseDrag(int x, int y, bool dragRight, bool dragLeft);
	bool HandleKeys(uint64_t vkey);

	void RightClick(int x, int y);
	void LoadSettings();

private:
	ContextGl *context;
	float clearColor[3];
	int style_idx = 0;
	float zoom_idx = 3.0f;
	bool smoothRender = false;


	Render render;
	VectorTXT vectors;

	// Multi-character support
	std::vector<std::unique_ptr<CharacterInstance>> characters;
	int activeCharacterIndex = -1;

	std::string currentFilePath;

	// Helper methods
	CharacterInstance* getActive();
	const CharacterInstance* getActive() const;
	void setActiveCharacter(int index);
	void closeCharacter(int index);
	bool tryCloseCharacter(int index); // Returns true if closed, false if cancelled
	void openCharacterDialog();

	void DrawBack();
	void DrawUi();
	void Menu(unsigned int errorId);

	void RenderUpdate();
	void AdvancePattern(int dir);
	void AdvanceFrame(int dir);

	void SetZoom(float level);
	void LoadTheme(int i );
	void WarmStyle();
	void ChangeClearColor(float r, float g, float b);

	int mDeltaX = 0, mDeltaY = 0;

	// Character close confirmation
	int pendingCloseCharacterIndex = -1;

	// Panes (will be dynamically updated with active character)
	std::unique_ptr<MainPane> mainPane;
	std::unique_ptr<RightPane> rightPane;
	std::unique_ptr<BoxPane> boxPane;
	AboutWindow aboutWindow;
};


#endif /* MAINFRAME_H_GUARD */
