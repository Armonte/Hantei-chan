#include "cgm_struct.h"
#include "cgm_ops.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace cgm {

namespace {

struct Grid {   // free/used atlas cells
	int cu, cpr, pages;
	std::vector<std::vector<uint8_t>> used;
	explicit Grid(const Bank &b) : cu(b.cellUnit()), cpr(256 / b.cellUnit()), pages(b.pages() - 1) {
		int maxPage = -1;
		for (const Image &im : b.images) for (const Block &bl : im.blocks) maxPage = std::max<int>(maxPage, bl.page);
		pages = std::max(maxPage + 1, (int)b.H[0]);
		used.assign((size_t)pages, std::vector<uint8_t>((size_t)cpr * cpr, 0));
		for (const Image &im : b.images) for (const Block &bl : im.blocks) if (!bl.copy) mark(bl.page, bl.sx / cu, bl.sy / cu, bl.w / cu, bl.h / cu, 1);
	}
	void mark(int page, int cx, int cy, int w, int h, uint8_t v) {
		if (page < 0) return;
		while (page >= (int)used.size()) { used.emplace_back((size_t)cpr * cpr, 0); pages = (int)used.size(); }
		for (int y = cy; y < cy + h && y < cpr; y++) for (int x = cx; x < cx + w && x < cpr; x++) used[page][(size_t)y * cpr + x] = v;
	}
	bool free(int page, int cx, int cy, int w, int h) const {
		if (cx + w > cpr || cy + h > cpr) return false;
		for (int y = cy; y < cy + h; y++) for (int x = cx; x < cx + w; x++) if (used[page][(size_t)y * cpr + x]) return false;
		return true;
	}
	// first fit over the existing pages, then a new page
	void alloc(int w, int h, int &page, int &cx, int &cy) {
		for (int p = 0; p < (int)used.size(); p++) for (int y = 0; y + h <= cpr; y++) for (int x = 0; x + w <= cpr; x++) if (free(p, x, y, w, h)) { page = p; cx = x; cy = y; mark(p, x, y, w, h, 1); return; }
		page = (int)used.size(); cx = cy = 0; mark(page, 0, 0, w, h, 1);
	}
};

size_t PerPixel(int type) { return type == 1 ? 4 : type == 4 ? 2 : 1; }
size_t Prefix(int type, int bpp) { return bpp == 32 ? (type == 2 || type == 4 ? 1024 : type == 3 ? 4 : 0) : 0; }

void Finish(Bank &bank) { bank.recomputeLayout(); }
void SetPages(Bank &bank, const Grid &g) { int mp = -1; for (const Image &im : bank.images) for (const Block &b : im.blocks) mp = std::max<int>(mp, b.page); bank.H[0] = (uint32_t)std::max<int>((int)bank.H[0], mp + 1); (void)g; }

// remap for an insertion / removal: ids >= at shift
std::vector<int> ShiftRemap(size_t n, int at, int delta) { std::vector<int> r(n); for (size_t i = 0; i < n; i++) r[i] = (int)i >= at ? (int)i + delta : (int)i; return r; }
}

std::vector<int> IdentityRemap(size_t n) { std::vector<int> r(n); for (size_t i = 0; i < n; i++) r[i] = (int)i; return r; }
std::vector<int> InvertRemap(const std::vector<int> &remap, size_t newCount) {
	std::vector<int> inv(newCount, -1); for (size_t i = 0; i < remap.size(); i++) if (remap[i] >= 0 && (size_t)remap[i] < newCount) inv[remap[i]] = (int)i; return inv;
}

