// fbarctool: command line front end of the generalized French-Bread archive layer (src/fbarc).
//
//   fbarctool ls <archive>                     header facts + every entry (index, offset, size, path)
//   fbarctool count <archive>...               kind and entry count
//   fbarctool rt <archive>...                  rebuild with no edits; the result must equal the file byte for byte
//   fbarctool extract <archive> <path|-all> <outdir>
//   fbarctool pack <archive-as-template> <dir> <out>   rebuild with every file of <dir> replacing the same-named entry
//   fbarctool magics <archive>...              per-extension histogram of the first 4 plain bytes (cipher sanity)
//
// Exit code 0 when everything passed. Arguments are UTF-8.
#include "fb_archive.h"
#include "../han2/misc_formats.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <strings.h>

using namespace fbarc;
namespace fs = std::filesystem;

static std::string ExtOf(const std::string& n) { size_t d = n.rfind('.'); return d == std::string::npos ? std::string() : n.substr(d); }

static int CmdLs(int argc, char** argv)
{
	if (argc < 1) return 2;
	std::string err; auto a = Open(argv[0], &err);
	if (!a) { printf("FAIL %s: %s\n", argv[0], err.c_str()); return 1; }
	printf("%s\n", a->describe().c_str());
	for (size_t i = 0; i < a->entries().size(); i++)
		printf("%6zu %10llu %10llu %s\n", i, (unsigned long long)a->entries()[i].offset, (unsigned long long)a->entries()[i].size, NameToUtf8(a->relativePath(i)).c_str());
	return 0;
}

static int CmdCount(int argc, char** argv)
{
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		std::string err; auto a = Open(argv[i], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		uint64_t total = 0; for (auto& e : a->entries()) total += e.size;
		printf("OK   %-48s %-40s %6zu entries %12llu bytes\n", argv[i], KindName(a->kind()), a->entries().size(), (unsigned long long)total);
	}
	return fails ? 1 : 0;
}

static int CmdRt(int argc, char** argv)
{
	int fails = 0;
	for (int i = 0; i < argc; i++) {
		std::string err; auto a = Open(argv[i], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fails++; continue; }
		std::string d;
		if (VerifyRebuild(*a, &d)) printf("OK   %s byte-identical (%s, %zu entries)\n", argv[i], KindName(a->kind()), a->entries().size());
		else { printf("DIFF %s: %s\n", argv[i], d.c_str()); fails++; }
	}
	printf("SECTION fbrt: pass %d fail %d skipped 0\n", argc - fails, fails);
	return fails ? 1 : 0;
}

static bool WriteFile(const fs::path& p, const std::vector<uint8_t>& b)
{
	std::ofstream f(p, std::ios::binary);
	f.write((const char*)b.data(), (std::streamsize)b.size());
	return (bool)f;
}

static int CmdExtract(int argc, char** argv)
{
	if (argc < 3) return 2;
	std::string err; auto a = Open(argv[0], &err);
	if (!a) { printf("FAIL %s: %s\n", argv[0], err.c_str()); return 1; }
	const bool all = !strcmp(argv[1], "-all");
	int n = 0, bad = 0;
	for (size_t i = 0; i < a->entries().size(); i++) {
		if (!all && a->find(argv[1]) != (int)i) continue;
		std::vector<uint8_t> d;
		if (!a->read(i, d, &err)) { printf("FAIL %s: %s\n", a->relativePath(i).c_str(), err.c_str()); bad++; continue; }
		fs::path out = fs::u8path(argv[2]) / fs::u8path(NameToUtf8(a->relativePath(i)));
		std::error_code ec; fs::create_directories(out.parent_path(), ec);
		if (!WriteFile(out, d)) { printf("FAIL write %s\n", out.string().c_str()); bad++; continue; }
		n++;
	}
	printf("extracted %d entries\n", n);
	return bad ? 1 : 0;
}

static int CmdPack(int argc, char** argv)
{
	if (argc < 3) return 2;
	std::string err; auto a = Open(argv[0], &err);
	if (!a) { printf("FAIL %s: %s\n", argv[0], err.c_str()); return 1; }
	Edit e; int repl = 0, added = 0;
	std::error_code ec;
	for (auto& it : fs::recursive_directory_iterator(fs::u8path(argv[1]), ec)) {
		if (!it.is_regular_file()) continue;
		std::string rel = fs::relative(it.path(), fs::u8path(argv[1])).generic_u8string();
		std::ifstream f(it.path(), std::ios::binary);
		std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		int idx = a->find(NameFromUtf8(rel));
		if (idx >= 0) { e.replace[(size_t)idx] = std::move(d); repl++; }
		else { e.add.emplace_back(NameFromUtf8(it.path().filename().u8string()), std::move(d)); added++; }
	}
	if (!a->rebuild(argv[2], e, &err)) { printf("FAIL: %s\n", err.c_str()); return 1; }
	printf("wrote %s (%d replaced, %d added)\n", argv[2], repl, added);
	return 0;
}

