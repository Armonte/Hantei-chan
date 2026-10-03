// "RBO / GOF2 (HAN2)" inspector: every field of the current frame record, named after the IDA struct it is generated from.
#include "han2_character.h"
#include "character_instance.h"
#include "framedata_han2.h"
#include "han2/rbo_types_gen.h"
#include "han2/rbo_at_gen.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace han2ui {

bool showInspector = true;

static const Han2EnumInfo *FindEnum(const char *name)
{
	for (const auto &e : kRboTypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kRboAtEnums) if (!strcmp(e.name, name)) return &e;
	return nullptr;
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

// Edits the fields of one record; returns true when a byte changed.
static bool EditRecord(const char *id, uint8_t *rec, const Han2FieldInfo *tbl, int n)
{
	bool changed = false;
	ImGui::PushID(id);
	bool showUnused = false;
	for (int pass = 0; pass < 2; pass++) {
		if (pass == 1) {
			if (!ImGui::TreeNode("unused", "Unused / never read fields")) break;
			showUnused = true;
		}
		for (int i = 0; i < n; i++) {
			const Han2FieldInfo &f = tbl[i];
			bool unused = !strncmp(f.name, "unused_", 7);
			if (unused != showUnused) continue;
			ImGui::PushID(i);
			for (int k = 0; k < f.count; k++) {
				ImGui::PushID(k);
				uint8_t *p = rec + f.offset + k * f.size;
				char label[96];
				if (f.count > 1) snprintf(label, sizeof(label), "%s[%d] +0x%X", f.name, k, f.offset + k * f.size);
				else snprintf(label, sizeof(label), "%s +0x%X", f.name, f.offset);
				bool sign = f.kind == 1;
				int64_t v = ReadVal(p, f.size, sign);
				if (f.kind == 4) { ImGui::TextDisabled("%s (struct)", label); ImGui::PopID(); continue; }
				if (unused && f.count * f.size > 8) {   // wide zero fill: show as a hex run
					ImGui::TextDisabled("%s: %u bytes", label, (unsigned)(f.count * f.size));
					ImGui::PopID();
					break;
				}
				const Han2EnumInfo *en = f.enumName ? FindEnum(f.enumName) : nullptr;
				ImGui::SetNextItemWidth(150);
				if (en && !en->flags) {
					const char *cur = nullptr;
					for (int e = 0; e < en->count; e++) if (en->values[e].value == v) cur = en->values[e].name;
					char buf[64];
					if (!cur) { snprintf(buf, sizeof(buf), "%lld (unnamed)", (long long)v); cur = buf; }
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

void DrawInspector(CharacterInstance *ch, FrameState &state)
{
	if (!ch || !ch->frameData.isHan2() || !showInspector) return;
	ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 420, vp->WorkPos.y + 60), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(400, 560), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("RBO / GOF2 (HAN2)", &showInspector)) { ImGui::End(); return; }

	const Han2Container &c = *ch->frameData.m_han2;
	ImGui::TextWrapped("%s", c.sourcePath.c_str());
	ImGui::TextDisabled("%s, %s. parts %zu B, CG %zu B", c.sub == 2 ? "GOF2" : "RBO", c.kind == 3 ? ".DT2 (pattern area only)" : ".DAT (full)",
	                    c.parts.size(), c.cg.size());
	ImGui::TextDisabled("Save As .DT2 writes the file the game prefers; .DAT writes the full character.");

	bool changed = false;
	Sequence *seq = ch->frameData.get_sequence(state.pattern);
	if (seq) {
		ImGui::SeparatorText("Pattern");
		int flags = (int)seq->han2.patFlags;
		ImGui::SetNextItemWidth(120);
		if (ImGui::InputInt("pattern flags (0x40 = alt draw mode)", &flags, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) { seq->han2.patFlags = (uint32_t)flags; changed = true; }
		if (state.frame >= 0 && state.frame < (int)seq->frames.size()) {
			Frame &f = seq->frames[state.frame];
			ImGui::SeparatorText("Frame record");
			if (!f.han2.valid) {
				ImGui::TextDisabled("This frame has no source record yet (it is new); fields appear after the first save and reload.");
			} else if (c.sub == 1) {
				if (EditRecord("frame", f.han2.rec, kRboFrameRecordFields, (int)(sizeof(kRboFrameRecordFields) / sizeof(kRboFrameRecordFields[0])))) {
					han2::RedecodeFrame(f);
					changed = true;
				}
				// attack data record (120 bytes, pattern-area section 3)
				ImGui::SeparatorText("Attack record (AT)");
				if (f.han2.hadAT) {
					if (EditRecord("at", f.han2.at, kRboAtRecordFields, (int)(sizeof(kRboAtRecordFields) / sizeof(kRboAtRecordFields[0])))) {
						han2::RedecodeFrame(f);
						changed = true;
					}
				} else {
					ImGui::TextDisabled("none: add an attack box (Atk slot) in the Box Controls to create one.");
				}
				// script lists (sections 6 and 7): five script ids, 0 = unused
				for (int k = 0; k < 2; k++) {
					char hdr[64]; snprintf(hdr, sizeof(hdr), "Script list %c (section %d)", 'A' + k, 6 + k);
					ImGui::SeparatorText(hdr);
					bool had = (f.han2.scriptHad >> k) & 1;
					if (ImGui::Checkbox(k == 0 ? "has list A (frame-enter actions)" : "has list B (transition rules)", &had)) {
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
