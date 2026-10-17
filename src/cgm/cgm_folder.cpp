#include "cgm_folder.h"
#include "cgm_ops.h"
#include "cgm_palette.h"
#include "cgm_struct.h"
#include "../png_writer.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace cgm {
namespace fs = std::filesystem;

static std::string Lower(std::string s) { for (char &c : s) c = (char)tolower((unsigned char)c); return s; }

bool ReadFolderPng(const std::string &path, std::vector<uint8_t> &rgba, int &w, int &h, std::vector<uint8_t> *idx, uint32_t pal[256], std::string *err) {
	std::string e; std::vector<uint8_t> ix; uint32_t p[256];
	if (ReadImageIndexed(path, ix, p, w, h, e)) {
		rgba.assign((size_t)w * h * 4, 0);
		// tRNS / index 0: index 0 is transparent in every 8-bit storage type
		for (int i = 0; i < w * h; i++) { const uint32_t c = p[ix[i]]; if (ix[i] && (c >> 24)) { rgba[i * 4] = c & 255; rgba[i * 4 + 1] = (c >> 8) & 255; rgba[i * 4 + 2] = (c >> 16) & 255; rgba[i * 4 + 3] = 255; } }
		if (idx) *idx = std::move(ix);
		if (pal) memcpy(pal, p, 1024);
		return true;
	}
	if (idx) idx->clear();
	if (!ReadImageRgba(path, rgba, w, h, e)) { if (err) *err = e; return false; }
	return true;
}

static bool ParseLeadingNumber(const std::string &f, double &v) {
	size_t i = 0; while (i < f.size() && isdigit((unsigned char)f[i])) i++;
	if (!i) return false;
	size_t j = i;
	if (j < f.size() && f[j] == '.' && j + 1 < f.size() && isdigit((unsigned char)f[j + 1])) { j++; while (j < f.size() && isdigit((unsigned char)f[j])) j++; }
	if (j >= f.size() || (f[j] != '_' && f[j] != '-' && f[j] != ' ' && !(f[j] == '.' && Lower(f.substr(j)) == ".png"))) return false;
	v = atof(f.substr(0, j).c_str()); return true;
}
// "12.5#name_ID_40.png": an explicit place in the new order (a number and '#')
static bool ParseOrderPrefix(const std::string &f, double &v) {
	size_t i = 0; while (i < f.size() && (isdigit((unsigned char)f[i]) || f[i] == '.')) i++;
	if (!i || i >= f.size() || f[i] != '#') return false;
	v = atof(f.substr(0, i).c_str()); return true;
}
static int IdTag(const std::string &f) {
	const std::string l = Lower(f); size_t k = l.rfind("_id_"); if (k == std::string::npos) return -1;
	size_t i = k + 4, j = i; while (j < l.size() && isdigit((unsigned char)l[j])) j++;
	if (j == i) return -1;
	return atoi(l.substr(i, j - i).c_str());
}

static void SortEntries(std::vector<FolderEntry> &v) {
	std::stable_sort(v.begin(), v.end(), [](const FolderEntry &a, const FolderEntry &b) {
		if (a.order != b.order) return a.order < b.order;
		if ((a.id >= 0) != (b.id >= 0)) return a.id >= 0;   // at the same key an existing image goes before a new one
		if (a.explicitOrder != b.explicitOrder) return !a.explicitOrder;   // a renamed file goes after the image already at that number
		return Lower(a.file) < Lower(b.file);
	});
}

