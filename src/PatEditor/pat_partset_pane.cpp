#include "pat_partset_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <cmath>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"
#include "../ini.h"

PatPartSetPane::PatPartSetPane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    partSetDecoratedNames(nullptr),
                                                                    partPropsDecoratedNames(nullptr)
{
    // Highlight on/off and its opacity are preferences (saved in the ini), not per tab.
    activeHighligth = gSettings.patHighlight;
    highlightOpacity = gSettings.patHighlightOpacity;
}

void PatPartSetPane::SaveHighlightSettings()
{
    gSettings.patHighlight = activeHighligth;
    gSettings.patHighlightOpacity = highlightOpacity;
    ImGui::MarkIniSettingsDirty();
}

void PatPartSetPane::SyncNames()
{
    if(!curInstance->parts || !curInstance->parts->loaded) return;
    const int sets = (int)curInstance->parts->partSets.size();
    if(partSetNamesCount != sets || !partSetDecoratedNames)
        RegeneratePartSetNames();
    const auto* ps = curInstance->parts->GetPartSet(curInstance->currState->partSet);
    const int props = ps ? (int)ps->groups.size() : 0;
    if(partPropNamesSet != curInstance->currState->partSet || partPropNamesCount != props)
        RegeneratePartPropNames();
}

void PatPartSetPane::RegeneratePartSetNames()
{
    delete[] partSetDecoratedNames;

    partSetNamesCount = -1;
    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->partSets.size());
        partSetDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            partSetDecoratedNames[i] = curInstance->parts->GetPartSetDecorateName(i);
        }
        partSetNamesCount = count;
    }
    else
        partSetDecoratedNames = nullptr;
}

void PatPartSetPane::RegeneratePartPropNames()
{
    delete[] partPropsDecoratedNames;

    partPropNamesCount = -1;
    partPropNamesSet = -1;
    // The selected part set can be stale after undo / reload / a deleted part set: never index past the list.
    auto* partSet = curInstance->parts && curInstance->parts->loaded ? curInstance->parts->GetPartSet(curInstance->currState->partSet) : nullptr;
    if(partSet)
    {
        int count = static_cast<int>(partSet->groups.size());
        partPropsDecoratedNames = new std::string[count > 0 ? count : 1];

        for(int i = 0; i < count; i++)
        {
            partPropsDecoratedNames[i] = curInstance->parts->GetPartPropsDecorateName(curInstance->currState->partSet, i);
        }
        partPropNamesCount = count;
        partPropNamesSet = curInstance->currState->partSet;
    }
    else
        partPropsDecoratedNames = nullptr;
}

void PatPartSetPane::UpdateRenderFrame(int effectId, bool dontChangeProp)
{
    // Bounds check effectId
    if (effectId < 0 || effectId >= (int)curInstance->parts->partSets.size()) {
        return;
    }
    
    const bool changed = curInstance->currState->partSet != effectId;
    curInstance->currState->partSet = effectId;
    // A different part set has different parts: the selected part resets in EVERY render mode (sosfiro reset it in
    // the default view only, so UV / texture mode kept an index past the new set's list: TODO "UV render + part set
    // change crashes").
    if(changed && !dontChangeProp)
        curInstance->currState->partProp = 0;
    if(curInstance->currState->animating) return;
    if (curInstance->framedata->isHan2()) {   // the view draws currState->partSet; the character's frames are game data
        if (!dontChangeProp) curInstance->currState->partProp = 0;
        curInstance->parts->partHighlight = activeHighligth ? curInstance->currState->partProp : -1;
        curInstance->parts->SetHighlightOpacity(highlightOpacity);
        return;
    }

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
    }
    RegeneratePartPropNames();
    UpdateHighligth(activeHighligth);
}

