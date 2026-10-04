// RBO / GOF2 (French-Bread HAN2RBO) glue: opening characters and archives, the PAC browser window.
// Included into main_frame.cpp like the other ui/*_impl.h files.

bool MainFrame::openHan2File(const std::string& path)
{
	std::vector<uint8_t> head;
	{
		std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
		head.resize(0x40);
		f.read((char*)head.data(), 0x40);
		head.resize((size_t)f.gcount());
	}
	if (const auto fk = fbarc::Detect(head.data(), head.size()); fk == fbarc::Kind::PkFileInfo || fk == fbarc::Kind::MbFilePacA) {
		std::string err = han2ui::AddArchive(path);
		if (!err.empty()) requestErrorPopup("Load Error", err);
		return true;
	}
	if (!pac::LooksLikePac(head.data(), head.size()) && gof1::LooksLikeArchive(head.data(), head.size()) && std::filesystem::u8path(path).extension() == ".p") {
		std::string err = han2ui::AddArchive(path);
		if (!err.empty()) requestErrorPopup("Load Error", err);
		return true;
	}
	if (pac::LooksLikePac(head.data(), head.size())) {
		std::string err = han2ui::AddArchive(path);
		if (!err.empty()) { requestErrorPopup("Load Error", err); return true; }
		return true;
	}
	{   // standalone GOF2 .PAT (parts) or .CHP (sprite bank)
		std::vector<uint8_t> all;
		{ std::ifstream f(std::filesystem::u8path(path), std::ios::binary); all.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); }
		const bool isPat = han2::IsPat(all.data(), all.size());
		const bool isChp = all.size() > 0x4f30 && memcmp(all.data(), "BMP Cutter", 10) == 0;
		if (isPat || isChp) {
			auto character = std::make_unique<CharacterInstance>();
			character->frameData.initEmpty();
			character->setName(std::filesystem::u8path(path).filename().string());
			if (isChp) {
				if (!character->cg.loadFromMemory(all.data(), (unsigned)all.size())) { requestErrorPopup("Load Error", std::string(TXT("Could not read the sprite bank: ")) + path); return true; }
				han2ui::showCgWindow = true;
			} else {
				std::string perr;
				if (!han2::PatToParts(all.data(), all.size(), character->parts, &perr)) { requestErrorPopup("Load Error", perr); return true; }
				han2::UploadPartsTextures(character->parts);
			}
			characters.push_back(std::move(character));
			createViewForCharacter(characters.back().get());
			if (isPat) openPartsEditorForCharacter(characters.back().get());
			markProjectModified();
			return true;
		}
	}
	if (!han2::IsHan2(head.data(), head.size())) {
		requestErrorPopup("Load Error", std::string(TXT("Not an RBO / GOF2 character or PAC archive:\n")) + path);
		return true;
	}
	if (findCharacterByPath(path)) { requestErrorPopup("Load Error", std::string(TXT("Already open: ")) + path); return true; }
	std::filesystem::path p = std::filesystem::u8path(path);
	abrowser::OpenRequest req;
	req.stem = p.stem().string();
	req.read = han2::DirReader(p.parent_path().string());
	req.origin = p.parent_path().string();
	auto character = std::make_unique<CharacterInstance>();
	std::string err;
	if (!character->loadHan2(req.stem, req.read, req.origin, path, err)) {
		requestErrorPopup("Load Error", err);
		return true;
	}
	characters.push_back(std::move(character));
	createViewForCharacter(characters.back().get());
	markProjectModified();
	return true;
}

void MainFrame::openBrowserRequest(const abrowser::OpenRequest& req)
{
	using abrowser::OpenRequest;
	std::string err;
	auto adopt = [&](std::unique_ptr<CharacterInstance> character, const std::string& home) {
		character->archiveHome = home;
		const bool han2 = character->frameData.isHan2();
		characters.push_back(std::move(character));
		createViewForCharacter(characters.back().get());
		markProjectModified();
		if (han2) { han2ui::showAnimWindow = true; han2ui::showAnimList = true; if (auto* v = getActiveView()) v->setZoom(1.25f); }   // playback controls docked next to the box controls; sprites are bigger than HA6 ones
	};
	switch (req.kind) {
	case OpenRequest::Han2Stem: {
		auto character = std::make_unique<CharacterInstance>();
		if (!character->loadHan2(req.stem, req.read, std::string(TXT("archive ")) + req.origin, std::string(), err)) { requestErrorPopup("Load Error", err); return; }
		adopt(std::move(character), req.archivePath);
		break;
	}
	case OpenRequest::Gof1Entry: {
		auto character = std::make_unique<CharacterInstance>();
		if (!character->loadGof1(req.archivePath, req.entryName, err)) { requestErrorPopup("Load Error", err); return; }
		adopt(std::move(character), req.archivePath);
		break;
	}
	case OpenRequest::LooseFile: {
		const size_t before = characters.size();
		std::string path = req.path;
		{   // a loose MBAACC / UNI .HA6: its <stem>.txt lists the whole stack (HA6 files, sprite bank, palettes); opening only the .HA6 would show no sprites
			std::filesystem::path pp = std::filesystem::u8path(path);
			std::string e = pp.extension().string(); for (auto& c : e) c = (char)tolower((unsigned char)c);
			if (e == ".ha6") {
				std::filesystem::path txt = pp; txt.replace_extension(".txt");
				std::error_code ec;
				if (std::filesystem::exists(txt, ec)) {
					char probe[64]{};
					GetPrivateProfileStringA("DataFile", "FileNum", "", probe, sizeof probe, txt.u8string().c_str());
					if (probe[0]) path = txt.u8string();
				}
			}
		}
		openAnyFile(path);
		if (characters.size() > before && characters.back()->frameData.isHan2()) han2ui::showAnimWindow = true;
		break;
	}
	default: break;
	}
}

