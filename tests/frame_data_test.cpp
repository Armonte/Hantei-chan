// frame_data_test: [authoring] inline frame data (authoring/frame_summary.h) and the MBAC reference values
// (authoring/mbac_reference.h), docs/HANTEI_AUTHORING_MODE.md §8.9.
//   * synthetic patterns: startup / active / recovery, a counted loop, an infinite loop, a jump out, an end into
//     another pattern, a held frame, invulnerability;
//   * MBAC (when the install is on disk): every one of the 22 shared characters has a [TeamChangeData] row, SHIKI is
//     241 / 242, and its tag-in pattern summarizes from the .DAT through the editor's HA4 loader;
//   * MBAACC (when C:\games\mbaacc_dev\data\shiki_0.txt exists): a real pattern through the .txt stack, read-only.
#include "authoring/frame_summary.h"
#include "authoring/mbac_reference.h"
#include "authoring/roster_mirror.h"
#include "framedata.h"
#include "tag_tuning/tag_levers.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace authoring;

static int g_pass = 0, g_fail = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECKM(c, m) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s  (%s)\n", __FILE__, __LINE__, #c, std::string(m).c_str()); } } while (0)

// f(duration, aniType, attack?, hurt?)
static Frame F(int dur, int aniType, bool attack, bool hurt = true)
{
	Frame f;
	f.AF.duration = dur;
	f.AF.aniType = aniType;
	f.AS.invincibility = 0;
	if (hurt) f.hitboxes[1] = Hitbox{ { -10, -80, 10, 0 } };
	if (attack) f.hitboxes[25] = Hitbox{ { 0, -60, 40, -40 } };
	f.hitboxes[0] = Hitbox{ { -8, -60, 8, 0 } };
	return f;
}

static void Put(FrameData& fd, int p, std::vector<Frame> frames, const char* name = "test")
{
	if ((int)fd.m_sequences.size() <= p) fd.m_sequences.resize(p + 1);
	Sequence& s = fd.m_sequences[p];
	s.name = name;
	s.frames = std::move(frames);
	s.initialized = true;
}

static void TestSynthetic()
{
	FrameData fd;
	// 3 startup frames (2+3+4 = 9 ticks), 2 active frames (2+1), 3 recovery (5+5+4), ends into pattern 0
	{
		std::vector<Frame> v = { F(2, 1, false), F(3, 1, false), F(4, 1, false), F(2, 1, true), F(1, 1, true),
		                         F(5, 1, false), F(5, 1, false), F(4, 0, false) };
		v.back().AF.jump = 0;
		Put(fd, 10, v, "5A");
	}
	MoveSummary m = SummarizePattern(fd, 10);
	CHECK(m.valid && m.frames == 8 && m.name == "5A");
	CHECK(m.totalTicks == 26);
	CHECK(m.startup == 9);            // first active tick = 10 (1-based)
	CHECK(m.active == 3);
	CHECK(m.recovery == 14);
	CHECK(m.activeText == "10-12");
	CHECK(!m.loops && m.note == "ends into pattern 0");
	CHECK(m.strip.size() == 8 && m.strip[3].attack && m.strip[3].startTick == 9 && !m.strip[0].attack);
	CHECK(m.Text() == "startup 9, active 10-12 (3), recovery 14, total 26");

	// two separate active ranges; duration 0 counts as one tick
	{
		std::vector<Frame> v = { F(0, 1, false), F(2, 1, true), F(3, 1, false), F(1, 1, true), F(2, 0, false) };
		Put(fd, 11, v);
	}
	m = SummarizePattern(fd, 11);
	CHECK(m.totalTicks == 9 && m.startup == 1 && m.activeText == "2-3, 7" && m.active == 6 && m.recovery == 2);

	// counted loop: frame 1..2 loop 3 times (loopCount set on frame 0, entered once), then loopEnd -> frame 3
	{
		std::vector<Frame> v = { F(1, 1, false), F(2, 1, true), F(2, 2, false), F(3, 0, false) };
		v[0].AF.loopCount = 3;
		v[2].AF.aniFlag = 2;      // counted: while the counter is non-zero jump, then loopEnd
		v[2].AF.jump = 1;
		v[2].AF.loopEnd = 3;
		v[3].AF.jump = 0;
		Put(fd, 12, v);
	}
	m = SummarizePattern(fd, 12);
	// visits: f0(1) f1 f2 (loop 1: counter 3->2 jump) f1 f2 (2->1 jump) f1 f2 (1->0 -> loopEnd) f3(3)
	CHECKM(!m.loops && m.totalTicks == 1 + 3 * 4 + 3, m.note + " total " + std::to_string(m.totalTicks));
	CHECK(m.startup == 1 && m.activeText == "2-3, 6-7, 10-11");

	// an infinite loop (aniType 2 without aniFlag 2): stops at the first repeat
	{
		std::vector<Frame> v = { F(2, 1, false), F(3, 1, true), F(4, 2, false) };
		v[2].AF.jump = 1;
		Put(fd, 13, v);
	}
	m = SummarizePattern(fd, 13);
	CHECK(m.loops && m.totalTicks == 9 && m.note == "loops at frame 1" && m.startup == 2 && m.recovery == 4);

	// the end restarts the pattern itself (aniType 0 with aniFlag 4, jump 0): a loop
	{
		std::vector<Frame> v = { F(4, 1, false), F(4, 0, false) };
		v[1].AF.aniFlag = 4;
		Put(fd, 14, v);
	}
	m = SummarizePattern(fd, 14);
	CHECK(m.loops && m.totalTicks == 8 && m.startup == -1 && m.Text().find("no attack box") == 0);

	// jump out of range, a held frame, and running off the end
	{
		std::vector<Frame> v = { F(2, 1, false), F(2, 2, false) };
		v[1].AF.jump = 9;
		Put(fd, 15, v);
		std::vector<Frame> h = { F(2, 1, false), F(5, 3, true) };
		Put(fd, 16, h);
		std::vector<Frame> r = { F(2, 1, false), F(2, 1, false) };
		Put(fd, 17, r);
	}
	m = SummarizePattern(fd, 15);
	CHECK(m.totalTicks == 4 && m.note.find("jumps out") == 0);
	m = SummarizePattern(fd, 16);
	CHECK(m.totalTicks == 7 && m.note.find("holds frame 1") == 0 && m.startup == 2 && m.active == 5 && m.recovery == 0);
	m = SummarizePattern(fd, 17);
	CHECK(m.totalTicks == 4 && m.note == "ends after the last frame");

	// invulnerability: no hurt box, or AS invincibility 3 (strike invincible)
	{
		std::vector<Frame> v = { F(2, 1, false, false), F(2, 1, false), F(2, 0, false) };
		v[1].AS.invincibility = 3;
		Put(fd, 18, v);
	}
	m = SummarizePattern(fd, 18);
	CHECK(m.strip.size() == 3 && m.strip[0].invuln && m.strip[1].invuln && !m.strip[2].invuln && m.strip[0].boxes == 1);

	// missing / empty patterns
	CHECK(!SummarizePattern(fd, 999).valid && !SummarizePattern(fd, -1).valid && !SummarizePattern(fd, 5).valid);
}

