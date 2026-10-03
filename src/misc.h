#ifndef MISC_H
#define MISC_H

#include <string>
#include <cstdint>
#include <vector>

static inline int to_pow2(int a) {
	int v = 1;
	while (v < a) {
		v <<= 1;
	}
	
	return v;
};

bool ReadInMem(const char *filename, char *&data, unsigned int &size);

// Atomically replace `filename` with `size` bytes from `data`.
// Writes a uniquely named temp file in the same directory, flushes it to disk,
// then swaps it over the target with MoveFileEx(MOVEFILE_REPLACE_EXISTING).
// On failure the original file is left untouched and the temp file is removed.
bool WriteFileAtomic(const char *filename, const void *data, size_t size);

// Saving a character loaded from an archive entry: Save As ... .p writes a NEW archive with the entry replaced. The editor installs the hook
// (fbarc origin registry); command line tools leave it null. Returns 0 = no origin known, 1 = written, -1 = failed (err set).
extern int (*g_saveEntryIntoArchive)(const std::string &loosePath, const std::vector<uint8_t> &storedBytes, const std::string &outPath, std::string *err);

std::string sj2utf8(const std::string &input);
std::string utf82sj(const std::string &input);

// Normalize path separators for consistency (converts backslashes to forward slashes,
// normalizes drive letters, removes trailing slashes)
std::string normalizePath(const std::string& path);


#endif
