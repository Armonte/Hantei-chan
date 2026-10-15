// MBAC (HA4) -> MBAACC (HA6) converter regression: the behaviour fixes of
// docs/mbac_moon/CONVERTER_FIDELITY.md (M1 parts-frame box scale, M2/M9 EF
// positions, M3 ATVV[2], M7 reaction patterns, M8 IF 51, M12 EF p2 bits).
// Unit part runs everywhere; the data part runs when the MBAC .DAT files are
// present (HA4CONV_DAT_DIR or /mnt/c/dev/hantei-chan/gamedata/mbac/02 as
// C:\dev\hantei-chan\gamedata\mbac\02) and is reported as skipped otherwise.
#include "framedata.h"
#include "framedata_ha4.h"
#include "ha4_convert.h"
#include "misc.h"
#include <glad/glad.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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


// --- S8b: ActCadenza CT guts, command file, spawn-leak no-ops, giant reactions ---
static std::vector<uint8_t> MakeCt(size_t n, const float f[5], int air, int t0, int t1)
{
	std::vector<uint8_t> d(n, 0);
	d[ha4conv::CtStatus::kStatusOff] = (uint8_t)air;
	d[ha4conv::CtStatus::kStatusOff + 5] = 0x39;
	std::memcpy(&d[ha4conv::CtStatus::kFloatOff], f, 5 * sizeof(float));
	int32_t t[2] = {t0, t1};
	std::memcpy(&d[ha4conv::CtStatus::kTeamOff], t, sizeof t);
	return d;
}

