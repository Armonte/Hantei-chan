#ifndef MISC_H
#define MISC_H

#include <string>

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

std::string sj2utf8(const std::string &input);
std::string utf82sj(const std::string &input);

// Normalize path separators for consistency (converts backslashes to forward slashes,
// normalizes drive letters, removes trailing slashes)
std::string normalizePath(const std::string& path);


#endif
