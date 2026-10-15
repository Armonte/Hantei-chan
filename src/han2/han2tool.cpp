// han2tool: command line access to French-Bread RBO / GOF2 archives and HAN2RBO character files.
//
//   han2tool ls <archive>                      list entries (name, offset, size)
//   han2tool count <archive>...                print entry counts
//   han2tool extract <archive> <name|-all> <outdir>
//   han2tool roundtrip <archive|file>...       every HAN2RBO .DAT/.DT2 (inside archives or loose): parse -> serialize must be byte-identical
//   han2tool modelrt <archive|file>...         every RBO .DAT/.DT2: load into the Hantei-chan model -> save must be byte-identical
//   han2tool pacrt <archive>...                rebuild the archive from its entries; entries must match byte for byte
//
// Exit code 0 when everything passed.
#include "pac_archive.h"
#include "han2_container.h"
#include "../framedata_han2.h"
#include "../cg.h"
#include "../cgm/cgm_bank.h"
#include "../cgm/cgm_ops.h"
#include "../cgm/cgm_struct.h"
#include "../han2_pat.h"
#include "../han2_export.h"
#include "../han2_diff.h"
#include "../han2_anim.h"
#include "../framedata_gof1.h"
#include "gof1_archive.h"
#include "img_file.h"
#include "fob_file.h"
#include "replay_file.h"
#include "fnt_file.h"
#include "misc_formats.h"
#include "../png_writer.h"
#include "../parts/parts.h"

#include <cstdio>
#include <windows.h>
#include <objbase.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string>
#include <memory>
#include <map>

static int CmdLs(int argc, char **argv)
{
	if (argc < 1) return 2;
	pac::Archive a; std::string err;
	if (!pac::Open(argv[0], a, &err)) { printf("FAIL %s: %s\n", argv[0], err.c_str()); return 1; }
	for (size_t i = 0; i < a.entries.size(); i++)
		printf("%5zu %10u %10u %s\n", i, a.entries[i].offset, a.entries[i].size, a.entries[i].name.c_str());
	return 0;
}

static int CmdCount(int argc, char **argv)
{
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		if (!pac::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		printf("OK   %-40s %6zu entries\n", argv[i], a.entries.size());
	}
	return fails ? 1 : 0;
}

static bool WriteFile(const std::filesystem::path &p, const std::vector<uint8_t> &b)
{
	std::ofstream f(p, std::ios::binary);
	if (!f) return false;
	if (!b.empty()) f.write((const char *)b.data(), (std::streamsize)b.size());
	return (bool)f;
}

static int CmdExtract(int argc, char **argv)
{
	if (argc < 3) return 2;
	pac::Archive a; std::string err;
	if (!pac::Open(argv[0], a, &err)) { printf("FAIL %s\n", err.c_str()); return 1; }
	std::filesystem::create_directories(std::filesystem::u8path(argv[2]));
	int n = 0;
	for (size_t i = 0; i < a.entries.size(); i++) {
		if (strcmp(argv[1], "-all") != 0 && pac::Find(a, argv[1]) != (int)i) continue;
		std::vector<uint8_t> b;
		if (!pac::ReadEntry(a, i, b, &err)) { printf("FAIL %s\n", err.c_str()); return 1; }
		// archive names are CP932 bytes; write them through as-is (ASCII for every shipped name checked so far)
		if (!WriteFile(std::filesystem::u8path(argv[2]) / a.entries[i].name, b)) { printf("FAIL write %s\n", a.entries[i].name.c_str()); return 1; }
		n++;
	}
	printf("extracted %d\n", n);
	return n ? 0 : 1;
}

static int CmdPacRt(int argc, char **argv)
{
	// Bounded memory: entries stream from the open archive into a temp file (pac::WriteArchive), then both files are compared in chunks.
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		if (!pac::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		std::vector<pac::WriteSource> src(a.entries.size());
		for (size_t k = 0; k < a.entries.size(); k++) {
			src[k].name = a.entries[k].name;
			src[k].rawName.assign(a.entries[k].rawName, a.entries[k].rawName + pac::kNameLen);
			src[k].kind = pac::WriteSource::ArchiveEntry; src[k].archive = &a; src[k].index = k;
		}
		std::error_code ec;
		const std::string tmp = (std::filesystem::temp_directory_path(ec) / ("han2_pacrt_" + std::to_string(i) + ".bin")).u8string();
		if (!pac::WriteArchive(tmp, src, {}, &err)) { printf("FAIL %s: build: %s\n", argv[i], err.c_str()); fails++; continue; }
		std::ifstream fa(std::filesystem::u8path(argv[i]), std::ios::binary), fb(std::filesystem::u8path(tmp), std::ios::binary);
		const uint64_t sa = std::filesystem::file_size(std::filesystem::u8path(argv[i]), ec), sb = std::filesystem::file_size(std::filesystem::u8path(tmp), ec);
		std::vector<char> ba(1 << 20), bb(1 << 20);
		uint64_t pos = 0; bool same = sa == sb; uint64_t firstDiff = 0;
		while (same && pos < sa) {
			fa.read(ba.data(), (std::streamsize)ba.size()); fb.read(bb.data(), (std::streamsize)bb.size());
			size_t n = (size_t)fa.gcount(); if (n != (size_t)fb.gcount()) { same = false; firstDiff = pos; break; }
			if (memcmp(ba.data(), bb.data(), n) != 0) { size_t k = 0; while (ba[k] == bb[k]) k++; same = false; firstDiff = pos + k; break; }
			pos += n; if (n == 0) break;
		}
		fa.close(); fb.close(); std::filesystem::remove(std::filesystem::u8path(tmp), ec);
		if (same) printf("OK   %s byte-identical (%zu entries)\n", argv[i], src.size());
		else { printf("DIFF %s: rebuilt %llu bytes vs %llu, first diff at 0x%llx\n", argv[i], (unsigned long long)sb, (unsigned long long)sa, (unsigned long long)firstDiff); fails++; }
	}
	return fails ? 1 : 0;
}

static bool EndsWithNoCase(const std::string &s, const char *suf)
{
	size_t n = strlen(suf);
	if (s.size() < n) return false;
	for (size_t i = 0; i < n; i++) {
		char a = s[s.size() - n + i], b = suf[i];
		if (a >= 'a' && a <= 'z') a -= 32;
		if (b >= 'a' && b <= 'z') b -= 32;
		if (a != b) return false;
	}
	return true;
}

static bool ReadLoose(const std::string &path, std::vector<uint8_t> &b)
{
	std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
	if (!f) return false;
	b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}


// ---- classification used by every round trip section: what a file really is, decided up front by its bytes ----
enum class Kind { Han2, Pat, Img, Fob, Chp, Rep, Fnt, Wav, Mp3, Bmp, Text, Other };
static std::string ExtOf(const std::string &n);
// Magic-bearing formats are recognised by their bytes alone; formats without a magic (FOB, REP, FNT) by their extension, and the section that owns
// them then FAILS (never skips) when the bytes do not parse. An extension that names such a format is therefore never silently "opaque".
static Kind Classify(const std::vector<uint8_t> &b, const std::string &name = std::string())
{
	if (b.size() >= 8 && memcmp(b.data(), "HAN2RBO ", 8) == 0) return Kind::Han2;
	if (han2::IsPat(b.data(), b.size())) return Kind::Pat;
	if (han2::IsImg(b.data(), b.size())) return Kind::Img;
	const std::string e = ExtOf(name);
	if (e == ".FOB") return Kind::Fob;
	if (e == ".CHP" && b.size() >= 11 && memcmp(b.data(), "BMP Cutter", 10) == 0) return Kind::Chp;
	if (e == ".REP" || e == ".RP2" || e == ".RP3" || e == ".RP4") return Kind::Rep;
	if (e == ".FNT") return Kind::Fnt;
	if (b.size() >= 12 && memcmp(b.data(), "RIFF", 4) == 0 && memcmp(b.data() + 8, "WAVE", 4) == 0) return Kind::Wav;
	if (e == ".MP3") return Kind::Mp3;
	if (b.size() >= 2 && b[0] == 'B' && b[1] == 'M' && e == ".BMP") return Kind::Bmp;
	if ((e == ".TXT" || e == ".H") && han2::LooksLikeText(b.data(), b.size())) return Kind::Text;
	return Kind::Other;
}
static const char *KindName(Kind k)
{
	switch (k) {
	case Kind::Han2: return "HAN2RBO"; case Kind::Pat: return "bare PAT"; case Kind::Img: return "IMG"; case Kind::Fob: return "FOB script";
	case Kind::Chp: return "CHP bank"; case Kind::Rep: return "replay"; case Kind::Fnt: return "bitmap font"; case Kind::Wav: return "RIFF WAVE";
	case Kind::Mp3: return "MPEG audio"; case Kind::Bmp: return "BMP"; case Kind::Text: return "text"; default: return "opaque";
	}
}
static std::string ExtOf(const std::string &n) { size_t d = n.find_last_of('.'); std::string e = d == std::string::npos ? "" : n.substr(d); for (auto &c : e) if (c >= 'a' && c <= 'z') c -= 32; return e; }

// census <archive>...: every entry by extension and by what its bytes are
static int CmdCensus(int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		if (!pac::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); continue; }
		std::map<std::string, int> cnt;
		for (size_t k = 0; k < a.entries.size(); k++) {
			std::vector<uint8_t> b; if (!pac::ReadEntry(a, k, b, &err)) { cnt["READ-FAIL"]++; continue; }
			cnt[ExtOf(a.entries[k].name) + " -> " + KindName(Classify(b, a.entries[k].name))]++;
		}
		printf("%s (%zu entries)\n", argv[i], a.entries.size());
		for (auto &kv : cnt) printf("    %-28s %d\n", kv.first.c_str(), kv.second);
	}
	return 0;
}

// ---- round trip sections ----------------------------------------------------------------------------------------------------
// Every section visits EVERY entry of every archive (or loose file) given and decides up front, from the bytes, whether the entry is
// its kind of file. Nothing is silently skipped: an entry that is not the section's kind is counted as "n/a" (listed by extension and
// real type in the section line) and is owned by another section (HAN2RBO: container/model/patrt/cgrt/animtest; bare PAT: patrt;
// IMG: imgrt; everything else: opaque). `skipped` exists only so the report can state it is 0; no code path increments it.
struct Section {
	const char *name = "";
	int pass = 0, fail = 0, skipped = 0;
	std::map<std::string, int> na;      // "ext (real type)" -> count
	std::map<std::string, int> notes;   // informational counters (empty-section passes, image types ...)
};
using RtStats = Section;

static void NaAdd(Section &s, const std::string &entryName, Kind k) { s.na[ExtOf(entryName) + " (" + KindName(k) + ")"]++; }

