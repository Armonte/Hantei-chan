#include "ha4_character.h"
#include "character_instance.h"
#include "framedata_ha4.h"
#include "ha4_parts.h"
#include "ha4_convert.h"
#include "misc.h"

#include <imgui.h>
#include <filesystem>
#include <cctype>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

namespace ha4 {

static std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

static std::string FindSibling(const fs::path &dir, const std::string &fileLower)
{
	std::error_code ec;
	for (auto &e : fs::directory_iterator(dir, ec))
		if (Lower(e.path().filename().string()) == fileLower) return e.path().string();
	return {};
}

std::string AttachCharacterResources(CharacterInstance &ch, const std::string &datPath)
{
	auto cont = ch.frameData.m_ha4;
	if (!cont) return {};
	fs::path dat(datPath);
	std::string summary;

	// CG: the bank is embedded in the .DAT (same "BMP Cutter3" format as MBAACC .cg).
	if (!cont->cg.empty() && ch.cg.loadFromMemory(cont->cg.data(), (unsigned)cont->cg.size()))
		summary += std::to_string(ch.cg.get_image_count()) + " CG images";
	else
		summary += "no CG";

	// Palette: <stem>.PAL next to the .DAT (64 x 256 colours, the MBAACC .pal format).
	std::string pal = FindSibling(dat.parent_path(), Lower(dat.stem().string()) + ".pal");
	if (!pal.empty() && ch.cg.loadPalette(pal.c_str())) {
		ch.cg.changePaletteNumber(ch.palette);
		summary += ", palette " + fs::path(pal).filename().string();
	}

	// Parts: old-format PAT blob -> Parts model (textures uploaded for rendering).
	if (!cont->parts.empty()) {
		std::string err;
		if (OldPatToParts(cont->parts.data(), cont->parts.size(), ch.parts, &err)) {
			UploadPartsTextures(ch.parts);
			summary += ", " + std::to_string(ch.parts.partSets.size()) + " part sets";
		} else {
			summary += ", parts: " + err;
		}
	}

	// Effect character: MBAC keeps shared effects in EFFECT.DAT (EF 8 targets).
	if (Lower(dat.stem().string()) != "effect" && !ch.effectCharacter) {
		std::string eff = FindSibling(dat.parent_path(), "effect.dat");
		if (!eff.empty()) {
			auto e = std::make_unique<CharacterInstance>();
			if (e->loadHA6(eff, false)) {
				e->setName("effect");
				ch.effectCharacter = std::move(e);
				summary += ", EFFECT.DAT";
			}
		}
	}
	printf("[HA4] %s: %s\n", dat.filename().string().c_str(), summary.c_str());
	return summary;
}

} // namespace ha4

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------
namespace ha4ui {

bool showInspector = true;

// hantei4.exe label tables (docs/tag_research/HANTEI4_EF_IF_LABELS.md §3)
static const char *const kFlipModes[] = {
	"0 Normal", "1 Flip H", "2 Flip V", "3 90 deg", "4 180 deg", "5 270 deg",
	"6 Flip H + 90", "7 Flip H + 270", "8 Arbitrary rotation", "9 Arbitrary rotation (R)" };
static const char *const kAniFlags[] = {
	"0 End", "1 Next", "2 Jump", "3 Next, end on landing", "4 Jump, end on landing", "5 Loop check" };
static const char *const kBlend[] = { "0 None", "1 Translucent (tool caption: multiply)", "2 Additive", "3 Subtractive" };
static const char *const kMoveType[] = {
	"0 Movement", "1 Normal", "2 Throw", "3 Special", "4 Special throw", "5 Other", "6 Super",
	"7 Super throw", "8 Super 2", "9 Super throw 2", "10 Super 3", "11 Super throw 3" };
static std::string PriorityLabel(int v)
{
	if (v == 0) return "unchanged";
	if (v == 1) return "frontmost";
	if (v == 2) return "backmost";
	if (v >= 3 && v <= 12) return "front " + std::to_string(v - 3);
	if (v >= 13 && v <= 22) return "back " + std::to_string(v - 13);
	if (v == 23) return "parent +1";
	if (v == 24) return "parent -1";
	if (v == 25) return "in front of BG";
	if (v == 26) return "frontmost +1";
	return "?";
}

static int AniIndex(const Frame &f)
{
	static const int map[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };
	for (int i = 0; i < 6; i++) if (map[i][0] == f.AF.aniType && (unsigned)map[i][1] == f.AF.aniFlag) return i;
	return -1;
}

void DrawInspector(CharacterInstance *ch, FrameState &state)
{
	if (!ch || !ch->frameData.isHA4() || !showInspector) return;
	ImGui::SetNextWindowSize(ImVec2(380, 460), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("MBAC (HA4)", &showInspector)) { ImGui::End(); return; }

	const Ha4Container &c = *ch->frameData.m_ha4;
	ImGui::TextWrapped("%s", c.sourcePath.c_str());
	ImGui::TextDisabled("parts blob %zu B, CG blob %zu B%s", c.parts.size(), c.cg.size(),
	                    ch->cg.getPalNumber() > 1 ? ", .PAL loaded" : "");
	ImGui::TextDisabled("Save writes HA4 (.DAT). Save As *.ha6 or File > Export converts.");

	bool changed = false;
	Sequence *seq = ch->frameData.get_sequence(state.pattern);
	if (seq) {
		ImGui::SeparatorText("Pattern");
		if (!seq->frames.empty()) {
			int32_t mi;
			memcpy(&mi, seq->ha4.hdr + 4, 4);
			if (!seq->ha4.valid) mi = seq->psts;
			int type = seq->psts & 0xF;
			if (ImGui::Combo("Move type (技情報)", &type, kMoveType, IM_ARRAYSIZE(kMoveType))) { seq->psts = type; changed = true; }
			bool b40 = mi & 0x40, b80 = mi & 0x80;
			bool e1 = ImGui::Checkbox("moveInfo 0x40 (ctl 1149)", &b40); ImGui::SameLine();
			bool e2 = ImGui::Checkbox("0x80 (ctl 1148)", &b80);
			if (e1 || e2) {
				mi = (mi & ~0xC0) | (b40 ? 0x40 : 0) | (b80 ? 0x80 : 0);
				if (!seq->ha4.valid) { memset(seq->ha4.hdr, 0, sizeof(seq->ha4.hdr)); seq->ha4.valid = true; }
				memcpy(seq->ha4.hdr + 4, &mi, 4);
				changed = true;
			}
			ImGui::TextDisabled("HA6: PSTS = type, PLVL = level; bits 0x40/0x80 have no HA6 tag.");
		} else {
			ImGui::TextDisabled("empty slot");
		}

		if (state.frame >= 0 && state.frame < (int)seq->frames.size()) {
			Frame &f = seq->frames[state.frame];
			ImGui::SeparatorText("Frame");
			if (!f.AF.layers.empty()) {
				Layer_Type &L = f.AF.layers[0];
				int raw = L.usePat ? L.spriteId : (L.spriteId < 0 ? L.spriteId : L.spriteId + 10000);
				ImGui::Text("Sprite id %d (%s)", raw, L.usePat ? "parts pattern" : raw >= 10000 ? "CG image" : "none");

				int mode, rot;
				ha4::EncodeFlip(L.rotation, mode, rot);
				if (ImGui::Combo("Display (表示)", &mode, kFlipModes, IM_ARRAYSIZE(kFlipModes))) {
					ha4::DecodeFlip(mode, rot, L.rotation); changed = true;
				}
				if (mode == 8 || mode == 9) {
					if (ImGui::InputInt("Angle (1/10000 turn)", &rot)) { ha4::DecodeFlip(mode, rot, L.rotation); changed = true; }
				}
				int blend = std::clamp(L.blend_mode, 0, 3);
				if (ImGui::Combo("Blend (半透明)", &blend, kBlend, IM_ARRAYSIZE(kBlend))) {
					L.blend_mode = blend;
					if (blend == 0) L.rgba[3] = 1.f;
					changed = true;
				}
				if (L.rgba[0] != 1.f || L.rgba[1] != 1.f || L.rgba[2] != 1.f)
					ImGui::TextColored(ImVec4(1, .6f, .2f, 1), "Colour tint (AFRG) is not stored in HA4");
			}
			int ai = AniIndex(f);
			if (ai < 0) ImGui::TextColored(ImVec4(1, .6f, .2f, 1), "AFF %d / AFFE %u has no exact HA4 AniFlag", f.AF.aniType, f.AF.aniFlag);
			else if (ImGui::Combo("AniFlag", &ai, kAniFlags, IM_ARRAYSIZE(kAniFlags))) {
				static const int map[6][2] = { {0,0}, {1,0}, {2,0}, {1,1}, {2,1}, {2,2} };
				f.AF.aniType = map[ai][0]; f.AF.aniFlag = map[ai][1]; changed = true;
			}
			ImGui::Text("Priority %d: %s", f.AF.priority, PriorityLabel(f.AF.priority).c_str());
			if (f.AS.sineFlags) ImGui::TextColored(ImVec4(1, .6f, .2f, 1), "AST0 sine motion is not stored in HA4");

			const Ha4FrameRaw &R = f.ha4;
			if (R.valid) {
				ImGui::SeparatorText("Original record (kept on save)");
				int16_t g[8];
				for (int i = 0; i < 7; i++) g[i] = R.idx(1 + i);
				g[7] = R.idx(57);
				ImGui::TextDisabled("idx[1..7],[57] (unused, heap garbage): %d %d %d %d %d %d %d | %d",
				                    g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7]);
				bool asPad = false;
				for (int i : {4, 5, 6, 7, 0x1B}) asPad |= R.rec[44 + i] != 0;
				if (asPad) ImGui::TextDisabled("AS has non-zero pad bytes");
				for (int k = 0; k < 8; k++) {
					if (R.idx(8 + k) == -1) continue;
					int32_t p[3];
					memcpy(p, R.ifr[k] + 4 + 36, 12);
					if (p[0] || p[1] || p[2]) ImGui::TextDisabled("IF slot %d params 9..11: %d %d %d", k, p[0], p[1], p[2]);
				}
				if (R.hadAT) {
					int16_t hs; memcpy(&hs, R.at + 0x30, 2);
					ImGui::TextDisabled("AT +0x30 hit-stop time (HA6 ATSN): %d", hs);
				}
				bool parts = !f.AF.layers.empty() && f.AF.layers[0].usePat;
				if (parts && R.boxMask)
					ImGui::TextDisabled("Parts frame: boxes stored at 2x resolution (odd coords kept)");
			} else {
				ImGui::TextDisabled("New frame: encoded from the editor fields on save");
			}
		}
	}

