#include "ha4_convert.h"
#include "framedata.h"
#include "framedata_ha4.h"
#include "ha4_parts.h"
#include "parts/parts.h"
#include "misc.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace ha4conv {

void PrepareForHA6(FrameData &fd)
{
	for (auto &seq : fd.m_sequences) {
		// Empty HA4 slots still carry a (default) name; HA6 only has entries
		// for patterns with frames.
		if (seq.frames.empty()) { seq.name.clear(); continue; }
		// HA6 has no per-frame HA4 bytes.
		seq.ha4 = Ha4SeqRaw{};
		for (auto &f : seq.frames) {
			f.ha4 = Ha4FrameRaw{};
			// Frames without an attack box never had an AT in HA4 and the HA6
			// writer only emits ATST with an attack box: nothing to do.
		}
	}
}

static std::string Lower(std::string s)
{
	for (auto &c : s) c = (char)tolower((unsigned char)c);
	return s;
}

// Find <dir>/<stem>.<ext> with any letter case.
static std::string FindSibling(const fs::path &dat, const std::string &ext)
{
	std::string want = Lower(dat.stem().string() + ext);
	std::error_code ec;
	for (auto &e : fs::directory_iterator(dat.parent_path(), ec))
		if (Lower(e.path().filename().string()) == want) return e.path().string();
	return {};
}

static bool WriteBytes(const std::string &path, const void *d, size_t n)
{
	return WriteFileAtomic(path.c_str(), d, n);
}

bool Convert(const FrameData &src, const std::string &datPath, const Options &opt, Report &rep)
{
	if (!src.m_ha4) { rep.lines.push_back("not HA4 data"); return false; }
	fs::path dat(datPath);
	std::string base = opt.baseName.empty() ? Lower(dat.stem().string()) : opt.baseName;
	fs::path out = opt.outDir.empty() ? dat.parent_path() : fs::path(opt.outDir);
	std::error_code ec;
	fs::create_directories(out, ec);

	// --- HA6 ---
	FrameData fd = src;
	PrepareForHA6(fd);
	fd.m_ha4.reset();             // force the HA6 writer
	rep.ha6Path = (out / (base + ".ha6")).string();
	if (!fd.save(rep.ha6Path.c_str())) { rep.lines.push_back("could not write " + rep.ha6Path); return false; }
	int pats = 0, frames = 0;
	for (auto &s : fd.m_sequences) if (!s.frames.empty()) { pats++; frames += (int)s.frames.size(); }
	rep.lines.push_back("ha6: " + std::to_string(pats) + " patterns, " + std::to_string(frames) + " frames");

	// --- CG ---
	const Ha4Container &c = *src.m_ha4;
	if (opt.writeCG && !c.cg.empty()) {
		rep.cgPath = (out / (base + ".cg")).string();
		if (!WriteBytes(rep.cgPath, c.cg.data(), c.cg.size())) { rep.lines.push_back("could not write " + rep.cgPath); return false; }
		if (!memcmp(c.cg.data(), "BMP Cutter2", 11))
			rep.lines.push_back("cg: source is \"BMP Cutter2\" (GAKIHA); written as-is");
	}

	// --- parts ---
	if (opt.writePat && !c.parts.empty()) {
		CG dummy;
		Parts parts(&dummy);
		std::string err;
		if (ha4::OldPatToParts(c.parts.data(), c.parts.size(), parts, &err)) {
			rep.patPath = (out / (base + ".pat")).string();
			if (!parts.Save(rep.patPath.c_str())) { rep.lines.push_back("could not write " + rep.patPath); return false; }
			rep.lines.push_back("pat: " + std::to_string(parts.partSets.size()) + " part sets, " +
			                    std::to_string(parts.cutOuts.size()) + " cutouts, " +
			                    std::to_string(parts.gfxMeta.size()) + " textures");
		} else {
			rep.lines.push_back("pat: " + err);
		}
	}

	// --- palette ---
	if (opt.writePal) {
		std::string pal = opt.palPath.empty() ? FindSibling(dat, ".pal") : opt.palPath;
		if (!pal.empty()) {
			rep.palPath = (out / (base + ".pal")).string();
			if (fs::equivalent(pal, rep.palPath, ec)) {}
			else if (!fs::copy_file(pal, rep.palPath, fs::copy_options::overwrite_existing, ec)) {
				rep.lines.push_back("could not copy palette " + pal);
				rep.palPath.clear();
			}
		} else {
			rep.lines.push_back("pal: no .PAL next to the .DAT (the CG's built-in palette applies)");
		}
	}

	// --- txt ---
	if (opt.writeTxt) {
		rep.txtPath = (out / (base + ".txt")).string();
		std::ofstream t(rep.txtPath, std::ios::binary | std::ios::trunc);
		t << "[DataFile]\r\nFileNum=1\r\nFile00=" << base << ".ha6\r\n";
		if (!rep.cgPath.empty()) t << "\r\n[BmpcutFile]\r\nFileNum=1\r\nFile00=" << base << ".cg\r\n";
		if (!rep.patPath.empty()) t << "\r\n[PAniFile]\r\nFileNum=1\r\nFile00=" << base << ".pat\r\n";
		if (!t) { rep.lines.push_back("could not write " + rep.txtPath); return false; }
	}
	return true;
}

bool ConvertFile(const std::string &datPath, const Options &opt, Report &rep)
{
	FrameData fd;
	std::string err;
	if (!ha4::LoadFile(fd, datPath.c_str(), &err)) { rep.lines.push_back("load: " + err); return false; }
	return Convert(fd, datPath, opt, rep);
}

} // namespace ha4conv
