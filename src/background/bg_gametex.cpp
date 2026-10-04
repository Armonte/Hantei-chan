// Game-accurate stage textures; see bg_gametex.h.
#include "bg_gametex.h"
#include "../cg.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <memory>
#include <map>
#include <cstdio>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#endif

namespace bg {

namespace {

int Pow2Min16(int v) { int p = 16; while (p < v) p *= 2; return p; }
int Pow2Min64(int v) { int p = 64; while (p < v) p *= 2; return p; }

const char* g_encoder = "";

// ---- built-in BC3 (stand-in when D3DX is unavailable) ---------------------------

inline uint16_t To565(const uint8_t* c) {
	return (uint16_t)(((c[0] * 31 + 127) / 255) << 11 | ((c[1] * 63 + 127) / 255) << 5 | ((c[2] * 31 + 127) / 255));
}
inline void From565(uint16_t v, int* c) {
	int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
	c[0] = (r << 3) | (r >> 2); c[1] = (g << 2) | (g >> 4); c[2] = (b << 3) | (b >> 2);
}

void EncodeBlockBC3(const uint8_t px[16][4], uint8_t out[16]) {
	// alpha: 8-level ramp between min and max
	int amin = 255, amax = 0;
	for (int i = 0; i < 16; ++i) { amin = std::min(amin, (int)px[i][3]); amax = std::max(amax, (int)px[i][3]); }
	out[0] = (uint8_t)amax; out[1] = (uint8_t)amin;
	uint64_t abits = 0;
	for (int i = 0; i < 16; ++i) {
		int a = px[i][3], idx = 0;
		if (amax != amin) {
			int best = 1 << 30;
			for (int k = 0; k < 8; ++k) {
				int v = k == 0 ? amax : k == 1 ? amin : ((8 - k) * amax + (k - 1) * amin) / 7;
				int d = std::abs(v - a);
				if (d < best) { best = d; idx = k; }
			}
		}
		abits |= (uint64_t)idx << (3 * i);
	}
	for (int i = 0; i < 6; ++i) out[2 + i] = (uint8_t)(abits >> (8 * i));
	// colour: bounding-box endpoints along the principal axis, 4-colour mode
	float mean[3] = {0, 0, 0};
	for (int i = 0; i < 16; ++i) for (int c = 0; c < 3; ++c) mean[c] += px[i][c] / 16.0f;
	float cov[6] = {0};
	for (int i = 0; i < 16; ++i) {
		float d[3] = {px[i][0] - mean[0], px[i][1] - mean[1], px[i][2] - mean[2]};
		cov[0] += d[0] * d[0]; cov[1] += d[0] * d[1]; cov[2] += d[0] * d[2];
		cov[3] += d[1] * d[1]; cov[4] += d[1] * d[2]; cov[5] += d[2] * d[2];
	}
	float axis[3] = {1, 1, 1};
	for (int it = 0; it < 8; ++it) {
		float n[3] = {cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2],
		              cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2],
		              cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2]};
		float l = std::max(std::fabs(n[0]), std::max(std::fabs(n[1]), std::fabs(n[2])));
		if (l < 1e-6f) break;
		for (int c = 0; c < 3; ++c) axis[c] = n[c] / l;
	}
	int lo = 0, hi = 0; float dlo = 1e30f, dhi = -1e30f;
	for (int i = 0; i < 16; ++i) {
		float d = (px[i][0] - mean[0]) * axis[0] + (px[i][1] - mean[1]) * axis[1] + (px[i][2] - mean[2]) * axis[2];
		if (d < dlo) { dlo = d; lo = i; }
		if (d > dhi) { dhi = d; hi = i; }
	}
	uint16_t c0 = To565(px[hi]), c1 = To565(px[lo]);
	if (c0 < c1) std::swap(c0, c1);
	int pal[4][3];
	From565(c0, pal[0]); From565(c1, pal[1]);
	for (int c = 0; c < 3; ++c) { pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3; pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3; }
	uint32_t cbits = 0;
	for (int i = 0; i < 16; ++i) {
		int best = 1 << 30, idx = 0;
		for (int k = 0; k < (c0 == c1 ? 1 : 4); ++k) {
			int d = 0;
			for (int c = 0; c < 3; ++c) { int e = pal[k][c] - px[i][c]; d += e * e; }
			if (d < best) { best = d; idx = k; }
		}
		cbits |= (uint32_t)idx << (2 * i);
	}
	out[8] = (uint8_t)c0; out[9] = (uint8_t)(c0 >> 8); out[10] = (uint8_t)c1; out[11] = (uint8_t)(c1 >> 8);
	for (int i = 0; i < 4; ++i) out[12 + i] = (uint8_t)(cbits >> (8 * i));
}

