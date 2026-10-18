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
inline void ImageWithOwnSampler(ImDrawList* dl, ImTextureID tex, const ImVec2& a, const ImVec2& b,
                                const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1),
                                ImU32 tint = IM_COL32_WHITE)
{
	ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
	if (pio.DrawCallback_SetSamplerFromTex)
		dl->AddCallback(pio.DrawCallback_SetSamplerFromTex, nullptr);
	dl->AddImage(tex, a, b, uv0, uv1, tint);
	if (pio.DrawCallback_SetSamplerFromTex)
		dl->AddCallback(pio.DrawCallback_ResetRenderState, nullptr);
}

}  // namespace ui

#endif /* IMGUI_UTILS_H_GUARD */

