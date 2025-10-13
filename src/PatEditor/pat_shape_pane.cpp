#include "pat_shape_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"

PatShapePane::PatShapePane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    shapesDecoratedNames(nullptr)
{
}

void PatShapePane::RegenerateShapesNames()
{
    delete[] shapesDecoratedNames;

    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->shapes.size());
        shapesDecoratedNames = new std::string[count];

        for(int i = 0; i < count; i++)
        {
            shapesDecoratedNames[i] = curInstance->parts->GetShapesDecorateName(i);
        }
    }
    else
        shapesDecoratedNames = nullptr;
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

void PatShapePane::Draw()
{
    if(isVisible) {
        ImGui::Begin("Shape Pane", 0);
        auto seq = curInstance->framedata->get_sequence(curInstance->currState->pattern);
        auto pat = curInstance->parts;
        if(curInstance->parts->loaded) {
            DrawShape();
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

void PatShapePane::DrawPopUp() {

    if (ImGui::BeginPopupModal("Shape Delete"))
    {
        ImGui::Text("Are you sure you want to delete this Part?");

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 40))) {
            curInstance->parts->shapes[curInstance->currState->partShape] = {};
            RegenerateShapesNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatShapePane::DrawShape()
{

        auto nShape = curInstance->parts->shapes.size();
        if(ImGui::Button("Add Shape")){
            auto item = &curInstance->parts->shapes.emplace_back();
            item->id = curInstance->parts->shapes.size() - 1;
            RegenerateShapesNames();
            curInstance->currState->partShape = item->id;
        }
        ImGui::SameLine();
        if(nShape > 0 && ImGui::Button("Delete Shape")){
            openpopupWithId = "Shape Delete";
        }
        if(nShape > 0) {
            if (ImGui::BeginCombo("Shape", shapesDecoratedNames[curInstance->currState->partShape].c_str(),
                                  ImGuiComboFlags_HeightLargest)) {

                auto count = curInstance->parts->shapes.size();
                for (int n = 0; n < count; n++) {
                    const bool is_selected = (curInstance->currState->partShape == n);
                    if (ImGui::Selectable(shapesDecoratedNames[n].c_str(), is_selected)) {
                        curInstance->currState->partShape = n;
                        RegenerateShapesNames();
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            auto shape = &curInstance->parts->shapes[curInstance->currState->partShape];
            if (shape) {
                if (ImGui::InputText("Shape name", &shape->name)) {
                    shapesDecoratedNames[curInstance->currState->partShape] = curInstance->parts->GetShapesDecorateName(curInstance->currState->partShape);
                }
                ImGui::NewLine();
                ImGui::SameLine(ImGui::GetWindowWidth()-200);
                if(ImGui::Button("Copy Shape")){
                    shape->CopyTo(&CopyManager::copiedParts->shape);
                }
                ImGui::SameLine();
                if(ImGui::Button("Paste Shape")){
                    CopyManager::copiedParts->shape.CopyTo(shape);
                    RegenerateShapesNames();
                }
                ImGui::Separator();

                if (ImGui::BeginCombo("ShapeType", shapeList[(int) shape->type - 1],
                                      ImGuiComboFlags_HeightLargest)) {

                    for (int n = 0; n < 6; n++) {
                        const bool is_selected = (curInstance->currState->partShape == n);
                        if (ImGui::Selectable(shapeList[n], is_selected)) {
                            shape->type = (ShapeType) (n + 1);
                        }

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                if (shape->type == ShapeType::PLANE || shape->type == ShapeType::UNK2) {
                    // no parameters
                } else {
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt("Radius", &shape->radius, 1, 0, INT32_MAX);
                    if (shape->type == ShapeType::RING || shape->type == ShapeType::ARC) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt("Width", &shape->width, 1, 0, INT32_MAX);
                    }
                    if (shape->type != ShapeType::SPHERE) {
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt("Dz", &shape->dz, 1, 0, INT32_MAX);
                    }
                    if (shape->type == ShapeType::RING || shape->type == ShapeType::ARC) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt("DRadius", &shape->dRadius, 1, 0, INT32_MAX);
                    }
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt("Vertex", &shape->vertexCount, 1, 0, INT32_MAX);
                    if (shape->type == ShapeType::SPHERE || shape->type == ShapeType::CONE) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt("Vertex 2", &shape->vertexCount2, 1, 0, INT32_MAX);
                    }
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt("Length", &shape->length, 10, 0, INT32_MAX);
                    if (shape->type == ShapeType::SPHERE) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt("Length 2", &shape->length2, 10, 0, INT32_MAX);
                    }
                }
            }
        }
}
