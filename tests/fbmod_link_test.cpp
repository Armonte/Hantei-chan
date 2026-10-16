// fbmod pipe reply parsing (src/squirrel/fbmod_link.h): reloadchara engine rebuild / live / refusals.
#include "squirrel/fbmod_link.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main()
{
	using namespace sqedit;
	CHECK(FbmodReloadRequest("chr000", false) == "reloadchara chr000");
	CHECK(FbmodReloadRequest("chr012", true) == "reloadchara chr012 live");

	// Engine rebuild, success (fbmod core/hotreload.cpp format).
	auto a = FbmodParseReloadReply("OK engine rebuild chr000: chars 0/0 stage 3 quick 1; ha6 root 1A2B3C4D -> 0BADF00D; back in battle after 1840 ms");
	CHECK(a.engine && !a.refused && a.hasRoots && a.backInBattle);
	CHECK(a.rootBefore == 0x1A2B3C4Du && a.rootAfter == 0x0BADF00Du && a.ms == 1840);

	// Engine rebuild that did not come back.
	auto b = FbmodParseReloadReply("ERROR engine rebuild chr000: chars 0/0; ha6 root 1A2B3C4D -> 00000000; NOT back in battle after 120003 ms");
	CHECK(b.engine && !b.refused && b.hasRoots && !b.backInBattle && b.ms == 120003);

	// Refusals outside a training battle.
	CHECK(FbmodParseReloadReply("ERROR reloadchara refused in session Offline: the engine rebuild is the training menu's character switch (training battle only)").refused);
	CHECK(FbmodParseReloadReply("ERROR not in a battle (start training first)").refused);
	CHECK(FbmodParseReloadReply("ERROR timeout: no battle frame within 5 s (is a battle running?)").refused);
	CHECK(!FbmodParseReloadReply("ERROR bad character folder name").refused);

	// Live reload reply: no roots, no timing.
	auto c = FbmodParseReloadReply("OK resources: chr000 P1 (slot 0): ha6 cg pat pal | scripts: OK reloaded chr000");
	CHECK(!c.engine && !c.refused && !c.hasRoots && c.ms == -1);

	if (g_fail) return 1;
	std::printf("fbmod_link_test: ok\n");
	return 0;
}
