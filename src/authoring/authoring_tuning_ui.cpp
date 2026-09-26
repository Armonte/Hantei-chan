// [authoring] The Tuning tab (docs/HANTEI_AUTHORING_MODE.md §5.5 as changed by §8.1-§8.4 / §8.9 and §9.1):
//   * the sidecar tree bar: files, save / revert, checkpoints, undo / redo, A/B snapshots, migrate, the game's sha;
//   * Global & styles: the style, Save as style, every non-CharOnly lever with its provenance and the MBAC reference;
//   * one inspector per picked character + moon: the layer (all moons = chars\<f>.ini, or the moon file), every perChar
//     lever, tag-in / tag-out with the HA6 pattern picker (read-only) + inline frame data + the MBAC reference, and the
//     five assist slots with the _c.txt command picker (read-only) and the resolved move's frame data.
// Every edit goes through the EditSink: one undo step, saved at once, live-applied (ApplyTuning).
#include "authoring_state.h"
#include "frame_summary.h"
#include "mbac_reference.h"
#include "roster_mirror.h"
#include "../cmdfile/cmd_io.h"
#include "../framedata.h"
#include "../tag_tuning/tag_assist.h"
#include "../tag_tuning/tag_migrate.h"
#include "../tag_tuning/tag_widgets.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>

