// "RBO / GOF2 (HAN2)" inspector: every field of the current frame record, named after the IDA struct it is generated from.
#include "han2_character.h"
#include "character_instance.h"
#include "framedata_han2.h"
#include "han2/rbo_types_gen.h"
#include "han2/rbo_at_gen.h"
#include "han2/gof2_types_gen.h"
#include "han2/gof2_at_gen.h"
#include "han2/gof1_types_gen.h"
#include "han2/mbr_types_gen.h"
#include "han2/pb2k1_types_gen.h"
#include "han2/qoh98_types_gen.h"
#include "han2/qoh99_types_gen.h"
#include "framedata_qoh.h"
#include "framedata_pb2k1.h"

#include "cg.h"
#include "han2_diff.h"
#include "han2_anim.h"
#include "framedata_gof1.h"
#include "character_view.h"
#include <chrono>
#include "filedialog.h"
#include "png_writer.h"
#include "i18n.h"
#include <cstdarg>
#include <glad/glad.h>
#include <imgui.h>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <string>

namespace han2ui {

bool showInspector = true;
unsigned dockInspectorId = 0, dockAnimId = 0;

static const Han2EnumInfo *FindEnum(const char *name)
{
	for (const auto &e : kRboTypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kRboAtEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kGof2TypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kGof2AtEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kGof1TypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kMbrTypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kPb2k1TypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kQoh98TypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kQoh99TypesEnums) if (!strcmp(e.name, name)) return &e;
	return nullptr;
}

static std::string Fmt(const char *fmt, ...)
{
	char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); return b;
}

static uint32_t rdu32le(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static int64_t ReadVal(const uint8_t *p, int size, bool sign)
{
	switch (size) {
	case 1: return sign ? (int64_t)(int8_t)p[0] : (int64_t)p[0];
	case 2: { uint16_t v; memcpy(&v, p, 2); return sign ? (int64_t)(int16_t)v : (int64_t)v; }
	case 4: { uint32_t v; memcpy(&v, p, 4); return sign ? (int64_t)(int32_t)v : (int64_t)v; }
	}
	return 0;
}
static void WriteVal(uint8_t *p, int size, int64_t v) { memcpy(p, &v, size); }   // little endian host

// Field and enum names shown by the generic reflection editor come from the generated spec tables (rbo_*_gen.h etc.) and stay
// English: they are the spec identifiers. Only the chrome around them (headers, hints, counts) is translated.
// Edits the fields of one record; returns true when a byte changed.
bool EditRecordFields(const char *id, uint8_t *rec, const Han2FieldInfo *tbl, int n);
static bool EditRecord(const char *id, uint8_t *rec, const Han2FieldInfo *tbl, int n) { return EditRecordFields(id, rec, tbl, n); }
bool EditRecordFields(const char *id, uint8_t *rec, const Han2FieldInfo *tbl, int n)
{
	bool changed = false;
	ImGui::PushID(id);
	bool showUnused = false;
	for (int pass = 0; pass < 2; pass++) {
		if (pass == 1) {
			if (!ImGui::TreeNode(LBL("unused"), "%s", TXT("Unused / never read fields"))) break;
			showUnused = true;
		}
		for (int i = 0; i < n; i++) {
			const Han2FieldInfo &f = tbl[i];
			bool unused = !strncmp(f.name, "unused_", 7);
			if (unused != showUnused) continue;
			ImGui::PushID(i);
			if (f.kind == 5) {   // fixed-size text field: CP932 bytes, NUL padded; edited as UTF-8, written back as CP932 (rest of the field zero-filled)
				std::string cur((const char *)rec + f.offset, strnlen((const char *)rec + f.offset, f.count));
				std::string u = sj2utf8(cur);
				char label[96]; snprintf(label, sizeof(label), "%s +0x%X", f.name, f.offset);
				std::vector<char> buf(u.begin(), u.end()); buf.resize(u.size() + 512, 0);
				if (ImGui::InputTextMultiline(label, buf.data(), buf.size(), ImVec2(380, 54))) {
					std::string sj = utf82sj(std::string(buf.data()));
					if (sj.size() >= f.count) sj.resize(f.count - 1);
					memset(rec + f.offset, 0, f.count); memcpy(rec + f.offset, sj.data(), sj.size()); changed = true;
				}
				if (f.comment && f.comment[0] && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.comment);
				ImGui::PopID(); continue;
			}
			for (int k = 0; k < f.count; k++) {
				ImGui::PushID(k);
				uint8_t *p = rec + f.offset + k * f.size;
				char label[96];
				if (f.count > 1) snprintf(label, sizeof(label), "%s[%d] +0x%X", f.name, k, f.offset + k * f.size);
				else snprintf(label, sizeof(label), "%s +0x%X", f.name, f.offset);
				bool sign = f.kind == 1;
				int64_t v = ReadVal(p, f.size, sign);
				if (f.kind == 4) { ImGui::TextDisabled(TXT("%s (struct)"), label); ImGui::PopID(); continue; }
				if (unused && f.count * f.size > 8) {   // wide zero fill: show as a hex run
					ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("%s: %u bytes"), label, (unsigned)(f.count * f.size)); ImGui::PopTextWrapPos();
					ImGui::PopID();
					break;
				}
				if (f.kind == 6) {
					float fv; memcpy(&fv, p, 4); ImGui::SetNextItemWidth(150);
					if (ImGui::InputFloat(label, &fv, 0, 0, "%.6g")) { memcpy(p, &fv, 4); changed = true; }
					if (f.comment && f.comment[0] && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.comment);
					ImGui::PopID(); continue;
				}
				const Han2EnumInfo *en = f.enumName ? FindEnum(f.enumName) : nullptr;
				ImGui::SetNextItemWidth(150);
				if (en && !en->flags) {
					const char *cur = nullptr;
					for (int e = 0; e < en->count; e++) if (en->values[e].value == v) cur = en->values[e].name;
					char buf[64];
					if (!cur) { snprintf(buf, sizeof(buf), TXT("%lld (unnamed)"), (long long)v); cur = buf; }
					if (ImGui::BeginCombo(label, cur)) {
						for (int e = 0; e < en->count; e++)
							if (ImGui::Selectable(en->values[e].name, en->values[e].value == v)) { WriteVal(p, f.size, en->values[e].value); changed = true; }
						ImGui::EndCombo();
					}
				} else if (en && en->flags) {
					char hex[32]; snprintf(hex, sizeof(hex), "0x%llX", (unsigned long long)v);
					if (ImGui::TreeNode(label, "%s = %s", label, hex)) {
						for (int e = 0; e < en->count; e++) {
							bool on = (v & en->values[e].value) != 0;
							if (en->values[e].value && ImGui::Checkbox(en->values[e].name, &on)) {
								int64_t nv = on ? (v | en->values[e].value) : (v & ~en->values[e].value);
								WriteVal(p, f.size, nv); changed = true; v = nv;
							}
						}
						ImGui::TreePop();
					}
				} else {
					int iv = (int)v;
					if (ImGui::InputInt(label, &iv, 0, 0)) { WriteVal(p, f.size, iv); changed = true; }
				}
				if (f.comment && f.comment[0] && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.comment);
				ImGui::PopID();
			}
			ImGui::PopID();
		}
		if (pass == 1) ImGui::TreePop();
	}
	ImGui::PopID();
	return changed;
}

