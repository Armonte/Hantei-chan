#include "pat_texture_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"

PatTexturePane::PatTexturePane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    textureDecoratedNames(nullptr)
{

}

void PatTexturePane::RegenerateTexturesNames()
{
    delete[] textureDecoratedNames;

    textureNamesCount = -1;
    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->gfxMeta.size());
        textureDecoratedNames = new std::string[count > 0 ? count : 1];

        for(int i = 0; i < count; i++)
        {
            textureDecoratedNames[i] = curInstance->parts->GetTexturesDecorateName(i);
        }
        textureNamesCount = count;
    }
    else
        textureDecoratedNames = nullptr;
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;
const char* const flipList[] = {
        "None",
        "Flip Horizontally",
        "Flip Vertically",
};
const char* const shapeList[] = {
        "Plane",
        "Unk2",
        "Ring",
        "Arc",
        "Sphere",
        "Cone"
};

void PatTexturePane::Draw()
{
    if(isVisible) {
        ImGui::Begin(LBL("Texture Pane"), 0);
        if(curInstance->parts->loaded) {
            if(textureNamesCount != (int)curInstance->parts->gfxMeta.size() || !textureDecoratedNames)
                RegenerateTexturesNames();
            int& sel = curInstance->currState->partGraph;
            if(sel < 0 || sel >= (int)curInstance->parts->gfxMeta.size()) sel = 0;
            DrawTexture();
        }
        ImGui::End();   // one End per Begin

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatTexturePane::DrawPopUp() {
    if (ImGui::BeginPopupModal(LBL("Texture Delete")))
    {
        ImGui::Text(TXT("Are you sure you want to delete this Part?"));

        ImGui::Separator();

        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {
            if (auto* g = curInstance->parts->GetPartGfx(curInstance->currState->partGraph)) {
                const int id = g->id;
                *g = {};
                g->id = id;
            }
            RegenerateTexturesNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(LBL("Import Texture Error"), NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(customPopUpMessage.c_str());

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() -140);
        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatTexturePane::DrawTexture()
{

        auto nTexture = curInstance->parts->gfxMeta.size();
        if(ImGui::Button(LBL("Add Texture"))){
            auto item = &curInstance->parts->gfxMeta.emplace_back();
            item->id = curInstance->parts->gfxMeta.size() - 1;
            RegenerateTexturesNames();
            curInstance->currState->partGraph = item->id;
        }
        ImGui::SameLine();
        if(nTexture > 0 && ImGui::Button(LBL("Delete Texture"))){
            openpopupWithId = LBL("Texture Delete");
        }
        if(nTexture > 0) {
            if (ImGui::BeginCombo(LBL("Texture"), textureDecoratedNames[curInstance->currState->partGraph].c_str(),
                                  ImGuiComboFlags_HeightLargest)) {
                auto count = curInstance->parts->gfxMeta.size();
                for (int n = 0; n < count; n++) {
                    const bool is_selected = (curInstance->currState->partGraph == n);
                    if (ImGui::Selectable(textureDecoratedNames[n].c_str(), is_selected)) {
                        curInstance->currState->partGraph = n;
                        RegenerateTexturesNames();
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            auto gfx = &curInstance->parts->gfxMeta[curInstance->currState->partGraph];
            if (gfx) {
                if (ImGui::InputText(LBL("Texture name"), &gfx->name)) {
                    textureDecoratedNames[curInstance->currState->partGraph] = curInstance->parts->GetTexturesDecorateName(curInstance->currState->partGraph);
                }
                if(ImGui::Button(*curInstance->renderMode == RenderMode::TEXTURE_VIEW ?
                    LBL("Hide Texture Render") : LBL("Show Texture Render")))
                {
                    
                    *curInstance->renderMode = *curInstance->renderMode == RenderMode::TEXTURE_VIEW ?
                        RenderMode::DEFAULT : RenderMode::TEXTURE_VIEW;
                    

                    auto& frame = curInstance->currState->animationSequence.frames.at(curInstance->currState->frame);
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
                ImGui::Checkbox(LBL("No compression on pat file"), &gfx->noCompress);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                    Tooltip("Use this only if you see that on compression texture has bigger data size than uncompressed.");
                if (ImGui::Button(LBL("Import Texture"))) {
                    std::string &&file = FileDialog(fileType::DDS);
                    if (!file.empty()) {
                        // MBAACC .pat files get PGTX (32-bit BGRA), the only texture record MBAA.exe reads.
                        std::string message = curInstance->parts->gfxMeta[curInstance->currState->partGraph].ImportTexture(file.c_str(), curInstance->parts->textures, curInstance->parts->useMBAACCFormat);
                        if (message.empty()) {
                            textureDecoratedNames[curInstance->currState->partGraph] = curInstance->parts->GetTexturesDecorateName(
                                    curInstance->currState->partGraph);
                            for(auto &cutOut : curInstance->parts->cutOuts)
                            {
                                //Update PPTE of parts that use current texture
                                if(cutOut.texture == curInstance->currState->partGraph)
                                {
                                    cutOut.ppte[0] = gfx->pgte[0];
                                    cutOut.ppte[1] = gfx->pgte[1];
                                }
                            }
                        }
                        else
                        {
                            openpopupWithId = LBL("Import Texture Error");
                            customPopUpMessage = message;
                        }
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", curInstance->parts->useMBAACCFormat
                        ? TXT("MBAACC .pat: the DDS (DXT1, DXT5 or 32-bit) is stored as 32-bit BGRA (PGTX), the format MBAA.exe reads.")
                        : TXT("UNI / MBTL .pat: width and height must be multiples of 256.\nThe DDS must be DXT1, DXT5 or uncompressed 32-bit."));
                ImGui::SameLine(0, 20.f);
                if (ImGui::Button(LBL("Export Texture"))) {
                    std::string filename(gfx->name);
                    std::string &&file = FileDialog(fileType::DDS, true, const_cast<char *>(filename.c_str()));
                    if (!file.empty()) {
                        std::string err = gfx->ExportTexture(file.c_str());
                        if (!err.empty()) {
                            openpopupWithId = LBL("Import Texture Error");
                            customPopUpMessage = err;
                        }
                    }
                }
                ImGui::NewLine();
                ImGui::SameLine(i18n::RightPairX(LBL("Copy Texture"), LBL("Paste Texture")));
                if(ImGui::Button(LBL("Copy Texture"))){
                    gfx->CopyTo(&CopyManager::copiedParts->gfx);
                }
                ImGui::SameLine();
                if(ImGui::Button(LBL("Paste Texture"))){
                    CopyManager::copiedParts->gfx.CopyTo(gfx);
                    RegenerateTexturesNames();
                }

                ImGui::Text("");
                ImGui::Separator();
                ImGui::Text(TXT("*Information*"));
                ImGui::Text(" ");
                ImGui::Text(TXT("Width: %d"), gfx->w);
                ImGui::Text(TXT("Height %d"), gfx->h);
                ImGui::Text(TXT("Type %d"), gfx->type);
                ImGui::Text(TXT("BPP %d"), gfx->bpp);
                ImGui::Text(TXT("UV BPP Width %d"), gfx->uvBpp[0]);
                ImGui::Text(TXT("UV BPP Height %d"), gfx->uvBpp[1]);
                ImGui::Text(TXT("PGTE Width %d"), gfx->pgte[0]);
                ImGui::Text(TXT("PGTE Height %d"), gfx->pgte[1]);
            }
        }
}
