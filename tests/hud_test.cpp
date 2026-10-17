// HUD colour tests (issue #56).
//   hud_test [MBAA.exe]
// Theme JSON round trip always; with an exe: every colour slot is located on
// the reported instruction and holds the vanilla default, and a patched copy
// changes exactly those immediates.

#include "hud_colors.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(x) do { if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

static std::string ReadAll(const std::string& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(f), {});
}

int main(int argc, char** argv)
{
	const auto& slots = hud::Slots();
	std::vector<uint32_t> colors;
	for (auto& s : slots) colors.push_back(s.defaultArgb ^ 0x00123456u);
	std::string err;
	CHECK(hud::SaveThemeColors("hud_test_theme.json", colors, &err));
	std::vector<uint32_t> back;
	CHECK(hud::LoadThemeColors("hud_test_theme.json", back, &err) && back == colors);
	std::remove("hud_test_theme.json");
	uint32_t v;
	CHECK(hud::FromHex("#ffc80000", &v) && v == 0xffc80000u && hud::ToHex(v) == "#ffc80000");
	CHECK(!hud::FromHex("#fff", &v));

	if (argc > 1) {
		std::vector<hud::ExeColor> exe;
		CHECK(hud::ReadExeColors(argv[1], exe, &err));
		for (size_t i = 0; i < slots.size() && i < exe.size(); ++i) {
			std::printf("  %-20s hint 0x%05x -> imm @0x%05x %s (default %s)\n", slots[i].key, slots[i].rvaHint,
				exe[i].fileOffset, exe[i].found ? hud::ToHex(exe[i].argb).c_str() : "NOT FOUND",
				hud::ToHex(slots[i].defaultArgb).c_str());
			CHECK(exe[i].found && exe[i].argb == slots[i].defaultArgb);
		}
		CHECK(hud::WritePatchedExe(argv[1], "hud_test_patched.exe", colors, &err));
		std::vector<hud::ExeColor> patched;
		CHECK(hud::ReadExeColors("hud_test_patched.exe", patched, &err));
		for (size_t i = 0; i < slots.size() && i < patched.size(); ++i) CHECK(patched[i].argb == colors[i]);
		const std::string a = ReadAll(argv[1]), b = ReadAll("hud_test_patched.exe");
		size_t diff = 0;
		for (size_t i = 0; i < a.size() && i < b.size(); ++i) diff += a[i] != b[i];
		CHECK(a.size() == b.size() && diff <= slots.size() * 4 && diff > 0);
		std::printf("patched copy differs in %zu byte(s)\n", diff);
		CHECK(!hud::WritePatchedExe(argv[1], argv[1], colors, &err));
		std::remove("hud_test_patched.exe");
	}
	std::printf(g_fail == 0 ? "HUD_TEST_PASS\n" : "HUD_TEST_FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
