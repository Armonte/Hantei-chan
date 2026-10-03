#include "han2_character.h"
#include "character_instance.h"
#include "framedata_han2.h"
#include "han2/han2_container.h"
#include "han2/pac_archive.h"
#include "han2_pat.h"
#include "han2/gof1_archive.h"
#include "framedata_gof1.h"
#include "han2/mb_cg.h"
#include "framedata_pb2k1.h"
#include "misc.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace han2 {

// Sprite bank embedded in a GOF1-family character: BMP Cutter bank (GOF1 / GOF2 style) or the Melty Blood strip bank.
static bool LoadEmbeddedCg(CharacterInstance &ch, const std::vector<uint8_t> &cg)
{
	if (cg.empty()) return false;
	if (ch.cg.loadFromMemory(cg.data(), (unsigned)cg.size())) return true;
	std::string e; auto mb = MbCgBank::Parse(cg.data(), cg.size(), &e);
	return mb && ch.cg.loadForeign(mb);
}

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

	if (cont->sub == 2) {   // GOF2: companions <stem>00.PAT / <stem>00.CHP
		std::vector<uint8_t> pat, chp;
		const std::string v = cont->gofVariant;
		if (read(stem + v + ".PAT", pat) && IsPat(pat.data(), pat.size())) cont->parts = std::move(pat);
		if (read(stem + v + ".CHP", chp)) cont->cg = std::move(chp);
		cont->sourcePath = origin + "/" + stem + ".DT2";
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

bool LoadGof1Character(CharacterInstance &ch, const std::string &archivePath, const std::string &entryName, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	gof1::Archive a; std::string e;
	if (!gof1::Open(archivePath, a, &e)) return fail(e);
	int idx = gof1::Find(a, entryName);
	if (idx < 0) return fail(entryName + " is not in " + archivePath);
	std::vector<uint8_t> d;
	if (!gof1::ReadEntry(a, (size_t)idx, d, &e)) return fail(e);
	gof1::DecryptDat(d);
	if (!gof1::Load(ch.frameData, d.data(), d.size(), &e)) return fail(entryName + ": " + e);
	auto cont = ch.frameData.m_han2;
	cont->sourcePath = archivePath; cont->gof1Name = a.entries[(size_t)idx].name;
	if (!cont->parts.empty()) {
		std::string pe;
		if (PatToParts(cont->parts.data(), cont->parts.size(), ch.parts, &pe)) UploadPartsTextures(ch.parts);
	}
	LoadEmbeddedCg(ch, cont->cg);
	return true;
}

bool LoadGof1CharacterFile(CharacterInstance &ch, const std::string &path, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
	if (!f) return fail("cannot read " + path);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	gof1::DecryptDat(d);
	std::string e;
	if (!gof1::Load(ch.frameData, d.data(), d.size(), &e)) return fail(path + ": " + e);
	auto cont = ch.frameData.m_han2;
	cont->sourcePath = path; cont->gof1Name.clear();
	if (!cont->parts.empty()) {
		std::string pe;
		if (PatToParts(cont->parts.data(), cont->parts.size(), ch.parts, &pe)) UploadPartsTextures(ch.parts);
	}
	LoadEmbeddedCg(ch, cont->cg);
	return true;
}

bool LoadPb2k1CharacterFile(CharacterInstance &ch, const std::string &path, std::string *err)
{
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
	if (!f) return fail("cannot read " + path);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (!pb2k1::Decrypt(d)) return fail(path + ": not a Party Breakers character file");
	std::string e;
	if (!pb2k1::Load(ch.frameData, d.data(), d.size(), &e)) return fail(path + ": " + e);
	auto cont = ch.frameData.m_han2;
	cont->sourcePath = path; cont->gof1Name.clear();
	std::string be; auto bank = MbCgBank::Parse(cont->cg.data(), cont->cg.size(), &be, kPb2CgLayout);
	if (bank) ch.cg.loadForeign(bank);
	return true;
}

bool SyncPartsToContainer(CharacterInstance &ch, bool *partsChanged, std::string *err)
{
	if (partsChanged) *partsChanged = false;
	auto cont = ch.frameData.m_han2;
	if (!cont) return true;
	if (ch.cg.foreign() && ch.cg.foreign()->dirty() && !cont->cg.empty()) {   // sprite import on a non-Cutter bank: the module's stored bytes replace the container's
		std::vector<uint8_t> nb; ch.cg.foreign()->serialize(nb);
		if (nb.size() == cont->cg.size() && nb != cont->cg) { cont->cg = std::move(nb); cont->cgDirty = true; }
		ch.cg.foreign()->clearDirty();
	}
	if (cont->parts.empty() || !ch.parts.loaded) {
		if (!cont->cg.empty() && ch.cg.m_loaded && ch.cg.bank_size() == cont->cg.size() && memcmp(ch.cg.bank_data(), cont->cg.data(), cont->cg.size()) != 0) { memcpy(cont->cg.data(), ch.cg.bank_data(), cont->cg.size()); cont->cgDirty = true; }
		return true;
	}
	std::vector<uint8_t> out;
	if (!BuildPatSection(ch.parts, cont->parts, out, err)) return false;
	if (out != cont->parts) {
		if (partsChanged) *partsChanged = true;
		cont->partsDirty = true;
		cont->parts.swap(out);
	}
	// CG edits (sprite import) change pixel bytes in place; the bank size never changes
	if (!cont->cg.empty() && ch.cg.m_loaded && ch.cg.bank_size() == cont->cg.size() && memcmp(ch.cg.bank_data(), cont->cg.data(), cont->cg.size()) != 0)
	{ memcpy(cont->cg.data(), ch.cg.bank_data(), cont->cg.size()); cont->cgDirty = true; }
	return true;
}

bool SaveGof2Companions(CharacterInstance &ch, const std::string &dt2Path, std::string *err)
{
	auto cont = ch.frameData.m_han2;
	if (!cont || cont->sub != 2) return true;
	std::filesystem::path dir = std::filesystem::u8path(dt2Path).parent_path();
	std::string stem = std::filesystem::u8path(dt2Path).stem().string();
	auto put = [&](const std::string &ext, std::vector<uint8_t> &bytes, bool &dirty) {
		if (!dirty || bytes.empty()) return true;
		std::filesystem::path p = dir / (stem + cont->gofVariant + ext);
		std::error_code ec;
		std::filesystem::path bak = p; bak += ".bak";
		if (std::filesystem::exists(p, ec) && !std::filesystem::exists(bak, ec)) std::filesystem::copy_file(p, bak, ec);
		if (!WriteFileAtomic(p.u8string().c_str(), bytes.data(), bytes.size())) { if (err) *err = "could not write " + p.u8string(); return false; }
		dirty = false;
		return true;
	};
	return put(".PAT", cont->parts, cont->partsDirty) && put(".CHP", cont->cg, cont->cgDirty);
}

} // namespace han2