bool ScanImageFolder(const std::string &dir, std::vector<FolderEntry> &out, std::string *err) {
	out.clear(); std::error_code ec;
	const fs::path root(Utf8ToWide(dir));
	if (!fs::is_directory(root, ec)) { if (err) *err = "not a folder: " + dir; return false; }
	for (auto &e : fs::directory_iterator(root, ec)) {
		if (!e.is_regular_file(ec) || Lower(e.path().extension().string()) != ".png") continue;
		FolderEntry f; f.path = WideToUtf8(e.path().wstring()); f.file = WideToUtf8(e.path().filename().wstring());
		const int tag = IdTag(f.file);
		double ord = 0; const bool hasOrd = ParseOrderPrefix(f.file, ord);
		double lead = 0; const bool idLead = !hasOrd && ParseLeadingNumber(f.file, lead) && lead == (double)(long long)lead;
		f.id = tag >= 0 ? tag : idLead ? (int)lead : -1;
		if (hasOrd) { f.order = ord; f.explicitOrder = true; }
		else if (f.id >= 0) f.order = f.id;
		out.push_back(f);
	}
	// order.txt: explicit order, one file name per line
	std::ifstream ot(root / "order.txt", std::ios::binary);
	if (ot) {
		std::map<std::string, int> pos; std::string line; int k = 0;
		while (std::getline(ot, line)) { while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back(); if (!line.empty()) pos[Lower(line)] = k++; }
		for (FolderEntry &f : out) { auto it = pos.find(Lower(f.file)); f.order = it == pos.end() ? 1e17 : it->second; f.explicitOrder = it != pos.end(); }
	}
	SortEntries(out);
	return true;
}

static bool UsesPalette(const std::vector<uint8_t> &idx, const uint32_t *pngPal, const uint32_t bankPal[256]) {
	bool used[256] = {}; for (uint8_t v : idx) used[v] = true;
	for (int k = 1; k < 256; k++) if (used[k] && ((pngPal[k] ^ bankPal[k]) & 0xFFFFFFu)) return false;
	return true;
}

static int AddPlaceholder(Bank &bank, int at, const std::string &name, int cw, int ch, std::vector<int> &remap) {
	Image ph; ph.present = true; strncpy(ph.name, name.c_str(), 31); ph.type = -1; ph.bpp = 32; ph.w = cw; ph.h = ch; ph.uid = NewImageUid();
	ph.blob = std::make_shared<const std::vector<uint8_t>>(bank.cutter2 ? 0 : 4, (uint8_t)0);
	if (at < 0 || at > (int)bank.images.size()) at = (int)bank.images.size();
	remap.assign(bank.images.size(), 0); for (size_t i = 0; i < remap.size(); i++) remap[i] = (int)i >= at ? (int)i + 1 : (int)i;
	bank.images.insert(bank.images.begin() + at, std::move(ph)); bank.recomputeLayout();
	return at;
}

// crop of the canvas from the cell-aligned corner (ax, ay) to the content's far edge; idx cropped alike
static void Crop(const uint8_t *rgba, int cw, const std::vector<uint8_t> *idx, int ax, int ay, int x1, int y1, std::vector<uint8_t> &out, std::vector<uint8_t> *outIdx) {
	const int W = x1 - ax + 1, H = y1 - ay + 1;
	out.assign((size_t)W * H * 4, 0); if (outIdx) outIdx->assign((size_t)W * H, 0);
	for (int y = 0; y < H; y++) {
		memcpy(&out[(size_t)y * W * 4], &rgba[((size_t)(ay + y) * cw + ax) * 4], (size_t)W * 4);
		if (outIdx && idx) memcpy(&(*outIdx)[(size_t)y * W], &(*idx)[(size_t)(ay + y) * cw + ax], (size_t)W);
	}
}

bool AddCanvasImage(Bank &bank, int at, const std::string &name, int type, const uint8_t *rgba, int cw, int ch, const std::vector<uint8_t> *idx, const uint32_t *pngPal,
                    std::vector<int> &remap, int *newId, std::string *err) {
	int x0, y0, x1, y1;
	if (!TrimToContent(rgba, cw, ch, x0, y0, x1, y1)) { const int id = AddPlaceholder(bank, at, name, cw, ch, remap); if (newId) *newId = id; return true; }
	const int cu = bank.cellUnit(), ax = x0 / cu * cu, ay = y0 / cu * cu;
	std::vector<uint8_t> crop, cropIdx;
	uint32_t bankPal[256]; bank.palette(0, bankPal);
	const bool exact = idx && pngPal && !idx->empty() && UsesPalette(*idx, pngPal, bankPal) && (type == -99 || type == 0);
	Crop(rgba, cw, idx, ax, ay, x1, y1, crop, exact ? &cropIdx : nullptr);
	const int W = x1 - ax + 1, H = y1 - ay + 1;
	NewImageSpec sp; sp.name = name; sp.type = exact ? 0 : type; sp.x1 = ax; sp.y1 = ay;
	const int Wc = (W + cu - 1) / cu * cu, Hc = (H + cu - 1) / cu * cu;
	sp.canvasW = std::max(cw, ax + Wc); sp.canvasH = std::max(ch, ay + Hc);
	int id = -1;
	if (!AddImage(bank, at, sp, crop.data(), W, H, remap, &id, err)) return false;
	if (exact) {   // the indices exactly as drawn (a palette with repeated colours would otherwise snap to the first match)
		const Image &im = bank.images[id]; const int bw = im.boundsW(), bh = im.boundsH();
		std::vector<uint8_t> px((size_t)bw * bh * 4, 0), ix((size_t)bw * bh, 0);
		for (int y = 0; y < H; y++) { memcpy(&px[(size_t)y * bw * 4], &crop[(size_t)y * W * 4], (size_t)W * 4); memcpy(&ix[(size_t)y * bw], &cropIdx[(size_t)y * W], (size_t)W); }
		std::string e; if (!ReplaceImage(bank, id, px.data(), bw, bh, &e, nullptr, &ix)) { if (err) *err = e; return false; }
	}
	if (newId) *newId = id;
	return true;
}

