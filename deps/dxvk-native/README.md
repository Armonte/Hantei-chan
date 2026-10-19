# deps/dxvk-native

Vendored native build of DXVK, from the official GitHub release (lane LINUX
deliverable 4, PLAN.md P5 step 2). Read at build time, not a submodule: the
release ships the prebuilt shared objects, which is what the plan asks for.

- Release: **v3.1.1** (published 2026-09-15; at least two weeks old when vendored
  on 2026-10-18, per PLAN.md section 9).
- Asset: `dxvk-native-3.1.1-steamrt-sniper.tar.gz`
  (`https://github.com/doitsujin/dxvk/releases/download/v3.1.1/dxvk-native-3.1.1-steamrt-sniper.tar.gz`).
- Licence: zlib (see the DXVK repository).

What was taken from the tarball, unmodified:

- `lib/libdxvk_d3d9.so*`, `lib/libdxvk_dxgi.so*` - the x86_64 libraries.
  `libdxvk_dxgi.so` is present because `libdxvk_d3d9.so` links it for the
  DXGI-backed surface path.
- `include/` - the full `usr/include/dxvk` header set, so `d3d9.h`,
  `d3d9types.h` and the `wsi/native_sdl3.h` shim come from the same release as
  the library.
- `*.pc` - the release pkg-config files, kept as the record of the build flags.

Not taken: `usr/lib32/` (the i386 build) and the D3D8/10/11 libraries, none of
which this project uses.

The libraries depend only on libc/libdl/libm/libpthread (`ldd`), so they run
against the system Vulkan loader; lavapipe
(`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`) is enough for the
smoke test. DXVK's SDL3 WSI `dlopen`s `libSDL3.so.0` by name, which is why
`cmake/linux.cmake` builds a shared SDL3 from the static submodule for the smoke
test only.

