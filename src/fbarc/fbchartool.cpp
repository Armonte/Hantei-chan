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
#include "../han2/fob_file.h"
#include "../han2/img_file.h"
#include "../han2/dmp_fob.h"
#include "../han2/dmp_types_gen.h"
#include "../han2/mbr_formats.h"
#include "../han2/mb_formats.h"
#include "../han2/pb2k1_types_gen.h"
#include "../framedata_pb2k1.h"
#include "../framedata.h"
#include "../framedata_ha4.h"
#include "../framedata_gof1.h"
#include "../han2/gof1_archive.h"
#include "../background/bg_file.h"
#include "../cg.h"
#include "../png_writer.h"
#include <windows.h>
#include <objbase.h>
#include "../han2/mb_cg.h"

#include <cstdio>
#include <memory>
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
	if (fd.m_han2 && !fd.m_han2->cg.empty()) {   // the embedded sprite bank must load as a CG bank
		auto cg = std::make_unique<CG>();
		if (cg->loadFromMemory(fd.m_han2->cg.data(), (unsigned)fd.m_han2->cg.size())) s.notes["  embedded BMP Cutter banks"]++;
		else {
			std::string ce; auto mbcg = han2::MbCgBank::Parse(fd.m_han2->cg.data(), fd.m_han2->cg.size(), &ce);
			if (!mbcg) { Fail(s, label, "embedded CG bank does not parse: " + ce); return true; }
			std::vector<uint8_t> back; mbcg->serialize(back);
			if (back != fd.m_han2->cg) { Fail(s, label, "embedded CG bank does not serialize identically"); return true; }
			unsigned drawn = 0;
			for (unsigned i = 0; i < mbcg->imageCount(); i++) { int b, t, x1, y1, x2, y2; if (!mbcg->imageInfo(i, b, t, x1, y1, x2, y2)) continue; std::unique_ptr<ImageData> im(mbcg->draw(i, false, false, mbcg->palette(0))); if (!im) { Fail(s, label, "MB CG image " + std::to_string(i) + " does not decode"); return true; } drawn++; }
			s.notes["  embedded MB strip bank (parsed, serialized, every image decoded)"]++; s.notes["  MB CG images decoded"] += (int)drawn;
		}
	}
	if (out == plain && enc == stage1) Ok(s, "character .DAT (GOF1 / MB container)");
	else { Diff("character DAT", label, out, plain); if (enc != stage1) printf("  re-encrypt differs\n"); s.fail++; }
	return true;
}

// Party Breakers character (three-section cipher) + embedded sprite bank
bool Pb2CharMember(const std::string &label, const std::vector<uint8_t> &stored, Section &s)
{
	if (!pb2k1::LooksLikeCharacter(stored.data(), stored.size())) return false;
	std::string err; std::vector<uint8_t> plain = stored;
	if (!pb2k1::Decrypt(plain)) { Fail(s, label, "decrypt"); return true; }
	FrameData fd;
	if (!pb2k1::Load(fd, plain.data(), plain.size(), &err)) { Fail(s, label, "load: " + err); return true; }
	std::vector<uint8_t> out;
	if (!pb2k1::Serialize(fd, out, &err)) { Fail(s, label, "save: " + err); return true; }
	std::vector<uint8_t> enc = out; pb2k1::Encrypt(enc);
	{
		std::string ce; auto bank = han2::MbCgBank::Parse(fd.m_han2->cg.data(), fd.m_han2->cg.size(), &ce, han2::kPb2CgLayout);
		if (!bank) { Fail(s, label, "sprite bank does not parse: " + ce); return true; }
		std::vector<uint8_t> back; bank->serialize(back);
		if (back != fd.m_han2->cg) { Fail(s, label, "sprite bank does not serialize identically"); return true; }
		unsigned drawn = 0;
		for (unsigned i = 0; i < bank->imageCount(); i++) { int b, t, x1, y1, x2, y2; if (!bank->imageInfo(i, b, t, x1, y1, x2, y2)) continue; std::unique_ptr<ImageData> im(bank->draw(i, false, false, bank->palette(0))); if (!im) { Fail(s, label, "sprite group " + std::to_string(i) + " does not decode"); return true; } drawn++; }
		s.notes["  PB sprite groups decoded"] += (int)drawn;
	}
	if (out == plain && enc == stored) Ok(s, "character .DAT (Party Breakers, three-section cipher)");
	else { Diff("character DAT", label, out, plain); if (enc != stored) printf("  re-encrypt differs\n"); s.fail++; }
	return true;
}

