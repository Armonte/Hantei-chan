#ifndef UI_MAIN_MENU_IMPL_H_GUARD
#define UI_MAIN_MENU_IMPL_H_GUARD

// ============================================================================
// Main Menu Implementation
// ============================================================================
// Contains the MainFrame::Menu() member function implementation
//
// This file is included at the end of main_frame.cpp to keep the menu system
// separate from the main file for better organization.
//
// Menu layout (old -> new mapping: docs/ui/menu_map.md):
// - File   : Open... / Open Recent, Import >, Save*, Export >, Project >, New/Close Character, Exit
// - Edit   : Undo / Redo, Keyboard shortcuts, Preferences >
// - View   : Zoom, Filter, Background colour, Palette number, PUPS, Game format, Language, Onion skin, PNG export
// - Tools  : Animation player, Diff, Pattern manager / comparison, Variable references, Command files,
//            PAT editor, CG sprites, PAC browser / creator, MBAACC package
// - Stage  : stage files and stage display options
// - Game   : Game Link, Authoring >, Game view, HUD preview, BGM preview, Legacy >
// - Window : pane toggles, inspectors, reports, Notes, Vectors guide, Stage Browser, detached windows
// - Help   : About
// ============================================================================

void MainFrame::addRecentFile(const std::string& path)
{
	if (path.empty()) return;
	const std::string normalized = normalizePath(path);
	auto it = std::find_if(gSettings.recentFiles.begin(), gSettings.recentFiles.end(),
		[&](const std::string& e) { return normalizePath(e) == normalized; });
	if (it != gSettings.recentFiles.end()) gSettings.recentFiles.erase(it);
	gSettings.recentFiles.insert(gSettings.recentFiles.begin(), normalized);
	if (gSettings.recentFiles.size() > 10) gSettings.recentFiles.resize(10);
}

// File > Open...: ONE entry for every supported file. The format is decided by the header magic (never by
// extension alone: .DAT is MBAC "Hantei4", RBO/GOF2 "HAN2RBO", a PAC archive or a "bgmake" stage), then routed to
// the existing per-format loader (the same ones File > Import reaches directly).
void MainFrame::openAnyFile(const std::string& path)
{
	namespace fs = std::filesystem;
	std::vector<uint8_t> head;
	{
		std::ifstream f(fs::u8path(path), std::ios::binary);
		head.resize(4096);
		f.read((char*)head.data(), (std::streamsize)head.size());
		head.resize((size_t)f.gcount());
	}
	std::string ext = fs::u8path(path).extension().string();
	for (auto& c : ext) c = (char)tolower((unsigned char)c);
	auto starts = [&](const char* m, size_t n) { return head.size() >= n && memcmp(head.data(), m, n) == 0; };
	auto fail = [&](const std::string& why) {
		requestErrorPopup("Load Error", why + "\n" + path);
	};
	auto loadChar = [&](bool (CharacterInstance::*fn)(const std::string&)) {
		if (findCharacterByPath(path)) { fail(TXT("Already open:")); return; }
		auto character = std::make_unique<CharacterInstance>();
		if (!((*character).*fn)(path)) { fail(TXT("Could not load the character .txt:")); return; }
		characters.push_back(std::move(character));
		createViewForCharacter(characters.back().get());
		markProjectModified();
		addRecentFile(path);
	};

	if (head.empty()) { fail(TXT("Cannot open the file (empty or unreadable):")); return; }

	if (starts("Hantei6DataFile", 15) || starts("Hantei4\0", 8)) {   // HA6 and MBAC (Act Cadenza) .DAT
		if (findCharacterByPath(path)) { fail(TXT("Already open:")); return; }
		auto character = std::make_unique<CharacterInstance>();
		if (!character->loadHA6(path, false)) { fail(TXT("Not a valid Hantei4 (MBAC) or HA6 file:")); return; }
		characters.push_back(std::move(character));
		createViewForCharacter(characters.back().get());
		markProjectModified();
		addRecentFile(path);
		return;
	}
	if (han2::IsHan2(head.data(), head.size()) || pac::LooksLikePac(head.data(), head.size()) ||
	    starts("BMP Cutter", 10) ||
	    (ext == ".p" && gof1::LooksLikeArchive(head.data(), head.size())) ||
	    (head.size() >= 8 && head[4] == 0x67 && head[5] == 0x45 && head[6] == 0x23 && head[7] == 0x01 && head[0] >= 2 && head[0] <= 4 && head[1] == 0)) {
		// RBO / GOF2 / GOF1 character (.DT2/.DAT), PAC archive (opens the archive browser), standalone .PAT, .CHP sprite bank
		const size_t before = characters.size();
		const bool isArchive = pac::LooksLikePac(head.data(), head.size()) || ext == ".p";
		openHan2File(path);
		if (characters.size() > before || isArchive) addRecentFile(path);
		return;
	}
	if (starts("bgmake", 6)) {   // stage .DAT
		loadStageFile(path);
		addRecentFile(path);
		return;
	}
	if (ext == ".hproj" && !head.empty()) {
		size_t i = 0;
		if (starts("\xEF\xBB\xBF", 3)) i = 3;
		while (i < head.size() && isspace(head[i])) i++;
		if (i < head.size() && head[i] == '{') {
			openRecentProject(path);   // project file: goes through the unsaved-changes prompt
			addRecentFile(path);
			return;
		}
	}
	{   // .txt character descriptor: an INI with a [DataFile] section
		std::string text(head.begin(), head.end());
		for (auto& c : text) c = (char)tolower((unsigned char)c);
		if (text.find("[datafile]") != std::string::npos) { loadChar(&CharacterInstance::loadFromTxt); return; }
	}
	fail(std::string(TXT("Unrecognized file format (not HA6, MBAC .DAT, RBO / GOF2 .DT2/.DAT, PAT, CHP, PAC, stage, project or character .txt).")) + "\n" +
	     TXT("Use File > Import for vector / command / palette / CG files:"));
}

