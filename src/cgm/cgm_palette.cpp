#include "cgm_palette.h"
#include "../png_writer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace cgm {

static std::filesystem::path P(const std::string &u) { return std::filesystem::path(Utf8ToWide(u)); }

bool PalSet::parse(const uint8_t *d, size_t n, PalSet &out, std::string *err) {
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	if (n < 4) return fail("palette file too short");
	uint32_t w0; memcpy(&w0, d, 4);
	out = PalSet(); out.raw.assign(d, d + n);
	if ((unsigned long long)w0 * 0x400 + 4 <= n) { out.count = (int)w0; out.offsetDw = 1; }
	else {
		if (n < 16) return fail("palette file too short");
		uint32_t c; memcpy(&c, d + 12, 4);
		if ((unsigned long long)c * 0x400 + 16 > n) return fail("palette count does not match the file size");
		out.count = (int)c; out.offsetDw = 4;
	}
	return true;
}
bool PalSet::addCopy(int from, std::string *err) {
	if (offsetDw != 1 || from < 0 || from >= count || raw.size() != (size_t)4 + (size_t)count * 1024) { if (err) *err = "this palette file layout cannot grow"; return false; }
	std::vector<uint8_t> nw(raw.begin() + 4 + (size_t)from * 1024, raw.begin() + 4 + (size_t)(from + 1) * 1024);
	raw.insert(raw.end(), nw.begin(), nw.end()); count++; memcpy(raw.data(), &count, 4); return true;
}
bool PalSet::remove(int i, std::string *err) {
	if (offsetDw != 1 || i < 0 || i >= count || count < 2 || raw.size() != (size_t)4 + (size_t)count * 1024) { if (err) *err = "cannot remove from this palette file"; return false; }
	raw.erase(raw.begin() + 4 + (size_t)i * 1024, raw.begin() + 4 + (size_t)(i + 1) * 1024); count--; memcpy(raw.data(), &count, 4); return true;
}
void PalSet::swapPalettes(int a, int b) { if (a != b && a >= 0 && b >= 0 && a < count && b < count) std::swap_ranges(pal(a), pal(a) + 256, pal(b)); }
void PalSet::movePalette(int from, int to) {
	if (from == to || from < 0 || to < 0 || from >= count || to >= count) return;
	std::vector<uint32_t> tmp(pal(from), pal(from) + 256);
	const int step = from < to ? 1 : -1;
	for (int i = from; i != to; i += step) memcpy(pal(i), pal(i + step), 1024);
	memcpy(pal(to), tmp.data(), 1024);
}

void PalSet::movePair(int from, int to) {
	if (!paired()) { movePalette(from, to); return; }
	const int h = half(); from = colourOf(from); to = colourOf(to);
	if (from == to || to < 0 || to >= h) return;
	movePalette(from, to); movePalette(from + h, to + h);
}
void PalSet::swapPair(int a, int b) {
	if (!paired()) { swapPalettes(a, b); return; }
	const int h = half(); a = colourOf(a); b = colourOf(b);
	swapPalettes(a, b); swapPalettes(a + h, b + h);
}
PalSet PalSet::makeMbaacc(int n, const uint32_t *fill) {
	PalSet o; o.offsetDw = 1; o.count = n; o.raw.assign(4 + (size_t)n * 1024, 0);
	memcpy(o.raw.data(), &n, 4);
	for (int i = 0; i < n; i++) memcpy(o.pal(i), fill, 1024);
	return o;
}
PalSet PalSet::converted(bool toUni) const {
	PalSet o;
	if (toUni) {
		const int colours = 65, n = 130;
		o.offsetDw = 4; o.count = n; o.raw.assign(16 + (size_t)n * 1024, 0);
		const uint32_t hdr[4] = {0xFFFFu, 1u, 0u, (uint32_t)n}; memcpy(o.raw.data(), hdr, 16);
		for (int c = 0; c < colours; c++) {
			const int src = count ? std::min(colourOf(c), count - 1) : -1;
			if (src >= 0) memcpy(o.pal(c), pal(src), 1024);
			// the alternate set: the pair's second half; a flat file of 130+ palettes (a UNI file saved in the MBAACC layout) keeps
			// its own c + 65, so UNI -> MBAACC -> UNI is lossless; a short MBAACC file repeats the colour
			const int alt = paired() && c < half() ? c + half() : (!paired() && count >= 2 * colours ? c + colours : src);
			if (alt >= 0) memcpy(o.pal(c + colours), pal(alt), 1024);
		}
		return o;
	}
	o.offsetDw = 1; o.count = count; o.raw.assign(4 + (size_t)count * 1024, 0);
	memcpy(o.raw.data(), &o.count, 4);
	for (int i = 0; i < count; i++) memcpy(o.pal(i), pal(i), 1024);
	return o;
}

