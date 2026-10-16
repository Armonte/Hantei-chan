// PAT editor: part set renumbering (move / place / insert / delete / clear / swap), src/parts/pat_renumber.h.
// The operation changes the model in one frame (one undo step of the PAT document). The HA6 files that draw the .pat
// are rewritten when the PAT is saved (MainFrame::savePatOf), from Parts::diskIdOfOrig and the slots' origId.
#include "pat_partset_pane.h"
#include "../parts/parts.h"
#include "../parts/pat_renumber.h"
#include "../png_writer.h"
#include "../i18n.h"
#include "../ui/ui_style.h"
#include "../ui/ui_layout2.h"
#include "../app_log.h"
#include <windows.h>
#include <imgui.h>
#include <algorithm>

static std::string AcpToUtf8(const std::string &s)
{
	if (s.empty()) return s;
	const int n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, 0); MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), &w[0], n);
	return WideToUtf8(w);
}

static const char *const kRnKinds[] = {"Move block", "Place block in empty ids", "Insert empty ids", "Delete (close up)", "Clear (leave gaps)", "Swap two ids"};

static patrn::Op OpFrom(int kind, int from, int to, int target, int count)
{
	patrn::Op op; op.kind = (patrn::Op::Kind)kind;
	op.a = from; op.b = std::max(from, to); op.to = target; op.n = count;
	if (op.kind == patrn::Op::Insert) op.to = from;
	if (op.kind == patrn::Op::Swap) op.b = target;
	return op;
}

void PatPartSetPane::DrawRenumber()
{
	Parts *parts = curInstance->parts;
	if (!ImGui::CollapsingHeader(LBL("Renumber part sets"))) return;
	if (!curInstance->isPatDocument || !curInstance->isPatDocument()) {
		ImGui::TextWrapped("%s", TXT("Open the .pat on its own (File > Open PAT) to renumber part sets: the HA6 files that draw it are rewritten when it is saved."));
		return;
	}
	const std::string path = AcpToUtf8(parts->filePath);
	if (path.empty()) { ImGui::TextWrapped("%s", TXT("Save the PAT first: renumbering rewrites the HA6 files of the projects that use it.")); return; }
	auto *users = static_cast<patrn::Users *>(rnUsers);
	if (!users || rnScannedPath != path || ImGui::Button(LBL("Rescan references"))) {
		delete users; users = new patrn::Users(patrn::FindUsers(path)); rnUsers = users; rnScannedPath = path;
		HC_LOG_INFO("[PatRenumber] {}: {} user files, {} references", path, users->files.size(), users->refCount());
	}
	ImGui::Text(TXT("%d HA6 files draw this .pat (%d references)"), (int)users->files.size(), users->refCount());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", TXT("Every [DataFile] HA6 of the project .txt files in this folder whose [PAniFile] names the .pat."));
	ImGui::SetNextItemWidth(ui::Px(220));
	ImGui::Combo(LBL("Operation"), &rnKind, [](void *, int i) { return TXT(kRnKinds[i]); }, nullptr, IM_ARRAYSIZE(kRnKinds));
	const auto kind = (patrn::Op::Kind)rnKind;
	ImGui::SetNextItemWidth(ui::Px(100)); ImGui::InputInt(kind == patrn::Op::Insert ? LBL("At id") : kind == patrn::Op::Swap ? LBL("Id A") : LBL("From id"), &rnFrom);
	if (kind != patrn::Op::Insert && kind != patrn::Op::Swap) { ui::SameLineField(ui::Px(100), LBL("To id (block end)")); ImGui::SetNextItemWidth(ui::Px(100)); ImGui::InputInt(LBL("To id (block end)"), &rnTo); }
	if (kind == patrn::Op::Move || kind == patrn::Op::Place || kind == patrn::Op::Swap) { ImGui::SetNextItemWidth(ui::Px(100)); ImGui::InputInt(kind == patrn::Op::Swap ? LBL("Id B") : LBL("Target id"), &rnTarget); }
	if (kind == patrn::Op::Insert) { ui::SameLineField(ui::Px(100), LBL("Count")); ImGui::SetNextItemWidth(ui::Px(100)); ImGui::InputInt(LBL("Count"), &rnCount); }
	if (ImGui::Button(LBL("Use selected part set"))) { rnFrom = rnTo = curInstance->currState->partSet; }
	ui::SameLineButton(LBL("Preview..."));
	if (ImGui::Button(LBL("Preview..."))) {
		// apply to the live model, plan, and put the model back: the preview changes nothing
		const auto saved = parts->partSets; const auto savedDisk = parts->diskIdOfOrig;
		std::string err;
		const int extent = patrn::MaxRef(*users) + 1;
		const patrn::Op op = OpFrom(rnKind, rnFrom, rnTo, rnTarget, rnCount);
		rnPreview = patrn::Describe(op) + "\n\n";
		if (!patrn::ApplyOp(*parts, op, extent, &err)) { rnPreview += err; rnPreviewOk = false; }
		else {
			const patrn::Plan plan = patrn::MakePlan(*users, patrn::PendingDiskRemap(*parts, extent));
			rnPreviewOk = plan.ok();
			char line[64]; snprintf(line, sizeof line, TXT("%d references change on save:"), plan.changed);
			rnPreview += parts->filePath + "\n" + line + "\n";
			for (auto &f : plan.files) {
				char b[48]; snprintf(b, sizeof b, " %d / %d", f.changed, f.refs);
				rnPreview += "  " + f.path + b + (f.shared ? TXT(" (shared, never rewritten)") : "") + "\n";
			}
			for (auto &r : plan.refusals) rnPreview += std::string(TXT("Refused: ")) + r + "\n";
		}
		parts->partSets = saved; parts->diskIdOfOrig = savedDisk;
		rnPreviewOpen = true;
	}
	{
		const patrn::Plan pending = patrn::MakePlan(*users, patrn::PendingDiskRemap(*parts, patrn::MaxRef(*users) + 1));
		if (pending.changed) ImGui::TextColored(ImVec4(1.f, .8f, .3f, 1.f), TXT("Unsaved: %d references in %d files change when the PAT is saved"), pending.changed,
			(int)std::count_if(pending.files.begin(), pending.files.end(), [](const patrn::Plan::File &f) { return f.changed > 0 && !f.shared; }));
	}
	if (!rnMessage.empty()) ImGui::TextWrapped("%s", rnMessage.c_str());
}

