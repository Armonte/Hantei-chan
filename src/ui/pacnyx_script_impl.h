// --ui-script commands for the PACNyx-parity features (PAT editor of RBO / GOF2 characters, CG window, export, tabs), so the
// screenshots / evidence in docs/audit_2026_10_14/evidence/pacnyx/ are produced by the app itself without a real mouse or keyboard.
// Included into main_frame.cpp like the other ui/*_impl.h files. Returns false when `cmd` is not one of these.
//   partseditor | pose N | part K | sheetview on|off | sheet N | sheetsel X Y W H | importsheet <png> | addpart
//   layer front|back|fwd|bwd|<N> | nudge DX DY | rotate DEG | basepoint DX DY | addpose NAME | undo | redo | dirty
//   cgwindow on|off | cgselect N | cgimport <png> | exporticaroffa <dir> | export <dir> | newimg <png> | closealltabs | savechar-as <path>
//   cgm on|off | cgmselect N | lang ja|en
#include "../han2_parts_edit.h"
#include <sstream>

bool MainFrame::PacnyxScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	auto* view = getActiveView();
	auto* ch = view ? view->getCharacter() : nullptr;
	auto ints = [&](int n, int* out) { std::istringstream is(arg); for (int i = 0; i < n; i++) if (!(is >> out[i])) return false; return true; };
	auto say = [&](const std::string& m) { if (msg) *msg = m; };
	auto regen = [&] {
		if (!view) return;
		if (auto* p = view->getPartSetPane()) { p->RegeneratePartSetNames(); p->RegeneratePartPropNames(); }
		if (auto* t = view->getTexturePane()) t->RegenerateTexturesNames();
	};
	auto st = [&]() -> FrameState& { return view->getState(); };
	struct Notify { CharacterInstance* c; ~Notify() { if (c) c->undoManager.markModified(); } } notify{ch};   // a scripted edit ends like a click: one undo step
	if (cmd == "partseditor") { if (!ch || !ch->parts.loaded) { say("no parts"); return true; } openPartsEditorForCharacter(ch); return true; }
	if (cmd == "closealltabs") { m_closeAllPending = true; return true; }
	if (cmd == "newimg") { say(han2ui::NewImgFromImage(arg)); return true; }
	if (cmd == "undo" || cmd == "redo") { if (view) PerformUndoRedo(view, cmd == "redo"); regen(); return true; }
	if (cmd == "dirty") { say(ch ? (ch->isModified() ? "modified" : "clean") : "no character"); return true; }
	if (cmd == "savechar-as") { say(ch && saveCharacterAs(ch, arg) ? "saved" : "FAILED"); return true; }
	if (cmd == "export" || cmd == "exporticaroffa") {
		if (!ch || !ch->frameData.isHan2()) { say("not RBO / GOF2"); return true; }
		han2::ExportOptions opt; opt.icaroffaLayout = cmd == "exporticaroffa";
		han2::ExportReport rep; std::string e;
		const bool ok = han2::ExportCharacter(ch->frameData, ch->cg, ch->parts, arg, opt, rep, &e);
		char b[256]; snprintf(b, sizeof b, "%s: %d pose png, %d frame png, %d patterns, %d write errors %s", ok ? "ok" : "FAILED", rep.posePngs, rep.framePngs, rep.patternsWritten, rep.writeErrors, e.c_str());
		say(b); return true;
	}
	if (cmd == "cgwindow") { han2ui::showCgWindow = arg != "off"; return true; }
	if (cmd == "cgimport") { say(han2ui::DropImageOnCgWindow(ch, arg)); return true; }
	if (cmd == "cgm" && (arg == "on" || arg == "off")) { m_cgm.open = arg == "on"; return true; }   // other `cgm <sub>` lines: startup_args.cpp cgmScript
	if (cmd == "cgmselect") { m_cgm.select(atoi(arg.c_str())); return true; }
	if (cmd == "lang") { i18n::language = arg == "ja" ? 1 : 0; return true; }
	static const char* const patCmds[] = { "pose", "part", "sheetview", "sheet", "sheetsel", "importsheet", "addpart", "layer", "nudge", "rotate", "basepoint", "addpose" };
	bool isPatCmd = false;
	for (const char* c : patCmds) if (cmd == c) isPatCmd = true;
	if (!isPatCmd) return false;   // other commands (S2 PAT tab, S3 edits, browser) are handled by their own dispatchers
	if (!view || !view->isPatEditor() || !ch || !ch->parts.fbPartModel) {
		if (cmd == "layer") return false;   // outside an RBO / GOF2 PAT tab `layer` is the frame layer (startup_args.cpp editCmd)
		say("needs an RBO / GOF2 PAT editor tab (partseditor)"); return true;
	}
	Parts& parts = ch->parts;
	int v[4]{};
	if (cmd == "pose") { st().partSet = atoi(arg.c_str()); st().partProp = 0; return true; }
	if (cmd == "part") { st().partProp = atoi(arg.c_str()); return true; }
	if (cmd == "sheetview") { st().renderMode = arg == "off" ? RenderMode::DEFAULT : RenderMode::TEXTURE_VIEW; return true; }
	if (cmd == "sheet") { st().partGraph = atoi(arg.c_str()); return true; }
	if (cmd == "sheetsel") { if (!ints(4, v)) return true; for (int i = 0; i < 4; i++) st().sheetSel[i] = v[i]; st().sheetSelValid = v[2] > 0 && v[3] > 0; return true; }
	if (cmd == "importsheet") { if (auto* t = view->getTexturePane()) say(t->ImportSheetFile(arg, false)); regen(); return true; }
	if (cmd == "addpart") {
		std::string e; const int slot = han2::edit::AddPartFromSheet(parts, st().partSet, st().partGraph, st().sheetSel[0], st().sheetSel[1], st().sheetSel[2], st().sheetSel[3], st().partProp, &e);
		if (slot >= 0) st().partProp = slot;
		say(slot >= 0 ? "part " + std::to_string(slot) : e); regen(); return true;
	}
	if (cmd == "addpose") { int at = -1; std::string e; say(han2::edit::AddPose(parts, arg, &at, &e) ? "pose " + std::to_string(at) : e); if (at >= 0) st().partSet = at; regen(); return true; }
	PartSet<>* ps = parts.GetPartSet(st().partSet);
	if (!ps) { say("no pose"); return true; }
	if (cmd == "basepoint") { if (ints(2, v)) han2::edit::ShiftPose(*ps, v[0], v[1]); return true; }
	if (st().partProp < 0 || st().partProp >= (int)ps->groups.size() || ps->groups[st().partProp].ppId < 0) { say("no part selected"); return true; }
	PartProperty& p = ps->groups[st().partProp];
	using han2::edit::LayerCmd;
	if (cmd == "layer") {
		const bool ok = arg == "front" ? han2::edit::LayerCommand(*ps, st().partProp, LayerCmd::ToFront) : arg == "back" ? han2::edit::LayerCommand(*ps, st().partProp, LayerCmd::ToBack)
			: arg == "fwd" ? han2::edit::LayerCommand(*ps, st().partProp, LayerCmd::Forward) : arg == "bwd" ? han2::edit::LayerCommand(*ps, st().partProp, LayerCmd::Backward)
			: han2::edit::LayerCommand(*ps, st().partProp, LayerCmd::Set, atoi(arg.c_str()));
		say(std::string(ok ? "layer " : "unchanged, layer ") + std::to_string(han2::edit::LayerOf(p))); return true;
	}
	if (cmd == "nudge") { if (ints(2, v)) { p.x += v[0]; p.y += v[1]; } return true; }
	if (cmd == "rotate") { p.rotation[3] += (float)atof(arg.c_str()) / 360.f; return true; }
	return false;
}
