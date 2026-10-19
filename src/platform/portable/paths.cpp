#include "paths.h"
#include "process.h"   // NowMs (the temp-name tick)

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <cerrno>
#include <iconv.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace portable {

namespace {

// A process id, spelled the same on both platforms. Only used to make a temp
// file name unique between two instances saving at the same moment.
unsigned Pid()
{
#ifdef _WIN32
	return (unsigned)_getpid();
#else
	return (unsigned)getpid();
#endif
}

}  // namespace

#ifdef _WIN32

namespace {

std::wstring WidenUtf8(const std::string& s)
{
	if (s.empty()) return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

std::string NarrowUtf8(const std::wstring& w)
{
	if (w.empty()) return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

// CP_ACP when cp == 0, so the call sites keep the semantics of the Ansi* helpers.
unsigned ResolveCp(unsigned cp) { return cp == 0 ? CP_ACP : cp; }

}  // namespace

std::string CpToUtf8(const std::string& bytes, unsigned cp)
{
	if (bytes.empty()) return std::string();
	const UINT code = ResolveCp(cp);
	const int n = MultiByteToWideChar(code, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(code, 0, bytes.data(), (int)bytes.size(), w.data(), n);
	return NarrowUtf8(w);
}

std::string Utf8ToCp(const std::string& utf8, unsigned cp)
{
	if (utf8.empty()) return std::string();
	const UINT code = ResolveCp(cp);
	const std::wstring w = WidenUtf8(utf8);
	const int n = WideCharToMultiByte(code, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(code, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

std::wstring Utf8ToWide(const std::string& utf8) { return WidenUtf8(utf8); }
std::string WideToUtf8(const std::wstring& wide) { return NarrowUtf8(wide); }

// The names png_writer.h already exports, kept so moving the definition here is
// a drop-in for the callers that use them.
std::string AnsiToUtf8(const std::string& bytes) { return CpToUtf8(bytes, 0); }
std::string Utf8ToAnsi(const std::string& utf8) { return Utf8ToCp(utf8, 0); }

std::string ExecutablePath()
{
	wchar_t buf[32768];
	const DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
	if (n == 0) return std::string();
	return NarrowUtf8(std::wstring(buf, n));
}

std::string UserDataDir()
{
	const char* local = std::getenv("LOCALAPPDATA");
	if (!local || !*local) return std::string();
	return std::string(local) + "\\hantei-chan";
}

std::string CurrentDir()
{
	char buf[4096];
	const DWORD n = GetCurrentDirectoryA((DWORD)sizeof buf, buf);
	return n > 0 ? std::string(buf, n) : std::string();
}

// The misc.cpp sj2utf8 body, kept call for call: a strict CP932 pass
// (MB_ERR_INVALID_CHARS) so a bad sequence is reported, then the lenient pass
// the original fell back to; a conversion that still fails gives "".
std::string SjisToUtf8(const std::string& sjis)
{
	if (sjis.empty()) return std::string();
	const UINT cp = 932;
	SetLastError(0);
	int wideLen = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, sjis.c_str(), (int)sjis.size(), nullptr, 0);
	if (wideLen == 0) {
		const DWORD error = GetLastError();
		if (error == ERROR_NO_UNICODE_TRANSLATION) {
			SetLastError(0);
			wideLen = MultiByteToWideChar(cp, 0, sjis.c_str(), (int)sjis.size(), nullptr, 0);
			if (wideLen == 0) return std::string();
		} else {
			return std::string();
		}
	}
	std::wstring wide((size_t)wideLen, L'\0');
	if (MultiByteToWideChar(cp, 0, sjis.c_str(), (int)sjis.size(), wide.data(), wideLen) == 0) return std::string();
	return NarrowUtf8(wide);
}

// The misc.cpp utf82sj body: UTF-8 to UTF-16, then UTF-16 to CP932. Unlike the
// forward direction the original has no strict/lenient retry here, and it trims
// the result to the length the second call reported.
std::string Utf8ToSjis(const std::string& utf8)
{
	if (utf8.empty()) return std::string();
	const UINT cp = 932;
	const int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	if (wideLen == 0) return std::string();
	std::wstring wide((size_t)wideLen, L'\0');
	if (MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), wideLen) == 0) return std::string();
	const int sjisLen = WideCharToMultiByte(cp, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
	if (sjisLen == 0) return std::string();
	std::string out((size_t)sjisLen, '\0');
	const int result = WideCharToMultiByte(cp, 0, wide.c_str(), wideLen, out.data(), sjisLen, nullptr, nullptr);
	if (result == 0) return std::string();
	out.resize((size_t)result);
	return out;
}

// The misc.cpp ReadInMem body. The caller owns the buffer (delete[]) and an
// empty file still gets a one-byte allocation, exactly as before.
bool ReadFileAlloc(const std::string& utf8Path, char*& data, unsigned& size)
{
	data = nullptr;
	size = 0;
	HANDLE file = CreateFileA(utf8Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
	                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER fileSize{};
	if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart > 0x7FFFFFFF) {
		CloseHandle(file);
		return false;
	}
	const DWORD wanted = (DWORD)fileSize.QuadPart;
	char* buffer = new (std::nothrow) char[wanted ? wanted : 1];
	if (!buffer) {
		CloseHandle(file);
		return false;
	}
	DWORD readBytes = 0;
	const BOOL ok = ReadFile(file, buffer, wanted, &readBytes, nullptr);
	CloseHandle(file);
	if (!ok || readBytes != wanted) {
		delete[] buffer;
		return false;
	}
	data = buffer;
	size = wanted;
	return true;
}


#else  /* POSIX */

namespace {

// UTF-8 <-> UTF-32 in bytes. wchar_t is UTF-32 on Linux, which is what
// std::filesystem and fopen want, so no locale is involved and the helpers
// behave the same on a C locale.
void AppendUtf8(std::string& out, char32_t cp)
{
	if (cp <= 0x7f) {
		out.push_back((char)cp);
	} else if (cp <= 0x7ff) {
		out.push_back((char)(0xc0 | (cp >> 6)));
		out.push_back((char)(0x80 | (cp & 0x3f)));
	} else if (cp <= 0xffff) {
		out.push_back((char)(0xe0 | (cp >> 12)));
		out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
		out.push_back((char)(0x80 | (cp & 0x3f)));
	} else {
		out.push_back((char)(0xf0 | (cp >> 18)));
		out.push_back((char)(0x80 | ((cp >> 12) & 0x3f)));
		out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
		out.push_back((char)(0x80 | (cp & 0x3f)));
	}
}

// Decodes one code point; advances i past it. Invalid sequences become U+FFFD,
// which is what Windows' conversion does for an undecodable byte too.
char32_t NextUtf8(const std::string& s, size_t& i)
{
	const unsigned char c = (unsigned char)s[i];
	if (c < 0x80) { ++i; return c; }
	auto cont = [&](size_t k) { return i + k < s.size() && ((unsigned char)s[i + k] & 0xc0) == 0x80; };
	if ((c & 0xe0) == 0xc0 && cont(1)) {
		char32_t cp = char32_t(c & 0x1f) << 6 | ((unsigned char)s[i + 1] & 0x3f);
		i += 2; return cp;
	}
	if ((c & 0xf0) == 0xe0 && cont(1) && cont(2)) {
		char32_t cp = char32_t(c & 0x0f) << 12 | ((unsigned char)s[i + 1] & 0x3f) << 6 | ((unsigned char)s[i + 2] & 0x3f);
		i += 3; return cp;
	}
	if ((c & 0xf8) == 0xf0 && cont(1) && cont(2) && cont(3)) {
		char32_t cp = char32_t(c & 0x07) << 18 | ((unsigned char)s[i + 1] & 0x3f) << 12 |
		               ((unsigned char)s[i + 2] & 0x3f) << 6 | ((unsigned char)s[i + 3] & 0x3f);
		i += 4; return cp;
	}
	++i;
	return 0xfffd;
}

std::string WideToUtf8Local(const std::wstring& w)
{
	std::string out;
	out.reserve(w.size());
	for (wchar_t wc : w) AppendUtf8(out, (char32_t)(unsigned long)wc);
	return out;
}

std::wstring Utf8ToWideLocal(const std::string& s)
{
	std::wstring out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size();) out.push_back((wchar_t)NextUtf8(s, i));
	return out;
}

}  // namespace

// ---- iconv-backed code page conversion (CP932 and friends) ----
//
// glibc ships the CP932 (Shift-JIS) converter in libc, so the games' Japanese
// names decode without vendoring a table or a libiconv submodule. cp 0 (the
// "ANSI" code page, which on POSIX is UTF-8) and 65001 are the identity, and any
// other code page is iconv's name for it.
static std::string IconvConvert(const std::string& in, const char* from, const char* to, bool lenient)
{
	iconv_t cd = iconv_open(to, from);
	if (cd == (iconv_t)-1) return std::string();
	std::string out;
	out.reserve(in.size());
	char buf[4096];
	char* src = const_cast<char*>(in.data());
	size_t left = in.size();
	for (;;) {
		char* dst = buf;
		size_t room = sizeof buf;
		const size_t rc = iconv(cd, left ? &src : nullptr, &left, &dst, &room);
		out.append(buf, (size_t)(dst - buf));
		if (rc != (size_t)-1) break;
		if (errno == E2BIG) continue;           // buffer full, output is fine
		if (errno == EILSEQ && lenient) {
			// This is the Windows lenient pass (MultiByteToWideChar without
			// MB_ERR_INVALID_CHARS): the offending byte becomes U+FFFD and the
			// conversion continues, so one bad byte does not lose the rest.
			out.append("\xef\xbf\xbd");
			if (left) { ++src; --left; }
			continue;
		}
		iconv_close(cd);
		return std::string();                  // EILSEQ strict, or EINVAL (truncated)
	}
	iconv_close(cd);
	return out;
}

std::string CpToUtf8(const std::string& bytes, unsigned cp)
{
	if (bytes.empty()) return std::string();
	// cp 0 is "the ANSI code page": cp932 on a Japanese Windows machine, and
	// UTF-8 on POSIX, which is what the editor's Ansi* helpers mean on each
	// platform. 65001 is UTF-8 explicitly. Both are the identity here.
	if (cp == 0 || cp == 65001 || cp == 1200 || cp == 1201) return bytes;
	const std::string name = "CP" + std::to_string(cp);
	return IconvConvert(bytes, name.c_str(), "UTF-8", true);
}

std::string Utf8ToCp(const std::string& utf8, unsigned cp)
{
	if (utf8.empty()) return std::string();
	if (cp == 0 || cp == 65001 || cp == 1200 || cp == 1201) return utf8;
	const std::string name = "CP" + std::to_string(cp);
	return IconvConvert(utf8, "UTF-8", name.c_str(), true);
}
// ---- end iconv ----

// The names png_writer.h already exports, kept so moving the definition here is
// a drop-in for the callers that use them.
std::string AnsiToUtf8(const std::string& bytes) { return CpToUtf8(bytes, 0); }
std::string Utf8ToAnsi(const std::string& utf8) { return Utf8ToCp(utf8, 0); }

std::wstring Utf8ToWide(const std::string& utf8) { return Utf8ToWideLocal(utf8); }
std::string WideToUtf8(const std::wstring& wide) { return WideToUtf8Local(wide); }

std::string ExecutablePath()
{
	char buf[4096];
	const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
	if (n <= 0) return std::string();
	buf[n] = '\0';
	return std::string(buf, (size_t)n);
}

// The misc.cpp sj2utf8 pair, on the same two-attempt contract as the Windows
// body: a strict CP932 pass, then the lenient pass the original fell back to.
// A conversion that fails in both gives "".
std::string SjisToUtf8(const std::string& sjis)
{
	if (sjis.empty()) return std::string();
	std::string out = IconvConvert(sjis, "CP932", "UTF-8", false);
	if (out.empty()) out = IconvConvert(sjis, "CP932", "UTF-8", true);
	return out;
}

// UTF-8 to CP932: the Windows body has no strict/lenient retry in this
// direction, and neither does this one.
std::string Utf8ToSjis(const std::string& utf8)
{
	if (utf8.empty()) return std::string();
	return IconvConvert(utf8, "UTF-8", "CP932", false);
}

// The misc.cpp ReadInMem body (POSIX side): the whole file into a caller-owned
// new char[], shared for read. An empty file still gets a one-byte allocation.
bool ReadFileAlloc(const std::string& utf8Path, char*& data, unsigned& size)
{
	data = nullptr;
	size = 0;
	std::error_code ec;
	const uintmax_t n = fs::file_size(fs::u8path(utf8Path), ec);
	if (ec || n > 0x7FFFFFFFu) return false;
	const unsigned wanted = (unsigned)n;
	char* buffer = new (std::nothrow) char[wanted ? wanted : 1];
	if (!buffer) return false;
	std::ifstream f(fs::u8path(utf8Path), std::ios::binary);
	if (!f || (wanted && !f.read(buffer, (std::streamsize)wanted))) {
		delete[] buffer;
		return false;
	}
	if (f.gcount() != (std::streamsize)wanted) {
		delete[] buffer;
		return false;
	}
	data = buffer;
	size = wanted;
	return true;
}

std::string UserDataDir()
{
	const char* xdg = std::getenv("XDG_DATA_HOME");
	if (xdg && *xdg) return std::string(xdg) + "/hantei-chan";
	const char* home = std::getenv("HOME");
	if (!home || !*home) return std::string();
	return std::string(home) + "/.local/share/hantei-chan";
}

std::string CurrentDir()
{
	std::error_code ec;
	return fs::current_path(ec).u8string();
}

#endif  /* _WIN32 */

std::string ExecutableDir()
{
	const std::string exe = ExecutablePath();
	if (exe.empty()) return std::string();
	fs::path p = fs::u8path(exe).parent_path();
	return p.u8string();
}

std::string NormalizeSeparators(std::string path)
{
	for (char& c : path) if (c == '\\') c = '/';
	// Upper-case a leading drive letter so "c:/x" and "C:/x" compare equal;
	// this is the misc.cpp normalizePath behaviour, kept byte for byte.
	if (path.size() >= 2 && path[1] == ':') path[0] = (char)std::toupper((unsigned char)path[0]);
	while (path.size() > 1 && path.back() == '/') path.pop_back();
	return path;
}

bool FileExists(const std::string& utf8Path)
{
	std::error_code ec;
	return fs::is_regular_file(fs::u8path(utf8Path), ec);
}

uint64_t FileSize(const std::string& utf8Path)
{
	std::error_code ec;
	const uint64_t n = (uint64_t)fs::file_size(fs::u8path(utf8Path), ec);
	return ec ? 0 : n;
}

bool CreateDirectories(const std::string& utf8Dir, std::string* error)
{
	std::error_code ec;
	const fs::path dir = fs::u8path(utf8Dir);
	if (dir.empty()) {
		if (error) *error = "No output folder was given.";
		return false;
	}
	fs::create_directories(dir, ec);   // never throws
	if (ec || !fs::is_directory(dir, ec)) {
		if (error) *error = "Could not create the output folder: " + utf8Dir;
		return false;
	}
	return true;
}

bool ReadWholeFile(const std::string& utf8Path, std::vector<uint8_t>& out)
{
	std::ifstream f(fs::u8path(utf8Path), std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

// The temp file lives next to the target so the final replace stays on one
// volume; pid + a per-process counter keeps concurrent saves apart.
namespace {

std::string TempSiblingPath(const std::string& utf8Path)
{
	static unsigned counter = 0;
	// Byte for byte the name misc.cpp's WriteFileAtomic built: the target, ".tmp",
	// the pid, the monotonic tick and a per-process counter. The tick is only
	// there to separate two instances saving at the same millisecond; the
	// counter already separates two saves inside one process.
	return utf8Path + ".tmp" + std::to_string(Pid()) + "_" + std::to_string(NowMs()) + "_" + std::to_string(++counter);
}

}  // namespace

#ifdef _WIN32

// Copied from misc.cpp's WriteFileAtomic (the Windows body) so the switch-over
// is a rename, not a rewrite.
bool ReplaceFileAtomic(const std::string& utf8Path, const void* data, size_t size)
{
	if (utf8Path.empty()) return false;
	const std::string tmp = TempSiblingPath(utf8Path);
	HANDLE file = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	bool ok = true;
	const char* p = (const char*)data;
	size_t remaining = size;
	while (ok && remaining > 0) {
		DWORD chunk = remaining > 0x40000000u ? 0x40000000u : (DWORD)remaining;
		DWORD written = 0;
		if (!WriteFile(file, p, chunk, &written, nullptr) || written != chunk) ok = false;
		p += written;
		remaining -= written;
	}
	if (ok && !FlushFileBuffers(file)) ok = false;
	if (!CloseHandle(file)) ok = false;

	// A just-written target is often held for a moment by antivirus / the
	// indexer, which makes the replace fail with a sharing, lock or access
	// error; retry for up to ~1 s before giving up.
	if (ok) {
		ok = false;
		for (int attempt = 0; attempt < 40; ++attempt) {
			if (MoveFileExA(tmp.c_str(), utf8Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { ok = true; break; }
			DWORD err = GetLastError();
			if (err != ERROR_SHARING_VIOLATION && err != ERROR_LOCK_VIOLATION && err != ERROR_ACCESS_DENIED) break;
			Sleep(25);
		}
	}
	if (!ok) DeleteFileA(tmp.c_str());
	return ok;
}

#else  /* POSIX */

bool ReplaceFileAtomic(const std::string& utf8Path, const void* data, size_t size)
{
	if (utf8Path.empty()) return false;
	const std::string tmp = TempSiblingPath(utf8Path);
	{
		std::ofstream f(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
		if (!f) return false;
		if (size) f.write((const char*)data, (std::streamsize)size);
		f.flush();
		if (!f) { std::error_code ec; fs::remove(fs::u8path(tmp), ec); return false; }
	}
	// POSIX rename(2) replaces the target atomically; there is no sharing-
	// violation retry to do here.
	std::error_code ec;
	fs::rename(fs::u8path(tmp), fs::u8path(utf8Path), ec);
	if (ec) {
		fs::remove(fs::u8path(tmp), ec);
		return false;
	}
	return true;
}

#endif  /* _WIN32 */

namespace {

std::string TrimSpace(const std::string& s)
{
	size_t a = 0, b = s.size();
	while (a < b && (unsigned char)s[a] <= ' ') ++a;
	while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
	return s.substr(a, b - a);
}

// Reads the ini into lines, then looks for [section] and key=. Shared by the
// getters; IniSetString keeps the file and only touches the one line.
std::vector<std::string> ReadIniLines(const std::string& iniPath)
{
	std::vector<std::string> lines;
	std::ifstream f(fs::u8path(iniPath), std::ios::binary);
	std::string line;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		lines.push_back(line);
	}
	return lines;
}

// Passed the raw value, returns everything before the first comment marker that
// is not inside the value: GetPrivateProfileString stops at ';'.
std::string StripIniComment(const std::string& v)
{
	const size_t at = v.find(';');
	return at == std::string::npos ? v : v.substr(0, at);
}

}  // namespace

std::string IniGetString(const std::string& iniPath, const std::string& section, const std::string& key,
                         const std::string& def)
{
	const std::vector<std::string> lines = ReadIniLines(iniPath);
	bool inSection = false;
	for (const std::string& raw : lines) {
		const std::string line = TrimSpace(raw);
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line.front() == '[' && line.back() == ']') {
			inSection = TrimSpace(line.substr(1, line.size() - 2)) == section;
			continue;
		}
		if (!inSection) continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		if (TrimSpace(line.substr(0, eq)) != key) continue;
		return TrimSpace(StripIniComment(line.substr(eq + 1)));
	}
	return def;
}

int IniGetInt(const std::string& iniPath, const std::string& section, const std::string& key, int def)
{
	const std::string v = IniGetString(iniPath, section, key, std::string());
	if (v.empty()) return def;
	char* end = nullptr;
	const long n = std::strtol(v.c_str(), &end, 10);
	return end && end != v.c_str() ? (int)n : def;
}

bool IniSetString(const std::string& iniPath, const std::string& section, const std::string& key, const std::string& value)
{
	std::vector<std::string> lines = ReadIniLines(iniPath);
	bool inSection = false;
	bool wrote = false;
	int sectionEnd = (int)lines.size();
	for (int i = 0; i < (int)lines.size(); ++i) {
		const std::string line = TrimSpace(lines[i]);
		if (!line.empty() && line.front() == '[' && line.back() == ']') {
			if (inSection) { sectionEnd = i; break; }
			inSection = TrimSpace(line.substr(1, line.size() - 2)) == section;
			if (inSection) sectionEnd = (int)lines.size();
			continue;
		}
		if (!inSection) continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos || TrimSpace(line.substr(0, eq)) != key) continue;
		lines[i] = key + "=" + value;
		wrote = true;
		break;
	}
	if (!wrote) {
		if (sectionEnd >= (int)lines.size()) {
			if (!lines.empty() && !TrimSpace(lines.back()).empty()) lines.push_back(std::string());
			lines.push_back("[" + section + "]");
			lines.push_back(key + "=" + value);
		} else {
			lines.insert(lines.begin() + sectionEnd, key + "=" + value);
		}
	}
	std::string text;
	for (const std::string& l : lines) { text += l; text += '\n'; }
	return ReplaceFileAtomic(iniPath, text.data(), text.size());
}

}  // namespace portable
