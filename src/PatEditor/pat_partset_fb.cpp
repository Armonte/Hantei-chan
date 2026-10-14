// PAT editor, poses and parts of a French-Bread (RBO / GOF2 / GOF1) character: the PACNyx+ DATView pose / body-part controls
// (pose index, named add, rename, reorder, base point, save as image; part add from a sheet rectangle, nudge, rotate, flip, layer
// commands, colour, source rect / origin). Reimplemented from the behaviour of PACNyx+ (2010) and icaroffa's 2026 build.
// Model operations live in han2_parts_edit.cpp; undo / the tab '*' come from the character's undo history (parts/parts_undo.h).
#include "pat_partset_pane.h"
#include "../parts/parts.h"
#include "../filedialog.h"
#include "../han2_parts_edit.h"
#include "../han2_export.h"
#include "../png_writer.h"
#include "../copy_manager.h"
#include "../imgui_utils.h"
#include "../i18n.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {
std::string Fmt(const char *fmt, ...)
{
    char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); return b;
}
int Step() { return ImGui::GetIO().KeyShift ? 10 : 1; }

// Frames reference poses by index (layer spriteId with usePat): keep them pointing at the same content after a swap.
int RemapPoseRefs(FrameData &fd, int a, int b)
{
    int n = 0;
    for (size_t s = 0; s < fd.m_sequences.size(); s++) {
        const int before = n;
        for (auto &f : fd.m_sequences[s].frames)
            for (auto &l : f.AF.layers) {
                if (!l.usePat) continue;
                if (l.spriteId == a) { l.spriteId = b; n++; }
                else if (l.spriteId == b) { l.spriteId = a; n++; }
            }
        if (n != before) fd.mark_modified((int)s);   // the undo manager's incremental commit only diffs marked patterns
    }
    return n;
}
} // namespace