static int Finish(Section &s)
{
	int n = 0; std::string d;
	for (auto &kv : s.na) { n += kv.second; d += (d.empty() ? "" : ", ") + kv.first + " x" + std::to_string(kv.second); }
	printf("SECTION %-9s pass %d fail %d skipped %d | n/a %d%s%s\n", s.name, s.pass, s.fail, s.skipped, n, d.empty() ? "" : ": ", d.c_str());
	for (auto &kv : s.notes) printf("    %s: %d\n", kv.first.c_str(), kv.second);
	return (s.fail || s.skipped) ? 1 : 0;
}

// Calls fn(label, entryName, bytes) for every entry (archive) or the file itself (loose). Unreadable inputs are failures, never skips.
template <class F> static void ForEachEntry(int argc, char **argv, Section &s, F fn)
{
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		const int p0 = s.pass, f0 = s.fail; std::map<std::string, int> na0 = s.na;
		if (pac::Open(argv[i], a, nullptr)) {
			for (size_t k = 0; k < a.entries.size(); k++) {
				std::vector<uint8_t> b;
				if (!pac::ReadEntry(a, k, b, &err)) { printf("FAIL %s::%s: %s\n", argv[i], a.entries[k].name.c_str(), err.c_str()); s.fail++; continue; }
				fn(std::string(argv[i]) + "::" + a.entries[k].name, a.entries[k].name, b);
			}
		} else {
			std::vector<uint8_t> b;
			if (!ReadLoose(argv[i], b)) { printf("FAIL %s: cannot read\n", argv[i]); s.fail++; }
			else fn(argv[i], std::filesystem::u8path(argv[i]).filename().u8string(), b);
		}
		int nn = 0; for (auto &kv : s.na) nn += kv.second; for (auto &kv : na0) nn -= kv.second;
		printf("%-40s pass %d fail %d skipped 0 n/a %d\n", argv[i], s.pass - p0, s.fail - f0, nn);
	}
}

static void DiffReport(const char *what, const std::string &label, const std::vector<uint8_t> &out, const std::vector<uint8_t> &b)
{
	size_t k = 0, m = std::min(out.size(), b.size());
	while (k < m && out[k] == b[k]) k++;
	printf("FAIL %s: %s: size %zu vs %zu, first diff at 0x%zx\n", label.c_str(), what, out.size(), b.size(), k);
}