bool Pb2Member(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (Pb2CharMember(label, d, s)) return true;
	if ((e == ".CT" || e == ".CCT") && d.size() == sizeof(Pb2CtFile)) { Ok(s, "_C.CT / .CCT command table (typed, Pb2CtFile 4232 bytes)"); return true; }
	if (e == ".CT" || e == ".CCT") {
		han2::CharSel c; if (!han2::ParseCharSel(d.data(), d.size(), c, &err)) { Fail(s, label, "charsel: " + err); return true; }
		han2::SerializeCharSel(c, out); if (out == d) Ok(s, "CHARSEL.CT (enciphered character table)"); else Bad(s, "charsel", label, out, d);
		return true;
	}
	if (e == ".WMT") { han2::Wmt w; if (!han2::ParseWmt(d.data(), d.size(), w, &err)) { Fail(s, label, "wmt: " + err); return true; } han2::SerializeWmt(w, out); if (out == d) Ok(s, ".WMT (win quotes, 154-byte records)"); else Bad(s, "wmt", label, out, d); return true; }
	if (e == ".TXT" && name == "MULTICOM.TXT") {
		han2::AiLegacy a; if (!han2::ParseAiLegacy(d.data(), d.size(), a, &err)) { Fail(s, label, "ai legacy: " + err); return true; }
		han2::SerializeAiLegacy(a, out); if (out == d) Ok(s, "MULTICOM.TXT (legacy AI tables)"); else Bad(s, "ai legacy", label, out, d);
		return true;
	}
	if (e == ".B") {
		han2::PolyObject o; if (!han2::ParsePoly(d.data(), d.size(), o, &err)) { Fail(s, label, "poly: " + err); return true; }
		han2::SerializePoly(o, out); if (out == d) Ok(s, ".B (polygon object)"); else Bad(s, "poly", label, out, d);
		return true;
	}
	if (e == ".CPF" || (e == ".TXT" && d.size() >= 56048 && name.size() > 7 && ([&] { std::string t = name.substr(name.size() - 7); for (auto &c : t) c = (char)toupper((unsigned char)c); return t == "COM.TXT"; }()))) {
		han2::AiFile a; if (!han2::ParseAi(d.data(), d.size(), a, &err)) { Fail(s, label, "ai: " + err); return true; }
		han2::SerializeAi(a, out); if (out == d) Ok(s, "CPU script (.CPF / *COM.TXT)"); else Bad(s, "ai", label, out, d);
		return true;
	}
	return false;
}

using Handler = bool (*)(const std::string &, const std::string &, const std::vector<uint8_t> &, Section &);

// ---- per title -----------------------------------------------------------------------------------------------------------------------------
// Re-ACT satellites (docs/formats/mbr.md): <CHAR>_C.CT, <CHAR>.WMT, <CHAR>.CPF, CHARASELECT.CT
bool MbrSatellite(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".CT" && d.size() == han2::mbr::kCtSize) {
		han2::mbr::CtFile c; if (!han2::mbr::ParseCt(d.data(), d.size(), c, &err)) { Fail(s, label, "ct: " + err); return true; }
		han2::mbr::SerializeCt(c, out); if (out == d) Ok(s, "_C.CT command table (44-byte moves + params)"); else Bad(s, "ct", label, out, d);
		return true;
	}
	if (e == ".CT") {
		han2::mbr::CharaSelectFile c; if (!han2::mbr::ParseCharaSelect(d.data(), d.size(), c, &err)) { Fail(s, label, "charaselect: " + err); return true; }
		han2::mbr::SerializeCharaSelect(c, out); if (out == d) Ok(s, "CHARASELECT.CT (enciphered character table)"); else Bad(s, "charaselect", label, out, d);
		return true;
	}
	if (e == ".WMT") {
		han2::mbr::WmtFile w; if (!han2::mbr::ParseWmt(d.data(), d.size(), w, &err)) { Fail(s, label, "wmt: " + err); return true; }
		han2::mbr::SerializeWmt(w, out); if (out == d) Ok(s, ".WMT (win quotes)"); else Bad(s, "wmt", label, out, d);
		return true;
	}
	if (e == ".CPF") {
		auto *f = new MbrCpfFile; bool ok = han2::mbr::ParseCpf(d.data(), d.size(), *f, &err);
		if (ok) han2::mbr::SerializeCpf(*f, out);
		delete f;
		if (!ok) Fail(s, label, "cpf: " + err); else if (out == d) Ok(s, ".CPF (CPU script)"); else Bad(s, "cpf", label, out, d);
		return true;
	}
	return false;
}

