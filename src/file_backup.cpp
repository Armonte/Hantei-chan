// Auto-backup before save. See file_backup.h.
#include "file_backup.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

std::string DirOf(const std::string& p)
{
	const size_t s = p.find_last_of("\\/");
	return s == std::string::npos ? std::string(".") : p.substr(0, s);
}

std::string NameOf(const std::string& p)
{
	const size_t s = p.find_last_of("\\/");
	return s == std::string::npos ? p : p.substr(s + 1);
}

bool IsAbsolute(const std::string& p)
{
	return (p.size() >= 2 && p[1] == ':') || (!p.empty() && (p[0] == '\\' || p[0] == '/'));
}

bool FileExists(const std::string& p)
{
	const DWORD a = GetFileAttributesA(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// CreateDirectory for every missing component.
bool MakeDirs(const std::string& dir)
{
	if (dir.empty()) return false;
	const DWORD a = GetFileAttributesA(dir.c_str());
	if (a != INVALID_FILE_ATTRIBUTES) return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
	const size_t s = dir.find_last_of("\\/");
	if (s != std::string::npos && s > 0 && !(s == 2 && dir[1] == ':')) {
		if (!MakeDirs(dir.substr(0, s))) return false;
	}
	return CreateDirectoryA(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

// <name>.YYYYMMDD-HHMMSS-NN.bak
bool IsBackupName(const std::string& candidate, const std::string& base)
{
	static const size_t stampLen = 8 + 1 + 6 + 1 + 2;   // YYYYMMDD-HHMMSS-NN
	const std::string ext = ".bak";
	if (candidate.size() != base.size() + 1 + stampLen + ext.size()) return false;
	if (_strnicmp(candidate.c_str(), base.c_str(), base.size()) != 0 || candidate[base.size()] != '.') return false;
	const char* s = candidate.c_str() + base.size() + 1;
	for (size_t i = 0; i < stampLen; i++) {
		const bool dash = i == 8 || i == 15;
		if (dash ? s[i] != '-' : (s[i] < '0' || s[i] > '9')) return false;
	}
	return _stricmp(s + stampLen, ext.c_str()) == 0;
}

} // namespace

std::string BackupFolderFor(const std::string& target, const BackupOptions& opt)
{
	const std::string dir = DirOf(target);
	if (opt.folder.empty()) return dir + "\\backup";
	std::string f = opt.folder;
	while (f.size() > 1 && (f.back() == '\\' || f.back() == '/') && !(f.size() == 3 && f[1] == ':')) f.pop_back();
	return IsAbsolute(f) ? f : dir + "\\" + f;
}

std::vector<std::string> ListBackups(const std::string& target, const BackupOptions& opt)
{
	std::vector<std::string> names;
	const std::string folder = BackupFolderFor(target, opt);
	const std::string base = NameOf(target);
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((folder + "\\" + base + ".*.bak").c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsBackupName(fd.cFileName, base))
				names.push_back(fd.cFileName);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
	std::sort(names.begin(), names.end());
	for (auto& n : names) n = folder + "\\" + n;
	return names;
}

bool BackupBeforeSave(const std::string& target, const BackupOptions& opt, std::string* backupPath, std::string* err, std::time_t now)
{
	if (backupPath) backupPath->clear();
	if (!opt.enabled || target.empty() || !FileExists(target)) return true;

	const std::string folder = BackupFolderFor(target, opt);
	if (!MakeDirs(folder)) {
		if (err) *err = "could not create the backup folder " + folder;
		return false;
	}
	if (!now) now = std::time(nullptr);
	std::tm tmv{};
	localtime_s(&tmv, &now);
	char stamp[32];
	std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv);

	const std::string base = folder + "\\" + NameOf(target) + "." + stamp + "-";
	std::string dst;
	for (int n = 0; n < 100; n++) {
		char seq[8];
		snprintf(seq, sizeof seq, "%02d", n);
		const std::string candidate = base + seq + ".bak";
		if (!FileExists(candidate)) { dst = candidate; break; }
	}
	if (dst.empty()) {
		if (err) *err = "too many backups of " + NameOf(target) + " in one second";
		return false;
	}
	if (!CopyFileA(target.c_str(), dst.c_str(), TRUE)) {
		if (err) *err = "could not copy " + target + " to " + dst;
		return false;
	}
	if (backupPath) *backupPath = dst;

	// Rotation: drop the oldest beyond the cap (the one just written is the newest).
	if (opt.keep > 0) {
		auto all = ListBackups(target, opt);
		for (size_t i = 0; i + (size_t)opt.keep < all.size(); i++)
			DeleteFileA(all[i].c_str());
	}
	return true;
}
