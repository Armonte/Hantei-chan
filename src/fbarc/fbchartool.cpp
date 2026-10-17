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
#include "../han2/qoh_dat.h"
#include "../han2/qoh_cg.h"
#include "../han2/qoh_img.h"
#include "../han2/rosa_img.h"
#include "../framedata_qoh.h"
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
#include "../cgm/cgm_io.h"
#include "../cgm/cgm_export.h"
#include <filesystem>
#include "../png_writer.h"
#include <windows.h>
#include <objbase.h>
#include "../han2/mb_cg.h"

#include <algorithm>
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

struct Title { const char *key; Handler fn; Handler pre = nullptr; };   // pre: tried before the members every title shares
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

// Party Breakers BGM archives (docs/formats/pb2k1.md section 16): 02/03/04.dat entries are inner PB archives enciphered with the entry name (cipher N over the first 9696 bytes),
// whose members are MP3 files enciphered the same way; 04.dat also holds a scrambled 03.MP3. The game only probes dwords inside them (boot integrity checks).
bool Pb2BgmMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err;
	if (e == ".MP3" && d.size() >= 4 && !(d[0] == 0xFF && (d[1] & 0xE0) == 0xE0) && !Has(d, "ID3")) {
		std::vector<uint8_t> p = d; gof1::CipherEntry(p, name);
		han2::MpegInfo m; if (!han2::ValidateMpeg(p.data(), p.size(), m, &err)) { Fail(s, label, "scrambled mp3 does not decode to MPEG: " + err); return true; }
		std::vector<uint8_t> back = p; gof1::CipherEntry(back, name);
		if (back != d) { Bad(s, "scrambled mp3", label, back, d); return true; }
		Ok(s, ".MP3 scrambled with the name cipher (decodes to valid MPEG, re-enciphers exactly)");
		return true;
	}
	if (e == ".DAT" && d.size() > 1000000 && name.size() == 6 && isdigit((unsigned char)name[0]) && isdigit((unsigned char)name[1]) && !pb2k1::LooksLikeCharacter(d.data(), d.size())) {
		std::vector<uint8_t> blob = d; bool applied = true;
		gof1::CipherEntry(blob, name);
		if (!(blob.size() >= 8 && gof1::LooksLikeArchive(blob.data(), blob.size()))) { blob = d; applied = false; }   // 03.dat (plainFlag 0): the archive layer already undid the name cipher
		gof1::MemArchive m;
		if (blob.size() < 8 || !gof1::LooksLikeArchive(blob.data(), blob.size()) || !gof1::OpenMem(blob.data(), blob.size(), m, &err)) { Fail(s, label, "BGM blob is not an inner PB archive after the name cipher"); return true; }
		std::vector<uint8_t> idx; gof1::EncodeIndex(m.a, idx);
		if (idx.size() > blob.size() || memcmp(idx.data(), blob.data(), idx.size()) != 0) { Fail(s, label, "inner archive index does not re-encode identically"); return true; }
		if (!m.tiles) { Fail(s, label, "inner archive sizes do not add up"); return true; }
		for (size_t i = 0; i < m.a.entries.size(); i++) {
			std::vector<uint8_t> plain; if (!gof1::ReadEntryMem(m, blob.data(), i, plain)) { Fail(s, label + "/" + m.a.entries[i].name, "unreadable member"); continue; }
			std::vector<uint8_t> back = plain; if (m.a.plainFlag == 0) gof1::CipherEntry(back, m.a.entries[i].name);
			if (memcmp(back.data(), blob.data() + m.dataOffset[i], back.size()) != 0) { Fail(s, label + "/" + m.a.entries[i].name, "member cipher does not invert"); continue; }
			han2::MpegInfo mi; if (!han2::ValidateMpeg(plain.data(), plain.size(), mi, &err)) { Fail(s, label + "/" + m.a.entries[i].name, "member is not MPEG: " + err); continue; }
			s.notes["  BGM members decoded to valid MPEG"]++;
		}
		std::vector<uint8_t> back = blob; if (applied) gof1::CipherEntry(back, name);
		if (back != d) { Bad(s, "bgm blob", label, back, d); return true; }
		Ok(s, "BGM blob (inner PB archive under the name cipher, MP3 members under theirs)");
		return true;
	}
	return false;
}

