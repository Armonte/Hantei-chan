#include "han2_typed_files.h"
#include "han2/mbr_formats.h"
#include "han2/mbr_types_gen.h"
#include "han2/mb_formats.h"
#include "han2/mb_types_gen.h"
#include "han2/pb2k1_types_gen.h"
#include "misc.h"
#include <cstring>

namespace han2ui {

namespace {

std::string Upper(std::string s) { for (auto &c : s) if (c >= 'a' && c <= 'z') c -= 32; return s; }
std::string ExtOf(const std::string &n) { size_t d = n.find_last_of('.'); return d == std::string::npos ? "" : Upper(n.substr(d)); }
std::string BaseOf(const std::string &n) { size_t d = n.find_last_of("/\\"); return Upper(d == std::string::npos ? n : n.substr(d + 1)); }

const Han2FieldInfo kCountField[] = { {"count", 0, 4, 1, 0, nullptr, "number of defined entries"} };
const Han2FieldInfo kBaseSkillField[] = { {"baseSkill", 0, 4, 1, 1, nullptr, "CPU skill base 0..100 (CPU_SkillPercentFromBase)"} };
const Han2FieldInfo kCharaSelHeaderFields[] = {
	{"defaultCursorP1", 0, 4, 1, 0, nullptr, "default cursor character id of player 1"},
	{"defaultCursorP2", 4, 4, 1, 0, nullptr, "default cursor character id of player 2"},
	{"unused_08", 8, 4, 7, 0, nullptr, "never read"},
};

void Add(TypedFile &f, const char *title, size_t off, size_t stride, size_t count, const Han2FieldInfo *fld, int n, std::function<std::string(const uint8_t *, size_t)> label = nullptr, const char *group = "")
{
	TypedRegion r; r.title = title; r.offset = off; r.stride = stride; r.count = count; r.fields = fld; r.nfields = n; r.label = std::move(label); r.group = group; f.regions.push_back(std::move(r));
}

bool DescribeMbr(const std::string &name, const std::vector<uint8_t> &d, TypedFile &f)
{
	const std::string e = ExtOf(name); std::string err;
	if (e == ".CT" && d.size() == han2::mbr::kCtSize) {
		f.kind = "Re-ACT command table (<CHAR>_C.CT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "commands (200 x 44)", 4, sizeof(MbrCommandMove), 200, kMbrCommandMoveFields, (int)(sizeof(kMbrCommandMoveFields) / sizeof(kMbrCommandMoveFields[0])),
		    [](const uint8_t *r, size_t i) { char b[96]; if (r[0] == 0xFF) snprintf(b, sizeof b, "%3zu  (unused)", i); else { std::string seq; for (int k = 0; k < 16 && r[0x12 + k] != 0xFF; k++) { uint8_t c = r[0x12 + k]; seq += c < 10 ? char('0' + c) : (char)c; } snprintf(b, sizeof b, "%3zu  pattern %u  %s", i, r[0x22], seq.c_str()); } return std::string(b); });
		Add(f, "parameters", 4 + 200 * sizeof(MbrCommandMove), sizeof(MbrCtParams), 1, kMbrCtParamsFields, (int)(sizeof(kMbrCtParamsFields) / sizeof(kMbrCtParamsFields[0])));
		Add(f, "double-tap entries (6, 2, 4, 8)", 4 + 200 * sizeof(MbrCommandMove) + 0x1C, sizeof(MbrDoubleTapEntry), 4, kMbrDoubleTapEntryFields, (int)(sizeof(kMbrDoubleTapEntryFields) / sizeof(kMbrDoubleTapEntryFields[0])));
		return true;
	}
	if (e == ".CT" && BaseOf(name) == "CHARASELECT.CT") {
		han2::mbr::CharaSelectFile c; if (!han2::mbr::ParseCharaSelect(d.data(), d.size(), c, &err)) return false;
		f.kind = "Re-ACT character select table (enciphered)";
		std::vector<uint8_t> plain(d.begin(), d.end()); memcpy(plain.data() + 40, c.entries.data(), c.entries.size() * sizeof(MbrCharEntry)); f.work = plain;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "default cursors", 4, 36, 1, kCharaSelHeaderFields, 3);
		Add(f, "characters", 40, sizeof(MbrCharEntry), c.count, kMbrCharEntryFields, (int)(sizeof(kMbrCharEntryFields) / sizeof(kMbrCharEntryFields[0])),
		    [](const uint8_t *r, size_t i) { char b[96]; snprintf(b, sizeof b, "%2zu  %.40s", i, (const char *)r + 4); return std::string(b); });
		f.toStored = [](const std::vector<uint8_t> &w, std::vector<uint8_t> &out) {
			han2::mbr::CharaSelectFile c; c.count = *(const uint32_t *)w.data(); memcpy(c.header, w.data() + 4, 36); c.entries.resize(c.count);
			memcpy(c.entries.data(), w.data() + 40, (size_t)c.count * sizeof(MbrCharEntry)); han2::mbr::SerializeCharaSelect(c, out); };
		return true;
	}
	if (e == ".WMT") {
		han2::mbr::WmtFile w; if (!han2::mbr::ParseWmt(d.data(), d.size(), w, &err)) return false;
		f.kind = "Re-ACT win quotes (<CHAR>.WMT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "quotes", 4, sizeof(MbrWmtRecord), w.count, kMbrWmtRecordFields, (int)(sizeof(kMbrWmtRecordFields) / sizeof(kMbrWmtRecordFields[0])),
		    [](const uint8_t *r, size_t i) { char b[160]; snprintf(b, sizeof b, "%2zu  vs %s  %.60s", i, r[0] == 0xFF ? "any" : std::to_string(r[0]).c_str(), (const char *)r + 6); return sj2utf8(b); });
		return true;
	}
	if (e == ".CPF" && d.size() == sizeof(MbrCpfFile)) {
		f.kind = "Re-ACT CPU script (<CHAR>.CPF)"; f.work = d;
		Add(f, "base skill", 0, 4, 1, kBaseSkillField, 1);
		for (int i = 0; i < 100; i++) {
			char t[64], g[32]; snprintf(g, sizeof g, "script %d", i);
			const size_t base = 188 + (size_t)i * sizeof(MbrCpfScript);
			snprintf(t, sizeof t, "script %d: head", i);
			Add(f, t, base + 40 * sizeof(MbrCpfStep), sizeof(MbrCpfScriptHead), 1, kMbrCpfScriptHeadFields, (int)(sizeof(kMbrCpfScriptHeadFields) / sizeof(kMbrCpfScriptHeadFields[0])), nullptr, g);
			snprintf(t, sizeof t, "script %d: steps", i);
			Add(f, t, base, sizeof(MbrCpfStep), 40, kMbrCpfStepFields, (int)(sizeof(kMbrCpfStepFields) / sizeof(kMbrCpfStepFields[0])),
			    [](const uint8_t *r, size_t k) { char b[96]; uint32_t fl; memcpy(&fl, r + 0x18, 4); int16_t dur; memcpy(&dur, r + 2, 2); snprintf(b, sizeof b, "step %2zu  input %u  dur %d%s", k, r[0], dur, (fl & 3) ? "  (end)" : ""); return std::string(b); }, g);
		}
		return true;
	}
	return false;
}