static void TestActCadenzaData()
{
	namespace fs = std::filesystem;
	// CT: guts are f1..f4 (CT+0x2270..), f0 is never read.
	const float f[5] = {1.0f, 0.92f, 0.85f, 0.8f, 0.75f};
	auto ct = MakeCt(ha4conv::CtStatus::kSize, f, 1, 177, 178);
	ha4conv::CtStatus st;
	CHECK(ha4conv::ParseCtStatus(ct.data(), ct.size(), st));
	CHECK(st.guts(0) == 0.92f && st.guts(3) == 0.75f && st.airJumps() == 1 && st.team[0] == 177 && st.team[1] == 178);
	auto old = MakeCt(8908, f, 1, 0, 0);   // GAKIHA / F_CIEL
	CHECK(ha4conv::ParseCtStatus(old.data(), old.size(), st) && st.guts(1) == 0.85f);
	CHECK(!ha4conv::ParseCtStatus(ct.data(), 100, st));
	ha4conv::AcVersion v;
	CHECK(ha4conv::ParseAcVersion("1.03A", v) && v == ha4conv::AcVersion::V103A);
	CHECK(ha4conv::ParseAcVersion("1.00", v) && v == ha4conv::AcVersion::V100);
	CHECK(!ha4conv::ParseAcVersion("2.0", v));

	// Command file: [Status] from the CT, not from the TXT row.
	fs::path tmp = fs::temp_directory_path() / "ha4_convert_test_s8b";
	std::error_code ec;
	fs::create_directories(tmp, ec);
	{
		std::string txt = "// cmd\r\n 5 41236C 20000101 00000001 219 65535 0 0 0\r\nEND\r\n[TeamChangeData]\r\n177 178\r\n62 0\r\n63 0\r\n"
		                  "1 9 0 1 00111001 1.0 0.9 0.85 0.8 0.8\r\nEND\r\n0 55 0 0 0 0 7 16\r\n";
		std::string tp = (tmp / "X_C.TXT").string(), op = (tmp / "x_cc_c.txt").string();
		WriteFileAtomic(tp.c_str(), txt.data(), txt.size());
		ha4conv::ParseCtStatus(ct.data(), ct.size(), st);
		std::vector<std::string> log;
		CHECK(ha4conv::EmitCommandFile(tp, &st, "test CT", op, &log));
		char *d = nullptr; unsigned n = 0;
		CHECK(ReadInMem(op.c_str(), d, n));
		std::string o(d ? d : "", n); delete[] d;
		CHECK(o.find("Guard = 0.92, 0.85, 0.8, 0.75") != std::string::npos);
		CHECK(o.find("test = 177, 178") != std::string::npos);
		CHECK(o.find("Ground = 62,0") != std::string::npos && o.find("Air = 63,0") != std::string::npos);
		CHECK(o.find("AirJumpNum = 1") != std::string::npos);
	}
	fs::remove_all(tmp, ec);

	// Spawn leaks: target empty here, present in the MBAACC stack -> no-op; others untouched.
	{
		FrameData fd; fd.initEmpty(256);
		fd.m_sequences[0] = MakeSeq(2, "stand");
		fd.m_sequences[50] = MakeSeq(1, "own effect");
		Sequence sp = MakeSeq(2, "spawner");
		Frame_EF e{}; e.type = 1;
		e.number = 129; sp.frames[0].EF.push_back(e);   // leak
		e.number = 50;  sp.frames[0].EF.push_back(e);   // resolves in the ActCadenza data
		e.number = 77;  sp.frames[1].EF.push_back(e);   // dangling in both: spawns nothing anywhere
		fd.m_sequences[195] = sp;
		FrameData cc; cc.initEmpty(1000);
		cc.m_sequences[129] = MakeSeq(3, "MBAACC bird");
		cc.m_sequences[990] = MakeSeq(1, "taken");      // forces the next free slot
		std::vector<std::pair<int, int>> leaks;
		int noop = ha4conv::RewriteSpawnLeaks(fd, cc, 990, &leaks, nullptr);
		CHECK(noop == 989);
		CHECK(leaks.size() == 1 && leaks[0].first == 195 && leaks[0].second == 129);
		CHECK(fd.m_sequences[195].frames[0].EF[0].number == 989);
		CHECK(fd.m_sequences[195].frames[0].EF[1].number == 50);
		CHECK(fd.m_sequences[195].frames[1].EF[0].number == 77);
		Sequence *n = fd.get_sequence(989);
		CHECK(n && n->frames.size() == 1 && n->frames[0].AF.aniType == 0 && n->frames[0].AF.layers[0].spriteId == -1);
		FrameData clean; clean.initEmpty(256); clean.m_sequences[1] = MakeSeq(1, "x");
		CHECK(ha4conv::RewriteSpawnLeaks(clean, cc, 990, nullptr, nullptr) == -1);
	}
	// Giant reactions (GiantFlag 2 like MBAACC G_AKIHA; 1 like HERMES).
	{
		FrameData fd; fd.initEmpty(256);
		fd.m_sequences[0] = MakeSeq(3, "stand");
		fd.m_sequences[23] = MakeSeq(2, "head");
		Sequence body = MakeSeq(5, "body");
		body.frames[0].AF.layers[0].spriteId = 16; body.frames[0].AF.layers[0].usePat = true;
		body.frames[0].hitboxes[1] = Hitbox{{-100, -400, 100, 0}};
		fd.m_sequences[24] = body;
		FrameData donor; donor.initEmpty(400);
		for (int p : {23, 24, 25, 26, 44, 45, 350}) {
			Sequence d = MakeSeq(p == 44 ? 9 : 4, "donor");
			for (auto &fr : d.frames) { fr.AF.layers[0].spriteId = 250; fr.hitboxes[1] = Hitbox{{-1, -1, 1, 1}}; }
			Frame_EF e{}; e.type = 1; e.number = 123; d.frames[0].EF.push_back(e);   // donor-only effect
			donor.m_sequences[p] = d;
		}
		FrameData g1 = fd;
		auto added = ha4conv::AddGiantReactions(fd, donor, 2, nullptr);
		for (int p : {25, 26, 44, 45, 350, 900, 905, 908, 351, 354}) CHECK(fd.get_sequence(p) && !fd.get_sequence(p)->frames.empty());
		CHECK(fd.get_sequence(23)->frames.size() == 2);   // own slots are kept
		Sequence *ko = fd.get_sequence(44);
		CHECK(ko && ko->frames.size() == 9 && ko->frames[0].AF.layers[0].spriteId == 16 && ko->frames[0].AF.layers[0].usePat);
		CHECK(ko && ko->frames[0].EF.empty() && ko->frames[0].hitboxes.count(1) && ko->frames[0].hitboxes[1].xy[0] == -100);
		CHECK(std::find(added.begin(), added.end(), 44) != added.end());
		auto a1 = ha4conv::AddGiantReactions(g1, donor, 1, nullptr);
		CHECK(a1.size() == 2 && g1.get_sequence(44) && !g1.get_sequence(44)->frames.empty());
		CHECK(!g1.get_sequence(900) || g1.get_sequence(900)->frames.empty());
	}
}