static void TestMbacGlobals()
{
	for (const MbacGlobalRef& r : MbacGlobalReference()) {
		const int li = tagtune::FindLever(r.key);
		CHECKM(li >= 0, r.key);
		if (li >= 0) CHECKM(r.value >= tagtune::kLevers[li].lo && r.value <= tagtune::kLevers[li].hi, r.key);
		CHECK(r.source && r.source[0]);
	}
	CHECK(FindMbacGlobal("cooldownTicks") && FindMbacGlobal("cooldownTicks")->value == 120);
	CHECK(FindMbacGlobal("COOLDOWNTICKS") && !FindMbacGlobal("tagIn"));
}

static void TestMbacData()
{
	const std::string dir = DefaultMbacDir();
	std::error_code ec;
	if (!std::filesystem::exists(dir + "\\SHIKI_C.TXT", ec)) {
		std::printf("MBAC install not found at %s: MBAC data checks skipped\n", dir.c_str());
		return;
	}
	int rows = 0, dats = 0;
	for (const RosterMirrorRow& row : kRosterMirror) {
		if (!MbacStemForChara(row.chara)[0]) continue;
		const MbacCharRef& r = MbacReferenceFor(dir, row.chara);
		CHECKM(r.ok && r.tagIn > 0 && r.tagOut > 0, r.stem + ": " + r.error);
		rows += r.ok;
		dats += r.in.valid;
	}
	CHECK(rows == 22);
	const MbacCharRef& shiki = MbacReferenceFor(dir, 7);
	CHECK(shiki.stem == "SHIKI" && shiki.tagIn == 241 && shiki.tagOut == 242);
	CHECKM(shiki.in.valid && shiki.in.frames > 0 && shiki.in.totalTicks > 0, shiki.error);
	CHECK(&MbacReferenceFor(dir, 7) == &shiki);   // cached
	const MbacCharRef& akiha = MbacReferenceFor(dir, 3);
	CHECK(akiha.tagIn == 177 && akiha.tagOut == 178);
	const MbacCharRef& ciel = MbacReferenceFor(dir, 2);
	CHECK(ciel.tagIn == 243 && ciel.tagOut == 244);
	CHECK(!MbacReferenceFor(dir, 30).ok);   // Ries: no MBAC counterpart
	std::printf("MBAC: 22 [TeamChangeData] rows, %d tag-in patterns summarized; SHIKI 241: %s\n", dats, shiki.in.Text().c_str());
}

static void TestMbaacc()
{
	const std::string txt = "C:\\games\\mbaacc_dev\\data\\shiki_0.txt";
	std::ifstream f(txt, std::ios::binary);
	if (!f) { std::printf("%s not found: MBAACC pattern check skipped\n", txt.c_str()); return; }
	// the .txt stack ([DataFile] File00..), loaded in order like LoadFromIni (read-only)
	std::string line;
	std::vector<std::string> files;
	bool inData = false;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!line.empty() && line[0] == '[') { inData = line == "[DataFile]"; continue; }
		if (inData && line.compare(0, 4, "File") == 0 && line.compare(0, 7, "FileNum") != 0) {
			const size_t eq = line.find('=');
			if (eq != std::string::npos) files.push_back(line.substr(eq + 1));
		}
	}
	FrameData fd;
	bool ok = !files.empty();
	for (size_t i = 0; i < files.size() && ok; ++i) ok = fd.load(("C:\\games\\mbaacc_dev\\data\\" + files[i]).c_str(), i > 0);
	CHECK(ok);
	if (!ok) return;
	const MoveSummary tagIn = SummarizePattern(fd, 241), a5 = SummarizePattern(fd, 1);
	CHECK(tagIn.valid && tagIn.totalTicks > 0);
	CHECK(a5.valid && a5.startup > 0 && a5.active > 0);
	std::printf("MBAACC shiki_0: 241 %s; 1 %s\n", tagIn.Text().c_str(), a5.Text().c_str());
}

int main()
{
	TestSynthetic();
	TestMbacGlobals();
	TestMbacData();
	TestMbaacc();
	std::printf("frame_data_test: %d passed, %d failed -> %s\n", g_pass, g_fail, g_fail ? "FAIL" : "PASS");
	return g_fail ? 1 : 0;
}
