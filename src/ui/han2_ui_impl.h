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
				if (!character->cg.loadFromMemory(all.data(), (unsigned)all.size())) { requestErrorPopup("Load Error", "Could not read the sprite bank: " + path); return true; }
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
	han2ui::DrawPacCreate();
	han2ui::DrawFileViewers();
	han2ui::DrawCgWindow(getActiveCharacter());
	han2ui::DrawDiffWindow(getActiveCharacter());
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
		ok ? std::to_string(rep.cgPngs) + " CG images, " + std::to_string(rep.posePngs) + " poses, " + std::to_string(rep.framePngs) + " frames in " +
		     std::to_string(rep.patternsWritten) + " patterns (strip.png / sheet.png / animation.json each, poses.json, animations.json)\nin " + folder + "\\" + character->getName()
		   : err);
}