void EncodeBuiltin(const std::vector<uint8_t>& rgba, int texW, int texH, std::vector<uint8_t>& blocks) {
	blocks.assign((size_t)(texW / 4) * (texH / 4) * 16, 0);
	for (int by = 0; by < texH / 4; ++by)
		for (int bx = 0; bx < texW / 4; ++bx) {
			uint8_t px[16][4];
			for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x)
				std::memcpy(px[y * 4 + x], &rgba[(((size_t)by * 4 + y) * texW + bx * 4 + x) * 4], 4);
			EncodeBlockBC3(px, &blocks[((size_t)by * (texW / 4) + bx) * 16]);
		}
}

#ifdef _WIN32
// ---- the game's own path: D3DX DXT5, cell by cell -------------------------------

typedef IDirect3D9* (WINAPI* PFN_Direct3DCreate9)(UINT);
typedef HRESULT (WINAPI* PFN_D3DXCreateTexture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**);
typedef HRESULT (WINAPI* PFN_D3DXLoadSurfaceFromMemory)(IDirect3DSurface9*, const PALETTEENTRY*, const RECT*, LPCVOID,
                                                        D3DFORMAT, UINT, const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR);

struct D3dx {
	bool tried = false, ok = false;
	HMODULE d3d9 = nullptr, d3dx = nullptr;
	IDirect3D9* d3d = nullptr;
	IDirect3DDevice9* dev = nullptr;
	HWND hwnd = nullptr;
	PFN_D3DXCreateTexture createTexture = nullptr;
	PFN_D3DXLoadSurfaceFromMemory loadFromMemory = nullptr;

	bool Init() {
		if (tried) return ok;
		tried = true;
		d3d9 = LoadLibraryA("d3d9.dll");
		d3dx = LoadLibraryA("d3dx9_36.dll");   // the version MBAA.exe imports
		if (!d3d9 || !d3dx) return false;
		auto create = (PFN_Direct3DCreate9)GetProcAddress(d3d9, "Direct3DCreate9");
		createTexture = (PFN_D3DXCreateTexture)GetProcAddress(d3dx, "D3DXCreateTexture");
		loadFromMemory = (PFN_D3DXLoadSurfaceFromMemory)GetProcAddress(d3dx, "D3DXLoadSurfaceFromMemory");
		if (!create || !createTexture || !loadFromMemory) return false;
		d3d = create(D3D_SDK_VERSION);
		if (!d3d) return false;
		WNDCLASSA wc = {};
		wc.lpfnWndProc = DefWindowProcA;
		wc.hInstance = GetModuleHandleA(nullptr);
		wc.lpszClassName = "hantei_bg_d3dx";
		RegisterClassA(&wc);
		hwnd = CreateWindowA(wc.lpszClassName, "", WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, wc.hInstance, nullptr);
		D3DPRESENT_PARAMETERS pp = {};
		pp.Windowed = TRUE;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferFormat = D3DFMT_UNKNOWN;
		pp.BackBufferWidth = 16; pp.BackBufferHeight = 16;
		pp.hDeviceWindow = hwnd;
		HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
		                               D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev);
		if (FAILED(hr) || !dev) return false;
		ok = true;
		return true;
	}

	// canvas: texW x texH RGBA; cells: dest rects relative to the texture.
	bool Compress(const std::vector<uint8_t>& canvasRgba, int texW, int texH,
	              const std::vector<RECT>& cells, std::vector<uint8_t>& blocks) {
		if (!Init()) return false;
		IDirect3DTexture9* tex = nullptr;
		if (FAILED(createTexture(dev, texW, texH, 1, 0, (D3DFORMAT)MAKEFOURCC('D', 'X', 'T', '5'), D3DPOOL_MANAGED, &tex)) || !tex)
			return false;
		IDirect3DSurface9* surf = nullptr;
		tex->GetSurfaceLevel(0, &surf);
		// The game's page surfaces are A8R8G8B8 (BGRA in memory).
		std::vector<uint8_t> bgra(canvasRgba.size());
		for (size_t i = 0; i + 3 < canvasRgba.size(); i += 4) {
			bgra[i] = canvasRgba[i + 2]; bgra[i + 1] = canvasRgba[i + 1];
			bgra[i + 2] = canvasRgba[i]; bgra[i + 3] = canvasRgba[i + 3];
		}
		bool good = surf != nullptr;
		for (const RECT& r : cells) {
			if (!good) break;
			if (r.right <= r.left || r.bottom <= r.top) continue;
			if (FAILED(loadFromMemory(surf, nullptr, &r, bgra.data(), D3DFMT_A8R8G8B8, texW * 4, nullptr, &r,
			                          1 /*D3DX_FILTER_NONE*/, 0)))
				good = false;
		}
		if (good) {
			D3DLOCKED_RECT lr;
			if (SUCCEEDED(surf->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
				const int bw = texW / 4, bh = texH / 4;
				blocks.resize((size_t)bw * bh * 16);
				for (int y = 0; y < bh; ++y)
					std::memcpy(&blocks[(size_t)y * bw * 16], (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch, (size_t)bw * 16);
				surf->UnlockRect();
			} else good = false;
		}
		if (surf) surf->Release();
		tex->Release();
		return good;
	}
};

D3dx& GetD3dx() { static D3dx d; return d; }
#endif

} // namespace