void MainFrame::Menu(unsigned int errorPopupId)
{
	if (ImGui::BeginMenuBar())
	{
		//ImGui::Separator();
		if (ImGui::BeginMenu(LBL("File")))
		{
			auto* active = getActiveCharacter();
			bool hasActive = (active != nullptr);
			bool hasProject = ProjectManager::HasCurrentProject();

			// One smart Open: the format is detected from the header (see openAnyFile)
			if (ImGui::MenuItem(LBL("Open...")))
			{
				std::string path = FileDialog(fileType::OPENANY, false);
				if (!path.empty()) openAnyFile(path);
			}
			if (ImGui::BeginMenu(LBL("Open Recent"), !gSettings.recentFiles.empty()))
			{
				std::string pendingOpen;
				for (const auto& recentPath : gSettings.recentFiles) {
					size_t lastSlash = recentPath.find_last_of("/\\");
					std::string filename = (lastSlash != std::string::npos) ? recentPath.substr(lastSlash + 1) : recentPath;
					if (ImGui::MenuItem(filename.c_str())) pendingOpen = recentPath;
					if (ImGui::IsItemHovered()) {
						std::string displayPath = recentPath;
						std::replace(displayPath.begin(), displayPath.end(), '\\', '/');
						ImGui::SetTooltip("%s", displayPath.c_str());
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem(LBL("Clear Recent Files"))) gSettings.recentFiles.clear();
				ImGui::EndMenu();
				if (!pendingOpen.empty()) openAnyFile(pendingOpen);   // after the loop: opening reorders the list
			}

			// ---- Import: the per-format loaders (power users; Open... routes to the same ones) ----
			if (ImGui::BeginMenu(LBL("Import")))
			{
				if (ImGui::MenuItem(LBL("Load from .txt...")))
				{
					std::string path = FileDialog(fileType::TXT, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup(errorPopupId);
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadFromTxt(path)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							} else {
								ImGui::OpenPopup(errorPopupId);
							}
						}
					}
				}

				if (ImGui::MenuItem(LBL("Load chr HA6 from .txt...")))
				{
					std::string path = FileDialog(fileType::TXT, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup(errorPopupId);
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadChrHA6FromTxt(path)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							} else {
								ImGui::OpenPopup(errorPopupId);
							}
						}
					}
				}

				if (ImGui::MenuItem(LBL("Load HA6...")))
				{
					std::string path = FileDialog(fileType::HA6, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup(errorPopupId);
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadHA6(path, false)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							} else {
								ImGui::OpenPopup(errorPopupId);
							}
						}
					}
				}

				// MBAC (Act Cadenza) Hantei4 .DAT; detected by content (see framedata_ha4.h)
				if (ImGui::MenuItem(LBL("Load MBAC .DAT (Act Cadenza)...")))
				{
					std::string path = FileDialog(fileType::HA4, false);
					if (!path.empty()) {
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup(errorPopupId);
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadHA6(path, false)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							} else {
								requestErrorPopup("Load Error", std::string(TXT("Not a Hantei4 (MBAC) or HA6 file:")) + "\n" + path);
							}
						}
					}
				}

				// French-Bread RBO / GOF2 (.DAT/.DT2) and PAC archives; detected by content
				if (ImGui::MenuItem(LBL("Load RBO / GOF2 character (.DT2/.DAT)...")))
				{
					std::string path = FileDialog(fileType::HAN2, false);
					if (!path.empty() && !openHan2File(path))
						ImGui::OpenPopup(errorPopupId);
				}

				if (ImGui::MenuItem(LBL("Load HA6 and Patch...")))
				{
					std::string path = FileDialog(fileType::HA6, false);
					if (!path.empty()) {
						// Check for duplicate
						if (findCharacterByPath(path)) {
							ImGui::OpenPopup(errorPopupId);
						} else {
							auto character = std::make_unique<CharacterInstance>();
							if (character->loadHA6(path, true)) {
								characters.push_back(std::move(character));
								createViewForCharacter(characters.back().get());
								markProjectModified();
							} else {
								ImGui::OpenPopup(errorPopupId);
							}
						}
					}
				}

				ImGui::Separator();
				if (ImGui::MenuItem(LBL("Load Commands (_c.txt)..."), nullptr, false, hasActive))
				{
					if (hasActive) {
						std::string &&file = FileDialog(fileType::TXT);
						if(!file.empty())
						{
							loadCommandsForActive(file);
						}
					}
				}

				if (ImGui::MenuItem(LBL("Load CG..."), nullptr, false, hasActive))
				{
					if (hasActive) {
						std::string &&file = FileDialog(fileType::CG);
						if(!file.empty())
						{
							if(!active->loadCG(file))
							{
								ImGui::OpenPopup(errorPopupId);
							}
							render.SwitchImage(-1);
						}
					}
				}

				if (ImGui::MenuItem(LBL("Load palette..."), nullptr, false, hasActive))
				{
					if (hasActive) {
						std::string &&file = FileDialog(fileType::PAL);
						if(!file.empty())
						{
							if(!active->cg.loadPalette(file.c_str()))
							{
								ImGui::OpenPopup(errorPopupId);
							}
							render.SwitchImage(-1);
						}
					}
				}

				if (ImGui::MenuItem(LBL("Load vector.txt...")))
				{
					std::string&& file = FileDialog(fileType::VECTOR);
					if (!file.empty())
					{
						if (!vectors.load(file.c_str()))
						{
							ImGui::OpenPopup(errorPopupId);
						}
						render.SwitchImage(-1);
					}
				}

				if (ImGui::MenuItem(LBL("Load Parts (.pat)...")))
				{
					std::string &&file = FileDialog(fileType::PAT);
					if(!file.empty())
					{
						if (hasActive) {
							// Load PAT into existing character
							if(!active->loadPAT(file))
							{
								ImGui::OpenPopup(errorPopupId);
							}
							else
							{
								render.SetParts(&active->parts);
								// Hide Right Pane (attack params) when loading PAT - it's not relevant for PAT editing
								auto* view = getActiveView();
								if (view && view->getRightPane()) {
									view->getRightPane()->isVisible = false;
								}
							}
						}
						else {
							// No character loaded - create standalone PAT editor
							createPatEditorView(file);
						}
					}
				}

				ImGui::Separator();

				// Effect.ha6 status (per-character, auto-loaded from character folder)
				if (active && active->effectCharacter) {
					// Show status when loaded
					ImGui::TextDisabled(TXT("Effect.ha6: Loaded for %s"), active->getName().c_str());
					if (ImGui::IsItemHovered()) {
						std::string folder = active->effectCharacter->getBaseFolder();
						int patternCount = active->effectCharacter->frameData.get_sequence_count();
						int imageCount = active->effectCharacter->cg.get_image_count();

						// Get first image filename if available
						const char* cgFileName = (imageCount > 0) ? active->effectCharacter->cg.get_filename(0) : nullptr;
						std::string cgFile = cgFileName ? cgFileName : TXT("None");

						ImGui::BeginTooltip();
						ImGui::Text(TXT("Effect.ha6 Details for %s:"), active->getName().c_str());
						ImGui::Separator();
						ImGui::Text(TXT("Folder: %s"), folder.c_str());
						ImGui::Text(TXT("CG File: %s"), cgFile.c_str());
						ImGui::Text(TXT("Images: %d"), imageCount);
						ImGui::Text(TXT("Pattern Count: %d"), patternCount);
						ImGui::EndTooltip();
					}

					if (ImGui::MenuItem(LBL("Reload Effect.ha6"))) {
						printf("[Effect] Reloading effect.ha6 for %s\n", active->getName().c_str());
						active->effectCharacter.reset();
						if (active->loadEffectCharacter()) {
							printf("[Effect] ✓ Successfully reloaded\n");
						} else {
							printf("[Effect] ✗ Failed to reload\n");
						}
					}
				} else if (active) {
					// Show status when not loaded for current character
					ImGui::TextDisabled(TXT("Effect.ha6: Not loaded for %s"), active->getName().c_str());
				} else {
					// No active character
					ImGui::TextDisabled(TXT("Effect.ha6: No character loaded"));
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			// Ctrl+S saves the active character (and the .hproj when a project is open).
			if (ImGui::MenuItem(LBL("Save Character"), shortcuts.registry().label(ShortcutAction::save).c_str(), false, hasActive))
			{
				if (hasActive) {
					saveCharacter(active);
				}
			}

			if (ImGui::MenuItem(LBL("Save Character As..."), nullptr, false, hasActive))
			{
				if (hasActive) {
					std::string &&file = FileDialog(active->frameData.isHan2() ? fileType::HAN2SAVE : fileType::HA6, true);
					if(!file.empty())
					{
						saveCharacterAs(active, file);
					}
				}
			}

			if (ImGui::MenuItem(LBL("Save Merged Stack As..."), nullptr, false, hasActive && active->frameData.ownFile() >= 0))
			{
				// Flatten every file of the .txt stack into one HA6 (what Save
				// used to write into the target file before issue #71).
				std::string &&file = FileDialog(fileType::HA6, true);
				if (!file.empty() && !active->frameData.save_merged(file.c_str()))
					requestErrorPopup("Save Error", std::string(TXT("Could not write ")) + file);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(TXT("Write the merged view of all files in the .txt stack to one HA6.\nSave Character writes only the target file's own patterns plus your edits."));

			if (ImGui::MenuItem(LBL("Save as MOD..."), nullptr, false, hasActive && !active->getTxtPath().empty()))
			{
				if (hasActive) {
					// Generate MOD filename from top HA6 path
					const auto& topHA6 = active->getTopHA6Path();
					if(!topHA6.empty())
					{
						size_t dotPos = topHA6.find_last_of(".");
						std::string basePath = (dotPos != std::string::npos) ? topHA6.substr(0, dotPos) : topHA6;
						std::string modPath = basePath + "_MOD.HA6";

						if (!active->saveModifiedOnly(modPath)) {
							requestErrorPopup("Save Error", std::string(TXT("Could not write ")) + modPath);
						}
					}
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem(LBL("Save Parts (.pat)"), nullptr, false, hasActive && active->parts.loaded))
			{
				if (hasActive) {
					if(!active->savePAT())
					{
						ImGui::OpenPopup(errorPopupId);
					}
				}
			}

			if (ImGui::MenuItem(LBL("Save Parts As..."), nullptr, false, hasActive && active->parts.loaded))
			{
				if (hasActive) {
					std::string &&file = FileDialog(fileType::PAT, true);
					if(!file.empty())
					{
						if(!active->savePATAs(file))
						{
							ImGui::OpenPopup(errorPopupId);
						}
					}
				}
			}

			ImGui::Separator();

			// ---- Export ----
			if (ImGui::BeginMenu(LBL("Export")))
			{
				if (ImGui::MenuItem(LBL("Export MBAC as HA6..."), nullptr, false, hasActive && active->frameData.isHA4()))
				{
					std::string &&file = FileDialog(fileType::HA6, true);
					if (!file.empty()) {
						std::string report;
						bool ok = ha4ui::ExportAsHA6(active, file, report);
						requestErrorPopup(ok ? "MBAC Export" : "Export Error", report);
					}
				}

				if (ImGui::MenuItem(LBL("Export RBO / GOF2 sprites, poses and animations (PNG + JSON)..."), nullptr, false, hasActive && active->frameData.isHan2()))
					exportHan2Character(active);
				ImGui::EndMenu();
			}

			// ---- Project ----
			if (ImGui::BeginMenu(LBL("Project")))
			{
				if (ImGui::MenuItem(LBL("New Project"), shortcuts.registry().label(ShortcutAction::newProject).c_str()))
				{
					newProject();
				}

				if (ImGui::MenuItem(LBL("Open Project..."), shortcuts.registry().label(ShortcutAction::openProject).c_str()))
				{
					openProject();
				}

				// Recent projects submenu
				if (ImGui::BeginMenu(LBL("Recent Projects"), !gSettings.recentProjects.empty()))
				{
					for (const auto& recentPath : gSettings.recentProjects) {
						// Extract filename for display
						size_t lastSlash = recentPath.find_last_of("/\\");
						std::string filename = (lastSlash != std::string::npos)
							? recentPath.substr(lastSlash + 1)
							: recentPath;

						if (ImGui::MenuItem(filename.c_str())) {
							openRecentProject(recentPath);
						}

						// Show full path as tooltip (convert backslashes to forward slashes for display)
						if (ImGui::IsItemHovered()) {
							std::string displayPath = recentPath;
							std::replace(displayPath.begin(), displayPath.end(), '\\', '/');
							ImGui::SetTooltip("%s", displayPath.c_str());
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem(LBL("Clear Recent Projects"))) {
						gSettings.recentProjects.clear();
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem(LBL("Save Project"), hasProject ? shortcuts.registry().label(ShortcutAction::save).c_str() : nullptr, false, hasProject))
				{
					saveProject();
				}

				if (ImGui::MenuItem(LBL("Save Project As..."), shortcuts.registry().label(ShortcutAction::saveProjectAs).c_str()))
				{
					saveProjectAs();
				}

				ImGui::Separator();

				if (ImGui::MenuItem(LBL("Close Project"), nullptr, false, hasProject))
				{
					closeProject();
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			// Character menu items
			if (ImGui::MenuItem(LBL("New Character")))
			{
				auto character = std::make_unique<CharacterInstance>();
				character->frameData.initEmpty();
				character->setName("Untitled");
				characters.push_back(std::move(character));
				createViewForCharacter(characters.back().get());
				markProjectModified();
			}

			if (ImGui::MenuItem(LBL("Reopen Closed Tab"), shortcuts.registry().label(ShortcutAction::reopenClosedView).c_str(), false, !m_closedTabs.empty()))
				reopenClosedTab();

			if (ImGui::MenuItem(LBL("Close Character"), nullptr, false, hasActive))
			{
				if (hasActive) {
					tryCloseView(activeViewIndex);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem(LBL("Exit"))) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Edit")))
		{
			auto* active = getActiveCharacter();
			auto* view = getActiveView();
			const bool editable = active && view && !view->isStageView();
			const UndoManager* undo = editable ? &active->undoManager : nullptr;
			auto stepName = [](const UndoManager::Entry* e) {
				if (!e) return std::string();
				std::string name = e->label.empty() ? std::string(TXT("Edit")) : e->label;
				char cnt[96];
				if (e->changes.size() == 1) snprintf(cnt, sizeof(cnt), TXT(" (pattern %d)"), (int)e->changes.front().index);
				else snprintf(cnt, sizeof(cnt), TXT(" (%d patterns)"), (int)e->changes.size());
				name += cnt;
				return name;
			};
			const std::string undoLabel = std::string(TXT("Undo ")) + (undo ? stepName(undo->peekUndo()) : std::string());
			const std::string redoLabel = std::string(TXT("Redo ")) + (undo ? stepName(undo->peekRedo()) : std::string());
			if (ImGui::MenuItem(undoLabel.c_str(), shortcuts.registry().label(ShortcutAction::undo).c_str(),
			                    false, undo && undo->canUndo()))
				PerformUndoRedo(getActiveView(), false);
			if (ImGui::MenuItem(redoLabel.c_str(), shortcuts.registry().label(ShortcutAction::redo).c_str(),
			                    false, undo && undo->canRedo()))
				PerformUndoRedo(getActiveView(), true);
			if (undo) {
				ImGui::TextDisabled(TXT("History: %zu undo / %zu redo, ~%zu KiB"),
					undo->undoCount(), undo->redoCount(), undo->historyBytes() / 1024);
			}
			// [authoring] the tuning history (sidecar files): Ctrl+Z / Ctrl+Y reach it while the Authoring window has focus
			if (authoring::showWindow) {
				const std::string tu = authoring::UndoLabel(false), tr = authoring::UndoLabel(true);
				if (ImGui::MenuItem((std::string(TXT("Undo tuning: ")) + (tu.empty() ? std::string("-") : tu)).c_str(), nullptr, false, !tu.empty()))
					authoring::UndoRedo(false);
				if (ImGui::MenuItem((std::string(TXT("Redo tuning: ")) + (tr.empty() ? std::string("-") : tr)).c_str(), nullptr, false, !tr.empty()))
					authoring::UndoRedo(true);
			}
			// Stage tabs: object / frame / event / Info.txt edits (bg::File) and
			// BgList.ini / bgm.txt edits (Stage Browser) share one history.
			if (view && view->isStageView()) {
				const std::string su = stageUndoLabel(false), sr = stageUndoLabel(true);
				if (ImGui::MenuItem((std::string(TXT("Undo ")) + su).c_str(), shortcuts.registry().label(ShortcutAction::undo).c_str(),
				                    false, !su.empty()))
					stageUndoRedo(false, true);
				if (ImGui::MenuItem((std::string(TXT("Redo ")) + sr).c_str(), shortcuts.registry().label(ShortcutAction::redo).c_str(),
				                    false, !sr.empty()))
					stageUndoRedo(true, true);
				if (ImGui::MenuItem(LBL("Save stage + metadata"), shortcuts.registry().label(ShortcutAction::save).c_str(),
				                    false, currentBgFile != nullptr))
					saveStageAll();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(LBL("Keyboard shortcuts..."), nullptr, m_showKeyBindings))
				m_showKeyBindings = !m_showKeyBindings;
			if (ImGui::BeginMenu(LBL("Preferences")))
			{
				DrawExtensionProfileMenu();
				if (ImGui::BeginMenu(LBL("Switch preset style")))
				{
					if (i18n::Combo(LBL("Style"), &style_idx, "Warm\0Dark\0Light\0ImGui\0"))
					{
						LoadTheme(style_idx);
					}
					ImGui::EndMenu();
				}
				DrawDetachPreference();
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("View")))
		{
			auto* active = getActiveCharacter();
			if (ImGui::BeginMenu(LBL("Zoom level")))
			{
				auto* view = getActiveView();
				float currentZoom = view ? view->getZoom() : zoom_idx;
				
				ImGui::SetNextItemWidth(80);
				if (ImGui::SliderFloat(LBL("Zoom"), &currentZoom, 0.25f, 20.0f, "%.2f"))
				{
					SetZoom(currentZoom);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					Tooltip(TXT("Ctrl + Click to set exact value (per-view setting)"));
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(LBL("Filter")))
			{
				if (ImGui::Checkbox(LBL("Bilinear"), &smoothRender))
				{
					render.filter = smoothRender;
					render.SwitchImage(-1);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(LBL("Background color")))
			{
				ImGui::ColorEdit3("##clearColor", (float*)&clearColor, ImGuiColorEditFlags_NoInputs);
				ImGui::EndMenu();
			}
			if (active && active->cg.getPalNumber() > 0 && ImGui::BeginMenu(LBL("Palette number")))
			{
				ImGui::SetNextItemWidth(80);
				ImGui::InputInt(LBL("Palette"), &active->palette);
				if(active->palette >= active->cg.getPalNumber())
					active->palette = active->cg.getPalNumber()-1;
				else if(active->palette < 0)
					active->palette = 0;
				if(active->cg.changePaletteNumber(active->palette))
					render.SwitchImage(-1);
				if (active->cg.getPalNumber() == 130)
					ImGui::TextDisabled(TXT("130 = 65 colours x 2 sets:\npalette n+65 is colour n's alternate set."));
				if (active->cg.pupsBankCount() > 1) {
					ImGui::TextDisabled(TXT("PUPS files loaded:"));
					for (int b = 0; b < active->cg.pupsBankCount(); ++b)
						if (active->cg.hasPupsBank(b)) { ImGui::SameLine(); ImGui::TextDisabled(b == 0 ? ".pal" : "_p%d", b); }
				}
				ImGui::EndMenu();
			}
			ImGui::MenuItem(LBL("PUPS palette files"), nullptr, &render.followPups);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Draw patterns with PUPS n using <cg>_pn.pal, as the game does (#76)."));
			if (ImGui::BeginMenu(LBL("Game format")))
			{
				// Which game's HA6 dialect the labels and the writer follow (issues #14, #74).
				const Ha6Game detected = active ? active->frameData.detectedGame() : Ha6Game::Auto;
				char autoBuf[160]; snprintf(autoBuf, sizeof(autoBuf), TXT("Auto (detected: %s)"), Ha6GameName(detected));
				std::string autoLabel = autoBuf;
				if (ImGui::MenuItem(autoLabel.c_str(), nullptr, g_ha6GameOverride == Ha6Game::Auto))
					g_ha6GameOverride = Ha6Game::Auto;
				if (ImGui::MenuItem("MBAACC", nullptr, g_ha6GameOverride == Ha6Game::MBAACC))
					g_ha6GameOverride = Ha6Game::MBAACC;
				if (ImGui::MenuItem(LBL("UNI / UNIST / UNI2 (5 layers)"), nullptr, g_ha6GameOverride == Ha6Game::UNI))
					g_ha6GameOverride = Ha6Game::UNI;
				if (ImGui::MenuItem(LBL("MELTY BLOOD: TYPE LUMINA (3 layers)"), nullptr, g_ha6GameOverride == Ha6Game::MBTL))
					g_ha6GameOverride = Ha6Game::MBTL;
				ImGui::EndMenu();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Detected from the data: AFGX layer count (5 = UNI, 3 = MBTL),\n"
					"no ATV2/AFGX = MBAACC. Switches field labels (e.g. attack flag 4, #74)\n"
					"and the format new patterns are saved in."));
			if (ImGui::BeginMenu("Language / \xe8\xa8\x80\xe8\xaa\x9e###Language")) {
				if (ImGui::MenuItem("English", nullptr, i18n::language == 0)) { i18n::language = 0; han2ui::SaveHan2Settings(); }
				if (ImGui::MenuItem("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", nullptr, i18n::language == 1)) { i18n::language = 1; han2ui::SaveHan2Settings(); }
				ImGui::EndMenu();
			}
			ImGui::Separator();
			DrawViewRenderItems();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Tools")))
		{
			auto* active = getActiveCharacter();
			bool hasActive = (active != nullptr);
			if (ImGui::MenuItem(LBL("Animation player (game rules, onion skin)..."), nullptr, han2ui::showAnimWindow, hasActive && active->frameData.isHan2()))
				han2ui::showAnimWindow = !han2ui::showAnimWindow;
			if (ImGui::MenuItem(LBL("Changes against the loaded RBO / GOF2 file (diff)..."), nullptr, han2ui::showDiffWindow, hasActive && active->frameData.isHan2()))
				han2ui::showDiffWindow = !han2ui::showDiffWindow;
			if (ImGui::MenuItem(LBL("Pattern manager"), nullptr, m_patMgr.open)) m_patMgr.open = !m_patMgr.open;
			if (ImGui::MenuItem(LBL("Pattern comparison"), nullptr, m_showCompare)) m_showCompare = !m_showCompare;
			if (ImGui::MenuItem(LBL("Variable references (batch replace)"), nullptr, m_varRefs.open)) m_varRefs.open = !m_varRefs.open;
			ImGui::Separator();
			if (ImGui::MenuItem(LBL("Command File Editor (active character)"), nullptr, false, hasActive))
				openCommandEditorForActive();
			if (ImGui::MenuItem(LBL("Open Command File...")))
			{
				std::string &&file = FileDialog(fileType::CMDTXT);
				if (!file.empty()) openCommandEditor(file);
			}
			ImGui::Separator();
			if (ImGui::MenuItem(LBL("Edit parts of this RBO / GOF2 character (PAT editor)"), nullptr, false, hasActive && active->frameData.isHan2() && active->parts.loaded))
				openPartsEditorForCharacter(active);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(TXT("Parts live in the .DAT: save as .DAT to keep part edits (a .DT2 carries only the pattern area)."));
			if (ImGui::MenuItem(LBL("CG sprites of this RBO / GOF2 character (export / import)..."), nullptr, han2ui::showCgWindow, hasActive && active->cg.m_loaded))
				han2ui::showCgWindow = !han2ui::showCgWindow;
			if (ImGui::MenuItem(LBL("RBO / GOF2 archives (PAC)..."), nullptr, han2ui::showBrowser))
				han2ui::showBrowser = !han2ui::showBrowser;
			if (ImGui::MenuItem(LBL("Create / patch a PAC archive..."), nullptr, han2ui::showPacCreate))
				han2ui::showPacCreate = !han2ui::showPacCreate;
			ImGui::Separator();
			DrawPackageToolsMenuItems();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Stage")))
		{
			if (ImGui::MenuItem(LBL("Load Stage File...")))
			{
				std::string path = FileDialog(fileType::DAT, false);
				if (!path.empty())
					loadStageFile(path);
			}
			if (ImGui::MenuItem(LBL("Save Stage As..."), nullptr, false, currentBgFile != nullptr))
			{
				std::string path = FileDialog(fileType::DAT, true);
				if (!path.empty())
					currentBgFile->Save(path.c_str());
			}
			if (ImGui::MenuItem(LBL("Clear Stage"), nullptr, false, currentBgFile != nullptr))
				clearStage();
			if (ImGui::BeginMenu(LBL("Open game stage"), ensureStageProject())) {
				const bg::StageEntry* cur = currentBgFile ? stageProject.FindByDat(currentBgFile->GetFilename()) : nullptr;
				for (const auto& e : stageProject.Entries()) {
					if (e.datPath.empty()) continue;
					if (ImGui::MenuItem(e.Label().c_str(), nullptr, cur && cur->id == e.id)) openStageInActiveTab(e.datPath);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem(LBL("Previous stage"), shortcuts.registry().label(ShortcutAction::previousStage).c_str(), false, currentBgFile != nullptr))
				stepStage(-1);
			if (ImGui::MenuItem(LBL("Next stage"), shortcuts.registry().label(ShortcutAction::nextStage).c_str(), false, currentBgFile != nullptr))
				stepStage(1);
			bool authoring = bgRenderer.GetPatPlacement() == bg::Renderer::PatPlacement::Authoring;
			if (ImGui::Checkbox(LBL("PAT placement: Authoring"), &authoring)) {
				bgRenderer.SetPatPlacement(authoring ? bg::Renderer::PatPlacement::Authoring : bg::Renderer::PatPlacement::Game);
				gSettings.stagePatAuthoring = authoring;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Off (Game-exact): PAT part positions as MBAACC applies them.\n"
				                  "On (Authoring): parts at the PAT canvas origin (320, 320) are drawn unpositioned,\n"
				                  "as MBAC would; this puts the bg18/bg20/bg47 wind on the grass. MBAACC itself draws\n"
				                  "that wind below the floor, off screen."));
			if (ImGui::Checkbox(LBL("Clamp camera to the game's limits"), &bgCamera.clampToGame)) {
				gSettings.stageClampCamera = bgCamera.clampToGame;
				if (bgCamera.clampToGame) bgCamera.ClampToGame();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("The game camera never goes past x +-208, y -340..0. With this on, panning further\n"
				                  "moves the view but not the camera, so parallax layers stay where the game shows them."));
			ImGui::Checkbox(LBL("Show game view"), &m_showGameViewRect);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Outline of the 640x480 area the game shows at the current game camera.\n"
				                  "Parallax layers only line up the way the game shows them inside it."));
			bool gameTex = bgRenderer.IsGameTextures();
			if (ImGui::Checkbox(LBL("Game-accurate textures"), &gameTex))
				bgRenderer.SetGameTextures(gameTex);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("On: textures as the game builds them (DXT5 on stages over the\n"
				                  "30000 KB budget via bg_dxt32.exe, pow2 textures, the game's UV insets).\n"
				                  "Off: the clean CG, for editing."));

			ImGui::Separator();

			bool bgEnabled = bgRenderer.IsEnabled();
			if (ImGui::Checkbox(LBL("Enable Stage Rendering"), &bgEnabled))
				bgRenderer.SetEnabled(bgEnabled);

			bool showDebug = bgRenderer.IsShowingDebugOverlay();
			if (ImGui::Checkbox(LBL("Show Debug Overlay"), &showDebug))
				bgRenderer.SetShowDebugOverlay(showDebug);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Bounding boxes, camera position, screen center"));

			bool parallaxEnabled = bgRenderer.IsParallaxEnabled();
			if (ImGui::Checkbox(LBL("Enable Parallax"), &parallaxEnabled))
				bgRenderer.SetParallaxEnabled(parallaxEnabled);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TXT("Disable to see objects at world positions without parallax effect"));

			if (currentBgFile && currentBgFile->IsLoaded())
			{
				ImGui::Separator();
				ImGui::TextDisabled(TXT("Loaded: %zu objects"), currentBgFile->GetObjects().size());
				ImGui::TextDisabled(TXT("Camera: (%.0f, %.0f)"), bgCamera.panLastX, bgCamera.panLastY);
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Game")))
		{
			if (ImGui::MenuItem(LBL("Game Link (MBAACC)"), nullptr, gamelink::showPanel)) gamelink::showPanel = !gamelink::showPanel;
			ImGui::Separator();
			// [authoring] docs/HANTEI_AUTHORING_MODE.md §8.11: Authoring ships in the public build, here (was "Experimental: Authoring").
			if (ImGui::BeginMenu(LBL("Authoring")))
			{
				if (ImGui::MenuItem(LBL("Authoring workspace (MBAACC)"), nullptr, authoring::showWindow)) {
					if (authoring::showWindow) authoring::showWindow = false; else authoring::Open("Setup");
				}
				ImGui::Separator();
				if (ImGui::MenuItem(LBL("Setup: characters, stage, mode"))) authoring::Open("Setup");
				if (ImGui::MenuItem(LBL("Saved setups"))) authoring::Open("Setups");
				if (ImGui::MenuItem(LBL("Tuning: tag / assists (sidecars)"))) authoring::Open("Tuning");
				if (ImGui::MenuItem(LBL("TAG HUD layout"))) authoring::Open("HUD");
				if (ImGui::MenuItem(LBL("Live: what the game resolved"))) authoring::Open("Live");
				if (ImGui::MenuItem(LBL("Game log"))) authoring::Open("Log");
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem(LBL("Game view (the game inside Hantei-chan)"), nullptr, authoring::showGameView))
				authoring::showGameView = !authoring::showGameView;
			ImGui::Separator();
			if (ImGui::MenuItem(LBL("HUD preview / colours"), nullptr, m_showHud)) m_showHud = !m_showHud;
			if (ImGui::MenuItem(LBL("BGM preview"), nullptr, m_showBgm)) m_showBgm = !m_showBgm;
			ImGui::Separator();
			if (ImGui::BeginMenu(LBL("Legacy")))
			{
				if (ImGui::MenuItem(LBL("Legacy tag_tuning.ini panel"), nullptr, tagpanel::showPanel)) tagpanel::showPanel = !tagpanel::showPanel;
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Window")))
		{
			auto* view = getActiveView();
			bool hasView = (view != nullptr);
			bool isPatEditor = hasView && view->isPatEditor();

			// HA6 Editor panes (only show when not in PAT editor mode)
			if (!isPatEditor && hasView) {
				if (view->getMainPane()) {
					std::string label = view->getMainPane()->isVisible ? TXT("Hide Animation Panel") : TXT("Show Animation Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getMainPane()->isVisible = !view->getMainPane()->isVisible;
					}
					// Pattern search bar toggle
					std::string searchLabel = view->getMainPane()->showPatternSearchBar ? TXT("Hide Pattern Search Bar") : TXT("Show Pattern Search Bar");
					if (ImGui::MenuItem(searchLabel.c_str())) {
						view->getMainPane()->showPatternSearchBar = !view->getMainPane()->showPatternSearchBar;
					}
				}
				if (view->getRightPane()) {
					std::string label = view->getRightPane()->isVisible ? TXT("Hide Attack Panel") : TXT("Show Attack Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getRightPane()->isVisible = !view->getRightPane()->isVisible;
					}
				}
				if (view->getBoxPane()) {
					std::string label = view->getBoxPane()->isVisible ? TXT("Hide Hitbox Panel") : TXT("Show Hitbox Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getBoxPane()->isVisible = !view->getBoxPane()->isVisible;
					}
				}
				ImGui::Separator();
			}

			// PAT Editor panes (only show when in PAT editor mode)
			if (isPatEditor && hasView) {
				if (view->getPartSetPane()) {
					std::string label = view->getPartSetPane()->isVisible ? TXT("Hide PartSet Panel") : TXT("Show PartSet Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getPartSetPane()->isVisible = !view->getPartSetPane()->isVisible;
					}
				}
				if (view->getPartPane()) {
					std::string label = view->getPartPane()->isVisible ? TXT("Hide Part Panel") : TXT("Show Part Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getPartPane()->isVisible = !view->getPartPane()->isVisible;
					}
				}
				if (view->getShapePane()) {
					std::string label = view->getShapePane()->isVisible ? TXT("Hide Shape Panel") : TXT("Show Shape Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getShapePane()->isVisible = !view->getShapePane()->isVisible;
					}
				}
				if (view->getTexturePane()) {
					std::string label = view->getTexturePane()->isVisible ? TXT("Hide Texture Panel") : TXT("Show Texture Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getTexturePane()->isVisible = !view->getTexturePane()->isVisible;
					}
				}
				if (view->getToolPane()) {
					std::string label = view->getToolPane()->isVisible ? TXT("Hide Tool Panel") : TXT("Show Tool Panel");
					if (ImGui::MenuItem(label.c_str())) {
						view->getToolPane()->isVisible = !view->getToolPane()->isVisible;
					}
				}
				ImGui::Separator();
			}
			if (ImGui::MenuItem(LBL("MBAC (HA4) Inspector"), nullptr, ha4ui::showInspector)) ha4ui::showInspector = !ha4ui::showInspector;
			if (ImGui::MenuItem(LBL("RBO / GOF2 (HAN2) Inspector"), nullptr, han2ui::showInspector)) han2ui::showInspector = !han2ui::showInspector;
			if (ImGui::MenuItem(han2ui::Tr("RBO / GOF2 loading report", "RBO / GOF2 \xe8\xaa\xad\xe3\x81\xbf\xe8\xbe\xbc\xe3\x81\xbf\xe3\x83\xac\xe3\x83\x9d\xe3\x83\xbc\xe3\x83\x88"), nullptr, han2ui::showLoadReport)) han2ui::showLoadReport = !han2ui::showLoadReport;
			ImGui::Separator();
			if (ImGui::MenuItem(LBL("Notes"), nullptr, m_showNotes)) m_showNotes = !m_showNotes;
			if (ImGui::MenuItem(LBL("Vectors Guide"))) vectors.drawWindow = !vectors.drawWindow;
			if (ImGui::MenuItem(LBL("Stage Browser"), nullptr, m_showStageBrowser)) m_showStageBrowser = !m_showStageBrowser;
			ImGui::Separator();
			DrawDetachedWindowItems();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(LBL("Help")))
		{
			if (ImGui::MenuItem(LBL("About"))) aboutWindow.isVisible = !aboutWindow.isVisible;
			ImGui::EndMenu();
		}

		// Status display - show active character info
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto* active = getActiveCharacter();
		if (active) {
			const auto& txtPath = active->getTxtPath();
			const auto& topHA6 = active->getTopHA6Path();

			if (!txtPath.empty() && !topHA6.empty())
			{
				// Extract just the filename from the paths for cleaner display
				size_t txtSlash = txtPath.find_last_of("/\\");
				size_t ha6Slash = topHA6.find_last_of("/\\");
				std::string txtName = (txtSlash != std::string::npos) ? txtPath.substr(txtSlash + 1) : txtPath;
				std::string ha6Name = (ha6Slash != std::string::npos) ? topHA6.substr(ha6Slash + 1) : topHA6;

				ImGui::TextDisabled(TXT("Loaded: %s"), txtName.c_str());
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), TXT("-> Saves to: %s"), ha6Name.c_str());
				if (ImGui::IsItemHovered() && active->frameData.ownFile() >= 0) {
					ImGui::SetTooltip(TXT("Saving writes the patterns that came from %s plus every\n"
					                  "pattern you edited. %d unedited pattern(s) inherited from the\n"
					                  "other files of %s are left in those files."),
					                  ha6Name.c_str(), active->frameData.inheritedPatternCount(), txtName.c_str());
				}
			}
			else if (!topHA6.empty())
			{
				size_t slash = topHA6.find_last_of("/\\");
				std::string filename = (slash != std::string::npos) ? topHA6.substr(slash + 1) : topHA6;
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), TXT("Save to: %s"), filename.c_str());
			}
			else
			{
				ImGui::TextDisabled(TXT("No save target set for %s"), active->getName().c_str());
			}
		}
		else
		{
			ImGui::TextDisabled(TXT("No character loaded"));
		}

		ImGui::EndMenuBar();
	}
}

#endif /* UI_MAIN_MENU_IMPL_H_GUARD */