void PatPartSetPane::DrawFbPoses()
{
    using namespace han2::edit;
    Parts &parts = *curInstance->parts;
    FrameState &st = *curInstance->currState;
    const int nSets = (int)parts.partSets.size();
    const int maxPoses = MaxPoses(parts);
    if (st.partSet < 0 || st.partSet >= nSets) st.partSet = nSets > 0 ? 0 : -1;

    // ---- pose index / name ----
    ImGui::SetNextItemWidth(130);
    int idx = st.partSet;
    if (ImGui::InputInt(LBL("Pose"), &idx, 1, 10) && nSets > 0) { idx = std::clamp(idx, 0, nSets - 1); UpdateRenderFrame(idx); }
    ImGui::SameLine();
    ImGui::TextDisabled(TXT("of %d (this PAT holds %d)"), nSets, maxPoses);
    if (nSets > 0 && partSetDecoratedNames && ImGui::BeginCombo(LBL("Part Set"), partSetDecoratedNames[st.partSet].c_str(), ImGuiComboFlags_HeightLargest)) {
        for (int n = 0; n < nSets; n++) {
            if (ImGui::Selectable(partSetDecoratedNames[n].c_str(), st.partSet == n)) { UpdateRenderFrame(n); RegeneratePartPropNames(); }
            if (st.partSet == n) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    PartSet<> *ps = st.partSet >= 0 ? parts.GetPartSet(st.partSet) : nullptr;
    if (ps) {
        if (ImGui::InputText(LBL("Pose name"), &ps->name)) partSetDecoratedNames[st.partSet] = parts.GetPartSetDecorateName(st.partSet);
    }

    // ---- add (named) / delete ----
    ImGui::SetNextItemWidth(160);
    ImGui::InputText(LBL("New pose name"), &fbNewPoseName);
    ImGui::SameLine();
    std::string why;
    const bool canAdd = CanAddPose(parts, &why) && !fbNewPoseName.empty();
    ImGui::BeginDisabled(!canAdd);
    if (ImGui::Button(LBL("Add pose"))) {
        int at = -1; std::string e;
        if (AddPose(parts, fbNewPoseName, &at, &e)) { RegeneratePartSetNames(); UpdateRenderFrame(at); RegeneratePartPropNames(); fbMessage = Fmt(TXT("pose %d added"), at); fbNewPoseName.clear(); curInstance->markEdited(); }
        else fbMessage = e;
    }
    ImGui::EndDisabled();
    if (!canAdd && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", fbNewPoseName.empty() ? TXT("A pose needs a name.") : TXT(why.c_str()));
    if (ps) {
        ImGui::SameLine();
        if (ImGui::Button(LBL("Clear pose"))) {   // the slot stays (frames reference poses by index)
            ps->groups.clear(); ps->name.clear();
            RegeneratePartSetNames(); RegeneratePartPropNames(); st.partProp = 0; curInstance->markEdited();
        }
    }
    if (!ps) { if (!fbMessage.empty()) ImGui::TextWrapped("%s", fbMessage.c_str()); return; }

    // ---- reorder: swap with another index (frames follow) ----
    ImGui::SetNextItemWidth(130);
    ImGui::InputInt(LBL("Move to index"), &fbMoveTarget, 1, 10);
    fbMoveTarget = std::clamp(fbMoveTarget, 0, maxPoses - 1);
    ImGui::SameLine();
    if (ImGui::Button(LBL("Swap"))) {
        std::string e; const int from = st.partSet;
        if (SwapPoses(parts, from, fbMoveTarget, &e)) {
            const int refs = fbRemapFrames ? RemapPoseRefs(*curInstance->framedata, from, fbMoveTarget) : 0;
            RegeneratePartSetNames(); UpdateRenderFrame(fbMoveTarget, true); RegeneratePartPropNames();
            fbMessage = Fmt(TXT("pose %d <-> %d, %d frame references updated"), from, fbMoveTarget, refs); curInstance->markEdited();
        } else fbMessage = e;
    }
    ImGui::SameLine();
    ImGui::Checkbox(LBL("Update frame references"), &fbRemapFrames);

    // ---- base point: move every part of the pose (PACNyx "Set pose base point") ----
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TXT("Pose against its base point"));
    ImGui::SameLine();
    ImGui::PushButtonRepeat(true);
    if (ImGui::ArrowButton("##pl", ImGuiDir_Left)) ShiftPose(*ps, -Step(), 0);
    ImGui::SameLine(); if (ImGui::ArrowButton("##pr", ImGuiDir_Right)) ShiftPose(*ps, Step(), 0);
    ImGui::SameLine(); if (ImGui::ArrowButton("##pu", ImGuiDir_Up)) ShiftPose(*ps, 0, -Step());
    ImGui::SameLine(); if (ImGui::ArrowButton("##pd", ImGuiDir_Down)) ShiftPose(*ps, 0, Step());
    ImGui::PopButtonRepeat();
    ImGui::SameLine();
    ImGui::Checkbox(LBL("Show base point"), &st.showPoseBasePoint);
    if (ImGui::Button(LBL("Save pose as image..."))) {
        std::vector<uint8_t> px; int w = 0, h = 0, ax = 0, ay = 0;
        if (!han2::RenderPoseRgba(parts, st.partSet, px, w, h, ax, ay, 1)) fbMessage = TXT("This pose draws nothing.");
        else {
            char def[260]; snprintf(def, sizeof def, "%s.png", ps->name.empty() ? ("pose" + std::to_string(st.partSet)).c_str() : ps->name.c_str());
            std::string f = FileDialog(fileType::IMAGESAVE, true, def);
            if (!f.empty()) { std::string e; f = AnsiToUtf8(f); fbMessage = WriteImageRgba(f, px.data(), w, h, e) ? Fmt(TXT("saved %s (%d x %d, base point at %d,%d)"), f.c_str(), w, h, ax, ay) : e; }
        }
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(LBL("Highlight selected Part Prop"), &activeHighligth)) UpdateHighligth(activeHighligth);

    DrawFbParts(*ps);
    if (!fbMessage.empty()) { ImGui::PushTextWrapPos(0.0f); ImGui::TextUnformatted(fbMessage.c_str()); ImGui::PopTextWrapPos(); }
}

void PatPartSetPane::DrawFbParts(PartSet<> &ps)
{
    using namespace han2::edit;
    Parts &parts = *curInstance->parts;
    FrameState &st = *curInstance->currState;
    ImGui::Separator();
    const int used = UsedPartSlots(ps);
    int count = 0; for (auto &g : ps.groups) if (g.ppId >= 0) count++;
    ImGui::Text(TXT("Parts: %d of 40 (draw order: higher layer on top)"), count);

    // slot list (40 rows, like PACNyx's body-part list)
    ImGui::BeginChild("fbparts", ImVec2(0, 150), true);
    for (int k = 0; k < kPartsPerPose; k++) {
        const bool has = k < (int)ps.groups.size() && ps.groups[k].ppId >= 0;
        std::string label;
        if (has) {
            const auto &g = ps.groups[k];
            const CutOut<> *co = parts.GetCutOut(g.ppId);
            label = Fmt("%2d  L%-3d sheet %d  (%d,%d)", k, LayerOf(g), co ? co->texture : -1, g.x, g.y);
        } else label = Fmt("%2d  -", k);
        if (!has) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable((label + "##slot" + std::to_string(k)).c_str(), st.partProp == k)) {
            st.partProp = k;
            parts.partHighlight = activeHighligth ? k : -1;
        }
        if (!has) ImGui::PopStyleColor();
        if (k > used + 2 && k > 8) { ImGui::TextDisabled("..."); break; }
    }
    ImGui::EndChild();

    // add / copy / paste / remove
    std::string why;
    const bool canAdd = CanAddPart(parts, st.partSet, &why);
    ImGui::BeginDisabled(!canAdd || !st.sheetSelValid);
    if (ImGui::Button(LBL("Add part from sheet selection"))) {
        std::string e;
        const int slot = AddPartFromSheet(parts, st.partSet, st.partGraph, st.sheetSel[0], st.sheetSel[1], st.sheetSel[2], st.sheetSel[3], st.partProp, &e);
        if (slot >= 0) { st.partProp = slot; RegeneratePartPropNames(); RegeneratePartSetNames(); fbMessage = Fmt(TXT("part %d added from sheet %d"), slot, st.partGraph); curInstance->markEdited(); }
        else fbMessage = e;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", !canAdd ? TXT(why.c_str()) : TXT("Show a sprite sheet (Texture pane) and right-drag a rectangle on it, then add the part."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!canAdd);
    if (ImGui::Button(LBL("Add part..."))) { fbAddOpen = true; fbAddSheet = std::max(0, st.partGraph); if (st.sheetSelValid) for (int i = 0; i < 4; i++) fbAddRect[i] = st.sheetSel[i]; }
    ImGui::EndDisabled();
    if (fbAddOpen) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(120); ImGui::InputInt(LBL("Sprite sheet"), &fbAddSheet);
        ImGui::SetNextItemWidth(260); ImGui::InputInt4(LBL("Rect x, y, w, h (pixels)"), fbAddRect);
        if (ImGui::Button(LBL("Create part"))) {
            std::string e;
            const int slot = AddPartFromSheet(parts, st.partSet, fbAddSheet, fbAddRect[0], fbAddRect[1], fbAddRect[2], fbAddRect[3], st.partProp, &e);
            if (slot >= 0) { st.partProp = slot; fbAddOpen = false; RegeneratePartPropNames(); RegeneratePartSetNames(); fbMessage = Fmt(TXT("part %d added from sheet %d"), slot, fbAddSheet); curInstance->markEdited(); }
            else fbMessage = e;
        }
        ImGui::SameLine();
        if (ImGui::Button(LBL("Cancel"))) fbAddOpen = false;
        ImGui::Unindent();
    }

    const bool has = st.partProp >= 0 && st.partProp < (int)ps.groups.size() && ps.groups[st.partProp].ppId >= 0;
    ImGui::BeginDisabled(!has);
    if (ImGui::Button(LBL("Copy Part Prop")) && has) PartSet<>::CopyPropertyTo(&CopyManager::copiedParts->partProperty, &ps.groups[st.partProp]);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool clip = CopyManager::copiedParts->partProperty.ppId >= 0 && CopyManager::copiedParts->partProperty.ppId < (int)parts.cutOuts.size();
    ImGui::BeginDisabled(!clip || st.partProp < 0 || st.partProp >= kPartsPerPose || (!has && !canAdd));
    if (ImGui::Button(LBL("Paste Part Prop"))) {
        if ((int)ps.groups.size() <= st.partProp) { const size_t old = ps.groups.size(); ps.groups.resize(st.partProp + 1); for (size_t k = old; k < ps.groups.size(); k++) ps.groups[k].propId = (int)k; }
        PartSet<>::CopyPropertyTo(&ps.groups[st.partProp], &CopyManager::copiedParts->partProperty);
        ps.groups[st.partProp].propId = st.partProp;
        ps.groups[st.partProp].priority = (float)LayerOf(ps.groups[st.partProp]) + st.partProp / 1000.f;
        RegeneratePartPropNames(); RegeneratePartSetNames(); curInstance->markEdited();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!has);
    if (ImGui::Button(LBL("Remove part")) && has) { ps.groups[st.partProp] = PartProperty{}; ps.groups[st.partProp].propId = st.partProp; RegeneratePartPropNames(); RegeneratePartSetNames(); curInstance->markEdited(); }
    ImGui::EndDisabled();
    if (!has) return;

    PartProperty &p = ps.groups[st.partProp];
    ImGui::Separator();
    // position + nudge
    int pos[2] = {p.x, p.y};
    ImGui::SetNextItemWidth(160);
    if (ImGui::DragInt2(LBL("Position"), pos, 1)) { p.x = pos[0]; p.y = pos[1]; }
    ImGui::SameLine();
    ImGui::PushButtonRepeat(true);
    if (ImGui::ArrowButton("##nl", ImGuiDir_Left)) p.x -= Step();
    ImGui::SameLine(); if (ImGui::ArrowButton("##nr", ImGuiDir_Right)) p.x += Step();
    ImGui::SameLine(); if (ImGui::ArrowButton("##nu", ImGuiDir_Up)) p.y -= Step();
    ImGui::SameLine(); if (ImGui::ArrowButton("##nd", ImGuiDir_Down)) p.y += Step();
    ImGui::PopButtonRepeat();
    // rotation (stored as turns: 1.0 = 360 degrees, the file keeps 1/10000 turns)
    float deg = p.rotation[3] * 360.f;
    ImGui::SetNextItemWidth(110);
    if (ImGui::DragFloat(LBL("Rotation (deg)"), &deg, 0.5f, -3600.f, 3600.f, "%.1f")) p.rotation[3] = deg / 360.f;
    ImGui::SameLine();
    ImGui::PushButtonRepeat(true);
    const float stepsDeg[4] = {-15.f, -1.f, 1.f, 15.f};
    const char *stepLbl[4] = {"-15", "-1", "+1", "+15"};
    for (int i = 0; i < 4; i++) {
        if (i) ImGui::SameLine(0, 2);
        if (ImGui::SmallButton((std::string(stepLbl[i]) + "##rot").c_str())) p.rotation[3] = std::round((p.rotation[3] * 360.f + stepsDeg[i]) * 10000.f / 360.f) / 10000.f;
    }
    ImGui::PopButtonRepeat();
    // scale (permille in the file) + flip
    float sc[2] = {p.scaleX * 100.f, p.scaleY * 100.f};
    ImGui::SetNextItemWidth(160);
    if (ImGui::DragFloat2(LBL("Scale H / V (%)"), sc, 0.5f, -1000.f, 1000.f, "%.1f")) { p.scaleX = sc[0] / 100.f; p.scaleY = sc[1] / 100.f; }
    static const char *const flips[] = {"None", "Flip Horizontally", "Flip Vertically", "Flip both"};
    ImGui::SetNextItemWidth(160);
    i18n::Combo(LBL("Flip Side"), &p.flip, flips, 4);
    // layer commands
    ImGui::AlignTextToFramePadding();
    ImGui::Text(TXT("Layer %d"), LayerOf(p));
    ImGui::SameLine();
    if (ImGui::Button(LBL("To back"))) LayerCommand(ps, st.partProp, LayerCmd::ToBack);
    ImGui::SameLine(); if (ImGui::Button(LBL("Back one"))) LayerCommand(ps, st.partProp, LayerCmd::Backward);
    ImGui::SameLine(); if (ImGui::Button(LBL("Forward one"))) LayerCommand(ps, st.partProp, LayerCmd::Forward);
    ImGui::SameLine(); if (ImGui::Button(LBL("To front"))) LayerCommand(ps, st.partProp, LayerCmd::ToFront);
    ImGui::SetNextItemWidth(110);
    ImGui::InputInt(LBL("Layer value"), &fbLayerValue, 1, 10);
    fbLayerValue = std::clamp(fbLayerValue, 0, 255);
    ImGui::SameLine();
    if (ImGui::Button(LBL("Set layer"))) LayerCommand(ps, st.partProp, LayerCmd::Set, fbLayerValue);
    // colour (bytes A,R,G,B in the file) and additive tint
    float rgba[4] = {p.bgra[2] / 255.f, p.bgra[1] / 255.f, p.bgra[0] / 255.f, p.bgra[3] / 255.f};
    if (ImGui::ColorEdit4(LBL("RGBA Color"), rgba)) {
        p.bgra[2] = (unsigned char)std::lround(rgba[0] * 255.f); p.bgra[1] = (unsigned char)std::lround(rgba[1] * 255.f);
        p.bgra[0] = (unsigned char)std::lround(rgba[2] * 255.f); p.bgra[3] = (unsigned char)std::lround(rgba[3] * 255.f);
    }
    ImGui::ColorEdit3(LBL("Add Color"), p.addColor);

    // ---- the part's cut-out: sheet, source rect, quad size, origin (made unique to this part before an edit) ----
    CutOut<> *co = parts.GetCutOut(p.ppId);
    if (!co) return;
    ImGui::Separator();
    const PartGfx<> *g = parts.GetPartGfx(co->texture);
    const int sheetW = g ? g->w : 256, sheetH = g ? g->h : 256;
    int sheet = co->texture;
    int src[4] = {UnitsToPx(co->uv[0], sheetW), UnitsToPx(co->uv[1], sheetH), UnitsToPx(co->uv[2], sheetW), UnitsToPx(co->uv[3], sheetH)};
    int wh[2] = {co->wh[0], co->wh[1]}, org[2] = {co->xy[0], co->xy[1]};
    auto own = [&]() -> CutOut<> * { const int c = UniqueCutOut(parts, st.partSet, st.partProp); return parts.GetCutOut(c); };
    ImGui::SetNextItemWidth(110);
    if (ImGui::InputInt(LBL("Sprite sheet##part"), &sheet) && sheet >= 0 && sheet < (int)parts.gfxMeta.size()) { if (auto *c = own()) c->texture = sheet; }
    ImGui::SetNextItemWidth(260);
    if (ImGui::InputInt4(LBL("Source rect (px)"), src)) {
        if (auto *c = own()) { c->uv[0] = PxToUnits(src[0], sheetW); c->uv[1] = PxToUnits(src[1], sheetH); c->uv[2] = PxToUnits(src[2], sheetW); c->uv[3] = PxToUnits(src[3], sheetH); }
    }
    ImGui::BeginDisabled(!st.sheetSelValid);
    if (ImGui::Button(LBL("Use sheet selection"))) {
        if (auto *c = own()) {
            const PartGfx<> *sg = parts.GetPartGfx(st.partGraph);
            const int w2 = sg ? sg->w : sheetW, h2 = sg ? sg->h : sheetH;
            c->texture = st.partGraph;
            c->uv[0] = PxToUnits(st.sheetSel[0], w2); c->uv[1] = PxToUnits(st.sheetSel[1], h2); c->uv[2] = PxToUnits(st.sheetSel[2], w2); c->uv[3] = PxToUnits(st.sheetSel[3], h2);
            c->wh[0] = st.sheetSel[2]; c->wh[1] = st.sheetSel[3];
        }
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(160);
    if (ImGui::DragInt2(LBL("Quad size"), wh, 1)) { if (auto *c = own()) { c->wh[0] = wh[0]; c->wh[1] = wh[1]; } }
    ImGui::SetNextItemWidth(160);
    if (ImGui::DragInt2(LBL("Origin (pivot)"), org, 1)) { if (auto *c = own()) { c->xy[0] = org[0]; c->xy[1] = org[1]; } }
    ImGui::SameLine();
    if (ImGui::Button(LBL("Centre"))) { if (auto *c = own()) { c->xy[0] = c->wh[0] / 2; c->xy[1] = c->wh[1] / 2; } }
}