// ---- Queen of Heart '98 / '99 (docs/formats/qoh98.md, qoh99.md) -----------------------------------------------------------------------------------------------
bool QohChar(const std::string &label, int version, const std::vector<uint8_t> &stored, const std::string &stem, Section &s)
{
	std::string err; std::vector<uint8_t> plain = stored;
	if (version == han2::qoh::V99 && !han2::qoh::Decrypt99(plain, stem, &err)) { Fail(s, label, "decrypt: " + err); return true; }
	FrameData fd;
	if (!qoh::Load(fd, plain.data(), plain.size(), version, &err)) { Fail(s, label, "load: " + err); return true; }
	std::vector<uint8_t> out;
	if (!qoh::Serialize(fd, out, &err)) { Fail(s, label, "save: " + err); return true; }
	std::vector<uint8_t> enc = out; if (version == han2::qoh::V99) han2::qoh::Encrypt99(enc, stem);
	auto bank = han2::QohCgBank::Parse(fd.m_han2->cg.data(), fd.m_han2->cg.size(), version, &err);
	if (!bank) { Fail(s, label, "sprite bank: " + err); return true; }
	unsigned drawn = 0;
	for (unsigned i = 0; i < bank->imageCount(); i++) { int b, t, x1, y1, x2, y2; if (!bank->imageInfo(i, b, t, x1, y1, x2, y2)) continue; std::unique_ptr<ImageData> im(bank->draw(i, false, false, bank->palette(0))); if (!im) { Fail(s, label, "image " + std::to_string(i) + " does not decode"); return true; } drawn++; }
	s.notes["  QoH sprite images decoded"] += (int)drawn;
	if (out == plain && enc == stored) Ok(s, version == han2::qoh::V99 ? "character .chr (QoH99, name-keyed cipher)" : "character .dat (QoH98)");
	else { Diff("character", label, out, plain); if (enc != stored) printf("  re-encrypt differs\n"); s.fail++; }
	return true;
}

bool Qoh99Member(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".CHR") return QohChar(label, han2::qoh::V99, d, han2::qoh::StemOfPath(name), s);
	if (e == ".FOB") {
		han2::dmpfob::File f; if (!han2::dmpfob::Parse(d.data(), d.size(), f, &err, han2::dmpfob::Dialect::Qoh99)) { Fail(s, label, "fob: " + err); return true; }
		han2::dmpfob::Serialize(f, out);
		if (out != d) { Bad(s, "qoh99 fob", label, out, d); return true; }
		Ok(s, ".Fob (CPU script bank, QoH99 VM)"); s.notes["  decoded instructions"] += (int)f.nInsns; s.notes["  raw (switch table / data) bytes"] += (int)f.rawBytes;
		return true;
	}
	if (e == ".IMG") {
		han2::QohImg im; if (!han2::ParseQohImg(d.data(), d.size(), im, &err)) { Fail(s, label, "img: " + err); return true; }
		han2::SerializeQohImg(im, out); if (out == d) Ok(s, ".Img (palettised / 24-bit bitmap)"); else Bad(s, "qoh img", label, out, d);
		return true;
	}
	return false;
}

