// MBAC (HA4) -> MBAACC (HA6) converter regression: the behaviour fixes of
// docs/mbac_moon/CONVERTER_FIDELITY.md (M1 parts-frame box scale, M2/M9 EF
// positions, M3 ATVV[2], M7 reaction patterns, M8 IF 51, M12 EF p2 bits).
// Unit part runs everywhere; the data part runs when the MBAC .DAT files are
// present (HA4CONV_DAT_DIR or /mnt/c/dev/hantei-chan/gamedata/mbac/02 as
// C:\dev\hantei-chan\gamedata\mbac\02) and is reported as skipped otherwise.
#include "framedata.h"
#include "framedata_ha4.h"
#include "ha4_convert.h"
#include <glad/glad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool Eq4(const int a[4], int x0, int y0, int x1, int y1) { return a[0] == x0 && a[1] == y0 && a[2] == x1 && a[3] == y1; }

static void TestBoxes()
{
	// G_CHAOS worked cases (CONVERTER_FIDELITY.md M1): parts frame -> (x-320, y-448).
	const int16_t arcDrive[4] = {190, 23, 704, 454}, push[4] = {272, 189, 356, 447};
	int m[4];
	ha4::BoxToModel(arcDrive, true, m);
	CHECK(Eq4(m, -130, -425, 384, 6));
	ha4::BoxToModel(push, true, m);
	CHECK(Eq4(m, -48, -259, 36, -1));
	int16_t back[4];
	ha4::BoxFromModel(m, true, back);
	CHECK(!std::memcmp(back, push, sizeof back));
	// CG frames unchanged: (x-128, y-224).
	ha4::BoxToModel(push, false, m);
	CHECK(Eq4(m, 144, -35, 228, 223));
	// Odd raw values survive (the old (x>>1)-160 rule lost the low bit).
	const int16_t odd[4] = {321, 449, 323, 451};
	ha4::BoxToModel(odd, true, m);
	ha4::BoxFromModel(m, true, back);
	CHECK(!std::memcmp(back, odd, sizeof back));
}

static void Ef(int type, bool parts, int x, int y, int p2, int p3, int wx, int wy)
{
	int raw[12] = {x, y, p2, p3, 5, 6, 7, 8, 9, 10, 11, 12}, mod[12], back[12];
	ha4::EfParamsToModel(type, parts, raw, mod);
	g_checks++;
	if (mod[0] != wx || mod[1] != wy || mod[2] != p2 || mod[3] != p3 || mod[4] != 5) {
		g_fail++;
		std::fprintf(stderr, "FAIL EF%d parts=%d (%d,%d) p2=%#x p3=%#x -> (%d,%d), want (%d,%d)\n", type, parts, x, y, p2, p3, mod[0], mod[1], wx, wy);
	}
	ha4::EfParamsFromModel(type, parts, mod, back);
	if (!(parts && (p2 & 0x80) && !(p3 & 0x200) && (y & 1))) CHECK(!std::memcmp(back, raw, sizeof raw));
}

static void TestEfPositions()
{
	// CG frames, mbacPC EffectObject_InitFromParent 0x423140 / MBAA Effect_InitializeComplex 0x454900
	Ef(1, false, 58, 0, 0, 0, -70, -224);          // plain: (128,224) bias, raw 0 included (FB nero p50)
	Ef(1, false, 48, 0, 0x10, 0, -80, 0);          // screen-relative: x-128, y unbiased (FB akaakiha p217)
	Ef(1, false, 0, 224, 0, 0x100, 0, 0);          // target-relative x: unbiased (FB akiha p161)
	Ef(1, false, 300, 100, 0, 0x200, 300, 100);    // absolute: both engines subtract 128 themselves
	Ef(8, false, 128, 0, 0x80, 0, 0, 0);           // p2 bit7 absolute y (FB warakia p49)
	Ef(11, false, 350, 0, 0, 0, 222, -224);        // EF11 plain (FB satsuki p124)
	// parts frames (spawner sprite < 10000): (320,448), <<7 in both engines (M2)
	Ef(1, true, 320, 448, 0, 0, 0, 0);
	Ef(11, true, 400, 500, 0, 0, 80, 52);
	Ef(1, true, 300, 100, 0x10, 0, 44, 100);        // screen-relative parts: x-256
	Ef(1, true, 320, 100, 0x80, 0, 0, 50);          // absolute y: mbacPC p1<<7 vs MBAA p1<<8
	// EF 2/3/4 (Actor_FrameOffsetToWorld 0x423AB0): frame rule only
	Ef(3, false, 0, 0, 0x1e, 0, -128, -224);
	Ef(4, true, 0, 0, 0, 0x15, -320, -448);
	// EF 5/6/9 carry no position bias
	Ef(9, false, 12, 34, 0, 0, 12, 34);
}