const char* EncoderName() { return g_encoder; }

namespace {
struct BankCache {
	const CG* cg = nullptr;
	unsigned long long gen = 0;
	std::map<int, std::vector<uint8_t>> blocks;
	bool helperOk = false;
} g_bank;
std::string g_helperMode;

bool ComposeCanvas(CG& cg, int n, GameTexture& out, int& bpp) {
	out = GameTexture();
	int tid, x1, y1, x2, y2;
	if (!cg.image_info(n, bpp, tid, x1, y1, x2, y2)) return false;
	std::unique_ptr<ImageData> img(cg.draw_texture(n, true, false));
	if (!img || !img->pixels) return false;
	out.w = x2 - x1 + 1; out.h = y2 - y1 + 1;
	out.originX = x1; out.originY = y1;
	out.texW = Pow2Min16(out.w); out.texH = Pow2Min16(out.h);
	out.rgba.assign((size_t)out.texW * out.texH * 4, 0);
	const int cw = std::min(img->width, out.texW), ch = std::min(img->height, out.texH);
	for (int y = 0; y < ch; ++y)
		std::memcpy(&out.rgba[(size_t)y * out.texW * 4], img->pixels + (size_t)y * img->width * 4, (size_t)cw * 4);
	return true;
}

void CellRects(CG& cg, int n, const GameTexture& t, std::vector<int32_t>& out) {
	std::vector<CG::CellRect> cells;
	cg.image_cells(n, cells);
	out.clear();
	for (const auto& c : cells) {
		out.push_back(std::max(0, c.x - t.originX));
		out.push_back(std::max(0, c.y - t.originY));
		out.push_back(std::min(t.texW, c.x - t.originX + c.w));
		out.push_back(std::min(t.texH, c.y - t.originY + c.h));
	}
}
} // namespace

void SetDxtHelperMode(const char* mode) { g_helperMode = mode ? mode : ""; g_bank = BankCache(); }