bool DescribeMb(const std::string &name, const std::vector<uint8_t> &d, TypedFile &f)
{
	const std::string e = ExtOf(name); std::string err;
	if ((e == ".CT" || e == ".CT2") && d.size() == sizeof(MbCtFile)) {
		f.kind = "Melty Blood command table (<CHAR>_C.CT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "commands (100 x 44)", 4, sizeof(MbCtCommand), 100, kMbCtCommandFields, (int)(sizeof(kMbCtCommandFields) / sizeof(kMbCtCommandFields[0])),
		    [](const uint8_t *r, size_t i) { char b[96]; if (r[0] == 0xFF) snprintf(b, sizeof b, "%3zu  (unused)", i); else { std::string seq; for (int k = 0; k < 32 && r[2 + k] != 0xFF; k++) { uint8_t c = r[2 + k]; seq += c < 10 ? char('0' + c) : (char)c; } snprintf(b, sizeof b, "%3zu  pattern %u  %s", i, r[0x22], seq.c_str()); } return std::string(b); });
		Add(f, "parameters", sizeof(MbCtFile) - sizeof(MbCtHeader), sizeof(MbCtHeader), 1, kMbCtHeaderFields, (int)(sizeof(kMbCtHeaderFields) / sizeof(kMbCtHeaderFields[0])));
		Add(f, "double-tap entries (6, 2, 4, 8)", sizeof(MbCtFile) - sizeof(MbCtHeader) + 0x1C, sizeof(MbCtEvadeEntry), 4, kMbCtEvadeEntryFields, (int)(sizeof(kMbCtEvadeEntryFields) / sizeof(kMbCtEvadeEntryFields[0])));
		return true;
	}
	if (e == ".CT" && BaseOf(name) == "CHARSEL.CT") {
		han2::mb::CharSelFile c; if (!han2::mb::ParseCharSel(d.data(), d.size(), c, &err)) return false;
		f.kind = "Melty Blood character select table (enciphered)";
		std::vector<uint8_t> plain(d.begin(), d.end()); memcpy(plain.data() + 4, c.entries.data(), c.entries.size() * sizeof(MbCharSelEntry)); f.work = plain;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "characters", 4, sizeof(MbCharSelEntry), c.count, kMbCharSelEntryFields, (int)(sizeof(kMbCharSelEntryFields) / sizeof(kMbCharSelEntryFields[0])),
		    [](const uint8_t *r, size_t i) { char b[96]; snprintf(b, sizeof b, "%2zu  %.30s", i, (const char *)r); return std::string(b); });
		f.toStored = [](const std::vector<uint8_t> &w, std::vector<uint8_t> &out) {
			han2::mb::CharSelFile c; c.count = *(const uint32_t *)w.data(); c.entries.resize(c.count); memcpy(c.entries.data(), w.data() + 4, (size_t)c.count * sizeof(MbCharSelEntry)); han2::mb::SerializeCharSel(c, out); };
		return true;
	}
	if (e == ".WMT" && d.size() >= 4 && d.size() == 4 + 156 * (size_t)*(const uint32_t *)d.data()) {
		const uint32_t n = *(const uint32_t *)d.data();
		f.kind = "Melty Blood win messages (.WMT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "records", 4, sizeof(MbWmtRecord), n, kMbWmtRecordFields, (int)(sizeof(kMbWmtRecordFields) / sizeof(kMbWmtRecordFields[0])),
		    [](const uint8_t *r, size_t i) { char b[64]; snprintf(b, sizeof b, "%2zu  vs %s", i, r[0] == 0xFF ? "any" : std::to_string(r[0]).c_str()); return std::string(b); });
		return true;
	}
	if (e == ".CPF" && d.size() == sizeof(MbCpfFile)) {
		f.kind = "Melty Blood CPU script (.CPF)"; f.work = d;
		Add(f, "guard percent", 0, 4, 1, kBaseSkillField, 1);
		for (int i = 0; i < 100; i++) {
			char t[64], g[32]; snprintf(g, sizeof g, "script %d", i);
			const size_t base = 188 + (size_t)i * sizeof(MbCpfScript);
			snprintf(t, sizeof t, "script %d: condition", i);
			Add(f, t, base + 40 * sizeof(MbCpfStep), sizeof(MbCpfCondition), 1, kMbCpfConditionFields, (int)(sizeof(kMbCpfConditionFields) / sizeof(kMbCpfConditionFields[0])), nullptr, g);
			snprintf(t, sizeof t, "script %d: steps", i);
			Add(f, t, base, sizeof(MbCpfStep), 40, kMbCpfStepFields, (int)(sizeof(kMbCpfStepFields) / sizeof(kMbCpfStepFields[0])),
			    [](const uint8_t *r, size_t k) { char b[96]; uint16_t code, dur; uint32_t fl; memcpy(&code, r, 2); memcpy(&dur, r + 2, 2); memcpy(&fl, r + 0x18, 4); snprintf(b, sizeof b, "step %2zu  input %u  dur %u%s", k, code, dur, (fl & 3) ? "  (end)" : ""); return std::string(b); }, g);
		}
		return true;
	}
	return false;
}

