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
#include "../han2_pat.h"
#include "../han2_export.h"
#include "../han2_diff.h"
#include "../han2_anim.h"
#include "../framedata_gof1.h"
#include "gof1_archive.h"
#include "img_file.h"
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
enum class Kind { Han2, Pat, Img, Other };
static Kind Classify(const std::vector<uint8_t> &b)
{
	if (b.size() >= 8 && memcmp(b.data(), "HAN2RBO ", 8) == 0) return Kind::Han2;
	if (han2::IsPat(b.data(), b.size())) return Kind::Pat;
	if (han2::IsImg(b.data(), b.size())) return Kind::Img;
	return Kind::Other;
}
static const char *KindName(Kind k) { return k == Kind::Han2 ? "HAN2RBO" : k == Kind::Pat ? "bare PAT" : k == Kind::Img ? "IMG" : "opaque"; }
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
			cnt[ExtOf(a.entries[k].name) + " -> " + KindName(Classify(b))]++;
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
		Kind k = Classify(b);
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
		Kind k = Classify(b);
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
		Kind k = Classify(b);
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
		Kind k = Classify(b);
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
			Kind k = Classify(b); if (k != Kind::Other) NaAdd(s, argv[i], k); else s.pass++;
			continue;
		}
		std::ifstream raw(std::filesystem::u8path(argv[i]), std::ios::binary);
		for (size_t k = 0; k < a.entries.size(); k++) {
			const std::string &name = a.entries[k].name; const std::string label = std::string(argv[i]) + "::" + name;
			std::vector<uint8_t> b;
			if (!pac::ReadEntry(a, k, b, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); s.fail++; continue; }
			Kind kind = Classify(b);
			if (kind != Kind::Other) { NaAdd(s, name, kind); continue; }
			const std::string ext = ExtOf(name);
			const bool structuredExt = ext == ".DAT" || ext == ".DT2" || ext == ".PAT" || ext == ".IMG";
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

// cgrt: every character's CG bank (BMP Cutter): (1) loading it must not touch a byte (bank == area); (2) ENCODER proof: on a scratch copy every image's
// own pixels are force re-encoded (replace_image_rgba force=true) and must render identically (types 1, 2, 3, 4); (3) WRITER proof: on the pristine
// bank every image's own pixels are re-imported normally (unchanged pixels leave the stored bytes alone) and the whole bank must stay BYTE-IDENTICAL;
// (4) images the engine cannot draw (type -1: no pixel data / empty bounds / bad table) have nothing to import and are counted by reason; their
// bytes are covered by the whole-bank comparison in (3).
static int CmdCgRt(int argc, char **argv)
{
	Section s; s.name = "cg";
	static CG *cg = new CG(), *scratch = new CG();
	ForEachEntry(argc, argv, s, [&](const std::string &label, const std::string &name, const std::vector<uint8_t> &b) {
		Kind k = Classify(b);
		if (k != Kind::Han2) { NaAdd(s, name, k); return; }
		han2::Han2File f; std::string err;
		if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); s.fail++; return; }
		const auto &cgb = f.area[han2::kAreaCg];
		if (cgb.empty()) { s.notes["empty CG area " + ExtOf(name) + " (counted in pass: no bank to load; the container round trip proves the empty area)"]++; s.pass++; return; }
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
	if (argc < 2) { puts("export <in.DAT|DT2> <outdir> [--no-cg] [--no-poses] [--no-patterns] [--scale N]"); return 2; }
	han2::ExportOptions opt;
	for (int i = 2; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--no-cg") opt.cgImages = false; else if (a == "--no-poses") opt.poses = false; else if (a == "--no-patterns") opt.patterns = false;
		else if (a == "--scale" && i + 1 < argc) opt.scale = atoi(argv[++i]);
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
	printf("exported: %d CG png, %d pose png, %d frame png, %d patterns, %d strips/sheets\n", rep.cgPngs, rep.posePngs, rep.framePngs, rep.patternsWritten, rep.sheets);
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
		Kind k = Classify(b);
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

// gof1rt: GOF1 .p archives. (1) the archive rebuilt entry by entry from plain bytes (cipher undone and re-applied, own index re-encoded) must equal the
// file; (2) every character .DAT: stage-1 + stage-2 decrypt -> model -> save must equal the decrypted file and the re-encrypt must equal the stored bytes.
// Non-.DAT entries (sound/graphics/data blobs: no structured reader) are raw passthrough, proven byte-exact by (1); they are listed as n/a by extension.
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
			if (!EndsWithNoCase(a.entries[k].name, ".DAT")) { s.na[ExtOf(a.entries[k].name) + " (opaque, proven by the archive rebuild)"]++; continue; }
			std::vector<uint8_t> stored, plain;
			if (!gof1::ReadEntry(a, k, stored, &err)) { printf("FAIL %s::%s: %s\n", argv[i], a.entries[k].name.c_str(), err.c_str()); s.fail++; continue; }
			plain = stored; gof1::DecryptDat(plain);
			FrameData fd;
			if (!gof1::Load(fd, plain.data(), plain.size(), &err)) { printf("FAIL %s::%s: load: %s\n", argv[i], a.entries[k].name.c_str(), err.c_str()); s.fail++; continue; }
			std::vector<uint8_t> out;
			if (!gof1::Serialize(fd, out, &err)) { printf("FAIL %s: %s\n", a.entries[k].name.c_str(), err.c_str()); s.fail++; continue; }
			std::vector<uint8_t> enc = out; gof1::EncryptDat(enc);
			if (k == 0 || getenv("PARTS")) { auto cgp = new CG(); Parts &pp = *new Parts(cgp); std::string pe; const auto &blob = fd.m_han2->parts; bool ok = han2::PatToParts(blob.data(), blob.size(), pp, &pe); printf("  %s parts: %s, %zu part sets, %zu cutouts, %zu textures %s\n", a.entries[k].name.c_str(), ok ? "ok" : "FAIL", pp.partSets.size(), pp.cutOuts.size(), pp.gfxMeta.size(), pe.c_str()); }
			if (out == plain && enc == stored) s.pass++;
			else { size_t d = 0; while (d < std::min(out.size(), plain.size()) && out[d] == plain[d]) d++; printf("FAIL %s::%s: first diff 0x%zx (sizes %zu vs %zu)%s\n", argv[i], a.entries[k].name.c_str(), d, out.size(), plain.size(), enc == stored ? "" : " re-encrypt differs"); s.fail++; }
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
	if (c == "cgrt") return CmdCgRt(argc - 2, argv + 2);
	if (c == "diff") return CmdDiff(argc - 2, argv + 2);
	if (c == "animtest") return CmdAnimTest(argc - 2, argv + 2);
	if (c == "gof1rt") return CmdGof1Rt(argc - 2, argv + 2);
	if (c == "gof1shift") return CmdGof1Shift(argc - 2, argv + 2);
	if (c == "opaquert") return CmdOpaqueRt(argc - 2, argv + 2);
	if (c == "census") return CmdCensus(argc - 2, argv + 2);
	if (c == "pacrt") return CmdPacRt(argc - 2, argv + 2);
	printf("unknown command %s\n", c.c_str());
	return 2;
}
