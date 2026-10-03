#include "han2_character.h"
#include "character_instance.h"
#include "framedata_han2.h"
#include "han2/han2_container.h"
#include "han2/pac_archive.h"
#include "han2_pat.h"
#include "misc.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace han2 {

static std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

ReadFn DirReader(const std::string &dir)
{
	return [dir](const std::string &name, std::vector<uint8_t> &out) {
		std::error_code ec;
		const std::string want = Lower(name);
		for (auto &e : fs::directory_iterator(fs::u8path(dir), ec)) {
			if (Lower(e.path().filename().string()) != want) continue;
			std::ifstream f(e.path(), std::ios::binary);
			if (!f) return false;
			out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
			return true;
		}
		return false;
	};
}

ReadFn PacReader(const std::vector<std::shared_ptr<pac::Archive>> &archives)
{
	return [archives](const std::string &name, std::vector<uint8_t> &out) {
		for (auto &a : archives) {
			int i = pac::Find(*a, name);
			if (i >= 0) return pac::ReadEntry(*a, (size_t)i, out, nullptr);
		}
		return false;
	};
}

bool LoadCharacter(CharacterInstance &ch, const std::string &stem, const ReadFn &read, const std::string &origin, std::string *summary, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::vector<uint8_t> dt2, dat;
	const bool haveDt2 = read(stem + ".DT2", dt2) && IsHan2(dt2.data(), dt2.size());
	const bool haveDat = read(stem + ".DAT", dat) && IsHan2(dat.data(), dat.size());
	if (!haveDt2 && !haveDat) return fail("no " + stem + ".DT2 / " + stem + ".DAT in " + origin);

	// DAT areas (parts, CG, names) are needed even when the frames come from the DT2
	Han2File datFile;
	bool datParsed = false;
	std::string perr;
	if (haveDat) datParsed = Parse(dat.data(), dat.size(), datFile, &perr);
	if (haveDat && !datParsed && !haveDt2) return fail(stem + ".DAT: " + perr);

	const std::vector<uint8_t> &frameBytes = haveDt2 ? dt2 : dat;
	const uint8_t *names = datParsed && datFile.area[kAreaNames].size() >= (size_t)kNamesSize ? datFile.area[kAreaNames].data() : nullptr;
	std::string lerr;
	if (!Load(ch.frameData, frameBytes.data(), frameBytes.size(), &lerr, names, names ? (size_t)kNamesSize : 0))
		return fail((haveDt2 ? stem + ".DT2: " : stem + ".DAT: ") + lerr);
	auto cont = ch.frameData.m_han2;
	cont->sourcePath = origin + "/" + stem + (haveDt2 ? ".DT2" : ".DAT");
	if (datParsed) {
		cont->datPath = origin + "/" + stem + ".DAT";
		if (haveDt2) {   // frames from the DT2: resources and the offsets of a later full save come from the DAT
			cont->parts = datFile.area[kAreaParts]; cont->cg = datFile.area[kAreaCg]; cont->names = datFile.area[kAreaNames];
			for (int i = 0; i < 4; i++) cont->areaOff[i] = datFile.areaOff[i];
			memcpy(cont->header, datFile.header, 0x40);
		}
	}

	std::string s = haveDt2 ? "frames from .DT2" : "frames from .DAT";
	if (haveDt2 && !haveDat) s += ", no .DAT (no sprites)";
	if (!cont->cg.empty() && ch.cg.loadFromMemory(cont->cg.data(), (unsigned)cont->cg.size()))
		s += ", " + std::to_string(ch.cg.get_image_count()) + " CG images";
	else s += ", no CG";
	if (!cont->parts.empty()) {
		std::string perr2;
		if (PatToParts(cont->parts.data(), cont->parts.size(), ch.parts, &perr2)) {
			UploadPartsTextures(ch.parts);
			s += ", " + std::to_string(ch.parts.partSets.size()) + " part sets";
		} else s += ", parts: " + perr2;
	}
	if (summary) *summary = s;
	return true;
}

bool SyncPartsToContainer(CharacterInstance &ch, bool *partsChanged, std::string *err)
{
	if (partsChanged) *partsChanged = false;
	auto cont = ch.frameData.m_han2;
	if (!cont || cont->parts.empty() || !ch.parts.loaded) return true;
	std::vector<uint8_t> out;
	if (!BuildPat(ch.parts, cont->parts, out, err)) return false;
	if (out != cont->parts) {
		if (partsChanged) *partsChanged = true;
		cont->parts.swap(out);
	}
	return true;
}

} // namespace han2