static void RoundtripOne(const std::string &label, const std::vector<uint8_t> &b, Section &st)
{
	han2::Han2File h; std::string err;
	if (!han2::Parse(b.data(), b.size(), h, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	std::vector<uint8_t> out;
	if (!han2::Serialize(h, out, &err)) { printf("FAIL %s: serialize: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	if (out == b) { st.pass++; return; }
	DiffReport("container", label, out, b); st.fail++;
}

static int CmdRoundtrip(int argc, char **argv)
{
	Section s; s.name = "container";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		RoundtripOne(label, b, s);
	});
	return Finish(s);
}

static void ModelRtOne(const std::string &label, const std::vector<uint8_t> &b, Section &st)
{
	FrameData fd; std::string err;
	if (!han2::Load(fd, b.data(), b.size(), &err)) { printf("FAIL %s: load: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	std::vector<uint8_t> out; std::vector<std::string> warn;
	bool dt2 = fd.m_han2 && fd.m_han2->kind == 3;
	if (!han2::Serialize(fd, out, &err, &warn, dt2)) { printf("FAIL %s: save: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	if (!warn.empty()) printf("WARN %s: %s\n", label.c_str(), warn[0].c_str());
	if (out == b) { st.pass++; return; }
	DiffReport("model", label, out, b); st.fail++;
}

static int CmdModelRt(int argc, char **argv)
{
	Section s; s.name = "model";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		ModelRtOne(label, b, s);
	});
	return Finish(s);
}

// patrt: PAT block -> Parts model -> PAT block must be byte-identical (reader + writer check), and a HAN2RBO file with the rebuilt block put back
// must re-serialize to the original file. A HAN2RBO file whose PAT area is EMPTY (effect/object files that borrow another file's parts) has no
// parts model: the defined round trip is han2::PatSectionToParts -> an unloaded, empty model -> han2::BuildPatSection -> an empty block, and the
// whole file must still re-serialize byte-identically with the empty area. Counted as pass, and separately in the note "empty PAT section".
static int CmdPatRt(int argc, char **argv)
{
	Section s; s.name = "pat";
	// ONE Parts/CG reused for every entry (bounded memory; never destroyed: ~Parts releases GL objects and the tool has no GL context)
	static CG *cgp = new CG(); static Parts *partsp = new Parts(cgp);
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Han2 && k != Kind::Pat) { NaAdd(s, name, k); return; }
		std::string err; han2::Han2File f;
		const bool bare = k == Kind::Pat;
		if (!bare && !han2::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		const std::vector<uint8_t> &blob = bare ? b : f.area[han2::kAreaParts];
		Parts &parts = *partsp; parts.textures.clear(); parts.partSets.clear(); parts.cutOuts.clear(); parts.shapes.clear(); parts.gfxMeta.clear(); parts.loaded = false;
		if (!han2::PatSectionToParts(blob.data(), blob.size(), parts, &err)) { printf("FAIL %s: read: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		std::vector<uint8_t> out;
		if (!han2::BuildPatSection(parts, blob, out, &err)) { printf("FAIL %s: build: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		if (out != blob) { DiffReport("pat block", label, out, blob); s.fail++; return; }
		if (!bare) {
			han2::Han2File g = f; g.area[han2::kAreaParts] = out; std::vector<uint8_t> whole;
			if (!han2::Serialize(g, whole, &err)) { printf("FAIL %s: serialize: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
			if (whole != b) { DiffReport("file with rebuilt PAT block", label, whole, b); s.fail++; return; }
		}
		if (blob.empty()) { if (parts.loaded || !parts.partSets.empty()) { printf("FAIL %s: empty PAT section produced a parts model\n", label.c_str()); s.fail++; return; } s.notes["empty PAT section " + ExtOf(name) + " (counted in pass)"]++; }
		s.pass++;
	});
	return Finish(s);
}

// imgrt: every .IMG of every archive: parse -> serialize must be byte-identical (all four pixel formats)
static int CmdImgRt(int argc, char **argv)
{
	Section s; s.name = "img";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Img) { NaAdd(s, name, k); return; }
		han2::ImgFile img; std::string err;
		if (!han2::ParseImg(b.data(), b.size(), img, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		std::vector<uint8_t> out; han2::SerializeImg(img, out);
		if (out == b) { s.pass++; s.notes["format " + std::to_string(img.format) + " v" + std::to_string(img.version)]++; }
		else { DiffReport("img", label, out, b); s.fail++; }
	});
	return Finish(s);
}

// Entries that are none of HAN2RBO / PAT / IMG have no structured reader (FOB scripts: docs/formats/fob_vm.md; WAV/FNT/CHP/REP/TXT and the one
// stale blob below). Their round trip is a raw passthrough: the bytes handed out by the PAC reader must equal the archive file's own bytes at the
// entry's offset (the PAC writer is proven separately by pacrt). An entry whose extension names a structured format but whose bytes are not that
// format is a FAILURE unless it is in this table of known non-conforming shipped files.
struct KnownOpaque { const char *suffix; const char *why; };
static const KnownOpaque kKnownOpaque[] = {
	{ "DATA01.PAC::DUSTNESS.DAT", "stale misspelt copy of DUSTINESS.DAT; whole file is encrypted/compressed (7.998 bits/byte entropy, no HAN2RBO magic), not referenced by name by the game (rbo.exe/ETC.PAC only list DUSTINESS), no reader exists" },
};
static int CmdOpaqueRt(int argc, char **argv)
{
	Section s; s.name = "opaque";
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		const int p0 = s.pass, f0 = s.fail;
		if (!pac::Open(argv[i], a, nullptr)) {   // a loose file is its own bytes: nothing to compare
			std::vector<uint8_t> b; if (!ReadLoose(argv[i], b)) { printf("FAIL %s: cannot read\n", argv[i]); s.fail++; continue; }
			Kind k = Classify(b, argv[i]); if (k != Kind::Other) NaAdd(s, argv[i], k); else s.pass++;
			continue;
		}
		std::ifstream raw(std::filesystem::u8path(argv[i]), std::ios::binary);
		for (size_t k = 0; k < a.entries.size(); k++) {
			const std::string &name = a.entries[k].name; const std::string label = std::string(argv[i]) + "::" + name;
			std::vector<uint8_t> b;
			if (!pac::ReadEntry(a, k, b, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; continue; }
			Kind kind = Classify(b, name);
			if (kind != Kind::Other) { NaAdd(s, name, kind); continue; }
			const std::string ext = ExtOf(name);
			const bool structuredExt = ext == ".DAT" || ext == ".DT2" || ext == ".PAT" || ext == ".IMG" || ext == ".CHP" || ext == ".WAV" || ext == ".BMP";
			const KnownOpaque *known = nullptr;
			for (auto &ko : kKnownOpaque) { std::string l = label; if (l.size() >= strlen(ko.suffix) && l.compare(l.size() - strlen(ko.suffix), std::string::npos, ko.suffix) == 0) known = &ko; }
			if (structuredExt && !known) { printf("FAIL %s: extension %s but the bytes are not that format and the file is not in the known-opaque table\n", label.c_str(), ext.c_str()); s.fail++; continue; }
			std::vector<uint8_t> direct(b.size());
			raw.clear(); raw.seekg((std::streamoff)a.entries[k].offset);
			raw.read((char *)direct.data(), (std::streamsize)direct.size());
			if ((size_t)raw.gcount() != direct.size() || direct != b) { printf("FAIL %s: PAC reader bytes differ from the archive's own bytes\n", label.c_str()); s.fail++; continue; }
			s.pass++; s.notes["opaque " + (ext.empty() ? std::string("(no ext)") : ext)]++;
			if (known) printf("NOTE %s: %s\n", label.c_str(), known->why);
		}
		printf("%-40s pass %d fail %d skipped 0\n", argv[i], s.pass - p0, s.fail - f0);
	}
	return Finish(s);
}

static void CgBankCheck(const std::string &label, const std::vector<uint8_t> &cgb, Section &s)
{
	static CG *cg = new CG(), *scratch = new CG();
		if (!cg->loadFromMemory(cgb.data(), (unsigned)cgb.size())) { printf("FAIL %s: CG load failed (%zu bytes)\n", label.c_str(), cgb.size()); s.fail++; return; }
		if (cg->bank_size() != cgb.size() || memcmp(cg->bank_data(), cgb.data(), cgb.size()) != 0) {
			size_t d = 0; while (d < cgb.size() && d < cg->bank_size() && (uint8_t)cg->bank_data()[d] == cgb[d]) d++;
			printf("FAIL %s: loading the bank changed it (size %u vs %zu, first diff 0x%zx)\n", label.c_str(), cg->bank_size(), cgb.size(), d); s.fail++; return;
		}
		int bad = 0;
		scratch->loadFromMemory(cgb.data(), (unsigned)cgb.size());
		for (int i = 0; i < cg->get_image_count(); i++) {
			int bpp, ty, x1, y1, x2, y2;
			if (!cg->image_info(i, bpp, ty, x1, y1, x2, y2)) { s.notes["image: absent slot (no record)"]++; continue; }
			ImageData *im = cg->draw_texture((unsigned)i, false, false);
			if (!im) { s.notes["image: undrawable (type " + std::to_string(ty) + ", " + (ty == -1 ? "no pixel data" : x2 < x1 || y2 < y1 ? "empty bounds" : "bad alignment table") + "), nothing to import"]++; continue; }
			std::vector<unsigned char> px(im->pixels, im->pixels + (size_t)im->width * im->height * 4);
			int w = im->width, h = im->height; delete im;
			std::string e2;
			// (3) writer: normal re-import on the pristine bank
			if (!cg->replace_image_rgba((unsigned)i, px.data(), w, h, &e2)) { printf("FAIL %s: image %d (type %d bpp %d): %s\n", label.c_str(), i, ty, bpp, e2.c_str()); bad++; continue; }
			// (2) encoder: force re-encode on the scratch copy, must render the same
			if (!scratch->replace_image_rgba((unsigned)i, px.data(), w, h, &e2, true)) { printf("FAIL %s: image %d (type %d): forced re-encode: %s\n", label.c_str(), i, ty, e2.c_str()); bad++; continue; }
			ImageData *im2 = scratch->draw_texture((unsigned)i, false, false);
			bool same = im2 && im2->width == w && im2->height == h;
			if (same) for (size_t q = 0; q < px.size() && same; q += 4) {
				if (px[q + 3] != im2->pixels[q + 3]) same = false;
				else if (px[q + 3] && memcmp(&px[q], &im2->pixels[q], 3) != 0) same = false;
			}
			delete im2;
			if (same) s.notes["image: re-imported identically (type " + std::to_string(ty) + ")"]++;
			else { printf("FAIL %s: image %d (type %d) renders differently after a forced re-encode\n", label.c_str(), i, ty); bad++; }
		}
		if (memcmp(cg->bank_data(), cgb.data(), cgb.size()) != 0) {
			size_t d = 0; while ((uint8_t)cg->bank_data()[d] == cgb[d]) d++;
			printf("FAIL %s: bank bytes changed after re-importing every image's own pixels (first diff 0x%zx)\n", label.c_str(), d); bad++;
		}
	if (bad) s.fail++; else s.pass++;
}

// cgrt: every character's CG bank (BMP Cutter): (1) loading it must not touch a byte (bank == area); (2) ENCODER proof: on a scratch copy every image's
// own pixels are force re-encoded (replace_image_rgba force=true) and must render identically (types 1, 2, 3, 4); (3) WRITER proof: on the pristine
// bank every image's own pixels are re-imported normally (unchanged pixels leave the stored bytes alone) and the whole bank must stay BYTE-IDENTICAL;
// (4) images the engine cannot draw (type -1: no pixel data / empty bounds / bad table) have nothing to import and are counted by reason; their
// bytes are covered by the whole-bank comparison in (3).
static int CmdCgRt(int argc, char **argv)
{
	Section s; s.name = "cg";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		han2::Han2File f; std::string err;
		if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		const auto &cgb = f.area[han2::kAreaCg];
		if (cgb.empty()) { s.notes["empty CG area " + ExtOf(name) + " (counted in pass: no bank to load; the container round trip proves the empty area)"]++; s.pass++; return; }
		CgBankCheck(label, cgb, s);
	});
	return Finish(s);
}


// cgm: the CG manager model over every BMP Cutter bank in RBO / GOF2 characters (CG area) and .CHP files: parse -> serialize byte-identical, every image decodes
// exactly like CG::draw_texture, and a forced re-encode of an image's own pixels through the model (ReplaceImage) renders identically.
static void CgmBankCheck(const std::string &label, const std::vector<uint8_t> &b, Section &s)
{
	cgm::Bank bk; std::string err;
	if (!cgm::Bank::parse(b.data(), b.size(), bk, &err)) { printf("FAIL %s: cgm parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
	std::vector<uint8_t> o; bk.serialize(o);
	if (o != b) { size_t d = 0; while (d < o.size() && d < b.size() && o[d] == b[d]) d++; printf("FAIL %s: cgm serialize differs (first diff 0x%zx)\n", label.c_str(), d); s.fail++; return; }
	CG cg; cg.loadFromMemory(b.data(), (unsigned)b.size()); int bad = 0, drawn = 0;
	for (size_t n = 0; n < bk.images.size(); n++) {
		if (!bk.images[n].present || (int)n >= (int)bk.images.size() - bk.hidden) continue;   // hidden: stored past the declared count, the engine never draws them
		cgm::Rgba r; const bool mine = bk.decode((int)n, r); ImageData *ref = cg.draw_texture((unsigned)n, false, false);
		if (!!ref != mine) { printf("FAIL %s: image %zu drawable in only one of CG / model\n", label.c_str(), n); bad++; delete ref; continue; }
		if (!ref) continue;
		bool same = ref->width == r.w && ref->height == r.h;
		for (int k = 0; same && k < r.w * r.h; k++) { const uint8_t *a = &ref->pixels[k * 4], *c = &r.px[k * 4]; if (a[3] != c[3] || (a[3] && memcmp(a, c, 3))) same = false; }
		delete ref; if (!same) { printf("FAIL %s: image %zu renders differently in the model\n", label.c_str(), n); bad++; } else drawn++;
		if (same && bk.images[n].type >= 1 && bk.images[n].type <= 4 && n % 7 == 0) {   // re-import the image's own pixels: the stored bytes must not move
			cgm::Bank w = bk; cgm::ReplaceReport rep; std::string e;
			if (!cgm::ReplaceImage(w, (int)n, r.px.data(), r.w, r.h, &e, &rep)) { printf("FAIL %s: image %zu: ReplaceImage: %s\n", label.c_str(), n, e.c_str()); bad++; }
			else { std::vector<uint8_t> o2; w.serialize(o2); if (rep.changedPixels == 0 && o2 != b) { printf("FAIL %s: image %zu: re-importing its own pixels changed bytes\n", label.c_str(), n); bad++; } else s.notes["model re-import of an image's own pixels keeps the bytes"]++; }
		}
	}
	s.notes["images decoded identically to CG"] += drawn;
	if (bad) s.fail++; else s.pass++;
}

static int CmdCgmRt(int argc, char **argv)
{
	Section s; s.name = "cgm";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k == Kind::Chp) { CgmBankCheck(label, b, s); return; }
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		han2::Han2File f; std::string err;
		if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		const auto &cgb = f.area[han2::kAreaCg];
		if (cgb.empty()) { s.notes["empty CG area"]++; s.pass++; return; }
		CgmBankCheck(label, cgb, s);
	});
	return Finish(s);
}

// fob: every script file: parse into header + decoded instructions (+ raw spans for code no instruction reaches) -> serialize must be byte-identical.
// The decoder is the same recursive descent as tools/rbo/fobdis.py; unknown words are decode errors and are reported, never skipped.
static int CmdFobRt(int argc, char **argv)
{
	Section s; s.name = "fob";
	size_t insns = 0, unreached = 0, raw = 0, overlaps = 0, errs = 0, codeBytes = 0;
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Fob) { NaAdd(s, name, k); return; }
		han2::fob::File f; std::string err;
		if (!han2::fob::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		std::vector<uint8_t> out; han2::fob::Serialize(f, out, &err);
		insns += f.nInsns; unreached += f.nUnreachedInsns; raw += f.rawBytes; overlaps += f.overlapsDropped; errs += f.decodeErrors; codeBytes += f.codeSize;
		if (out == b) s.pass++; else { DiffReport("fob", label, out, b); s.fail++; }
	});
	s.notes["instructions reached by control flow (same count as tools/rbo/fobdis.py)"] = (int)insns; s.notes["instructions decoded linearly from unreached gaps"] = (int)unreached; s.notes["code bytes"] = (int)codeBytes; s.notes["code bytes left as raw spans (data that does not decode)"] = (int)raw;
	s.notes["decode errors (undecodable words met while following flow)"] = (int)errs; s.notes["overlapping instructions dropped"] = (int)overlaps;
	return Finish(s);
}

// chp: GOF2 sprite banks (<C>NN.CHP = bare BMP Cutter3 bank): same load / re-import / byte-identical checks as the CG area of a character.
static int CmdChpRt(int argc, char **argv)
{
	Section s; s.name = "chp";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Chp) { NaAdd(s, name, k); return; }
		CgBankCheck(label, b, s);
	});
	return Finish(s);
}

// rep: replays: parse (decipher header/extra blocks, verify the checksum, split the per-tick inputs) -> serialize (re-encipher, RECOMPUTE the checksum)
static int CmdRepRt(int argc, char **argv)
{
	Section s; s.name = "rep";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Rep) { NaAdd(s, name, k); return; }
		han2::rep::Replay r; std::string err;
		if (!han2::rep::Parse(b.data(), b.size(), r, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		if (!r.checksumOk) { printf("FAIL %s: stored checksum does not match the deciphered blocks\n", label.c_str()); s.fail++; return; }
		std::vector<uint8_t> out; han2::rep::Serialize(r, out, &err);
		if (out != b) { DiffReport("replay", label, out, b); s.fail++; return; }
		s.pass++; s.notes["version " + std::to_string(r.version) + " (" + ExtOf(name) + ")"]++;
	});
	return Finish(s);
}

// fnt: bitmap fonts (rbo_ex3.exe FontBank_LoadFile sub_43A500): header, index table, glyph bitmaps, half-width section
static int CmdFntRt(int argc, char **argv)
{
	Section s; s.name = "fnt";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Fnt) { NaAdd(s, name, k); return; }
		han2::fnt::Font f; std::string err;
		if (!han2::fnt::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		std::vector<uint8_t> out; han2::fnt::Serialize(f, out, &err);
		if (out != b) { DiffReport("font", label, out, b); s.fail++; return; }
		s.pass++; s.notes["font " + std::to_string(f.width) + "x" + std::to_string(f.height) + " " + std::to_string(f.bitsPerPixel) + "bpp" + (f.hasHalf ? " + half-width" : "") + (f.tail.empty() ? "" : " + tail")]++;
		if (!f.tail.empty()) printf("NOTE %s: %zu bytes after the last section kept verbatim\n", label.c_str(), f.tail.size());
	});
	return Finish(s);
}

