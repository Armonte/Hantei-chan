// Empty translation unit for the libSDL3.so.0 shim CMake builds from the static
// SDL3 archive (cmake/linux.cmake). DXVK's SDL3 WSI dlopen()s "libSDL3.so.0"
// by name, so the smoke test needs a shared SDL3 even though the app links the
// static one. A shared library needs at least one source; this is it.

// Nothing here. The symbols come from SDL3-static through --whole-archive.
