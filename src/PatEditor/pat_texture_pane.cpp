#include "pat_texture_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"

PatTexturePane::PatTexturePane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    textureDecoratedNames(nullptr)
{

}

void PatTexturePane::RegenerateTexturesNames()
{
    delete[] textureDecoratedNames;

    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->gfxMeta.size());
        textureDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            textureDecoratedNames[i] = curInstance->parts->GetTexturesDecorateName(i);
        }
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
        ImGui::Begin("Texture Pane", 0);
        auto seq = curInstance->framedata->get_sequence(curInstance->currState->pattern);
        auto pat = curInstance->parts;
        if(curInstance->parts->loaded) {
            DrawTexture();
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

void PatTexturePane::DrawPopUp() {
    if (ImGui::BeginPopupModal("Texture Delete"))
    {
        ImGui::Text("Are you sure you want to delete this Part?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 40))) {
            curInstance->parts->gfxMeta[curInstance->currState->partGraph] = {};
            RegenerateTexturesNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Import Texture Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(customPopUpMessage.c_str());

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() -140);
        if (ImGui::Button("OK", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatTexturePane::DrawTexture()
{

        auto nTexture = curInstance->parts->gfxMeta.size();
        if(ImGui::Button("Add Texture")){
            auto item = &curInstance->parts->gfxMeta.emplace_back();
            item->id = curInstance->parts->gfxMeta.size() - 1;
            RegenerateTexturesNames();
            curInstance->currState->partGraph = item->id;
        }
        ImGui::SameLine();
        if(nTexture > 0 && ImGui::Button("Delete Texture")){
            openpopupWithId = "Texture Delete";
        }
        if(nTexture > 0) {
            if (ImGui::BeginCombo("Texture", textureDecoratedNames[curInstance->currState->partGraph].c_str(),
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
                if (ImGui::InputText("Texture name", &gfx->name)) {
                    textureDecoratedNames[curInstance->currState->partGraph] = curInstance->parts->GetTexturesDecorateName(curInstance->currState->partGraph);
                }
                if(ImGui::Button(*curInstance->renderMode == RenderMode::TEXTURE_VIEW ?
                    "Hide Texture Render" : "Show Texture Render"))
                {
                    printf("[TEXTURE_VIEW BUTTON] Clicked! Current mode: %d\n", (int)*curInstance->renderMode);
                    
                    *curInstance->renderMode = *curInstance->renderMode == RenderMode::TEXTURE_VIEW ?
                        RenderMode::DEFAULT : RenderMode::TEXTURE_VIEW;
                    
                    printf("[TEXTURE_VIEW BUTTON] New mode: %d\n", (int)*curInstance->renderMode);
                    printf("[TEXTURE_VIEW BUTTON] Parts renderMode pointer: %p\n", (void*)curInstance->parts->renderMode);
                    printf("[TEXTURE_VIEW BUTTON] Parts currState pointer: %p\n", (void*)curInstance->parts->currState);

                    auto& frame = curInstance->currState->animationSequence.frames.at(curInstance->currState->frame);
                    // Ensure frame has at least one layer
                    if (frame.AF.layers.empty()) {
                        frame.AF.layers.push_back({});
                    }

                    if(*curInstance->renderMode != RenderMode::DEFAULT)
                    {
                        frame.AF.layers[0].spriteId = 0;
                        printf("[TEXTURE_VIEW BUTTON] Set spriteId to 0 for texture view\n");
                    }
                    else
                    {
                        frame.AF.layers[0].spriteId = curInstance->currState->partSet;
                        printf("[TEXTURE_VIEW BUTTON] Set spriteId to %d for default view\n", curInstance->currState->partSet);
                    }
                }
                ImGui::Checkbox("No compression on pat file", &gfx->noCompress);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                    Tooltip("Use this only if you see that on compression texture has bigger data size than uncompressed.");
                if (ImGui::Button("Import Texture")) {
                    std::string &&file = FileDialog(fileType::DDS);
                    if (!file.empty()) {
                        std::string message = curInstance->parts->gfxMeta[curInstance->currState->partGraph].ImportTexture(file.c_str(), curInstance->parts->textures);
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
                            openpopupWithId = "Import Texture Error";
                            customPopUpMessage = message;
                        }
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                    Tooltip("Textures on import need to be divisible of 128\non width and height.\nAlso DDS need to be: DXT1, DXT5 or Uncompressed.");
                ImGui::SameLine(0, 20.f);
                if (ImGui::Button("Export Texture")) {
                    std::string filename(gfx->name);
                    std::string &&file = FileDialog(fileType::DDS, true, const_cast<char *>(filename.c_str()));
                    if (!file.empty()) {
                        gfx->ExportTexture(file.c_str());
                    }
                }
                ImGui::NewLine();
                ImGui::SameLine(ImGui::GetWindowWidth()-220);
                if(ImGui::Button("Copy Texture")){
                    gfx->CopyTo(&CopyManager::copiedParts->gfx);
                }
                ImGui::SameLine();
                if(ImGui::Button("Paste Texture")){
                    CopyManager::copiedParts->gfx.CopyTo(gfx);
                    RegenerateTexturesNames();
                }

                ImGui::Text("");
                ImGui::Separator();
                ImGui::Text("*Information*");
                ImGui::Text(" ");
                ImGui::Text("Width: %d", gfx->w);
                ImGui::Text("Height %d", gfx->h);
                ImGui::Text("Type %d", gfx->type);
                ImGui::Text("BPP %d", gfx->bpp);
                ImGui::Text("UV BPP Width %d", gfx->uvBpp[0]);
                ImGui::Text("UV BPP Height %d", gfx->uvBpp[1]);
                ImGui::Text("PGTE Width %d", gfx->pgte[0]);
                ImGui::Text("PGTE Height %d", gfx->pgte[1]);
            }
        }
}