// SimulateFlow's end note is English (it also goes into exports): translate it for display.
static std::string TrFlowNote(const std::string &n)
{
	int v = 0; char buf[200];
	if (sscanf(n.c_str(), "ends: jumps to pattern %d", &v) == 1) { snprintf(buf, sizeof buf, TXT("ends: jumps to pattern %d"), v); return buf; }
	if (sscanf(n.c_str(), "ani flag %d depends on runtime state", &v) == 1) { snprintf(buf, sizeof buf, TXT("ani flag %d depends on runtime state"), v); return buf; }
	return TXT(n.c_str());
}
void DrawInspector(CharacterInstance *ch, FrameState &state)
{
	if (!ch || !ch->frameData.isHan2() || !showInspector) return;
	ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 420, vp->WorkPos.y + 60), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 560), ImGuiCond_FirstUseEver);
	if (dockInspectorId) ImGui::SetNextWindowDockID(dockInspectorId, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("RBO / GOF2 (HAN2)"), &showInspector)) { ImGui::End(); return; }

	const Han2Container &c = *ch->frameData.m_han2;
	ImGui::TextWrapped("%s", c.sourcePath.c_str());
	ImGui::TextDisabled(TXT("%s, %s. parts %zu B, CG %zu B"), c.sub == 5 ? (c.kind == 99 ? "Queen of Heart 99" : "Queen of Heart 98") : c.sub == 4 ? "Party Breakers" : c.sub == 3 ? "GOF1 / MB" : c.sub == 2 ? "GOF2" : "RBO", c.kind == 3 ? TXT(".DT2 (pattern area only)") : TXT(".DAT (full)"),
	                    c.parts.size(), c.cg.size());
	ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("Save As .DT2 writes the file the game prefers; .DAT writes the full character.")); ImGui::PopTextWrapPos();

	bool changed = false;
	Sequence *seq = ch->frameData.get_sequence(state.pattern);
	if (seq) {
		ImGui::SeparatorText(TXT("Pattern"));
		int flags = (int)seq->han2.patFlags;
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputInt(LBL("pattern flags (0x40 = alt draw mode)"), &flags, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) { seq->han2.patFlags = (uint32_t)flags; changed = true; }
		{
			std::vector<std::pair<int, int>> visits; std::string note;
			han2::SimulateFlow(*seq, visits, note);
			int ticks = 0; for (auto &v : visits) ticks += v.second;
			ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("game flow: %zu frame entries, %d ticks, %s"), visits.size(), ticks, TrFlowNote(note).c_str()); ImGui::PopTextWrapPos();
			if (ImGui::IsItemHovered()) {
				std::string t; for (size_t i = 0; i < visits.size() && i < 60; i++) t += std::to_string(visits[i].first) + "(" + std::to_string(visits[i].second) + ") ";
				ImGui::SetTooltip(TXT("frame(ticks) in the order the engine visits them:\n%s"), t.c_str());
			}
		}
		if (state.frame >= 0 && state.frame < (int)seq->frames.size()) {
			Frame &f = seq->frames[state.frame];
			ImGui::SeparatorText(TXT("Frame record"));
			if (!f.han2.valid) {
				ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("This frame has no source record yet (it is new); fields appear after the first save and reload.")); ImGui::PopTextWrapPos();
			} else if (c.sub == 3) {
				bool ch1 = EditRecord("g1af", f.han2.rec, kGof1AnimFrameFields, (int)(sizeof(kGof1AnimFrameFields) / sizeof(kGof1AnimFrameFields[0])));
				ImGui::SeparatorText(TXT("State (AS)"));
				ch1 |= EditRecord("g1as", f.han2.rec + 0x28, kGof1StateFrameFields, (int)(sizeof(kGof1StateFrameFields) / sizeof(kGof1StateFrameFields[0])));
				ImGui::SeparatorText(TXT("Attack record (AT, 26 bytes)"));
				if (f.han2.hadAT) ch1 |= EditRecord("g1at", f.han2.at, kGof1AtRecordFields, (int)(sizeof(kGof1AtRecordFields) / sizeof(kGof1AtRecordFields[0])));
				else ImGui::TextDisabled("%s", TXT("none: add an attack box (slot 8) to create one."));
				for (int k = 0; k < 3; k++) if (f.han2.gofIfMask >> k & 1) { char h[96]; snprintf(h, sizeof(h), TXT("IF slot %d (28 bytes)"), k); ImGui::SeparatorText(h); ch1 |= EditRecord(("g1if" + std::to_string(k)).c_str(), f.han2.gofIf[k], kGof1IfRecordFields, (int)(sizeof(kGof1IfRecordFields) / sizeof(kGof1IfRecordFields[0]))); }
				for (int k = 0; k < 4; k++) if (f.han2.gofEfMask >> k & 1) { char h[96]; snprintf(h, sizeof(h), TXT("EF slot %d (20 bytes)"), k); ImGui::SeparatorText(h); ch1 |= EditRecord(("g1ef" + std::to_string(k)).c_str(), f.han2.gofEf[k], kGof1EfRecordFields, (int)(sizeof(kGof1EfRecordFields) / sizeof(kGof1EfRecordFields[0]))); }
				if (ch1) { gof1::RedecodeFrame(f); changed = true; }
			} else if (c.sub == 4) {   // Party Breakers (docs/formats/pb2k1.md section 4)
				bool ch4 = EditRecord("p2af", f.han2.rec, kPb2AnimFrameFields, (int)(sizeof(kPb2AnimFrameFields) / sizeof(kPb2AnimFrameFields[0])));
				ImGui::SeparatorText(TXT("State (AS)"));
				ch4 |= EditRecord("p2as", f.han2.rec + 0x18, kPb2StateFrameFields, (int)(sizeof(kPb2StateFrameFields) / sizeof(kPb2StateFrameFields[0])));
				ImGui::SeparatorText(TXT("Attack record (AT, 26 bytes)"));
				if (f.han2.hadAT) ch4 |= EditRecord("p2at", f.han2.at, kPb2AtRecordFields, (int)(sizeof(kPb2AtRecordFields) / sizeof(kPb2AtRecordFields[0])));
				else ImGui::TextDisabled("%s", TXT("none: add an attack box (slot 8) to create one."));
				for (int k = 0; k < 3; k++) if (f.han2.gofIfMask >> k & 1) { char h[96]; snprintf(h, sizeof(h), TXT("IF slot %d (20 bytes)"), k); ImGui::SeparatorText(h); ch4 |= EditRecord(("p2if" + std::to_string(k)).c_str(), f.han2.gofIf[k], kPb2IfRecordFields, (int)(sizeof(kPb2IfRecordFields) / sizeof(kPb2IfRecordFields[0]))); }
				for (int k = 0; k < 4; k++) if (f.han2.gofEfMask >> k & 1) { char h[96]; snprintf(h, sizeof(h), TXT("EF slot %d (12 bytes)"), k); ImGui::SeparatorText(h); ch4 |= EditRecord(("p2ef" + std::to_string(k)).c_str(), f.han2.gofEf[k], kPb2EfRecordFields, (int)(sizeof(kPb2EfRecordFields) / sizeof(kPb2EfRecordFields[0]))); }
				if (ch4) { pb2k1::RedecodeFrame(f); changed = true; }
			} else if (c.sub == 5) {   // Queen of Heart '98 / '99: the frame (sprite) record and the attack records; boxes are edited in the box pane
				const bool v99 = c.kind == 99;
				bool ch5 = v99 ? EditRecord("qf", f.han2.rec, kQohFrameFields, (int)(sizeof(kQohFrameFields) / sizeof(kQohFrameFields[0])))
				               : EditRecord("qf", f.han2.rec, kQoh98SpriteFields, (int)(sizeof(kQoh98SpriteFields) / sizeof(kQoh98SpriteFields[0])));
				for (int j = 0; j < 8; j++) if (f.han2.boxMask >> (16 + j) & 1) {
					char h[96]; snprintf(h, sizeof(h), TXT("Attack record %d (24 bytes)"), j); ImGui::SeparatorText(h);
					ch5 |= v99 ? EditRecord(("qa" + std::to_string(j)).c_str(), f.han2.rec + 168 + 24 * j, kQohAttackFields, (int)(sizeof(kQohAttackFields) / sizeof(kQohAttackFields[0])))
					           : EditRecord(("qa" + std::to_string(j)).c_str(), f.han2.rec + 168 + 24 * j, kQoh98HitBoxFields, (int)(sizeof(kQoh98HitBoxFields) / sizeof(kQoh98HitBoxFields[0])));
				}
				if (ch5) { qoh::RedecodeFrame(f, (int)c.kind); changed = true; }
			} else if (c.sub == 2) {
				if (EditRecord("gframe", f.han2.rec, kGof2FrameRecordFields, (int)(sizeof(kGof2FrameRecordFields) / sizeof(kGof2FrameRecordFields[0])))) {
					han2::RedecodeFrame(f);
					changed = true;
				}
				ImGui::SeparatorText(TXT("Attack record (AT, 236 bytes)"));
				if (f.han2.hadAT) { if (EditRecord("gat", f.han2.at, kGof2AtRecordFields, (int)(sizeof(kGof2AtRecordFields) / sizeof(kGof2AtRecordFields[0])))) changed = true; }
				else ImGui::TextDisabled("%s", TXT("none: add an attack box to create one (starts zero-filled)."));
				if (f.han2.hadFx) {
					ImGui::SeparatorText(TXT("Effect-spawn record (section 8, 96 bytes)"));
					if (EditRecord("gfx", f.han2.fx, kGof2EffectSpawnRecordFields, (int)(sizeof(kGof2EffectSpawnRecordFields) / sizeof(kGof2EffectSpawnRecordFields[0])))) changed = true;
				}
				for (int k = 0; k < 2; k++) if (f.han2.scriptHad >> k & 1) {
					char hdr[128]; snprintf(hdr, sizeof(hdr), TXT("Script list %c (section %d)"), 'A' + k, 6 + k); ImGui::SeparatorText(hdr);
					if (EditRecord(k == 0 ? "gslA" : "gslB", f.han2.script[k], kGof2ScriptListEntryFields, (int)(sizeof(kGof2ScriptListEntryFields) / sizeof(kGof2ScriptListEntryFields[0])))) changed = true;
				}
			} else if (c.sub == 1) {
				if (EditRecord("frame", f.han2.rec, kRboFrameRecordFields, (int)(sizeof(kRboFrameRecordFields) / sizeof(kRboFrameRecordFields[0])))) {
					han2::RedecodeFrame(f);
					changed = true;
				}
				// attack data record (120 bytes, pattern-area section 3)
				ImGui::SeparatorText(TXT("Attack record (AT)"));
				if (f.han2.hadAT) {
					if (EditRecord("at", f.han2.at, kRboAtRecordFields, (int)(sizeof(kRboAtRecordFields) / sizeof(kRboAtRecordFields[0])))) {
						han2::RedecodeFrame(f);
						changed = true;
					}
				} else {
					ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("none: add an attack box (Atk slot) in the Box Controls to create one.")); ImGui::PopTextWrapPos();
				}
				// script lists (sections 6 and 7): five script ids, 0 = unused
				for (int k = 0; k < 2; k++) {
					char hdr[128]; snprintf(hdr, sizeof(hdr), TXT("Script list %c (section %d)"), 'A' + k, 6 + k);
					ImGui::SeparatorText(hdr);
					bool had = (f.han2.scriptHad >> k) & 1;
					if (ImGui::Checkbox(k == 0 ? LBL("has list A (frame-enter actions)") : LBL("has list B (transition rules)"), &had)) {
						if (had) f.han2.scriptHad |= (uint8_t)(1 << k); else f.han2.scriptHad &= (uint8_t)~(1 << k);
						// a frame without a list has index 0; a new list gets a UNIQUE placeholder index so the writer emits its own
						// record (the writer shares records by original index, a placeholder must never collide with a real one)
						static uint32_t s_placeholder = 0x40000000u;
						const uint32_t ph = ++s_placeholder;
						if (had && rdu32le(f.han2.rec + (k == 0 ? 0xBC : 0xC0)) == 0) memcpy(f.han2.rec + (k == 0 ? 0xBC : 0xC0), &ph, 4);
						changed = true;
					}
					if (had && EditRecord(k == 0 ? "slA" : "slB", f.han2.script[k], kRboScriptListEntryFields, (int)(sizeof(kRboScriptListEntryFields) / sizeof(kRboScriptListEntryFields[0])))) changed = true;
				}
			}
		}
	}
	if (changed) {
		ch->undoManager.markModified();
		ch->markModified();
		ch->frameData.mark_modified(state.pattern);
	}
	ImGui::End();
}

} // namespace han2ui