void PatPartSetPane::DrawRenumberPopup()
{
	if (rnPreviewOpen) { ImGui::OpenPopup(LBL("Renumber part sets##preview")); rnPreviewOpen = false; }
	if (!ImGui::BeginPopupModal(LBL("Renumber part sets##preview"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
	ImGui::BeginChild("rnlist", ImVec2(std::min(ui::Px(640), ImGui::GetContentRegionAvail().x), ui::Px(260)), true);
	ImGui::TextUnformatted(rnPreview.c_str());
	ImGui::EndChild();
	ImGui::BeginDisabled(!rnPreviewOk);
	if (ImGui::Button(LBL("Apply"), ImVec2(ui::Px(120), 0))) {
		auto *users = static_cast<patrn::Users *>(rnUsers);
		std::string err;
		const patrn::Op op = OpFrom(rnKind, rnFrom, rnTo, rnTarget, rnCount);
		if (users && patrn::ApplyOp(*curInstance->parts, op, patrn::MaxRef(*users) + 1, &err)) {
			rnMessage = patrn::Describe(op) + TXT(": applied (Ctrl+Z undoes it; Save writes the .pat and the HA6 files)");
			HC_LOG_INFO("[PatRenumber] {}", rnMessage);
			curInstance->markEdited();
			RegeneratePartSetNames();
			UpdateRenderFrame(std::min(curInstance->currState->partSet, (int)curInstance->parts->partSets.size() - 1), true);
		} else rnMessage = err;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(LBL("Cancel"), ImVec2(ui::Px(120), 0))) ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}