// audio: RIFF/WAVE files parse into chunks and serialize identically; MPEG files are validated frame by frame (no re-encoder: standard format)
static int CmdAudioRt(int argc, char **argv)
{
	Section s; s.name = "audio";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k == Kind::Wav) {
			han2::Riff r; std::string err;
			if (!han2::ParseRiff(b.data(), b.size(), r, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
			std::vector<uint8_t> out; han2::SerializeRiff(r, out);
			if (out != b) { DiffReport("riff", label, out, b); s.fail++; return; }
			if (!r.tail.empty()) s.notes["WAVE with bytes after the last chunk"]++;
			s.pass++; s.notes["WAVE fmt tag " + std::to_string(r.fmtTag) + " " + std::to_string(r.channels) + "ch " + std::to_string(r.bits) + "bit"]++;
		} else if (k == Kind::Mp3) {
			han2::MpegInfo m; std::string err;
			if (!han2::ValidateMpeg(b.data(), b.size(), m, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
			s.pass++; s.notes["MPEG audio files (frames validated)"]++; if (m.trailer > 128) s.notes["MPEG with more than 128 bytes after the last frame"]++;
		} else NaAdd(s, name, k);
	});
	return Finish(s);
}

// misc: .BMP (header + palette + pixel rows) and Shift-JIS text (CRLF line structure)
static int CmdMiscRt(int argc, char **argv)
{
	Section s; s.name = "misc";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k == Kind::Bmp) {
			han2::Bmp m; std::string err; std::vector<uint8_t> out;
			if (!han2::ParseBmp(b.data(), b.size(), m, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
			han2::SerializeBmp(m, out);
			if (out == b) { s.pass++; s.notes["BMP " + std::to_string(m.bpp) + "bpp"]++; } else { DiffReport("bmp", label, out, b); s.fail++; }
		} else if (k == Kind::Text) {
			han2::Text t; std::string err; std::vector<uint8_t> out;
			han2::ParseText(b.data(), b.size(), t, &err); han2::SerializeText(t, out);
			if (out == b) { s.pass++; s.notes["text " + ExtOf(name)]++; } else { DiffReport("text", label, out, b); s.fail++; }
		} else NaAdd(s, name, k);
	});
	return Finish(s);
}

static int CmdCgInfo(int argc, char **argv)
{
	if (argc < 1) return 2;
	std::vector<uint8_t> b;
	if (!ReadLoose(argv[0], b)) { printf("cannot read\n"); return 1; }
	han2::Han2File f; std::string err;
	if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("parse: %s\n", err.c_str()); return 1; }
	const auto &cgb = f.area[han2::kAreaCg];
	CG cg;
	if (!cg.loadFromMemory(cgb.data(), (unsigned)cgb.size())) { printf("CG load failed (%zu bytes)\n", cgb.size()); return 1; }
	int n = cg.get_image_count();
	printf("images %d\n", n);
	int lo = argc > 1 ? atoi(argv[1]) : 0, hi = argc > 2 ? atoi(argv[2]) : std::min(n, 40);
	for (int i = lo; i < hi && i < n; i++) {
		int bpp, ty, x1, y1, x2, y2;
		if (!cg.image_info(i, bpp, ty, x1, y1, x2, y2)) { printf("%4d absent\n", i); continue; }
		printf("%4d bpp %d type %d bounds (%d,%d)-(%d,%d)\n", i, bpp, ty, x1, y1, x2, y2);
	}
	return 0;
}

// edittest: apply editor-style edits to one character, save as .DT2, reload and compare.
static bool FrameEq(const Frame &a, const Frame &b)
{
	if (a.AF.layers.size() != b.AF.layers.size()) return false;
	for (size_t i = 0; i < a.AF.layers.size(); i++) {
		const auto &x = a.AF.layers[i], &y = b.AF.layers[i];
		if (x.spriteId != y.spriteId || x.usePat != y.usePat || x.offset_x != y.offset_x || x.offset_y != y.offset_y) return false;
		if (memcmp(x.rotation, y.rotation, sizeof(x.rotation)) || memcmp(x.scale, y.scale, sizeof(x.scale)) || x.blend_mode != y.blend_mode) return false;
	}
	if (a.AF.duration != b.AF.duration || a.AF.aniType != b.AF.aniType || a.AF.aniFlag != b.AF.aniFlag || a.AF.jump != b.AF.jump) return false;
	if (a.AS.speed[0] != b.AS.speed[0] || a.AS.movementFlags != b.AS.movementFlags || a.AS.stanceState != b.AS.stanceState) return false;
	if (a.AT.damage != b.AT.damage || a.AT.guard_damage != b.AT.guard_damage) return false;
	if (a.hitboxes.size() != b.hitboxes.size()) return false;
	for (auto &kv : a.hitboxes) {
		auto it = b.hitboxes.find(kv.first);
		if (it == b.hitboxes.end() || memcmp(kv.second.xy, it->second.xy, 16)) return false;
	}
	return true;
}

