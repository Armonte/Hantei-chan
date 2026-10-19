#pragma once
#include <imgui.h>
#include <algorithm>

namespace ui {
// Scoped to property inspectors: never resize text, viewport tools, menus or
// unrelated windows. Both workspace hosts draw these same inspector functions.
class CompactInspectorScope {
    bool enabled;
public:
    explicit CompactInspectorScope(bool compact) : enabled(compact) {
        if (!enabled) return;
        const auto spacing = ImGui::GetStyle().ItemSpacing;
        const auto frame = ImGui::GetStyle().FramePadding;
        const auto cell = ImGui::GetStyle().CellPadding;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x, std::max(1.f, spacing.y * .5f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(frame.x, std::max(1.f, frame.y * .67f)));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(cell.x, std::max(1.f, cell.y * .5f)));
    }
    ~CompactInspectorScope() { if (enabled) ImGui::PopStyleVar(3); }
    CompactInspectorScope(const CompactInspectorScope&) = delete;
    CompactInspectorScope& operator=(const CompactInspectorScope&) = delete;
};
}
