#include "pat_shape_pane.h"
#include "pat_search.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"

PatShapePane::PatShapePane(Render* render, StateReference *curInstance) : DrawWindow(render, curInstance),
                                                                    shapesDecoratedNames(nullptr)
{
}

void PatShapePane::RegenerateShapesNames()
{
    delete[] shapesDecoratedNames;

    shapesNamesCount = -1;
    if(curInstance->parts && curInstance->parts->loaded)
    {
        int count = static_cast<int>(curInstance->parts->shapes.size());
        shapesDecoratedNames = new std::string[count > 0 ? count : 1];

        for(int i = 0; i < count; i++)
        {
            shapesDecoratedNames[i] = curInstance->parts->GetShapesDecorateName(i);
        }
        shapesNamesCount = count;
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
        "Rect",
        "Ring",
        "Arc",
        "Sphere",
        "Cone"
};

void PatShapePane::Draw()
{
    if(isVisible) {
        ImGui::Begin(LBL("Shape Pane"), 0);
        if(curInstance->parts->loaded) {
            if(shapesNamesCount != (int)curInstance->parts->shapes.size() || !shapesDecoratedNames)
                RegenerateShapesNames();
            int& sel = curInstance->currState->partShape;
            const int n = (int)curInstance->parts->shapes.size();
            if(sel < 0 || sel >= n) sel = 0;
            DrawShape();
        }
        ImGui::End();   // one End per Begin

        if (!openpopupWithId.empty()) {
            ImGui::OpenPopup(openpopupWithId.c_str());
            openpopupWithId = "";
        }
        DrawPopUp();
    }
}

void PatShapePane::DrawPopUp() {

    if (ImGui::BeginPopupModal(LBL("Shape Delete")))
    {
        ImGui::Text(TXT("Are you sure you want to delete this Part?"));

        ImGui::Separator();

        if (ImGui::Button(LBL("OK"), ImVec2(120, 40))) {
            if (auto* sh = curInstance->parts->GetShape(curInstance->currState->partShape)) {
                const int id = sh->id;
                *sh = {};
                sh->id = id;
            }
            RegenerateShapesNames();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"), ImVec2(120, 40))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PatShapePane::DrawShape()
{

        auto nShape = curInstance->parts->shapes.size();
        if(ImGui::Button(LBL("Add Shape"))){
            auto item = &curInstance->parts->shapes.emplace_back();
            item->id = curInstance->parts->shapes.size() - 1;
            RegenerateShapesNames();
            curInstance->currState->partShape = item->id;
        }
        ImGui::SameLine();
        if(nShape > 0 && ImGui::Button(LBL("Delete Shape"))){
            openpopupWithId = LBL("Shape Delete");
        }
        if(nShape > 0) {
            if (ImGui::BeginCombo(LBL("Shape"), shapesDecoratedNames[curInstance->currState->partShape].c_str(),
                                  ImGuiComboFlags_HeightLargest)) {

                patsearch::FilterBox();
                auto count = curInstance->parts->shapes.size();
                for (int n = 0; n < count; n++) {
                    if (!patsearch::Matches(shapesDecoratedNames[n].c_str())) continue;
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
                if (ImGui::InputText(LBL("Shape name"), &shape->name)) {
                    shapesDecoratedNames[curInstance->currState->partShape] = curInstance->parts->GetShapesDecorateName(curInstance->currState->partShape);
                }
                ImGui::NewLine();
                ImGui::SameLine(i18n::RightPairX(LBL("Copy Shape"), LBL("Paste Shape")));
                if(ImGui::Button(LBL("Copy Shape"))){
                    shape->CopyTo(&CopyManager::copiedParts->shape);
                }
                ImGui::SameLine();
                if(ImGui::Button(LBL("Paste Shape"))){
                    CopyManager::copiedParts->shape.CopyTo(shape);
                    RegenerateShapesNames();
                }
                ImGui::Separator();

                const int typeIndex = (int) shape->type - 1;
                if (ImGui::BeginCombo(LBL("ShapeType"), typeIndex >= 0 && typeIndex < 6 ? shapeList[typeIndex] : "?",
                                      ImGuiComboFlags_HeightLargest)) {

                    for (int n = 0; n < 6; n++) {
                        const bool is_selected = (typeIndex == n);
                        if (ImGui::Selectable(shapeList[n], is_selected)) {
                            shape->type = (ShapeType) (n + 1);
                        }

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                if (shape->type == ShapeType::PLANE) {
                    // no parameters
                } else if (shape->type == ShapeType::RECT) {
                    // CPAniPrimRender_Rect: corners (x, y) .. (x + w, y + h); w/h lerp under AFHK.
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Rect X"), &shape->rectX);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Rect Y"), &shape->rectY);
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Rect W"), &shape->rectW);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Rect H"), &shape->rectH);
                } else {
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Radius"), &shape->radius, 1, 0, INT32_MAX);
                    if (shape->type == ShapeType::RING || shape->type == ShapeType::ARC) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt(LBL("Width"), &shape->width, 1, 0, INT32_MAX);
                    }
                    if (shape->type != ShapeType::SPHERE) {
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt(LBL("Dz"), &shape->dz, 1, 0, INT32_MAX);
                    }
                    if (shape->type == ShapeType::RING || shape->type == ShapeType::ARC) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt(LBL("DRadius"), &shape->dRadius, 1, 0, INT32_MAX);
                    }
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Vertex"), &shape->vertexCount, 1, 0, INT32_MAX);
                    if (shape->type == ShapeType::SPHERE || shape->type == ShapeType::CONE) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt(LBL("Vertex 2"), &shape->vertexCount2, 1, 0, INT32_MAX);
                    }
                    ImGui::SetNextItemWidth(width);
                    ImGui::DragInt(LBL("Length"), &shape->length, 10, 0, INT32_MAX);
                    if (shape->type == ShapeType::SPHERE) {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(width);
                        ImGui::DragInt(LBL("Length 2"), &shape->length2, 10, 0, INT32_MAX);
                    }
                }
            }
        }
}
