#ifndef FILE_BACKUP_H_GUARD
#define FILE_BACKUP_H_GUARD

// Auto-backup before a save overwrites a file (HA6 / character files and .pat).
//
// Sosfiro's fork copies the file to <dir>/backup/<name>.bk[N] before the menu
// Save and Save As (never on Ctrl+S) and never deletes a backup. Here every
// save path of the editor (menu, Ctrl+S, the close prompt, Save all) calls
// BackupBeforeSave(), and the number of backups kept per file is capped.
//
// Backup name: <file name>.<YYYYMMDD-HHMMSS>-<NN>.bak, NN = 00..99 for saves
// made within the same second. Names sort in time order, so rotation deletes
// the lexicographically smallest names first.
//
// Paths are in the ANSI code page, like every other file path of the editor
// (ReadInMem, WriteFileAtomic).

#include <ctime>
#include <string>
#include <vector>

struct BackupOptions {
	bool enabled = true;
	int keep = 10;        // backups kept per file; 0 = keep all
	std::string folder;   // empty = "backup" next to the file; relative = relative to the file's folder
};

// Folder that holds the backups of `target`.
std::string BackupFolderFor(const std::string& target, const BackupOptions& opt);

// Backups of `target` in its backup folder, oldest first (full paths).
std::vector<std::string> ListBackups(const std::string& target, const BackupOptions& opt);

// Copy `target` into its backup folder (created if needed), then delete the oldest backups beyond opt.keep.
// Nothing happens (true) when backups are disabled or `target` does not exist yet.
// Returns false and sets *err when the copy failed; callers still save (a failed backup must not block a save) but
// report it. `now` is injectable for tests.
bool BackupBeforeSave(const std::string& target, const BackupOptions& opt, std::string* backupPath = nullptr,
                      std::string* err = nullptr, std::time_t now = 0);

#endif /* FILE_BACKUP_H_GUARD */
