// Headless test for the pattern-name search (issue #81). Runs the same ImSearch
// calls as MainPane (inline search bar and the "Search Pattern Names" window) over
// the decorated pattern names of real HA6 files, types queries one character per
// frame through ImGui's input queue, and checks:
//   - no ImGui assertion fires (tests/imgui_assert_config.h),
//   - every draw command's indices stay inside the vertex buffer (what the GL
//     backend would read), and the output is identical for a second run.
//
//   pattern_search_test [--font FILE.otf] [--queries N] HA6...
// With no HA6 argument it uses a built-in list of names shaped like issue #81's.

#include "framedata.h"
#include "imsearch.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static int g_asserts = 0;
static std::string g_typed; // query typed so far, for the failure report
void CmdfileUiSmokeAssert(const char* expr, const char* file, int line)
{
	if (++g_asserts <= 20) std::printf("IM_ASSERT failed after typing \"%s\": %s (%s:%d)\n", g_typed.c_str(), expr, file, line);
}

namespace {

int g_badDraws = 0;

// Every index the renderer would read must land inside the vertex buffer.
void CheckDrawData()
{
	ImDrawData* dd = ImGui::GetDrawData();
	if (!dd) return;
	for (int l = 0; l < dd->CmdListsCount; ++l) {
		const ImDrawList* list = dd->CmdLists[l];
		for (const ImDrawCmd& cmd : list->CmdBuffer) {
			if (cmd.UserCallback) continue;
			if ((int)(cmd.IdxOffset + cmd.ElemCount) > list->IdxBuffer.Size) {
				if (++g_badDraws <= 10) std::printf("draw cmd reads past the index buffer\n");
				continue;
			}
			for (unsigned i = 0; i < cmd.ElemCount; ++i) {
				const unsigned v = cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i];
				if ((int)v >= list->VtxBuffer.Size) {
					if (++g_badDraws <= 10)
						std::printf("draw cmd index %u + VtxOffset %u >= %d vertices\n",
							list->IdxBuffer[cmd.IdxOffset + i], cmd.VtxOffset, list->VtxBuffer.Size);
					break;
				}
			}
		}
	}
}

struct Pane {
	std::vector<std::string> names;
	int pattern = 0;
	bool popup = false;

	void Draw()
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(320, 700), ImGuiCond_Always);
		ImGui::Begin("Left Pane");
		if (ImSearch::BeginSearch()) {
			ImSearch::SearchBar("Search pattern names...");
			const char* query = ImSearch::GetUserQuery();
			if (query && std::strlen(query) > 0) {
				for (int n = 0; n < (int)names.size(); ++n) {
					ImSearch::SearchableItem(names[n].c_str(), [&, n](const char* name) {
						if (ImGui::Selectable(name, pattern == n)) pattern = n;
					});
				}
			}
			ImSearch::EndSearch();
		}
		ImGui::Text("pattern %d", pattern);
		ImGui::End();

		if (popup) {
			ImGui::SetNextWindowPos(ImVec2(400, 50), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Always);
			ImGui::Begin("Search Pattern Names", &popup);
			if (ImSearch::BeginSearch()) {
				ImSearch::SearchBar("Search pattern names...");
				for (int n = 0; n < (int)names.size(); ++n) {
					ImSearch::SearchableItem(names[n].c_str(), [&, n](const char* name) {
						if (ImGui::Selectable(name, pattern == n)) pattern = n;
					});
				}
				ImSearch::EndSearch();
			}
			ImGui::End();
		}
	}
};

void DrawFrame(Pane& pane)
{
	ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
	ImGui::NewFrame();
	pane.Draw();
	ImGui::Render();
	CheckDrawData();
}

// Clear the focused search field and type `q` one character per frame.
int TypeQuery(Pane& pane, const std::string& q)
{
	ImGuiIO& io = ImGui::GetIO();
	int frames = 0;
	// Select all + delete
	io.AddKeyEvent(ImGuiMod_Ctrl, true);
	io.AddKeyEvent(ImGuiKey_A, true);
	DrawFrame(pane); ++frames;
	io.AddKeyEvent(ImGuiKey_A, false);
	io.AddKeyEvent(ImGuiMod_Ctrl, false);
	io.AddKeyEvent(ImGuiKey_Delete, true);
	DrawFrame(pane); ++frames;
	io.AddKeyEvent(ImGuiKey_Delete, false);
	DrawFrame(pane); ++frames;
	g_typed.clear();
	for (char c : q) {
		g_typed += c;
		io.AddInputCharacter((unsigned char)c);
		DrawFrame(pane); ++frames;
	}
	// Tab = autocomplete, then a couple of idle frames.
	io.AddKeyEvent(ImGuiKey_Tab, true);
	DrawFrame(pane); ++frames;
	io.AddKeyEvent(ImGuiKey_Tab, false);
	DrawFrame(pane); ++frames;
	return frames;
}

