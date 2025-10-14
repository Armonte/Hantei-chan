#include "project_manager.h"
#include "character_view.h"
#include "../third_party/json/json.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

std::string ProjectManager::s_currentProjectPath;

const std::string& ProjectManager::GetCurrentProjectPath()
{
	return s_currentProjectPath;
}

void ProjectManager::SetCurrentProjectPath(const std::string& path)
{
	s_currentProjectPath = path;
}

void ProjectManager::ClearCurrentProjectPath()
{
	s_currentProjectPath.clear();
}

bool ProjectManager::HasCurrentProject()
{
	return !s_currentProjectPath.empty();
}

// Helper: Convert absolute path to relative (relative to project file)
static std::string MakeRelativePath(const std::string& filePath, const std::string& projectPath)
{
	try {
		std::filesystem::path file(filePath);
		std::filesystem::path project(projectPath);
		std::filesystem::path projectDir = project.parent_path();

		// If same drive/root, make relative
		if (file.root_path() == projectDir.root_path()) {
			auto rel = std::filesystem::relative(file, projectDir);
			return rel.string();
		}
	} catch (...) {
		// Fall through to return absolute
	}
	return filePath;
}

// Helper: Convert relative path to absolute (relative to project file)
static std::string MakeAbsolutePath(const std::string& filePath, const std::string& projectPath)
{
	std::filesystem::path file(filePath);
	if (file.is_absolute()) {
		return filePath;
	}

	std::filesystem::path project(projectPath);
	std::filesystem::path projectDir = project.parent_path();
	auto absolute = projectDir / file;
	return absolute.string();
}

// Helper: Get current timestamp in ISO 8601 format
static std::string GetCurrentTimestamp()
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	std::tm tm;
#ifdef _WIN32
	localtime_s(&tm, &time_t);
#else
	localtime_r(&time_t, &tm);
#endif

	std::stringstream ss;
	ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
	return ss.str();
}

bool ProjectManager::SaveProject(
	const std::string& path,
	const std::vector<std::unique_ptr<CharacterInstance>>& characters,
	int activeIndex,
	int theme,
	float zoomLevel,
	bool smoothRender,
	const float clearColor[3])
{
	try {
		json j;

		// Metadata
		j["version"] = "1.0";
		j["app_version"] = HA6GUIVERSION;
		j["created"] = GetCurrentTimestamp();
		j["modified"] = GetCurrentTimestamp();

		// Active character
		j["active_character"] = activeIndex;

		// Characters array
		json charactersArray = json::array();
		for (const auto& character : characters) {
			json charObj;

			charObj["name"] = character->getName();

			// Determine type and path
			const auto& txtPath = character->getTxtPath();
			const auto& ha6Path = character->getTopHA6Path();

			if (!txtPath.empty()) {
				charObj["type"] = "txt";
				charObj["path"] = MakeRelativePath(txtPath, path);
			} else if (!ha6Path.empty()) {
				charObj["type"] = "ha6";
				charObj["path"] = MakeRelativePath(ha6Path, path);
			} else {
				// Skip unsaved characters
				continue;
			}

			// Render state
			charObj["render_x"] = character->renderX;
			charObj["render_y"] = character->renderY;
			charObj["zoom"] = character->zoom;
			charObj["palette"] = character->palette;

			// Frame state
			charObj["pattern"] = character->state.pattern;
			charObj["frame"] = character->state.frame;
			charObj["selected_layer"] = character->state.selectedLayer;

			charactersArray.push_back(charObj);
		}
		j["characters"] = charactersArray;

		// UI state
		json uiState;
		uiState["theme"] = theme;
		uiState["zoom_level"] = zoomLevel;
		uiState["smooth_render"] = smoothRender;
		uiState["clear_color"] = json::array({clearColor[0], clearColor[1], clearColor[2]});
		j["ui_state"] = uiState;

		// Write to file
		std::ofstream file(path);
		if (!file.is_open()) {
			return false;
		}

		file << j.dump(2); // Pretty print with 2-space indentation
		file.close();

		return true;
	} catch (...) {
		return false;
	}
}