bool AddImage(Bank &bank, int at, const NewImageSpec &spec, const uint8_t *rgba, int w, int h, std::vector<int> &remap, int *newId, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const int cu = bank.cellUnit();
	if ((int)bank.images.size() >= kMaxImages - 1) return fail("the bank already holds the maximum number of images");
	if (w <= 0 || h <= 0) return fail("empty image");
	if (at < 0 || at > (int)bank.images.size()) at = (int)bank.images.size();
	const int W = (w + cu - 1) / cu * cu, H = (h + cu - 1) / cu * cu;
	std::vector<uint8_t> px((size_t)W * H * 4, 0);
	for (int y = 0; y < h; y++) memcpy(&px[(size_t)y * W * 4], &rgba[(size_t)y * w * 4], (size_t)w * 4);
	// storage type
	int type = spec.type;
	if (type == -99) {
		std::set<uint32_t> cols; bool soft = false;
		for (int i = 0; i < W * H; i++) { const uint8_t a = px[i * 4 + 3]; if (!a) continue; if (a != 255) soft = true; cols.insert(((uint32_t)px[i * 4] << 16) | ((uint32_t)px[i * 4 + 1] << 8) | px[i * 4 + 2]); }
		type = cols.size() > 255 ? 1 : soft ? 4 : 2;
	}
	if (type != 0 && type != 1 && type != 2 && type != 4) return fail("storage type " + std::to_string(type) + " cannot be created");
	Image im; im.present = true;
	strncpy(im.name, spec.name.c_str(), 31); im.name[31] = 0;
	im.type = type; im.bpp = type == 0 ? 8 : 32;
	im.w = std::max(spec.canvasW, W); im.h = std::max(spec.canvasH, H);
	int x1 = spec.x1, y1 = spec.y1;
	if (x1 == INT32_MIN) x1 = (im.w - W) / 2 / cu * cu;
	if (y1 == INT32_MIN) y1 = (im.h - H) / 2 / cu * cu;
	x1 = x1 / cu * cu; y1 = y1 / cu * cu;
	im.x1 = x1; im.y1 = y1; im.x2 = x1 + W - 1; im.y2 = y1 + H - 1;
	if (x1 < 0 || y1 < 0 || im.x2 >= im.w || im.y2 >= im.h) return fail("the sprite does not fit on its canvas");
	// blocks: runs of non-empty cells per cell row, each run placed in free atlas cells
	Grid grid(bank);
	const int cw = W / cu, ch = H / cu;
	auto cellEmpty = [&](int cx, int cy) { for (int y = 0; y < cu; y++) for (int x = 0; x < cu; x++) if (px[((size_t)(cy * cu + y) * W + cx * cu + x) * 4 + 3]) return false; return true; };
	for (int cy = 0; cy < ch; cy++) for (int cx = 0; cx < cw;) {
		if (cellEmpty(cx, cy)) { cx++; continue; }
		int run = 1; while (cx + run < cw && run < grid.cpr && !cellEmpty(cx + run, cy)) run++;
		Block b; b.x = x1 + cx * cu; b.y = y1 + cy * cu; b.w = run * cu; b.h = cu;
		int page, sx, sy; grid.alloc(run, 1, page, sx, sy);
		b.page = (int16_t)page; b.sx = (int16_t)(sx * cu); b.sy = (int16_t)(sy * cu);
		im.blocks.push_back(b); cx += run;
	}
	size_t sz = Prefix(type, im.bpp); for (const Block &b : im.blocks) sz += (size_t)b.w * b.h * PerPixel(type);
	im.blob = std::make_shared<const std::vector<uint8_t>>(sz, (uint8_t)0);
	remap = ShiftRemap(bank.images.size(), at, 1);
	bank.images.insert(bank.images.begin() + at, std::move(im));
	SetPages(bank, grid); Finish(bank);
	std::string e; ReplaceReport rep;
	if (!ReplaceImage(bank, at, px.data(), W, H, &e, &rep)) { bank.images.erase(bank.images.begin() + at); Finish(bank); return fail(e); }
	if (newId) *newId = at;
	return true;
}

bool ClearImage(Bank &bank, int n, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (n < 0 || n >= (int)bank.images.size() || !bank.images[n].present) return fail("no such image");
	auto atlas = bank.buildAtlas(); auto dep = bank.dependants(n, atlas);
	if (!dep.empty()) { std::string s; for (int d : dep) s += " " + std::to_string(d); return fail("image(s)" + s + " borrow cells of this image; make them independent first"); }
	Image &im = bank.images[n];
	im.type = -1; im.bpp = 32; im.x1 = im.y1 = im.x2 = im.y2 = 0; im.blocks.clear();
	im.blob = std::make_shared<const std::vector<uint8_t>>(4, (uint8_t)0);
	Finish(bank); return true;
}

bool UnshareImage(Bank &bank, int n, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (n < 0 || n >= (int)bank.images.size()) return fail("no such image");
	Image &im = bank.images[n];
	if (!im.drawable()) return true;
	bool any = false; for (const Block &b : im.blocks) any |= b.copy != 0;
	if (!any) return true;
	const int type = im.type;
	std::vector<uint8_t> idx, alpha; uint32_t pal[256]; Rgba rgba;
	if (type == 0 || type == 2 || type == 4) { if (!bank.decodeIndexed(n, idx, pal, &alpha)) return fail("cannot read the image"); }
	if (!bank.decode(n, rgba)) return fail("cannot read the image");
	const int W = im.boundsW(), H = im.boundsH(), cu = bank.cellUnit();
	Grid grid(bank);
	std::vector<char> wasCopy; std::vector<uint32_t> oldOff;
	for (const Block &b : im.blocks) { wasCopy.push_back(b.copy != 0); oldOff.push_back(b.dataOff); }
	for (Block &b : im.blocks) if (b.copy) {
		int page, sx, sy; grid.alloc(b.w / cu, b.h / cu, page, sx, sy);
		b.page = (int16_t)page; b.sx = (int16_t)(sx * cu); b.sy = (int16_t)(sy * cu); b.copy = 0;
	}
	size_t off = Prefix(type, im.bpp); std::vector<size_t> offs;
	for (const Block &b : im.blocks) { offs.push_back(off); off += (size_t)b.w * b.h * PerPixel(type); }
	std::vector<uint8_t> out(off, 0);
	memcpy(out.data(), im.blob->data(), std::min(Prefix(type, im.bpp), im.blob->size()));   // palette / colour prefix stays
	for (size_t k = 0; k < im.blocks.size(); k++) {
		const Block &b = im.blocks[k];
		if (!wasCopy[k]) {   // already owned: keep its bytes exactly
			const size_t n2 = (size_t)b.w * b.h * PerPixel(type);
			if (oldOff[k] + n2 <= im.blob->size()) memcpy(&out[offs[k]], &(*im.blob)[oldOff[k]], n2);
			continue;
		}
		for (int ly = 0; ly < b.h; ly++) for (int lx = 0; lx < b.w; lx++) {
			const int cx = b.x + lx - im.x1, cy = b.y + ly - im.y1; const bool inside = cx >= 0 && cy >= 0 && cx < W && cy < H;
			const size_t o = offs[k] + ((size_t)ly * b.w + lx) * (type == 1 ? 4 : 1), pix = (size_t)cy * W + cx;
			if (type == 1) { uint8_t v[4] = {0, 0, 0, 0}; if (inside) { v[0] = rgba.px[pix * 4 + 2]; v[1] = rgba.px[pix * 4 + 1]; v[2] = rgba.px[pix * 4]; v[3] = rgba.px[pix * 4 + 3]; } memcpy(&out[o], v, 4); }
			else if (type == 3) out[o] = inside ? rgba.px[pix * 4 + 3] : 0;
			else { out[o] = inside ? idx[pix] : 0; if (type == 4) out[o + (size_t)b.w * b.h] = inside ? alpha[pix] : 0; }
		}
	}
	im.blob = std::make_shared<const std::vector<uint8_t>>(std::move(out));
	SetPages(bank, grid); Finish(bank);
	return true;
}

