#include "han2_diff.h"
#include "framedata_han2.h"
#include "han2/rbo_types_gen.h"
#include "han2/gof2_types_gen.h"
#include "han2/rbo_at_gen.h"

#include <cstdio>
#include <cstring>

namespace han2 {

namespace {

const Han2EnumInfo *FindEnum(const char *name)
{
	for (const auto &e : kRboTypesEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kRboAtEnums) if (!strcmp(e.name, name)) return &e;
	for (const auto &e : kGof2TypesEnums) if (!strcmp(e.name, name)) return &e;
	return nullptr;
}

std::string ValueText(const uint8_t *p, const Han2FieldInfo &f)
{
	int64_t v = 0;
	bool sign = f.kind == 1;
	switch (f.size) {
	case 1: v = sign ? (int64_t)(int8_t)p[0] : p[0]; break;
	case 2: { uint16_t x; memcpy(&x, p, 2); v = sign ? (int64_t)(int16_t)x : x; break; }
	default: { uint32_t x; memcpy(&x, p, 4); v = sign ? (int64_t)(int32_t)x : x; break; }
	}
	if (f.enumName) if (auto *e = FindEnum(f.enumName)) { if (!e->flags) for (int i = 0; i < e->count; i++) if (e->values[i].value == v) return e->values[i].name; }
	char b[32];
	if (f.enumName) snprintf(b, sizeof(b), "0x%llX", (unsigned long long)v); else snprintf(b, sizeof(b), "%lld", (long long)v);
	return b;
}

void DiffRecord(const uint8_t *a, const uint8_t *b, const Han2FieldInfo *tbl, int n, const char *prefix, int pat, int frame, std::vector<DiffEntry> &out)
{
	for (int i = 0; i < n; i++) {
		const Han2FieldInfo &f = tbl[i];
		if (f.kind == 4) continue;
		// table indices and group counts are renumbered by every save; boxes / attacks / scripts are compared by content below
		{ std::string nm = f.name; if (nm.find("Idx") != std::string::npos || nm.find("Index") != std::string::npos || nm.find("BoxCount") != std::string::npos || nm.find("Enable") != std::string::npos || nm == "hasAttack" || nm.find("hasAttack") != std::string::npos) continue; }
		for (int k = 0; k < f.count; k++) {
			const int off = f.offset + k * f.size;
			if (memcmp(a + off, b + off, f.size) == 0) continue;
			DiffEntry d; d.pattern = pat; d.frame = frame;
			char nm[96]; if (f.count > 1) snprintf(nm, sizeof(nm), "%s%s[%d]", prefix, f.name, k); else snprintf(nm, sizeof(nm), "%s%s", prefix, f.name);
			d.what = std::string(nm) + " " + ValueText(a + off, f) + " -> " + ValueText(b + off, f);
			out.push_back(d);
		}
	}
}

} // namespace

bool DiffAgainstOriginal(const FrameData &fd, std::vector<DiffEntry> &out, std::string *err)
{
	out.clear();
	if (!fd.m_han2 || fd.m_han2->originalPatternFile.empty()) { if (err) *err = "no original data kept for this character"; return false; }
	FrameData orig;
	std::string e;
	if (!Load(orig, fd.m_han2->originalPatternFile.data(), fd.m_han2->originalPatternFile.size(), &e)) { if (err) *err = e; return false; }
	const bool gof = fd.m_han2->sub == 2;
	const Han2FieldInfo *tbl = gof ? kGof2FrameRecordFields : kRboFrameRecordFields;
	const int ntbl = gof ? (int)(sizeof(kGof2FrameRecordFields) / sizeof(kGof2FrameRecordFields[0])) : (int)(sizeof(kRboFrameRecordFields) / sizeof(kRboFrameRecordFields[0]));
	for (int p = 0; p < 256; p++) {
		const auto &A = orig.m_sequences[p], &B = p < (int)fd.m_sequences.size() ? fd.m_sequences[p] : orig.m_sequences[p];
		if (A.frames.size() != B.frames.size()) { DiffEntry d; d.pattern = p; d.what = "frame count " + std::to_string(A.frames.size()) + " -> " + std::to_string(B.frames.size()); out.push_back(d); }
		if (A.han2.patFlags != B.han2.patFlags) { DiffEntry d; d.pattern = p; d.what = "pattern flags changed"; out.push_back(d); }
		const size_t n = std::min(A.frames.size(), B.frames.size());
		for (size_t k = 0; k < n; k++) {
			const Frame &fa = A.frames[k], &fb = B.frames[k];
			DiffRecord(fa.han2.rec, fb.han2.rec, tbl, ntbl, "", p, (int)k, out);
			if (fa.han2.hadAT != fb.han2.hadAT) { DiffEntry d; d.pattern = p; d.frame = (int)k; d.what = fb.han2.hadAT ? "attack record added" : "attack record removed"; out.push_back(d); }
			else if (fb.han2.hadAT && !gof) DiffRecord(fa.han2.at, fb.han2.at, kRboAtRecordFields, (int)(sizeof(kRboAtRecordFields) / sizeof(kRboAtRecordFields[0])), "AT.", p, (int)k, out);
			else if (fb.han2.hadAT && memcmp(fa.han2.at, fb.han2.at, 236)) { DiffEntry d; d.pattern = p; d.frame = (int)k; d.what = "attack record bytes changed"; out.push_back(d); }
			for (auto &kv : fb.hitboxes) {
				auto it = fa.hitboxes.find(kv.first);
				char buf[160];
				if (it == fa.hitboxes.end()) { snprintf(buf, sizeof(buf), "box %d added (%d,%d,%d,%d)", kv.first, kv.second.xy[0], kv.second.xy[1], kv.second.xy[2], kv.second.xy[3]); DiffEntry d{p, (int)k, buf}; out.push_back(d); }
				else if (memcmp(kv.second.xy, it->second.xy, 16)) { snprintf(buf, sizeof(buf), "box %d (%d,%d,%d,%d) -> (%d,%d,%d,%d)", kv.first, it->second.xy[0], it->second.xy[1], it->second.xy[2], it->second.xy[3], kv.second.xy[0], kv.second.xy[1], kv.second.xy[2], kv.second.xy[3]); DiffEntry d{p, (int)k, buf}; out.push_back(d); }
			}
			for (auto &kv : fa.hitboxes) if (!fb.hitboxes.count(kv.first)) { char buf[64]; snprintf(buf, sizeof(buf), "box %d removed", kv.first); DiffEntry d{p, (int)k, buf}; out.push_back(d); }
			for (int s = 0; s < 3; s++) if (memcmp(fa.han2.script[s], fb.han2.script[s], 20) || ((fa.han2.scriptHad ^ fb.han2.scriptHad) >> s & 1)) { DiffEntry d{p, (int)k, std::string("script list ") + char('A' + s) + " changed"}; out.push_back(d); }
		}
	}
	return true;
}

} // namespace han2
