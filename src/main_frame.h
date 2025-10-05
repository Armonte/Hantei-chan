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
#include "character_view.h"
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

	// Multi-character/view architecture
	std::vector<std::unique_ptr<CharacterInstance>> characters;  // Shared character data
	std::vector<std::unique_ptr<CharacterView>> views;           // Independent views
	int activeViewIndex = -1;

	// Shared effect character (effect.ha6) - loaded automatically from MBAACC data folder
	std::unique_ptr<CharacterInstance> effectCharacter;

	std::string currentFilePath;

	// Helper methods
	CharacterView* getActiveView();
	const CharacterView* getActiveView() const;
	CharacterInstance* getActiveCharacter();
	const CharacterInstance* getActiveCharacter() const;

	void setActiveView(int index);
	void closeView(int index);
	bool tryCloseView(int index); // Returns true if closed, false if cancelled

	// Character/view management
	CharacterInstance* findCharacterByPath(const std::string& path);
	void createViewForCharacter(CharacterInstance* character);
	int countViewsForCharacter(CharacterInstance* character);

	// Effect character management
	bool loadEffectCharacter(const std::string& baseFolder);
	CharacterInstance* getEffectCharacter();
	void tryLoadEffectCharacter(CharacterInstance* character);

	void DrawBack();
	void DrawUi();
	void DrawPresetEffectMarkers(FrameState& state, CharacterInstance* character);
	void Menu(unsigned int errorId);

	void RenderUpdate();
	void AdvancePattern(int dir);
	void AdvanceFrame(int dir);

	void SetZoom(float level);
	void LoadTheme(int i );
	void WarmStyle();
	void ChangeClearColor(float r, float g, float b);

	int mDeltaX = 0, mDeltaY = 0;

	// View close confirmation
	int pendingCloseViewIndex = -1;
	bool shouldOpenUnsavedDialog = false;

	// Right-click context menu
	int contextMenuViewIndex = -1;

	// Project management
	bool m_projectModified = false;
	bool m_pendingProjectClose = false;
	bool shouldOpenUnsavedProjectDialog = false;
	enum class ProjectCloseAction { None, New, Open, Close } m_projectCloseAction = ProjectCloseAction::None;
	void newProject();
	void openProject();
	void saveProject();
	void saveProjectAs();
	void closeProject();
	void updateWindowTitle();
	bool tryCloseProject(); // Returns true if closed, false if cancelled
	void markProjectModified();
	void addRecentProject(const std::string& path);
	void openRecentProject(const std::string& path);

	AboutWindow aboutWindow;
};


#endif /* MAINFRAME_H_GUARD */