void MainFrame::DrawHan2Windows()
{
	static bool settingsLoaded = false;
	if (!settingsLoaded) { settingsLoaded = true; han2ui::LoadHan2Settings(); }
	ProcessDroppedFiles();
	if (ImGuiWindow* w = ImGui::FindWindowByName("Box Pane")) han2ui::dockAnimId = w->DockId;
	if (ImGuiWindow* w = ImGui::FindWindowByName("Right Pane")) han2ui::dockInspectorId = w->DockId;
	{   // first HAN2 character of a session: the animation list takes the upper half of the Left Pane's dock node (a split, not a tab: both stay visible)
		static bool animListDocked = false;
		if (han2ui::showAnimList && !animListDocked) {
			if (ImGuiWindow* lp = ImGui::FindWindowByName("Left Pane")) if (lp->DockId && ImGui::DockBuilderGetNode(lp->DockId)) {
				ImGuiID rest = lp->DockId, top = 0;
				top = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Up, 0.48f, nullptr, &rest);
				ImGui::DockBuilderDockWindow("Animations###animlist", top);
				ImGui::DockBuilderFinish(top);
				animListDocked = true;
			}
		}
	}
	han2ui::BeginThumbFrame();
	han2ui::DrawLoadReport();
	han2ui::DrawPacCreate();
	han2ui::DrawFileViewers();
	han2ui::DrawCgWindow(getActiveCharacter());
	han2ui::DrawDiffWindow(getActiveCharacter());
	if (auto* av = getActiveView()) { han2ui::DrawAnimWindow(av->getCharacter(), av->getState(), &av->onion()); han2ui::DrawAnimListWindow(av->getCharacter(), av->getState()); }
	abrowser::OpenRequest req;
	if (abrowser::Draw(req)) openBrowserRequest(req);
}

void MainFrame::openPartsEditorForCharacter(CharacterInstance* character)
{
	if (!character || !character->parts.loaded) return;
	render.SetParts(&character->parts);
	auto view = std::make_unique<CharacterView>(character, &render);
	view->setPatEditor(true);
	int viewNumber = 0;
	for (auto& v : views) if (v->getCharacter() == character) viewNumber = std::max(viewNumber, v->getViewNumber() + 1);
	view->setViewNumber(viewNumber);
	view->refreshPanes(&render);
	views.push_back(std::move(view));
	setActiveView(views.size() - 1);
	markProjectModified();
	needsDockRebuild = true;
}

void MainFrame::exportHan2Character(CharacterInstance* character)
{
	if (!character || !character->frameData.isHan2()) return;
	std::string folder = BrowseForFolderUtf8("");
	if (folder.empty()) return;
	han2::ExportOptions opt;
	han2::ExportReport rep;
	std::string err;
	const bool ok = han2::ExportCharacter(character->frameData, character->cg, character->parts, folder + "\\" + character->getName(), opt, rep, &err);
	requestErrorPopup(ok ? "RBO / GOF2 export" : "Export Error",
		ok ? [&] {
			char b[1024];
			snprintf(b, sizeof(b), TXT("%d CG images, %d poses, %d frames in %d patterns (strip.png / sheet.png / animation.json each, poses.json, animations.json)\nin %s"),
			         (int)rep.cgPngs, (int)rep.posePngs, (int)rep.framePngs, (int)rep.patternsWritten, (folder + "\\" + character->getName()).c_str());
			return std::string(b);
		}()
		   : err);
}

void MainFrame::ProcessDroppedFiles()
{
	if (m_droppedFiles.empty()) return;
	std::vector<std::string> files; files.swap(m_droppedFiles);
	for (auto& f : files) {
		std::string ext = std::filesystem::u8path(f).extension().string();
		for (auto& c : ext) c = (char)tolower((unsigned char)c);
		if (ext == ".img") {
			std::ifstream in(std::filesystem::u8path(f), std::ios::binary);
			std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			han2ui::OpenFileViewer(utf82sj(std::filesystem::u8path(f).filename().string()), std::move(b), "dropped");
		} else if (ext == ".dt2" || ext == ".dat" || ext == ".pat" || ext == ".chp" || ext == ".pac") {
			openHan2File(f);
		} else if (ext == ".ha6" || ext == ".txt" || ext == ".hproj" || ext == ".p") {
			openAnyFile(f);   // same header-based routing as File > Open...
		} else if (ext == ".png" || ext == ".bmp") {
			requestErrorPopup("Dropped image", std::string(TXT("Open the CG sprite window or an IMG viewer first, then use its Import PNG button.\n")) + f);
		} else {
			requestErrorPopup("Dropped file", std::string(TXT("Not an RBO / GOF2 file type: ")) + f);
		}
	}
}
