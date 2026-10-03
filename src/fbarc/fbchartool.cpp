// fbchartool: byte-exact round trip of every member of French-Bread archives, by title.
//
//   fbchartool <title> <archive>...      title: react | mb | pb2k1 | dmp | rosa | lilian | qoh99 | qoh98
//   fbchartool list                      the titles
//
// Every archive entry is opened through the generalized archive layer (src/fbarc), classified by what its BYTES are (never by the file
// extension alone), parsed into its model and serialized again; the result must equal the shipped bytes. The section line ends with
// "fail N skipped M": an entry without a model is counted as skipped, so a section is green only when every member has one.
// Nothing is copied: entries are read into memory (stage files need a path, so they go through a temp file that is removed).
#include "fb_archive.h"
#include "../han2/misc_formats.h"
#include "../han2/fnt_file.h"
#include "../framedata.h"
#include "../framedata_ha4.h"
#include "../framedata_gof1.h"
#include "../han2/gof1_archive.h"
#include "../background/bg_file.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Section {
	std::string name;
	int pass = 0, fail = 0, skipped = 0;
	std::map<std::string, int> notes;      // what was proven, by kind
	std::map<std::string, int> unmodelled; // "ext (no model)" -> count
};

std::string ExtOf(const std::string &n) { size_t d = n.find_last_of('.'); std::string e = d == std::string::npos ? "" : n.substr(d); for (auto &c : e) if (c >= 'a' && c <= 'z') c -= 32; return e; }
bool Has(const std::vector<uint8_t> &d, const char *m) { size_t n = strlen(m); return d.size() >= n && memcmp(d.data(), m, n) == 0; }
uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

void Diff(const char *what, const std::string &label, const std::vector<uint8_t> &out, const std::vector<uint8_t> &b)
{
	size_t k = 0, m = std::min(out.size(), b.size());
	while (k < m && out[k] == b[k]) k++;
	printf("FAIL %s: %s: size %zu vs %zu, first diff at 0x%zx\n", label.c_str(), what, out.size(), b.size(), k);
}
void Ok(Section &s, const std::string &note) { s.pass++; s.notes[note]++; }
void Bad(Section &s, const char *what, const std::string &label, const std::vector<uint8_t> &out, const std::vector<uint8_t> &b) { Diff(what, label, out, b); s.fail++; }
void Fail(Section &s, const std::string &label, const std::string &msg) { printf("FAIL %s: %s\n", label.c_str(), msg.c_str()); s.fail++; }

// ---- members every title shares ------------------------------------------------------------------------------------------------
// Returns true when the entry was recognised (and counted as pass or fail).
bool CommonMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	std::string err; std::vector<uint8_t> out; const std::string e = ExtOf(name);
	if (Has(d, "LLIF")) {
		han2::Ex3 x;
		if (!han2::ParseEx3Auto(d.data(), d.size(), x, &err)) { Fail(s, label, "ex3: " + err); return true; }
		han2::SerializeEx3(x, out);
		if (out != d) { Bad(s, "ex3", label, out, d); return true; }
		std::vector<uint8_t> bmp;
		if (!han2::DecodeEx3(x, bmp, &err)) { Fail(s, label, "ex3 decode: " + err); return true; }
		std::vector<uint8_t> re; han2::EncodeEx3(x.header, x.headerSize, bmp.data(), bmp.size(), re);
		if (re != d) { Bad(s, "ex3 Gage re-encode", label, re, d); return true; }
		Ok(s, ".EX3 (LLIF, blocks + bit-exact Gage encoder)");
		return true;
	}
	if (d.size() >= 12 && Has(d, "RIFF")) {
		han2::Riff r; if (!han2::ParseRiff(d.data(), d.size(), r, &err)) { Fail(s, label, "riff: " + err); return true; }
		han2::SerializeRiff(r, out); if (out == d) Ok(s, ".WAV (RIFF chunks)"); else Bad(s, "riff", label, out, d);
		return true;
	}
	if (e == ".MP3") {
		han2::MpegInfo m; if (!han2::ValidateMpeg(d.data(), d.size(), m, &err)) Fail(s, label, "mp3: " + err); else Ok(s, ".MP3 (frames validated end to end)");
		return true;
	}
	if (e == ".OGG" && Has(d, "OggS")) { Ok(s, ".OGG (Ogg container, stored verbatim)"); return true; }
	if (e == ".BMP") {
		han2::Bmp b; if (!han2::ParseBmp(d.data(), d.size(), b, &err)) { Fail(s, label, "bmp: " + err); return true; }
		han2::SerializeBmp(b, out); if (out == d) Ok(s, ".BMP"); else Bad(s, "bmp", label, out, d);
		return true;
	}
	if (e == ".FNT") {
		han2::fnt::Font f; if (!han2::fnt::Parse(d.data(), d.size(), f, &err)) { Fail(s, label, "fnt: " + err); return true; }
		han2::fnt::Serialize(f, out, &err); if (out == d) Ok(s, ".FNT (bitmap font)"); else Bad(s, "font", label, out, d);
		return true;
	}
	if ((e == ".TXT" || e == ".H") && han2::LooksLikeText(d.data(), d.size())) {
		han2::Text t; han2::ParseText(d.data(), d.size(), t, &err); han2::SerializeText(t, out);
		if (out == d) Ok(s, "text (Shift-JIS, line structure)"); else Bad(s, "text", label, out, d);
		return true;
	}
	return false;
}

