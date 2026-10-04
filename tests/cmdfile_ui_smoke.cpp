// Headless smoke test for the command-file editor UI: runs the real ImGui code (no
// renderer) over a command file for every tab and both extension profiles, with ImGui
// assertions enabled (tests/imgui_assert_config.h). Catches ID-stack, Begin/End, table and
// popup misuse that the app build (asserts compiled out) would hide.
//
//   cmdfile_ui_smoke FILE...

#include "cmdfile/cmd_editor_ui.h"
#include "extension_profile.h"

#include <imgui.h>

#include <cstdio>
#include <string>

static int g_asserts = 0;
void CmdfileUiSmokeAssert(const char* expr, const char* file, int line)
{
	if (++g_asserts <= 20) std::printf("IM_ASSERT failed: %s (%s:%d)\n", expr, file, line);
}

int main(int argc, char** argv)
{
	if (argc < 2) { std::printf("usage: cmdfile_ui_smoke FILE...\n"); return 2; }
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(1600, 900);
	io.IniFilename = nullptr;
	io.Fonts->Build();

	cmdfile::CommandFileEditor editor;
	for (int i = 1; i < argc; ++i) {
		std::string error;
		if (!editor.open(argv[i], &error)) { std::printf("open failed: %s\n", error.c_str()); return 1; }
	}
	int changes = 0;
	for (int pass = 0; pass < 4; ++pass) {
		const int profile = pass & 1;
		if (pass == 2) editor.debugSelectFirst(); // second half: detail panels populated
		SetExtensionProfile(profile ? ExtensionProfile::Extended : ExtensionProfile::Vanilla);
		for (int tab = 0; tab < 5; ++tab) {
			editor.debugRequestTab(tab);
			for (int frame = 0; frame < 4; ++frame) {
				io.DeltaTime = 1.0f / 60.0f;
				ImGui::NewFrame();
				editor.draw([&](const std::string&, const cmdfile::Document&) { ++changes; });
				ImGui::Render();
			}
		}
	}
	ImGui::DestroyContext();
	std::printf("%d frame(s) drawn, %d change notification(s), %d ImGui assertion(s)\n", 80, changes, g_asserts);
	return g_asserts == 0 ? 0 : 1;
}
