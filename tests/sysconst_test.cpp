// System constants model (src/authoring/sysconst_model): _type.txt and vector.txt edits replace only the edited
// value's bytes. Synthetic files always; the vanilla MBAACC files too when gamedata/mbaacc_ref is present.
#include "authoring/sysconst_model.h"
#include "authoring/winquote_model.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace sysconst;
static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)
#define CHECK_EQ(a, b) do { const std::string a_ = (a), b_ = (b); if (a_ != b_) { std::printf("FAIL %s:%d: %s\n  got      %s\n  expected %s\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); ++g_failures; } } while (0)

static std::string Get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }

static void TypeSynthetic()
{
	const std::string raw =
		"[System]\r\nFloatLimit = 0.008 // c1\r\n\r\n// note\r\n"
		"[Type_0] // Crescent ---\r\nShieldKeepTime = 6\r\nExShieldTime=8// tight\r\n"
		"[Type_1]\r\nShieldKeepTime = 6\r\n";
	TypeFile t;
	CHECK(t.loadText(raw));
	CHECK(t.sections().size() == 3);
	CHECK(t.keys().size() == 4);
	const TypeKey* k = t.find("Type_0", "exshieldtime");
	CHECK(k && k->value == "8" && k->comment == "tight");
	CHECK(t.setValue("Type_0", "ShieldKeepTime", "10"));
	CHECK(t.setValue("Type_0", "ExShieldTime", "12"));
	CHECK(!t.setValue("Type_2", "ShieldKeepTime", "1"));
	CHECK_EQ(t.text(),
		"[System]\r\nFloatLimit = 0.008 // c1\r\n\r\n// note\r\n"
		"[Type_0] // Crescent ---\r\nShieldKeepTime = 10\r\nExShieldTime=12// tight\r\n"
		"[Type_1]\r\nShieldKeepTime = 6\r\n");
	CHECK(t.find("Type_1", "ShieldKeepTime")->value == "6");
	CHECK(t.dirty());
}

static void VectorSynthetic()
{
	const std::string raw =
		"// list\r\n  0  900 0 -70 0 // a\r\n 1 1200 0 -75 0// b\r\n//10 0 1 2 3\r\nEND\r\n"
		"// No name ...\r\n 0 \x93\xaa 1 0 10 57 1\r\n  0 900 17 14 14 00000000 //  0 23 17\r\n"
		"11 \x8e\xce 1 30 100 0\r\n 11 26 19 -1 8 00000000\r\n";
	VectorFile v;
	CHECK(v.loadText(raw));
	CHECK(v.rows().size() == 6);
	CHECK(v.rows()[0].kind == VectorFile::List && v.rows()[1].kind == VectorFile::List);
	CHECK(v.rows()[2].kind == VectorFile::Header && v.rows()[3].kind == VectorFile::Step && v.rows()[3].owner == 2);
	CHECK(v.rows()[4].kind == VectorFile::Header && v.rows()[4].tok.size() == 6); // KoCheck missing, as in vanilla row 11
	CHECK_EQ(v.field(1, 1), "1200");
	CHECK(v.setField(1, 1, "1300"));
	CHECK(v.setField(3, 2, "18"));
	CHECK(!v.setField(3, 2, "1 8"));
	CHECK(v.idOf(4) == 11);
	CHECK_EQ(v.text(),
		"// list\r\n  0  900 0 -70 0 // a\r\n 1 1300 0 -75 0// b\r\n//10 0 1 2 3\r\nEND\r\n"
		"// No name ...\r\n 0 \x93\xaa 1 0 10 57 1\r\n  0 900 18 14 14 00000000 //  0 23 17\r\n"
		"11 \x8e\xce 1 30 100 0\r\n 11 26 19 -1 8 00000000\r\n");
}