bool ReactMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	if (MbrSatellite(label, name, d, s)) return true;
	if (Ha4Member(label, d, s)) return true;
	if (StageMember(label, d, s)) return true;
	if (Gof1CharMember(label, d, s)) return true;
	return false;
}

struct Title { const char *key; Handler fn; };
// Melty Blood 2002 satellites (docs/formats/mb.md)
bool MbSatellite(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".CT" || e == ".CT2") {
		if (d.size() == sizeof(MbCtFile)) {
			han2::mb::CtFile c; if (!han2::mb::ParseCt(d.data(), d.size(), c, &err)) { Fail(s, label, "ct: " + err); return true; }
			han2::mb::SerializeCt(c, out); if (out == d) Ok(s, "_C.CT command table (MB, 44-byte moves + header)"); else Bad(s, "ct", label, out, d);
			return true;
		}
		if (d.size() == 4232 || d.size() == 4432) {
			han2::mb::OldCtFile c; if (!han2::mb::ParseOldCt(d.data(), d.size(), c, &err)) { Fail(s, label, "old ct: " + err); return true; }
			han2::mb::SerializeOldCt(c, out); if (out == d) Ok(s, "older command table variants (42/44-byte moves, never loaded by mb.exe)"); else Bad(s, "old ct", label, out, d);
			return true;
		}
		han2::mb::CharSelFile c; if (!han2::mb::ParseCharSel(d.data(), d.size(), c, &err)) { Fail(s, label, "charsel: " + err); return true; }
		han2::mb::SerializeCharSel(c, out); if (out == d) Ok(s, "CHARSEL.CT (enciphered character table)"); else Bad(s, "charsel", label, out, d);
		return true;
	}
	if (e == ".WMT") {
		{   // HISKOH.WMT: orphan in the older 154-byte GOF1 layout (docs/formats/mb.md 5.3)
			han2::Wmt g; if (d.size() >= 4 && han2::ParseWmt(d.data(), d.size(), g, &err) && !han2::mb::ParseWmt(d.data(), d.size(), *std::make_unique<han2::mb::WmtFile>(), nullptr)) {
				han2::SerializeWmt(g, out); if (out == d) Ok(s, ".WMT orphan in the older 154-byte layout (never loaded)"); else Bad(s, "wmt154", label, out, d);
				return true;
			}
		}
		han2::mb::WmtFile w; if (!han2::mb::ParseWmt(d.data(), d.size(), w, &err)) { Fail(s, label, "wmt: " + err); return true; }
		han2::mb::SerializeWmt(w, out); if (out == d) Ok(s, ".WMT (win messages)"); else Bad(s, "wmt", label, out, d);
		return true;
	}
	if (e == ".CPF") {
		auto *f = new MbCpfFile; bool ok = han2::mb::ParseCpf(d.data(), d.size(), *f, &err);
		if (ok) han2::mb::SerializeCpf(*f, out);
		delete f;
		if (!ok) Fail(s, label, "cpf: " + err); else if (out == d) Ok(s, ".CPF (CPU script)"); else Bad(s, "cpf", label, out, d);
		return true;
	}
	return false;
}

bool MbMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	if (MbSatellite(label, name, d, s)) return true;
	if (Gof1CharMember(label, d, s)) return true;
	return false;
}

// dMp / Rosa: script VM (.FOB) + sprite sheets (.IMG)
bool FobImgMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".FOB") {
		han2::fob::File f; if (!han2::fob::Parse(d.data(), d.size(), f, &err)) { Fail(s, label, "fob: " + err); return true; }
		if (!han2::fob::Serialize(f, out, &err)) { Fail(s, label, "fob save: " + err); return true; }
		if (out == d) Ok(s, ".FOB (script bytecode)"); else Bad(s, "fob", label, out, d);
		return true;
	}
	if (e == ".IMG") {
		han2::ImgFile im; if (!han2::ParseImg(d.data(), d.size(), im, &err)) { Fail(s, label, "img: " + err); return true; }
		han2::SerializeImg(im, out);
		if (out == d) Ok(s, ".IMG (sprite sheet)"); else Bad(s, "img", label, out, d);
		return true;
	}
	return false;
}

