// han2tool: command line access to French-Bread RBO / GOF2 archives and HAN2RBO character files.
//
//   han2tool ls <archive>                      list entries (name, offset, size)
//   han2tool count <archive>...                print entry counts
//   han2tool extract <archive> <name|-all> <outdir>
//   han2tool roundtrip <archive|file>...       every HAN2RBO .DAT/.DT2 (inside archives or loose): parse -> serialize must be byte-identical
//   han2tool pacrt <archive>...                rebuild the archive from its entries; entries must match byte for byte
//
// Exit code 0 when everything passed.
#include "pac_archive.h"
#include "han2_container.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string>

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

int main(int argc, char **argv)
{
	if (argc < 2) { puts("usage: han2tool ls|count|extract|pacrt ..."); return 2; }
	std::string c = argv[1];
	if (c == "ls") return CmdLs(argc - 2, argv + 2);
	if (c == "count") return CmdCount(argc - 2, argv + 2);
	if (c == "extract") return CmdExtract(argc - 2, argv + 2);
	if (c == "roundtrip") return CmdRoundtrip(argc - 2, argv + 2);
	if (c == "pacrt") return CmdPacRt(argc - 2, argv + 2);
	printf("unknown command %s\n", c.c_str());
	return 2;
}