namespace han2ui {

bool showCgWindow = false;

namespace {
struct CgPreview { const CharacterInstance *owner = nullptr; int image = -1; unsigned long long gen = 0; GLuint tex = 0; int w = 0, h = 0; int ox = 0, oy = 0; };
CgPreview g_prev;
char g_cgFilter[32] = "";
std::vector<std::vector<char>> g_cgUndo, g_cgRedo;   // whole-bank snapshots (max 8)
std::string g_cgMsg;
int g_cgSel = 0;
float g_cgZoom = 1.f;

void LoadPreview(CharacterInstance &ch, int image)
{
	g_prev.owner = &ch; g_prev.image = image; g_prev.gen = ch.cg.generation();
	ImageData *im = ch.cg.draw_texture((unsigned)image, false, false);
	if (!im) { g_prev.w = g_prev.h = 0; return; }
	if (!g_prev.tex) glGenTextures(1, &g_prev.tex);
	glBindTexture(GL_TEXTURE_2D, g_prev.tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->width, im->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, im->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	g_prev.w = im->width; g_prev.h = im->height; g_prev.ox = im->offsetX; g_prev.oy = im->offsetY;
	delete im;
}
} // namespace

void DrawCgWindow(CharacterInstance *ch)
{
	if (!showCgWindow) return;
	ImGui::SetNextWindowSize(ImVec2(700, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("CG sprites (RBO / GOF2 bank)"), &showCgWindow)) { ImGui::End(); return; }
	if (!ch || !ch->cg.m_loaded) { ImGui::TextDisabled("%s", TXT("The active character has no CG bank.")); ImGui::End(); return; }
	const int n = ch->cg.get_image_count();
	ImGui::SetNextItemWidth(100); ImGui::InputText(LBL("filter"), g_cgFilter, sizeof(g_cgFilter)); ImGui::SameLine(); ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("%d images"), n); ImGui::PopTextWrapPos();
	ImGui::BeginChild("list", ImVec2(210, 0), true);
	for (int i = 0; i < n; i++) {
		int bpp, ty, x1, y1, x2, y2;
		if (!ch->cg.image_info(i, bpp, ty, x1, y1, x2, y2)) continue;
		char label[96]; snprintf(label, sizeof(label), "%4d  %dx%d  t%d", i, x2 - x1 + 1, y2 - y1 + 1, ty);
		if (g_cgFilter[0] && !strstr(label, g_cgFilter)) continue;
		if (ImGui::Selectable(label, g_cgSel == i)) g_cgSel = i;
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginGroup();
	if (g_prev.owner != ch || g_prev.image != g_cgSel || g_prev.gen != ch->cg.generation()) LoadPreview(*ch, g_cgSel);
	int bpp = 0, ty = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0; ch->cg.image_info(g_cgSel, bpp, ty, x1, y1, x2, y2);
	ImGui::Text(TXT("image %d: storage type %d, %d-bit, canvas bounds (%d,%d)-(%d,%d)"), g_cgSel, ty, bpp, x1, y1, x2, y2);
	ImGui::SetNextItemWidth(120); ImGui::SliderFloat(LBL("zoom"), &g_cgZoom, 0.25f, 6.f);
	if (ImGui::Button(LBL("Export PNG..."))) {
		std::string p = FileDialog(-1, true);
		if (!p.empty()) { if (p.size() < 4 || p.substr(p.size() - 4) != ".png") p += ".png";
			ImageData *im = ch->cg.draw_texture((unsigned)g_cgSel, false, false); std::string e;
			g_cgMsg = (im && WritePngRgba(p, im->pixels, im->width, im->height, e)) ? Fmt(TXT("exported %s"), p.c_str()) : (e.empty() ? std::string(TXT("empty image")) : e); delete im; }
	}
	i18n::SameLineFit(i18n::ButtonWidth(LBL("Import PNG (same size)...")));
	if (ImGui::Button(LBL("Import PNG (same size)..."))) {
		std::string p = FileDialog(-1, false);
		if (!p.empty()) {
			std::vector<uint8_t> px; int w = 0, h = 0; std::string e;
			if (!ReadImageRgba(p, px, w, h, e)) g_cgMsg = e;
			else if ((g_cgUndo.push_back(std::vector<char>(ch->cg.bank_data(), ch->cg.bank_data() + ch->cg.bank_size())), g_cgRedo.clear(), g_cgUndo.size() > 8 && (g_cgUndo.erase(g_cgUndo.begin()), true), ch->cg.replace_image_rgba((unsigned)g_cgSel, px.data(), w, h, &e))) { ch->markModified(); ch->undoManager.markModified(); g_cgMsg = Fmt(TXT("imported %s (save as .DAT to keep it)"), p.c_str()); }
			else g_cgMsg = e;
		}
	}
	i18n::SameLineFit(i18n::ButtonWidth(LBL("Undo import")));
	ImGui::BeginDisabled(g_cgUndo.empty());
	if (ImGui::Button(LBL("Undo import"))) { g_cgRedo.push_back(std::vector<char>(ch->cg.bank_data(), ch->cg.bank_data() + ch->cg.bank_size())); ch->cg.restore_bank(g_cgUndo.back().data(), (unsigned)g_cgUndo.back().size()); g_cgUndo.pop_back(); ch->markModified(); g_cgMsg = TXT("import undone"); }
	ImGui::EndDisabled();
	i18n::SameLineFit(i18n::ButtonWidth(LBL("Redo")));
	ImGui::BeginDisabled(g_cgRedo.empty());
	if (ImGui::Button(LBL("Redo"))) { g_cgUndo.push_back(std::vector<char>(ch->cg.bank_data(), ch->cg.bank_data() + ch->cg.bank_size())); ch->cg.restore_bank(g_cgRedo.back().data(), (unsigned)g_cgRedo.back().size()); g_cgRedo.pop_back(); ch->markModified(); g_cgMsg = TXT("import redone"); }
	ImGui::EndDisabled();
	i18n::SameLineFit(i18n::ButtonWidth(LBL("Export all...")));
	if (ImGui::Button(LBL("Export all..."))) {
		std::string d = BrowseForFolderUtf8("");
		if (!d.empty()) { int ok = 0; std::string e; for (int i = 0; i < n; i++) { ImageData *im = ch->cg.draw_texture((unsigned)i, false, false); if (!im) continue; char nm[64]; snprintf(nm, sizeof(nm), "\\cg_%04d.png", i); if (WritePngRgba(d + nm, im->pixels, im->width, im->height, e)) ok++; delete im; } g_cgMsg = Fmt(TXT("exported %d PNGs to %s"), ok, d.c_str()); }
	}
	if (!g_cgMsg.empty()) ImGui::TextWrapped("%s", g_cgMsg.c_str());
	ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("CG sprites live in the .DAT: save as .DAT to keep imports. Storage types 1, 2 and 4 can be imported; palettes are quantized to 255 colours.")); ImGui::PopTextWrapPos();
	ImGui::BeginChild("prev", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	if (g_prev.w > 0) {
		ImVec2 p0 = ImGui::GetCursorScreenPos(), sz(g_prev.w * g_cgZoom, g_prev.h * g_cgZoom);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		for (float y = 0; y < sz.y; y += 16) for (float x = 0; x < sz.x; x += 16)
			dl->AddRectFilled(ImVec2(p0.x + x, p0.y + y), ImVec2(std::min(p0.x + x + 16, p0.x + sz.x), std::min(p0.y + y + 16, p0.y + sz.y)), (((int)(x / 16) + (int)(y / 16)) & 1) ? IM_COL32(110, 110, 110, 255) : IM_COL32(160, 160, 160, 255));
		ImGui::Image((ImTextureID)(intptr_t)g_prev.tex, sz);
	} else ImGui::TextDisabled("%s", TXT("(empty image)"));
	ImGui::EndChild();
	ImGui::EndGroup();
	ImGui::End();
}

} // namespace han2ui

namespace han2ui {

bool showDiffWindow = false;

void DrawDiffWindow(CharacterInstance *ch)
{
	if (!showDiffWindow) return;
	ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Changes against the loaded file"), &showDiffWindow)) { ImGui::End(); return; }
	if (!ch || !ch->frameData.isHan2()) { ImGui::TextDisabled("%s", TXT("The active character is not an RBO / GOF2 file.")); ImGui::End(); return; }
	static std::vector<han2::DiffEntry> entries; static std::string err; static bool ran = false; static const CharacterInstance *who = nullptr;
	if (ImGui::Button(LBL("Compare now")) || who != ch) { ran = true; who = ch; err.clear(); han2::DiffAgainstOriginal(ch->frameData, entries, &err); }
	ImGui::SameLine(); ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled(TXT("%zu changes"), entries.size()); ImGui::PopTextWrapPos();
	if (!err.empty()) ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "%s", err.c_str());
	if (ran && ImGui::BeginTable("diff", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn(LBL("pattern")); ImGui::TableSetupColumn(LBL("frame")); ImGui::TableSetupColumn(LBL("change"));
		ImGui::TableSetupScrollFreeze(0, 1); ImGui::TableHeadersRow();
		for (size_t i = 0; i < entries.size() && i < 5000; i++) {
			ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("%d", entries[i].pattern);
			ImGui::TableSetColumnIndex(1); if (entries[i].frame >= 0) ImGui::Text("%d", entries[i].frame);
			ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(entries[i].what.c_str());
		}
		ImGui::EndTable();
	}
	ImGui::End();
}

} // namespace han2ui

namespace han2ui {

bool showAnimWindow = false;

void DrawAnimWindow(CharacterInstance *ch, FrameState &state, void *onionPtr)
{
	if (!showAnimWindow) return;
	OnionSkinSettings &onion = *(OnionSkinSettings *)onionPtr;
	ImGui::SetNextWindowSize(ImVec2(620, 330), ImGuiCond_FirstUseEver);
	if (dockAnimId) ImGui::SetNextWindowDockID(dockAnimId, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Animation (game rules)"), &showAnimWindow)) { ImGui::End(); return; }
	if (!ch || !ch->frameData.isHan2()) { ImGui::TextDisabled("%s", TXT("The active character is not an RBO / GOF2 file.")); ImGui::End(); return; }
	FrameData &fd = ch->frameData;
	static han2::AnimState st; static bool playing = false, loop = true, follow = false, init = false; static int rateHz = 120; static float speed = 1.f;
	static std::chrono::steady_clock::time_point last; static double acc = 0.0;
	static const CharacterInstance *who = nullptr; static int lastPattern = -1, lastFrame = -1;
	// follow selection changes made elsewhere (pattern list / frame buttons) while not playing
	if (!init || who != ch || (!playing && (state.pattern != lastPattern || state.frame != lastFrame))) {
		han2::AnimStart(fd, st, state.pattern); st.frame = std::max(0, state.frame); init = true; who = ch; playing = false;
	}
	Sequence *seq = fd.get_sequence(state.pattern);
	if (!seq || seq->frames.empty()) { ImGui::TextDisabled("%s", TXT("This pattern has no frames.")); ImGui::End(); return; }
	// controls
	if (ImGui::Button(playing ? LBL("Pause") : LBL("Play"))) { playing = !playing; last = std::chrono::steady_clock::now(); acc = 0; if (playing && st.ended) { han2::AnimStart(fd, st, state.pattern); } }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Restart"))) { han2::AnimStart(fd, st, state.pattern); acc = 0; }
	ImGui::SameLine();
	if (ImGui::Button(LBL("< frame"))) { playing = false; han2::AnimStepFrame(fd, st, -1); }
	ImGui::SameLine();
	if (ImGui::Button(LBL("frame >"))) { playing = false; han2::AnimStepFrame(fd, st, +1); }
	ImGui::SameLine();
	if (ImGui::Button(LBL("+1 tick"))) { playing = false; han2::AnimTick(fd, st, follow, loop); }
	ImGui::SameLine(); ImGui::Checkbox(LBL("loop"), &loop); ImGui::SameLine(); ImGui::Checkbox(LBL("follow pattern jumps"), &follow);
	ImGui::SetNextItemWidth(90);
	static const int rates[] = { 30, 60, 120, 240 }; static int ri = 2;
	if (ImGui::BeginCombo(LBL("logic rate"), (std::to_string(rateHz) + " Hz").c_str())) { for (int i = 0; i < 4; i++) if (ImGui::Selectable((std::to_string(rates[i]) + " Hz").c_str(), rateHz == rates[i])) { rateHz = rates[i]; ri = i; } ImGui::EndCombo(); }
	ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::SliderFloat(LBL("speed"), &speed, 0.1f, 4.f, "x%.2f");
	ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("durations are logic ticks; the rate is the assumed engine tick rate (the game window shows FPS 60 (120))")); ImGui::PopTextWrapPos();
	// advance
	if (playing) {
		auto now = std::chrono::steady_clock::now();
		acc += std::chrono::duration<double>(now - last).count() * rateHz * speed; last = now;
		int guard = 0;
		while (acc >= 1.0 && guard++ < 600) { acc -= 1.0; han2::AnimTick(fd, st, follow, loop); if (st.ended) { playing = false; break; } }
	}
	if (st.pattern != state.pattern && st.pattern >= 0) state.pattern = st.pattern;
	state.frame = st.frame; state.currentTick = 0;
	lastPattern = state.pattern; lastFrame = state.frame;
	seq = fd.get_sequence(state.pattern);
	ImGui::Text(TXT("pattern %d  frame %d/%zu  tick %d/%d  loop counter %d  elapsed %d ticks (%.2f s)%s"), st.pattern, st.frame, seq ? seq->frames.size() : 0,
	            st.ticksInFrame, seq && st.frame < (int)seq->frames.size() ? seq->frames[st.frame].AF.duration : 0, st.loopCounter, st.totalTicks, st.totalTicks / (double)rateHz, st.ended ? TXT("  [ended]") : "");
	// onion skin
	ImGui::Checkbox(LBL("onion skin"), &onion.enabled);
	if (onion.enabled) { ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::SliderInt(LBL("before"), &onion.before, 0, 6); ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::SliderInt(LBL("after"), &onion.after, 0, 6); ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::SliderInt(LBL("spacing"), &onion.spacing, 1, 12); ImGui::SameLine(); ImGui::Checkbox(LBL("keyframes only"), &onion.keyframesOnly); }
	// timeline: one cell per frame, width proportional to its duration; red = attack boxes, blue = script lists (effects), current outlined
	if (seq) {
		ImDrawList *dl = ImGui::GetWindowDrawList();
		ImVec2 p0 = ImGui::GetCursorScreenPos(); float W = ImGui::GetContentRegionAvail().x - 6, H = 34;
		int total = 0; for (auto &f : seq->frames) total += std::max(1, f.AF.duration);
		float x = p0.x;
		for (size_t i = 0; i < seq->frames.size(); i++) {
			const Frame &f = seq->frames[i]; float w = W * std::max(1, f.AF.duration) / (float)std::max(1, total);
			bool atk = f.han2.hadAT, fx = f.han2.scriptHad != 0 || f.han2.hadFx;
			ImU32 col = atk ? IM_COL32(190, 60, 60, 255) : IM_COL32(90, 110, 90, 255);
			dl->AddRectFilled(ImVec2(x, p0.y), ImVec2(x + w - 1, p0.y + H), col);
			if (fx) dl->AddRectFilled(ImVec2(x, p0.y + H - 8), ImVec2(x + w - 1, p0.y + H), IM_COL32(80, 160, 255, 255));
			if ((int)i == st.frame) {
				dl->AddRect(ImVec2(x, p0.y), ImVec2(x + w - 1, p0.y + H), IM_COL32(255, 255, 0, 255), 0, 0, 2.f);
				float fillW = f.AF.duration > 0 ? w * std::min(1.f, st.ticksInFrame / (float)f.AF.duration) : w;
				dl->AddRectFilled(ImVec2(x, p0.y + H + 2), ImVec2(x + fillW, p0.y + H + 6), IM_COL32(255, 255, 0, 255));
			}
			ImGui::SetCursorScreenPos(ImVec2(x, p0.y)); ImGui::InvisibleButton(("fr" + std::to_string(i)).c_str(), ImVec2(std::max(w - 1, 2.f), H));
			if (ImGui::IsItemClicked()) { playing = false; st.frame = (int)i; st.ticksInFrame = 0; st.ended = false; }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("frame %zu: %d ticks%s%s"), i, f.AF.duration, atk ? TXT(", attack") : "", fx ? TXT(", script/effect list") : "");
			x += w;
		}
		ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + H + 10));
		ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("timeline: red = attack frame, blue strip = script/effect list, width = duration. Boxes are drawn by the main view for the playing frame.")); ImGui::PopTextWrapPos();
	}
	ImGui::End();
}

} // namespace han2ui
