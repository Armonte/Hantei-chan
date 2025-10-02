#ifndef PROJECT_MANAGER_H_GUARD
#define PROJECT_MANAGER_H_GUARD

#include "character_instance.h"
#include "character_view.h"
#include <string>
#include <vector>
#include <memory>

class ProjectManager
{
public:
	// Save/Load project files with view support
	static bool SaveProject(const std::string& path,
	                        const std::vector<std::unique_ptr<CharacterInstance>>& characters,
	                        const std::vector<std::unique_ptr<CharacterView>>& views,
	                        int activeViewIndex,
	                        int theme,
	                        float zoomLevel,
	                        bool smoothRender,
	                        const float clearColor[3]);

	static bool LoadProject(const std::string& path,
	                        std::vector<std::unique_ptr<CharacterInstance>>& characters,
	                        std::vector<std::unique_ptr<CharacterView>>& views,
	                        int& activeViewIndex,
	                        class Render* render,
	                        int* outTheme = nullptr,
	                        float* outZoomLevel = nullptr,
	                        bool* outSmoothRender = nullptr,
	                        float* outClearColor = nullptr);

	// Legacy support (deprecated, uses temporary stubs)
	static bool SaveProject(const std::string& path,
	                        const std::vector<std::unique_ptr<CharacterInstance>>& characters,
	                        int activeIndex,
	                        int theme,
	                        float zoomLevel,
	                        bool smoothRender,
	                        const float clearColor[3]);

	static bool LoadProject(const std::string& path,
	                        std::vector<std::unique_ptr<CharacterInstance>>& characters,
	                        int& activeIndex,
	                        int* outTheme = nullptr,
	                        float* outZoomLevel = nullptr,
	                        bool* outSmoothRender = nullptr,
	                        float* outClearColor = nullptr);

	// Current project path management
	static bool HasCurrentProject();
	static const std::string& GetCurrentProjectPath();
	static void SetCurrentProjectPath(const std::string& path);
	static void ClearCurrentProjectPath();

private:
	static std::string s_currentProjectPath;
};

#endif /* PROJECT_MANAGER_H_GUARD */
