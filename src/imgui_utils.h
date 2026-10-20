#ifndef IMGUI_UTILS_H_GUARD
#define IMGUI_UTILS_H_GUARD

#include <imgui.h>

bool BitField(const char* label, unsigned* bits, unsigned* hoverIndex, int showBits = 32);
void Tooltip(const char* text);

namespace ui {

// Draw an app-owned texture with the texture's own sampler settings.
//
// The editor uploads pixel art with GL_NEAREST (see MakeTex in
// archive_browser_ui.cpp). ImGui's GL3 backend used to leave glTexParameteri
// alone, but since it prefers glBindSampler (which the backend only uses when
// the GL context supports it, i.e. every desktop 3.3+ context) it binds its own
// linear sampler and overrides the texture's filter, blurring the art.
// DrawCallback_SetSamplerFromTex asks it to use the texture's parameters.
//
// These textures are drawn at 1:1 or by an integer factor, so the correct
// filter is exactly the one they carry.
//
// The baseline build (IMGUI_IMPL_OPENGL_ES2) never bound a sampler, so every
// app texture, linear or nearest, was drawn with its own filter and wrap mode.
// Every app-texture draw goes through one of the helpers below (ctest
// imgui_sampler_lint checks src/ for raw ImGui::Image / AddImage calls).

// Start a run of draws that use the textures' own parameters. The run ends at
// EndOwnSampler or at the next DrawCallback_ResetRenderState.
inline void BeginOwnSampler(ImDrawList* dl)
{
	ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
	if (pio.DrawCallback_SetSamplerFromTex)
		dl->AddCallback(pio.DrawCallback_SetSamplerFromTex, nullptr);
}

inline void EndOwnSampler(ImDrawList* dl)
{
	ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
	if (pio.DrawCallback_SetSamplerFromTex)
		dl->AddCallback(pio.DrawCallback_ResetRenderState, nullptr);
}

inline void ImageWithOwnSampler(ImDrawList* dl, ImTextureID tex, const ImVec2& a, const ImVec2& b,
                                const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1),
                                ImU32 tint = IM_COL32_WHITE)
{
	BeginOwnSampler(dl);
	dl->AddImage(tex, a, b, uv0, uv1, tint);   // own sampler
	EndOwnSampler(dl);
}

// ImGui::Image (item layout included) with the texture's own sampler.
inline void Image(ImTextureID tex, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1))
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	BeginOwnSampler(dl);
	ImGui::Image(tex, size, uv0, uv1);   // own sampler
	EndOwnSampler(dl);
}

}  // namespace ui

#endif /* IMGUI_UTILS_H_GUARD */

