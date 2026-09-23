#include "extension_profile.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <initializer_list>

namespace {
ExtensionProfile g_profile = ExtensionProfile::Vanilla;

void* ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char*) { return &g_profile; }

void ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line)
{
	int value = 0;
	if (sscanf(line, "Profile=%d", &value) == 1)
		g_profile = value == 1 ? ExtensionProfile::Extended : ExtensionProfile::Vanilla;
}

void WriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
	buf->appendf("[%s][]\n", handler->TypeName);
	buf->appendf("Profile=%d\n\n", static_cast<int>(g_profile));
}
} // namespace

ExtensionProfile GetExtensionProfile() { return g_profile; }

void SetExtensionProfile(ExtensionProfile profile)
{
	if (g_profile == profile) return;
	g_profile = profile;
	if (ImGui::GetCurrentContext()) ImGui::MarkIniSettingsDirty();
}

const char* ExtensionProfileName(ExtensionProfile profile)
{
	return profile == ExtensionProfile::Extended ? "Extended Melty / BOF" : "Vanilla MBAACC";
}

void RegisterExtensionProfileSettings(ImGuiContext& context)
{
	ImGuiSettingsHandler handler{};
	handler.TypeName = "Extension profile";
	handler.TypeHash = ImHashStr("Extension profile");
	handler.ReadOpenFn = ReadOpen;
	handler.ReadLineFn = ReadLine;
	handler.WriteAllFn = WriteAll;
	context.SettingsHandlers.push_back(handler);
}

void DrawExtensionProfileMenu()
{
	if (!ImGui::BeginMenu("Extension profile")) return;
	for (ExtensionProfile p : { ExtensionProfile::Vanilla, ExtensionProfile::Extended }) {
		if (ImGui::MenuItem(ExtensionProfileName(p), nullptr, GetExtensionProfile() == p))
			SetExtensionProfile(p);
	}
	ImGui::Separator();
	ImGui::TextDisabled(GetExtensionProfile() == ExtensionProfile::Vanilla
		? "Stock MBAACC: only engine-verified IDs and rules."
		: "Patched BOF executables: adds IF 154-157, EF6 154,\nExComCheck Types 2/3 and the BOF _c rules.");
	ImGui::EndMenu();
}