bool ProjectManager::LoadProject(
	const std::string& path,
	std::vector<std::unique_ptr<CharacterInstance>>& characters,
	int& activeIndex,
	int* outTheme,
	float* outZoomLevel,
	bool* outSmoothRender,
	float* outClearColor)
{
	try {
		std::ifstream file(path);
		if (!file.is_open()) {
			return false;
		}

		json j;
		file >> j;
		file.close();

		// Clear existing characters
		characters.clear();

		// Load characters
		if (j.contains("characters") && j["characters"].is_array()) {
			for (const auto& charObj : j["characters"]) {
				auto character = std::make_unique<CharacterInstance>();

				// Load based on type
				std::string type = charObj.value("type", "");
				std::string relativePath = charObj.value("path", "");
				std::string absolutePath = MakeAbsolutePath(relativePath, path);

				bool loaded = false;
				if (type == "txt") {
					loaded = character->loadFromTxt(absolutePath);
				} else if (type == "ha6") {
					loaded = character->loadHA6(absolutePath, false);
				}

				if (!loaded) {
					// TODO: Show missing file dialog
					continue; // Skip this character for now
				}

				// Restore render state
				character->renderX = charObj.value("render_x", 0);
				character->renderY = charObj.value("render_y", 150);
				character->zoom = charObj.value("zoom", 3.0f);
				character->palette = charObj.value("palette", 0);

				// Restore frame state
				character->state.pattern = charObj.value("pattern", 0);
				character->state.frame = charObj.value("frame", 0);
				character->state.selectedLayer = charObj.value("selected_layer", 0);

				characters.push_back(std::move(character));
			}
		}

		// Load active character index
		activeIndex = j.value("active_character", 0);
		if (activeIndex >= characters.size()) {
			activeIndex = characters.empty() ? -1 : 0;
		}

		// Load UI state
		if (j.contains("ui_state")) {
			const auto& uiState = j["ui_state"];

			if (outTheme) *outTheme = uiState.value("theme", 0);
			if (outZoomLevel) *outZoomLevel = uiState.value("zoom_level", 3.0f);
			if (outSmoothRender) *outSmoothRender = uiState.value("smooth_render", false);

			if (outClearColor && uiState.contains("clear_color") && uiState["clear_color"].is_array()) {
				auto colorArray = uiState["clear_color"];
				if (colorArray.size() >= 3) {
					outClearColor[0] = colorArray[0].get<float>();
					outClearColor[1] = colorArray[1].get<float>();
					outClearColor[2] = colorArray[2].get<float>();
				}
			}
		}

		return true;
	} catch (...) {
		return false;
	}
}

// New view-based SaveProject
bool ProjectManager::SaveProject(
	const std::string& path,
	const std::vector<std::unique_ptr<CharacterInstance>>& characters,
	const std::vector<std::unique_ptr<CharacterView>>& views,
	int activeViewIndex,
	int theme,
	float zoomLevel,
	bool smoothRender,
	const float clearColor[3])
{
	try {
		json j;

		// Metadata
		j["version"] = "2.0";  // Bumped version for view support
		j["app_version"] = HA6GUIVERSION;
		j["created"] = GetCurrentTimestamp();
		j["modified"] = GetCurrentTimestamp();

		// Characters array
		json charactersArray = json::array();
		for (const auto& character : characters) {
			json charObj;

			charObj["name"] = character->getName();

			// Determine type and path
			const auto& txtPath = character->getTxtPath();
			const auto& ha6Path = character->getTopHA6Path();

			if (!txtPath.empty()) {
				charObj["type"] = "txt";
				charObj["path"] = MakeRelativePath(txtPath, path);
			} else if (!ha6Path.empty()) {
				charObj["type"] = "ha6";
				charObj["path"] = MakeRelativePath(ha6Path, path);
			} else {
				// Skip unsaved characters
				continue;
			}

			// Render state
			charObj["render_x"] = character->renderX;
			charObj["render_y"] = character->renderY;
			charObj["zoom"] = character->zoom;
			charObj["palette"] = character->palette;

			charactersArray.push_back(charObj);
		}
		j["characters"] = charactersArray;

		// Views array
		json viewsArray = json::array();
		for (const auto& view : views) {
			json viewObj;

			// Find character index
			auto* character = view->getCharacter();
			auto it = std::find_if(characters.begin(), characters.end(),
				[character](const std::unique_ptr<CharacterInstance>& c) {
					return c.get() == character;
				});

			if (it == characters.end()) {
				continue; // Skip views with invalid characters
			}

			int characterIndex = std::distance(characters.begin(), it);
			viewObj["character_index"] = characterIndex;

			// View state
			const auto& state = view->getState();
			viewObj["pattern"] = state.pattern;
			viewObj["frame"] = state.frame;
			viewObj["selected_layer"] = state.selectedLayer;
			viewObj["sprite_id"] = state.spriteId;
			
			// View-specific render settings
			viewObj["zoom"] = view->getZoom();
			viewObj["is_pat_editor"] = view->isPatEditor();

			viewsArray.push_back(viewObj);
		}
		j["views"] = viewsArray;

		// Active view
		j["active_view"] = activeViewIndex;

		// UI state
		json uiState;
		uiState["theme"] = theme;
		uiState["zoom_level"] = zoomLevel;
		uiState["smooth_render"] = smoothRender;
		uiState["clear_color"] = json::array({clearColor[0], clearColor[1], clearColor[2]});
		j["ui_state"] = uiState;

		// Write to file
		std::ofstream file(path);
		if (!file.is_open()) {
			return false;
		}

		file << j.dump(2); // Pretty print with 2-space indentation
		file.close();

		return true;
	} catch (...) {
		return false;
	}
}

