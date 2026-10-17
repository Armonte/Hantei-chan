// Issue #71 reproduction: load a project .txt stack the way the editor does,
// edit ONE pattern, save the project's own file, and write it out. The caller
// (tools/uni/uni_regress.sh) then diffs the output against the original file:
// only the edited pattern may change.
//
//   uni_edit_test OWN PATTERN OUT FILE0 [FILE1 ...]
//
// Built against the current tree (target uni_edit_test). Compile with
// -DOLD_API to run the same edit through the pre-fix loader/saver
// (c015c09: every file overlays, no own-file filter).
#include <cstdlib>
#include <iostream>
#include <string>
#include "framedata.h"

int main(int argc, char** argv)
{
	if (argc < 5) {
		std::cerr << "usage: uni_edit_test OWN PATTERN OUT FILE0 [FILE1 ...]\n";
		return 1;
	}
	const int own = std::atoi(argv[1]);
	const int pat = std::atoi(argv[2]);
	const std::string out = argv[3];
	const int nfiles = argc - 4;
	FrameData fd;
	for (int i = 0; i < nfiles; ++i) {
#ifdef OLD_API
		const bool ok = fd.load(argv[4 + i], i > 0);
#else
		const bool ok = fd.load(argv[4 + i], i > 0, nfiles > 1 && i > own);
#endif
		if (!ok) { std::cerr << "load failed: " << argv[4 + i] << "\n"; return 2; }
	}
#ifndef OLD_API
	if (nfiles > 1) fd.setOwnFile(own);
#endif
	Sequence* seq = fd.get_sequence(pat);
	if (!seq || seq->frames.empty()) { std::cerr << "pattern " << pat << " has no frames\n"; return 3; }
	// The edit: first frame one tick longer.
	seq->frames[0].AF.duration += 1;
	fd.mark_modified(pat);
	if (!fd.save(out.c_str())) { std::cerr << "save failed\n"; return 6; }
	std::cout << "edited pattern " << pat << " frame 0 duration -> " << seq->frames[0].AF.duration << "\n";
	return 0;
}