Bank EmptyBank(const uint32_t pal[256]) {
	Bank b;
	memcpy(b.head.data(), "BMP Cutter3", 11); const uint32_t one = 1; memcpy(b.head.data() + 0x10, &one, 4);   // +0x10 = 1: "the bank carries its palette" (MBAC skips a bank without it)
	for (int s = 0; s < 8; s++) for (int i = 0; i < 256; i++) { const uint32_t c = (pal[i] & 0xFFFFFFu) | 0x01000000u; memcpy(b.palettes.data() + 0x400 * s + 4 * i, &c, 4); }   // alpha byte 1 like the shipped banks
	b.H[0] = 0; b.H[4] = 16;
	return b;
}

static void TidyPages(Bank &b) {   // shipped Cutter3 convention: H0 = the last page index
	int mp = 0; for (const Image &im : b.images) for (const Block &bl : im.blocks) mp = std::max<int>(mp, bl.page);
	b.H[0] = (uint32_t)mp;
}

static bool BuildFrom(const std::vector<FolderEntry> &entries, const BuildOptions &opt, Bank &out, BuildReport &rep) {
	uint32_t pal[256] = {}; bool havePal = false; std::string e;
	if (!opt.palettePath.empty()) {
		if (!ReadPalFileColors(opt.palettePath, pal, &e, opt.palIndex)) { rep.error = e; return false; }
		havePal = true;
	}
	struct Pic { std::vector<uint8_t> rgba, idx; uint32_t pal[256]; int w = 0, h = 0; };
	std::vector<Pic> pics(entries.size());
	for (size_t i = 0; i < entries.size(); i++) {
		Pic &p = pics[i];
		if (!ReadFolderPng(entries[i].path, p.rgba, p.w, p.h, &p.idx, p.pal, &e)) { rep.error = entries[i].file + ": " + e; return false; }
		if (!havePal && !p.idx.empty()) { memcpy(pal, p.pal, 1024); havePal = true; }
	}
	if (!havePal) for (int i = 0; i < 256; i++) pal[i] = Rgb((uint8_t)i, (uint8_t)i, (uint8_t)i);   // grey ramp: every picture becomes type 2/4/1
	out = EmptyBank(pal);
	// ids: an entry with an identity keeps it (gaps become empty placeholders), the others follow
	int next = 0;
	for (size_t i = 0; i < entries.size(); i++) {
		const FolderEntry &f = entries[i]; Pic &p = pics[i];
		const int want = f.id >= 0 && f.id >= next ? f.id : next;
		while ((int)out.images.size() < want) { std::vector<int> r; AddPlaceholder(out, -1, "", 1, 1, r); rep.placeholders++; }
		std::string name = fs::path(Utf8ToWide(f.path)).stem().string();
		const size_t tag = Lower(name).rfind("_id_"); if (tag != std::string::npos) name = name.substr(0, tag);
		if (Lower(name).size() < 4 || Lower(name).substr(name.size() - 4) != ".bmp") name += ".bmp";
		std::vector<int> r; int id = -1;
		if (!AddCanvasImage(out, -1, name, -99, p.rgba.data(), p.w, p.h, p.idx.empty() ? nullptr : &p.idx, p.idx.empty() ? nullptr : p.pal, r, &id, &e)) { rep.error = f.file + ": " + e; return false; }
		if (out.images[id].type == 0) rep.indexed++;
		rep.images++; next = (int)out.images.size();
	}
	TidyPages(out); out.recomputeLayout();
	return true;
}

