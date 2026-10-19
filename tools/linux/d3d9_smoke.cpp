// DXVK-native smoke test (lane LINUX deliverable 4, PLAN.md P5 step 2).
//
// SDL3 hidden window -> DXVK's Direct3DCreate9Ex -> clear a 64x64 render target
// to a known colour -> GetRenderTargetData -> compare the pixels. It proves the
// vendored deps/dxvk-native libdxvk_d3d9.so creates a device on lavapipe and the
// clear/readback path works; it is a bring-up check, not a renderer test.
//
// Run:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
//   SDL_VIDEO_DRIVER=offscreen DXVK_WSI_DRIVER=SDL3 ./d3d9_smoke
//
// Guarded on Linux: the Windows exe keeps its own D3D9 from the system DLL.
#ifndef _WIN32

#include <cstdio>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <windows.h>
#include <d3d9.h>

// The WSI shim DXVK's native build expects. DXVK_WSI_SDL3 comes from CMake and
// must be 1 (the header tests its value); DXVK_WSI_DRIVER=SDL3 selects the same
// shim at run time.
#include <wsi/native_wsi.h>

namespace {

constexpr UINT kW = 64, kH = 64;
// The clear colour in D3DCOLOR_ARGB order, and the RGBA bytes it must read back.
constexpr D3DCOLOR kClear = 0xff3366cc;   // a=ff r=33 g=66 b=cc
constexpr uint8_t kExpect[4] = { 0x33, 0x66, 0xcc, 0xff };

IDirect3D9Ex* g_d3d = nullptr;
IDirect3DDevice9Ex* g_device = nullptr;
SDL_Window* g_window = nullptr;

int Fail(const char* what)
{
	std::fprintf(stderr, "d3d9_smoke: FAIL: %s\n", what);
	return 1;
}

}  // namespace

int main()
{
	if (!SDL_Init(SDL_INIT_VIDEO)) return Fail("SDL_Init (is SDL_VIDEO_DRIVER=offscreen set?)");
	// SDL_WINDOW_HIDDEN: the smoke never shows a window, matching the headless
	// runs the parity harness uses.
	g_window = SDL_CreateWindow("d3d9_smoke", kW, kH, SDL_WINDOW_HIDDEN);
	if (!g_window) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return Fail("SDL_CreateWindow"); }

	// HWND is the SDL window through the WSI shim DXVK's native build uses.
	HWND hwnd = dxvk::wsi::toHwnd(g_window);

	using Create9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
	// The vendored .so is loaded dynamically, the way the Windows build loads
	// d3dcompiler_47: a missing lib gives a message, not a link error.
	// HANTEI_DXVK_LIB is the absolute path of the vendored tarball's .so (set by
	// CMake), so the test does not depend on LD_LIBRARY_PATH; the soname is the
	// fallback for a system install.
	SDL_SharedObject* lib = SDL_LoadObject(HANTEI_DXVK_LIB);
	if (!lib) lib = SDL_LoadObject("libdxvk_d3d9.so.0");
	if (!lib) { std::fprintf(stderr, "dlopen: %s\n", SDL_GetError()); return Fail("libdxvk_d3d9.so not found (deps/dxvk-native/lib)"); }
	auto create = (Create9ExFn)SDL_LoadFunction(lib, "Direct3DCreate9Ex");
	if (!create) return Fail("Direct3DCreate9Ex missing from libdxvk_d3d9.so");
	if (FAILED(create(D3D_SDK_VERSION, &g_d3d))) return Fail("Direct3DCreate9Ex");

	D3DPRESENT_PARAMETERS pp{};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = hwnd;
	pp.BackBufferWidth = kW;
	pp.BackBufferHeight = kH;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	HRESULT hr = g_d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
	                                   D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, nullptr, &g_device);
	if (FAILED(hr)) { std::fprintf(stderr, "CreateDeviceEx hr=0x%08lx\n", (unsigned long)hr); return Fail("CreateDeviceEx"); }

	// A render target offscreen (D3DPOOL_DEFAULT) plus a system-memory copy for
	// the readback, which is the exact pair PLAN.md 5.3 describes.
	IDirect3DTexture9* rt = nullptr;
	hr = g_device->CreateTexture(kW, kH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rt, nullptr);
	if (FAILED(hr)) return Fail("CreateTexture(render target)");
	IDirect3DSurface9* rtSurface = nullptr;
	rt->GetSurfaceLevel(0, &rtSurface);

	IDirect3DSurface9* sysSurface = nullptr;
	hr = g_device->CreateOffscreenPlainSurface(kW, kH, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sysSurface, nullptr);
	if (FAILED(hr)) return Fail("CreateOffscreenPlainSurface");

	g_device->SetRenderTarget(0, rtSurface);
	g_device->Clear(0, nullptr, D3DCLEAR_TARGET, kClear, 1.0f, 0);

	hr = g_device->GetRenderTargetData(rtSurface, sysSurface);
	if (FAILED(hr)) return Fail("GetRenderTargetData");

	D3DLOCKED_RECT lr{};
	hr = sysSurface->LockRect(&lr, nullptr, D3DLOCK_READONLY);
	if (FAILED(hr)) return Fail("LockRect");

	// Readback is BGRA in memory; the editor's convention is RGBA top-down, the
	// swizzle PLAN.md 5.3 names.
	int bad = 0, x0 = -1, y0 = -1;
	const uint8_t* base = (const uint8_t*)lr.pBits;
	for (UINT y = 0; y < kH; ++y) {
		const uint8_t* row = base + (size_t)y * (size_t)lr.Pitch;
		for (UINT x = 0; x < kW; ++x) {
			const uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2], a = row[x * 4 + 3];
			if (r != kExpect[0] || g != kExpect[1] || b != kExpect[2] || a != kExpect[3]) {
				if (!bad) { x0 = (int)x; y0 = (int)y; }
				++bad;
			}
		}
	}
	sysSurface->UnlockRect();

	if (bad) {
		std::fprintf(stderr, "d3d9_smoke: %d/%u pixels differ, first at (%d,%d)\n", bad, kW * kH, x0, y0);
		return Fail("render target pixels");
	}

	std::printf("d3d9_smoke: ok (%ux%u cleared to ARGB %08lx and read back)\n", kW, kH, (unsigned long)kClear);

	sysSurface->Release();
	rtSurface->Release();
	rt->Release();
	g_device->Release();
	g_d3d->Release();
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	return 0;
}

#else  /* _WIN32 */

#include <cstdio>
int main() { std::fprintf(stderr, "d3d9_smoke is Linux-only (the Windows build uses the system d3d9)\n"); return 77; }

#endif  /* _WIN32 */