namespace authoring {

namespace fs = std::filesystem;
namespace wire = gamelink::wire;
using namespace tagtune;

namespace {

bool FileExists(const std::string& p) { std::error_code ec; return !p.empty() && fs::exists(fs::u8path(p), ec); }

tagtune::Layer TargetLayer() { return St().editShipped ? tagtune::Layer::Shipped : tagtune::Layer::Local; }

std::string ProvText(const Provenance& p)
{
	if (p.src == Src::Default) return "default";
	std::string t = SrcName(p.src);
	if (p.src != Src::Env && p.src != Src::Css) t += p.layer == tagtune::Layer::Local ? " - local" : " - shipped";
	return t;
}

ImVec4 ProvColor(const Provenance& p)
{
	switch (p.src) {
	case Src::Default: return kColDim;
	case Src::Style: return ImVec4(0.7f, 0.85f, 0.7f, 1.0f);
	case Src::Tuning: return kColOver;
	case Src::Char: return ImVec4(0.55f, 1.0f, 0.75f, 1.0f);
	case Src::Moon: return kColMoon;
	default: return kColWarn;
	}
}

// ---- the _c.txt command list (read-only), cached per path ----
const std::vector<CommandInfo>& CommandsOf(const std::string& path)
{
	static std::map<std::string, std::vector<CommandInfo>> cache;
	auto it = cache.find(path);
	if (it != cache.end()) return it->second;
	std::string bytes;
	std::vector<CommandInfo> v;
	if (cmdfile::ReadFileBytes(path, bytes, nullptr)) v = ParseCommands(bytes, &cmdfile::Cp932ToUtf8);
	return cache[path] = std::move(v);
}

std::string PatternLabel(const FrameData* fd, int p)
{
	if (!fd || p < 0) return std::to_string(p);
	auto* seq = const_cast<FrameData*>(fd)->get_sequence(p);
	return seq && !seq->name.empty() ? std::to_string(p) + "  " + std::string(seq->name.c_str()) : std::to_string(p);
}

// A pattern picker over an open character (read-only HA6), falling back to a number box. True when v changed.
bool PatternPicker(const FrameData* fd, int32_t& v, int lo, int hi, float width)
{
	bool changed = false;
	ImGui::SetNextItemWidth(width);
	if (fd && const_cast<FrameData*>(fd)->get_sequence_count() > 0) {
		if (ImGui::BeginCombo("##pat", v == 0 && lo == 0 ? "0  (unset / table)" : PatternLabel(fd, v).c_str(), ImGuiComboFlags_HeightLarge)) {
			if (lo <= 0 && ImGui::Selectable("0  (unset / table)", v == 0)) { v = 0; changed = true; }
			const int n = std::min<int>((int)const_cast<FrameData*>(fd)->get_sequence_count(), hi + 1);
			for (int p = 1; p < n; ++p) {
				auto* seq = const_cast<FrameData*>(fd)->get_sequence(p);
				if (!seq || seq->frames.empty()) continue;
				if (ImGui::Selectable(PatternLabel(fd, p).c_str(), v == p)) { v = p; changed = true; }
				if (v == p) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	} else {
		changed = ImGui::InputInt("##pat", &v, 1, 10);
		v = std::clamp<int32_t>(v, lo, hi);
	}
	return changed;
}

struct CharView {
	std::string file;
	int pickMoon = 0;       // the setup's moon for it
	int chara = -1;
	int slot = -1;          // engine slot (first)
	std::string label;
};

std::vector<CharView> SetupChars()
{
	AuthoringState& a = St();
	std::vector<CharView> v;
	for (int slot : { 0, 2, 1, 3 }) {
		const SlotPick& p = a.setup.slot[slot];
		if (p.Empty() || (a.setup.mode == Mode::Versus && slot >= 2)) continue;
		const RosterChar* c = RosterFor(p.chara);
		if (!c) continue;
		bool dup = false;
		for (const CharView& x : v) dup = dup || (x.file == c->file1 && x.pickMoon == p.moon);
		if (dup) continue;
		CharView cv;
		cv.file = c->file1;
		cv.pickMoon = p.moon;
		cv.chara = c->chara;
		cv.slot = slot;
		cv.label = c->name + " - " + SlotRoleName(slot) + " (" + MoonShort(p.moon) + ")";
		v.push_back(cv);
	}
	return v;
}

// The game's resolved values for (file, moon), if a slot plays it.
const wire::TuningSlot* GameSlot(const gamelink::Snapshot& s, const std::string& file, int moon)
{
	if (!s.haveTuning) return nullptr;
	for (int k = 0; k < 4; ++k)
		if (s.haveTuningSlot[k] && s.tuningSlot[k].exists && file == s.tuningSlot[k].file && s.tuningSlot[k].moon == moon) return &s.tuningSlot[k];
	return nullptr;
}

// The game column of a row: "ok" when the game resolved the same, else its value and the likely cause.
void GameCell(const gamelink::Snapshot& s, int lever, int32_t mine, const int32_t* game, const uint32_t* cssMask)
{
	const Lever& l = kLevers[lever];
	if (!game) { ImGui::TextDisabled("-"); return; }
	if (s.haveTuning && wire::MaskBit(s.tuning.envMask, lever)) {
		ImGui::TextColored(kColWarn, "env %s", FormatLeverValue(l, *game).c_str());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("PCHOST_MBAACC_TAG_%s overrides this in the game: edits here have no effect there", l.key);
		return;
	}
	if (cssMask && wire::MaskBit(cssMask, lever)) {
		ImGui::TextColored(kColOver, "CSS %s", FormatLeverValue(l, *game).c_str());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("the setup's assist choice for this partner replaces the tuning's action (a match setting)");
		return;
	}
	if (*game == mine) { ImGui::TextColored(kColOk, "game ok"); return; }
	ImGui::TextColored(kColBad, "game %s", FormatLeverValue(l, *game).c_str());
	if (ImGui::IsItemHovered()) {
		std::string why = "The game resolved another value. Likely: ";
		if (l.scope == LeverScope::SimBoot) why += "parkX is boot-time (restart the game)";
		else if (s.haveTuning && (s.tuning.flags & wire::kTunFlagHotReloadPaused)) why += "hot reload paused in F3";
		else if (s.haveTuning && (s.tuning.flags & wire::kTunFlagReadError)) why += "the game could not read a file (kept the previous values)";
		else if (s.haveTuning && s.tuning.source == (uint8_t)wire::TuningSource::Adopted) why += "a session: the host's tuning";
		else if (s.haveTuning && s.tuning.source == (uint8_t)wire::TuningSource::Legacy) why += "the game reads tag_tuning.ini (no sidecar support / no tree)";
		else why += "not re-read yet, or a stale pchost.dll";
		ImGui::SetTooltip("%s", why.c_str());
	}
}

void MbacCell(int lever, EditSink& sink, TagIni& doc, DocKind kind, const std::string& label)
{
	const MbacGlobalRef* m = FindMbacGlobal(kLevers[lever].key);
	if (!m) return;
	ImGui::SameLine();
	ImGui::TextColored(kColMbac, "MBAC %s", FormatLeverValue(kLevers[lever], m->value).c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", m->source);
	ImGui::SameLine();
	ImGui::BeginDisabled(!sink.CanEdit());
	if (ImGui::SmallButton("use MBAC")) {
		sink.Begin(std::string(kLevers[lever].key) + " = MBAC (" + label + ")");
		SetDocLever(doc, kind, lever, m->value);
		sink.Edited();
		sink.End();
	}
	ImGui::EndDisabled();
}

// One lever row on a document: [x] override here | value | provenance | game | MBAC | promote.
void LeverRow(int i, SidecarDoc& doc, const Values& resolved, const std::array<Provenance, kLeverCount>& from, const std::string& label,
              const gamelink::Snapshot& s, const int32_t* game, const uint32_t* cssMask)
{
	AuthoringState& a = St();
	EditSink& sink = Sink();
	const Lever& l = kLevers[i];
	ImGui::PushID(i);
	int32_t here = 0;
	const bool has = DocHasLever(doc.ini, doc.id.kind, i, &here);
	bool on = has;
	ImGui::BeginDisabled(!sink.CanEdit());
	if (ImGui::Checkbox("##ov", &on)) {
		sink.Begin(std::string(l.key) + (on ? " override (" : " inherit (") + label + ")");
		if (on) SetDocLever(doc.ini, doc.id.kind, i, resolved[i]);
		else ClearDocLever(doc.ini, doc.id.kind, i);
		sink.Edited();
		sink.End();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("ticked = %s sets this lever; unticked = inherited", doc.id.Label().c_str());
	ImGui::SameLine();
	int32_t v = has ? here : resolved[i];
	if (!has) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
	const bool changed = tagui::ValueWidget(l, v, 190);
	if (ImGui::IsItemActivated()) sink.Begin(std::string(l.key) + " (" + label + ")");
	if (changed) { SetDocLever(doc.ini, doc.id.kind, i, v); sink.Edited(); }
	if (ImGui::IsItemDeactivated() || (changed && !ImGui::IsItemActive())) sink.End();
	if (!has) ImGui::PopStyleVar();
	ImGui::EndDisabled();
	tagui::LeverTooltip(l, resolved[i], "resolved");
	ImGui::SameLine(250);
	ImGui::TextColored(has ? kColOver : kColDim, "%s", l.key);
	tagui::LeverTooltip(l, resolved[i], "resolved");
	ImGui::SameLine(450);
	ImGui::TextColored(ProvColor(from[i]), "%s", ProvText(from[i]).c_str());
	ImGui::SameLine(580);
	GameCell(s, i, resolved[i], game, cssMask);
	MbacCell(i, sink, doc.ini, doc.id.kind, label);
	if (has && doc.id.layer == tagtune::Layer::Local) {
		ImGui::SameLine();
		ImGui::BeginDisabled(!sink.CanEdit());
		if (ImGui::SmallButton("promote")) {
			sink.Begin(std::string("promote ") + l.key + " (" + label + ")");
			std::string err;
			a.ws.PromoteLever(doc.id, i, &err);
			sink.Edited();
			sink.End();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Promote to defaults: move this value into the shipped file");
	}
	if (l.scope == LeverScope::SimBoot) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "boot-time: restart the game"); }
	ImGui::PopID();
}

// ---- the tree bar ----
void TreeBar(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	(void)host;
	EditSink& sink = Sink();
	if (!a.ws.IsOpen()) { ImGui::TextColored(kColWarn, "Set the game folder (header) to edit its povertycaster\\tag\\ tree."); return; }
	ImGui::Text("%s", a.ws.Root().c_str());
	ImGui::SameLine();
	if (a.ws.Exists()) ImGui::TextColored(kColOk, "sidecars");
	else ImGui::TextColored(kColWarn, "no sidecar files yet");
	if (!a.tagRootOverride.empty()) { ImGui::SameLine(); ImGui::TextDisabled("(override)"); }
	ImGui::SameLine();
	ImGui::TextDisabled("editing: %s", a.editShipped ? "SHIPPED defaults" : "local overlay");
	// migrate: a legacy tag_tuning.ini and no local tree
	const std::string legacy = a.gameDir.empty() ? std::string() : a.gameDir + "\\tag_tuning.ini";
	bool localExists = false;
	for (const SidecarDoc* d : a.ws.Docs()) localExists |= d->id.layer == tagtune::Layer::Local && d->onDisk && d->id.kind != DocKind::Hud;
	if (FileExists(legacy) && !localExists) {
		ImGui::TextColored(kColWarn, "tag_tuning.ini found and no local overlay:");
		ImGui::SameLine();
		static std::string migrateLog;
		if (ImGui::SmallButton("Migrate (dry run)")) migrateLog = MigrateGameDir(a.gameDir, true, NowText()).log;
		ImGui::SameLine();
		ImGui::BeginDisabled(!p.canWriteFiles);
		if (ImGui::SmallButton("Migrate tag_tuning.ini -> local\\")) {
			SYSTEMTIME st; GetLocalTime(&st);
			char date[32];
			std::snprintf(date, sizeof date, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
			const MigrationResult r = MigrateGameDir(a.gameDir, false, date);
			migrateLog = r.log;
			if (r.wrote) {
				a.wsRoot.clear();
				EnsureWorkspace();
				if (p.canApplyTuning) Link().ApplyTuning(wire::kFlagQueryAfter);
			}
		}
		ImGui::EndDisabled();
		if (!migrateLog.empty()) ImGui::TextWrapped("%s", migrateLog.c_str());
	}
	// dirty files + external changes
	std::string dirty;
	for (const SidecarDoc* d : a.ws.Docs()) {
		if (d->ini.IsDirty()) dirty += (dirty.empty() ? "" : ", ") + d->id.Label();
		if (d->externalChange) {
			ImGui::TextColored(kColWarn, "%s changed on disk while you had edits:", d->id.Label().c_str());
			ImGui::SameLine();
			const DocId id = d->id;
			if (ImGui::SmallButton(("Load theirs##" + id.Label()).c_str())) a.ws.ResolveExternal(id, true);
			ImGui::SameLine();
			if (ImGui::SmallButton(("Keep mine##" + id.Label()).c_str())) a.ws.ResolveExternal(id, false);
		}
	}
	ImGui::BeginDisabled(!a.ws.AnyDirty() || !p.canWriteFiles);
	if (ImGui::Button("Save all")) SaveAndApply("save");
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(130);
	if (ImGui::BeginCombo("##revert", "Revert file", ImGuiComboFlags_WidthFitPreview)) {
		for (const SidecarDoc* d : a.ws.Docs())
			if (d->ini.IsDirty() && ImGui::Selectable(d->id.Label().c_str())) a.ws.Revert(d->id);
		if (a.ws.DirtyDocs().empty()) ImGui::TextDisabled("nothing unsaved");
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::BeginCombo("##checkpoint", "Revert to checkpoint", ImGuiComboFlags_WidthFitPreview)) {
		for (int k = (int)a.checkpoints.size() - 1; k >= 0; --k) {
			const Checkpoint& c = a.checkpoints[k];
			if (ImGui::Selectable((c.time + "  " + c.label + "##cp" + std::to_string(k)).c_str()) && sink.CanEdit()) {
				sink.Begin("revert to checkpoint " + c.time);
				a.ws.Restore(c.snap);
				sink.Edited();
				sink.End();
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Every sidecar file's bytes at window open and at each Load in game / Launch:\nundo a whole experiment in one step (itself undoable).");
	ImGui::SameLine();
	ImGui::BeginDisabled(!a.history.CanUndo());
	if (ImGui::Button("Undo")) UndoRedo(false);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && a.history.CanUndo()) ImGui::SetTooltip("Undo %s (Ctrl+Z)", a.history.UndoLabel().c_str());
	ImGui::SameLine();
	ImGui::BeginDisabled(!a.history.CanRedo());
	if (ImGui::Button("Redo")) UndoRedo(true);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && a.history.CanRedo()) ImGui::SetTooltip("Redo %s (Ctrl+Y)", a.history.RedoLabel().c_str());
	ImGui::SameLine();
	ImGui::Checkbox("Live apply", &a.liveApply);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("On: every edit is saved at once and the game re-reads the files (ApplyTuning).");
	if (!dirty.empty()) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "unsaved: %s", dirty.c_str()); }
	// A / B
	ImGui::TextUnformatted("A/B");
	for (int k = 0; k < 2; ++k) {
		ImGui::SameLine();
		ImGui::PushID(k);
		const bool active = a.abActive == k;
		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.2f, 1.0f));
		const std::string name = std::string(k ? "B" : "A") + (a.ab[k].set ? ": " + a.ab[k].label : ": (empty)");
		ImGui::BeginDisabled(!a.ab[k].set || !sink.CanEdit());
		if (ImGui::Button(name.c_str())) {
			sink.Begin(std::string("A/B: load ") + (k ? "B" : "A"));
			a.ws.Restore(a.ab[k].snap);
			a.abActive = k;
			sink.Edited();
			sink.End();
		}
		ImGui::EndDisabled();
		if (active) ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::SmallButton(k ? "take B" : "take A")) {
			a.ab[k] = { true, NowText(), NowText(), a.ws.Take() };
			a.ab[k].label = std::string(k ? "B" : "A") + " @ " + a.ab[k].time;
			a.abActive = k;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remember every sidecar file as snapshot %s", k ? "B" : "A");
		ImGui::PopID();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!(a.ab[0].set && a.ab[1].set) || !sink.CanEdit());
	if (ImGui::Button("Swap A <-> B")) {
		const int to = a.abActive == 0 ? 1 : 0;
		sink.Begin(std::string("A/B: swap to ") + (to ? "B" : "A"));
		a.ws.Restore(a.ab[to].snap);
		a.abActive = to;
		sink.Edited();
		sink.End();
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Write the other snapshot's files and re-read them in the game (one undo step)");
	// the game's view of the same files
	if (s.haveTuning) {
		const GlobalResolution g = a.ws.Global();
		int diffs = 0;
		for (size_t i = 0; i < kLeverCount; ++i)
			if (kLevers[i].scope != LeverScope::CharOnly && kLevers[i].scope != LeverScope::Harness && s.tuning.values[i] != g.values[i] &&
			    !wire::MaskBit(s.tuning.envMask, (int)i)) ++diffs;
		for (int k = 0; k < 4; ++k) {
			if (!s.haveTuningSlot[k] || !s.tuningSlot[k].exists) continue;
			const wire::TuningSlot& t = s.tuningSlot[k];
			const SlotResolution r = a.ws.Slot(t.file, t.moon);
			for (size_t i = 0; i < kLeverCount; ++i)
				if (t.values[i] != r.values[i] && !wire::MaskBit(t.cssMask, (int)i) && !wire::MaskBit(s.tuning.envMask, (int)i) &&
				    kLevers[i].scope != LeverScope::Harness) ++diffs;
		}
		ImGui::SameLine(0, 30);
		if (!diffs) ImGui::TextColored(kColOk, "game: sha %s  matches these files", s.tuning.sha);
		else {
			ImGui::TextColored(kColBad, "game differs (%d lever values)", diffs);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("See the Live tab's grid (red cells) for which, and the likely cause.");
		}
		if (!a.lastApply.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", a.lastApply.c_str()); }
	}
	// warnings
	const std::vector<Warning> w = AllSidecarWarnings(a.ws.Set());
	if (!w.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, kColWarn);
		const bool open = ImGui::TreeNode("warnings", "%zu warning(s) the game logs (their keys are skipped)", w.size());
		ImGui::PopStyleColor();
		if (open) { for (const Warning& x : w) ImGui::BulletText("%s", x.Text().c_str()); ImGui::TreePop(); }
	}
	// the answer to the last ApplyTuning
	if (a.applySeq) {
		wire::Reply r{};
		if (Link().PeekReply(a.applySeq, r)) {
			a.lastApply = std::string("apply: ") + wire::StatusName(r.status) + " - " + r.message;
			a.applySeq = 0;
		}
	}
}

// ---- Global & styles ----
void GlobalView(const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	EditSink& sink = Sink();
	SidecarDoc& doc = a.ws.Doc(GlobalDoc(TargetLayer()));
	const GlobalResolution g = a.ws.Global();
	static bool dropOverrides = false;
	static char newStyle[48] = "";
	ImGui::SetNextItemWidth(200);
	ImGui::BeginDisabled(!sink.CanEdit());
	if (ImGui::BeginCombo("Style", g.style.empty() ? "(none: defaults)" : g.style.c_str())) {
		for (const std::string& n : SidecarStyleNames(a.ws.Set())) {
			const BuiltinStyle* b = FindBuiltinStyle(n);
			if (ImGui::Selectable(n.c_str(), ieq(n, g.style))) {
				sink.Begin("style " + n + (dropOverrides ? " (drops [tuning] overrides)" : ""));
				SelectStyle(doc.ini, n, dropOverrides);
				a.setup.style = n;
				sink.Edited();
				sink.End();
			}
			if (b && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b->desc);
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox("picking a style drops [tuning] overrides", &dropOverrides);
	ImGui::SetNextItemWidth(160);
	ImGui::InputTextWithHint("##newstyle", "new style name", newStyle, sizeof newStyle);
	ImGui::SameLine();
	const bool nameOk = newStyle[0] && !std::strpbrk(newStyle, "[]=;# \t");
	ImGui::BeginDisabled(!nameOk);
	if (ImGui::Button("Save as style")) {
		sink.Begin(std::string("save as style ") + newStyle);
		SaveAsStyle(doc.ini, newStyle, g.values);
		sink.Edited();
		sink.End();
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	if (!g.styleFound) ImGui::TextColored(kColWarn, "active_style '%s' is unknown: the game uses the defaults", g.styleRequested.c_str());
	ImGui::TextDisabled("ticked = %s sets it in [tuning]. Colour = where the value comes from. Hover a lever for the guide.", doc.id.Label().c_str());
	const char* group = nullptr;
	for (size_t i = 0; i < kLeverCount; ++i) {
		const Lever& l = kLevers[i];
		if (l.scope == LeverScope::CharOnly) continue;
		if (!group || std::strcmp(group, l.group)) { group = l.group; ImGui::SeparatorText(group); }
		LeverRow((int)i, doc, g.values, g.from, "global", s, s.haveTuning ? &s.tuning.values[i] : nullptr, nullptr);
	}
}

// ---- one character + moon ----
void CharacterView(HostContext& host, const gamelink::Snapshot& s, const CharView& cv)
{
	AuthoringState& a = St();
	EditSink& sink = Sink();
	int layer = a.layer == -2 ? cv.pickMoon : a.layer;
	ImGui::TextUnformatted("Layer");
	ImGui::SameLine();
	if (ImGui::RadioButton("All moons (chars\\<file>.ini)", layer == -1)) a.layer = layer = -1;
	for (int m = 0; m < 3; ++m) {
		ImGui::SameLine();
		std::string l = MoonText(m) + (m == cv.pickMoon ? " *" : "");
		if (ImGui::RadioButton(l.c_str(), layer == m)) a.layer = layer = m;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(* = the setup's moon)");
	const int viewMoon = layer >= 0 ? layer : cv.pickMoon;
	SidecarDoc& doc = a.ws.Doc(CharDoc(TargetLayer(), cv.file, layer));
	const SlotResolution r = a.ws.Slot(cv.file, viewMoon);
	const std::string txt = DataTxt(cv.file, viewMoon);
	const FrameData* fd = host.frameDataFor ? host.frameDataFor(txt) : nullptr;
	const wire::TuningSlot* gs = GameSlot(s, cv.file, viewMoon);
	const std::string label = cv.file + (layer >= 0 ? std::string(", ") + MoonName(layer) : std::string(", all moons"));
	ImGui::Text("%s  -  editing %s%s", CharLabel(cv.file).c_str(), doc.id.Label().c_str(), doc.onDisk ? "" : "  (created on the first edit)");
	ImGui::SameLine();
	if (fd) ImGui::TextColored(kColOk, "%s_%d.txt open", cv.file.c_str(), viewMoon);
	else {
		ImGui::TextColored(kColWarn, "%s_%d.txt not open", cv.file.c_str(), viewMoon);
		ImGui::SameLine();
		if (ImGui::SmallButton("open") && host.openCharacter) host.openCharacter(txt, false);
	}
	if (!gs && s.haveTuning) { ImGui::SameLine(); ImGui::TextDisabled("(no game slot plays %s %s)", cv.file.c_str(), MoonShort(viewMoon)); }
	if (layer == -1) ImGui::TextDisabled("All moons: the %s file below wins over these values where it sets them.", MoonText(viewMoon).c_str());

	// ---- tag-in / tag-out ----
	ImGui::SeparatorText("Tag-in / tag-out");
	const TeamChangeRow* row = FindTeamChangeRow(cv.file);
	const MbacCharRef& mref = MbacReferenceFor(a.mbacDir, cv.chara);
	for (const char* key : { "tagIn", "tagOut" }) {
		const int i = FindLever(key);
		const bool in = !std::strcmp(key, "tagIn");
		ImGui::PushID(key);
		int32_t here = 0;
		const bool has = DocHasLever(doc.ini, doc.id.kind, i, &here);
		bool on = has;
		ImGui::BeginDisabled(!sink.CanEdit());
		if (ImGui::Checkbox("##ov", &on)) {
			sink.Begin(std::string(key) + (on ? " override (" : " inherit (") + label + ")");
			if (on) SetDocLever(doc.ini, doc.id.kind, i, r.values[i] ? r.values[i] : (row ? (in ? row->tagIn : row->tagOut) : 0));
			else ClearDocLever(doc.ini, doc.id.kind, i);
			sink.Edited();
			sink.End();
		}
		ImGui::SameLine();
		int32_t v = has ? here : r.values[i];
		if (!has) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
		if (PatternPicker(fd, v, 0, 999, 240)) { sink.Begin(std::string(key) + " (" + label + ")"); SetDocLever(doc.ini, doc.id.kind, i, v); sink.Edited(); sink.End(); }
		if (!has) ImGui::PopStyleVar();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextColored(has ? kColOver : kColDim, "%s", key);
		ImGui::SameLine();
		ImGui::TextColored(ProvColor(r.from[i]), "%s", ProvText(r.from[i]).c_str());
		const int table = row ? (in ? row->tagIn : row->tagOut) : 0;
		const int effective = r.values[i] ? r.values[i] : table;
		ImGui::SameLine();
		ImGui::TextDisabled("table default %d", table);
		ImGui::SameLine();
		if (ImGui::SmallButton("jump") && host.showPattern) host.showPattern(txt, effective, 0, true);
		ImGui::SameLine();
		GameCell(s, i, r.values[i], gs ? &gs->values[i] : nullptr, gs ? gs->cssMask : nullptr);
		DrawMoveInline(host, txt, effective, in ? "tag-in" : "tag-out");
		if (mref.ok) {
			const int mv = in ? mref.tagIn : mref.tagOut;
			const MoveSummary& ms = in ? mref.in : mref.out;
			ImGui::TextColored(kColMbac, "   MBAC %s %d%s%s", key, mv, ms.valid ? ": " : "", ms.valid ? ms.Text().c_str() : "");
			ImGui::SameLine();
			ImGui::BeginDisabled(!sink.CanEdit() || mv <= 0);
			if (ImGui::SmallButton("use MBAC value")) {
				sink.Begin(std::string(key) + " = MBAC " + std::to_string(mv) + " (" + label + ")");
				SetDocLever(doc.ini, doc.id.kind, i, mv);
				sink.Edited();
				sink.End();
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("MBAC's [TeamChangeData] number for %s. The MBAACC HA6 pattern of that number is what plays:\ncompare the two frame-data lines.", mref.stem.c_str());
		} else if (!mref.error.empty()) {
			ImGui::TextDisabled("   MBAC: %s", mref.error.c_str());
		}
		ImGui::PopID();
	}

	// ---- the assists ----
	ImGui::SeparatorText("Assists (FN1 + direction)");
	const std::string ctxt = CommandsTxt(cv.file, viewMoon);
	const std::vector<CommandInfo>& cmds = CommandsOf(ctxt);
	ImGui::TextDisabled("commands from %s_%d_c.txt (%zu, read-only)%s", cv.file.c_str(), viewMoon, cmds.size(),
	                    layer == -1 ? " - command ids may differ per moon" : "");
	if (ImGui::BeginTable("##assists", 5, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
		for (int slot = 0; slot < 5; ++slot) {
			const SlotLevers sl = SlotLeverIndices(slot);
			ImGui::PushID(slot);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d+FN1", kAssistDirs[slot]);
			// entry
			ImGui::TableNextColumn();
			{
				int32_t ev = 0;
				const bool hasE = DocHasLever(doc.ini, doc.id.kind, sl.entry, &ev);
				if (!hasE) ev = r.values[sl.entry];
				ImGui::BeginDisabled(!sink.CanEdit());
				if (tagui::ValueWidget(kLevers[sl.entry], ev, 90)) {
					sink.Begin(std::string(kLevers[sl.entry].key) + " (" + label + ")");
					SetDocLever(doc.ini, doc.id.kind, sl.entry, ev);
					sink.Edited();
					sink.End();
				}
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("entry: %s", ProvText(r.from[sl.entry]).c_str());
			}
			// mode in THIS layer
			ImGui::TableNextColumn();
			int32_t pv = 0, cv2 = -1, mv = 0;
			const bool hp = DocHasLever(doc.ini, doc.id.kind, sl.pattern, &pv) && pv > 0;
			const bool hc = DocHasLever(doc.ini, doc.id.kind, sl.command, &cv2) && cv2 >= 0;
			const bool hm = DocHasLever(doc.ini, doc.id.kind, sl.motion, &mv) && mv != 0;
			int mode = hp ? 1 : hc ? 2 : hm ? 3 : 0;
			const char* modes[] = { "inherit", "pattern", "command", "motion" };
			ImGui::SetNextItemWidth(90);
			ImGui::BeginDisabled(!sink.CanEdit());
			if (ImGui::Combo("##mode", &mode, modes, 4)) {
				sink.Begin(std::string("assist ") + std::to_string(kAssistDirs[slot]) + " " + modes[mode] + " (" + label + ")");
				ClearDocLever(doc.ini, doc.id.kind, sl.pattern);
				ClearDocLever(doc.ini, doc.id.kind, sl.command);
				ClearDocLever(doc.ini, doc.id.kind, sl.motion);
				const AssistAction cur = ResolveAssistSlot(r.values, slot);
				const ActionView av = DescribeAction(cur, cmds);
				if (mode == 1) SetDocLever(doc.ini, doc.id.kind, sl.pattern, av.pattern > 0 ? av.pattern : 1);
				if (mode == 2) {
					const int d = PickDefaultCommand(cmds);
					SetDocLever(doc.ini, doc.id.kind, sl.command, cur.mode == AssistMode::Command ? cur.value : d >= 0 ? cmds[d].id : 0);
				}
				if (mode == 3) { int32_t p = 0; PackMotion("236A", p); SetDocLever(doc.ini, doc.id.kind, sl.motion, p); }
				sink.Edited();
				sink.End();
			}
			ImGui::EndDisabled();
			// the value
			ImGui::TableNextColumn();
			ImGui::BeginDisabled(!sink.CanEdit());
			if (mode == 1) {
				if (PatternPicker(fd, pv, 1, 999, 220)) { sink.Begin(std::string(kLevers[sl.pattern].key) + " (" + label + ")"); SetDocLever(doc.ini, doc.id.kind, sl.pattern, pv); sink.Edited(); sink.End(); }
			} else if (mode == 2) {
				ImGui::SetNextItemWidth(220);
				const CommandInfo* cur = FindCommand(cmds, cv2);
				const std::string prev = cur ? std::to_string(cur->id) + "  " + cur->input + " -> p" + std::to_string(cur->pattern) : std::to_string(cv2);
				if (ImGui::BeginCombo("##cmd", prev.c_str(), ImGuiComboFlags_HeightLarge)) {
					for (const CommandInfo& c : cmds) {
						std::string l = std::to_string(c.id) + "  " + c.input + " -> p" + std::to_string(c.pattern) + (c.meter ? " $" + std::to_string(c.meter) : "") + "  " + c.name;
						if (ImGui::Selectable(l.c_str(), c.id == cv2)) { sink.Begin(std::string(kLevers[sl.command].key) + " (" + label + ")"); SetDocLever(doc.ini, doc.id.kind, sl.command, c.id); sink.Edited(); sink.End(); }
					}
					if (cmds.empty()) ImGui::TextDisabled("no %s", ctxt.c_str());
					ImGui::EndCombo();
				}
			} else if (mode == 3) {
				char buf[16];
				std::snprintf(buf, sizeof buf, "%s", UnpackMotion(mv).c_str());
				ImGui::SetNextItemWidth(90);
				if (ImGui::InputText("##motion", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
					int32_t p = 0;
					if (ValidateMotion(buf).empty() && PackMotion(buf, p)) { sink.Begin(std::string(kLevers[sl.motion].key) + " (" + label + ")"); SetDocLever(doc.ini, doc.id.kind, sl.motion, p); sink.Edited(); sink.End(); }
					else a.status = std::string("motion '") + buf + "': " + ValidateMotion(buf);
				}
			} else {
				ImGui::TextDisabled("inherits");
			}
			ImGui::EndDisabled();
			// resolved
			ImGui::TableNextColumn();
			const AssistAction act = ResolveAssistSlot(r.values, slot);
			const ActionView av = DescribeAction(act, cmds);
			ImGui::TextColored(av.problem ? kColBad : kColDim, "%s -> %s entry", av.text.c_str(), kAssistEntryNames[act.entry >= 0 && act.entry < 4 ? act.entry : 0]);
			if (av.pattern > 0) {
				ImGui::SameLine();
				if (ImGui::SmallButton("jump") && host.showPattern) host.showPattern(txt, av.pattern, 0, true);
				ImGui::SameLine();
				if (fd) {
					const MoveSummary ms = SummarizePattern(*fd, av.pattern);
					ImGui::TextDisabled("%s", ms.valid ? ms.Text().c_str() : "(pattern not in the open HA6)");
				}
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (gs && (gs->flags & wire::kTunSlotCssAssists)) ImGui::TextColored(kColOver, "the setup's assist choices replace some of these for this partner (CSS)");

	// ---- every other perChar lever ----
	const char* group = nullptr;
	for (size_t i = 0; i < kLeverCount; ++i) {
		const Lever& l = kLevers[i];
		if (!l.perChar || tagui::IsSlotAction(l) || tagui::IsSlotEntry(l) || !std::strcmp(l.key, "tagIn") || !std::strcmp(l.key, "tagOut")) continue;
		if (!group || std::strcmp(group, l.group)) { group = l.group; ImGui::SeparatorText(group); }
		LeverRow((int)i, doc, r.values, r.from, label, s, gs ? &gs->values[i] : nullptr, gs ? gs->cssMask : nullptr);
	}
}

} // namespace

// ================== the inline frame-data strip ==================

void DrawMoveInline(HostContext& host, const std::string& txtPath, int pattern, const char* label)
{
	const FrameData* fd = host.frameDataFor ? host.frameDataFor(txtPath) : nullptr;
	if (!fd) { ImGui::TextDisabled("   %s p%d: open the character to see its frame data", label, pattern); return; }
	static std::map<std::string, MoveSummary> cache;
	const std::string key = txtPath + "|" + std::to_string(pattern) + "|" + std::to_string(fd->dataVersion);
	auto it = cache.find(key);
	if (it == cache.end()) { if (cache.size() > 256) cache.clear(); it = cache.emplace(key, SummarizePattern(*fd, pattern)).first; }
	const MoveSummary& m = it->second;
	if (!m.valid) { ImGui::TextColored(kColBad, "   %s p%d: not in the loaded HA6", label, pattern); return; }
	ImGui::TextDisabled("   %s p%d %s: %s", label, pattern, m.name.c_str(), m.Text().c_str());
	// the strip: one cell per tick (scaled to fit), startup grey, active red, recovery blue, invulnerable outlined yellow
	const float h = ImGui::GetTextLineHeight() * 0.7f;
	const float avail = std::max(120.0f, ImGui::GetContentRegionAvail().x - 40.0f);
	const int ticks = std::max(1, m.totalTicks);
	const float w = std::min(8.0f, avail / (float)ticks);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 o = ImVec2(ImGui::GetCursorScreenPos().x + 24, ImGui::GetCursorScreenPos().y);
	int firstActive = m.startup, lastActive = m.startup + m.active - 1;
	for (const FrameStrip& f : m.strip)
		for (int t = f.startTick; t < f.startTick + f.duration && t < ticks; ++t) {
			const bool act = f.attack;
			ImU32 col = act ? IM_COL32(220, 60, 60, 255) : (m.startup >= 0 && t < firstActive) ? IM_COL32(120, 140, 120, 255)
			          : (m.startup >= 0 && t > lastActive) ? IM_COL32(70, 110, 200, 255) : IM_COL32(110, 110, 110, 255);
			const ImVec2 a0(o.x + t * w, o.y), a1(o.x + (t + 1) * w - (w > 3 ? 1 : 0), o.y + h);
			dl->AddRectFilled(a0, a1, col);
			if (f.invuln) dl->AddRect(a0, a1, IM_COL32(240, 220, 60, 255));
		}
	ImGui::Dummy(ImVec2(24 + ticks * w, h + 2));
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("%s p%d: %d frames, %d ticks%s", label, pattern, m.frames, m.totalTicks, m.loops ? " (loops)" : "");
		if (!m.note.empty()) ImGui::TextUnformatted(m.note.c_str());
		ImGui::TextDisabled("grey-green startup, red active, blue recovery, yellow outline = strike invulnerable");
		ImGui::EndTooltip();
	}
}

// ================== the tab ==================

void DrawTuningTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	TreeBar(host, s, p);
	if (!a.ws.IsOpen()) return;
	ImGui::Separator();
	const std::vector<CharView> chars = SetupChars();
	if (a.tuningView.empty()) a.tuningView = "global";
	if (ImGui::BeginTabBar("##tuningviews", ImGuiTabBarFlags_FittingPolicyScroll)) {
		auto tab = [&](const std::string& id, const std::string& label) {
			const ImGuiTabItemFlags fl = a.forceViewFrames > 0 && a.tuningView == id ? ImGuiTabItemFlags_SetSelected : 0;
			const bool open = ImGui::BeginTabItem((label + "###" + id).c_str(), nullptr, fl);
			if (open && a.forceViewFrames <= 0) a.tuningView = id;
			return open;
		};
		if (tab("global", "Global & styles")) { GlobalView(s); ImGui::EndTabItem(); }
		for (const CharView& cv : chars) {
			const std::string id = cv.file + ":" + std::to_string(cv.pickMoon);
			if (tab(id, cv.label)) { CharacterView(host, s, cv); ImGui::EndTabItem(); }
		}
		// Other: a character file in the tree that the setup does not pick, or any roster character
		const std::string other = a.tuningView.rfind("other:", 0) == 0 ? a.tuningView.substr(6) : std::string();
		if (!other.empty()) {
			CharView cv;
			const size_t c = other.find(':');
			cv.file = other.substr(0, c);
			cv.pickMoon = c == std::string::npos ? 0 : std::atoi(other.substr(c + 1).c_str());
			const RosterChar* rc = FindFile(a.roster, cv.file);
			cv.chara = rc ? rc->chara : -1;
			cv.label = (rc ? rc->name : cv.file) + " (other)";
			if (tab(a.tuningView, cv.label)) { CharacterView(host, s, cv); ImGui::EndTabItem(); }
		}
		if (ImGui::TabItemButton("Other...", ImGuiTabItemFlags_Trailing)) ImGui::OpenPopup("##otherchar");
		if (ImGui::BeginPopup("##otherchar")) {
			ImGui::TextDisabled("character files in the tree");
			for (const std::string& f : a.ws.CharFiles())
				if (ImGui::Selectable(CharLabel(f).c_str())) { a.tuningView = "other:" + f + ":0"; a.forceViewFrames = 2; }
			ImGui::Separator();
			ImGui::TextDisabled("any character (a file is created on the first edit)");
			for (const RosterChar& rc : a.roster)
				if (!rc.Duo() && ImGui::Selectable((rc.name + "##r" + std::to_string(rc.chara)).c_str())) { a.tuningView = "other:" + rc.file1 + ":0"; a.forceViewFrames = 2; }
			ImGui::EndPopup();
		}
		ImGui::EndTabBar();
	}
	if (a.forceViewFrames > 0) --a.forceViewFrames;
}

} // namespace authoring
