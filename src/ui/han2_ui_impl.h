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
	han2ui::OpenRequest req;
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

void MainFrame::openHan2Request(const han2ui::OpenRequest& req)
{
	auto character = std::make_unique<CharacterInstance>();
	std::string err;
	if (!character->loadHan2(req.stem, req.read, "archive " + req.origin, std::string(), err)) {
		requestErrorPopup("Load Error", err);
		return;
	}
	characters.push_back(std::move(character));
	createViewForCharacter(characters.back().get());
	markProjectModified();
}

void MainFrame::DrawHan2Windows()
{
	static bool settingsLoaded = false;
	if (!settingsLoaded) { settingsLoaded = true; han2ui::LoadHan2Settings(); }
	ProcessDroppedFiles();
	han2ui::DrawLoadReport();
	han2ui::DrawPacCreate();
	han2ui::DrawFileViewers();
	han2ui::DrawCgWindow(getActiveCharacter());
	han2ui::DrawDiffWindow(getActiveCharacter());
	if (auto* av = getActiveView()) han2ui::DrawAnimWindow(av->getCharacter(), av->getState(), &av->onion());
	han2ui::OpenRequest req;
	static std::string message;
	if (han2ui::DrawBrowser(req, message)) {
		message.clear();
		if (req.stem == "\x01open") openHan2File(req.origin);
		else if (req.stem == "\x01gof1") {
			auto character = std::make_unique<CharacterInstance>(); std::string err;
			if (!character->loadGof1(req.gof1Archive, req.gof1Entry, err)) requestErrorPopup("Load Error", err);
			else { characters.push_back(std::move(character)); createViewForCharacter(characters.back().get()); markProjectModified(); }
		}
		else openHan2Request(req);
	}
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
		} else if (ext == ".png" || ext == ".bmp") {
			requestErrorPopup("Dropped image", std::string(TXT("Open the CG sprite window or an IMG viewer first, then use its Import PNG button.\n")) + f);
		} else {
			requestErrorPopup("Dropped file", std::string(TXT("Not an RBO / GOF2 file type: ")) + f);
		}
	}
}