bool BuildBankFromFolder(const std::string &dir, const BuildOptions &opt, Bank &out, BuildReport &rep) {
	std::vector<FolderEntry> en; if (!ScanImageFolder(dir, en, &rep.error)) return false;
	if (en.empty()) { rep.error = "no PNG files in " + dir; return false; }
	return BuildFrom(en, opt, out, rep);
}
bool BuildBankFromFiles(const std::vector<std::string> &files, const BuildOptions &opt, Bank &out, BuildReport &rep) {
	std::vector<FolderEntry> en;
	for (size_t i = 0; i < files.size(); i++) { FolderEntry f; f.path = files[i]; f.file = WideToUtf8(fs::path(Utf8ToWide(files[i])).filename().wstring()); f.id = (int)i; f.order = (double)i; en.push_back(f); }
	if (en.empty()) { rep.error = "no files"; return false; }
	return BuildFrom(en, opt, out, rep);
}

bool CanvasPicture(const Bank &bank, int n, std::vector<uint8_t> &rgba, int &cw, int &ch, const uint32_t *pal) {
	if (n < 0 || n >= (int)bank.images.size() || !bank.images[n].present) return false;
	const Image &im = bank.images[n]; cw = std::max(1, im.w); ch = std::max(1, im.h);
	rgba.assign((size_t)cw * ch * 4, 0);
	Rgba r; if (!im.drawable() || !bank.decode(n, r, pal)) return true;
	for (int y = 0; y < r.h; y++) for (int x = 0; x < r.w; x++) { const int cx = x + im.x1, cy = y + im.y1; if (cx >= 0 && cy >= 0 && cx < cw && cy < ch) memcpy(&rgba[((size_t)cy * cw + cx) * 4], &r.px[((size_t)y * r.w + x) * 4], 4); }
	return true;
}

static bool SamePixels(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b, bool binary) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i += 4) {
		uint8_t aa = a[i + 3], ba = b[i + 3]; if (binary) { aa = aa >= 128 ? 255 : 0; ba = ba >= 128 ? 255 : 0; }
		if (aa != ba || (aa && memcmp(&a[i], &b[i], 3))) return false;
	}
	return true;
}

