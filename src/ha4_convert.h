#ifndef HA4_CONVERT_H_GUARD
#define HA4_CONVERT_H_GUARD

// MBAC (Hantei4 .DAT) -> MBAACC-style HA6 character export.
// Output for <name>: <name>.ha6, <name>.cg (the embedded CG, same BMP Cutter3
// format MBAACC uses), <name>.pat (embedded parts converted to PAniDataFile,
// only if the .DAT has parts), <name>.pal (copied from the MBAC .PAL next to
// the .DAT, same 64 x 256 format) and <name>.txt (Hantei-chan/MBAACC character
// list: [DataFile] / [BmpcutFile] / [PAniFile]).
// Conversion rules: docs/HANTEI_MBAC_SUPPORT.md section 4.

#include <string>
#include <vector>

class FrameData;

namespace ha4conv {

struct Options {
	std::string outDir;        // output folder (created if missing)
	std::string baseName;      // defaults to the lower-case .DAT stem
	std::string palPath;       // defaults to <stem>.PAL next to the .DAT (any case)
	bool writeCG = true;
	bool writePat = true;
	bool writePal = true;
	bool writeTxt = true;
};

struct Report {
	std::vector<std::string> lines;
	std::string ha6Path, cgPath, patPath, palPath, txtPath;
};

// Apply the HA4 -> HA6 rules that are not already implied by the model
// mapping (in place; call it on a copy of the loaded data).
void PrepareForHA6(FrameData &fd);

// Convert already-loaded HA4 data (fd.m_ha4 must be set).
bool Convert(const FrameData &fd, const std::string &datPath, const Options &opt, Report &rep);

// Load + convert a .DAT file.
bool ConvertFile(const std::string &datPath, const Options &opt, Report &rep);

} // namespace ha4conv

#endif
