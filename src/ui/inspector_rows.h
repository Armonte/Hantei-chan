#pragma once
#include "ui_style.h"
#include "../i18n.h"

namespace ui {
inline void AxisLabel(int axis) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(AxisColor(axis), "%s", axis == 0 ? "X" : axis == 1 ? "Y" : "Z");
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-FLT_MIN);
}
inline void RowLabel(const char* label) {
    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(TXT(label));
}
inline bool InlineInt(const char* label, int* value, float width) {
    ImGui::PushID(label); RowLabel(label); ImGui::SameLine(); ImGui::SetNextItemWidth(width);
    const bool changed=ImGui::InputInt("##value",value,0,0); ImGui::PopID(); return changed;
}
inline bool InlineFloat(const char* label, float* value, float width) {
    ImGui::PushID(label); RowLabel(label); ImGui::SameLine(); ImGui::SetNextItemWidth(width);
    const bool changed=ImGui::InputFloat("##value",value,0,0); ImGui::PopID(); return changed;
}
inline bool AxisFlags(unsigned* flags, int axis) {
    bool changed=false; ImGui::BeginGroup();ImGui::PushID(axis);
    // AS source contract: bits4..7 are X, bits0..3 are Y. Preserve all other bits.
    for(int bit=0;bit<4;++bit) {
        if(bit) ImGui::SameLine(0,2);
        ImGui::PushID(bit); const unsigned mask=1u<<(bit+(axis==0?4:0));
        const float side=ImGui::GetFrameHeight()*.75f;
        const auto p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##flag",ImVec2(side,side))) {*flags^=mask;changed=true;}
        ImGui::GetWindowDrawList()->AddRectFilled(p,ImVec2(p.x+side,p.y+side),ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg));
        if(*flags&mask) ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x+3,p.y+3),ImVec2(p.x+side-3,p.y+side-3),ImGui::GetColorU32(ImGuiCol_CheckMark));
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %s (bit %d)",axis==0?"X":"Y",bit==0?"Set speed":bit==1?"Add speed":"Unknown",bit+(axis==0?4:0));
        ImGui::PopID();
    }
    ImGui::PopID();ImGui::EndGroup();return changed;
}
inline void InspectorItem(const char* name) {
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    auto* storage=ImGui::GetStateStorage();const auto key=ImHashStr(name);
    storage->SetFloat(key,a.x);storage->SetFloat(key+1,a.y);storage->SetFloat(key+2,b.x);storage->SetFloat(key+3,b.y);
}
inline bool AxisFloats(const char* id,float* values,int count,float speed) {
    ImGui::PushID(id);bool edited=false;
    const float total=ImGui::GetContentRegionAvail().x;
    const float cell=(total-ImGui::GetStyle().ItemSpacing.x*(count-1))/count;
    for(int axis=0;axis<count;++axis) {
        if(axis) ImGui::SameLine();ImGui::PushID(axis);
        AxisLabel(axis);ImGui::SetNextItemWidth(std::max(1.f,cell-LabelTextW("X")-ImGui::GetStyle().ItemInnerSpacing.x));
        ImGui::DragFloat("##value",&values[axis],speed);edited|=ImGui::IsItemDeactivatedAfterEdit();ImGui::PopID();
    }
    ImGui::PopID();return edited;
}
inline bool BeginInspectorTable(const char* id,int columns) {
    return ImGui::BeginTable(id,columns,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings|ImGuiTableFlags_NoClip);
}
inline void InspectorCell(int row,int column) {
    if(column==0) ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(column); ImGui::SetNextItemWidth(-FLT_MIN);
}
}