Hsv ToHsv(uint32_t c) {
	const float r = R(c) / 255.f, g = G(c) / 255.f, b = B(c) / 255.f, mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
	Hsv o; o.v = mx; o.s = mx > 0 ? d / mx : 0;
	if (d > 0) {
		if (mx == r) o.h = 60.f * std::fmod((g - b) / d, 6.f); else if (mx == g) o.h = 60.f * ((b - r) / d + 2.f); else o.h = 60.f * ((r - g) / d + 4.f);
		if (o.h < 0) o.h += 360.f;
	}
	return o;
}
uint32_t FromHsv(Hsv h, uint8_t alpha) {
	h.s = std::clamp(h.s, 0.f, 1.f); h.v = std::clamp(h.v, 0.f, 1.f);
	float hh = std::fmod(h.h, 360.f); if (hh < 0) hh += 360.f;
	const float c = h.v * h.s, x = c * (1.f - std::fabs(std::fmod(hh / 60.f, 2.f) - 1.f)), m = h.v - c;
	float r = 0, g = 0, b = 0;
	if (hh < 60) { r = c; g = x; } else if (hh < 120) { r = x; g = c; } else if (hh < 180) { g = c; b = x; } else if (hh < 240) { g = x; b = c; } else if (hh < 300) { r = x; b = c; } else { r = c; b = x; }
	auto q = [](float v) { return (uint8_t)std::clamp((int)std::lround(v * 255.f), 0, 255); };
	return Rgb(q(r + m), q(g + m), q(b + m), alpha);
}
uint32_t Adjust(uint32_t c, const ColorAdjust &a) {
	if (a.neutral()) return c;
	Hsv h = ToHsv(c);
	h.h += a.hueDeg; h.s *= a.satMul; h.v *= a.valMul; h.s += a.satAdd / 255.f; h.v += a.valAdd / 255.f;
	return FromHsv(h, A(c));
}

static void Order(int &a, int &b) { if (a > b) std::swap(a, b); a = std::max(a, 0); b = std::min(b, 255); }
void FillRange(uint32_t *p, int a, int b, uint32_t rgb) { Order(a, b); for (int i = a; i <= b; i++) p[i] = (p[i] & 0xFF000000u) | (rgb & 0xFFFFFFu); }
void Gradient(uint32_t *p, int a, int b, uint32_t from, uint32_t to, bool viaHsv) {
	Order(a, b); if (a == b) return;
	const Hsv h0 = ToHsv(from), h1 = ToHsv(to);
	for (int i = a; i <= b; i++) {
		const float t = (float)(i - a) / (float)(b - a); uint32_t c;
		if (viaHsv) {
			float dh = h1.h - h0.h; if (dh > 180) dh -= 360; if (dh < -180) dh += 360;   // shortest way round the hue circle
			Hsv h{h0.h + dh * t, h0.s + (h1.s - h0.s) * t, h0.v + (h1.v - h0.v) * t}; c = FromHsv(h, A(p[i]));
		} else {
			auto L = [&](int x, int y) { return (uint8_t)std::lround(x + (y - x) * t); };
			c = Rgb(L(R(from), R(to)), L(G(from), G(to)), L(B(from), B(to)), A(p[i]));
		}
		p[i] = c;
	}
}
void AdjustRange(uint32_t *p, int a, int b, const ColorAdjust &adj) { Order(a, b); for (int i = a; i <= b; i++) p[i] = Adjust(p[i], adj); }
void ReverseRange(uint32_t *p, int a, int b) { Order(a, b); for (int i = a, j = b; i < j; i++, j--) { const uint32_t ci = p[i] & 0xFFFFFFu, cj = p[j] & 0xFFFFFFu; p[i] = (p[i] & 0xFF000000u) | cj; p[j] = (p[j] & 0xFF000000u) | ci; } }
void CopyRange(const uint32_t *src, int a, int b, uint32_t *dst, int at) {
	Order(a, b);
	std::vector<uint32_t> tmp(src + a, src + b + 1);
	for (int i = 0; i < (int)tmp.size() && at + i < 256; i++) if (at + i >= 0) dst[at + i] = (dst[at + i] & 0xFF000000u) | (tmp[i] & 0xFFFFFFu);
}
int ReplaceColor(uint32_t *p, uint32_t from, uint32_t to, int tol) {
	int n = 0;
	for (int i = 0; i < 256; i++) {
		const int d = std::max({std::abs((int)R(p[i]) - R(from)), std::abs((int)G(p[i]) - G(from)), std::abs((int)B(p[i]) - B(from))});
		if (d <= tol && (p[i] & 0xFFFFFFu) != (to & 0xFFFFFFu)) { p[i] = (p[i] & 0xFF000000u) | (to & 0xFFFFFFu); n++; }
	}
	return n;
}

