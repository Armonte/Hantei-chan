#include "pat_partset_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"

PatPartSetPane::PatPartSetPane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    partSetDecoratedNames(nullptr),
                                                                    partPropsDecoratedNames(nullptr)
{

}

void PatPartSetPane::RegeneratePartSetNames()
{
    delete[] partSetDecoratedNames;

    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->partSets.size());
        partSetDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            partSetDecoratedNames[i] = curInstance->parts->GetPartSetDecorateName(i);
        }
    }
    else
        partSetDecoratedNames = nullptr;
}

void PatPartSetPane::RegeneratePartPropNames()
{
    delete[] partPropsDecoratedNames;

    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->partSets[curInstance->currState->partSet].groups.size());
        partPropsDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            partPropsDecoratedNames[i] = curInstance->parts->GetPartPropsDecorateName(curInstance->currState->partSet, i);
        }
    }
    else
        partPropsDecoratedNames = nullptr;
}

void PatPartSetPane::UpdateRenderFrame(int effectId, bool dontChangeProp)
{
    // Bounds check effectId
    if (effectId < 0 || effectId >= curInstance->parts->partSets.size()) {
        printf("[UpdateRenderFrame] ERROR: Invalid effectId %d (partSets.size=%zu)\n", 
            effectId, curInstance->parts->partSets.size());
        return;
    }
    
    curInstance->currState->partSet = effectId;
    if(curInstance->currState->animating) return;

    auto& frame = curInstance->framedata->get_sequence(0)->frames.at(0);
    // Ensure frame has at least one layer
    if (frame.AF.layers.empty()) {
        frame.AF.layers.push_back({});
    }

    if(*curInstance->renderMode != RenderMode::DEFAULT)
    {
        frame.AF.layers[0].spriteId = 0;
    }
    else
    {
        frame.AF.layers[0].spriteId = effectId;
        if(!dontChangeProp)
        {
            curInstance->currState->partProp = 0;
        }
    }
    curInstance->parts->partHighlight = activeHighligth ?
        curInstance->currState->partProp : -1;
    curInstance->parts->SetHighlightOpacity(highlightOpacity);
}