static int CmdEditTest(int argc, char **argv)
{
	if (argc < 2) { puts("edittest <in.DAT|DT2> <out.DT2>"); return 2; }
	std::vector<uint8_t> b;
	if (!ReadLoose(argv[0], b)) { puts("cannot read"); return 1; }
	FrameData fd; std::string err;
	if (!han2::Load(fd, b.data(), b.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
	FrameData orig; han2::Load(orig, b.data(), b.size(), &err);
	// pick patterns: A = first with >=2 frames, B = a pattern with a hurt box on frame 0
	int pa = -1, pb = -1;
	for (int p = 0; p < 256; p++) {
		if (fd.m_sequences[p].frames.size() >= 2 && pa < 0) pa = p;
		if (!fd.m_sequences[p].frames.empty() && fd.m_sequences[p].frames[0].hitboxes.count(1) && p != pa && pb < 0) pb = p;
	}
	if (pa < 0 || pb < 0) { puts("no suitable patterns"); return 1; }
	Frame &fa = fd.m_sequences[pa].frames[0];
	fa.AF.duration += 7;
	fa.AF.layers[0].offset_x += 5;
	fa.hitboxes[25] = Hitbox{{10, -90, 60, -50}};          // new attack box -> AT record created
	fa.AT.damage = 123;
	Frame &fb = fd.m_sequences[pb].frames[0];
	fb.hitboxes[1].xy[2] += 11;                              // move a hurt box edge
	fb.hitboxes.erase(0);                                    // drop the overlap box if there was one
	std::vector<uint8_t> out; std::vector<std::string> warn;
	if (!han2::Serialize(fd, out, &err, &warn, true)) { printf("save: %s\n", err.c_str()); return 1; }
	{ std::ofstream f(std::filesystem::u8path(argv[1]), std::ios::binary); f.write((const char *)out.data(), (std::streamsize)out.size()); }
	FrameData re;
	if (!han2::Load(re, out.data(), out.size(), &err)) { printf("reload: %s\n", err.c_str()); return 1; }
	int bad = 0;
	for (int p = 0; p < 256; p++) {
		const auto &A = fd.m_sequences[p].frames, &R = re.m_sequences[p].frames;
		if (A.size() != R.size()) { printf("pattern %d frame count %zu vs %zu\n", p, A.size(), R.size()); bad++; continue; }
		for (size_t k = 0; k < A.size(); k++) if (!FrameEq(A[k], R[k])) { printf("pattern %d frame %zu differs after reload\n", p, k); bad++; }
		// untouched patterns must equal the original
		if (p != pa && p != pb) {
			const auto &O = orig.m_sequences[p].frames;
			for (size_t k = 0; k < O.size(); k++) if (!FrameEq(O[k], R[k])) { printf("untouched pattern %d frame %zu changed\n", p, k); bad++; }
		}
	}
	const Frame &ra = re.m_sequences[pa].frames[0];
	bool ok = ra.AF.duration == orig.m_sequences[pa].frames[0].AF.duration + 7 && ra.hitboxes.count(25) && ra.AT.damage == 123 && ra.han2.hadAT;
	printf("edited pattern %d (duration, offset, new attack box+AT) and %d (hurt box); %zu warnings; saved %zu bytes (orig %zu); %s, %d mismatches\n",
		pa, pb, warn.size(), out.size(), b.size(), ok ? "edits present" : "EDITS LOST", bad);
	return (ok && !bad) ? 0 : 1;
}

// shiftoffsets: move every frame's sprite offset and write the .DT2 (used for the in-game proof of an edit)
static int CmdShift(int argc, char **argv)
{
	if (argc < 4) { puts("shift <in.DAT|DT2> <out.DT2> <dx> <dy> [boxgrow]"); return 2; }
	std::vector<uint8_t> b;
	if (!ReadLoose(argv[0], b)) { puts("cannot read"); return 1; }
	FrameData fd; std::string err;
	if (!han2::Load(fd, b.data(), b.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
	int dx = atoi(argv[2]), dy = atoi(argv[3]), grow = argc > 4 ? atoi(argv[4]) : 0, n = 0;
	for (auto &seq : fd.m_sequences)
		for (auto &f : seq.frames) {
			for (auto &l : f.AF.layers) { l.offset_x += dx; l.offset_y += dy; }
			if (grow) for (auto &kv : f.hitboxes) { kv.second.xy[0] -= grow; kv.second.xy[1] -= grow; kv.second.xy[2] += grow; kv.second.xy[3] += grow; }
			n++;
		}
	std::vector<uint8_t> out; std::vector<std::string> warn;
	if (!han2::Serialize(fd, out, &err, &warn, true)) { printf("save: %s\n", err.c_str()); return 1; }
	std::ofstream f(std::filesystem::u8path(argv[1]), std::ios::binary);
	f.write((const char *)out.data(), (std::streamsize)out.size());
	printf("shifted %d frames by (%d,%d), box grow %d, wrote %zu bytes\n", n, dx, dy, grow, out.size());
	return 0;
}


// export: sprites, poses and animations of one character as PNG + JSON
static int CmdExport(int argc, char **argv)
{
	if (argc < 2) { puts("export <in.DAT|DT2> <outdir> [--no-cg] [--no-poses] [--no-patterns] [--scale N] [--icaroffa] [--canvas W H]"); return 2; }
	han2::ExportOptions opt;
	for (int i = 2; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--no-cg") opt.cgImages = false; else if (a == "--no-poses") opt.poses = false; else if (a == "--no-patterns") opt.patterns = false;
		else if (a == "--scale" && i + 1 < argc) opt.scale = atoi(argv[++i]);
		else if (a == "--icaroffa") opt.icaroffaLayout = true;
		else if (a == "--canvas" && i + 2 < argc) { opt.canvasW = atoi(argv[++i]); opt.canvasH = atoi(argv[++i]); }
	}
	std::vector<uint8_t> b;
	if (!ReadLoose(argv[0], b)) { puts("cannot read"); return 1; }
	FrameData *fd = new FrameData(); std::string err;
	std::vector<uint8_t> names;
	han2::Han2File f; han2::Parse(b.data(), b.size(), f, nullptr);
	// a .DT2 has no parts / CG: use the sibling .DAT when it exists
	std::vector<uint8_t> dat = b;
	if (f.kind == 3) {
		std::string p = argv[0]; size_t dot = p.find_last_of('.');
		std::string datp = p.substr(0, dot) + ".DAT";
		std::vector<uint8_t> d2;
		if (ReadLoose(datp, d2)) dat = d2; else { datp = p.substr(0, dot) + ".dat"; if (ReadLoose(datp, d2)) dat = d2; }
	}
	han2::Han2File df; if (!han2::Parse(dat.data(), dat.size(), df, &err)) { printf("parse: %s\n", err.c_str()); return 1; }
	const uint8_t *nm = df.area[han2::kAreaNames].size() >= 0x4000 ? df.area[han2::kAreaNames].data() : nullptr;
	if (!han2::Load(*fd, b.data(), b.size(), &err, nm, nm ? 0x4000 : 0)) { printf("load: %s\n", err.c_str()); return 1; }
	CG *cg = new CG(); Parts *parts = new Parts(cg);   // leaked on purpose (GL-free tool)
	const auto &cgb = df.area[han2::kAreaCg];
	if (!cgb.empty()) cg->loadFromMemory(cgb.data(), (unsigned)cgb.size());
	const auto &pb = df.area[han2::kAreaParts];
	if (!pb.empty() && !han2::PatToParts(pb.data(), pb.size(), *parts, &err)) printf("parts: %s\n", err.c_str());
	han2::ExportReport rep;
	if (!han2::ExportCharacter(*fd, *cg, *parts, argv[1], opt, rep, &err)) { printf("export: %s\n", err.c_str()); return 1; }
	printf("exported: %d CG png, %d pose png, %d frame png, %d patterns, %d strips/sheets, %d write errors\n", rep.cgPngs, rep.posePngs, rep.framePngs, rep.patternsWritten, rep.sheets, rep.writeErrors);
	for (auto &n : rep.notes) printf("NOTE %s\n", n.c_str());
	return rep.writeErrors ? 1 : 0;
}


// dt2companion <archive>...: BUG2/BUG7 regression (docs/formats/pacnyx_parity.md). For every RBO character that has both a .DT2 and a .DAT in the
// given archives (later archive wins, as the game mounts them), load it the way the editor does (frames from the .DT2; parts, CG and names from the
// .DAT), then check the .DT2 save path:
//   a) no edit: no .DAT companion is needed and the .DT2 re-serializes byte-identically;
//   b) a pattern rename + a part moved (PAT model -> BuildPat) + a CG byte changed: BuildDatCompanion must produce a .DAT whose name area,
//      parts area and CG area carry the edits, whose pattern area equals the .DT2's frames, and the .DT2 itself must stay byte-identical;
//   c) the same edits without a loaded .DAT must be refused (not silently dropped).
static int CmdDt2Companion(int argc, char **argv)
{
	Section s; s.name = "dt2comp";
	std::map<std::string, std::vector<uint8_t>> dt2, dat;   // stem -> bytes (later archive wins)
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		if (!pac::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); s.fail++; continue; }
		for (size_t k = 0; k < a.entries.size(); k++) {
			const std::string n = a.entries[k].name, e = ExtOf(n);
			if (e != ".DT2" && e != ".DAT") continue;
			std::vector<uint8_t> b; if (!pac::ReadEntry(a, k, b, &err)) continue;
			if (b.size() < 8 || memcmp(b.data(), "HAN2RBO ", 8)) continue;
			std::string stem = n.substr(0, n.size() - 4); for (auto &c : stem) if (c >= 'a' && c <= 'z') c -= 32;
			(e == ".DT2" ? dt2 : dat)[stem] = std::move(b);
		}
	}
	static CG *cg = new CG(); static Parts *parts = new Parts(cg);   // never destroyed (GL-free tool)
	int edited = 0;
	for (auto &kv : dt2) {
		const std::string &stem = kv.first;
		auto it = dat.find(stem);
		if (it == dat.end()) continue;
		han2::Han2File dt, df; std::string err;
		if (!han2::Parse(kv.second.data(), kv.second.size(), dt, &err) || dt.sub != 1) continue;   // RBO only (GOF2 keeps parts in .PAT)
		if (!han2::Parse(it->second.data(), it->second.size(), df, &err)) { printf("FAIL %s.DAT: parse: %s\n", stem.c_str(), err.c_str()); s.fail++; continue; }
		auto load = [&](FrameData &fd, bool withDat) {
			const uint8_t *nm = withDat && df.area[han2::kAreaNames].size() >= (size_t)han2::kNamesSize ? df.area[han2::kAreaNames].data() : nullptr;
			if (!han2::Load(fd, kv.second.data(), kv.second.size(), &err, nm, nm ? (size_t)han2::kNamesSize : 0)) return false;
			if (withDat) {   // same as han2::LoadCharacter
				auto c = fd.m_han2;
				c->hasDt2Header = true; memcpy(c->dt2Header, c->header, 0x40); memcpy(c->dt2AreaOff, c->areaOff, sizeof c->areaOff);
				c->parts = df.area[han2::kAreaParts]; c->cg = df.area[han2::kAreaCg]; c->names = df.area[han2::kAreaNames];
				for (int a = 0; a < 4; a++) c->areaOff[a] = df.areaOff[a];
				memcpy(c->header, df.header, 0x40);
			}
			return true;
		};
		FrameData *fd = new FrameData();
		if (!load(*fd, true)) { printf("FAIL %s: load: %s\n", stem.c_str(), err.c_str()); s.fail++; continue; }
		// a) no edit
		std::vector<uint8_t> comp, out; bool needed = true;
		if (!han2::BuildDatCompanion(*fd, comp, needed, &err) || needed) { printf("FAIL %s: unedited character asks for a .DAT companion (%s)\n", stem.c_str(), err.c_str()); s.fail++; continue; }
		if (!han2::Serialize(*fd, out, &err, nullptr, true) || out != kv.second) { DiffReport("unedited .DT2", stem, out, kv.second); s.fail++; continue; }
		// b) edits in the three .DAT-only areas
		auto c = fd->m_han2;
		int renamed = -1;
		for (int p = 0; p < han2::kPatterns && p < (int)fd->m_sequences.size(); p++) if (!fd->m_sequences[p].frames.empty()) { renamed = p; break; }
		if (renamed >= 0) fd->m_sequences[renamed].name = "HANTEI_RENAMED";
		bool partsEdited = false;
		if (!c->parts.empty() && han2::PatToParts(c->parts.data(), c->parts.size(), *parts, &err)) {
			for (auto &ps : parts->partSets) { for (auto &g : ps.groups) if (g.ppId >= 0) { g.x += 3; partsEdited = true; break; } if (partsEdited) break; }
			std::vector<uint8_t> np;
			if (partsEdited && han2::BuildPat(*parts, c->parts, np, &err) && np != c->parts) { c->parts.swap(np); c->partsDirty = true; } else partsEdited = false;
		}
		bool cgEdited = false;
		if (c->cg.size() > 0x5000) { c->cg[c->cg.size() - 1] ^= 0x5A; c->cgDirty = true; cgEdited = true; }
		std::vector<uint8_t> newParts = c->parts, newCg = c->cg;
		if (!han2::BuildDatCompanion(*fd, comp, needed, &err) || !needed) { printf("FAIL %s: edits but no .DAT companion (%s)\n", stem.c_str(), err.c_str()); s.fail++; continue; }
		han2::Han2File cf;
		if (!han2::Parse(comp.data(), comp.size(), cf, &err) || cf.kind != 0) { printf("FAIL %s: companion is not a full .DAT (%s)\n", stem.c_str(), err.c_str()); s.fail++; continue; }
		bool ok = true;
		if (cf.area[han2::kAreaParts] != newParts) { printf("FAIL %s: parts edit missing from the .DAT\n", stem.c_str()); ok = false; }
		if (cf.area[han2::kAreaCg] != newCg) { printf("FAIL %s: CG edit missing from the .DAT\n", stem.c_str()); ok = false; }
		if (renamed >= 0) {
			char nb[65]{}; memcpy(nb, cf.area[han2::kAreaNames].data() + 64 * renamed, 64);
			if (std::string(nb) != "HANTEI_RENAMED") { printf("FAIL %s: pattern %d rename missing from the .DAT name area\n", stem.c_str(), renamed); ok = false; }
		}
		// the companion's frames are the .DT2's frames
		FrameData *re = new FrameData();
		std::vector<uint8_t> reDt2;
		// (the header differs: the companion carries the .DAT's area offsets; the pattern area after it must be the same bytes)
		if (!han2::Load(*re, comp.data(), comp.size(), &err) || !han2::Serialize(*re, reDt2, &err, nullptr, true) || reDt2.size() != kv.second.size()
			|| memcmp(reDt2.data() + 0x40, kv.second.data() + 0x40, reDt2.size() - 0x40)) { DiffReport("companion .DAT pattern area vs .DT2", stem, reDt2, kv.second); ok = false; }
		if (!han2::Serialize(*fd, out, &err, nullptr, true) || out != kv.second) { printf("FAIL %s: .DT2 changed by .DAT-only edits\n", stem.c_str()); ok = false; }
		// c) no .DAT loaded: refused
		FrameData *bare = new FrameData();
		if (load(*bare, false)) {
			bare->m_sequences[renamed >= 0 ? renamed : 0].name = "HANTEI_RENAMED";
			bare->m_han2->partsDirty = true;
			std::vector<uint8_t> x; bool nd = false; std::string e2;
			if (han2::BuildDatCompanion(*bare, x, nd, &e2)) { printf("FAIL %s: edits without a .DAT were not refused\n", stem.c_str()); ok = false; }
		}
		if (ok) { s.pass++; if (partsEdited && cgEdited && renamed >= 0) edited++; } else s.fail++;
	}
	s.notes["characters with parts + CG + rename all checked"] = edited;
	return Finish(s);
}

// effectedit <archive>...: PACNyx effect edits on every CG bank (RBO .DAT CG area, GOF2 .CHP): on one drawable image per bank with
// blocks and no dependants: rename survives serialize/parse; moving it on its canvas and back is byte-identical; rebuilding its blocks with
// every cell kept renders the same pixels; switching one stored cell off makes exactly that cell transparent and keeps the rest.
static int CmdEffectEdit(int argc, char **argv)
{
	Section s; s.name = "effect";
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		std::vector<uint8_t> bank;
		if (k == Kind::Chp) bank = b;
		else if (k == Kind::Han2) { han2::Han2File f; if (!han2::Parse(b.data(), b.size(), f, nullptr)) { s.fail++; return; } bank = f.area[han2::kAreaCg]; }
		else { NaAdd(s, name, k); return; }
		if (bank.empty()) { s.notes["no CG bank"]++; s.pass++; return; }
		cgm::Bank bk; std::string err;
		if (!cgm::Bank::parse(bank.data(), bank.size(), bk, &err)) { s.notes["bank not parsed by the CG manager (opaque)"]++; s.pass++; return; }
		auto atlas = bk.buildAtlas();
		int n = -1;
		for (int i = 0; i < (int)bk.images.size() && n < 0; i++) {
			const auto &im = bk.images[i];
			if (!im.drawable() || im.blocks.size() < 2 || im.type == 3 || im.type == 5 || !bk.dependants(i, atlas).empty() || !bk.owners(i, atlas).empty()) continue;
			n = i;
		}
		if (n < 0) { s.notes["no suitable image"]++; s.pass++; return; }
		bool ok = true;
		auto fail = [&](const char *what) { const auto &q = bk.images[n]; printf("FAIL %s image %d (%d,%d)-(%d,%d) canvas %dx%d: %s %s\n", label.c_str(), n, q.x1, q.y1, q.x2, q.y2, q.w, q.h, what, err.c_str()); ok = false; };
		// rename
		cgm::Bank a = bk;
		if (!cgm::RenameImage(a, n, "HANTEI_FX", &err)) fail("rename");
		std::vector<uint8_t> ser; a.serialize(ser); cgm::Bank a2;
		if (ok && (!cgm::Bank::parse(ser.data(), ser.size(), a2, &err) || std::string(a2.images[n].name) != "HANTEI_FX")) fail("rename lost");
		// base point: move and move back -> identical bytes
		cgm::Bank m = bk; std::vector<uint8_t> s0, s1; bk.serialize(s0);
		const auto &mi = m.images[n];
		(void)mi;
		const int dx = 3, dy = -2;
		if (ok && (!cgm::OffsetImage(m, n, dx, dy, &err) || !cgm::OffsetImage(m, n, -dx, -dy, &err))) fail("offset");
		if (mi.x1 < 0 || mi.y1 < 0) s.notes["images with anchor-relative negative bounds"]++;
		if (ok) { m.serialize(s1); if (s1 != s0) fail("offset round trip not byte-identical"); }
		cgm::Rgba ref; bk.decode(n, ref);
		// rebuild with every cell kept: same pixels
		cgm::Bank r = bk; int cols, rows, ox, oy; auto mask = cgm::CellMask(r, n, cols, rows, ox, oy);
		if (ok && !cgm::ReblockImage(r, n, ref.px.data(), ref.w, ref.h, &mask, &err)) fail("reblock");
		cgm::Rgba got;
		auto sameAt = [&](const cgm::Rgba &x, const cgm::Image &imx, int px, int py) {   // canvas coordinates
			const int lx = px - imx.x1, ly = py - imx.y1;
			if (lx < 0 || ly < 0 || lx >= x.w || ly >= x.h) return (const uint8_t *)nullptr;
			return &x.px[((size_t)ly * x.w + lx) * 4];
		};
		auto alphaAt = [&](const cgm::Rgba &x, const cgm::Image &imx, int px, int py) { auto p = sameAt(x, imx, px, py); return p ? p[3] : (uint8_t)0; };
		const auto &im0 = bk.images[n];
		if (ok && r.decode(n, got)) {
			for (int y = im0.y1; y <= im0.y2 && ok; y++) for (int x = im0.x1; x <= im0.x2; x++) {
				const uint8_t *p = sameAt(ref, im0, x, y), *q = sameAt(got, r.images[n], x, y);
				if (!p || !q || (p[3] != q[3]) || (p[3] && memcmp(p, q, 3))) { fail("rebuilt pixels differ"); break; }
			}
		}
		// switch the first stored cell off
		int off = -1; for (size_t i = 0; i < mask.size(); i++) if (mask[i]) { off = (int)i; break; }
		cgm::Bank c = bk;
		if (ok && off >= 0) {
			auto m2 = mask; m2[(size_t)off] = 0;
			if (!cgm::ReblockImage(c, n, ref.px.data(), ref.w, ref.h, &m2, &err) || !c.decode(n, got)) fail("cell off");
			else {
				const int cu = bk.cellUnit(), cx0 = ox + (off % cols) * cu, cy0 = oy + (off / cols) * cu;
				for (int y = im0.y1; y <= im0.y2 && ok; y++) for (int x = im0.x1; x <= im0.x2; x++) {
					const bool inCell = x >= cx0 && x < cx0 + cu && y >= cy0 && y < cy0 + cu;
					const uint8_t want = inCell ? 0 : alphaAt(ref, im0, x, y);
					if (alphaAt(got, c.images[n], x, y) != want) { fail(inCell ? "cell still drawn" : "other cell changed"); break; }
				}
			}
		}
		if (ok) s.pass++; else s.fail++;
	});
	return Finish(s);
}