static int CmdMagics(int argc, char** argv)
{
	for (int k = 0; k < argc; k++) {
		std::string err; auto a = Open(argv[k], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[k], err.c_str()); return 1; }
		std::map<std::string, std::map<std::string, int>> h;
		for (size_t i = 0; i < a->entries().size(); i++) {
			std::vector<uint8_t> d; a->read(i, d, &err, 8);
			std::string ext = ExtOf(a->entries()[i].name);
			for (auto& c : ext) c = (char)toupper((unsigned char)c);
			char buf[32] = "";
			for (size_t j = 0; j < 4 && j < d.size(); j++) { unsigned char c = d[j]; snprintf(buf + strlen(buf), 8, (c >= 32 && c < 127) ? "%c" : "\\x%02x", c); }
			h[ext][buf]++;
		}
		printf("%s\n", argv[k]);
		for (auto& [ext, m] : h) { printf("  %-8s", ext.c_str()); int shown = 0; for (auto& [mg, c] : m) { if (shown++ < 4) printf(" [%s]x%d", mg.c_str(), c); } if (m.size() > 4) printf(" (+%zu more)", m.size() - 4); printf("\n"); }
	}
	return 0;
}

// census <archive>...: TSV  archive, kind, ext, magic(hex of the first 4 plain bytes), count, total bytes   (input of docs/formats/fb_format_matrix.md)
static int CmdCensus(int argc, char** argv)
{
	for (int k = 0; k < argc; k++) {
		std::string err; auto a = Open(argv[k], &err);
		if (!a) { printf("FAIL\t%s\t%s\n", argv[k], err.c_str()); continue; }
		std::map<std::pair<std::string, std::string>, std::pair<uint64_t, uint64_t>> h;
		for (size_t i = 0; i < a->entries().size(); i++) {
			std::vector<uint8_t> d; a->read(i, d, &err, 4);
			std::string ext = ExtOf(a->entries()[i].name);
			for (auto& c : ext) c = (char)toupper((unsigned char)c);
			char buf[16] = ""; for (size_t j = 0; j < 4 && j < d.size(); j++) snprintf(buf + strlen(buf), 4, "%02x", d[j]);
			auto& g = h[{ext, buf}]; g.first++; g.second += a->entries()[i].size;
		}
		for (auto& [key, v] : h) printf("%s\t%s\t%s\t%s\t%llu\t%llu\n", argv[k], KindName(a->kind()), key.first.c_str(), key.second.c_str(), (unsigned long long)v.first, (unsigned long long)v.second);
	}
	return 0;
}

// edittest <archive>...: replace one entry (size changes), remove one, add one (in a folder), rebuild, reopen and check every entry
// against what it should be. Proves the writer for edited archives, not only for the unchanged rebuild.
static int CmdEditTest(int argc, char** argv)
{
	int fails = 0;
	for (int k = 0; k < argc; k++) {
		std::string err; auto a = Open(argv[k], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[k], err.c_str()); fails++; continue; }
		const size_t n = a->entries().size();
		if (n < 3) { printf("FAIL %s: fewer than 3 entries\n", argv[k]); fails++; continue; }
		// smallest non-empty entry gets replaced, the next smallest removed
		std::vector<size_t> ord; for (size_t i = 0; i < n; i++) if (a->entries()[i].size > 0) ord.push_back(i);
		std::sort(ord.begin(), ord.end(), [&](size_t x, size_t y) { return a->entries()[x].size < a->entries()[y].size; });
		if (ord.size() < 3) { printf("FAIL %s: fewer than 3 non-empty entries\n", argv[k]); fails++; continue; }
		const size_t rep = ord[0], rem = ord[1], keep = ord[2];
		std::vector<uint8_t> orig; a->read(rep, orig, &err);
		std::vector<uint8_t> nw = orig; for (int i = 0; i < 5000; i++) nw.push_back((uint8_t)(i * 7 + 1)); if (!nw.empty()) nw[0] ^= 0x5A;
		std::vector<uint8_t> added(9000); for (size_t i = 0; i < added.size(); i++) added[i] = (uint8_t)(i * 13 + 5);
		Edit e; e.replace[rep] = nw; e.remove.insert(rem);
		const bool folders = a->kind() == Kind::MbFilePacA;
		const std::string addName = folders ? "FBTESTDIR/FBTEST.BIN" : "FBTEST.BIN";
		e.add.emplace_back(addName, added);
		std::error_code ec; const std::string tmp = (fs::temp_directory_path(ec) / "fbarc_edit.bin").u8string();
		if (!a->rebuild(tmp, e, &err)) { printf("FAIL %s: rebuild: %s\n", argv[k], err.c_str()); fails++; continue; }
		auto b = Open(tmp, &err);
		bool ok = (bool)b;
		if (!ok) printf("FAIL %s: reopen: %s\n", argv[k], err.c_str());
		else {
			ok = b->entries().size() == n; // -1 +1
			if (!ok) printf("FAIL %s: entry count %zu, expected %zu\n", argv[k], b->entries().size(), n);
			int ir = b->find(a->relativePath(rep)), im = b->find(a->relativePath(rem)), ik = b->find(a->relativePath(keep)), ia = b->find(addName);
			std::vector<uint8_t> x;
			if (ir < 0 || !b->read((size_t)ir, x, &err) || x != nw) { printf("FAIL %s: replaced entry wrong\n", argv[k]); ok = false; }
			if (im >= 0) { printf("FAIL %s: removed entry still present\n", argv[k]); ok = false; }
			std::vector<uint8_t> o2, k2;
			if (ik < 0 || !b->read((size_t)ik, k2, &err) || !a->read(keep, o2, &err) || k2 != o2) { printf("FAIL %s: untouched entry changed\n", argv[k]); ok = false; }
			if (ia < 0 || !b->read((size_t)ia, x, &err) || x != added) { printf("FAIL %s: added entry wrong\n", argv[k]); ok = false; }
			// every surviving original entry must read back identical
			if (ok) for (size_t i = 0; i < n && ok; i++) {
				if (i == rep || i == rem) continue;
				int j = b->find(a->relativePath(i)); std::vector<uint8_t> p, q;
				if (j < 0 || !a->read(i, p, &err) || !b->read((size_t)j, q, &err) || p != q) { printf("FAIL %s: entry %s differs after edit\n", argv[k], a->relativePath(i).c_str()); ok = false; }
			}
		}
		b.reset();
		fs::remove(fs::u8path(tmp), ec);
		if (ok) printf("OK   %s edit (replace+remove+add) verified\n", argv[k]); else fails++;
	}
	printf("SECTION fbedit: pass %d fail %d skipped 0\n", argc - fails, fails);
	return fails ? 1 : 0;
}

