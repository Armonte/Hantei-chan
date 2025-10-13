#include "pat_tool_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"

PatToolPane::PatToolPane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    partSetDecoratedNames(nullptr)
{

}

void PatToolPane::RegeneratePartsetNames()
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

constexpr float width = 75.f;
constexpr float widthInput = 150.f;

void PatToolPane::Draw()
{
    if(isVisible) {
        ImGui::Begin("Tool Pane", 0);
        if(curInstance->parts->loaded) {
            DrawTool();
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

void PatToolPane::DrawPopUp() {

    if (ImGui::BeginPopupModal("Frame Delete"))
    {
        ImGui::Text("Are you sure you want to delete this frame?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 40))) {

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatToolPane::DrawTool()
{
    if (!curInstance->currState->animationSequence.frames.empty())
    {
        int nframes = curInstance->currState->animationSequence.frames.size() - 1;
        if (nframes >= 0) {
            const char* const interpolationList[] = {
                "None",
                "Linear"
            };

            ImGui::Text("PartSet Test Animation");
            if(!curInstance->currState->animating)
            {
                float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 160.f);
                ImGui::SliderInt("##frameSlider", &curInstance->currState->framePatEditor, 0, nframes);
                ImGui::SameLine();
                ImGui::PushButtonRepeat(true);
                if (ImGui::ArrowButton("##left", ImGuiDir_Left))
                {
                    curInstance->currState->framePatEditor--;
                }
                ImGui::SameLine(0.0f, spacing);
                if (ImGui::ArrowButton("##right", ImGuiDir_Right))
                {
                    curInstance->currState->framePatEditor++;
                }
                auto frame = &curInstance->currState->animationSequence.frames.at(curInstance->currState->framePatEditor);
                ImGui::PopButtonRepeat();
                ImGui::SameLine();
                ImGui::Text("%d/%d", curInstance->currState->framePatEditor + 1, nframes + 1);
                if (curInstance->currState->framePatEditor < 0)
                    curInstance->currState->framePatEditor = 0;
                else if (curInstance->currState->framePatEditor > nframes)
                    curInstance->currState->framePatEditor = nframes;

                // Ensure frame has at least one layer
                if (frame->AF.layers.empty()) {
                    frame->AF.layers.push_back({});
                }

                auto nPartSet = curInstance->parts->partSets.size();
                if(nPartSet > 0)
                {
                    // Clamp spriteId to valid range to prevent out-of-bounds access
                    if (frame->AF.layers[0].spriteId < 0 || frame->AF.layers[0].spriteId >= nPartSet) {
                        frame->AF.layers[0].spriteId = 0;
                    }

                    if (ImGui::BeginCombo("Part Set", partSetDecoratedNames[frame->AF.layers[0].spriteId].c_str(),
                                          ImGuiComboFlags_HeightLargest))
                    {
                        auto count = curInstance->parts->partSets.size();
                        for (int n = 0; n < count; n++) {
                            const bool is_selected = (frame->AF.layers[0].spriteId == n);
                            if (ImGui::Selectable(partSetDecoratedNames[n].c_str(), is_selected)) {
                                frame->AF.layers[0].spriteId = n;
                            }

                            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }
                    auto af = &frame->AF;
                    ImGui::InputInt("Duration", &af->duration, 1, 0);
                    ImGui::Combo("Interpolation", &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList));
                }
            }

            auto text = curInstance->currState->animating ? "Stop Animation" : "Play Animation";
            if (ImGui::Button(text)) {
                curInstance->currState->animating = !curInstance->currState->animating;
                curInstance->currState->animeSeq = curInstance->currState->pattern;
                if(curInstance->currState->animating)
                {
                    curInstance->currState->frame = curInstance->currState->framePatEditor;
                    curInstance->currState->previewSequence = *curInstance->framedata->get_sequence(0);
                    curInstance->framedata->m_sequences[0] = curInstance->currState->animationSequence;
                }
                else
                {
                    curInstance->currState->frame = 0;
                    curInstance->framedata->m_sequences[0] = curInstance->currState->previewSequence;
                    auto& restoreFrame = curInstance->framedata->m_sequences[0].frames.at(0);
                    if (restoreFrame.AF.layers.empty()) {
                        restoreFrame.AF.layers.push_back({});
                    }
                    restoreFrame.AF.layers[0].spriteId = curInstance->currState->partSet;
                }
            }

            ImGui::Separator();

            if(!curInstance->currState->animating)
            {
                if(ImGui::Button("Add Frame"))
                {
                    auto frame = &curInstance->currState->animationSequence.frames.emplace_back();
                    // Ensure new frame has at least one layer
                    if (frame->AF.layers.empty()) {
                        frame->AF.layers.push_back({});
                    }
                    frame->AF.layers[0].usePat = true;
                    frame->AF.layers[0].spriteId = 0;
                    frame->AF.aniType = 1;
                    curInstance->currState->framePatEditor = curInstance->currState->animationSequence.frames.size() - 1;
                    nframes = curInstance->currState->animationSequence.frames.size() - 1;
                }
                ImGui::SameLine();
                if(ImGui::Button("Duplicate Frame"))
                {
                    auto copyFrame = curInstance->currState->animationSequence.frames[curInstance->currState->framePatEditor];
                    curInstance->currState->animationSequence.frames.push_back(copyFrame);
                    curInstance->currState->framePatEditor = curInstance->currState->animationSequence.frames.size() - 1;
                    nframes = curInstance->currState->animationSequence.frames.size() - 1;
                    auto frame = &curInstance->currState->animationSequence.frames.back();
                }
                if(nframes > 0)
                {
                    ImGui::SameLine();
                    if(ImGui::Button("Delete Frame"))
                    {
                        auto frames = &curInstance->currState->animationSequence.frames;
                        frames->erase(
                            frames->begin() + curInstance->currState->framePatEditor);
                        int curFrame = curInstance->currState->framePatEditor;
                        nframes = frames->size() - 1;
                        if(curFrame > nframes)
                            curInstance->currState->framePatEditor--;
                        auto frame = &curInstance->currState->animationSequence.frames.at(curInstance->currState->framePatEditor);
                    }
                }
            }
        }
    }
}
