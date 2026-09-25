/* bg_dxt32.exe: 32-bit DXT5 helper for the game-accurate stage textures
 * (src/background/bg_gametex.cpp). MBAA.exe is a 32-bit program that compresses
 * over-budget stage sprites with the 32-bit d3dx9_36.dll (x87 math); the 64-bit
 * d3dx9_36.dll Hantei-chan can load uses SSE and rounds some block endpoints
 * differently. This helper runs the game's own 32-bit path.
 *
 *   bg_dxt32.exe <in.bin> <out.bin> [fpu24]
 * in:  u32 count, then per image: u32 texW, texH, ncells, ncells x i32 {l,t,r,b},
 *      texW*texH*4 bytes A8R8G8B8 (BGRA in memory)
 * out: u32 count, then per image: u32 ok, texW/4*texH/4*16 bytes of DXT5 blocks
 * "fpu24" sets the x87 control word to 24-bit precision first (what a D3D9
 * device created without D3DCREATE_FPU_PRESERVE leaves on its thread).
 * Build: i686-w64-mingw32-gcc -O2 -o bg_dxt32.exe bg_dxt32.c -static
 */
#include <windows.h>
#include <d3d9.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef IDirect3D9* (WINAPI* PFN_Create)(UINT);
typedef HRESULT (WINAPI* PFN_CreateTexture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**);
typedef HRESULT (WINAPI* PFN_LoadMem)(IDirect3DSurface9*, const PALETTEENTRY*, const RECT*, LPCVOID, D3DFORMAT, UINT,
                                      const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR);

static unsigned rd(FILE* f) { unsigned v = 0; fread(&v, 4, 1, f); return v; }

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    HMODULE d9 = LoadLibraryA("d3d9.dll"), dx = LoadLibraryA("d3dx9_36.dll");
    if (!d9 || !dx) return 3;
    PFN_Create create = (PFN_Create)GetProcAddress(d9, "Direct3DCreate9");
    PFN_CreateTexture mk = (PFN_CreateTexture)GetProcAddress(dx, "D3DXCreateTexture");
    PFN_LoadMem load = (PFN_LoadMem)GetProcAddress(dx, "D3DXLoadSurfaceFromMemory");
    if (!create || !mk || !load) return 3;
    IDirect3D9* d3d = create(D3D_SDK_VERSION);
    if (!d3d) return 4;
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(0); wc.lpszClassName = "bg_dxt32";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("bg_dxt32", "", WS_POPUP, 0, 0, 16, 16, 0, 0, wc.hInstance, 0);
    D3DPRESENT_PARAMETERS pp; memset(&pp, 0, sizeof pp);
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferWidth = 16; pp.BackBufferHeight = 16;
    pp.hDeviceWindow = hwnd;
    IDirect3DDevice9* dev = 0;
    /* The game: MIXED | MULTITHREADED, no FPU_PRESERVE (RenderState_CreateDirect3DDevice 0x4bdac0). */
    if (FAILED(IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &pp, &dev)))
        return 5;
    if (argc >= 4 && !strcmp(argv[3], "fpu24")) _controlfp(_PC_24, _MCW_PC);
    else if (argc >= 4 && !strcmp(argv[3], "fpu53")) _controlfp(_PC_53, _MCW_PC);
    FILE* in = fopen(argv[1], "rb");
    FILE* out = fopen(argv[2], "wb");
    if (!in || !out) return 6;
    unsigned count = rd(in);
    fwrite(&count, 4, 1, out);
    for (unsigned i = 0; i < count; ++i) {
        unsigned tw = rd(in), th = rd(in), nc = rd(in);
        RECT* cells = (RECT*)malloc(sizeof(RECT) * (nc ? nc : 1));
        for (unsigned c = 0; c < nc; ++c) { cells[c].left = rd(in); cells[c].top = rd(in); cells[c].right = rd(in); cells[c].bottom = rd(in); }
        size_t bytes = (size_t)tw * th * 4;
        unsigned char* px = (unsigned char*)malloc(bytes);
        fread(px, 1, bytes, in);
        size_t nb = (size_t)(tw / 4) * (th / 4) * 16;
        unsigned char* blocks = (unsigned char*)calloc(1, nb);
        unsigned ok = 0;
        IDirect3DTexture9* tex = 0;
        if (SUCCEEDED(mk(dev, tw, th, 1, 0, (D3DFORMAT)MAKEFOURCC('D', 'X', 'T', '5'), D3DPOOL_MANAGED, &tex)) && tex) {
            IDirect3DSurface9* s = 0;
            IDirect3DTexture9_GetSurfaceLevel(tex, 0, &s);
            ok = s != 0;
            for (unsigned c = 0; ok && c < nc; ++c) {
                if (cells[c].right <= cells[c].left || cells[c].bottom <= cells[c].top) continue;
                if (FAILED(load(s, 0, &cells[c], px, D3DFMT_A8R8G8B8, tw * 4, 0, &cells[c], 1, 0))) ok = 0;
            }
            if (ok) {
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(IDirect3DSurface9_LockRect(s, &lr, 0, D3DLOCK_READONLY))) {
                    for (unsigned y = 0; y < th / 4; ++y) memcpy(blocks + (size_t)y * (tw / 4) * 16, (unsigned char*)lr.pBits + (size_t)y * lr.Pitch, (tw / 4) * 16);
                    IDirect3DSurface9_UnlockRect(s);
                } else ok = 0;
            }
            if (s) IDirect3DSurface9_Release(s);
            IDirect3DTexture9_Release(tex);
        }
        fwrite(&ok, 4, 1, out);
        fwrite(blocks, 1, nb, out);
        free(blocks); free(px); free(cells);
    }
    fclose(in); fclose(out);
    return 0;
}