static void Vanilla(const fs::path& dir, const fs::path& out)
{
	const fs::path type = dir / "_type.txt", vec = dir / "vector.txt";
	if (!fs::exists(type) || !fs::exists(vec)) {
		std::printf("skip vanilla (no %s)\n", dir.u8string().c_str());
		return;
	}
	TypeFile t;
	CHECK(t.load(type.u8string()));
	CHECK(t.sections().size() >= 5);
	CHECK(t.find("System", "Value_005") != nullptr && t.find("Type_0", "MagicCircuitMax") != nullptr);
	fs::create_directories(out);
	CHECK(t.save((out / "_type.txt").u8string()));
	CHECK(Get(out / "_type.txt") == Get(type));
	VectorFile v;
	CHECK(v.load(vec.u8string()));
	size_t lists = 0, heads = 0, steps = 0;
	for (const auto& r : v.rows()) (r.kind == VectorFile::List ? lists : r.kind == VectorFile::Header ? heads : steps)++;
	std::printf("vanilla vector.txt: %zu list rows, %zu hit vectors, %zu steps\n", lists, heads, steps);
	CHECK(lists > 50 && heads > 50 && steps >= heads);
	CHECK(v.save((out / "vector.txt").u8string()));
	CHECK(Get(out / "vector.txt") == Get(vec));
}

static void WinQuotes(const fs::path& battle, const fs::path& out)
{
	// "■" = 81 A1. Quote lines, a comment, END, trailing text.
	const std::string raw = "255  0  0\tA_W00.adx \x8a\x79\x8f\x9f\x81\xa1x\r\n  7  0  1\tA_W01.adx hi\r\n\r\n// note\r\n 28  1  1\tA_W02.adx boss\r\n\r\nEND\r\n\r\n// 1234\r\n";
	winquote::File w;
	CHECK(w.loadText(raw));
	CHECK(w.quotes().size() == 3);
	CHECK(w.quotes()[1].opponent == 7 && w.quotes()[1].face == 1 && w.quotes()[1].voice == "A_W01.adx" && w.quotes()[1].text == "hi");
	CHECK(w.quotes()[2].mode == 1);
	CHECK(winquote::File::widestLine(w.quotes()[0].text) == 4);
	winquote::Quote q = w.quotes()[1];
	q.text = "hello";
	w.set(1, q);
	w.add(99, winquote::Quote{255, 0, 2, "A_W03.adx", "new", 0});
	w.remove(0);
	CHECK_EQ(w.text(), "  7  0  1\tA_W01.adx hello\r\n\r\n// note\r\n 28  1  1\tA_W02.adx boss\r\n\r\n255  0  2\tA_W03.adx new\r\nEND\r\n\r\n// 1234\r\n");
	const fs::path arc = battle / "arc_win.txt";
	if (!fs::exists(arc)) { std::printf("skip real win quotes (no %s)\n", arc.u8string().c_str()); return; }
	winquote::File r;
	CHECK(r.load(arc.u8string()));
	std::printf("arc_win.txt: %zu quotes\n", r.quotes().size());
	CHECK(r.quotes().size() > 10);
	fs::create_directories(out);
	CHECK(r.save((out / "arc_win.txt").u8string()));
	CHECK(Get(out / "arc_win.txt") == Get(arc));
	// An unchanged set() is byte-identical too (the vanilla layout is "%3d %2d %2d\t").
	r.set(0, r.quotes()[0]);
	CHECK(r.text() == Get(arc));
}

int main(int argc, char** argv)
{
	const fs::path out = argc > 1 ? fs::u8path(argv[1]) : fs::path("build/sysconst_test_out");
	const fs::path vanilla = argc > 2 ? fs::u8path(argv[2]) : fs::path("../../gamedata/mbaacc_ref/vanilla_0002");
	TypeSynthetic();
	VectorSynthetic();
	Vanilla(vanilla, out);
	WinQuotes(argc > 3 ? fs::u8path(argv[3]) : fs::path("C:/games/mbaacc/story/battle"), out);
	if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
	std::printf("sysconst: all checks passed\n");
	return 0;
}