std::vector<std::string> Tokens(const std::string& s)
{
	std::vector<std::string> out;
	std::string cur;
	for (unsigned char c : s) {
		if (c < 0x80 && std::isalnum(c)) cur += (char)c;
		else if (!cur.empty()) { out.push_back(cur); cur.clear(); }
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

} // namespace

int main(int argc, char** argv)
{
	std::string font;
	int queriesPerFile = 60;
	std::vector<std::string> files;
	for (int i = 1; i < argc; ++i) {
		if (!std::strcmp(argv[i], "--font") && i + 1 < argc) font = argv[++i];
		else if (!std::strcmp(argv[i], "--queries") && i + 1 < argc) queriesPerFile = std::atoi(argv[++i]);
		else files.push_back(argv[i]);
	}

	ImGui::CreateContext();
	ImSearch::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(1280, 720);
	io.IniFilename = nullptr;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	if (!font.empty()) {
		if (!io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()))
			std::printf("font load failed: %s\n", font.c_str());
	}
	io.Fonts->Build();

	std::vector<std::vector<std::string>> nameSets;
	std::vector<std::string> labels;
	if (files.empty()) {
		// Names shaped like the character in issue #81's video.
		nameSets.push_back({
			"000 STAND", "041 \xe3\x80\x87 ", "078 @^ 8 N D.HOP", "092 2 AIR DASH D",
			"210 ___ OD ___________", "211 UC OD 8A+D SPECIAL", "215 SHERMIE 236AD OD",
			"217 @ 236AD DAMENE FOLLOWUP OD", "218 @ 236A DAMENE 214 LEG VER OD",
			"243 @ GROUND DODGE", "256 O DON'T USE", "336 \xc2\xac THROW DATA CLUTCH",
			"337 \xc2\xac THROW DATA SPIDER", "352 SLIDE DOWN WAKEUP", "367 SBC BLOWBACK FOOT DUST",
			"480 OD COMBO UI CHECK CELL 1", "520 OD CELL STATE MANAGER", "521 OD GAUGE METER MANAGER",
			"549 OD PROJVAR BRIDGE 9 SJC", "582 ANGEL UC HELPER INF MODE", "476 TURN AROUND P1 CHECK P2 old",
		});
		labels.push_back("builtin");
	}
	for (const auto& f : files) {
		FrameData fd;
		if (!fd.load(f.c_str())) { std::printf("load failed: %s\n", f.c_str()); continue; }
		std::vector<std::string> names;
		for (int n = 0; n < (int)fd.get_sequence_count(); ++n) names.push_back(fd.GetDecoratedName(n));
		nameSets.push_back(std::move(names));
		labels.push_back(f);
	}

	std::mt19937 rng(81);
	long frames = 0, queries = 0;
	for (size_t s = 0; s < nameSets.size(); ++s) {
		Pane pane;
		pane.names = nameSets[s];
		pane.popup = true;
		DrawFrame(pane); DrawFrame(pane); // focus lands on the first search bar
		std::vector<std::string> tokens;
		for (const auto& n : pane.names)
			for (auto& t : Tokens(n)) tokens.push_back(t);
		std::vector<std::string> qs = {"OD DA", "OD D", "od da", "A", " ", "  OD  ", "D.HOP", "___", "8A+D"};
		std::uniform_int_distribution<int> any(0, 1 << 30);
		for (int q = 0; q < queriesPerFile && !tokens.empty(); ++q) {
			std::string a = tokens[any(rng) % tokens.size()];
			std::string b = tokens[any(rng) % tokens.size()];
			switch (q % 4) {
			case 0: qs.push_back(a + " " + b); break;
			case 1: qs.push_back(a + " " + b.substr(0, 1 + any(rng) % b.size())); break;
			case 2: qs.push_back(a.substr(0, 1 + any(rng) % a.size())); break;
			default: {
				std::string r;
				for (int k = 0; k < 1 + any(rng) % 6; ++k) r += " ADOQ._@+"[any(rng) % 9];
				qs.push_back(r);
			}
			}
		}
		for (const auto& q : qs) { frames += TypeQuery(pane, q); ++queries; }
		std::printf("%s: %zu names, %zu queries\n", labels[s].c_str(), pane.names.size(), qs.size());
	}

	ImSearch::DestroyContext();
	ImGui::DestroyContext();
	std::printf("%ld frame(s), %ld quer(ies), %d ImGui assertion(s), %d bad draw command(s)\n",
		frames, queries, g_asserts, g_badDraws);
	const bool ok = g_asserts == 0 && g_badDraws == 0;
	std::printf(ok ? "PATTERN_SEARCH_TEST_PASS\n" : "PATTERN_SEARCH_TEST_FAIL\n");
	return ok ? 0 : 1;
}
