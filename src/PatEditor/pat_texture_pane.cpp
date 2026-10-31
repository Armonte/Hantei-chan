#include "pat_texture_pane.h"
#include "pat_search.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../main_frame.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"
#include "../han2_parts_edit.h"
#include "../han2_pat.h"
#include "../png_writer.h"
#include "../cgm/cgm_texsrc.h"
#include "../parts/pat_texconv.h"
#include <filesystem>
#include <cstdarg>

static std::string Fmt(const char *fmt, ...)
{
    char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); return b;
}

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

std::string PatTexturePane::ImportSheetFile(const std::string &path, bool replaceSelected)
{
    auto *parts = curInstance->parts;
    std::vector<uint8_t> px; int w = 0, h = 0; std::string e;
    if (!ReadImageRgba(path, px, w, h, e)) return e;
    int slot = replaceSelected ? curInstance->currState->partGraph : (int)parts->gfxMeta.size();
    if (!replaceSelected) {   // reuse an empty slot first, so sheet ids stay low
        for (size_t i = 0; i < parts->gfxMeta.size(); i++) if (!parts->gfxMeta[i].data) { slot = (int)i; break; }
    }
    if (!han2::edit::ImportSheetRgba(*parts, slot, px.data(), w, h, &e)) return e;
    han2::UploadPartsTextures(*parts);
    curInstance->markEdited();   // also reached from a file drop / ui-script, where no mouse gesture ends the undo step
    curInstance->currState->partGraph = slot;
    RegenerateTexturesNames();
    const int users = han2::edit::SheetUsers(*parts, slot);
    std::string msg = Fmt(TXT("sheet %d: %s (%d x %d)"), slot, std::filesystem::u8path(path).filename().u8string().c_str(), w, h);
    if (replaceSelected && users) msg += Fmt(TXT(" - %d parts draw from this sheet"), users);
    return msg;
}