// The renderer compares the highlight with each part's propId (Parts::Draw): use the selected part's propId, not its
// list index (they differ after Insert Prop / a file with gaps: TODO "adding a prop breaks the highlight id").
void PatPartSetPane::UpdateHighligth(bool state)
{
    activeHighligth = state;
    const auto* prop = curInstance->parts->GetPartProp(curInstance->currState->partSet, curInstance->currState->partProp);
    curInstance->parts->partHighlight = activeHighligth && prop ? prop->propId : -1;
    curInstance->parts->SetHighlightOpacity(highlightOpacity);
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;
// PRRV is a UV mirror bitmask (uni2.exe PatPart_GetCutoutUVWithFlip 0x413C50): 1 = H, 2 = V, 3 = both.
const char* const flipList[] = {
        "None",
        "Flip Horizontally",
        "Flip Vertically",
        "Flip Both",
};
// PRAL byte (PatPart_SetBlendFilterGlobals_PRAL_PRFL 0x413840).
const char* const blendList[] = {
        "Normal",
        "Additive",
        "Subtractive",
        "Blur mask (normal)",
};

void PatPartSetPane::Draw()
{
    if(isVisible) {
        ImGui::Begin(LBL("PartSet Pane"), 0);
        if(curInstance->parts->loaded) {
            SyncNames();
            if (curInstance->parts->fbPartModel) DrawFbPoses();
            else DrawPartSet();
        } else {
            ImGui::Text(TXT("Load PAT file first"));
        }
        ImGui::End();   // one End per Begin (the old code called it twice when a PAT was loaded)

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
        DrawRenumberPopup();
    }
}

void PatPartSetPane::DrawPopUp() {
    if (ImGui::BeginPopupModal(LBL("PartSet Delete")))
    {
        ImGui::Text(TXT("Are you sure you want to delete this Part Set?"));

        ImGui::Separator();

        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {
            if (auto* ps = curInstance->parts->GetPartSet(curInstance->currState->partSet)) {
                const int id = ps->partId, orig = ps->origId;
                const bool loaded = ps->wasLoaded;
                *ps = {};
                ps->partId = id;   // the slot keeps its id (the file is indexed by it)
                ps->origId = orig; ps->wasLoaded = loaded;   // and its record: written as an empty stub
            }
            curInstance->currState->partProp = 0;
            RegeneratePartSetNames();
            RegeneratePartPropNames();
            UpdateHighligth(activeHighligth);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(LBL("PartProps Delete")))
    {
        auto partSet = curInstance->parts->GetPartSet(curInstance->currState->partSet);
        ImGui::Text(TXT("Are you sure you want to delete this Part Property?"));

        ImGui::Separator();

        const int sel = curInstance->currState->partProp;
        const bool valid = partSet && sel >= 0 && sel < (int)partSet->groups.size();
        if (ImGui::Button(LBL("Remove from list"), ImVec2(120, 40))) {
            if (valid) {
                auto props = &partSet->groups;
                props->erase(props->begin()+sel);
                for(int i = 0; i < (int)props->size(); ++i) {
                    auto prop = &props->at(i);
                    if(prop->propId >= sel) {
                        prop->propId = i;
                    }
                }
                curInstance->currState->partProp = 0;
                RegeneratePartPropNames();
                RegeneratePartSetNames();
                UpdateHighligth(activeHighligth);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Remove data only"), ImVec2(120, 40))) {
            if (valid) {
                const int id = partSet->groups[sel].propId;
                partSet->groups[sel] = {};
                partSet->groups[sel].propId = id;
                RegeneratePartPropNames();
                RegeneratePartSetNames();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatPartSetPane::DrawPartSet()
{

        auto nPartSet = curInstance->parts->partSets.size();
        if(ImGui::Button(LBL("Add Part Set"))){
            auto item = &curInstance->parts->partSets.emplace_back();
            item->partId = curInstance->parts->partSets.size() - 1;
            RegeneratePartSetNames();
            // Show the new (empty) set and refresh the part list and highlight with it (TODO "render not reset after
            // adding a part set", "highlight applied to every prop after adding a part set").
            UpdateRenderFrame(item->partId);
        }
        ImGui::SameLine();
        if(nPartSet > 0 && ImGui::Button(LBL("Delete Part Set"))){
            openpopupWithId = LBL("PartSet Delete");
        }
        if(nPartSet > 0) {
            // Bounds check currentPartSet
            if (curInstance->currState->partSet < 0 || curInstance->currState->partSet >= (int)nPartSet) {
                curInstance->currState->partSet = 0;
                UpdateRenderFrame(0);
            }
            SyncNames();
            if (!partSetDecoratedNames || partSetNamesCount != (int)curInstance->parts->partSets.size())
                return;  // Can't proceed without names
            
            if (ImGui::BeginCombo(LBL("Part Set"), partSetDecoratedNames[curInstance->currState->partSet].c_str(),
                                  ImGuiComboFlags_HeightLargest)) {

                auto count = curInstance->parts->partSets.size();
                for (int n = 0; n < count; n++) {
                    const bool is_selected = (curInstance->currState->partSet == n);
                    if (ImGui::Selectable(partSetDecoratedNames[n].c_str(), is_selected)) {
                        UpdateRenderFrame(n);
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            auto partSet = curInstance->parts->GetPartSet(curInstance->currState->partSet);
            if (partSet) {
                if (ImGui::InputText(LBL("Part Set name"), &partSet->name)) {
                    partSetDecoratedNames[curInstance->currState->partSet] = curInstance->parts->GetPartSetDecorateName(curInstance->currState->partSet);
                }
                if(ImGui::Checkbox(LBL("Highlight selected Part Prop"), &activeHighligth))
                {
                    UpdateHighligth(activeHighligth);
                    SaveHighlightSettings();
                }
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", TXT("H toggles it in the view. The other parts are drawn at the opacity on the right."));
                if(activeHighligth)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(width);
                    if(ImGui::DragFloat(LBL("Opacity"), &highlightOpacity, 0.01f, 0.f, 1.f))
                    {
                        curInstance->parts->SetHighlightOpacity(highlightOpacity);
                        SaveHighlightSettings();
                    }
                }
                ImGui::NewLine();
                ImGui::SameLine(i18n::RightPairX(LBL("Copy Part Set"), LBL("Paste Part Set")));
                if(ImGui::Button(LBL("Copy Part Set"))){
                    partSet->CopyPartSetTo(&CopyManager::copiedParts->partSet);
                }
                ImGui::SameLine();
                if(ImGui::Button(LBL("Paste Part Set"))){
                    CopyManager::copiedParts->partSet.CopyPartSetTo(partSet);
                    if (curInstance->currState->partProp >= (int)partSet->groups.size())
                        curInstance->currState->partProp = 0;
                    RegeneratePartPropNames();
                    RegeneratePartSetNames();
                    UpdateHighligth(activeHighligth);
                }
                DrawRenumber();
                DrawPartProperty(partSet);
            }
        }


}

void PatPartSetPane::DrawPartProperty(PartSet<>* partSet)
{
    ImGui::Separator();
    ImGui::Text(TXT("Parts Property Data"));
    int nparts = partSet->groups.size() - 1;
    if(ImGui::Button(LBL("Add Prop"))){
        auto item = &partSet->groups.emplace_back();
        item->propId = partSet->groups.size() - 1;
        curInstance->currState->partProp = (int)partSet->groups.size() - 1;
        RegeneratePartPropNames();
        RegeneratePartSetNames();
        UpdateHighligth(activeHighligth);
    }
    ImGui::SameLine();
    if(nparts >= 0 && ImGui::Button(LBL("Insert Prop"))){
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
        UpdateHighligth(activeHighligth);
    }
    ImGui::SameLine();
    if(nparts >= 0 && ImGui::Button(LBL("Delete Prop"))){
        openpopupWithId = LBL("PartProps Delete");
    }
    if (curInstance->currState->partProp < 0 || curInstance->currState->partProp > nparts)
        curInstance->currState->partProp = 0;
    if (nparts >= 0 && (!partPropsDecoratedNames || partPropNamesCount != nparts + 1))
        RegeneratePartPropNames();
    if (nparts >= 0 && partPropsDecoratedNames)
    {
        if (ImGui::BeginCombo(LBL("Parts"),
                              partPropsDecoratedNames[curInstance->currState->partProp].c_str(),
                              ImGuiComboFlags_HeightLargest)) {

            auto count = partSet->groups.size();
            for (int n = 0; n < count; n++) {
                const bool is_selected = (curInstance->currState->partProp == n);
                if (ImGui::Selectable(partPropsDecoratedNames[n].c_str(), is_selected)) {
                    curInstance->currState->partProp = n;
                    UpdateHighligth(activeHighligth);
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
                              }
        auto props = &partSet->groups[curInstance->currState->partProp];
        ImGui::NewLine();
        ImGui::SameLine(i18n::RightPairX(LBL("Copy Part Prop"), LBL("Paste Part Prop")));
        if(ImGui::Button(LBL("Copy Part Prop"))){
            PartSet<>::CopyPropertyTo(&CopyManager::copiedParts->partProperty, props);
        }
        ImGui::SameLine();
        if(ImGui::Button(LBL("Paste Part Prop"))){
            // The pasted prop keeps this slot's id (a copied id would make two parts answer to one highlight).
            const int keepId = props->propId;
            PartSet<>::CopyPropertyTo(props, &CopyManager::copiedParts->partProperty);
            props->propId = keepId;
            RegeneratePartPropNames();
            RegeneratePartSetNames();
            UpdateHighligth(activeHighligth);
        }
        if(ImGui::Button(LBL("Load Reference Data"))){
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
        if(ImGui::DragInt2(LBL("Position"), pos, 1))
        {
            props->x = pos[0];
            props->y = pos[1];
        }
        float rotation[] = {props->rotation[1], props->rotation[2], props->rotation[3]};
        if(ImGui::DragFloat3(LBL("Rotation"), rotation, 0.01))
        {
            props->rotation[1] = rotation[0];
            props->rotation[2] = rotation[1];
            props->rotation[3] = rotation[2];
        }
        if(ImGui::IsItemHovered())
            Tooltip("PRA3 X, Y, Z in turns (1 = 360 degrees).");
        ImGui::SetNextItemWidth(widthInput);
        ImGui::DragFloat(LBL("PRA3 [0]"), &props->rotation[0], 0.01f);
        if(ImGui::IsItemHovered())
            Tooltip("First PRA3 value. Kept in the file; the games' part matrix does not read it.");
        ImGui::DragInt2(LBL("Pivot (PRAS)"), props->pras, 1);
        if(ImGui::IsItemHovered())
            Tooltip("PRAS: offset applied between scale and rotation (UNI2). MBTL does not read it.");
        float scale[] = {props->scaleX, props->scaleY};
        if(ImGui::DragFloat2(LBL("Scale"), scale, 0.01))
        {
            props->scaleX = scale[0];
            props->scaleY = scale[1];
        }
        ImGui::SetNextItemWidth(widthInput);
        if(ImGui::InputInt(LBL("Part ID"), &props->ppId, 1, 0)) {
            partPropsDecoratedNames[curInstance->currState->partProp] =
                    curInstance->parts->GetPartPropsDecorateName(curInstance->currState->partSet,curInstance->currState->partProp);
        }
        // PRPR is an int; the model keeps the slot / 1000 as a draw-order tiebreak, which is not shown.
        {
            const float frac = props->priority - std::floor(props->priority);
            int prio = (int)std::floor(props->priority);
            ImGui::SetNextItemWidth(widthInput);
            if(ImGui::InputInt(LBL("Priority"), &prio, 1, 10))
                props->priority = (float)prio + frac;
            if(ImGui::IsItemHovered())
                Tooltip("PRPR. Higher draws first (further back); negative values are allowed.");
        }
        {
            int blend = props->additive ? (props->pral ? props->pral : 1) : 0;
            const bool known = blend >= 0 && blend < IM_ARRAYSIZE(blendList);
            ImGui::SetNextItemWidth(widthInput);
            if(ImGui::BeginCombo(LBL("Blend (PRAL)"), known ? blendList[blend] : "Other")) {
                for(int n = 0; n < IM_ARRAYSIZE(blendList); n++) {
                    if(ImGui::Selectable(blendList[n], n == blend)) {
                        props->pral = (unsigned char)n;
                        props->additive = n != 0;
                    }
                }
                ImGui::EndCombo();
            }
            if(ImGui::IsItemHovered())
                Tooltip("PRAL: 1 = additive, 2 = subtractive (reverse subtract), 3 = drawn normally and used\n"
                        "by the game as the mask of a screen blur (the blur is not previewed).");
        }
        ImGui::SameLine();
        ImGui::Checkbox(LBL("Bilinear Filter"), &props->filter);
        {
            int prpa = props->prpa;
            ImGui::SetNextItemWidth(widthInput);
            if(ImGui::InputInt(LBL("PRPA"), &prpa, 1, 10)) {
                props->prpa = prpa;
                props->hasPrpa = true;
            }
            if(ImGui::IsItemHovered())
                Tooltip("PRPA (UNI2/MBTL, part +16). Kept as loaded; no draw use found in the games.");
        }
        // Convert bgra bytes to float for UI, then back
        float rgba_float[4] = {
            props->bgra[2] / 255.f, // R from B position
            props->bgra[1] / 255.f, // G
            props->bgra[0] / 255.f, // B from R position  
            props->bgra[3] / 255.f  // A
        };
        if (ImGui::ColorEdit4(LBL("RGBA Color"), rgba_float)) {
            props->bgra[2] = (unsigned char)(rgba_float[0] * 255.f); // R -> B position
            props->bgra[1] = (unsigned char)(rgba_float[1] * 255.f); // G
            props->bgra[0] = (unsigned char)(rgba_float[2] * 255.f); // B -> R position
            props->bgra[3] = (unsigned char)(rgba_float[3] * 255.f); // A
        }
        ImGui::ColorEdit4(LBL("Add Color"), props->addColor);
        if(ImGui::IsItemHovered())
            Tooltip("PRSP specular colour, added to the part's output (alpha included).");
        i18n::Combo(LBL("Flip Side"), &props->flip, flipList, IM_ARRAYSIZE(flipList));
    }
}