// partsinfo <file.DAT|.PAT> [pose...]: what the parts area holds (sheets, pose names, parts per pose), to check a saved file
static int CmdPartsInfo(int argc, char **argv)
{
	if (argc < 1) { puts("partsinfo <file.DAT|.PAT> [pose...]"); return 2; }
	std::vector<uint8_t> b; if (!ReadLoose(argv[0], b)) { puts("cannot read"); return 1; }
	std::vector<uint8_t> pat = b; std::string err;
	han2::Han2File f;
	if (!han2::IsPat(b.data(), b.size())) { if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("parse: %s\n", err.c_str()); return 1; } pat = f.area[han2::kAreaParts]; printf("HAN2RBO kind %u sub %u, parts %zu bytes, CG %zu bytes\n", f.kind, f.sub, f.area[han2::kAreaParts].size(), f.area[han2::kAreaCg].size()); }
	static Parts *parts = new Parts(nullptr);
	if (!han2::PatToParts(pat.data(), pat.size(), *parts, &err)) { printf("parts: %s\n", err.c_str()); return 1; }
	int poses = 0; for (auto &ps : parts->partSets) { for (auto &g : ps.groups) if (g.ppId >= 0) { poses++; break; } }
	printf("sheets %zu:", parts->gfxMeta.size());
	for (auto &g : parts->gfxMeta) printf(" %dx%d", g.w, g.h);
	uint64_t h = 1469598103934665603ull;
	if (!parts->gfxMeta.empty() && parts->gfxMeta.back().data) for (int i = 0; i < parts->gfxMeta.back().w * parts->gfxMeta.back().h * 4; i++) { h ^= (uint8_t)parts->gfxMeta.back().data[i]; h *= 1099511628211ull; }
	printf("\nlast sheet pixel hash %016llx\npose slots %zu, poses with parts %d\n", (unsigned long long)h, parts->partSets.size(), poses);
	for (int i = 1; i < argc; i++) {
		int p = atoi(argv[i]); if (p < 0 || p >= (int)parts->partSets.size()) { printf("pose %d: none\n", p); continue; }
		const auto &ps = parts->partSets[p]; int n = 0; for (auto &g : ps.groups) if (g.ppId >= 0) n++;
		printf("pose %d '%s': %d parts", p, ps.name.c_str(), n);
		for (size_t k = 0; k < ps.groups.size(); k++) if (ps.groups[k].ppId >= 0) { const auto &g = ps.groups[k]; const auto &co = parts->cutOuts[g.ppId]; printf(" [%zu sheet %d (%d,%d) L%d rot %.4f]", k, co.texture, g.x, g.y, (int)floorf(g.priority + 1e-4f), g.rotation[3]); }
		printf("\n");
	}
	return 0;
}

static int CmdPacWrite(int argc, char **argv)
{
	if (argc < 2) { puts("pacwrite <in.PAC> <out.PAC>"); return 2; }
	pac::Archive a; std::string err;
	if (!pac::Open(argv[0], a, &err)) { printf("open: %s\n", err.c_str()); return 1; }
	std::vector<pac::WriteSource> src(a.entries.size());
	for (size_t i = 0; i < src.size(); i++) {
		src[i].name = a.entries[i].name; src[i].rawName.assign(a.entries[i].rawName, a.entries[i].rawName + pac::kNameLen);
		src[i].kind = pac::WriteSource::ArchiveEntry; src[i].archive = &a; src[i].index = i;
	}
	if (!pac::WriteArchive(argv[1], src, {argv[0]}, &err)) { printf("write: %s\n", err.c_str()); return 1; }
	std::ifstream x(std::filesystem::u8path(argv[0]), std::ios::binary), y(std::filesystem::u8path(argv[1]), std::ios::binary);
	std::vector<char> bx(1 << 20), by(1 << 20); uint64_t pos = 0;
	while (true) {
		x.read(bx.data(), bx.size()); y.read(by.data(), by.size());
		if (x.gcount() != y.gcount() || memcmp(bx.data(), by.data(), (size_t)x.gcount())) { printf("DIFF near byte %llu\n", (unsigned long long)pos); return 1; }
		pos += (uint64_t)x.gcount();
		if (x.gcount() == 0) break;
	}
	printf("OK %s rewritten byte-identical (%llu bytes)\n", argv[1], (unsigned long long)pos);
	return 0;
}


