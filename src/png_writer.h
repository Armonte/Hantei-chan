#ifndef PNG_WRITER_H_GUARD
#define PNG_WRITER_H_GUARD

// PNG output through the Windows Imaging Component (works with MinGW-w64:
// wincodec.h + -lwindowscodecs -lole32). Paths are UTF-8 and converted to
// UTF-16 for the Win32 calls, so non-ASCII folders (Japanese character names,
// user profiles) work. Files are written to "<path>.tmp" first and moved over
// the destination, so a failed export never leaves a truncated PNG.

#include <cstdint>
#include <string>
#include <vector>

// rgba: top-down rows, 8-bit straight (non-premultiplied) alpha.
bool WritePngRgba(const std::string& utf8Path, const uint8_t* rgba, int width, int height, std::string& error);

// Decode any WIC-readable image (PNG, BMP, JPEG, ...) to straight-alpha RGBA, top-down rows.
bool ReadImageRgba(const std::string& utf8Path, std::vector<uint8_t>& rgba, int& width, int& height, std::string& error);

// 8-bit palette PNG (PLTE + tRNS). pal = 256 entries, memory order R,G,B,A (value 0xAABBGGRR); alpha goes to tRNS.
bool WritePngIndexed(const std::string& utf8Path, const uint8_t* indices, int width, int height, const uint32_t pal[256], std::string& error);
// Reads a palette-indexed image (1/2/4/8 bpp) as 8-bit indices + 256-entry palette (same memory order). false + error when it is not indexed.
bool ReadImageIndexed(const std::string& utf8Path, std::vector<uint8_t>& indices, uint32_t pal[256], int& width, int& height, std::string& error);

// UTF-8 <-> UTF-16 / ANSI helpers used by the export UI.
std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
std::string AnsiToUtf8(const std::string& s);   // CP_ACP -> UTF-8 (editor paths are ANSI)

// Create a directory tree (UTF-8). Returns false with a message on failure.
bool CreateDirectoriesUtf8(const std::string& utf8Dir, std::string& error);

// Folder picker (IFileOpenDialog, FOS_PICKFOLDERS). Empty when cancelled.
std::string BrowseForFolderUtf8(const std::string& initialUtf8);

#endif /* PNG_WRITER_H_GUARD */