bool Qoh98Member(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".DAT" && han2::qoh::LooksLike98(d.data(), d.size())) return QohChar(label, han2::qoh::V98, d, "", s);
	if (e == ".TIM") {   // PlayStation TIM as the game reads it (Tim_LoadTo256x256Dib): magic, flags, CLUT block size, CLUT, image block, pixels; bytes kept verbatim after validation
		if (d.size() < 0x20) { Fail(s, label, "tim too short"); return true; }
		const uint32_t flags = R32(d.data() + 4), bnum = R32(d.data() + 8);
		if ((flags & 7) > 1 || bnum < 12 || (uint64_t)8 + bnum + 12 + 1 > d.size()) { Fail(s, label, "tim header does not describe a 4/8-bit image"); return true; }
		Ok(s, ".TIM (PlayStation TIM, 4/8-bit + CLUT, validated)"); return true;
	}
	if (e == ".BMP") {   // the shipped bitmaps start with 00 instead of BM; bfSize must equal the file size, 8-bit DIB
		if (d.size() < 54 || R32(d.data() + 2) != d.size() || R32(d.data() + 14) != 40) { Fail(s, label, "bmp header does not match"); return true; }
		Ok(s, ".BMP (DIB, magic bytes not checked by the game)"); return true;
	}
	if (e == ".MID") {
		size_t q = 0; int chunks = 0;
		while (q + 8 <= d.size()) { const uint32_t len = (uint32_t)d[q + 4] << 24 | d[q + 5] << 16 | d[q + 6] << 8 | d[q + 7]; if (!(memcmp(&d[q], "MThd", 4) == 0 || memcmp(&d[q], "MTrk", 4) == 0)) break; q += 8 + (size_t)len; chunks++; }
		if (q != d.size() || chunks < 2) { Fail(s, label, "midi chunks do not tile the file"); return true; }
		Ok(s, ".MID (standard MIDI file chunks tile the file)"); return true;
	}
	if (e == ".TXT") {   // command.txt: Shift-JIS text with // comments
		han2::Text t; han2::ParseText(d.data(), d.size(), t, &err); han2::SerializeText(t, out);
		if (out == d) Ok(s, "text (Shift-JIS, line structure)"); else Bad(s, "text", label, out, d);
		return true;
	}
	return false;
}

// Rosa Chinensis Four hand: dMp-family scripts with the Rosa opcode numbering, enciphered v4 / plain v1 .IMG (docs/formats/rosa.md)
bool RosaMember(const std::string &label, const std::string &name, const std::vector<uint8_t> &d, Section &s)
{
	const std::string e = ExtOf(name); std::string err; std::vector<uint8_t> out;
	if (e == ".FOB") {
		han2::dmpfob::File f; if (!han2::dmpfob::Parse(d.data(), d.size(), f, &err, han2::dmpfob::Dialect::Rosa)) { Fail(s, label, "rosa fob: " + err); return true; }
		han2::dmpfob::Serialize(f, out);
		if (out != d) { Bad(s, "rosa fob", label, out, d); return true; }
		Ok(s, ".FOB (Rosa script bank)"); s.notes["  decoded instructions"] += (int)f.nInsns; s.notes["  raw data bytes"] += (int)f.rawBytes;
		return true;
	}
	if (e == ".IMG") {
		const std::string stem = han2::RosaStemOfName(name);
		han2::RosaImg im; if (!han2::ParseRosaImg(d.data(), d.size(), stem, im, &err)) { Fail(s, label, "rosa img: " + err); return true; }
		han2::SerializeRosaImg(im, stem, out);
		if (out == d) Ok(s, im.version == 4 ? ".IMG v4 (enciphered with the file stem, 44-byte header)" : ".IMG v1 (plain, 28-byte header)"); else Bad(s, "rosa img", label, out, d);
		return true;
	}
	return false;
}

const Title kTitles[] = { { "react", ReactMember }, { "mb", MbMember }, { "pb2k1", Pb2Member, Pb2BgmMember }, { "qoh99", Qoh99Member }, { "qoh98", Qoh98Member, Qoh98Member }, { "dmp", DmpMember }, { "rosa", RosaMember } };

// Loose-file titles (QoH '98 / '99 ship as plain files): an argument "dir:<folder>" is walked recursively, every file is a member; a plain file argument is one member.
// Folders named like the debris of netplay test runs are skipped (desync_*, ckdump, arena_*, _archive ...), nothing else is filtered: a file nobody models counts as skipped.
bool SkipDir(const std::string &n)
{
	std::string l = n; for (auto &c : l) c = (char)tolower((unsigned char)c);
	return l.rfind("desync", 0) == 0 || l == "ckdump" || l.rfind("arena", 0) == 0 || l == "_archive" || l == "replaypc" || l == "screenshots" || l == "shaders" || l.rfind("ctrl_", 0) == 0 || l == "zzz_tmpneocor" || l == "__pycache__";
}

