#include "pat_part_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"

PatPartPane::PatPartPane(Render* render, StateReference *curInstance, PatPartSetPane* partsetPaneRef) : DrawWindow(render, curInstance),
                                                                    cutOutsDecoratedNames(nullptr)
{
    partsetPane = partsetPaneRef;
}

void PatPartPane::RegeneratePartCutOutsNames()
{
    delete[] cutOutsDecoratedNames;

    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->cutOuts.size());
        cutOutsDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            cutOutsDecoratedNames[i] = curInstance->parts->GetPartCutOutsDecorateName(i);
        }
    }
    else
        cutOutsDecoratedNames = nullptr;
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;

void PatPartPane::Draw()
{
    if(isVisible) {
        ImGui::Begin("Part Pane", 0);
        auto seq = curInstance->framedata->get_sequence(curInstance->currState->pattern);
        auto pat = curInstance->parts;
        if(curInstance->parts->loaded) {
            DrawPartCutOut();
            ImGui::End();
        }
        ImGui::End();

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatPartPane::DrawPopUp() {

    if (ImGui::BeginPopupModal("Part Delete"))
    {
        ImGui::Text("Are you sure you want to delete this Part?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 40))) {
            curInstance->parts->cutOuts[curInstance->currState->partCutOut] = {};
            partsetPane->RegeneratePartSetNames();
            partsetPane->RegeneratePartPropNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

const char* labelTL = "TL";
const char* labelTC = "TC";
const char* labelTR = "TR";
const char* labelCL = "CL";
const char* labelCenter = "C0";
const char* labelCR = "CR";
const char* labelBL = "BL";
const char* labelBC = "BC";
const char* labelBR = "BR";

void PatPartPane::DrawPartCutOut()
{
    auto npartData = curInstance->parts->cutOuts.size();
        if(ImGui::Button("Add Part")){
            auto item = &curInstance->parts->cutOuts.emplace_back();
            item->id = curInstance->parts->cutOuts.size() - 1;
            RegeneratePartCutOutsNames();
            partsetPane->RegeneratePartPropNames();
            curInstance->currState->partCutOut = item->id;
        }
        ImGui::SameLine();
        if(npartData > 0 && ImGui::Button("Delete Part")){
            openpopupWithId = "Part Delete";
        }

        if (ImGui::BeginCombo("Part", cutOutsDecoratedNames[curInstance->currState->partCutOut].c_str(),
                              ImGuiComboFlags_HeightLargest)) {

            auto count = curInstance->parts->cutOuts.size();
            for (int n = 0; n < count; n++) {
                const bool is_selected = (curInstance->currState->partCutOut == n);
                if (ImGui::Selectable(cutOutsDecoratedNames[n].c_str(), is_selected)) {
                    curInstance->currState->partCutOut = n;
                    RegeneratePartCutOutsNames();
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        if(npartData > 0) {
            auto cutOut = &curInstance->parts->cutOuts[curInstance->currState->partCutOut];
            if (cutOut) {
                if (ImGui::InputText("Part name", &cutOut->name)) {
                    cutOutsDecoratedNames[curInstance->currState->partCutOut] = curInstance->parts->GetPartCutOutsDecorateName(
                            curInstance->currState->partCutOut);
                }
                ImGui::NewLine();
                ImGui::SameLine(ImGui::GetWindowWidth()-170);
                if(ImGui::Button("Copy Part")){
                    cutOut->CopyTo(&CopyManager::copiedParts->cutOut);
                }
                ImGui::SameLine();
                if(ImGui::Button("Paste Part")){
                    CopyManager::copiedParts->cutOut.CopyTo(cutOut);
                    RegeneratePartCutOutsNames();
                }

                if (ImGui::Button("Load Reference Data")) {
                    auto shapeId = cutOut->shapeIndex;
                    auto textureId = cutOut->texture;
                    if (curInstance->parts->shapes.size() > shapeId && shapeId >= 0)
                        curInstance->currState->partShape = shapeId;
                    if (curInstance->parts->gfxMeta.size() > textureId && textureId >= 0)
                        curInstance->currState->partGraph = textureId;
                }
                ImGui::Separator();
                ImGui::Text("UV Settings");
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt("Top", &cutOut->uv[0], 1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt("Width", &cutOut->uv[2], 1);
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt("Left", &cutOut->uv[1], 1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt("Height", &cutOut->uv[3], 1);
                if(ImGui::Button(*curInstance->renderMode == RenderMode::UV_SETTING_VIEW ?
                    "Hide UV Render" : "Show UV Render"))
                {
                    *curInstance->renderMode = *curInstance->renderMode == RenderMode::UV_SETTING_VIEW ?
                        RenderMode::DEFAULT : RenderMode::UV_SETTING_VIEW;
                    if(*curInstance->renderMode != RenderMode::DEFAULT)
                    {
                        curInstance->framedata->get_sequence(0)->frames.at(0).AF.spriteId = 0;
                    }
                    else
                    {
                        curInstance->framedata->get_sequence(0)->frames.at(0).AF.spriteId = curInstance->currState->partSet;
                    }
                }
                ImGui::Separator();

                ImGui::SetNextItemWidth(widthInput);
                if(ImGui::InputInt("Texture ID", &cutOut->texture, 1, 0))
                {
                    if(cutOut->texture < curInstance->parts->gfxMeta.size()) {
                        auto gfx = &curInstance->parts->gfxMeta[cutOut->texture];
                        cutOut->ppte[0] = gfx->pgte[0];
                        cutOut->ppte[1] = gfx->pgte[1];
                    }
                    else
                    {
                        cutOut->ppte[0] = 0;
                        cutOut->ppte[1] = 0;
                    }
                }
                ImGui::SetNextItemWidth(widthInput);
                ImGui::InputInt("Shape ID", &cutOut->shapeIndex, 1, 0);
                ImGui::SetNextItemWidth(widthInput);
                ImGui::InputInt("Color Slot", &cutOut->colorSlot, 1, 0);

                int pos[] = {cutOut->xy[0], cutOut->xy[1]};
                if(ImGui::DragInt2("Center Pos.", pos, 1))
                {
                    cutOut->xy[0] = pos[0];
                    cutOut->xy[1] = pos[1];
                }
                int wh[] = {cutOut->wh[0], cutOut->wh[1]};
                if(ImGui::DragInt2("Width/Height", wh, 1))
                {
                    cutOut->wh[0] = wh[0];
                    cutOut->wh[1] = wh[1];
                }
                auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
                if(ImGui::Button("Set WH with UV"))
                {
                    if(gfx != nullptr)
                    {
                        cutOut->wh[0] = cutOut->uv[2] * gfx->uvBpp[0];
                        cutOut->wh[1] = cutOut->uv[3] * gfx->uvBpp[1];
                    }
                }

                ImGui::Text("Set Center Position with WH");
                SetButtonCenterCalc(labelTL, new int[]{0,0}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelTC, new int[]{cutOut->wh[0] / 2,0}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelTR, new int[]{cutOut->wh[0],0}, cutOut);
                SetButtonCenterCalc(labelCL, new int[]{0,cutOut->wh[1] / 2}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelCenter, new int[]{cutOut->wh[0] / 2,cutOut->wh[1] / 2}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelCR, new int[]{cutOut->wh[0],cutOut->wh[1] / 2}, cutOut);
                SetButtonCenterCalc(labelBL, new int[]{0,cutOut->wh[1]}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelBC, new int[]{cutOut->wh[0] / 2,cutOut->wh[1]}, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelBR, new int[]{cutOut->wh[0],cutOut->wh[1]}, cutOut);

                ImGui::Checkbox("Show unknown params", &isUnkParams);
                if(isUnkParams)
                {
                    int ppjp[] = {cutOut->ppjp[0], cutOut->ppjp[1]};
                    if(ImGui::InputInt2("PPJP", ppjp))
                    {
                        cutOut->ppjp[0] = ppjp[0];
                        cutOut->ppjp[1] = ppjp[1];
                    }
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt("PPTX", &cutOut->pptx, 1, 0);
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt("PPTE W", &cutOut->ppte[0], 1, 0, ImGuiInputTextFlags_ReadOnly);
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt("PPTE H", &cutOut->ppte[1], 1, 0, ImGuiInputTextFlags_ReadOnly);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if(ImGui::IsItemHovered())
                        Tooltip("PPTE is obtained from texture PGTE values");
                }
            }
        }
}

void PatPartPane::SetButtonCenterCalc(std::string label, int xy[], CutOut<>* cutOut)
{
    if(ImGui::Button(label.c_str()))
    {
        cutOut->xy[0] = xy[0];
        cutOut->xy[1] = xy[1];
    }
}

void PatPartPane::BoxStart(int x, int y)
{
    if(!curInstance->parts->loaded || *curInstance->renderMode != RenderMode::UV_SETTING_VIEW) return;
    auto cutOut = curInstance->parts->GetCutOut(curInstance->currState->partCutOut);
    if(cutOut == nullptr) return;
    auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
    if(gfx == nullptr) return;

    cutOut->uv[0] = x / gfx->uvBpp[0];
    cutOut->uv[2] = 0;
    cutOut->uv[1] = y / gfx->uvBpp[1];
    cutOut->uv[3] = 0;

    dragxy[0] = x;
    dragxy[1] = y;
}

void PatPartPane::BoxDrag(int x, int y)
{
    if(!curInstance->parts->loaded || *curInstance->renderMode != RenderMode::UV_SETTING_VIEW) return;
    auto cutOut = curInstance->parts->GetCutOut(curInstance->currState->partCutOut);
    if(cutOut == nullptr) return;
    auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
    if(gfx == nullptr) return;

    dragxy[0] += x/render->scale;
    dragxy[1] += y/render->scale;

    int newWidth = dragxy[0] / gfx->uvBpp[0] - cutOut->uv[0];
    int newHeight = dragxy[1] / gfx->uvBpp[1] - cutOut->uv[1];

    if(newWidth < 0 || newHeight < 0) return;
    //if((int)dragxy[0] % gfx->uvBpp[0] != 0 || (int)dragxy[1] % gfx->uvBpp[1] != 0 ) return;

    cutOut->uv[2] = newWidth;
    cutOut->uv[3] = newHeight;
}
