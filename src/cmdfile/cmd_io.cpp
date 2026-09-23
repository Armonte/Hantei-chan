#include "cmd_io.h"

#include <windows.h>

#include <ctime>
#include <fstream>
#include <sstream>

namespace cmdfile {

std::string Cp932ToUtf8(const std::string& in)
{
	if (in.empty()) return {};
	const int wideLen = MultiByteToWideChar(932, 0, in.data(), static_cast<int>(in.size()), nullptr, 0);
	if (wideLen <= 0) return {};
	std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
	MultiByteToWideChar(932, 0, in.data(), static_cast<int>(in.size()), wide.data(), wideLen);
	const int outLen = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
	if (outLen <= 0) return {};
	std::string out(static_cast<std::size_t>(outLen), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, out.data(), outLen, nullptr, nullptr);
	return out;
}

std::optional<std::string> Utf8ToCp932(const std::string& in)
{
	if (in.empty()) return std::string();
	const int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), static_cast<int>(in.size()), nullptr, 0);
	if (wideLen <= 0) return std::nullopt;
	std::wstring wide(static_cast<std::size_t>(wideLen), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), static_cast<int>(in.size()), wide.data(), wideLen);
	BOOL usedDefault = FALSE;
	const int outLen = WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), wideLen, nullptr, 0, nullptr, &usedDefault);
	if (outLen <= 0 || usedDefault) return std::nullopt;
	std::string out(static_cast<std::size_t>(outLen), '\0');
	usedDefault = FALSE;
	WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), wideLen, out.data(), outLen, nullptr, &usedDefault);
	if (usedDefault) return std::nullopt;
	return out;
}

bool ReadFileBytes(const std::string& path, std::string& out, std::string* error)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) { if (error) *error = "Cannot open " + path; return false; }
	std::ostringstream ss;
	ss << in.rdbuf();
	if (in.bad()) { if (error) *error = "Cannot read " + path; return false; }
	out = ss.str();
	return true;
}

bool FileExists(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool WriteUniqueBackup(const std::string& targetPath, const std::string& bytes, std::string* backupPath, std::string* error)
{
	const auto slash = targetPath.find_last_of("\\/");
	const std::string dir = slash == std::string::npos ? std::string(".") : targetPath.substr(0, slash);
	const std::string name = slash == std::string::npos ? targetPath : targetPath.substr(slash + 1);
	const std::string backupDir = dir + "\\.hantei-backups";
	if (!CreateDirectoryA(backupDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
		if (error) *error = "Cannot create backup folder " + backupDir;
		return false;
	}
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &now);
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
	for (int attempt = 0; attempt < 1000; ++attempt) {
		std::string candidate = backupDir + "\\" + name + "." + stamp;
		if (attempt) candidate += "-" + std::to_string(attempt);
		candidate += ".bak";
		HANDLE h = CreateFileA(candidate.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) {
			const DWORD e = GetLastError();
			if (e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS) continue;
			if (error) *error = "Cannot create backup " + candidate;
			return false;
		}
		DWORD written = 0;
		const bool ok = bytes.empty() ||
			(WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size());
		const bool flushed = ok && FlushFileBuffers(h);
		CloseHandle(h);
		if (!flushed) {
			DeleteFileA(candidate.c_str());
			if (error) *error = "Cannot write backup " + candidate;
			return false;
		}
		if (backupPath) *backupPath = candidate;
		return true;
	}
	if (error) *error = "No free backup name in " + backupDir;
	return false;
}

std::vector<std::string> CommandFileCandidates(const std::string& txtPath)
{
	std::vector<std::string> out;
	const auto dot = txtPath.find_last_of('.');
	const auto slash = txtPath.find_last_of("\\/");
	const std::string stem = (dot != std::string::npos && (slash == std::string::npos || dot > slash)) ? txtPath.substr(0, dot) : txtPath;
	const std::string ext = (dot != std::string::npos && (slash == std::string::npos || dot > slash)) ? txtPath.substr(dot) : std::string(".txt");
	const bool upper = ext == ".TXT";
	auto add = [&](const std::string& p) {
		if (FileExists(p)) for (const auto& e : out) if (e == p) return;
		if (FileExists(p)) out.push_back(p);
	};
	add(stem + (upper ? "_C.TXT" : "_c.txt"));
	for (const char* moon : { "_0", "_1", "_2", "_9", "_8" })
		add(stem + moon + (upper ? "_C.TXT" : "_c.txt"));
	return out;
}

} // namespace cmdfile
