#include "misc.h"
#include "platform/portable/paths.h"

#include <algorithm>
#include <cctype>

// This file used to be the place where the editor reached for windows.h: the
// shared file read, the CP932 name conversion and the atomic save all went
// through Win32 directly. They now go through src/platform/portable, which has
// the Windows body and a POSIX body behind one signature, so the Windows build
// keeps exactly the behaviour it had:
//
//   ReadInMem      -> portable::ReadFileAlloc    (CreateFileA + FILE_FLAG_SEQUENTIAL_SCAN)
//   sj2utf8        -> portable::SjisToUtf8       (CP932, MB_ERR_INVALID_CHARS then lenient)
//   utf82sj        -> portable::Utf8ToSjis
//   WriteFileAtomic-> portable::ReplaceFileAtomic (temp file, flush, MoveFileEx retry loop)
//   normalizePath  -> portable::NormalizeSeparators

bool ReadInMem(const char *filename, char *&data, unsigned int &size)
{
	return portable::ReadFileAlloc(filename ? filename : "", data, size);
}

std::string sj2utf8(const std::string &input)
{
	return portable::SjisToUtf8(input);
}

std::string utf82sj(const std::string &input)
{
	return portable::Utf8ToSjis(input);
}

int (*g_saveEntryIntoArchive)(const std::string &, const std::vector<uint8_t> &, const std::string &, std::string *) = nullptr;

bool WriteFileAtomic(const char *filename, const void *data, size_t size)
{
	if(!filename || !*filename)
		return false;
	return portable::ReplaceFileAtomic(filename, data, size);
}

std::string normalizePath(const std::string& path)
{
	return portable::NormalizeSeparators(path);
}
