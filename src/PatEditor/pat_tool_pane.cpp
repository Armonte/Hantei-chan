#include "pat_tool_pane.h"
#include "pat_search.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"

PatToolPane::PatToolPane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    partSetDecoratedNames(nullptr)
{

}

void PatToolPane::RegeneratePartsetNames()
{
    delete[] partSetDecoratedNames;

    partSetNamesCount = -1;
    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->partSets.size());
        partSetDecoratedNames = new std::string[count > 0 ? count : 1];

        for(int i = 0; i < count; i++)
        {
            partSetDecoratedNames[i] = curInstance->parts->GetPartSetDecorateName(i);
        }
        partSetNamesCount = count;
    }
    else
        partSetDecoratedNames = nullptr;
}

constexpr float width = 75.f;
constexpr float widthInput = 150.f;

void PatToolPane::Draw()
{
    if(isVisible) {
        ImGui::Begin(LBL("Tool Pane"), 0);
        if(curInstance->parts->loaded) {
            if(partSetNamesCount != (int)curInstance->parts->partSets.size() || !partSetDecoratedNames)
                RegeneratePartsetNames();
            DrawTool();
        }
        ImGui::End();   // one End per Begin

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatToolPane::DrawPopUp() {

    if (ImGui::BeginPopupModal(LBL("Frame Delete")))
    {
        ImGui::Text(TXT("Are you sure you want to delete this frame?"));

        ImGui::Separator();

        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatToolPane::DrawTool()
{
    // RBO / GOF2 character: its pattern list is real game data (this test animation swaps pattern 0 in and out).
    if (curInstance->framedata->isHan2()) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", TXT("RBO / GOF2: play the character's real patterns in Tools > Animation player."));
        ImGui::PopTextWrapPos();
        return;
    }
    if (!curInstance->currState->animationSequence.frames.empty())
    {
        int nframes = curInstance->currState->animationSequence.frames.size() - 1;
        if (nframes >= 0) {
            const char* const interpolationList[] = {
                "None",
                "Linear"
            };

            ImGui::Text(TXT("PartSet Test Animation"));
            if(!curInstance->currState->animating)
            {
                float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                auto clampFrame = [&] {
                    int& f = curInstance->currState->framePatEditor;
                    f = f < 0 ? 0 : f > nframes ? nframes : f;
                };
                clampFrame();
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
                ImGui::PopButtonRepeat();
                // Clamp BEFORE indexing: at either end the arrow moved the index out of the list and .at() threw
                // (sosfiro TODO "animator arrow buttons crash").
                clampFrame();
                auto frame = &curInstance->currState->animationSequence.frames.at(curInstance->currState->framePatEditor);
                ImGui::SameLine();
                ImGui::Text("%d/%d", curInstance->currState->framePatEditor + 1, nframes + 1);

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

                    // The preview shows the live name (a rename in the PartSet pane updates it at once: sosfiro TODO
                    // "animator part set combo not refreshed on rename"); the list is rebuilt when it opens.
                    const std::string current = curInstance->parts->GetPartSetDecorateName(frame->AF.layers[0].spriteId);
                    if (ImGui::BeginCombo(LBL("Part Set"), current.c_str(), ImGuiComboFlags_HeightLargest))
                    {
                        if (ImGui::IsWindowAppearing())
                            RegeneratePartsetNames();
                        patsearch::FilterBox();
                        auto count = curInstance->parts->partSets.size();
                        for (int n = 0; n < count; n++) {
                            if (!patsearch::Matches(partSetDecoratedNames[n].c_str())) continue;
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
                    ImGui::InputInt(LBL("Duration"), &af->duration, 1, 0);
                    i18n::Combo(LBL("Interpolation"), &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList));
                }
            }

            auto text = curInstance->currState->animating ? LBL("Stop Animation") : LBL("Play Animation");
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
                if(ImGui::Button(LBL("Add Frame")))
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
                if(ImGui::Button(LBL("Duplicate Frame")))
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
                    if(ImGui::Button(LBL("Delete Frame")))
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