static Sequence MakeSeq(int frames, const char *name)
{
	Sequence s;
	s.name = name;
	s.initialized = true;
	s.empty = false;
	s.frames.resize(frames);
	for (auto &f : s.frames) { f.AF.layers.resize(1); f.AF.duration = 3; f.AF.aniType = 1; }
	return s;
}

static void TestPrepare()
{
	FrameData fd;
	fd.initEmpty(256);
	for (int p : {17, 18, 19, 23, 24, 25, 26}) fd.m_sequences[p] = MakeSeq(p == 23 ? 8 : 6, "react");
	// Shield like SION p65: frame 0 is the EX window (p0 -1), release frame 3, success 5, EX follow-up frame 11
	Sequence sh = MakeSeq(12, "shield");
	Frame_IF c{}; c.type = 51; c.parameters[0] = -1; c.parameters[1] = 5; c.parameters[2] = 11;
	sh.frames[0].IF.push_back(c);
	sh.frames[0].AF.duration = 6;
	c.parameters[0] = 3;
	sh.frames[1].IF.push_back(c);
	sh.frames[2].IF.push_back(c);
	sh.frames[11].AF.aniType = 0; sh.frames[11].AF.jump = 130; sh.frames[11].AF.duration = 1;
	fd.m_sequences[65] = sh;
	// An attack frame with an MBAC stun value, and an EF1 with p2 bits MBAA reads but MBAC does not.
	Sequence atk = MakeSeq(2, "attack");
	atk.frames[1].hitboxes[25] = Hitbox{{-10, -50, 40, 0}};
	atk.frames[1].AT.guard_damage = 37;
	atk.frames[1].AT.damage = 800;
	Frame_EF e{}; e.type = 1; e.number = 125; e.parameters[2] = 0x258;
	atk.frames[1].EF.push_back(e);
	fd.m_sequences[100] = atk;

	FrameData raw = fd;
	std::vector<std::string> log;
	ha4conv::PrepareForHA6(fd, ha4conv::Options(), &log);
	for (auto &l : log) std::printf("  log: %s\n", l.c_str());

	// M7
	CHECK(fd.get_sequence_count() >= 909);
	for (auto &r : ha4conv::SystemPatternRules()) {
		Sequence *t = fd.get_sequence(r.target);
		CHECK(t && t->frames.size() == raw.m_sequences[r.source].frames.size());
	}
	// M8
	auto &shf = fd.m_sequences[65].frames;
	CHECK(shf[0].IF[0].parameters[0] == 3);
	CHECK(shf[0].IF[0].parameters[1] == 5);
	CHECK(shf[0].IF[0].parameters[2] == 130 && shf[1].IF[0].parameters[2] == 130);
	// M3
	CHECK(fd.m_sequences[100].frames[1].AT.guard_damage == 0);
	CHECK(fd.m_sequences[100].frames[1].AT.damage == 800);
	// M12
	CHECK(fd.m_sequences[100].frames[1].EF[0].parameters[2] == 0x58);

	// switches off -> verbatim
	FrameData off = raw;
	ha4conv::Options o;
	o.systemPatterns = o.shieldRemap = o.efClearCcOnlyBits = false;
	o.guardDamage = ha4conv::GuardDamage::Raw;
	ha4conv::PrepareForHA6(off, o, nullptr);
	CHECK(!off.get_sequence(900) || off.get_sequence(900)->frames.empty());
	CHECK(off.m_sequences[65].frames[0].IF[0].parameters[0] == -1);
	CHECK(off.m_sequences[100].frames[1].AT.guard_damage == 37);

	// non-character data (EFFECT.DAT, CSEL_*) gets no reaction patterns
	FrameData eff;
	eff.initEmpty(256);
	eff.m_sequences[23] = MakeSeq(3, "effect");
	ha4conv::PrepareForHA6(eff, ha4conv::Options(), nullptr);
	CHECK(eff.get_sequence_count() == 256);

	// usePat box display scale (MBAA family only)
	Frame pf; pf.AF.layers.resize(1); pf.AF.layers[0].usePat = true;
	CHECK(fd.boxDisplayScale(pf) == 0.5f);
	pf.AF.layers[0].usePat = false;
	CHECK(fd.boxDisplayScale(pf) == 1.f);
}

