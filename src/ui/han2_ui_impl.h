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
	if (pac::LooksLikePac(head.data(), head.size())) {
		std::string err = han2ui::AddArchive(path);
		if (!err.empty()) { requestErrorPopup("Load Error", err); return true; }
		return true;
	}
	if (!han2::IsHan2(head.data(), head.size())) {
		requestErrorPopup("Load Error", "Not an RBO / GOF2 character or PAC archive:\n" + path);
		return true;
	}
	if (findCharacterByPath(path)) { requestErrorPopup("Load Error", "Already open: " + path); return true; }
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
	han2ui::OpenRequest req;
	static std::string message;
	if (han2ui::DrawBrowser(req, message)) {
		message.clear();
		openHan2Request(req);
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