// New view-based LoadProject
bool ProjectManager::LoadProject(
	const std::string& path,
	std::vector<std::unique_ptr<CharacterInstance>>& characters,
	std::vector<std::unique_ptr<CharacterView>>& views,
	int& activeViewIndex,
	class Render* render,
	int* outTheme,
	float* outZoomLevel,
	bool* outSmoothRender,
	float* outClearColor)
{
	try {
		std::ifstream file(path);
		if (!file.is_open()) {
			return false;
		}

		json j;
		file >> j;
		file.close();

		// Clear existing data
		characters.clear();
		views.clear();

		// Check version
		std::string version = j.value("version", "1.0");

		// Load characters
		if (j.contains("characters") && j["characters"].is_array()) {
			for (const auto& charObj : j["characters"]) {
				auto character = std::make_unique<CharacterInstance>();

				// Load based on type
				std::string type = charObj.value("type", "");
				std::string relativePath = charObj.value("path", "");
				std::string absolutePath = MakeAbsolutePath(relativePath, path);

				bool loaded = false;
				if (type == "txt") {
					loaded = character->loadFromTxt(absolutePath);
				} else if (type == "ha6") {
					loaded = character->loadHA6(absolutePath, false);
				}

				if (!loaded) {
					// TODO: Show missing file dialog
					continue; // Skip this character for now
				}

				// Restore render state
				character->renderX = charObj.value("render_x", 0);
				character->renderY = charObj.value("render_y", 150);
				character->zoom = charObj.value("zoom", 3.0f);
				character->palette = charObj.value("palette", 0);

				characters.push_back(std::move(character));
			}
		}

		// Load views (if version 2.0+)
		if (version >= "2.0" && j.contains("views") && j["views"].is_array()) {
			// Track view numbers per character
			std::vector<int> viewCounts(characters.size(), 0);

			for (const auto& viewObj : j["views"]) {
				int characterIndex = viewObj.value("character_index", -1);

				if (characterIndex < 0 || characterIndex >= characters.size()) {
					continue; // Skip invalid views
				}

				auto* character = characters[characterIndex].get();
				auto view = std::make_unique<CharacterView>(character, render);

				// Set view number
				int viewNumber = viewCounts[characterIndex]++;
				view->setViewNumber(viewNumber);

				// Restore view state
				auto& state = view->getState();
				state.pattern = viewObj.value("pattern", 0);
				state.frame = viewObj.value("frame", 0);
				state.selectedLayer = viewObj.value("selected_layer", 0);
				state.spriteId = viewObj.value("sprite_id", -1);
				
				// Restore view-specific render settings
				float viewZoom = viewObj.value("zoom", 3.0f);
				view->setZoom(viewZoom);
				
				// Restore PatEditor mode if applicable
				bool isPatEditor = viewObj.value("is_pat_editor", false);
				if (isPatEditor) {
					view->setPatEditor(true);
					view->refreshPanes(render);
				}

				views.push_back(std::move(view));
			}

			// Load active view
			activeViewIndex = j.value("active_view", 0);
			if (activeViewIndex >= views.size()) {
				activeViewIndex = views.empty() ? -1 : 0;
			}
		} else {
			// Legacy format (version 1.0) - create one view per character
			int legacyActiveIndex = j.value("active_character", 0);

			for (size_t i = 0; i < characters.size(); i++) {
				auto* character = characters[i].get();
				auto view = std::make_unique<CharacterView>(character, render);
				view->setViewNumber(0);

				// Legacy: character had state embedded
				// We already loaded it into character, but views need their own state
				// For now, just use defaults - legacy projects won't have multi-view state anyway

				views.push_back(std::move(view));
			}

			activeViewIndex = legacyActiveIndex;
			if (activeViewIndex >= views.size()) {
				activeViewIndex = views.empty() ? -1 : 0;
			}
		}

		// Load UI state
		if (j.contains("ui_state")) {
			const auto& uiState = j["ui_state"];

			if (outTheme) *outTheme = uiState.value("theme", 0);
			if (outZoomLevel) *outZoomLevel = uiState.value("zoom_level", 3.0f);
			if (outSmoothRender) *outSmoothRender = uiState.value("smooth_render", false);

			if (outClearColor && uiState.contains("clear_color") && uiState["clear_color"].is_array()) {
				auto colorArray = uiState["clear_color"];
				if (colorArray.size() >= 3) {
					outClearColor[0] = colorArray[0].get<float>();
					outClearColor[1] = colorArray[1].get<float>();
					outClearColor[2] = colorArray[2].get<float>();
				}
			}
		}

		return true;
	} catch (...) {
		return false;
	}
}