// Drill Milky Punch: dMp-dialect scripts, IMG v6 sheets, DEMOnn.DAT replays (docs/formats/dmp.md)
bool DmpMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".FOB") {
		han2::dmpfob::File f; if (!han2::dmpfob::Parse(d.data(), d.size(), f, &err)) { Fail(s, label, "dmp fob: " + err); return true; }
		han2::dmpfob::Serialize(f, out);
		if (out != d) { Bad(s, "dmp fob", label, out, d); return true; }
		Ok(s, ".FOB (dMp script bank)"); s.notes["  decoded instructions"] += (int)f.nInsns; s.notes["  raw data bytes"] += (int)f.rawBytes;
		return true;
	}
	if (e == ".IMG") {
		han2::ImgFile im; if (!han2::ParseImg(d.data(), d.size(), im, &err)) { Fail(s, label, "img: " + err); return true; }
		han2::SerializeImg(im, out);
		if (im.version == 6 && im.format == 0) {   // dMp: both D3D formats the callers use must re-encode every pixel word exactly (the editor's quantiser)
			for (int f16 = 0; f16 < 2; f16++) {
				std::vector<uint8_t> rgba, words; han2::DecodePixels16(im.native.data(), (size_t)im.width * im.height, f16, rgba); han2::EncodePixels16(rgba.data(), (size_t)im.width * im.height, f16, words);
				if (words != im.native) { Fail(s, label, f16 ? "A4R4G4B4 re-encode differs" : "A1R5G5B5 re-encode differs"); return true; }
			}
			s.notes["  16-bit pixel re-encode exact (A1R5G5B5 and A4R4G4B4)"]++;
		}
		if (out == d && im.version == 6) Ok(s, ".IMG v6 (raw 16-bit pixels)"); else if (out == d) Ok(s, ".IMG"); else Bad(s, "img", label, out, d);
		return true;
	}
	if (e == ".DAT" && d.size() == sizeof(DmpReplayFile)) {
		const DmpReplayFile *r = (const DmpReplayFile *)d.data();
		if (r->frameCount > 108001) { Fail(s, label, "replay frame count out of range"); return true; }
		for (uint32_t i = r->frameCount; i < 108001; i++) for (int k = 0; k < 6; k++) if (r->frames[i].rawInput[k]) { Fail(s, label, "replay frames beyond frameCount are not zero"); return true; }
		Ok(s, "DEMOnn.DAT (DmpMatchSetup 68 B + 108001 x 24-byte input rows, typed)");
		return true;
	}
	return false;
}

const Title kTitles[] = { { "react", ReactMember }, { "mb", MbMember }, { "pb2k1", Pb2Member }, { "dmp", DmpMember }, { "rosa", FobImgMember } };

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
			if (++s.unmodelled[ExtOf(name) + " (no model)"] <= 1 || getenv("FB_VERBOSE")) printf("SKIP %s (%zu bytes)\n", label.c_str(), d.size());
		}
		printf("%-40s pass %d fail %d skipped %d\n", argv[i], s.pass - p0, s.fail - f0, s.skipped - k0);
	}
	std::string un; for (auto &kv : s.unmodelled) un += (un.empty() ? "" : ", ") + kv.first + " x" + std::to_string(kv.second);
	printf("SECTION %-9s pass %d fail %d skipped %d%s%s\n", s.name.c_str(), s.pass, s.fail, s.skipped, un.empty() ? "" : " | unmodelled: ", un.c_str());
	for (auto &kv : s.notes) printf("    %s: %d\n", kv.first.c_str(), kv.second);
	return (s.fail || s.skipped) ? 1 : 0;
}

} // namespace