// One picture onto existing image n (ids unchanged). Returns 0 unchanged, 1 replaced in place, 2 re-cut, -1 error.
static int ApplyPicture(Bank &bank, int n, const std::vector<uint8_t> &png, int pw, int ph, const std::vector<uint8_t> *idx, const uint32_t *pngPal, std::string &e) {
	Image &im = bank.images[n];
	const bool binary = bank.cutter2 || im.type == 0 || im.type == 2 || im.type == -1;
	uint32_t bankPal[256]; bank.palette(0, bankPal);
	const bool exact = idx && pngPal && !idx->empty() && (im.type == 0 || bank.cutter2) && UsesPalette(*idx, pngPal, bankPal);
	if (im.drawable() && pw == im.boundsW() && ph == im.boundsH() && !(pw == im.w && ph == im.h)) {   // bounds-sized (our rgba export)
		if (SameRendering(bank, n, png.data(), pw, ph)) return 0;
		ReplaceReport rr; if (!ReplaceImage(bank, n, png.data(), pw, ph, &e, &rr, exact ? idx : nullptr)) return -1;
		return 1;
	}
	std::vector<uint8_t> cur; int cw, ch; CanvasPicture(bank, n, cur, cw, ch);
	if (pw == cw && ph == ch && SamePixels(cur, png, binary)) return 0;
	int x0, y0, x1, y1;
	if (!TrimToContent(png.data(), pw, ph, x0, y0, x1, y1)) {   // emptied: keep the id as an empty placeholder
		if (!im.drawable()) return 0;
		if (!ClearImage(bank, n, &e)) return -1;
		return 2;
	}
	if (im.drawable() && x0 >= im.x1 && y0 >= im.y1 && x1 <= im.x2 && y1 <= im.y2 && pw >= im.x2 + 1 && ph >= im.y2 + 1) {   // still inside the old box: in place
		std::vector<uint8_t> box((size_t)im.boundsW() * im.boundsH() * 4, 0), bix;
		if (exact) bix.assign((size_t)im.boundsW() * im.boundsH(), 0);
		for (int y = 0; y < im.boundsH(); y++) {
			memcpy(&box[(size_t)y * im.boundsW() * 4], &png[((size_t)(y + im.y1) * pw + im.x1) * 4], (size_t)im.boundsW() * 4);
			if (exact) memcpy(&bix[(size_t)y * im.boundsW()], &(*idx)[(size_t)(y + im.y1) * pw + im.x1], (size_t)im.boundsW());
		}
		ReplaceReport rr; if (!ReplaceImage(bank, n, box.data(), im.boundsW(), im.boundsH(), &e, &rr, exact ? &bix : nullptr)) return -1;
		return rr.changedPixels || rr.paletteChanged ? 1 : 0;
	}
	// another size / position: re-cut at the content box
	std::vector<uint8_t> crop, cix; Crop(png.data(), pw, exact ? idx : nullptr, x0, y0, x1, y1, crop, exact ? &cix : nullptr);
	if (!im.drawable()) {   // a placeholder gets pixels: rebuild it as a new image in its slot
		std::vector<int> r; int id = -1; Image keep = im;
		if (!AddCanvasImage(bank, -1, std::string(keep.name, strnlen(keep.name, 32)), -99, png.data(), pw, ph, idx, pngPal, r, &id, &e)) return -1;
		Image nw = std::move(bank.images[id]); bank.images.pop_back(); nw.uid = keep.uid; bank.images[n] = std::move(nw); bank.recomputeLayout();
		return 2;
	}
	if (!ReplaceImageResized(bank, n, crop.data(), x1 - x0 + 1, y1 - y0 + 1, x0, y0, nullptr, &e)) return -1;
	if (exact && bank.images[n].type == 0) {
		Image &ni = bank.images[n]; const int bw = ni.boundsW(), bh = ni.boundsH(), ox = x0 - ni.x1, oy = y0 - ni.y1, W = x1 - x0 + 1, H = y1 - y0 + 1;
		std::vector<uint8_t> px((size_t)bw * bh * 4, 0), ix((size_t)bw * bh, 0);
		for (int y = 0; y < H; y++) { memcpy(&px[((size_t)(y + oy) * bw + ox) * 4], &crop[(size_t)y * W * 4], (size_t)W * 4); memcpy(&ix[(size_t)(y + oy) * bw + ox], &cix[(size_t)y * W], (size_t)W); }
		ReplaceImage(bank, n, px.data(), bw, bh, &e, nullptr, &ix);
	}
	return 2;
}

bool ImportPlainFolder(Bank &bank, const std::string &dir, FolderImportReport &rep) {
	std::vector<FolderEntry> en; if (!ScanImageFolder(dir, en, &rep.error)) return false;
	for (const FolderEntry &f : en) {
		if (f.id < 0) { rep.warnings.push_back(f.file + ": not an image of this bank (no _ID_<n> tag or leading id); use the folder sync to add it"); continue; }
		if (f.id >= (int)bank.images.size() || !bank.images[f.id].present) { rep.warnings.push_back(f.file + ": image " + std::to_string(f.id) + " does not exist"); continue; }
		std::vector<uint8_t> px, idx; uint32_t pal[256]; int w, h; std::string e;
		if (!ReadFolderPng(f.path, px, w, h, &idx, pal, &e)) { rep.warnings.push_back(f.file + ": " + e); continue; }
		const int r = ApplyPicture(bank, f.id, px, w, h, idx.empty() ? nullptr : &idx, idx.empty() ? nullptr : pal, e);
		if (r < 0) rep.warnings.push_back(f.file + ": " + e);
		else if (r == 0) rep.unchanged++;
		else (r == 1 ? rep.replaced : rep.resized).push_back(f.id);
	}
	return true;
}

