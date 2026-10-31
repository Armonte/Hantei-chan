#include "pat_search.h"
#include "../i18n.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>

namespace patsearch {

static std::string g_query;

void FilterBox()
{
	if (ImGui::IsWindowAppearing()) {
		g_query.clear();
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("##patsearch", TXT("Search names..."), &g_query);
}

bool Matches(const char* name) { return Contains(name, g_query.c_str()); }

} // namespace patsearch