void PatPartSetPane::UpdateHighligth(bool state)
{
    activeHighligth = state;
    curInstance->parts->partHighlight = activeHighligth ?
            curInstance->currState->partProp : -1;
    curInstance->parts->SetHighlightOpacity(highlightOpacity);
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;
const char* const flipList[] = {
        "None",
        "Flip Horizontally",
        "Flip Vertically",
};

void PatPartSetPane::Draw()
{
    if(isVisible) {
        ImGui::Begin("PartSet Pane", 0);
        auto seq = curInstance->framedata->get_sequence(curInstance->currState->pattern);
        auto pat = curInstance->parts;
        if(curInstance->parts->loaded) {
            DrawPartSet();
            ImGui::End();
        } else {
            ImGui::Text("Load PAT file first");
        }
        ImGui::End();

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatPartSetPane::DrawPopUp() {
    if (ImGui::BeginPopupModal("PartSet Delete"))
    {
        ImGui::Text("Are you sure you want to delete this Part Set?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 40))) {
            curInstance->parts->partSets[curInstance->currState->partSet] = {};
            RegeneratePartSetNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("PartProps Delete"))
    {
        auto partSet = &curInstance->parts->partSets[curInstance->currState->partSet];
        ImGui::Text("Are you sure you want to delete this Part Property?");

        ImGui::Separator();

        if (ImGui::Button("Remove from list", ImVec2(120, 40))) {
            auto props = &partSet->groups;
            props->erase(props->begin()+curInstance->currState->partProp);
            for(int i = 0; i < props->size(); ++i) {
                auto prop = &props->at(i);
                if(prop->propId >= curInstance->currState->partProp) {
                    prop->propId = i;
                }
            }
            if(!props->empty()) {
                RegeneratePartPropNames();
                RegeneratePartSetNames();
                curInstance->currState->partProp = 0;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove data only", ImVec2(120, 40))) {
            partSet->groups[curInstance->currState->partProp] = {};
            RegeneratePartPropNames();
            RegeneratePartSetNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatPartSetPane::DrawPartSet()
{

        auto nPartSet = curInstance->parts->partSets.size();
        if(ImGui::Button("Add Part Set")){
            auto item = &curInstance->parts->partSets.emplace_back();
            item->partId = curInstance->parts->partSets.size() - 1;
            RegeneratePartSetNames();
            curInstance->currState->partSet = item->partId;
        }
        ImGui::SameLine();
        if(nPartSet > 0 && ImGui::Button("Delete Part Set")){
            openpopupWithId = "PartSet Delete";
        }
        if(nPartSet > 0) {
            // Safety check partSetDecoratedNames FIRST
            if (!partSetDecoratedNames) {
                printf("[PatPartSetPane] ERROR: partSetDecoratedNames is null, regenerating\n");
                RegeneratePartSetNames();
                if (!partSetDecoratedNames) {
                    printf("[PatPartSetPane] FATAL: Failed to generate partSetDecoratedNames\n");
                    return;  // Can't proceed without names
                }
            }
            
            // Bounds check currentPartSet
            if (curInstance->currState->partSet < 0 || curInstance->currState->partSet >= nPartSet) {
                printf("[PatPartSetPane] WARNING: partSet %d out of range [0, %d), resetting to 0\n",
                    curInstance->currState->partSet, (int)nPartSet);
                curInstance->currState->partSet = 0;
                UpdateRenderFrame(0);
            }
            
            if (ImGui::BeginCombo("Part Set", partSetDecoratedNames[curInstance->currState->partSet].c_str(),
                                  ImGuiComboFlags_HeightLargest)) {

                auto count = curInstance->parts->partSets.size();
                for (int n = 0; n < count; n++) {
                    const bool is_selected = (curInstance->currState->partSet == n);
                    if (ImGui::Selectable(partSetDecoratedNames[n].c_str(), is_selected)) {
                        printf("[PatPartSetPane] Switching to PartSet %d\n", n);
                        UpdateRenderFrame(n);
                        RegeneratePartPropNames();
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            auto partSet = curInstance->parts->GetPartSet(curInstance->currState->partSet);
            if (partSet) {
                if (ImGui::InputText("Part Set name", &partSet->name)) {
                    partSetDecoratedNames[curInstance->currState->partSet] = curInstance->parts->GetPartSetDecorateName(curInstance->currState->partSet);
                }
                if(ImGui::Checkbox("Highlight selected Part Prop", &activeHighligth))
                {
                    curInstance->parts->partHighlight = activeHighligth ?
                        curInstance->currState->partProp : -1;
                    curInstance->parts->SetHighlightOpacity(highlightOpacity);
                }
                if(activeHighligth)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(width);
                    if(ImGui::DragFloat("Opacity", &highlightOpacity, 0.01))
                    {
                        curInstance->parts->SetHighlightOpacity(highlightOpacity);
                    }
                }
                ImGui::NewLine();
                ImGui::SameLine(ImGui::GetWindowWidth()-210);
                if(ImGui::Button("Copy Part Set")){
                    partSet->CopyPartSetTo(&CopyManager::copiedParts->partSet);
                }
                ImGui::SameLine();
                if(ImGui::Button("Paste Part Set")){
                    CopyManager::copiedParts->partSet.CopyPartSetTo(partSet);
                    RegeneratePartPropNames();
                    RegeneratePartSetNames();
                }
                DrawPartProperty(partSet);
            }
        }


}

void PatPartSetPane::DrawPartProperty(PartSet<>* partSet)
{
    ImGui::Separator();
    ImGui::Text("Parts Property Data");
    int nparts = partSet->groups.size() - 1;
    if(ImGui::Button("Add Prop")){
        auto item = &partSet->groups.emplace_back();
        item->propId = partSet->groups.size() - 1;
        RegeneratePartPropNames();
        RegeneratePartSetNames();
        curInstance->currState->partProp = item->propId;
    }
    ImGui::SameLine();
    if(nparts >= 0 && ImGui::Button("Insert Prop")){
        auto props = &partSet->groups;
        PartProperty newItem = {};
        auto item = props->insert(props->begin()+curInstance->currState->partProp, newItem);
        for(int i = 0; i < props->size(); ++i) {
            auto prop = &props->at(i);
            if(prop->propId >= curInstance->currState->partProp) {
                prop->propId = i;
            }
        }
        item->propId = curInstance->currState->partProp;
        RegeneratePartPropNames();
        RegeneratePartSetNames();
        curInstance->currState->partProp = item->propId;
    }
    ImGui::SameLine();
    if(nparts >= 0 && ImGui::Button("Delete Prop")){
        openpopupWithId = "PartProps Delete";
    }
    if (nparts >= 0)
    {
        if (ImGui::BeginCombo("Parts",
                              partPropsDecoratedNames[curInstance->currState->partProp].c_str(),
                              ImGuiComboFlags_HeightLargest)) {

            auto count = partSet->groups.size();
            for (int n = 0; n < count; n++) {
                const bool is_selected = (curInstance->currState->partProp == n);
                if (ImGui::Selectable(partPropsDecoratedNames[n].c_str(), is_selected)) {
                    curInstance->currState->partProp = n;
                    curInstance->parts->partHighlight = activeHighligth ?
                        curInstance->currState->partProp : -1;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
                              }
        auto props = &partSet->groups[curInstance->currState->partProp];
        ImGui::NewLine();
        ImGui::SameLine(ImGui::GetWindowWidth()-230);
        if(ImGui::Button("Copy Part Prop")){
            PartSet<>::CopyPropertyTo(&CopyManager::copiedParts->partProperty, props);
        }
        ImGui::SameLine();
        if(ImGui::Button("Paste Part Prop")){
            PartSet<>::CopyPropertyTo(props, &CopyManager::copiedParts->partProperty);
            RegeneratePartPropNames();
            RegeneratePartSetNames();
        }
        if(ImGui::Button("Load Reference Data")){
            auto ppid = props->ppId;
            if(curInstance->parts->cutOuts.size() > ppid && ppid >= 0 )
            {
                curInstance->currState->partCutOut = ppid;
                auto cutOut = &curInstance->parts->cutOuts[curInstance->currState->partCutOut];
                auto shapeId = cutOut->shapeIndex;
                auto textureId = cutOut->texture;
                if(curInstance->parts->shapes.size() > shapeId && shapeId >= 0)
                    curInstance->currState->partShape = shapeId;
                if(curInstance->parts->gfxMeta.size() > textureId && textureId >= 0)
                    curInstance->currState->partGraph = textureId;
            }
        }

        ImGui::Separator();

        int pos[] = {props->x, props->y};
        if(ImGui::DragInt2("Position", pos, 1))
        {
            props->x = pos[0];
            props->y = pos[1];
        }
        float rotation[] = {props->rotation[1], props->rotation[2], props->rotation[3]};
        if(ImGui::DragFloat3("Rotation", rotation, 0.01))
        {
            props->rotation[1] = rotation[0];
            props->rotation[2] = rotation[1];
            props->rotation[3] = rotation[2];
        }
        float scale[] = {props->scaleX, props->scaleY};
        if(ImGui::DragFloat2("Scale", scale, 0.01))
        {
            props->scaleX = scale[0];
            props->scaleY = scale[1];
        }
        ImGui::SetNextItemWidth(widthInput);
        if(ImGui::InputInt("Part ID", &props->ppId, 1, 0)) {
            partPropsDecoratedNames[curInstance->currState->partProp] =
                    curInstance->parts->GetPartPropsDecorateName(curInstance->currState->partSet,curInstance->currState->partProp);
        }
        ImGui::SetNextItemWidth(widthInput);
        ImGui::DragFloat("Priority", &props->priority, 1.0f, 0.0f, 1000.0f);
        ImGui::Checkbox("Additive Filter", &props->additive);
        ImGui::SameLine();
        ImGui::Checkbox("Bilinear Filter", &props->filter);
        // Convert bgra bytes to float for UI, then back
        float rgba_float[4] = {
            props->bgra[2] / 255.f, // R from B position
            props->bgra[1] / 255.f, // G
            props->bgra[0] / 255.f, // B from R position  
            props->bgra[3] / 255.f  // A
        };
        if (ImGui::ColorEdit4("RGBA Color", rgba_float)) {
            props->bgra[2] = (unsigned char)(rgba_float[0] * 255.f); // R -> B position
            props->bgra[1] = (unsigned char)(rgba_float[1] * 255.f); // G
            props->bgra[0] = (unsigned char)(rgba_float[2] * 255.f); // B -> R position
            props->bgra[3] = (unsigned char)(rgba_float[3] * 255.f); // A
        }
        ImGui::ColorEdit4("Add Color", props->addColor);
        ImGui::Combo("Flip Side", &props->flip, flipList, IM_ARRAYSIZE(flipList));
    }
}