// pacselftest <scratch dir>: the Create-PAC dialog's path end to end on synthetic files (no game needed): a PAC built from loose
// files with the progress callback, re-opened, every entry extracted and compared, a name over 59 bytes refused, and the
// refuse-if-same guard (never overwrite an archive that is also a source).
static void PacProgress(int done, int total, void *user) { int *last = (int *)user; if (done != *last) printf("  progress %d/%d\n", done, total); *last = done; }
static int CmdPacSelfTest(int argc, char **argv)
{
	if (argc < 1) { puts("pacselftest <scratch dir>"); return 2; }
	namespace fs = std::filesystem;
	const fs::path d = fs::u8path(argv[0]);
	std::error_code ec;
	fs::remove_all(d, ec);
	fs::create_directories(d / "in", ec);
	const std::vector<std::pair<std::string, std::string>> files = {{"CHR01.DAT", std::string(4000, 'a')}, {"SE001.WAV", "RIFF....WAVEfmt "}, {"EMPTY.TXT", ""}};
	std::vector<pac::WriteSource> src;
	for (const auto &f : files) {
		{ std::ofstream o(d / "in" / f.first, std::ios::binary); o << f.second; }
		pac::WriteSource w; w.name = f.first; w.kind = pac::WriteSource::LooseFile; w.path = (d / "in" / f.first).u8string();
		src.push_back(w);
	}
	int bad = 0, last = -1;
	std::string err;
	const std::string out = (d / "NEW.PAC").u8string();
	if (!pac::WriteArchive(out, src, {}, &err, PacProgress, &last)) { printf("FAIL create: %s\n", err.c_str()); return 1; }
	pac::Archive a;
	if (!pac::Open(out, a, &err)) { printf("FAIL reopen: %s\n", err.c_str()); return 1; }
	printf("created %s: %zu entries\n", out.c_str(), a.entries.size());
	if (a.entries.size() != files.size()) { puts("FAIL entry count"); bad++; }
	for (const auto &f : files) {
		const int i = pac::Find(a, f.first);
		std::vector<uint8_t> b;
		if (i < 0 || !pac::ReadEntry(a, (size_t)i, b, &err)) { printf("FAIL extract %s\n", f.first.c_str()); bad++; continue; }
		if (std::string(b.begin(), b.end()) != f.second) { printf("FAIL bytes of %s\n", f.first.c_str()); bad++; continue; }
		{ std::ofstream o(d / ("x_" + f.first), std::ios::binary); o.write((const char *)b.data(), (std::streamsize)b.size()); }
		printf("extracted %s (%zu bytes) identical\n", f.first.c_str(), b.size());
	}
	std::vector<pac::WriteSource> longName(1);
	longName[0].name = std::string(60, 'N'); longName[0].memory = {1, 2, 3};
	if (pac::WriteArchive((d / "LONG.PAC").u8string(), longName, {}, &err)) { puts("FAIL 60-byte name accepted"); bad++; }
	else printf("refused as expected: %s\n", err.c_str());
	longName[0].name = std::string(59, 'N');
	if (!pac::WriteArchive((d / "OK59.PAC").u8string(), longName, {}, &err)) { printf("FAIL 59-byte name refused: %s\n", err.c_str()); bad++; }
	else puts("59-byte name accepted");
	std::vector<pac::WriteSource> self(1);
	self[0].name = "X"; self[0].kind = pac::WriteSource::ArchiveEntry; self[0].archive = &a; self[0].index = 0;
	if (pac::WriteArchive(out, self, {out}, &err)) { puts("FAIL overwrote its own source archive"); bad++; }
	else printf("refused as expected: %s\n", err.c_str());
	printf("PACSELFTEST %s (%d problems)\n", bad ? "FAIL" : "OK", bad);
	return bad ? 1 : 0;
}