static std::string PatchDir()
{
	if (const char *e = std::getenv("HA4CONV_AC_PATCH_DIR")) return e;
	for (const char *d : {"C:/dev/hantei-chan/gamedata/mbac_v11_english/06", "/mnt/c/dev/hantei-chan/gamedata/mbac_v11_english/06"})
		if (std::filesystem::exists(std::string(d) + "/AKIHA_C.CT")) return d;
	return {};
}

static std::string MbaaccDir()
{
	if (const char *e = std::getenv("HA4CONV_MBAACC_DIR")) return e;
	for (const char *d : {"C:/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002", "/mnt/c/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002"})
		if (std::filesystem::exists(std::string(d) + "/g_akiha.HA6")) return d;
	return {};
}

// Real data: AKIHA guts from the 1.03A CT (0.9 -> 0.92 at full HP), 129/130 leaks; GAKIHA giant slots.
static void TestActCadenzaRealData()
{
	std::string dir = DatDir(), patch = PatchDir(), cc = MbaaccDir();
	if (dir.empty() || patch.empty() || cc.empty()) { std::printf("  actcadenza data: SKIP (no .DAT / 06.p / MBAACC folder)\n"); return; }
	namespace fs = std::filesystem;
	fs::path tmp = fs::temp_directory_path() / "ha4_convert_test_s8b_data";
	std::error_code ec;
	fs::create_directories(tmp, ec);
	for (auto ver : {ha4conv::AcVersion::V103A, ha4conv::AcVersion::V100}) {
		ha4conv::Options o; o.outDir = tmp.string(); o.writeCG = o.writePat = o.writePal = o.writeTxt = false;
		o.acVersion = ver; o.acPatchDir = patch; o.mbaaccDataDir = cc;
		ha4conv::Report rep;
		CHECK(ha4conv::ConvertFile(dir + "/AKIHA.DAT", o, rep));
		CHECK(rep.guts[0] == (ver == ha4conv::AcVersion::V103A ? 0.92f : 0.9f));
		CHECK(rep.guts[1] == 0.85f);
		CHECK(rep.noopUsed == 990);
		bool l129 = false, l130 = false;
		for (auto &l : rep.leaks) { l129 |= l.second == 129; l130 |= l.second == 130; }
		CHECK(l129 && l130);
		CHECK(rep.exShield[0] == 6 && rep.exShield[1] == 6 && rep.exShield[2] == 10);
		CHECK(!rep.sidecarPath.empty() && fs::exists(rep.sidecarPath));
		FrameData h;
		CHECK(h.load(rep.ha6Path.c_str()));
		int bad = 0;
		for (int p = 0; p < h.get_sequence_count(); p++) {
			Sequence *s = h.get_sequence(p);
			if (s) for (auto &f : s->frames) for (auto &e : f.EF) if (e.type == 1 && (e.number == 129 || e.number == 130)) bad++;
		}
		CHECK(bad == 0);
		CHECK(h.get_sequence(990) && h.get_sequence(990)->frames.size() == 1);
	}
	{
		ha4conv::Options o; o.outDir = tmp.string(); o.writeCG = o.writePat = o.writePal = o.writeTxt = false;
		o.giantFlag = 2; o.giantDonorHa6 = cc + "/g_akiha.HA6"; o.acPatchDir = patch;
		ha4conv::Report rep;
		CHECK(ha4conv::ConvertFile(dir + "/GAKIHA.DAT", o, rep));
		CHECK(rep.guts[0] == 0.55f);
		FrameData h;
		CHECK(h.load(rep.ha6Path.c_str()));
		for (int p : {23, 24, 25, 26, 44, 45, 900, 908, 351, 354}) CHECK(h.get_sequence(p) && !h.get_sequence(p)->frames.empty());
	}
	fs::remove_all(tmp, ec);
	std::printf("  actcadenza data: AKIHA 1.03A/1.00 + GAKIHA checked\n");
}

static void APIENTRY NoGlDeleteBuffers(GLsizei, const GLuint *) {}

int main()
{
	if (!glad_glDeleteBuffers) glad_glDeleteBuffers = NoGlDeleteBuffers;
	TestBoxes();
	TestEfPositions();
	TestPrepare();
	TestData();
	TestActCadenzaData();
	TestActCadenzaRealData();
	std::printf("ha4_convert_test: %d checks, %d failed\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