int RecolorSet(PalSet &set, const ColorAdjust &adj, const RecolorScope &sc) {
	int n = 0;
	for (int i = 0; i < set.count; i++) {
		if (!sc.allPalettes && i != sc.onlyPalette) continue;
		AdjustRange(set.pal(i), sc.fromIndex, sc.toIndex, adj); n++;
	}
	return n;
}

int RecolorBank(Bank &bank, const ColorAdjust &adj, bool slots, bool imagePalettes, std::vector<int> *changed) {
	if (adj.neutral()) return 0;
	int n = 0;
	if (slots) for (int s = 0; s < 8; s++) { uint32_t p[256]; memcpy(p, bank.palettes.data() + 0x400 * s, 1024); AdjustRange(p, 1, 255, adj); memcpy(bank.palettes.data() + 0x400 * s, p, 1024); n++; }
	if (imagePalettes) for (size_t i = 0; i < bank.images.size(); i++) {
		Image &im = bank.images[i];
		if (!im.present || !im.blob || im.bpp != 32) continue;
		std::vector<uint8_t> nb(*im.blob);
		if ((im.type == 2 || im.type == 4) && nb.size() >= 1024) { uint32_t p[256]; memcpy(p, nb.data(), 1024); AdjustRange(p, 1, 255, adj); memcpy(nb.data(), p, 1024); }
		else if (im.type == 3 && nb.size() >= 4) { uint32_t c; memcpy(&c, nb.data(), 4); c = (Adjust(c, adj) & 0xFFFFFFu) | (c & 0xFF000000u); memcpy(nb.data(), &c, 4); }
		else continue;
		if (nb != *im.blob) { im.blob = std::make_shared<const std::vector<uint8_t>>(std::move(nb)); n++; if (changed) changed->push_back((int)i); }
	}
	return n;
}

// ---- files ----
static std::string Lower(std::string s) { for (char &c : s) c = (char)tolower((unsigned char)c); return s; }

int PalFileCount(const std::string &path) {
	const std::string ext = Lower(std::filesystem::path(path).extension().string());
	if (ext != ".pal") return 1;
	std::ifstream f(P(path), std::ios::binary); if (!f) return 0;
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (b.size() >= 24 && !memcmp(b.data(), "RIFF", 4)) return 1;
	PalSet s; return PalSet::parse(b.data(), b.size(), s, nullptr) ? std::max(1, s.count) : (b.size() == 1024 ? 1 : 0);
}

bool ReadPalFileColors(const std::string &path, uint32_t out[256], std::string *err, int index) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const std::string ext = Lower(std::filesystem::path(path).extension().string());
	if (ext == ".png") {
		std::vector<uint8_t> idx; uint32_t pal[256]; int w, h; std::string e;
		if (ReadImageIndexed(path, idx, pal, w, h, e)) { memcpy(out, pal, 1024); for (int i = 0; i < 256; i++) out[i] |= 0xFF000000u; return true; }
		std::vector<uint8_t> px;
		if (!ReadImageRgba(path, px, w, h, e)) return fail(e);
		for (int i = 0; i < 256; i++) out[i] = (size_t)i < (size_t)w * h ? Rgb(px[i * 4], px[i * 4 + 1], px[i * 4 + 2]) : Rgb(0, 0, 0);
		return true;
	}
	std::ifstream f(P(path), std::ios::binary); if (!f) return fail("cannot open " + path);
	std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (ext == ".gpl") {
		std::istringstream ss(std::string(b.begin(), b.end())); std::string line; int n = 0;
		if (!std::getline(ss, line) || line.compare(0, 12, "GIMP Palette") != 0) return fail("not a GIMP palette");
		for (int i = 0; i < 256; i++) out[i] = Rgb(0, 0, 0);
		while (std::getline(ss, line) && n < 256) { int r, g, bl; if (!line.empty() && line[0] != '#' && sscanf(line.c_str(), "%d %d %d", &r, &g, &bl) == 3) out[n++] = Rgb((uint8_t)r, (uint8_t)g, (uint8_t)bl); }
		return n > 0 ? true : fail("no colours in the file");
	}
	if (ext == ".act") {
		if (b.size() < 768) return fail("an .act file holds 256 x RGB (768 bytes)");
		for (int i = 0; i < 256; i++) out[i] = Rgb(b[i * 3], b[i * 3 + 1], b[i * 3 + 2]);
		return true;
	}
	if (b.size() >= 24 && !memcmp(b.data(), "RIFF", 4) && !memcmp(b.data() + 8, "PAL ", 4)) {   // Microsoft RIFF palette: PALETTEENTRY r,g,b,flags from offset 24
		const size_t n = std::min<size_t>(256, (b.size() - 24) / 4);
		for (size_t i = 0; i < 256; i++) out[i] = i < n ? Rgb(b[24 + i * 4], b[25 + i * 4], b[26 + i * 4]) : Rgb(0, 0, 0);
		return true;
	}
	PalSet s; std::string e;
	if (PalSet::parse(b.data(), b.size(), s, &e) && s.count >= 1) {
		if (index < 0 || index >= s.count) return fail("the file holds palettes 0.." + std::to_string(s.count - 1) + ", not " + std::to_string(index));
		memcpy(out, s.pal(index), 1024); return true;
	}
	if (b.size() == 1024) { memcpy(out, b.data(), 1024); return true; }
	return fail("unrecognised palette file");
}

