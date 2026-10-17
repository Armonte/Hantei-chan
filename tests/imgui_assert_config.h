#pragma once
// ImGui config for cmdfile_ui_smoke: same layout options as config/config.h, but IM_ASSERT
// records failures instead of being compiled out, so API misuse in the editor UI shows up.
void CmdfileUiSmokeAssert(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : CmdfileUiSmokeAssert(#_EXPR, __FILE__, __LINE__))
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
