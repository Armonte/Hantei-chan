#include "project_manager.h"
#include "character_view.h"
#include "hud_theme_exporter.h"
#include "misc.h"
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

// Point the character's CG at its saved palette number, clamped to what the
// loaded palette file provides.
static void ApplySavedPalette(CharacterInstance& character)
{
	const int palCount = character.cg.getPalNumber();
	if (palCount <= 0) {
		character.palette = 0;
		return;
	}
	if (character.palette < 0) character.palette = 0;
	if (character.palette >= palCount) character.palette = palCount - 1;
	character.cg.changePaletteNumber(character.palette);
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

// Helper: write the project JSON atomically (temp file + replace), so a
// failed or interrupted save never leaves a truncated .hproj behind.
static bool WriteProjectFile(const std::string& path, const json& j)
{
	const std::string text = j.dump(2); // Pretty print with 2-space indentation
	return WriteFileAtomic(path.c_str(), text.data(), text.size());
}

// Helper: project format major version. Accepts "2.0" strings and numbers;
// compared numerically ("10.0" > "2.0").
static int ProjectMajorVersion(const json& j)
{
	auto it = j.find("version");
	if (it == j.end()) return 1;
	if (it->is_number()) return (int)it->get<double>();
	if (it->is_string()) {
		try { return std::stoi(it->get<std::string>()); } catch (...) {}
	}
	return 1;
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

		// Write to file (atomic replace)
		if (!WriteProjectFile(path, j)) {
			return false;
		}

		// Export HUD theme stub alongside the project.
		ExportHudThemeProfile(std::filesystem::path(path).parent_path());

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
				// The stored number alone does nothing: the CG keeps pointing at
				// palette 0 until changePaletteNumber runs (issue #70).
				ApplySavedPalette(*character);

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
	const float clearColor[3],
	const std::string& workspaceJson)
{
	try {
		json j;

		// Metadata
		j["version"] = "2.0";  // Bumped version for view support
		j["app_version"] = HA6GUIVERSION;
		j["created"] = GetCurrentTimestamp();
		j["modified"] = GetCurrentTimestamp();

		// Characters array. savedIndex maps a character to its position in
		// the written array; unsaved characters are skipped, so this is not
		// the same as its index in `characters`.
		json charactersArray = json::array();
		std::vector<std::pair<const CharacterInstance*, int>> savedIndex;
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

			savedIndex.emplace_back(character.get(), (int)charactersArray.size());
			charactersArray.push_back(charObj);
		}
		j["characters"] = charactersArray;

		// Views array
		json viewsArray = json::array();
		for (const auto& view : views) {
			json viewObj;

			// Find the character's index in the *written* characters array
			auto* character = view->getCharacter();
			auto it = std::find_if(savedIndex.begin(), savedIndex.end(),
				[character](const std::pair<const CharacterInstance*, int>& e) {
					return e.first == character;
				});

			if (it == savedIndex.end()) {
				continue; // Skip views of unsaved/invalid characters
			}

			viewObj["character_index"] = it->second;

			// View state
			const auto& state = view->getState();
			viewObj["pattern"] = state.pattern;
			viewObj["frame"] = state.frame;
			viewObj["selected_layer"] = state.selectedLayer;
			viewObj["sprite_id"] = state.spriteId;
			
			// View-specific render settings
			viewObj["zoom"] = view->getZoom();
			viewObj["is_pat_editor"] = view->isPatEditor();
			// Per-view camera and onion skin (wave 2). Older builds ignore them.
			viewObj["id"] = view->getId();
			viewObj["camera_x"] = view->camera().panX;
			viewObj["camera_y"] = view->camera().panY;
			{
				const auto& o = view->onion();
				viewObj["onion"] = {
					{"enabled", o.enabled}, {"before", o.before}, {"after", o.after},
					{"spacing", o.spacing}, {"keyframes_only", o.keyframesOnly},
					{"include_spawns", o.includeSpawns}, {"alpha", o.alpha}, {"falloff", o.falloff},
					{"past_tint", {o.pastTint.r, o.pastTint.g, o.pastTint.b}},
					{"future_tint", {o.futureTint.r, o.futureTint.g, o.futureTint.b}},
				};
			}

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

		// Detached windows: which tabs each window holds, geometry and pane
		// layout toggle. Written in the same atomic pass as everything else
		// (EX rewrote the file a second time with a plain ofstream).
		if (!workspaceJson.empty()) {
			json ws = json::parse(workspaceJson, nullptr, false);
			if (!ws.is_discarded()) j["workspace"] = ws;
		}

		// Write to file (atomic replace)
		if (!WriteProjectFile(path, j)) {
			return false;
		}

		// Export HUD theme stub alongside the project.
		ExportHudThemeProfile(std::filesystem::path(path).parent_path());

		return true;
	} catch (...) {
		return false;
	}
}

// New view-based LoadProject
// Transactional: everything is built into local containers and only moved
// into `characters`/`views` once the file has parsed. Characters that fail to
// load are reported via outFailedCharacters, and views are bound through a
// file-index -> loaded-character map, so a skipped character never shifts
// later views onto the wrong character.
bool ProjectManager::LoadProject(
	const std::string& path,
	std::vector<std::unique_ptr<CharacterInstance>>& characters,
	std::vector<std::unique_ptr<CharacterView>>& views,
	int& activeViewIndex,
	class Render* render,
	int* outTheme,
	float* outZoomLevel,
	bool* outSmoothRender,
	float* outClearColor,
	std::vector<std::string>* outFailedCharacters,
	std::string* outWorkspaceJson)
{
	try {
		std::ifstream file(path);
		if (!file.is_open()) {
			return false;
		}

		json j;
		file >> j;
		file.close();

		std::vector<std::unique_ptr<CharacterInstance>> newCharacters;
		std::vector<std::unique_ptr<CharacterView>> newViews;
		std::vector<std::string> failed;
		int newActiveView = -1;

		// Check version (numeric major, not a string compare)
		int majorVersion = ProjectMajorVersion(j);

		// fileToLoaded[i] = pointer to the loaded character for entry i of the
		// file's "characters" array, or nullptr if it failed to load.
		std::vector<CharacterInstance*> fileToLoaded;

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
					failed.push_back(absolutePath.empty() ? charObj.value("name", std::string("(unnamed)")) : absolutePath);
					fileToLoaded.push_back(nullptr);
					continue;
				}

				// Restore render state
				character->renderX = charObj.value("render_x", 0);
				character->renderY = charObj.value("render_y", 150);
				character->zoom = charObj.value("zoom", 3.0f);
				character->palette = charObj.value("palette", 0);
				// The stored number alone does nothing: the CG keeps pointing at
				// palette 0 until changePaletteNumber runs (issue #70).
				ApplySavedPalette(*character);

				fileToLoaded.push_back(character.get());
				newCharacters.push_back(std::move(character));
			}
		}

		// Load views (if version 2.0+)
		if (majorVersion >= 2 && j.contains("views") && j["views"].is_array()) {
			// Track view numbers per character
			std::vector<std::pair<CharacterInstance*, int>> viewCounts;

			int fileViewIndex = -1;
			int requestedActive = j.value("active_view", 0);
			for (const auto& viewObj : j["views"]) {
				++fileViewIndex;
				int characterIndex = viewObj.value("character_index", -1);

				if (characterIndex < 0 || characterIndex >= (int)fileToLoaded.size() ||
				    !fileToLoaded[characterIndex]) {
					continue; // Invalid index, or its character failed to load
				}

				auto* character = fileToLoaded[characterIndex];
				auto view = std::make_unique<CharacterView>(character, render);

				// Set view number
				auto vc = std::find_if(viewCounts.begin(), viewCounts.end(),
					[character](const std::pair<CharacterInstance*, int>& e) { return e.first == character; });
				if (vc == viewCounts.end()) {
					viewCounts.emplace_back(character, 0);
					vc = viewCounts.end() - 1;
				}
				view->setViewNumber(vc->second++);

				// Restore view state
				auto& state = view->getState();
				state.pattern = viewObj.value("pattern", 0);
				state.frame = viewObj.value("frame", 0);
				state.selectedLayer = viewObj.value("selected_layer", 0);
				state.spriteId = viewObj.value("sprite_id", -1);
				
				// Restore view-specific render settings
				float viewZoom = viewObj.value("zoom", 3.0f);
				view->setZoom(viewZoom);
				{
					// Keep the saved id (detached-window tabs refer to it)
					// unless the file repeats one.
					const uint64_t savedId = viewObj.value("id", (uint64_t)0);
					bool duplicate = false;
					for (const auto& other : newViews) duplicate |= other->getId() == savedId;
					if (!duplicate) view->setId(savedId);
				}
				// The camera defaults to the character's (pre-wave-2 files).
				view->camera().panX = viewObj.value("camera_x", (float)character->renderX);
				view->camera().panY = viewObj.value("camera_y", (float)character->renderY);
				if (viewObj.contains("onion") && viewObj["onion"].is_object()) {
					const auto& oj = viewObj["onion"];
					auto& o = view->onion();
					o.enabled = oj.value("enabled", o.enabled);
					o.before = std::clamp(oj.value("before", o.before), 0, 16);
					o.after = std::clamp(oj.value("after", o.after), 0, 16);
					o.spacing = std::clamp(oj.value("spacing", o.spacing), 1, 600);
					o.keyframesOnly = oj.value("keyframes_only", o.keyframesOnly);
					o.includeSpawns = oj.value("include_spawns", o.includeSpawns);
					o.alpha = std::clamp(oj.value("alpha", o.alpha), 0.f, 1.f);
					o.falloff = std::clamp(oj.value("falloff", o.falloff), 0.f, 1.f);
					auto readTint = [&](const char* key, glm::vec3& out) {
						if (oj.contains(key) && oj[key].is_array() && oj[key].size() >= 3)
							out = glm::vec3(oj[key][0].get<float>(), oj[key][1].get<float>(), oj[key][2].get<float>());
					};
					readTint("past_tint", o.pastTint);
					readTint("future_tint", o.futureTint);
				}
				
				// Restore PatEditor mode if applicable
				bool isPatEditor = viewObj.value("is_pat_editor", false);
				if (isPatEditor) {
					view->setPatEditor(true);
					view->refreshPanes(render);
				}

				// Active view follows the view it named, not its file position
				if (fileViewIndex == requestedActive) {
					newActiveView = (int)newViews.size();
				}
				newViews.push_back(std::move(view));
			}

			if (newActiveView < 0 || newActiveView >= (int)newViews.size()) {
				newActiveView = newViews.empty() ? -1 : 0;
			}
		} else {
			// Legacy format (version 1.0) - create one view per character
			int legacyActiveIndex = j.value("active_character", 0);

			for (size_t i = 0; i < fileToLoaded.size(); i++) {
				auto* character = fileToLoaded[i];
				if (!character) {
					continue;
				}
				auto view = std::make_unique<CharacterView>(character, render);
				view->setViewNumber(0);

				// Legacy: character had state embedded
				// We already loaded it into character, but views need their own state
				// For now, just use defaults - legacy projects won't have multi-view state anyway

				if ((int)i == legacyActiveIndex) {
					newActiveView = (int)newViews.size();
				}
				newViews.push_back(std::move(view));
			}

			if (newActiveView < 0 || newActiveView >= (int)newViews.size()) {
				newActiveView = newViews.empty() ? -1 : 0;
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

		// Commit. Views are moved before characters are replaced so the
		// caller never holds a view whose character was destroyed.
		views = std::move(newViews);
		characters = std::move(newCharacters);
		activeViewIndex = newActiveView;
		if (outFailedCharacters) {
			*outFailedCharacters = std::move(failed);
		}
		if (outWorkspaceJson) {
			outWorkspaceJson->clear();
			if (j.contains("workspace") && j["workspace"].is_object())
				*outWorkspaceJson = j["workspace"].dump();
		}
		return true;
	} catch (...) {
		return false;
	}
}