// shift <archive> <ENTRY> <out.p> <dx> <dy>: the in-game proof file. Loads one character (Hantei4 or the GOF1-family container), moves every frame's
// sprite layers by (dx, dy), serializes it the way the editor does and writes a NEW archive with the entry replaced.
int CmdShift(int argc, char **argv)
{
	if (argc < 5) { puts("shift <archive> <ENTRY|-all> <out.p> <dx> <dy>"); return 2; }
	std::string err; auto a = fbarc::Open(argv[0], &err);
	if (!a) { printf("%s\n", err.c_str()); return 1; }
	const bool all = !strcmp(argv[1], "-all");
	const int dx = atoi(argv[3]), dy = atoi(argv[4]);
	fbarc::Edit e; int done = 0;
	for (size_t idx = 0; idx < a->entries().size(); idx++) {
		if (!all && (int)idx != a->find(fbarc::NameFromUtf8(argv[1]))) continue;
		std::vector<uint8_t> d; if (!a->read(idx, d, &err)) { printf("%s\n", err.c_str()); return 1; }
		FrameData fd; std::vector<uint8_t> out; bool gof = false, pb = false;
		if (ExtOf(a->entries()[idx].name) == ".DAT" && pb2k1::LooksLikeCharacter(d.data(), d.size())) {
			pb = true; pb2k1::Decrypt(d);
			if (!pb2k1::Load(fd, d.data(), d.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
		} else if (ha4::IsHA4(d.data(), d.size())) {
			if (!ha4::Load(fd, d.data(), d.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
		} else if (d.size() >= 4 && R32(d.data()) == 0x3dfe93d9u && ExtOf(a->entries()[idx].name) == ".DAT") {
			gof = true; gof1::DecryptDat(d);
			if (!gof1::Load(fd, d.data(), d.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
		} else { if (!all) { puts("not a character"); return 1; } continue; }
		if (all && (a->entries()[idx].name.find("EFFECT") != std::string::npos || a->entries()[idx].name.find("SAMPLE") != std::string::npos)) continue;
		for (auto &q : fd.m_sequences) for (auto &f : q.frames) for (auto &l : f.AF.layers) { l.offset_x += dx; l.offset_y += dy; }
		if (pb ? !pb2k1::Serialize(fd, out, &err) : gof ? !gof1::Serialize(fd, out, &err) : !ha4::Serialize(fd, out, &err)) { printf("save: %s\n", err.c_str()); return 1; }
		if (gof) gof1::EncryptDat(out);
		if (pb) pb2k1::Encrypt(out);
		e.replace[idx] = out; done++;
	}
	if (!done) { puts("nothing to edit"); return 1; }
	if (!a->rebuild(argv[2], e, &err)) { printf("rebuild: %s\n", err.c_str()); return 1; }
	printf("shifted %d character(s) of %s by (%d,%d), wrote %s\n", done, argv[0], dx, dy, argv[2]);
	return 0;
}

// Plain (decrypted) character bytes of any supported character container, plus the end of its pattern area (u32 @0x14 in all of them).
static bool PlainChar(const std::string &name, std::vector<uint8_t> d, std::vector<uint8_t> &plain, int &kind)
{
	if (ExtOf(name) != ".DAT") return false;
	if (pb2k1::LooksLikeCharacter(d.data(), d.size())) { if (!pb2k1::Decrypt(d)) return false; plain = d; kind = 2; return true; }
	if (ha4::IsHA4(d.data(), d.size())) { plain = d; kind = 0; return true; }
	if (d.size() >= 4 && R32(d.data()) == 0x3dfe93d9u) { gof1::DecryptDat(d); plain = d; kind = 1; return true; }
	return false;
}

// locality <archive>...: EDIT LOCALITY. Every character is loaded, every sprite layer is moved by one pixel, the model is saved; the result may differ from the
// original ONLY inside the pattern area (everything from the end of the pattern area on - parts, sprite bank / CG, names - must be byte-identical),
// by at most 2 bytes per layer (the offset word), and moving back must restore the original exactly.
int CmdLocality(int argc, char **argv)
{
	int pass = 0, fail = 0;
	for (int i = 0; i < argc; i++) {
		std::string err; auto a = fbarc::Open(argv[i], &err);
		if (!a) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fail++; continue; }
		for (size_t k = 0; k < a->entries().size(); k++) {
			std::vector<uint8_t> raw, plain; int kind = 0;
			if (ExtOf(a->entries()[k].name) != ".DAT") continue;
			if (!a->read(k, raw, &err)) { printf("FAIL %s: %s\n", argv[i], err.c_str()); fail++; continue; }
			if (!PlainChar(a->entries()[k].name, raw, plain, kind)) continue;
			const std::string label = std::string(argv[i]) + "::" + a->entries()[k].name;
			FrameData fd; bool ok = kind == 2 ? pb2k1::Load(fd, plain.data(), plain.size(), &err) : kind == 1 ? gof1::Load(fd, plain.data(), plain.size(), &err) : ha4::Load(fd, plain.data(), plain.size(), &err);
			if (!ok) { printf("FAIL %s: load %s\n", label.c_str(), err.c_str()); fail++; continue; }
			size_t layers = 0;
			for (auto &q : fd.m_sequences) for (auto &f : q.frames) for (auto &l : f.AF.layers) { l.offset_x += 1; layers++; }
			std::vector<uint8_t> out;
			ok = kind == 2 ? pb2k1::Serialize(fd, out, &err) : kind == 1 ? gof1::Serialize(fd, out, &err) : ha4::Serialize(fd, out, &err);
			if (!ok) { printf("FAIL %s: save %s\n", label.c_str(), err.c_str()); fail++; continue; }
			const uint32_t areaEnd = R32(plain.data() + 0x14);
			size_t diffs = 0, outside = 0, first = (size_t)-1;
			if (out.size() != plain.size()) { printf("FAIL %s: size changed %zu -> %zu\n", label.c_str(), plain.size(), out.size()); fail++; continue; }
			for (size_t b = 0; b < out.size(); b++) if (out[b] != plain[b]) { diffs++; if (first == (size_t)-1) first = b; if (b >= areaEnd) outside++; }
			for (auto &q : fd.m_sequences) for (auto &f : q.frames) for (auto &l : f.AF.layers) l.offset_x -= 1;
			std::vector<uint8_t> back;
			if (kind == 2) pb2k1::Serialize(fd, back, &err); else if (kind == 1) gof1::Serialize(fd, back, &err); else ha4::Serialize(fd, back, &err);
			if (outside || diffs > 2 * layers || back != plain) { printf("FAIL %s: locality: %zu bytes differ (%zu outside the pattern area, limit %zu), first at 0x%zx, areaEnd 0x%x, moving back %s\n", label.c_str(), diffs, outside, 2 * layers, first, areaEnd, back == plain ? "restores" : "DOES NOT restore"); fail++; }
			else { pass++; }
		}
	}
	printf("SECTION locality pass %d fail %d skipped 0\n", pass, fail);
	return fail ? 1 : 0;
}

// cgpng <archive> <ENTRY> <image> <out.png> [palette]: draws one image of the character's embedded MB strip bank (visual check of the decoder)
int CmdCgPng(int argc, char **argv)
{
	if (argc < 4) { puts("cgpng <archive> <ENTRY> <image> <out.png> [palette]"); return 2; }
	std::string err; auto a = fbarc::Open(argv[0], &err); if (!a) { printf("%s\n", err.c_str()); return 1; }
	const int idx = a->find(fbarc::NameFromUtf8(argv[1])); if (idx < 0) { puts("entry not found"); return 1; }
	std::vector<uint8_t> d; if (!a->read((size_t)idx, d, &err)) { printf("%s\n", err.c_str()); return 1; }
	gof1::DecryptDat(d); FrameData fd; if (!gof1::Load(fd, d.data(), d.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
	auto bank = han2::MbCgBank::Parse(fd.m_han2->cg.data(), fd.m_han2->cg.size(), &err); if (!bank) { printf("cg: %s\n", err.c_str()); return 1; }
	const int pal = argc > 4 ? atoi(argv[4]) : 0;
	std::unique_ptr<ImageData> im(bank->draw((unsigned)atoi(argv[2]), false, false, bank->palette(pal)));
	if (!im) { puts("no such image"); return 1; }
	std::string e; if (!WritePngRgba(argv[3], im->pixels, im->width, im->height, e)) { printf("png: %s\n", e.c_str()); return 1; }
	printf("wrote %s (%d x %d, offset %d,%d)\n", argv[3], im->width, im->height, im->offsetX, im->offsetY);
	return 0;
}

int main(int argc, char **argv)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (argc >= 2 && !strcmp(argv[1], "locality")) return CmdLocality(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "cgpng")) return CmdCgPng(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "shift")) return CmdShift(argc - 2, argv + 2);
	if (argc < 2) { puts("usage: fbchartool <title> <archive>...   (fbchartool list)"); return 2; }
	if (!strcmp(argv[1], "list")) { for (auto &t : kTitles) puts(t.key); return 0; }
	for (auto &t : kTitles) if (!strcmp(argv[1], t.key)) return Run(t, argc - 2, argv + 2);
	printf("unknown title %s\n", argv[1]);
	return 2;
}