static std::string DatDir()
{
	if (const char *e = std::getenv("HA4CONV_DAT_DIR")) return e;
	for (const char *d : {"C:/dev/hantei-chan/gamedata/mbac/02", "/mnt/c/dev/hantei-chan/gamedata/mbac/02"})
		if (std::filesystem::exists(std::string(d) + "/G_CHAOS.DAT")) return d;
	return {};
}

static void TestData()
{
	std::string dir = DatDir();
	if (dir.empty()) { std::printf("  data: SKIP (no MBAC .DAT folder)\n"); return; }
	namespace fs = std::filesystem;
	fs::path tmp = fs::temp_directory_path() / "ha4_convert_test";
	std::error_code ec;
	fs::create_directories(tmp, ec);

	// G_CHAOS: Arc Drive box at full scale through the whole export + HA6 reload.
	{
		ha4conv::Options o; o.outDir = tmp.string(); o.writeCG = o.writePat = o.writePal = o.writeTxt = false;
		ha4conv::Report rep;
		CHECK(ha4conv::ConvertFile(dir + "/G_CHAOS.DAT", o, rep));
		FrameData h;
		CHECK(h.load(rep.ha6Path.c_str()));
		Sequence *s = h.get_sequence(100);
		CHECK(s && s->frames.size() > 10 && s->frames[10].hitboxes.count(25));
		if (s && s->frames.size() > 10 && s->frames[10].hitboxes.count(25)) CHECK(Eq4(s->frames[10].hitboxes[25].xy, -130, -425, 384, 6));
		s = h.get_sequence(0);
		CHECK(s && !s->frames.empty() && s->frames[0].hitboxes.count(0) && Eq4(s->frames[0].hitboxes[0].xy, -48, -259, 36, -1));
		for (int p : {900, 905, 908}) CHECK(h.get_sequence(p) && !h.get_sequence(p)->frames.empty());
	}
	// SION: shields (65/66/67) in MBAA terms, reaction set, no stun values in ATVV[2].
	{
		ha4conv::Options o; o.outDir = tmp.string(); o.writeCG = o.writePat = o.writePal = o.writeTxt = false;
		ha4conv::Report rep;
		CHECK(ha4conv::ConvertFile(dir + "/SION.DAT", o, rep));
		FrameData h;
		CHECK(h.load(rep.ha6Path.c_str()));
		int if51 = 0, gdNonZero = 0;
		for (int p = 0; p < h.get_sequence_count(); p++) {
			Sequence *s = h.get_sequence(p);
			if (!s) continue;
			for (auto &f : s->frames) {
				for (auto &c : f.IF) if (c.type == 51) {
					if51++;
					CHECK(c.parameters[0] >= 0 && c.parameters[0] < (int)s->frames.size());
					CHECK(c.parameters[2] == 0 || (h.get_sequence(c.parameters[2]) && !h.get_sequence(c.parameters[2])->frames.empty()));
				}
				for (auto &b : f.hitboxes) if (b.first >= 25 && f.AT.guard_damage) { gdNonZero++; break; }
			}
		}
		CHECK(if51 >= 9);
		CHECK(gdNonZero == 0);
		for (auto &r : ha4conv::SystemPatternRules()) CHECK(h.get_sequence(r.target) && !h.get_sequence(r.target)->frames.empty());
		// sion p65 f0: (-1,5,11) -> (3,5,130)
		Sequence *s = h.get_sequence(65);
		CHECK(s && !s->frames.empty() && !s->frames[0].IF.empty());
		if (s && !s->frames.empty() && !s->frames[0].IF.empty())
			CHECK(s->frames[0].IF[0].parameters[0] == 3 && s->frames[0].IF[0].parameters[2] == 130);
	}
	fs::remove_all(tmp, ec);
	std::printf("  data: G_CHAOS + SION checked (%s)\n", dir.c_str());
}

static void APIENTRY NoGlDeleteBuffers(GLsizei, const GLuint *) {}

int main()
{
	if (!glad_glDeleteBuffers) glad_glDeleteBuffers = NoGlDeleteBuffers;
	TestBoxes();
	TestEfPositions();
	TestPrepare();
	TestData();
	std::printf("ha4_convert_test: %d checks, %d failed\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
