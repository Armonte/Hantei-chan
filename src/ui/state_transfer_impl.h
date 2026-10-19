#pragma once

void MainFrame::TraceStateTransfer(const char* action) {
    auto owner=m_session.owner(m_stateTransfer.view);
    auto* window=GImGui?GImGui->CurrentWindow:nullptr;
    const auto viewport=window && window->Viewport?window->Viewport->ID:0;
    HC_LOG_DEBUG("state transfer action={} view={} host={} viewport={} source={}:{}-{} target={}:{}-{} groups={} version={} changed={} ready={}",
        action,(unsigned long long)m_stateTransfer.view,(unsigned long long)(owner?*owner:0),
        viewport,m_stateTransfer.request.sourcePattern,m_stateTransfer.request.sourceFirst,
        m_stateTransfer.request.sourceLast,m_stateTransfer.request.targetPattern,m_stateTransfer.request.targetFirst,
        m_stateTransfer.request.targetLast,m_stateTransfer.request.groups,(unsigned long long)m_stateTransfer.version,
        m_stateTransfer.plan.changed,m_stateTransfer.ready);
}
void MainFrame::OpenStateTransfer(CharacterView* view) {
    if(!view || view->isStageView() || view->getCharacter()->frameData.game()!=Ha6Game::MBAACC) return;
    m_stateTransfer={}; auto& w=m_stateTransfer; w.open=true; w.view=view->getId();
    w.request.sourcePattern=w.request.targetPattern=view->getState().pattern;
    w.request.sourceFirst=w.request.sourceLast=w.request.targetFirst=w.request.targetLast=view->getState().frame;
    TraceStateTransfer("open");
}
void MainFrame::PreviewStateTransfer() {
    auto& w=m_stateTransfer; auto* view=findViewById(w.view);
    if(!view || view->getCharacter()->frameData.game()!=Ha6Game::MBAACC) {w.ready=false; w.message="Source view closed or unsupported."; return;}
    auto& fd=view->getCharacter()->frameData;
    w.plan=statetransfer::Preview(fd.m_sequences,w.request); w.version=fd.dataVersion;
    w.ready=w.plan.error.empty(); w.message=w.plan.error; TraceStateTransfer("preview");
}
bool MainFrame::ApplyStateTransfer() {
    auto& w=m_stateTransfer; auto* view=findViewById(w.view);
    if(!view || !w.ready || view->getCharacter()->frameData.dataVersion!=w.version) {
        w.ready=false; w.message="Document changed; preview again."; TraceStateTransfer("stale"); return false;
    }
    auto* ch=view->getCharacter(); auto& fd=ch->frameData; auto& undo=ch->undoManager;
    if(!w.plan.changed) {w.message="Nothing to change."; return false;}
    if(undo.inTransaction()) {w.message="Finish the current editing gesture first."; return false;}
    undo.flush(); undo.beginTransaction("State property transfer");
    const bool ok=statetransfer::Apply(fd.m_sequences,w.plan,w.message);
    if(ok) {
        fd.mark_modified(w.plan.request.targetPattern); markToolEdit(ch);
        for(auto& entry:views) if(entry->getCharacter()==ch) entry->getState().spriteId=-1;
    }
    undo.commitTransaction(); TraceStateTransfer(ok?"apply":"rejected"); w.ready=false;
    if(ok) w.message="Applied as one undo step. Files have not been saved.";
    return ok;
}
void MainFrame::DrawStateTransfer() {
    auto& w=m_stateTransfer; if(!w.open) return;
    auto* view=findViewById(w.view);
    if(view) if(auto* surface=FindSurface(view)) if(surface->viewport && surface->viewport!=w.hostViewport) {
        ImGui::SetNextWindowViewport(surface->viewport); w.hostViewport=surface->viewport;
    }
    ui::FitNextWindow(680,560);
    if(!ImGui::Begin("State property transfer",&w.open,ImGuiWindowFlags_NoDocking)) {ImGui::End(); return;}
    if(ImGui::IsWindowAppearing()) TraceStateTransfer("window");
    if(!view || view->getCharacter()->frameData.game()!=Ha6Game::MBAACC) {
        ImGui::TextWrapped("The owning character view was closed. Reopen this tool from the desired tab.");
        ImGui::End(); return;
    }
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        shortcuts.claimFocus(ShortcutContext::characterView,w.view);
    auto& fd=view->getCharacter()->frameData;
    if(w.ready && fd.dataVersion!=w.version) {w.ready=false; w.message="Document changed; preview again.";}
    const auto& path=view->getCharacter()->getTopHA6Path();
    ImGui::TextWrapped("Character: %s",path.c_str());
    ImGui::TextWrapped("Existing MBAACC states only. Effects, conditions, notes, jumps and loop settings are preserved.");
    auto& q=w.request; bool edited=false;
    ImGui::SeparatorText("Source");
    edited|=ImGui::InputInt("Source pattern",&q.sourcePattern);
    edited|=ImGui::Checkbox("Repeat one source state",&q.repeat);
    if(q.repeat) {edited|=ImGui::InputInt("Source state",&q.sourceFirst); q.sourceLast=q.sourceFirst;}
    else {int range[]={q.sourceFirst,q.sourceLast}; if(ImGui::InputInt2("Source states (inclusive)",range)) {q.sourceFirst=range[0];q.sourceLast=range[1];edited=true;}}
    if(q.sourcePattern>=0 && q.sourcePattern<fd.get_sequence_count()) ImGui::TextDisabled("%s",fd.GetDecoratedName(q.sourcePattern).c_str());
    ImGui::SeparatorText("Destination");
    edited|=ImGui::InputInt("Destination pattern",&q.targetPattern);
    int range[]={q.targetFirst,q.targetLast};
    if(ImGui::InputInt2("Destination states (inclusive)",range)) {q.targetFirst=range[0];q.targetLast=range[1];edited=true;}
    if(q.targetPattern>=0 && q.targetPattern<fd.get_sequence_count()) ImGui::TextDisabled("%s",fd.GetDecoratedName(q.targetPattern).c_str());
    ImGui::SeparatorText("Properties to replace");
    for(unsigned g=1;g<=statetransfer::Boxes;g<<=1) {
        bool selected=(q.groups&g)!=0;
        if(g!=1 && g!=statetransfer::Attack) ImGui::SameLine();
        if(ImGui::Checkbox(statetransfer::Name(g),&selected)) {q.groups=selected?q.groups|g:q.groups&~g;edited=true;}
        if(ImGui::IsItemHovered()) {
            if(g==statetransfer::Animation) ImGui::SetTooltip("Replaces all layers: sprite/PAT, offsets, scale, rotation, color, blend, layer priority and AFRT. Keeps interpolation and control flow.");
            if(g==statetransfer::State) ImGui::SetTooltip("Replaces all AS data, including movement, stance, cancels, invincibility, state flags and sine settings.");
            if(g==statetransfer::Attack) ImGui::SetTooltip("Replaces all AT data, including damage, hitgrab, hit/guard vectors and attack flags.");
            if(g==statetransfer::Boxes) ImGui::SetTooltip("Replaces the complete authored box list.");
        }
    }
    if(edited) {w.ready=false; w.message.clear();}
    if(ImGui::Button("Preview changes")) PreviewStateTransfer();
    ImGui::SameLine(); ImGui::BeginDisabled(!w.ready || !w.plan.changed);
    if(ImGui::Button("Apply preview (one undo step)")) ApplyStateTransfer();
    ImGui::EndDisabled();
    if(!w.message.empty()) ImGui::TextWrapped("%s",w.message.c_str());
    if(w.ready) {
        ImGui::Text("%d of %zu states change.",w.plan.changed,w.plan.rows.size());
        if(ImGui::BeginTable("changes",3,ImGuiTableFlags_Borders|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
            ImGui::TableSetupColumn("Source state"); ImGui::TableSetupColumn("Destination state"); ImGui::TableSetupColumn("Changed properties"); ImGui::TableHeadersRow();
            ImGuiListClipper clip; clip.Begin(int(w.plan.rows.size()));
            while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto& r=w.plan.rows[i]; ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("%d",r.source);
                ImGui::TableNextColumn(); ImGui::Text("%d",r.target); ImGui::TableNextColumn();
                std::string groups; for(unsigned g=1;g<=statetransfer::Boxes;g<<=1) if(r.changed&g) {if(!groups.empty()) groups+=", ";groups+=statetransfer::Name(g);}
                ImGui::TextWrapped("%s",groups.empty()?"Unchanged":groups.c_str());
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