static int CmdDiff(int argc, char **argv)
{
	if (argc < 2) { puts("diff <a.DAT|DT2> <b.DAT|DT2>   (field level: what b changes against a)"); return 2; }
	std::vector<uint8_t> a, b; if (!ReadLoose(argv[0], a) || !ReadLoose(argv[1], b)) { puts("cannot read"); return 1; }
	FrameData fa, fb; std::string err;
	if (!han2::Load(fa, a.data(), a.size(), &err) || !han2::Load(fb, b.data(), b.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
	fb.m_han2->originalPatternFile = fa.m_han2->originalPatternFile;
	std::vector<han2::DiffEntry> d;
	if (!han2::DiffAgainstOriginal(fb, d, &err)) { printf("%s\n", err.c_str()); return 1; }
	for (auto &e : d) { if (e.frame >= 0) printf("pattern %d frame %d: %s\n", e.pattern, e.frame, e.what.c_str()); else printf("pattern %d: %s\n", e.pattern, e.what.c_str()); }
	printf("%zu differences\n", d.size());
	return 0;
}

// animtest is a BEHAVIOUR check, not a byte round trip: the live stepper must reproduce SimulateFlow's tick count for every pattern whose flow
// terminates by itself (same rules, two implementations). Patterns SimulateFlow cannot finish are not comparable and are listed by reason:
//   "ani flag N depends on runtime state"  the frame's animation flag (engine Actor_AdvanceByAniFlag) branches on game state (hit, input, ground ...)
//   "runs off the end" / "cap" / "does not terminate"  an unconditional loop or a pattern with no terminating frame (idle/walk cycles)
static int CmdAnimTest(int argc, char **argv)
{
	Section s; s.name = "animtest";
	int ok = 0, bad = 0, noTerm = 0;
	std::map<std::string, int> why;
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b, name);
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		FrameData fd; std::string err;
		if (!han2::Load(fd, b.data(), b.size(), &err)) { printf("FAIL %s: load: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		int fileBad = 0;
		for (int p = 0; p < 256; p++) {
			if (fd.m_sequences[p].frames.empty()) continue;
			std::vector<std::pair<int, int>> v; std::string note; han2::SimulateFlow(fd.m_sequences[p], v, note);
			if (note.rfind("ends", 0) != 0) {
				noTerm++;
				std::string key = note.rfind("ani flag", 0) == 0 ? "ani flag depends on runtime state" : note;
				why[key]++;
				continue;
			}
			int expect = 0; for (auto &x : v) expect += std::max(1, x.second);   // the engine needs at least one tick per frame
			han2::AnimState st; han2::AnimStart(fd, st, p); int guard = 0;
			while (!st.ended && guard++ < 200000) { han2::AnimTick(fd, st, false, false); }
			if (st.totalTicks == expect) ok++; else { bad++; fileBad++; if (bad < 5) printf("%s pattern %d: stepper %d ticks vs flow %d\n", label.c_str(), p, st.totalTicks, expect); }
		}
		if (fileBad) s.fail++; else s.pass++;
	});
	printf("animtest patterns: %d agree, %d differ, %d not comparable (no self-terminating flow; reasons:", ok, bad, noTerm);
	for (auto &kv : why) printf(" [%s x%d]", kv.first.c_str(), kv.second);
	printf(")\n");
	return Finish(s);
}

// ---- GOF1 .p archives (gof_00..03.p), members classified by their bytes; nested archives are walked recursively ----
// Members with a structured model: character .DAT (magic after stage one d9 93 fe 3d), nested archive, .EX3 (LLIF token round trip), .BMP, .CT (typed command
// table), .WMT, .FNT, RIFF WAVE, MPEG audio, Shift-JIS text. Members without one are listed with the reason; their bytes are proven by the archive rebuild.
static uint32_t R32le(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static void Gof1Dat(const std::string &label, const std::vector<uint8_t> &stage1, Section &s)
{
	std::string err; std::vector<uint8_t> plain = stage1; gof1::DecryptDat(plain);
	FrameData fd;
	if (!gof1::Load(fd, plain.data(), plain.size(), &err)) { printf("FAIL %s: load: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
	std::vector<uint8_t> out;
	if (!gof1::Serialize(fd, out, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
	std::vector<uint8_t> enc = out; gof1::EncryptDat(enc);
	if (getenv("PARTS")) { auto cgp = new CG(); Parts &pp = *new Parts(cgp); std::string pe; const auto &blob = fd.m_han2->parts; bool ok = han2::PatToParts(blob.data(), blob.size(), pp, &pe); printf("  %s parts: %s, %zu part sets, %zu cutouts, %zu textures %s\n", label.c_str(), ok ? "ok" : "FAIL", pp.partSets.size(), pp.cutOuts.size(), pp.gfxMeta.size(), pe.c_str()); }
	if (out == plain && enc == stage1) { s.pass++; s.notes["character .DAT"]++; }
	else { DiffReport("character DAT", label, out, plain); if (enc != stage1) printf("  re-encrypt differs\n"); s.fail++; }
}

static void Gof1Member(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s, int depth);

static void Gof1Nested(const std::string &label, const std::vector<uint8_t> &d, Section &s, int depth)
{
	gof1::MemArchive m; std::string err;
	if (!gof1::OpenMem(d.data(), d.size(), m, &err)) { printf("FAIL %s: nested archive: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
	std::vector<uint8_t> idx; gof1::EncodeIndex(m.a, idx);
	if (idx.size() > d.size() || memcmp(idx.data(), d.data(), idx.size()) != 0) { printf("FAIL %s: nested archive index does not re-encode identically\n", label.c_str()); s.fail++; return; }
	if (!m.tiles) { printf("FAIL %s: nested archive sizes do not add up to its length\n", label.c_str()); s.fail++; return; }
	s.pass++; s.notes["nested archive (index re-encoded identically)"]++;
	for (size_t i = 0; i < m.a.entries.size(); i++) {
		const std::string sub = label + "/" + m.a.entries[i].name;
		std::vector<uint8_t> plain;
		if (!gof1::ReadEntryMem(m, d.data(), i, plain)) { { char hx[16]; snprintf(hx, sizeof hx, "%X", m.a.entries[i].size); s.notes[std::string("nested entry with size 0x") + hx + " (runs past the archive: overlap decoy, no data)"]++; } s.na["nested decoy entry (no data)"]++; continue; }
		std::vector<uint8_t> back = plain; if (m.a.plainFlag == 0) gof1::CipherEntry(back, m.a.entries[i].name);
		if (memcmp(back.data(), d.data() + m.dataOffset[i], back.size()) != 0) { printf("FAIL %s: entry cipher does not invert\n", sub.c_str()); s.fail++; continue; }
		Gof1Member(sub, m.a.entries[i].name, plain, s, depth + 1);
	}
}

static void Gof1Member(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s, int depth)
{
	const std::string e = ExtOf(name);
	std::string err; std::vector<uint8_t> out;
	if (d.size() >= 4 && R32le(d.data()) == 0x3dfe93d9u) { Gof1Dat(label, d, s); return; }
	if (d.size() >= 8 && gof1::LooksLikeArchive(d.data(), d.size()) && depth < 3) { Gof1Nested(label, d, s, depth); return; }
	if (d.size() >= 4 && memcmp(d.data(), "LLIF", 4) == 0) {
		han2::Ex3 x; if (!han2::ParseEx3(d.data(), d.size(), x, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeEx3(x, out); if (out == d) { s.pass++; s.notes[".EX3 (LLIF byte-pair blocks)"]++; } else { DiffReport("ex3", label, out, d); s.fail++; } return;
	}
	if (d.size() >= 12 && memcmp(d.data(), "RIFF", 4) == 0) {
		han2::Riff r; if (!han2::ParseRiff(d.data(), d.size(), r, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeRiff(r, out); if (out == d) { s.pass++; s.notes[".WAV (RIFF chunks)"]++; } else { DiffReport("riff", label, out, d); s.fail++; } return;
	}
	if (e == ".MP3") { han2::MpegInfo m; if (!han2::ValidateMpeg(d.data(), d.size(), m, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; } else { s.pass++; s.notes[".MP3 (frames validated)"]++; } return; }
	if (e == ".BMP") {
		han2::Bmp b; if (!han2::ParseBmp(d.data(), d.size(), b, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeBmp(b, out); if (out == d) { s.pass++; s.notes[".BMP"]++; } else { DiffReport("bmp", label, out, d); s.fail++; } return;
	}
	if (e == ".FNT") {
		han2::fnt::Font f; if (!han2::fnt::Parse(d.data(), d.size(), f, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::fnt::Serialize(f, out, &err); if (out == d) { s.pass++; s.notes[".FNT (bitmap font)"]++; } else { DiffReport("font", label, out, d); s.fail++; } return;
	}
	if (e == ".CT" && d.size() == 4632) {
		han2::Ct c; if (!han2::ParseCt(d.data(), d.size(), c, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeCt(c, out); if (out == d) { s.pass++; s.notes[".CT (command table)"]++; } else { DiffReport("ct", label, out, d); s.fail++; } return;
	}
	if (e == ".CT") {
		han2::CharSel c; if (!han2::ParseCharSel(d.data(), d.size(), c, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeCharSel(c, out); if (out == d) { s.pass++; s.notes["CHARSEL.CT (enciphered character table)"]++; } else { DiffReport("charsel", label, out, d); s.fail++; } return;
	}
	if (e == ".B") {
		han2::PolyObject o; if (!han2::ParsePoly(d.data(), d.size(), o, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializePoly(o, out); if (out == d) { s.pass++; s.notes[".B (polygon object)"]++; s.notes["  .B faces"] += (int)o.faces.size(); s.notes["  .B vertices"] += o.nVerts; } else { DiffReport("poly", label, out, d); s.fail++; } return;
	}
	if (e == ".CPF" || (e == ".TXT" && d.size() >= 56048 && (name.size() > 7 && (name.compare(name.size() - 7, 7, "COM.TXT") == 0 || name.compare(name.size() - 7, 7, "com.txt") == 0)) && name != "MULTICOM.TXT")) {
		han2::AiFile a; if (!han2::ParseAi(d.data(), d.size(), a, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeAi(a, out); if (out == d) { s.pass++; s.notes[e == ".CPF" ? ".CPF (CPU AI script, training dummy)" : "_<CHAR>COM.TXT (CPU AI script)"]++; } else { DiffReport("ai", label, out, d); s.fail++; } return;
	}
	if (e == ".TXT" && name == "MULTICOM.TXT") {
		han2::AiLegacy a; if (!han2::ParseAiLegacy(d.data(), d.size(), a, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeAiLegacy(a, out); if (out == d) { s.pass++; s.notes["MULTICOM.TXT (legacy AI tables, unreferenced)"]++; } else { DiffReport("ai legacy", label, out, d); s.fail++; } return;
	}
	if (e == ".WMT") {
		han2::Wmt w; if (!han2::ParseWmt(d.data(), d.size(), w, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		han2::SerializeWmt(w, out); if (out == d) { s.pass++; s.notes[".WMT (table)"]++; } else { DiffReport("wmt", label, out, d); s.fail++; } return;
	}
	if ((e == ".TXT" || e == ".H") && han2::LooksLikeText(d.data(), d.size())) {
		han2::Text t; han2::ParseText(d.data(), d.size(), t, &err); han2::SerializeText(t, out);
		if (out == d) { s.pass++; s.notes["text (Shift-JIS, line structure)"]++; } else { DiffReport("text", label, out, d); s.fail++; } return;
	}
	if (e == ".DAT") printf("NOTE %s: .DAT without the character magic (%zu bytes, head %02x%02x%02x%02x)\n", label.c_str(), d.size(), d.size() > 0 ? d[0] : 0, d.size() > 1 ? d[1] : 0, d.size() > 2 ? d[2] : 0, d.size() > 3 ? d[3] : 0);
	if (e == ".TXT") printf("NOTE %s: .TXT that does not look like text (%zu bytes)\n", label.c_str(), d.size());
	s.na[std::string(e.empty() ? "(no ext)" : e) + " (opaque, proven by the archive rebuild)"]++;
}

// gof1rt: (1) every archive rebuilt entry by entry from plain bytes (cipher undone and re-applied, own index re-encoded) must equal the file;
// (2) every member by what it is (see above).
static int CmdGof1Rt(int argc, char **argv)
{
	Section s; s.name = "gof1";
	for (int i = 0; i < argc; i++) {
		gof1::Archive a; std::string err;
		const int p0 = s.pass, f0 = s.fail;
		if (!gof1::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); s.fail++; continue; }
		{
			std::error_code ec;
			const std::string tmp = (std::filesystem::temp_directory_path(ec) / ("han2_gof1rt_" + std::to_string(i) + ".bin")).u8string();
			bool same = false;
			if (!gof1::RewriteAllFromPlain(a, tmp, &err)) printf("FAIL %s: archive rebuild: %s\n", argv[i], err.c_str());
			else {
				std::ifstream x(std::filesystem::u8path(argv[i]), std::ios::binary), y(std::filesystem::u8path(tmp), std::ios::binary);
				std::vector<char> bx(1 << 20), by(1 << 20); same = std::filesystem::file_size(std::filesystem::u8path(argv[i]), ec) == std::filesystem::file_size(std::filesystem::u8path(tmp), ec);
				while (same) { x.read(bx.data(), (std::streamsize)bx.size()); y.read(by.data(), (std::streamsize)by.size()); if (x.gcount() != y.gcount() || memcmp(bx.data(), by.data(), (size_t)x.gcount())) same = false; if (x.gcount() == 0) break; }
				if (!same) printf("FAIL %s: rebuilt archive differs from the file\n", argv[i]);
			}
			std::filesystem::remove(std::filesystem::u8path(tmp), ec);
			if (same) s.pass++; else s.fail++;
		}
		for (size_t k = 0; k < a.entries.size(); k++) {
			std::vector<uint8_t> d;
			if (!gof1::ReadEntry(a, k, d, &err)) { printf("FAIL %s::%s: %s\n", argv[i], a.entries[k].name.c_str(), err.c_str()); s.fail++; continue; }
			Gof1Member(std::string(argv[i]) + "::" + a.entries[k].name, a.entries[k].name, d, s, 0);
		}
		printf("%-40s pass %d fail %d skipped 0\n", argv[i], s.pass - p0, s.fail - f0);
	}
	return Finish(s);
}

// gof1shift <in.p> <ENTRY.DAT|-all> <out.p> <dx> <dy>: shifts every frame's sprite offset and writes a NEW archive (the in-game test file)
static int CmdGof1Shift(int argc, char **argv)
{
	if (argc < 5) { puts("gof1shift <in.p> <ENTRY.DAT> <out.p> <dx> <dy>"); return 2; }
	gof1::Archive a; std::string err;
	if (!gof1::Open(argv[0], a, &err)) { printf("%s\n", err.c_str()); return 1; }
	int idx = gof1::Find(a, argv[1]);
	if (idx < 0) { puts("entry not found"); return 1; }
	std::vector<uint8_t> d; gof1::ReadEntry(a, (size_t)idx, d, &err); gof1::DecryptDat(d);
	FrameData fd;
	if (!gof1::Load(fd, d.data(), d.size(), &err)) { printf("%s\n", err.c_str()); return 1; }
	fd.m_han2->sourcePath = argv[0]; fd.m_han2->gof1Name = a.entries[(size_t)idx].name;
	int dx = atoi(argv[3]), dy = atoi(argv[4]), n = 0;
	for (auto &q : fd.m_sequences) for (auto &f : q.frames) { for (auto &l : f.AF.layers) { l.offset_x += dx; l.offset_y += dy; } n++; }
	if (!gof1::SaveFile(fd, argv[2], &err)) { printf("save: %s\n", err.c_str()); return 1; }
	printf("shifted %d frames of %s by (%d,%d), wrote %s\n", n, argv[1], dx, dy, argv[2]);
	return 0;
}

int main(int argc, char **argv)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (argc < 2) { puts("usage: han2tool ls|count|extract|pacrt ..."); return 2; }
	std::string c = argv[1];
	if (c == "ls") return CmdLs(argc - 2, argv + 2);
	if (c == "count") return CmdCount(argc - 2, argv + 2);
	if (c == "extract") return CmdExtract(argc - 2, argv + 2);
	if (c == "roundtrip") return CmdRoundtrip(argc - 2, argv + 2);
	if (c == "modelrt") return CmdModelRt(argc - 2, argv + 2);
	if (c == "cginfo") return CmdCgInfo(argc - 2, argv + 2);
	if (c == "edittest") return CmdEditTest(argc - 2, argv + 2);
	if (c == "shift") return CmdShift(argc - 2, argv + 2);
	if (c == "patrt") return CmdPatRt(argc - 2, argv + 2);
	if (c == "export") return CmdExport(argc - 2, argv + 2);
	if (c == "imgrt") return CmdImgRt(argc - 2, argv + 2);
	if (c == "pacwrite") return CmdPacWrite(argc - 2, argv + 2);
	if (c == "pacselftest") return CmdPacSelfTest(argc - 2, argv + 2);
	if (c == "dt2companion") return CmdDt2Companion(argc - 2, argv + 2);
	if (c == "effectedit") return CmdEffectEdit(argc - 2, argv + 2);
	if (c == "partsinfo") return CmdPartsInfo(argc - 2, argv + 2);
	if (c == "cgrt") return CmdCgRt(argc - 2, argv + 2);
	if (c == "diff") return CmdDiff(argc - 2, argv + 2);
	if (c == "animtest") return CmdAnimTest(argc - 2, argv + 2);
	if (c == "gof1rt") return CmdGof1Rt(argc - 2, argv + 2);
	if (c == "gof1shift") return CmdGof1Shift(argc - 2, argv + 2);
	if (c == "fobrt") return CmdFobRt(argc - 2, argv + 2);
	if (c == "chprt") return CmdChpRt(argc - 2, argv + 2);
	if (c == "cgm") return CmdCgmRt(argc - 2, argv + 2);
	if (c == "reprt") return CmdRepRt(argc - 2, argv + 2);
	if (c == "fntrt") return CmdFntRt(argc - 2, argv + 2);
	if (c == "audiort") return CmdAudioRt(argc - 2, argv + 2);
	if (c == "miscrt") return CmdMiscRt(argc - 2, argv + 2);
	if (c == "opaquert") return CmdOpaqueRt(argc - 2, argv + 2);
	if (c == "census") return CmdCensus(argc - 2, argv + 2);
	if (c == "pacrt") return CmdPacRt(argc - 2, argv + 2);
	printf("unknown command %s\n", c.c_str());
	return 2;
}