int Run(const Title &t, int argc, char **argv)
{
	Section s; s.name = t.key;
	auto member = [&](const std::string &argLabel, const std::string &name, std::vector<uint8_t> &d) {
		const std::string label = argLabel + "::" + name;
		if (t.pre && t.pre(label, name, d, s)) return;
		if (CommonMember(label, name, d, s)) return;
		if (t.fn(label, name, d, s)) return;
		s.skipped++;
		if (++s.unmodelled[ExtOf(name) + " (no model)"] <= 1 || getenv("FB_VERBOSE")) printf("SKIP %s (%zu bytes)\n", label.c_str(), d.size());
	};
	for (int i = 0; i < argc; i++) {
		const std::string arg = argv[i];
		const int p0 = s.pass, f0 = s.fail, k0 = s.skipped;
		if (arg.rfind("dir:", 0) == 0) {
			std::error_code ec; std::string dirArg = arg.substr(4), extList;   // "dir:<folder>?ext=chr,fob,img" keeps only those extensions
			if (size_t q = dirArg.find("?ext="); q != std::string::npos) { extList = "," + dirArg.substr(q + 5) + ","; dirArg.resize(q); }
			const fs::path root = fs::u8path(dirArg);
			std::vector<fs::path> files;
			for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
				if (it->is_directory(ec)) { if (SkipDir(it->path().filename().u8string())) it.disable_recursion_pending(); continue; }
				if (it->is_regular_file(ec)) {
					if (!extList.empty()) { std::string x = it->path().extension().string(); for (auto &c : x) c = (char)tolower((unsigned char)c); if (x.empty() || extList.find("," + x.substr(1) + ",") == std::string::npos) continue; }
					files.push_back(it->path());
				}
			}
			std::sort(files.begin(), files.end());
			const std::string base = root.generic_u8string();
			for (auto &f : files) {
				std::ifstream in(f, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
				std::string rel = f.generic_u8string(); if (rel.compare(0, base.size(), base) == 0) rel = rel.substr(base.size() + (base.empty() || base.back() == '/' ? 0 : 1));
				member(arg, rel, d);
			}
		} else {
			std::string err; auto a = fbarc::Open(arg, &err);
			if (!a) {
				std::ifstream in(fs::u8path(arg), std::ios::binary);
				if (!in) { printf("FAIL %s: %s\n", arg.c_str(), err.c_str()); s.fail++; continue; }
				std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
				member(arg, fs::u8path(arg).filename().u8string(), d);
			} else {
				for (size_t k = 0; k < a->entries().size(); k++) {
					const std::string name = fbarc::NameToUtf8(a->relativePath(k));
					std::vector<uint8_t> d;
					if (!a->read(k, d, &err)) { Fail(s, arg + "::" + name, err); continue; }
					member(arg, name, d);
				}
			}
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


// cgio <archive> <ENTRY> <tmp-dir>: the CG manager's batch export / import over a foreign (MB strip) bank: export all, import unchanged -> identical bank bytes;
// then change a few pixels of one image's PNG, import -> exactly that image re-encoded and rendering the edit.
int CmdCgIo(int argc, char **argv)
{
	if (argc < 3) { puts("cgio <archive> <ENTRY> <tmp-dir>"); return 2; }
	std::string err; auto a = fbarc::Open(argv[0], &err); if (!a) { printf("%s\n", err.c_str()); return 1; }
	if (!strcmp(argv[1], "?")) { for (size_t i = 0; i < a->entries().size(); i++) puts(a->relativePath(i).c_str()); return 0; }
	const int idx = a->find(fbarc::NameFromUtf8(argv[1])); if (idx < 0) { puts("entry not found"); return 1; }
	std::vector<uint8_t> d; if (!a->read((size_t)idx, d, &err)) { printf("%s\n", err.c_str()); return 1; }
	gof1::DecryptDat(d); FrameData fd; if (!gof1::Load(fd, d.data(), d.size(), &err)) { printf("load: %s\n", err.c_str()); return 1; }
	std::shared_ptr<han2::MbCgBank> bank = han2::MbCgBank::Parse(fd.m_han2->cg.data(), fd.m_han2->cg.size(), &err); if (!bank) { printf("cg: %s\n", err.c_str()); return 1; }
	std::vector<uint8_t> orig; bank->serialize(orig);
	CG cg; cg.loadForeign(bank); cgm::CgIO io(cg);
	const std::string dir = std::string(argv[2]) + "\\cgio"; std::filesystem::remove_all(dir);
	cgm::ExportResult er; cgm::ExportOptions eo;
	if (!cgm::ExportBank(io, dir, argv[1], eo, er)) { printf("FAIL export: %s\n", er.error.c_str()); return 1; }
	cgm::ImportResult ir; if (!cgm::ImportBank(io, dir, ir)) { printf("FAIL import: %s\n", ir.error.c_str()); return 1; }
	std::vector<uint8_t> after; bank->serialize(after);
	bool bad = false;
	if (!ir.changed.empty() || after != orig) { printf("FAIL unchanged export -> import altered the bank (%zu re-encoded)\n", ir.changed.size()); bad = true; }
	int pick = -1; cgm::Rgba r; int p1 = -1, p2 = -1;
	for (int n = 0; n < io.slots() && pick < 0; n++) { cgm::ImgInfo ii; io.info(n, ii); if (!ii.drawable || !io.decode(n, r, nullptr)) continue;
		for (int i = 0; i < r.w * r.h && p1 < 0; i++) if (r.px[i * 4 + 3]) for (int k = i + 1; k < r.w * r.h; k++) if (r.px[k * 4 + 3] && memcmp(&r.px[i * 4], &r.px[k * 4], 3)) { p1 = i; p2 = k; break; }
		if (p1 >= 0) pick = n; }
	if (pick >= 0) {
		memcpy(&r.px[p1 * 4], &r.px[p2 * 4], 3);
		std::filesystem::remove_all(dir); cgm::ExportOptions o1 = eo; o1.onlyIds = {pick}; o1.indexed = false; cgm::ExportResult e2;
		cgm::ExportBank(io, dir, argv[1], o1, e2);
		std::string png; for (auto &e : std::filesystem::directory_iterator(std::filesystem::path(dir) / "rgba")) png = e.path().string();
		std::string we; WritePngRgba(png, r.px.data(), r.w, r.h, we);
		cgm::ImportResult i2; cgm::ImportBank(io, dir, i2);
		cgm::Rgba now; io.decode(pick, now, nullptr);
		if (i2.changed.size() != 1 || i2.changed[0] != pick || now.px != r.px) { printf("FAIL edit of image %d: changed=%zu renders-edit=%d\n", pick, i2.changed.size(), (int)(now.px == r.px)); bad = true; }
		else printf("OK  edited image %d re-encoded alone and renders the edit\n", pick);
	}
	std::filesystem::remove_all(dir);
	printf("SECTION cgio pass %d fail %d skipped 0\n", bad ? 0 : 1, bad ? 1 : 0);
	return bad ? 1 : 0;
}

static int Main8(int argc, char **argv)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (argc >= 2 && !strcmp(argv[1], "locality")) return CmdLocality(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "cgpng")) return CmdCgPng(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "cgio")) return CmdCgIo(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "shift")) return CmdShift(argc - 2, argv + 2);
	if (argc < 2) { puts("usage: fbchartool <title> <archive>...   (fbchartool list)"); return 2; }
	if (!strcmp(argv[1], "list")) { for (auto &t : kTitles) puts(t.key); return 0; }
	for (auto &t : kTitles) if (!strcmp(argv[1], t.key)) return Run(t, argc - 2, argv + 2);
	printf("unknown title %s\n", argv[1]);
	return 2;
}

// Windows: arguments arrive as UTF-16 (Japanese file names such as Rosa's sound PAC) and are converted to UTF-8 for the rest of the tool.
int wmain(int argc, wchar_t **wargv)
{
	std::vector<std::string> store; std::vector<char *> argv;
	for (int i = 0; i < argc; i++) {
		const int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
		std::string u(n > 0 ? (size_t)n - 1 : 0, '\0'); if (n > 1) WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &u[0], n, nullptr, nullptr);
		store.push_back(u);
	}
	for (auto &x : store) argv.push_back(&x[0]);
	argv.push_back(nullptr);
	return Main8(argc, argv.data());
}
