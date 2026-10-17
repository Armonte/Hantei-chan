// Per-game EF/IF labels (src/game_labels.h): UNI2 and MBTL files resolve their own
// table, MBAACC keeps its tables, and types with no handler say "unknown in <game>".
// The game-data cases load chr000.ha6 of UNI2 and MBTL (GAMEDATA, default
// C:/dev/hantei-chan/gamedata) and are skipped when the data is absent.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include "framedata.h"
#include "game_labels.h"

namespace fs = std::filesystem;
static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::string GameData()
{
	if (const char* g = getenv("GAMEDATA")) return g;
	for (const char* p : {"C:/dev/hantei-chan/gamedata", "/mnt/c/dev/hantei-chan/gamedata"})
		if (fs::exists(fs::u8path(p))) return p;
	return {};
}

static void TestTables()
{
	for (Ha6Game g : {Ha6Game::UNI, Ha6Game::MBTL}) {
		CHECK(GameHasOwnLabels(g));
		CHECK(GameLabelTitle(g, GameLabelKind::EF, 9) == "9: Play sound");
		CHECK(GameLabelTitle(g, GameLabelKind::EF6, 310) == "310: Set object flags");
		CHECK(GameLabelTitle(g, GameLabelKind::EF6, 116) == "116: Launch along rotation");
		CHECK(GameLabelTitle(g, GameLabelKind::IF, 56) == "56: Vacuum (pull opponents)");
		CHECK(FindGameLabel(g, GameLabelKind::IF, 56)->params[1] != nullptr);
		// No handler in uni2.exe 0x5B8620 / MBTL.exe 0x582420: never an MBAACC label.
		CHECK(FindGameLabel(g, GameLabelKind::EF, 4) == nullptr);
		CHECK(FindGameLabel(g, GameLabelKind::EF, 3) == nullptr);
		CHECK(FindGameLabel(g, GameLabelKind::IF, 1) == nullptr);
	}
	CHECK(GameLabelTitle(Ha6Game::UNI, GameLabelKind::EF, 4) == "EF 4 (unknown in UNI2)");
	CHECK(GameLabelTitle(Ha6Game::MBTL, GameLabelKind::IF, 43) == "IF 43 (unknown in MBTL)");
	CHECK(!GameHasOwnLabels(Ha6Game::MBAACC));
	CHECK(FindGameLabel(Ha6Game::MBAACC, GameLabelKind::EF, 1) == nullptr);
	// Rows are unique per table.
	for (GameLabelKind k : {GameLabelKind::EF, GameLabelKind::EF6, GameLabelKind::IF}) {
		std::map<int, int> seen;
		for (int i = 0; i < GameLabelCount(Ha6Game::UNI, k); ++i) ++seen[GameLabelAt(Ha6Game::UNI, k, i)->id];
		for (auto& [id, n] : seen) CHECK(n == 1);
	}
}

// Every EF/IF of a shipped character file: count the ones that resolve per game.
static void TestFile(const std::string& path, Ha6Game want, const char* name)
{
	if (!fs::exists(fs::u8path(path))) { std::printf("skip %s (%s missing)\n", name, path.c_str()); return; }
	FrameData fd;
	CHECK(fd.load(path.c_str()));
	CHECK(fd.game() == want);
	const Ha6Game g = fd.game();
	int ef = 0, efKnown = 0, ifs = 0, ifKnown = 0, ef6 = 0, ef6Known = 0;
	std::map<std::string, int> unknown;
	for (int s = 0; s < fd.get_sequence_count(); ++s) {
		Sequence* seq = fd.get_sequence(s);
		if (!seq) continue;
		for (auto& f : seq->frames) {
			for (auto& e : f.EF) {
				++ef; if (FindGameLabel(g, GameLabelKind::EF, e.type)) ++efKnown; else ++unknown[GameLabelTitle(g, GameLabelKind::EF, e.type)];
				if (e.type == 6) { ++ef6; if (FindGameLabel(g, GameLabelKind::EF6, e.number)) ++ef6Known; else ++unknown[GameLabelTitle(g, GameLabelKind::EF6, e.number)]; }
			}
			for (auto& c : f.IF) {
				++ifs; if (FindGameLabel(g, GameLabelKind::IF, c.type)) ++ifKnown; else ++unknown[GameLabelTitle(g, GameLabelKind::IF, c.type)];
			}
		}
	}
	std::printf("%s: EF %d/%d labelled, EF6 %d/%d, IF %d/%d\n", name, efKnown, ef, ef6Known, ef6, ifKnown, ifs);
	for (auto& [t, n] : unknown) std::printf("  %s x%d\n", t.c_str(), n);
	CHECK(ef > 0 && ifs > 0);
	CHECK(efKnown * 10 >= ef * 9);   // the shipped data is almost all handled types
	CHECK(ifKnown * 10 >= ifs * 9);
	CHECK(GameLabelTitle(g, GameLabelKind::EF, 1) == "1: Spawn pattern (character HA6)");
	CHECK(GameLabelTitle(g, GameLabelKind::EF, 6) == "6: Various effects 2 (see number)");
}

int main()
{
	TestTables();
	const std::string gd = GameData();
	if (!gd.empty()) {
		TestFile(gd + "/uni2/data/chr000/chr000.ha6", Ha6Game::UNI, "UNI2 chr000");
		TestFile(gd + "/mbtl/data/chr000/chr000.ha6", Ha6Game::MBTL, "MBTL chr000");
	} else {
		std::printf("skip game data (no GAMEDATA)\n");
	}
	std::printf("%d checks, %d failed\n", g_checks, g_fail);
	return g_fail ? 1 : 0;
}
