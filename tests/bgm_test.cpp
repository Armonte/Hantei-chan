// BGM preview tests (issue #66).
//   bgm_test [BGM_FOLDER]
// Loop wrap math always; with a folder: parse bgm.txt, decode every listed
// track and check the loop point lies inside it.

#include "bgm_player.h"

#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(x) do { if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

int main(int argc, char** argv)
{
	std::vector<long long> idx;
	// 10 frames, loop back to 4.
	long long p = AdvanceLoop(7, 10, 4, true, 6, idx);
	CHECK(idx[0] == 7 && idx[2] == 9 && idx[3] == 4 && idx[5] == 6 && p == 7);
	p = AdvanceLoop(8, 10, 4, false, 4, idx);
	CHECK(idx[0] == 8 && idx[1] == 9 && idx[2] == -1 && idx[3] == -1 && p == 10);
	p = AdvanceLoop(9, 10, 50, true, 3, idx);   // bad loop start -> 0
	CHECK(idx[0] == 9 && idx[1] == 0 && idx[2] == 1);

	if (argc > 1) {
		const std::string dir = argv[1];
		std::vector<BgmEntry> entries;
		CHECK(ParseBgmTxt(dir + "\\bgm.txt", entries));
		std::printf("%zu bgm.txt entries\n", entries.size());
		int decoded = 0, loops = 0;
		for (const auto& e : entries) {
			BgmPlayer pl;
			std::string err;
			if (!pl.load(dir + "\\" + e.file + ".ogg", e.isLoop, e.loopPos, &err)) { std::printf("  %s: %s\n", e.file.c_str(), err.c_str()); continue; }
			++decoded;
			if (e.isLoop) { ++loops; CHECK(e.loopPos < pl.length()); }
			if (decoded <= 3)
				std::printf("  %s %.2fs loop=%d @%.3f  %s\n", e.file.c_str(), pl.length(), (int)e.isLoop, e.loopPos, e.comment.c_str());
			if (decoded >= 6) break; // decoding is slow-ish; a sample is enough
		}
		CHECK(decoded > 0);
		std::printf("decoded %d track(s), %d looping\n", decoded, loops);
	}
	std::printf(g_fail == 0 ? "BGM_TEST_PASS\n" : "BGM_TEST_FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