// ex3rt <archive>...: every .EX3 entry: parse (header size auto-detected) -> serialize must be byte-identical, decoded size must equal the header word
static int CmdEx3Rt(int argc, char** argv)
{
	int pass = 0, fail = 0; std::map<std::string, int> layouts;
	for (int k = 0; k < argc; k++) {
		std::string err; auto a = Open(argv[k], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[k], err.c_str()); fail++; continue; }
		int p0 = pass, f0 = fail;
		for (size_t i = 0; i < a->entries().size(); i++) {
			const std::string& nm = a->entries()[i].name;
			if (ExtOf(nm).size() != 4 || strcasecmp(ExtOf(nm).c_str(), ".EX3") != 0) continue;
			std::vector<uint8_t> d; if (!a->read(i, d, &err)) { printf("FAIL %s::%s: %s\n", argv[k], nm.c_str(), err.c_str()); fail++; continue; }
			han2::Ex3 x; std::string e2;
			if (!han2::ParseEx3Auto(d.data(), d.size(), x, &e2)) { printf("FAIL %s::%s: %s\n", argv[k], nm.c_str(), e2.c_str()); fail++; continue; }
			std::vector<uint8_t> out; han2::SerializeEx3(x, out);
			std::vector<uint8_t> bmp; std::string e3;
			if (!han2::DecodeEx3(x, bmp, &e3) || bmp.size() != x.decodedBytes || bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M') { printf("FAIL %s::%s: decoded payload is not a BMP (%s)\n", argv[k], nm.c_str(), e3.c_str()); fail++; continue; }
			if (out == d) { pass++; layouts["header " + std::to_string(x.headerSize)]++; } else { printf("DIFF %s::%s\n", argv[k], nm.c_str()); fail++; }
		}
		printf("%-60s pass %d fail %d\n", argv[k], pass - p0, fail - f0);
	}
	for (auto& [l, n] : layouts) printf("  %s: %d files\n", l.c_str(), n);
	printf("SECTION ex3: pass %d fail %d skipped 0\n", pass, fail);
	return fail ? 1 : 0;
}

static int Run(int argc, char** argv)
{
	if (argc < 2) { puts("usage: fbarctool ls|count|rt|extract|pack|magics ..."); return 2; }
	std::string c = argv[1];
	if (c == "ls") return CmdLs(argc - 2, argv + 2);
	if (c == "count") return CmdCount(argc - 2, argv + 2);
	if (c == "rt") return CmdRt(argc - 2, argv + 2);
	if (c == "extract") return CmdExtract(argc - 2, argv + 2);
	if (c == "pack") return CmdPack(argc - 2, argv + 2);
	if (c == "census") return CmdCensus(argc - 2, argv + 2);
	if (c == "edittest") return CmdEditTest(argc - 2, argv + 2);
	if (c == "ex3rt") return CmdEx3Rt(argc - 2, argv + 2);
	if (c == "magics") return CmdMagics(argc - 2, argv + 2);
	printf("unknown command %s\n", c.c_str());
	return 2;
}

#ifdef _WIN32
#include <windows.h>
// Arguments arrive as UTF-16 and are handed to the commands as UTF-8 (archive names with spaces / Japanese characters).
int wmain(int argc, wchar_t** argv)
{
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::string> a;
	for (int i = 0; i < argc; i++) {
		const int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
		std::string u(n > 0 ? n : 1, '\0');
		if (n > 0) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, u.data(), n, nullptr, nullptr);
		u.pop_back();
		a.push_back(u);
	}
	std::vector<char*> p; for (auto& s : a) p.push_back(s.data());
	return Run(argc, p.data());
}
#else
int main(int argc, char** argv) { return Run(argc, argv); }
#endif
