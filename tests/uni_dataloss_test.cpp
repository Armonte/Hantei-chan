// UNI2/MBTL/UNIB data-loss fixes (parity push S3, D13-D18):
//   D13 pasted data is converted to the file's dialect; detection is fixed at load
//   D14 MBAACC-only AT fields are not written by the UNI writer (and are cleared on paste)
//   D15 10 condition parameters; overflow keeps the ones the game keeps
//   D16 layer-aware range paste, Delete Layer keeps ids
//   D17 BaseData PFLG template detection + Han6_MergeTemplatePattern_PFLG simulation
//   D18 UNIB detection and UNIB move-script lookup
// Synthetic cases always run. Game-data cases run when the data is present
// (GAMEDATA, default /mnt/c/dev/hantei-chan/gamedata as C:/dev/hantei-chan/gamedata,
// and UNIB at C:/games/unib/data) and are skipped otherwise.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "framedata.h"
#include "framedata_labels.h"
#include "mv_script.h"
#include "uni_vector_table.h"

namespace fs = std::filesystem;
static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::string ReadAll(const std::string& p)
{
	std::ifstream f(fs::u8path(p), std::ios::binary);
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static size_t Count(const std::string& hay, const char* needle)
{
	size_t n = 0, at = 0, len = strlen(needle);
	while ((at = hay.find(needle, at, len)) != std::string::npos) { ++n; at += len; }
	return n;
}
static std::string Tmp(const char* name) { return (fs::temp_directory_path() / name).u8string(); }

static Layer_Type L(int sprite) { Layer_Type l; l.spriteId = sprite; return l; }

// A UNI2-style pattern: 5 AFGX layers, sprites on layers 1 and 4.
static Sequence Uni2Pattern(int frames)
{
	Sequence s;
	s.name = "uni2 pat";
	s.usedAFGX = s.usedATV2 = true;
	s.initialized = true;
	for (int i = 0; i < frames; ++i) {
		Frame f;
		f.AF.duration = 3;
		f.AF.layers = {L(-1), L(10 + i), L(-1), L(-1), L(50)};
		f.AF.frameId = 7;
		f.AT.correction = 100;
		s.frames.push_back(f);
	}
	return s;
}
static Sequence MbaaccPattern()
{
	Sequence s;
	s.name = "mbaacc pat";
	s.initialized = true;
	Frame f;
	f.AF.duration = 4;
	f.AF.layers = {L(3)};
	f.AT.correction = 100;
	f.AT.red_damage = 120; f.AT.guard_damage = 5; f.AT.breakTime = 30; f.AT.extraGravity = 1.5f;
	f.hitboxes[25] = Hitbox{{0, 0, 10, 10}};
	s.frames.push_back(f);
	return s;
}
// A file with `game` forced, holding one pattern of that game at slot 0.
static void MakeFile(FrameData& fd, Ha6Game game)
{
	fd.initEmpty(8);
	fd.setGameOverride(game);
	Sequence& s = *fd.get_sequence(0);
	if (Ha6GameIsUni(game)) {
		s = Uni2Pattern(2);
		for (auto& f : s.frames) f.AF.layers.resize(Ha6GameLayerCount(game));
	} else {
		s = MbaaccPattern();
	}
}

static void TestPasteConversion()
{
	// UNI2 -> MBTL: layers past 3 are dropped (with a report), the file is not
	// padded to 5 layers, and its dialect does not change.
	{
		FrameData fd; MakeFile(fd, Ha6Game::MBTL);
		Sequence p = Uni2Pattern(3);
		Ha6ConvertReport r = fd.conformPasted(p);
		CHECK(r.to == Ha6Game::MBTL);
		CHECK(r.patterns == 1 && r.frames == 3);
		CHECK(r.layersDropped == 6 && r.spriteLayersDropped == 3);
		for (auto& f : p.frames) CHECK(f.AF.layers.size() == 3);
		CHECK(p.frames[0].AF.layers[1].spriteId == 10);
		*fd.get_sequence(1) = p;
		CHECK(fd.game() == Ha6Game::MBTL);
		const std::string out = Tmp("s3_paste_mbtl.ha6");
		CHECK(fd.save(out.c_str()));
		const std::string b = ReadAll(out);
		// 2 + 3 frames, 3 AFGX layers each.
		CHECK(Count(b, "AFGX") == 15);
	}
	// UNI2 -> MBAACC: one AFGP layer (the first one with a sprite), no AFGX/ATV2,
	// UNI-only fields cleared.
	{
		FrameData fd; MakeFile(fd, Ha6Game::MBAACC);
		Sequence p = Uni2Pattern(2);
		Ha6ConvertReport r = fd.conformPasted(p);
		CHECK(!p.usedAFGX && !p.usedATV2);
		for (auto& f : p.frames) { CHECK(f.AF.layers.size() == 1); CHECK(f.AF.frameId == 0); }
		CHECK(p.frames[1].AF.layers[0].spriteId == 11);
		CHECK(r.uniFieldsCleared == 2 && r.spriteLayersDropped == 2);
		*fd.get_sequence(1) = p;
		const std::string out = Tmp("s3_paste_mbaacc.ha6");
		CHECK(fd.save(out.c_str()));
		const std::string b = ReadAll(out);
		CHECK(Count(b, "AFGX") == 0 && Count(b, "ATV2") == 0 && Count(b, "AFID") == 0);
		CHECK(Count(b, "AFGP") == 3);
	}
	// MBAACC -> UNI2: MBAACC-only AT fields are cleared (D14) and reported.
	{
		FrameData fd; MakeFile(fd, Ha6Game::UNI);
		Sequence p = MbaaccPattern();
		Ha6ConvertReport r = fd.conformPasted(p);
		CHECK(p.usedAFGX && p.usedATV2);
		CHECK(r.mbaaccFieldsCleared == 1);
		CHECK(p.frames[0].AT.red_damage == 0 && p.frames[0].AT.breakTime == 0 && p.frames[0].AT.extraGravity == 0.f);
		Frame fr = MbaaccPattern().frames[0];
		r = fd.conformPasted(fr);
		CHECK(r.mbaaccFieldsCleared == 1);
		Frame_AT at = MbaaccPattern().frames[0].AT;
		r = fd.conformPastedAT(at);
		CHECK(r.mbaaccFieldsCleared == 1 && at.guard_damage == 0);
	}
	// An empty (new) file adopts the pasted pattern's dialect instead.
	{
		FrameData fd; fd.initEmpty(4);
		CHECK(fd.game() == Ha6Game::MBAACC);
		Sequence p = Uni2Pattern(1);
		Ha6ConvertReport r = fd.conformPasted(p);
		CHECK(r.adopted && fd.game() == Ha6Game::UNI);
		CHECK(p.frames[0].AF.layers.size() == 5);
	}
	// The override is per file.
	{
		FrameData a, b; a.initEmpty(1); b.initEmpty(1);
		a.setGameOverride(Ha6Game::MBTL);
		CHECK(a.game() == Ha6Game::MBTL && b.game() == Ha6Game::MBAACC);
		CHECK(a.uniLayerCountForSave() == 3 && b.uniLayerCountForSave() == 0);
	}
}

static void TestAtFields()
{
	// D14: the UNI writer never emits ATBT/ATUH, nor ATVV (VS/guard damage).
	FrameData fd; MakeFile(fd, Ha6Game::UNI);
	Frame& f = fd.get_sequence(0)->frames[0];
	f.hitboxes[25] = Hitbox{{0, 0, 10, 10}};
	f.AT.breakTime = 30; f.AT.extraGravity = 2.f; f.AT.red_damage = 9; f.AT.guard_damage = 9; f.AT.damage = 800;
	const std::string out = Tmp("s3_at.ha6");
	CHECK(fd.save(out.c_str()));
	const std::string b = ReadAll(out);
	CHECK(Count(b, "ATST") == 1);
	CHECK(Count(b, "ATBT") == 0 && Count(b, "ATUH") == 0 && Count(b, "ATVV") == 0);
	CHECK(Count(b, "ATAT") == 1);
}

static void TestTenConditionParams()
{
	FrameData fd; MakeFile(fd, Ha6Game::UNI);
	Frame_IF c{}; c.type = 56;
	for (int i = 0; i < kMaxIfParams; ++i) c.parameters[i] = 100 + i;
	fd.get_sequence(0)->frames[0].IF.push_back(c);
	Frame_EF e{}; e.type = 1; e.number = 2;
	for (int i = 0; i < kMaxEfParams; ++i) e.parameters[i] = 200 + i;
	fd.get_sequence(0)->frames[0].EF.push_back(e);
	const std::string out = Tmp("s3_if10.ha6");
	CHECK(fd.save(out.c_str()));
	FrameData rd;
	CHECK(rd.load(out.c_str()));
	const Frame& f = rd.get_sequence(0)->frames[0];
	CHECK(f.IF.size() == 1 && f.IF[0].parameters[9] == 109 && f.IF[0].parameters[0] == 100);
	CHECK(f.EF.size() == 1 && f.EF[0].parameters[11] == 211);
	// Byte-exact second save.
	const std::string out2 = Tmp("s3_if10b.ha6");
	CHECK(rd.save(out2.c_str()));
	CHECK(ReadAll(out) == ReadAll(out2));

	// Overflow: IFPR 12 / EFPR 14 keep the first 10 / 12 (what the game keeps)
	// and the rest of the file still parses.
	std::string b = ReadAll(out);
	auto grow = [&](const char* tag, int from, int to) {
		size_t at = b.find(tag);
		CHECK(at != std::string::npos);
		if (at == std::string::npos) return;
		int32_t n; memcpy(&n, &b[at + 4], 4);
		CHECK(n == from);
		memcpy(&b[at + 4], &to, 4);
		std::string extra;
		for (int i = from; i < to; ++i) { int32_t v = 999; extra.append((const char*)&v, 4); }
		b.insert(at + 8 + 4 * from, extra);
	};
	grow("IFPR", 10, 12);
	grow("EFPR", 12, 14);
	const std::string out3 = Tmp("s3_if12.ha6");
	{ std::ofstream o(fs::u8path(out3), std::ios::binary); o << b; }
	FrameData ov;
	CHECK(ov.load(out3.c_str()));
	const Sequence* s = ov.get_sequence(0);
	CHECK(s && s->frames.size() == 2);
	if (s && s->frames.size() == 2) {
		const Frame& g = s->frames[0];
		CHECK(g.IF.size() == 1 && g.IF[0].type == 56 && g.IF[0].parameters[9] == 109);
		CHECK(g.EF.size() == 1 && g.EF[0].number == 2 && g.EF[0].parameters[11] == 211);
		CHECK(s->frames[1].AF.duration == 3);
	}
}

static void TestLayerOps()
{
	Sequence s = Uni2Pattern(4);
	Frame src = s.frames[0];
	src.AF.layers[1].offset_x = 77; src.AF.layers[2].offset_x = 55; src.AF.layers[0].offset_x = 11;
	src.AF.layers[1].afrt = true;
	// Selected layer 1 only.
	PasteLayerRange(s, src, 1, 3, 1, LayerPaste::Offset);
	CHECK(s.frames[2].AF.layers[1].offset_x == 77);
	CHECK(s.frames[2].AF.layers[0].offset_x == 0 && s.frames[2].AF.layers[2].offset_x == 0);
	CHECK(s.frames[0].AF.layers[1].offset_x == 0); // outside the range
	// All layers.
	PasteLayerRange(s, src, 0, 3, -1, LayerPaste::Offset);
	CHECK(s.frames[3].AF.layers[0].offset_x == 11 && s.frames[3].AF.layers[2].offset_x == 55);
	// Rotation copies the layer's AFRT.
	PasteLayerRange(s, src, 2, 2, 1, LayerPaste::Rotation);
	CHECK(s.frames[2].AF.layers[1].afrt && !s.frames[3].AF.layers[1].afrt);
	// Sprite & duration on layer 1 (the MBTL body layer), not layer 0.
	src.AF.layers[1].spriteId = 99; src.AF.duration = 9;
	PasteLayerRange(s, src, 1, 1, 1, LayerPaste::SpriteDuration);
	CHECK(s.frames[1].AF.layers[1].spriteId == 99 && s.frames[1].AF.layers[0].spriteId == -1 && s.frames[1].AF.duration == 9);
	// A frame with fewer layers grows (sprite -1) up to the target id.
	Sequence one; one.frames.resize(1); one.frames[0].AF.layers = {L(1)};
	PasteLayerRange(one, src, 0, 0, 1, LayerPaste::SpriteDuration);
	CHECK(one.frames[0].AF.layers.size() == 2 && one.frames[0].AF.layers[1].spriteId == 99);

	// Delete Layer keeps ids in UNI files.
	Frame_AF af = Uni2Pattern(1).frames[0].AF;
	int sel = DeleteLayerKeepIds(af, 1, true);
	CHECK(af.layers.size() == 5 && af.layers[1].spriteId == -1 && af.layers[4].spriteId == 50 && sel == 1);
	sel = DeleteLayerKeepIds(af, 4, true);
	CHECK(af.layers.size() == 4 && sel == 3);
	Frame_AF mb; mb.layers = {L(1), L(2), L(3)};
	sel = DeleteLayerKeepIds(mb, 0, false);
	CHECK(mb.layers.size() == 2 && mb.layers[0].spriteId == 2);

	// Layer caps per game.
	CHECK(Ha6GameLayerCount(Ha6Game::UNI) == 5 && Ha6GameLayerCount(Ha6Game::UNIST) == 5);
	CHECK(Ha6GameLayerCount(Ha6Game::MBTL) == 3 && Ha6GameLayerCount(Ha6Game::UNIB) == 3);
}

static void TestTemplateMerge()
{
	Sequence tmpl;
	tmpl.flag = 1;
	for (int i = 0; i < 2; ++i) {
		Frame f; f.AF.duration = 5 + i; f.AF.layers = {L(900), L(-1), L(-1)};
		f.AF.layers[0].rgba[0] = 0.5f; f.AS.speed[0] = 1234;
		f.hitboxes[0] = Hitbox{{1, 1, 2, 2}};
		f.ha6.hadAT = (i == 1);
		tmpl.frames.push_back(f);
	}
	Sequence chr;
	for (int i = 0; i < 3; ++i) {
		Frame f; f.AF.duration = 1; f.AF.layers = {L(-1), L(20 + i), L(-1)};
		f.AF.layers[1].offset_x = 40;
		f.hitboxes[1] = Hitbox{{3, 3, 4, 4}};
		f.hitboxes[25] = Hitbox{{5, 5, 6, 6}};
		f.AS.speed[0] = 1;
		chr.frames.push_back(f);
	}
	Sequence m = MergeTemplatePattern(tmpl, chr, 3);
	CHECK(m.frames.size() == 2);                       // template frame count
	CHECK(m.frames[0].AF.duration == 5 && m.frames[1].AF.duration == 6); // template timing
	CHECK(m.frames[0].AS.speed[0] == 1234);            // template AS
	CHECK(m.frames[0].AF.layers[0].spriteId == -1);    // character sprites
	CHECK(m.frames[1].AF.layers[1].spriteId == 21 && m.frames[1].AF.layers[1].offset_x == 40);
	CHECK(m.frames[0].AF.layers[0].rgba[0] == 0.5f);   // template layer colour
	CHECK(m.frames[0].hitboxes.count(1) && !m.frames[0].hitboxes.count(0)); // character hurt boxes
	CHECK(!m.frames[0].hitboxes.count(25));            // no AT on template frame 0
	CHECK(m.frames[1].hitboxes.count(25));             // AT frame takes attack boxes
	CHECK(m.flag == 0);
}

static std::string GameData()
{
	const char* g = getenv("GAMEDATA");
	if (g) return g;
	for (const char* p : {"C:/dev/hantei-chan/gamedata", "/mnt/c/dev/hantei-chan/gamedata"})
		if (fs::exists(fs::u8path(p))) return p;
	return {};
}
static std::string UnibDir()
{
	const char* g = getenv("UNIB_DATA");
	if (g) return g;
	for (const char* p : {"C:/games/unib/data", "/mnt/c/games/unib/data"})
		if (fs::exists(fs::u8path(p))) return p;
	return {};
}
static bool IsHa6(const fs::path& p)
{
	std::string e = p.extension().u8string();
	for (auto& c : e) c = (char)tolower((unsigned char)c);
	if (e != ".ha6") return false;
	std::ifstream f(p, std::ios::binary); char h[15] = {};
	f.read(h, 15);
	return !memcmp(h, "Hantei6DataFile", 15);
}

static void TestDetection()
{
	struct Set { std::string dir; Ha6Game want; const char* name; };
	std::vector<Set> sets;
	const std::string g = GameData(), u = UnibDir();
	if (!g.empty()) { sets.push_back({g + "/uni2", Ha6Game::UNI, "UNI2"}); sets.push_back({g + "/mbtl", Ha6Game::MBTL, "MBTL"}); }
	if (!u.empty()) sets.push_back({u, Ha6Game::UNIB, "UNIB"});
	for (const auto& s : sets) {
		std::error_code ec;
		int n = 0, bad = 0;
		for (auto it = fs::recursive_directory_iterator(fs::u8path(s.dir), ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (!it->is_regular_file(ec) || !IsHa6(it->path())) continue;
			FrameData fd;
			if (!fd.load(it->path().u8string().c_str())) continue;
			++n;
			if (fd.detectedGame() != s.want) {
				++bad;
				std::fprintf(stderr, "  %s detected as %s: %s\n", s.name, Ha6GameName(fd.detectedGame()), it->path().u8string().c_str());
			}
		}
		std::printf("detect %s: %d files, %d wrong\n", s.name, n, bad);
		CHECK(n > 0 && bad == 0);
	}
	if (sets.empty()) std::printf("detect: game data not found, skipped\n");

	// UNIB move scripts: Hyd_0.txt -> Hyd_0_mv.txt.
	if (!u.empty() && fs::exists(fs::u8path(u + "/Hyd_0/Hyd_0_mv.txt"))) {
		MvScriptIndex mv;
		CHECK(mv.loadForCharacter(u + "/Hyd_0/Hyd_0.txt"));
		std::printf("UNIB mv script: found\n");
	}
}

// Stacked UNI2/MBTL project: templates are recorded for slots the character defines.
static void TestTemplateDetectionData()
{
	const std::string g = GameData();
	if (g.empty()) { std::printf("templates: game data not found, skipped\n"); return; }
	struct P { std::string dir; const char* chr; int layers; };
	for (const P& p : {P{g + "/uni2/data/chr000", "chr000.ha6", 5}, P{g + "/mbtl/data/chr000", "chr000.ha6", 3}}) {
		const std::string t = p.dir + "/_temp.ha6", c = p.dir + "/" + p.chr, b = p.dir + "/../BaseData.HA6";
		if (!IsHa6(fs::u8path(t)) || !IsHa6(fs::u8path(c)) || !IsHa6(fs::u8path(b))) { std::printf("templates: %s missing, skipped\n", p.dir.c_str()); continue; }
		FrameData fd;
		CHECK(fd.load(t.c_str()));
		CHECK(fd.load(c.c_str(), true));
		CHECK(fd.load(b.c_str(), true, true));
		fd.setOwnFile(1);
		// Expected: BaseData PFLG&1 patterns whose slot the character fills.
		FrameData base; CHECK(base.load(b.c_str()));
		int want = 0, got = 0;
		for (int i = 0; i < base.get_sequence_count(); ++i) {
			const Sequence* bs = base.get_sequence(i);
			const Sequence* cs = fd.get_sequence(i);
			if (bs && (bs->flag & 1) && !bs->frames.empty() && cs && !cs->frames.empty() && !(cs->flag & 1)) ++want;
			if (fd.templateFor(i)) {
				++got;
				const Sequence m = MergeTemplatePattern(*fd.templateFor(i), *cs, p.layers);
				CHECK(m.frames.size() == fd.templateFor(i)->frames.size());
			}
		}
		std::printf("templates %s: %d template-backed pattern(s) (expected %d)\n", p.dir.c_str(), got, want);
		CHECK(want > 0 && got == want);
	}
}

static void TestVectorTable()
{
	const std::string g = GameData();
	if (g.empty()) { std::printf("vector table: game data not found, skipped\n"); return; }
	for (const char* game : {"uni2", "mbtl"}) {
		const univec::Names* n = univec::ForFile(g + "/" + game + "/data/chr000/chr000.ha6");
		CHECK(n != nullptr);
		if (!n) continue;
		std::printf("VectorTable %s: %zu names\n", game, n->size());
		CHECK(n->size() > 50 && n->count(0));
	}
	const univec::Names* u = univec::ForFile(g + "/uni2/data/chr000/chr000.ha6");
	if (u && u->count(0)) {
		const std::string& v = u->at(0);
		std::printf("VectorTable uni2 #0 bytes:");
		for (unsigned char ch : v) std::printf(" %02x", ch);
		std::printf("\n");
		CHECK(v == "\xe9\xa0\xad\xe3\x82\x84\xe3\x82\x89\xe3\x82\x8c\xe5\xbc\xb1"); // 頭やられ弱
	}
}


// Same-dialect paste is a no-op: pasting every pattern of a real UNI2 / MBTL
// file onto itself (through conformPasted) saves byte-identically.
static void TestSameGamePasteByteExact()
{
	const std::string g = GameData();
	if (g.empty()) { std::printf("same-game paste: game data not found, skipped\n"); return; }
	for (const char* rel : {"/uni2/data/chr000/chr000.ha6", "/mbtl/data/chr000/chr000.ha6"}) {
		const std::string in = g + rel;
		if (!IsHa6(fs::u8path(in))) continue;
		FrameData a; CHECK(a.load(in.c_str()));
		const std::string o1 = Tmp("s3_same1.ha6"), o2 = Tmp("s3_same2.ha6");
		CHECK(a.save(o1.c_str()));
		int changed = 0;
		for (int i = 0; i < a.get_sequence_count(); ++i) {
			Sequence* s = a.get_sequence(i);
			if (!s || s->frames.empty()) continue;
			Sequence copy = *s;
			if (a.conformPasted(copy).changed()) ++changed;
			*s = copy;
		}
		CHECK(changed == 0);
		CHECK(a.save(o2.c_str()));
		CHECK(ReadAll(o1) == ReadAll(o2) && ReadAll(o1) == ReadAll(in));
		std::printf("same-game paste %s: %d pattern(s) changed, bytes %s\n", rel, changed, ReadAll(o2) == ReadAll(in) ? "identical" : "DIFFER");
	}
}

// AF "End" (aniType 3, AFFL 3): UNI2 1.42 AnimState_AdvanceTick_ComputeNext 0x4BD1C0 case 3 holds the
// frame and ends the pattern. The Animation combo must name it, and set/save/load must keep it.
static void TestAfEndAnimationType()
{
	CHECK(kAnimationTypeCount == 4 && std::strcmp(animationList[3], "End") == 0);
	CHECK(std::strcmp(animationList[0], "Go to pattern") == 0 && std::strcmp(animationList[1], "Next frame") == 0
	      && std::strcmp(animationList[2], "Go to frame") == 0);
	const struct { Ha6Game g; const char* tag; } games[] = {
		{Ha6Game::UNI, "uni2"}, {Ha6Game::MBTL, "mbtl"}, {Ha6Game::MBAACC, "mbaacc"}};
	for (const auto& gt : games) {
		FrameData fd; MakeFile(fd, gt.g);
		const std::string base = Tmp((std::string("afend_") + gt.tag + "_a.ha6").c_str());
		CHECK(fd.save(base.c_str()));
		FrameData a;
		CHECK(a.load(base.c_str()));
		a.setGameOverride(gt.g);
		const std::string same = Tmp((std::string("afend_") + gt.tag + "_b.ha6").c_str());
		CHECK(a.save(same.c_str()));
		CHECK(ReadAll(base) == ReadAll(same));   // unchanged file is byte-exact
		a.get_sequence(0)->frames[0].AF.aniType = 3;   // what picking "End" in the combo writes
		const std::string ended = Tmp((std::string("afend_") + gt.tag + "_c.ha6").c_str());
		CHECK(a.save(ended.c_str()));
		const std::string b = ReadAll(ended);
		const size_t at = b.find("AFFL");
		CHECK(at != std::string::npos);
		if (at != std::string::npos) { int32_t v; memcpy(&v, &b[at + 4], 4); CHECK(v == 3); }
		FrameData r;
		CHECK(r.load(ended.c_str()));
		CHECK(r.get_sequence(0)->frames[0].AF.aniType == 3);
		r.setGameOverride(gt.g);
		const std::string again = Tmp((std::string("afend_") + gt.tag + "_d.ha6").c_str());
		CHECK(r.save(again.c_str()));
		CHECK(ReadAll(ended) == ReadAll(again));
	}
}

int main()
{
	TestPasteConversion();
	TestAtFields();
	TestTenConditionParams();
	TestLayerOps();
	TestTemplateMerge();
	TestDetection();
	TestTemplateDetectionData();
	TestVectorTable();
	TestSameGamePasteByteExact();
	TestAfEndAnimationType();
	std::printf("%d checks, %d failed\n", g_checks, g_fail);
	if (g_fail) { std::printf("UNI_DATALOSS_FAIL\n"); return 1; }
	std::printf("UNI_DATALOSS_PASS\n");
	return 0;
}