bool SyncBankToFolder(Bank &bank, const std::string &dir, std::vector<int> &remap, FolderSyncReport &rep) {
	std::vector<FolderEntry> en; if (!ScanImageFolder(dir, en, &rep.error)) return false;
	const int N = (int)bank.images.size();
	std::vector<char> seen(N, 0);
	for (const FolderEntry &f : en) if (f.id >= 0 && f.id < N) { if (seen[f.id]) { rep.error = "image " + std::to_string(f.id) + " has two files in the folder"; return false; } seen[f.id] = 1; }
	for (const FolderEntry &f : en) if (f.id >= N) { rep.error = f.file + ": image " + std::to_string(f.id) + " does not exist in this bank (rename it without the id to add it as a new image)"; return false; }
	// images an export cannot write (empty placeholders, absent slots) have no file: they stay, at their own number
	for (int k = 0; k < N; k++) if (!seen[k] && !bank.images[k].drawable()) { FolderEntry v; v.id = k; v.order = k; v.file = ""; en.push_back(v); seen[k] = 1; }
	SortEntries(en);
	Bank work = bank; std::string e;
	std::vector<int> cur = IdentityRemap((size_t)N);   // old id -> id in `work`
	// 1. deletions (from the back; images that borrow a deleted image's cells are made independent first)
	for (int k = N - 1; k >= 0; k--) {
		if (seen[k]) continue;
		std::vector<int> r; if (!DeleteImage(work, cur[k], true, r, &e)) { rep.error = "delete " + std::to_string(k) + ": " + e; return false; }
		for (int &c : cur) if (c >= 0) c = r[c];
		rep.removed++;
	}
	// 2. new images (appended), then 3. the folder order
	std::vector<int> posOfEntry(en.size(), -1);
	for (size_t i = 0; i < en.size(); i++) {
		const FolderEntry &f = en[i];
		if (f.id >= 0) { posOfEntry[i] = cur[f.id]; continue; }
		std::vector<uint8_t> px, idx; uint32_t pal[256]; int w, h;
		if (!ReadFolderPng(f.path, px, w, h, &idx, pal, &e)) { rep.error = f.file + ": " + e; return false; }
		std::string name = fs::path(Utf8ToWide(f.path)).stem().string(); double lead;
		if (ParseOrderPrefix(name + ".png", lead)) name = name.substr(name.find('#') + 1);
		name += ".bmp";
		std::vector<int> r; int id = -1;
		if (!AddCanvasImage(work, -1, name, -99, px.data(), w, h, idx.empty() ? nullptr : &idx, idx.empty() ? nullptr : pal, r, &id, &e)) { rep.error = f.file + ": " + e; return false; }
		posOfEntry[i] = id; rep.added++;
	}
	std::vector<int> order; std::vector<char> placed(work.images.size(), 0);
	for (size_t i = 0; i < en.size(); i++) { order.push_back(posOfEntry[i]); placed[posOfEntry[i]] = 1; }
	// images that stayed without a file of their own (absent slots) keep their relative place at the end
	for (size_t k = 0; k < work.images.size(); k++) if (!placed[k]) order.push_back((int)k);
	std::vector<int> perm;
	if (!Permute(work, order, perm, &e)) { rep.error = e; return false; }
	for (int &c : cur) if (c >= 0) c = perm[c];
	for (int k = 0; k < N; k++) if (seen[k] && cur[k] != k) rep.moved++;
	rep.kept = N - rep.removed;
	// 4. pixels of the kept images
	for (const FolderEntry &f : en) {
		if (f.id < 0 || f.path.empty()) continue;
		std::vector<uint8_t> px, idx; uint32_t pal[256]; int w, h;
		if (!ReadFolderPng(f.path, px, w, h, &idx, pal, &e)) { rep.pixels.warnings.push_back(f.file + ": " + e); continue; }
		const int r = ApplyPicture(work, cur[f.id], px, w, h, idx.empty() ? nullptr : &idx, idx.empty() ? nullptr : pal, e);
		if (r < 0) rep.pixels.warnings.push_back(f.file + ": " + e); else if (r == 0) rep.pixels.unchanged++; else (r == 1 ? rep.pixels.replaced : rep.pixels.resized).push_back(cur[f.id]);
	}
	remap = cur; bank = std::move(work);
	return true;
}

} // namespace cgm