void PatTexturePane::DrawFbSheets()
{
    auto *parts = curInstance->parts;
    int &sel = curInstance->currState->partGraph;
    const int n = (int)parts->gfxMeta.size();
    if (sel < 0 || sel >= n) sel = n > 0 ? 0 : -1;
    if (!textureDecoratedNames && n > 0) RegenerateTexturesNames();

    ImGui::BeginDisabled(n >= han2::edit::kMaxSheets);
    if (ImGui::Button(LBL("Add sheet from image..."))) {
        std::string f = FileDialog(fileType::IMAGE);
        if (!f.empty()) fbMessage = ImportSheetFile(AnsiToUtf8(f), false);
    }
    ImGui::EndDisabled();
    if (n > 0 && sel >= 0) {
        ImGui::SameLine();
        if (ImGui::Button(LBL("Replace with image..."))) {
            std::string f = FileDialog(fileType::IMAGE);
            if (!f.empty()) fbMessage = ImportSheetFile(AnsiToUtf8(f), true);
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Remove sheet"))) {
            std::vector<int> poses;
            const int users = han2::edit::SheetUsers(*parts, sel, &poses);
            if (users) { fbConfirmRemove = true; fbMessage = Fmt(TXT("Sheet %d is used by %d parts in %d poses. Remove anyway?"), sel, users, (int)poses.size()); }
            else { std::string e; if (han2::edit::RemoveSheet(*parts, sel, false, &e)) { han2::UploadPartsTextures(*parts); RegenerateTexturesNames(); fbMessage = Fmt(TXT("sheet %d removed"), sel); curInstance->markEdited(); } else fbMessage = e; }
        }
        if (fbConfirmRemove) {
            ImGui::TextColored(ImVec4(1, .6f, .3f, 1), "%s", fbMessage.c_str());
            if (ImGui::Button(LBL("Remove (parts keep pointing at the empty sheet)"))) {
                std::string e; han2::edit::RemoveSheet(*parts, sel, true, &e);
                han2::UploadPartsTextures(*parts); RegenerateTexturesNames();
                fbConfirmRemove = false; fbMessage = Fmt(TXT("sheet %d removed"), sel); curInstance->markEdited();
            }
            ImGui::SameLine();
            if (ImGui::Button(LBL("Cancel"))) { fbConfirmRemove = false; fbMessage.clear(); }
        }
    }
    if (n == 0) { ImGui::TextDisabled("%s", TXT("This PAT has no sprite sheets.")); if (!fbMessage.empty()) ImGui::TextWrapped("%s", fbMessage.c_str()); return; }

    if (ImGui::BeginCombo(LBL("Sprite sheet"), textureDecoratedNames ? textureDecoratedNames[sel].c_str() : "", ImGuiComboFlags_HeightLargest)) {
        patsearch::FilterBox();
        for (int i = 0; i < n; i++) {
            const auto &g = parts->gfxMeta[i];
            std::string label = std::to_string(i) + ": " + (g.data ? g.name + "  " + std::to_string(g.w) + "x" + std::to_string(g.h) : std::string(TXT("(empty)")));
            if (!patsearch::Matches(label.c_str())) continue;
            if (ImGui::Selectable(label.c_str(), sel == i)) sel = i;
            if (sel == i) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    auto &g = parts->gfxMeta[sel];
    if (ImGui::Button(*curInstance->renderMode == RenderMode::TEXTURE_VIEW ? LBL("Hide Texture Render") : LBL("Show Texture Render")))
        *curInstance->renderMode = *curInstance->renderMode == RenderMode::TEXTURE_VIEW ? RenderMode::DEFAULT : RenderMode::TEXTURE_VIEW;
    ImGui::SameLine();
    ImGui::BeginDisabled(!g.data);
    if (ImGui::Button(LBL("Export image..."))) {
        // icaroffa 2026: export from the sheet in memory, .png by default, also .bmp / .jpg / .tif; failures are reported
        std::vector<uint8_t> px; int w = 0, h = 0;
        if (!han2::edit::SheetRgba(*parts, sel, px, w, h)) fbMessage = TXT("This sheet has no image to export.");
        else {
            char def[260]; snprintf(def, sizeof def, "%s.png", g.name.empty() ? ("sheet" + std::to_string(sel)).c_str() : g.name.c_str());
            std::string f = FileDialog(fileType::IMAGESAVE, true, def);
            if (!f.empty()) {
                std::string e;
                f = AnsiToUtf8(f);
                fbMessage = WriteImageRgba(f, px.data(), w, h, e) ? Fmt(TXT("exported %s"), f.c_str()) : e;
            }
        }
    }
    ImGui::EndDisabled();
    if (g.data) {
        std::vector<int> poses;
        const int users = han2::edit::SheetUsers(*parts, sel, &poses);
        ImGui::Text(TXT("%d x %d, 32-bit; used by %d parts in %d poses"), g.w, g.h, users, (int)poses.size());
    } else ImGui::TextDisabled("%s", TXT("(empty slot)"));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", TXT("Sheets must be square (width = height). Drop a PNG on the editor to add it as a sheet. Save writes the sheets into the .DAT (GOF2: the .PAT)."));
    if (!fbMessage.empty() && !fbConfirmRemove) ImGui::TextWrapped("%s", fbMessage.c_str());
    ImGui::PopTextWrapPos();
}

void PatTexturePane::DrawTexture()
{
        if (curInstance->parts->fbPartModel) { DrawFbSheets(); return; }

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
                patsearch::FilterBox();
                auto count = curInstance->parts->gfxMeta.size();
                for (int n = 0; n < count; n++) {
                    if (!patsearch::Matches(textureDecoratedNames[n].c_str())) continue;
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
                    if (*curInstance->renderMode == RenderMode::TEXTURE_VIEW) {
                        curInstance->currState->partGraph = curInstance->parts->ShownTextureSlot(curInstance->currState->partGraph);
                        RegenerateTexturesNames();
                    }

                    auto& anim = curInstance->currState->animationSequence.frames;   // the tool pane's test animation (empty for RBO / GOF2)
                    if (curInstance->currState->frame >= 0 && curInstance->currState->frame < (int)anim.size()) {
                    auto& frame = anim[curInstance->currState->frame];
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
                // PNG in / out through the CG manager's shared texture layer (cgm/cgm_texsrc.h): the texture keeps its kind
                // (MBAACC PGTX 32-bit; UNI / MBTL DXT1, DXT5, A8R8G8B8 or X8R8G8B8), a PNG that renders the same changes no byte.
                if (ImGui::Button(LBL("Import PNG"))) {
                    std::string &&file = FileDialog(fileType::IMAGE);
                    if (!file.empty()) {
                        cgm::PatTexSource src(*curInstance->parts); std::string err; bool changed = false;
                        const int t = curInstance->currState->partGraph;
                        if (!cgm::ImportPng(src, t, file, &err, &changed)) { openpopupWithId = LBL("Import Texture Error"); customPopUpMessage = err; }
                        else if (changed) {
                            std::vector<uint8_t> rgba; int w = 0, h = 0; std::string e2;
                            if (pattex::Decode(*gfx, rgba, w, h, &e2)) {
                                for (size_t k = 0; k < rgba.size(); k += 4) std::swap(rgba[k], rgba[k + 2]);
                                Texture* tex = gfx->GetTextureFromId(curInstance->parts->textures);
                                if (!tex) { tex = new Texture; curInstance->parts->textures.push_back(tex); }
                                tex->LoadDirect((char*)rgba.data(), w, h, true); tex->Apply(false, false); gfx->textureIndex = tex->id;
                            }
                            textureDecoratedNames[t] = curInstance->parts->GetTexturesDecorateName(t);
                            curInstance->markEdited();
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(LBL("Export PNG"))) {
                    std::string filename = std::filesystem::path(gfx->name.c_str()).stem().string() + ".png";
                    std::string &&file = FileDialog(fileType::IMAGESAVE, true, const_cast<char *>(filename.c_str()));
                    if (!file.empty()) {
                        cgm::PatTexSource src(*curInstance->parts); std::string err;
                        if (!cgm::ExportPng(src, curInstance->currState->partGraph, file, &err)) { openpopupWithId = LBL("Import Texture Error"); customPopUpMessage = err; }
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