bool WritePalFileColors(const std::string &path, const uint32_t pal[256], std::string *err, PalLayout layout) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const std::string ext = Lower(std::filesystem::path(path).extension().string());
	if (ext == ".png") {
		std::vector<uint8_t> px(16 * 16 * 4); for (int i = 0; i < 256; i++) { px[i * 4] = R(pal[i]); px[i * 4 + 1] = G(pal[i]); px[i * 4 + 2] = B(pal[i]); px[i * 4 + 3] = 255; }
		std::string e; return WritePngRgba(path, px.data(), 16, 16, e) ? true : fail(e);
	}
	std::ofstream f(P(path), std::ios::binary); if (!f) return fail("cannot write " + path);
	if (ext == ".gpl") {
		f << "GIMP Palette\nName: Hantei-chan CG manager\nColumns: 16\n#\n";
		for (int i = 0; i < 256; i++) { char l[64]; snprintf(l, sizeof(l), "%3d %3d %3d\tIndex %d\n", R(pal[i]), G(pal[i]), B(pal[i]), i); f << l; }
		return (bool)f;
	}
	if (ext == ".act") { for (int i = 0; i < 256; i++) { const char c[3] = {(char)R(pal[i]), (char)G(pal[i]), (char)B(pal[i])}; f.write(c, 3); } return (bool)f; }
	if (layout == PalLayout::Uni) { const uint32_t hdr[4] = {0xFFFFu, 0u, 0u, 1u}; f.write((const char *)hdr, 16); }   // UNI header, split 0: one plain palette
	else { const uint32_t one = 1; f.write((const char *)&one, 4); }   // MBAACC layout with one palette
	f.write((const char *)pal, 1024);
	return (bool)f;
}

bool WritePalSet(const std::string &path, const PalSet &set, bool backup, std::string *err) {
	namespace fs = std::filesystem; std::error_code ec;
	const fs::path t = P(path); fs::path bak = t; bak += L".bak"; fs::path tmp = t; tmp += L".tmp";
	if (backup && fs::exists(t, ec) && !fs::exists(bak, ec)) fs::copy_file(t, bak, ec);
	{ std::ofstream f(tmp, std::ios::binary); if (!f) { if (err) *err = "cannot write " + path; return false; } f.write((const char *)set.raw.data(), (std::streamsize)set.raw.size()); if (!f) { if (err) *err = "cannot write " + path; return false; } }
	fs::rename(tmp, t, ec);
	if (ec) { if (err) *err = "cannot replace " + path + ": " + ec.message(); return false; }
	return true;
}

std::string PupsPath(const std::string &path, int n) {
	std::string stem = path; const size_t dot = stem.find_last_of('.'), sl = stem.find_last_of("\\/");
	if (dot != std::string::npos && (sl == std::string::npos || dot > sl)) stem = stem.substr(0, dot);
	// a PUPS bank path given: strip its _pN
	if (stem.size() > 3 && stem[stem.size() - 3] == '_' && (stem[stem.size() - 2] == 'p' || stem[stem.size() - 2] == 'P') && isdigit((unsigned char)stem.back())) stem.resize(stem.size() - 3);
	return n <= 0 ? stem + ".pal" : stem + "_p" + std::to_string(n) + ".pal";
}

bool CreatePupsBank(const std::string &cgPath, int n, const PalSet &base, std::string *outPath, std::string *err) {
	if (n < 1 || n > 7) { if (err) *err = "PUPS banks are numbered 1 to 7"; return false; }
	const std::string p = PupsPath(cgPath, n);
	std::error_code ec;
	if (std::filesystem::exists(P(p), ec)) { if (err) *err = p + " already exists"; return false; }
	if (outPath) *outPath = p;
	return WritePalSet(p, base, false, err);
}

} // namespace cgm