void PrecomposeDxtBank(CG& cg) {
	if (g_bank.cg == &cg && g_bank.gen == cg.generation()) return;
	g_bank = BankCache();
	g_bank.cg = &cg;
	g_bank.gen = cg.generation();
#ifdef _WIN32
	char exe[MAX_PATH] = {0};
	GetModuleFileNameA(nullptr, exe, MAX_PATH);
	std::string dir(exe);
	dir = dir.substr(0, dir.find_last_of("\\/") + 1);
	std::string helper = dir + "bg_dxt32.exe";
	if (GetFileAttributesA(helper.c_str()) == INVALID_FILE_ATTRIBUTES) return;
	char tmp[MAX_PATH] = {0};
	GetTempPathA(MAX_PATH, tmp);
	char tag[64];
	snprintf(tag, sizeof(tag), "hantei_dxt_%lu_%p", GetCurrentProcessId(), (void*)&cg);
	std::string inPath = std::string(tmp) + tag + ".in", outPath = std::string(tmp) + tag + ".out";
	std::vector<int> ids;
	std::string req;
	{
		uint32_t count = 0;
		req.append((const char*)&count, 4);
		for (int i = 0; i < cg.get_image_count(); ++i) {
			GameTexture t; int bpp = 0;
			if (!ComposeCanvas(cg, i, t, bpp) || bpp == 8) continue;
			std::vector<int32_t> rects;
			CellRects(cg, i, t, rects);
			uint32_t hdr[3] = {(uint32_t)t.texW, (uint32_t)t.texH, (uint32_t)(rects.size() / 4)};
			req.append((const char*)hdr, sizeof(hdr));
			req.append((const char*)rects.data(), rects.size() * 4);
			for (size_t p = 0; p + 3 < t.rgba.size(); p += 4) std::swap(t.rgba[p], t.rgba[p + 2]);   // RGBA -> BGRA
			req.append((const char*)t.rgba.data(), t.rgba.size());
			ids.push_back(i);
		}
		count = (uint32_t)ids.size();
		std::memcpy(&req[0], &count, 4);
	}
	// Disk cache keyed by the request bytes (+ helper mode): compressing a big
	// bank takes a few seconds, the result never changes for the same input.
	uint64_t hsh = 1469598103934665603ull;
	for (unsigned char c : req) { hsh ^= c; hsh *= 1099511628211ull; }
	for (unsigned char c : g_helperMode) { hsh ^= c; hsh *= 1099511628211ull; }
	std::string cacheDir = std::string(tmp) + "hantei_dxt_cache\\";
	CreateDirectoryA(cacheDir.c_str(), nullptr);
	char hname[32]; snprintf(hname, sizeof(hname), "%016llx.bin", (unsigned long long)hsh);
	std::string cachePath = cacheDir + hname;
	std::vector<uint8_t> d;
	DWORD code = 1;
	{
		std::ifstream cf(cachePath, std::ios::binary);
		if (cf) { d.assign((std::istreambuf_iterator<char>(cf)), std::istreambuf_iterator<char>()); code = 0; }
	}
	if (d.empty()) {
		{ std::ofstream f(inPath, std::ios::binary); f.write(req.data(), (std::streamsize)req.size()); }
		std::string cmd = "\"" + helper + "\" \"" + inPath + "\" \"" + outPath + "\"";
		if (!g_helperMode.empty()) cmd += " " + g_helperMode;
		STARTUPINFOA si = {}; si.cb = sizeof(si);
		PROCESS_INFORMATION pi = {};
		std::vector<char> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back(0);
		if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
			WaitForSingleObject(pi.hProcess, 60000);
			GetExitCodeProcess(pi.hProcess, &code);
			CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
			std::ifstream f(outPath, std::ios::binary);
			d.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			if (code == 0 && !d.empty()) { std::ofstream cf(cachePath, std::ios::binary); cf.write((const char*)d.data(), (std::streamsize)d.size()); }
		}
	}
	{
		size_t pos = 4;
		if (code == 0 && d.size() >= 4) {
			uint32_t count = 0; std::memcpy(&count, d.data(), 4);
			for (uint32_t k = 0; k < count && k < ids.size(); ++k) {
				GameTexture t; int bpp = 0;
				ComposeCanvas(cg, ids[k], t, bpp);
				size_t nb = (size_t)(t.texW / 4) * (t.texH / 4) * 16;
				if (pos + 4 + nb > d.size()) break;
				uint32_t ok = 0; std::memcpy(&ok, d.data() + pos, 4);
				if (ok) g_bank.blocks[ids[k]].assign(d.begin() + pos + 4, d.begin() + pos + 4 + nb);
				pos += 4 + nb;
			}
			g_bank.helperOk = !g_bank.blocks.empty();
		}
	}
	DeleteFileA(inPath.c_str());
	DeleteFileA(outPath.c_str());
#endif
}