bool DescribePb(const std::string &name, const std::vector<uint8_t> &d, TypedFile &f)
{
	const std::string e = ExtOf(name);
	if ((e == ".CT" || e == ".CCT") && d.size() == sizeof(Pb2CtFile)) {
		f.kind = "Party Breakers command table (<CHAR>_C.CT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "commands (100 x 42)", 4, sizeof(Pb2CommandMove), 100, kPb2CommandMoveFields, (int)(sizeof(kPb2CommandMoveFields) / sizeof(kPb2CommandMoveFields[0])),
		    [](const uint8_t *r, size_t i) { char b[96]; if (r[0] == 0xFF) snprintf(b, sizeof b, "%3zu  (unused)", i); else { std::string seq; for (int k = 0; k < 32 && r[2 + k] != 0xFF; k++) { uint8_t c = r[2 + k]; seq += c < 10 ? char('0' + c) : (char)c; } snprintf(b, sizeof b, "%3zu  pattern %u  %s", i, r[0x22], seq.c_str()); } return std::string(b); });
		Add(f, "parameters", 4 + 100 * sizeof(Pb2CommandMove), sizeof(Pb2CtHeader), 1, kPb2CtHeaderFields, (int)(sizeof(kPb2CtHeaderFields) / sizeof(kPb2CtHeaderFields[0])));
		return true;
	}
	if (e == ".WMT" && d.size() >= 4 && d.size() == 4 + sizeof(Pb2WmtRecord) * (size_t)*(const uint32_t *)d.data()) {
		f.kind = "Party Breakers win quotes (.WMT)"; f.work = d;
		Add(f, "header", 0, 4, 1, kCountField, 1);
		Add(f, "records", 4, sizeof(Pb2WmtRecord), *(const uint32_t *)d.data(), kPb2WmtRecordFields, (int)(sizeof(kPb2WmtRecordFields) / sizeof(kPb2WmtRecordFields[0])),
		    [](const uint8_t *r, size_t i) { char b[160]; snprintf(b, sizeof b, "%2zu  vs %s  %.60s", i, r[0] == 0xFF ? "any" : std::to_string(r[0]).c_str(), (const char *)r + 4); return sj2utf8(b); });
		return true;
	}
	return false;
}

} // namespace

bool DescribeTypedFile(const std::string &name, const std::vector<uint8_t> &stored, TypedFile &out, const std::string &origin)
{
	out = TypedFile();
	const bool mbFirst = origin.find("data0") != std::string::npos;   // MeltyBlood\data00..03.p
	for (int pass = 0; pass < 2; pass++) {
		const bool mb = (pass == 0) == mbFirst;
		out = TypedFile();
		if (mb ? DescribeMb(name, stored, out) : DescribeMbr(name, stored, out)) return true;
	}
	out = TypedFile();
	if (DescribePb(name, stored, out)) return true;
	out = TypedFile();
	return false;
}

void TypedFileStored(const TypedFile &f, std::vector<uint8_t> &stored)
{
	if (f.toStored) f.toStored(f.work, stored); else stored = f.work;
}

} // namespace han2ui