	const auto &err = ha4::LastSaveError();
	const auto &warn = ha4::LastSaveWarnings();
	if (!err.empty() || !warn.empty()) {
		ImGui::SeparatorText("Last save");
		if (!err.empty()) ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", err.c_str());
		if (!warn.empty() && ImGui::TreeNode("warnings", "%zu lossy-field warnings", warn.size())) {
			for (size_t i = 0; i < warn.size() && i < 200; i++) ImGui::TextUnformatted(warn[i].c_str());
			ImGui::TreePop();
		}
	}

	if (changed) {
		ch->undoManager.markModified();
		ch->markModified();
		ch->frameData.mark_modified(state.pattern);
	}
	ImGui::End();
}

bool ExportAsHA6(CharacterInstance *ch, const std::string &ha6Path, std::string &report)
{
	if (!ch || !ch->frameData.isHA4()) { report = "The active character is not an MBAC .DAT."; return false; }
	fs::path out(ha6Path);
	ha4conv::Options opt;
	opt.outDir = out.parent_path().string();
	opt.baseName = out.stem().string();
	ha4conv::Report rep;
	bool ok = ha4conv::Convert(ch->frameData, ch->frameData.m_ha4->sourcePath, opt, rep);
	report = ok ? "Exported:\n" : "Export failed:\n";
	for (auto *p : {&rep.ha6Path, &rep.cgPath, &rep.patPath, &rep.palPath, &rep.txtPath})
		if (!p->empty()) report += "  " + *p + "\n";
	for (auto &l : rep.lines) report += l + "\n";
	return ok;
}

} // namespace ha4ui
