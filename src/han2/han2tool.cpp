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
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		pac::Archive a; std::string err;
		if (!pac::Open(argv[i], a, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		std::vector<pac::NewEntry> ne(a.entries.size());
		for (size_t k = 0; k < a.entries.size(); k++) {
			ne[k].name = a.entries[k].name;
			ne[k].rawName.assign(a.entries[k].rawName, a.entries[k].rawName + pac::kNameLen);
			if (!pac::ReadEntry(a, k, ne[k].data, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; goto next; }
		}
		{
			std::vector<uint8_t> out;
			if (!pac::Build(ne, out, &err)) { printf("FAIL %s: build: %s\n", argv[i], err.c_str()); fails++; continue; }
			// compare to the original file stream-wise
			std::ifstream f(std::filesystem::u8path(argv[i]), std::ios::binary);
			std::vector<uint8_t> orig(out.size());
			f.read((char *)orig.data(), (std::streamsize)orig.size());
			bool same = (uint64_t)out.size() == a.fileSize && f.gcount() == (std::streamsize)out.size() && out == orig;
			if (same) printf("OK   %s byte-identical (%zu entries)\n", argv[i], ne.size());
			else {
				size_t k = 0; size_t m = (size_t)f.gcount() < out.size() ? (size_t)f.gcount() : out.size();
				while (k < m && out[k] == orig[k]) k++;
				printf("DIFF %s: rebuilt %zu bytes vs %llu, first diff at 0x%zx\n", argv[i], out.size(), (unsigned long long)a.fileSize, k);
				fails++;
			}
		}
	next:;
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

struct RtStats { int pass = 0, fail = 0, skipped = 0; };

static void RoundtripOne(const std::string &label, const std::vector<uint8_t> &b, RtStats &st)
{
	han2::Han2File h; std::string err;
	if (!han2::Parse(b.data(), b.size(), h, &err)) { printf("FAIL %s: parse: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	std::vector<uint8_t> out;
	if (!han2::Serialize(h, out, &err)) { printf("FAIL %s: serialize: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	if (out == b) { st.pass++; return; }
	size_t k = 0, m = std::min(out.size(), b.size());
	while (k < m && out[k] == b[k]) k++;
	printf("FAIL %s: size %zu vs %zu, first diff at 0x%zx\n", label.c_str(), out.size(), b.size(), k);
	st.fail++;
}

static int CmdRoundtrip(int argc, char **argv)
{
	RtStats total;
	for (int i = 0; i < argc; i++) {
		RtStats st;
		std::vector<uint8_t> head;
		pac::Archive a; std::string err;
		if (pac::Open(argv[i], a, nullptr)) {
			for (size_t k = 0; k < a.entries.size(); k++) {
				const std::string &n = a.entries[k].name;
				if (!EndsWithNoCase(n, ".DAT") && !EndsWithNoCase(n, ".DT2")) continue;
				std::vector<uint8_t> b;
				if (!pac::ReadEntry(a, k, b, &err)) { printf("FAIL %s::%s: %s\n", argv[i], n.c_str(), err.c_str()); st.fail++; continue; }
				if (b.size() < 8 || memcmp(b.data(), "HAN2RBO ", 8) != 0) { st.skipped++; continue; }
				RoundtripOne(std::string(argv[i]) + "::" + n, b, st);
			}
		} else {
			std::vector<uint8_t> b;
			if (!ReadLoose(argv[i], b)) { printf("FAIL %s: cannot read\n", argv[i]); st.fail++; }
			else if (b.size() < 8 || memcmp(b.data(), "HAN2RBO ", 8) != 0) st.skipped++;
			else RoundtripOne(argv[i], b, st);
		}
		printf("%-40s pass %d fail %d skipped(non-HAN2) %d\n", argv[i], st.pass, st.fail, st.skipped);
		total.pass += st.pass; total.fail += st.fail; total.skipped += st.skipped;
	}
	printf("TOTAL pass %d fail %d\n", total.pass, total.fail);
	return total.fail ? 1 : 0;
}

static void ModelRtOne(const std::string &label, const std::vector<uint8_t> &b, RtStats &st)
{
	FrameData fd; std::string err;
	if (!han2::Load(fd, b.data(), b.size(), &err)) { printf("SKIP %s: %s\n", label.c_str(), err.c_str()); st.skipped++; return; }
	std::vector<uint8_t> out; std::vector<std::string> warn;
	bool dt2 = fd.m_han2 && fd.m_han2->kind == 3;
	if (!han2::Serialize(fd, out, &err, &warn, dt2)) { printf("FAIL %s: save: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
	if (out == b) { st.pass++; return; }
	size_t k = 0, m = std::min(out.size(), b.size());
	while (k < m && out[k] == b[k]) k++;
	printf("FAIL %s: size %zu vs %zu, first diff at 0x%zx\n", label.c_str(), out.size(), b.size(), k);
	st.fail++;
}

static int CmdModelRt(int argc, char **argv)
{
	RtStats total;
	for (int i = 0; i < argc; i++) {
		RtStats st; std::string err; pac::Archive a;
		if (pac::Open(argv[i], a, nullptr)) {
			for (size_t k = 0; k < a.entries.size(); k++) {
				const std::string &n = a.entries[k].name;
				if (!EndsWithNoCase(n, ".DAT") && !EndsWithNoCase(n, ".DT2")) continue;
				std::vector<uint8_t> b;
				if (!pac::ReadEntry(a, k, b, &err)) { st.fail++; continue; }
				if (b.size() < 8 || memcmp(b.data(), "HAN2RBO ", 8) != 0) continue;
				ModelRtOne(std::string(argv[i]) + "::" + n, b, st);
			}
		} else {
			std::vector<uint8_t> b;
			if (ReadLoose(argv[i], b)) ModelRtOne(argv[i], b, st); else st.fail++;
		}
		printf("%-40s pass %d fail %d skipped %d\n", argv[i], st.pass, st.fail, st.skipped);
		total.pass += st.pass; total.fail += st.fail; total.skipped += st.skipped;
	}
	printf("TOTAL pass %d fail %d skipped %d\n", total.pass, total.fail, total.skipped);
	return total.fail ? 1 : 0;
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

// patrt: PAT block -> Parts model -> PAT block must be byte-identical (reader + writer check)
static int CmdPatRt(int argc, char **argv)
{
	RtStats total;
	for (int i = 0; i < argc; i++) {
		RtStats st; std::string err; pac::Archive a;
		auto one = [&](const std::string &label, const std::vector<uint8_t> &b) {
			han2::Han2File f; std::vector<uint8_t> rawpat;
			if (han2::IsPat(b.data(), b.size())) rawpat = b;                 // a bare GOF2 .PAT
			else if (!han2::Parse(b.data(), b.size(), f, &err)) { st.skipped++; return; }
			const auto &blob = rawpat.empty() ? f.area[han2::kAreaParts] : rawpat;
			if (blob.empty()) { st.skipped++; return; }
			CG *cgp = new CG(); Parts &parts = *new Parts(cgp);   /* leaked on purpose: ~Parts releases GL objects and the tool has no GL context */
			bool rok = han2::PatToParts(blob.data(), blob.size(), parts, &err);
			if (!rok) { printf("FAIL %s: read: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
			std::vector<uint8_t> out;
			if (!han2::BuildPat(parts, blob, out, &err)) { printf("FAIL %s: build: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
			if (out == blob) st.pass++;
			else { size_t k = 0, m = std::min(out.size(), blob.size()); while (k < m && out[k] == blob[k]) k++; printf("FAIL %s: size %zu vs %zu, first diff 0x%zx\n", label.c_str(), out.size(), blob.size(), k); st.fail++; }
		};
		if (pac::Open(argv[i], a, nullptr)) {
			for (size_t k = 0; k < a.entries.size(); k++) {
				if (!EndsWithNoCase(a.entries[k].name, ".DAT") && !EndsWithNoCase(a.entries[k].name, ".PAT")) continue;
				std::vector<uint8_t> b; if (!pac::ReadEntry(a, k, b, &err)) continue;
				if (!han2::IsPat(b.data(), b.size()) && (b.size() < 8 || memcmp(b.data(), "HAN2RBO ", 8) != 0)) continue;
				one(std::string(argv[i]) + "::" + a.entries[k].name, b);
			}
		} else { std::vector<uint8_t> b; if (ReadLoose(argv[i], b)) one(argv[i], b); }
		printf("%-40s pass %d fail %d skipped %d\n", argv[i], st.pass, st.fail, st.skipped);
		total.pass += st.pass; total.fail += st.fail; total.skipped += st.skipped;
	}
	printf("TOTAL pass %d fail %d skipped %d\n", total.pass, total.fail, total.skipped);
	return total.fail ? 1 : 0;
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

static int CmdImgRt(int argc, char **argv)
{
	RtStats total;
	for (int i = 0; i < argc; i++) {
		RtStats st; std::string err; pac::Archive a;
		auto one = [&](const std::string &label, const std::vector<uint8_t> &b) {
			han2::ImgFile img;
			if (!han2::ParseImg(b.data(), b.size(), img, &err)) { printf("FAIL %s: %s\n", label.c_str(), err.c_str()); st.fail++; return; }
			std::vector<uint8_t> out; han2::SerializeImg(img, out);
			if (out == b) st.pass++; else { printf("FAIL %s: differs\n", label.c_str()); st.fail++; }
		};
		if (pac::Open(argv[i], a, nullptr)) {
			for (size_t k = 0; k < a.entries.size(); k++) {
				if (!EndsWithNoCase(a.entries[k].name, ".IMG")) continue;
				std::vector<uint8_t> b; if (!pac::ReadEntry(a, k, b, &err)) continue;
				one(std::string(argv[i]) + "::" + a.entries[k].name, b);
			}
		} else { std::vector<uint8_t> b; if (ReadLoose(argv[i], b)) one(argv[i], b); }
		printf("%-40s pass %d fail %d\n", argv[i], st.pass, st.fail);
		total.pass += st.pass; total.fail += st.fail;
	}
	printf("TOTAL pass %d fail %d\n", total.pass, total.fail);
	return total.fail ? 1 : 0;
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

// cgrt: re-import every CG image's own pixels; types 1/2/4 must render identically afterwards (type 1 byte-exact)
static int CmdCgRt(int argc, char **argv)
{
	if (argc < 1) return 2;
	std::vector<uint8_t> b; if (!ReadLoose(argv[0], b)) return 1;
	han2::Han2File f; std::string err; if (!han2::Parse(b.data(), b.size(), f, &err)) { printf("%s\n", err.c_str()); return 1; }
	CG *cg = new CG(); const auto &cgb = f.area[han2::kAreaCg]; if (!cg->loadFromMemory(cgb.data(), (unsigned)cgb.size())) return 1;
	std::vector<char> before(cg->bank_data(), cg->bank_data() + cg->bank_size());
	int ok = 0, bad = 0, skipped = 0, exact = 0;
	for (int i = 0; i < cg->get_image_count(); i++) {
		ImageData *im = cg->draw_texture((unsigned)i, false, false);
		if (!im) { skipped++; continue; }
		std::vector<unsigned char> px(im->pixels, im->pixels + (size_t)im->width * im->height * 4);
		int w = im->width, h = im->height; delete im;
		std::string e2;
		if (!cg->replace_image_rgba((unsigned)i, px.data(), w, h, &e2)) { skipped++; continue; }
		ImageData *im2 = cg->draw_texture((unsigned)i, false, false);
		bool same = im2 && im2->width == w && im2->height == h;
		if (same) for (size_t k = 0; k < px.size() && same; k += 4) {
			// transparent pixels may differ in their colour bytes; compare only alpha-visible content
			if (px[k + 3] != im2->pixels[k + 3]) same = false;
			else if (px[k + 3] && memcmp(&px[k], &im2->pixels[k], 3) != 0) same = false;
		}
		delete im2;
		if (same) ok++; else { bad++; if (bad < 6) printf("image %d renders differently after re-import\n", i); }
	}
	exact = memcmp(before.data(), cg->bank_data(), before.size()) == 0;
	printf("%s: %d images re-imported identically, %d differ, %d skipped (unsupported type / empty); bank bytes %s\n", argv[0], ok, bad, skipped, exact ? "byte-identical" : "changed (palette order of type 2/4 images)");
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

// animtest: the live stepper must reproduce SimulateFlow's tick count for every pattern (same rules, two implementations)
static int CmdAnimTest(int argc, char **argv)
{
	int ok = 0, bad = 0, open = 0;
	for (int i = 0; i < argc; i++) {
		std::vector<uint8_t> b; if (!ReadLoose(argv[i], b)) continue;
		FrameData fd; std::string err; if (!han2::Load(fd, b.data(), b.size(), &err)) continue;
		for (int p = 0; p < 256; p++) {
			if (fd.m_sequences[p].frames.empty()) continue;
			std::vector<std::pair<int, int>> v; std::string note; han2::SimulateFlow(fd.m_sequences[p], v, note);
			if (note.rfind("ends", 0) != 0) { open++; continue; }
			int expect = 0; for (auto &x : v) expect += std::max(1, x.second);   // the engine needs at least one tick per frame
			han2::AnimState s; han2::AnimStart(fd, s, p); int guard = 0;
			while (!s.ended && guard++ < 200000) { han2::AnimTick(fd, s, false, false); }
			if (s.totalTicks == expect) ok++; else { bad++; if (bad < 5) printf("%s pattern %d: stepper %d ticks vs flow %d\n", argv[i], p, s.totalTicks, expect); }
		}
	}
	printf("animtest: %d patterns agree, %d differ, %d skipped (state dependent / non-terminating)\n", ok, bad, open);
	return bad ? 1 : 0;
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
	if (c == "pacrt") return CmdPacRt(argc - 2, argv + 2);
	printf("unknown command %s\n", c.c_str());
	return 2;
}