long GameTextureBudgetKB(CG& cg) {
	long kb = 0;
	for (int i = 0; i < cg.get_image_count(); ++i) {
		int bpp, tid, x1, y1, x2, y2;
		if (!cg.image_info(i, bpp, tid, x1, y1, x2, y2) || bpp == 8) continue;
		kb += 4L * Pow2Min64(x2 - x1 + 1) * Pow2Min64(y2 - y1 + 1) / 1024;
	}
	return kb;
}

bool ComposeGameTexture(CG& cg, int n, bool dxt5, GameTexture& out) {
	int bpp = 0;
	if (!ComposeCanvas(cg, n, out, bpp)) return false;
	if (bpp == 8) {
		// A1R5G5B5 through the palette (Texture_ConvertFormat 0x402eb0, dest 0x19):
		// channel >> 3 with a nonzero channel kept at least 1, alpha = index != 0.
		for (size_t i = 0; i < out.rgba.size(); i += 4) {
			for (int c = 0; c < 3; ++c) {
				int v8 = out.rgba[i + c], v = v8 >> 3;
				if (v8 && !v) v = 1;
				out.rgba[i + c] = (uint8_t)((v << 3) | (v >> 2));
			}
			out.rgba[i + 3] = out.rgba[i + 3] >= 128 ? 255 : 0;
		}
		return true;
	}
	if (!dxt5) return true;
	out.dxt5 = true;
	if (g_bank.cg == &cg && g_bank.gen == cg.generation()) {
		auto it = g_bank.blocks.find(n);
		if (it != g_bank.blocks.end()) { out.blocks = it->second; g_encoder = "d3dx9_36-x86"; return true; }
	}
#ifdef _WIN32
	std::vector<int32_t> r4;
	CellRects(cg, n, out, r4);
	std::vector<RECT> rects;
	for (size_t i = 0; i + 3 < r4.size(); i += 4) { RECT r; r.left = r4[i]; r.top = r4[i + 1]; r.right = r4[i + 2]; r.bottom = r4[i + 3]; rects.push_back(r); }
	if (GetD3dx().Compress(out.rgba, out.texW, out.texH, rects, out.blocks)) {
		g_encoder = "d3dx9_36-x64";
		return true;
	}
#endif
	EncodeBuiltin(out.rgba, out.texW, out.texH, out.blocks);
	g_encoder = "builtin-bc3";
	return true;
}

void DecodeDxt5(const uint8_t* blocks, int texW, int texH, std::vector<uint8_t>& rgba) {
	rgba.assign((size_t)texW * texH * 4, 0);
	for (int by = 0; by < texH / 4; ++by)
		for (int bx = 0; bx < texW / 4; ++bx) {
			const uint8_t* b = blocks + ((size_t)by * (texW / 4) + bx) * 16;
			int a[8]; a[0] = b[0]; a[1] = b[1];
			if (a[0] > a[1]) for (int k = 2; k < 8; ++k) a[k] = ((8 - k) * a[0] + (k - 1) * a[1]) / 7;
			else { for (int k = 2; k < 6; ++k) a[k] = ((6 - k) * a[0] + (k - 1) * a[1]) / 5; a[6] = 0; a[7] = 255; }
			uint64_t ab = 0; for (int i = 0; i < 6; ++i) ab |= (uint64_t)b[2 + i] << (8 * i);
			uint16_t c0 = b[8] | (b[9] << 8), c1 = b[10] | (b[11] << 8);
			int pal[4][3]; From565(c0, pal[0]); From565(c1, pal[1]);
			for (int c = 0; c < 3; ++c) { pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3; pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3; }
			uint32_t cb = b[12] | (b[13] << 8) | (b[14] << 16) | ((uint32_t)b[15] << 24);
			for (int i = 0; i < 16; ++i) {
				uint8_t* d = &rgba[(((size_t)by * 4 + i / 4) * texW + bx * 4 + i % 4) * 4];
				int ci = (cb >> (2 * i)) & 3;
				d[0] = (uint8_t)pal[ci][0]; d[1] = (uint8_t)pal[ci][1]; d[2] = (uint8_t)pal[ci][2];
				d[3] = (uint8_t)a[(ab >> (3 * i)) & 7];
			}
		}
}

} // namespace bg