bool DeleteImage(Bank &bank, int n, bool unshare, std::vector<int> &remap, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (n < 0 || n >= (int)bank.images.size()) return fail("no such image");
	auto atlas = bank.buildAtlas(); auto dep = bank.dependants(n, atlas);
	if (!dep.empty()) {
		if (!unshare) { std::string s; for (int d : dep) s += " " + std::to_string(d); return fail("image(s)" + s + " borrow cells of this image"); }
		for (int d : dep) if (!UnshareImage(bank, d, err)) return false;
	}
	remap = ShiftRemap(bank.images.size(), n + 1, -1); remap[n] = -1;
	bank.images.erase(bank.images.begin() + n);
	Finish(bank); return true;
}

bool Permute(Bank &bank, const std::vector<int> &order, std::vector<int> &remap, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	const size_t N = bank.images.size();
	if (order.size() != N) return fail("the new order must list every image once");
	std::vector<char> seen(N, 0); for (int o : order) { if (o < 0 || (size_t)o >= N || seen[o]) return fail("the new order is not a permutation"); seen[o] = 1; }
	std::vector<Image> nw; nw.reserve(N); remap.assign(N, -1);
	for (size_t k = 0; k < N; k++) { nw.push_back(std::move(bank.images[order[k]])); remap[order[k]] = (int)k; }
	bank.images = std::move(nw); Finish(bank); return true;
}

bool MoveImage(Bank &bank, int from, int to, std::vector<int> &remap, std::string *err) {
	const int N = (int)bank.images.size();
	if (from < 0 || from >= N || to < 0 || to >= N) { if (err) *err = "image position out of range"; return false; }
	std::vector<int> order = IdentityRemap((size_t)N);
	const int v = order[from]; order.erase(order.begin() + from); order.insert(order.begin() + to, v);
	return Permute(bank, order, remap, err);
}

bool DuplicateImage(Bank &bank, int n, int at, std::vector<int> &remap, int *newId, std::string *err) {
	auto fail = [&](const std::string &m) { if (err) *err = m; return false; };
	if (n < 0 || n >= (int)bank.images.size() || !bank.images[n].present) return fail("no such image");
	if ((int)bank.images.size() >= kMaxImages - 1) return fail("the bank already holds the maximum number of images");
	if (at < 0 || at > (int)bank.images.size()) at = (int)bank.images.size();
	Grid grid(bank); const int cu = bank.cellUnit();   // the original's cells are marked; the copy gets fresh ones
	Image c = bank.images[n];
	for (Block &b : c.blocks) if (!b.copy) { int page, sx, sy; grid.alloc(b.w / cu, b.h / cu, page, sx, sy); b.page = (int16_t)page; b.sx = (int16_t)(sx * cu); b.sy = (int16_t)(sy * cu); }
	remap = ShiftRemap(bank.images.size(), at, 1);
	bank.images.insert(bank.images.begin() + at, std::move(c));
	SetPages(bank, grid); Finish(bank);
	if (newId) *newId = at;
	return true;
}

bool TrimPages(Bank &bank) {
	int mp = -1; for (const Image &im : bank.images) for (const Block &b : im.blocks) mp = std::max<int>(mp, b.page);
	const uint32_t want = (uint32_t)(mp + 1);
	if (want >= bank.H[0]) return false;
	bank.H[0] = want; return true;
}

} // namespace cgm
