#include "pat_part_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <algorithm>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"

PatPartPane::PatPartPane(Render* render, StateReference *curInstance, PatPartSetPane* partsetPaneRef) : DrawWindow(render, curInstance),
                                                                    cutOutsDecoratedNames(nullptr)
{
    partsetPane = partsetPaneRef;
}

void PatPartPane::RegeneratePartCutOutsNames()
{
    delete[] cutOutsDecoratedNames;

    cutOutsNamesCount = -1;
    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->cutOuts.size());
        cutOutsDecoratedNames = new std::string[count > 0 ? count : 1];

        for(int i = 0; i < count; i++)
        {
            cutOutsDecoratedNames[i] = curInstance->parts->GetPartCutOutsDecorateName(i);
        }
        cutOutsNamesCount = count;
    }
    else
        cutOutsDecoratedNames = nullptr;
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;

void PatPartPane::Draw()
{
    if(isVisible) {
        ImGui::Begin(LBL("Part Pane"), 0);
        if(curInstance->parts->loaded) {
            if(cutOutsNamesCount != (int)curInstance->parts->cutOuts.size() || !cutOutsDecoratedNames)
                RegeneratePartCutOutsNames();
            DrawPartCutOut();
        }
        ImGui::End();   // one End per Begin

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatPartPane::DrawPopUp() {

    if (ImGui::BeginPopupModal(LBL("Part Delete")))
    {
        ImGui::Text(TXT("Are you sure you want to delete this Part?"));

        ImGui::Separator();

        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {
            if (auto* co = curInstance->parts->GetCutOut(curInstance->currState->partCutOut)) {
                const int id = co->id;
                *co = {};
                co->id = id;
            }
            RegeneratePartCutOutsNames();
            partsetPane->RegeneratePartSetNames();
            partsetPane->RegeneratePartPropNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
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
        if(ImGui::Button(LBL("Add Part"))){
            auto item = &curInstance->parts->cutOuts.emplace_back();
            item->id = curInstance->parts->cutOuts.size() - 1;
            RegeneratePartCutOutsNames();
            partsetPane->RegeneratePartPropNames();
            curInstance->currState->partCutOut = item->id;
        }
        ImGui::SameLine();
        if(npartData > 0 && ImGui::Button(LBL("Delete Part"))){
            openpopupWithId = LBL("Part Delete");
        }

        if (curInstance->currState->partCutOut < 0 || curInstance->currState->partCutOut >= (int)npartData)
            curInstance->currState->partCutOut = 0;
        if (npartData > 0 && ImGui::BeginCombo(LBL("Part"), cutOutsDecoratedNames[curInstance->currState->partCutOut].c_str(),
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
                if (ImGui::InputText(LBL("Part name"), &cutOut->name)) {
                    cutOutsDecoratedNames[curInstance->currState->partCutOut] = curInstance->parts->GetPartCutOutsDecorateName(
                            curInstance->currState->partCutOut);
                }
                ImGui::NewLine();
                ImGui::SameLine(i18n::RightPairX(LBL("Copy Part"), LBL("Paste Part")));
                if(ImGui::Button(LBL("Copy Part"))){
                    cutOut->CopyTo(&CopyManager::copiedParts->cutOut);
                }
                ImGui::SameLine();
                if(ImGui::Button(LBL("Paste Part"))){
                    CopyManager::copiedParts->cutOut.CopyTo(cutOut);
                    RegeneratePartCutOutsNames();
                }

                if (ImGui::Button(LBL("Load Reference Data"))) {
                    auto shapeId = cutOut->shapeIndex;
                    auto textureId = cutOut->texture;
                    if (curInstance->parts->shapes.size() > shapeId && shapeId >= 0)
                        curInstance->currState->partShape = shapeId;
                    if (curInstance->parts->gfxMeta.size() > textureId && textureId >= 0)
                        curInstance->currState->partGraph = textureId;
                }
                ImGui::Separator();
                ImGui::Text(TXT("UV Settings"));
                // PPUV = x, y, w, h in 1/256 of the texture: uv[0] is the LEFT edge, uv[1] the TOP edge (sosfiro's labels
                // were swapped, its TODO list: "UV Top should be X and Left should be Y").
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt(LBL("Left"), &cutOut->uv[0], 1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt(LBL("Width"), &cutOut->uv[2], 1);
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt(LBL("Top"), &cutOut->uv[1], 1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(width);
                ImGui::DragInt(LBL("Height"), &cutOut->uv[3], 1);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", TXT("In the UV view, drag with the right mouse button over the texture to set the rectangle."));
                if(ImGui::Button(*curInstance->renderMode == RenderMode::UV_SETTING_VIEW ?
                    LBL("Hide UV Render") : LBL("Show UV Render")))
                {
                    *curInstance->renderMode = *curInstance->renderMode == RenderMode::UV_SETTING_VIEW ?
                        RenderMode::DEFAULT : RenderMode::UV_SETTING_VIEW;

                    if (!curInstance->framedata->isHan2()) {   // RBO / GOF2: the view follows the editor state, frames stay untouched
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
                        frame.AF.layers[0].spriteId = curInstance->currState->partSet;
                    }
                    }
                }
                ImGui::Separator();

                ImGui::SetNextItemWidth(widthInput);
                if(ImGui::InputInt(LBL("Texture ID"), &cutOut->texture, 1, 0))
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
                ImGui::InputInt(LBL("Shape ID"), &cutOut->shapeIndex, 1, 0);
                ImGui::SetNextItemWidth(widthInput);
                ImGui::InputInt(LBL("Color Slot"), &cutOut->colorSlot, 1, 0);

                int pos[] = {cutOut->xy[0], cutOut->xy[1]};
                if(ImGui::DragInt2(LBL("Center Pos."), pos, 1))
                {
                    cutOut->xy[0] = pos[0];
                    cutOut->xy[1] = pos[1];
                }
                int wh[] = {cutOut->wh[0], cutOut->wh[1]};
                if(ImGui::DragInt2(LBL("Width/Height"), wh, 1))
                {
                    cutOut->wh[0] = wh[0];
                    cutOut->wh[1] = wh[1];
                }
                auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
                if(ImGui::Button(LBL("Set WH with UV")))
                {
                    // UV is in 1/256 of the texture whatever its size (Parts::Draw divides by 256), so the size in
                    // pixels is uv * w / 256. uvBpp (w / 256) is 0 for MBAACC PGTX textures.
                    if(gfx != nullptr && gfx->w > 0 && gfx->h > 0)
                    {
                        cutOut->wh[0] = cutOut->uv[2] * gfx->w / 256;
                        cutOut->wh[1] = cutOut->uv[3] * gfx->h / 256;
                    }
                }

                // The 3 x 3 anchor grid (values are computed per click; the old code allocated nine int arrays every
                // frame and never freed them: sosfiro's "weird memory leak while animating").
                ImGui::Text(TXT("Set Center Position with WH"));
                const int w = cutOut->wh[0], h = cutOut->wh[1];
                SetButtonCenterCalc(labelTL, 0, 0, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelTC, w / 2, 0, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelTR, w, 0, cutOut);
                SetButtonCenterCalc(labelCL, 0, h / 2, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelCenter, w / 2, h / 2, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelCR, w, h / 2, cutOut);
                SetButtonCenterCalc(labelBL, 0, h, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelBC, w / 2, h, cutOut);
                ImGui::SameLine();
                SetButtonCenterCalc(labelBR, w, h, cutOut);

                ImGui::Checkbox(LBL("Show unknown params"), &isUnkParams);
                if(isUnkParams)
                {
                    int ppjp[] = {cutOut->ppjp[0], cutOut->ppjp[1]};
                    if(ImGui::InputInt2(LBL("PPJP"), ppjp))
                    {
                        cutOut->ppjp[0] = ppjp[0];
                        cutOut->ppjp[1] = ppjp[1];
                    }
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt(LBL("PPTX"), &cutOut->pptx, 1, 0);
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt(LBL("PPTE W"), &cutOut->ppte[0], 1, 0, ImGuiInputTextFlags_ReadOnly);
                    ImGui::SetNextItemWidth(widthInput);
                    ImGui::InputInt(LBL("PPTE H"), &cutOut->ppte[1], 1, 0, ImGuiInputTextFlags_ReadOnly);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if(ImGui::IsItemHovered())
                        Tooltip("PPTE is obtained from texture PGTE values");
                }
            }
        }
}

void PatPartPane::SetButtonCenterCalc(const char* label, int x, int y, CutOut<>* cutOut)
{
    if(ImGui::Button(label))
    {
        cutOut->xy[0] = x;
        cutOut->xy[1] = y;
    }
}

bool PatPartPane::BoxStart(float px, float py)
{
    if(!curInstance->parts->loaded || *curInstance->renderMode != RenderMode::UV_SETTING_VIEW) return false;
    auto cutOut = curInstance->parts->GetCutOut(curInstance->currState->partCutOut);
    if(cutOut == nullptr) return false;
    auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
    if(gfx == nullptr || gfx->w <= 0 || gfx->h <= 0) return false;

    dragxy[0] = px;
    dragxy[1] = py;
    BoxDrag(px, py);
    return true;
}

// Sheet pixels -> PPUV (1/256 of the texture), snapped to whole UV units, kept inside the texture.
void PatPartPane::BoxDrag(float px, float py)
{
    if(!curInstance->parts->loaded || *curInstance->renderMode != RenderMode::UV_SETTING_VIEW) return;
    auto cutOut = curInstance->parts->GetCutOut(curInstance->currState->partCutOut);
    if(cutOut == nullptr) return;
    auto gfx = curInstance->parts->GetPartGfx(cutOut->texture);
    if(gfx == nullptr || gfx->w <= 0 || gfx->h <= 0) return;

    auto toUv = [](float pixel, int size) {
        const float u = pixel * 256.f / (float)size;
        return u < 0.f ? 0 : u > 256.f ? 256 : (int)(u + 0.5f);
    };
    const int x0 = toUv(std::min(dragxy[0], px), gfx->w), x1 = toUv(std::max(dragxy[0], px), gfx->w);
    const int y0 = toUv(std::min(dragxy[1], py), gfx->h), y1 = toUv(std::max(dragxy[1], py), gfx->h);
    cutOut->uv[0] = x0;
    cutOut->uv[1] = y0;
    cutOut->uv[2] = x1 - x0;
    cutOut->uv[3] = y1 - y0;
}