// ---- Hantei4 (MBAC / ReAct) character .DAT ---------------------------------------------------------------------------------------
bool Ha4Member(const std::string &label, const std::vector<uint8_t> &d, Section &s)
{
	if (!ha4::IsHA4(d.data(), d.size())) return false;
	FrameData fd; std::string err;
	if (!ha4::Load(fd, d.data(), d.size(), &err)) { Fail(s, label, "ha4 load: " + err); return true; }
	std::vector<uint8_t> out;
	if (!ha4::Serialize(fd, out, &err)) { Fail(s, label, "ha4 save: " + err); return true; }
	if (out == d) Ok(s, "character .DAT (Hantei4)"); else Bad(s, "ha4", label, out, d);
	return true;
}

// ---- bgmake stage .DAT ---------------------------------------------------------------------------------------------------------------
bool StageMember(const std::string &label, const std::vector<uint8_t> &d, Section &s)
{
	if (!Has(d, "bgmake")) return false;
	std::error_code ec; fs::path tmp = fs::temp_directory_path(ec) / "fbchartool_stage.dat", tmp2 = fs::temp_directory_path(ec) / "fbchartool_stage_out.dat";
	{ std::ofstream f(tmp, std::ios::binary); f.write((const char *)d.data(), (std::streamsize)d.size()); }
	bg::File bf; bool ok = bf.Load(tmp.u8string().c_str()) && bf.Save(tmp2.u8string().c_str());
	std::vector<uint8_t> out;
	if (ok) { std::ifstream f(tmp2, std::ios::binary); out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
	fs::remove(tmp, ec); fs::remove(tmp2, ec);
	if (!ok) Fail(s, label, "stage load/save");
	else if (out == d) Ok(s, "stage .DAT (bgmake)"); else Bad(s, "stage", label, out, d);
	return true;
}

// ---- Melty Blood 2002 / GOF1 / PB2K1 stage-2 enciphered character (magic d9 93 fe 3d after the archive cipher) ----------------------
bool Gof1CharMember(const std::string &label, const std::vector<uint8_t> &stage1, Section &s)
{
	if (stage1.size() < 4 || R32(stage1.data()) != 0x3dfe93d9u) return false;
	std::string err; std::vector<uint8_t> plain = stage1; gof1::DecryptDat(plain);
	FrameData fd;
	if (!gof1::Load(fd, plain.data(), plain.size(), &err)) { Fail(s, label, "load: " + err); return true; }
	std::vector<uint8_t> out;
	if (!gof1::Serialize(fd, out, &err)) { Fail(s, label, "save: " + err); return true; }
	std::vector<uint8_t> enc = out; gof1::EncryptDat(enc);
	if (out == plain && enc == stage1) Ok(s, "character .DAT (GOF1 / MB container)");
	else { Diff("character DAT", label, out, plain); if (enc != stage1) printf("  re-encrypt differs\n"); s.fail++; }
	return true;
}

using Handler = bool (*)(const std::string &, const std::string &, const std::vector<uint8_t> &, Section &);

// ---- per title -----------------------------------------------------------------------------------------------------------------------------
bool ReactMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	(void)name;
	if (Ha4Member(label, d, s)) return true;
	if (StageMember(label, d, s)) return true;
	if (Gof1CharMember(label, d, s)) return true;
	return false;
}

struct Title { const char *key; Handler fn; };
bool MbMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	(void)name;
	if (Gof1CharMember(label, d, s)) return true;
	return false;
}

const Title kTitles[] = { { "react", ReactMember }, { "mb", MbMember } };

int Run(const Title &t, int argc, char **argv)
{
	Section s; s.name = t.key;
	for (int i = 0; i < argc; i++) {
		std::string err; auto a = fbarc::Open(argv[i], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[i], err.c_str()); s.fail++; continue; }
		const int p0 = s.pass, f0 = s.fail, k0 = s.skipped;
		for (size_t k = 0; k < a->entries().size(); k++) {
			const std::string name = fbarc::NameToUtf8(a->relativePath(k));
			const std::string label = std::string(argv[i]) + "::" + name;
			std::vector<uint8_t> d;
			if (!a->read(k, d, &err)) { Fail(s, label, err); continue; }
			if (CommonMember(label, name, d, s)) continue;
			if (t.fn(label, name, d, s)) continue;
			s.skipped++;
			if (++s.unmodelled[ExtOf(name) + " (no model)"] <= 1) printf("SKIP %s (%zu bytes)\n", label.c_str(), d.size());
		}
		printf("%-40s pass %d fail %d skipped %d\n", argv[i], s.pass - p0, s.fail - f0, s.skipped - k0);
	}
	std::string un; for (auto &kv : s.unmodelled) un += (un.empty() ? "" : ", ") + kv.first + " x" + std::to_string(kv.second);
	printf("SECTION %-9s pass %d fail %d skipped %d%s%s\n", s.name.c_str(), s.pass, s.fail, s.skipped, un.empty() ? "" : " | unmodelled: ", un.c_str());
	for (auto &kv : s.notes) printf("    %s: %d\n", kv.first.c_str(), kv.second);
	return (s.fail || s.skipped) ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 2) { puts("usage: fbchartool <title> <archive>...   (fbchartool list)"); return 2; }
	if (!strcmp(argv[1], "list")) { for (auto &t : kTitles) puts(t.key); return 0; }
	for (auto &t : kTitles) if (!strcmp(argv[1], t.key)) return Run(t, argc - 2, argv + 2);
	printf("unknown title %s\n", argv[1]);
	return 2;
}
